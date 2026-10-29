//! The sensor ECU application, as a state machine without I/O.
//!
//! What the C++ firmware spreads over `ecu.cpp` (common threads and
//! supervisor), `sensor_role.cpp`, `test_control.cpp` and the HAL's simulated
//! CAN controller lives here as one object, [`SensorEcu`], that an adapter
//! drives:
//!
//! * `on_frame` for every received standard data frame (the RX thread);
//! * `tx_tick` every 10 ms (the TX thread): the ECU hands the frames to send
//!   to a callback and accounts for what reached the bus;
//! * `supervisor_tick` every 5 ms: controller events, bus-off recovery,
//!   message timeouts, the node state machine and the watchdog decision; it
//!   reports what happened as [`Event`]s for the adapter to log;
//! * `check_in` / `take_stall` from each thread for the liveness monitor and
//!   the STALL_THREAD hook; `status` and `reset_faults` for the shell.
//!
//! Time is passed in as milliseconds since boot, so the whole ECU can be unit
//! tested with a fake clock and runs unchanged on any platform that provides
//! threads (or a scheduler) and a CAN driver.

use crate::can_error_sm::{Action, BusOffPolicy, CanErrorStateMachine, Phase};
use crate::e2e::{self, CounterGenerator};
use crate::error_counter_model::{ErrorCounterModel, Event as CounterEvent};
use crate::liveness::Liveness;
use crate::messages::{
    BatteryStatus, CanState, ControllerCommand, ControllerHeartbeat, EchoRequest, Message, Node,
    NodeState, SensorData, SensorEchoResponse, SensorHeartbeat, SensorStatus, TestCommand,
    TestControl,
};
use crate::node_state::{NodeInputs, NodePolicy, NodeStateMachine};
use crate::protected_rx::{ProtectedReceiver, RxResult, RxStats};
use crate::timeout_monitor::{MonitoredMessage, TimeoutMonitor};

/// TX scheduler base period: the greatest common divisor of all cycle times.
pub const BASE_TICK_MS: u32 = 10;
pub const THREAD_COUNT: usize = 3;

/// Bits of the heartbeat's `error_flags` field (assignment documented in the DBC).
pub const FLAG_CRC_FAULT_SEEN: u8 = 1 << 0;
pub const FLAG_COUNTER_FAULT_SEEN: u8 = 1 << 1;
pub const FLAG_RX_TIMEOUT_SEEN: u8 = 1 << 2;
pub const FLAG_BUS_OFF_SEEN: u8 = 1 << 3;
pub const FLAG_WATCHDOG_WARNING: u8 = 1 << 4;
pub const FLAG_TEST_HOOK_ACTIVE: u8 = 1 << 5;

/// The three application threads, in the order the liveness mask uses.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum Thread {
    Rx = 0,
    Tx = 1,
    Supervisor = 2,
}

impl Thread {
    pub const fn name(self) -> &'static str {
        match self {
            Self::Rx => "rx",
            Self::Tx => "tx",
            Self::Supervisor => "supervisor",
        }
    }

    pub const fn from_index(index: u32) -> Option<Self> {
        match index {
            0 => Some(Self::Rx),
            1 => Some(Self::Tx),
            2 => Some(Self::Supervisor),
            _ => None,
        }
    }
}

/// The application's timing and policy constants (`firmware/app/Kconfig`
/// defaults; the adapter may override them from a configuration source).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Config {
    pub supervisor_tick_ms: u32,
    pub command_timeout_ms: u32,
    pub heartbeat_timeout_ms: u32,
    pub safe_state_recovery_ms: u32,
    pub quality_fault_threshold: u32,
    pub bus_off: BusOffPolicy,
    pub watchdog_timeout_ms: u32,
    pub liveness_window_ms: u32,
    /// Depth of the adapter's receive queue, reported in the status.
    pub rx_queue_depth: u32,
    /// The modelled bus-off recovery sequence (128 x 11 bit times): 3 ms, as in the C++ HAL.
    pub sim_recovery_delay_ms: u32,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            supervisor_tick_ms: 5,
            command_timeout_ms: 70,
            heartbeat_timeout_ms: 350,
            safe_state_recovery_ms: 500,
            quality_fault_threshold: 3,
            bus_off: BusOffPolicy::default(),
            watchdog_timeout_ms: 1000,
            liveness_window_ms: 300,
            rx_queue_depth: 32,
            sim_recovery_delay_ms: 3,
        }
    }
}

/// A frame the ECU wants on the bus.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct TxFrame {
    pub id: u16,
    pub dlc: u8,
    pub data: [u8; 8],
}

impl TxFrame {
    pub fn payload(&self) -> &[u8] {
        &self.data[..self.dlc as usize]
    }
}

/// Why the adapter could not send a frame (the HAL's `busy` and `rejected`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SendError {
    Busy,
    Rejected,
}

/// Transmit-side fault injection requested through TEST_CONTROL (Phase 3).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct TxFaults {
    pub suppress_heartbeat_until_ms: i64,
    pub suppress_id: u16,
    pub suppress_id_until_ms: i64,
    pub corrupt_crc_remaining: u32,
    pub skip_counter_remaining: u32,
    pub freeze_counter_remaining: u32,
    /// Applied counts, for the shell and the tests.
    pub suppressed: u32,
    pub corrupted: u32,
    pub skipped: u32,
    pub frozen: u32,
}

/// The faults applied to one frame between protection and transmission; settled
/// by `transmit`: counted when the frame leaves the node, handed back to the
/// budget when it does not, so the hook counters mean frames that reached the bus.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Applied {
    pub corrupt: bool,
    pub skip: bool,
    pub freeze: bool,
}

/// What happened, for the adapter's log. The texts the C++ firmware logs are
/// reproduced by the adapter, so the same test tooling reads both ECUs.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Event {
    CanStateChanged {
        previous: CanState,
        state: CanState,
        tec: u8,
        rec: u8,
        before: Phase,
        after: Phase,
        polled: bool,
    },
    RecoveryAttempt {
        attempt: u32,
        backoff_ms: u32,
        accepted: bool,
    },
    RecoveryExhausted {
        attempts: u32,
    },
    Timeout {
        name: &'static str,
        timeout_ms: u32,
    },
    NodeState {
        before: NodeState,
        after: NodeState,
        reason: &'static str,
    },
    WatchdogNotFed {
        stale_mask: u32,
    },
    TestControl(TestControlEvent),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TestControlEvent {
    WrongMagic(u16),
    ResetFaults,
    InjectCanError {
        count: u32,
        state: CanState,
        tec: u8,
        rec: u8,
    },
    ForceBusOff {
        state: CanState,
    },
    SuppressHeartbeat {
        ms: u32,
    },
    CorruptCrc {
        count: u32,
    },
    SkipCounter {
        count: u32,
    },
    FreezeCounter {
        count: u32,
    },
    SuppressMessage {
        id: u16,
        ms: u32,
    },
    StallThread {
        thread: u32,
        ms: u32,
    },
    NoSuchThread(u32),
}

/// What the RX path asks the adapter to do after a frame was handled.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RxAction {
    None,
    /// Answer an ECHO_REQUEST: the adapter measures the turnaround and calls `send_echo`.
    Echo {
        seq: u8,
        payload: u32,
    },
}

/// Receive statistics of one monitored message, for the status.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RxStatus {
    pub name: &'static str,
    pub accepted: u32,
    pub crc_faults: u32,
    /// repeated + out of sequence
    pub counter_faults: u32,
    pub dlc_faults: u32,
    pub skipped: u32,
    pub lost_frames: u32,
    pub timeouts: u32,
    /// -1 if never received.
    pub age_ms: i64,
    pub alive: bool,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct HookCounters {
    pub suppressed: u32,
    pub corrupted: u32,
    pub skipped: u32,
    pub frozen: u32,
}

/// The role detail of the status line, numbers only; the adapter formats it.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Detail {
    pub tx_data: u32,
    pub tx_battery: u32,
    pub tx_heartbeat: u32,
    pub speed_cm_s: i32,
    pub yaw_0_1deg_s: i32,
    pub soc_0_5pct: u32,
    pub have_command: bool,
    pub command_mode: u8,
    pub torque_0_01nm: i32,
    pub actuators_enabled: u32,
}

/// Snapshot of the ECU state for the shell (`ecu status`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Status {
    pub uptime_ms: i64,
    pub node_state: NodeState,
    pub node_reason: &'static str,
    pub node_state_age_ms: i64,
    pub safe_state_entries: u32,
    pub error_flags: u8,
    pub can_phase: Phase,
    pub can_state: CanState,
    pub tec: u8,
    pub rec: u8,
    pub simulation_active: bool,
    pub bus_off_events: u32,
    pub passive_events: u32,
    pub recovery_attempts: u32,
    pub recoveries_ok: u32,
    pub recovery_failures: u32,
    pub last_recovery_duration_ms: i64,
    pub current_backoff_ms: u32,
    pub tx_sent: u32,
    pub tx_blocked_bus_off: u32,
    pub tx_tick_overruns: u32,
    pub rx_frames: u32,
    pub rx_unknown: u32,
    pub rx_queue_hwm: u32,
    pub rx_queue_depth: u32,
    pub can_events_dropped: u32,
    pub echo_responses: u32,
    pub test_commands: u32,
    pub wdt_timeout_ms: u32,
    pub wdt_feeds: u32,
    pub wdt_skipped: u32,
    pub liveness_stale_mask: u32,
    pub hooks: HookCounters,
    pub rx: [RxStatus; 2],
    pub detail: Detail,
}

/// Deterministic vehicle model: a slow speed swell, a yaw oscillation and a
/// draining battery, all functions of uptime so two runs produce the same data.
#[derive(Clone, Copy, Debug, Default)]
struct SensorModel {
    speed_mps: f32,
    yaw_rate_dps: f32,
    accel_x_mps2: f32,
    soc_pct: f32,
    voltage_v: f32,
    current_a: f32,
    temperature_c: f32,
}

impl SensorModel {
    fn step(&mut self, now_ms: i64) {
        const TWO_PI: f32 = core::f32::consts::TAU;
        let t = now_ms as f32 * 0.001;
        self.speed_mps = 12.0 + 8.0 * libm::sinf(TWO_PI * t / 60.0);
        self.accel_x_mps2 = 8.0 * (TWO_PI / 60.0) * libm::cosf(TWO_PI * t / 60.0);
        self.yaw_rate_dps = 10.0 * libm::sinf(TWO_PI * t / 20.0);
        self.soc_pct = 90.0 - t / 120.0; // 0.5 % per minute
        if self.soc_pct < 20.0 {
            self.soc_pct = 20.0;
        }
        self.current_a = 5.0 + 1.5 * self.speed_mps;
        self.voltage_v = 44.0 + 0.05 * self.soc_pct - 0.02 * self.current_a;
        self.temperature_c = 25.0 + 0.4 * self.speed_mps;
    }
}

/// Rolling counter of one transmitted message type, with the SKIP_COUNTER and
/// FREEZE_COUNTER faults applied so a receiver sees what a buggy or stuck
/// sender would produce.
#[derive(Clone, Copy, Debug, Default)]
struct TxCounter {
    gen: CounterGenerator,
    last: u8,
}

impl TxCounter {
    fn next(&mut self, faults: &mut TxFaults, applied: &mut Applied) -> u8 {
        if faults.freeze_counter_remaining != 0 {
            faults.freeze_counter_remaining -= 1;
            applied.freeze = true;
            return self.last; // a stuck sender: same counter as the previous frame
        }
        if faults.skip_counter_remaining != 0 {
            faults.skip_counter_remaining -= 1;
            applied.skip = true;
            let _ = self.gen.advance(); // one counter value is never transmitted
        }
        self.last = self.gen.advance();
        self.last
    }
}

#[derive(Clone, Copy, Debug)]
struct CanEvent {
    state: CanState,
    tec: u8,
    rec: u8,
    t_ms: i64,
}

const CAN_EVENT_DEPTH: usize = 8;

/// The sensor ECU.
#[derive(Debug)]
pub struct SensorEcu {
    cfg: Config,
    model: SensorModel,
    command_rx: ProtectedReceiver<ControllerCommand>,
    heartbeat_rx: ProtectedReceiver<ControllerHeartbeat>,
    last_command: ControllerCommand,
    have_command: bool,
    data_counter: TxCounter,
    battery_counter: TxCounter,
    heartbeat_counter: TxCounter,
    tx_data: u32,
    tx_battery: u32,
    tx_heartbeat: u32,
    monitor: TimeoutMonitor<2>,
    node: NodeStateMachine,
    can_sm: CanErrorStateMachine,
    sim: ErrorCounterModel,
    sim_active: bool,
    sim_last_state: CanState,
    sim_recover_due_ms: Option<i64>,
    can_events: [Option<CanEvent>; CAN_EVENT_DEPTH],
    can_events_dropped: u32,
    liveness: Liveness<THREAD_COUNT>,
    error_flags: u8,
    rx_frames: u32,
    rx_unknown: u32,
    rx_queue_hwm: u32,
    echo_responses: u32,
    test_commands: u32,
    tx_sent: u32,
    tx_blocked_bus_off: u32,
    tx_tick_overruns: u32,
    wdt_feeds: u32,
    wdt_skipped: u32,
    liveness_stale_mask: u32,
    tx_faults: TxFaults,
    stall_ms: [u32; THREAD_COUNT],
    last_wdt_warning_ms: i64,
}

// Slots within the 100 ms cycle (in 10 ms ticks) for the 100 ms messages, so
// they do not all land in the same tick as a sensor sample.
const TICKS_PER_100MS: u32 = BatteryStatus::CYCLE_MS as u32 / BASE_TICK_MS;
const BATTERY_SLOT: u32 = 3;
const HEARTBEAT_SLOT: u32 = 6;

impl SensorEcu {
    pub fn new(cfg: Config) -> Self {
        Self {
            cfg,
            model: SensorModel::default(),
            command_rx: ProtectedReceiver::new(),
            heartbeat_rx: ProtectedReceiver::new(),
            last_command: ControllerCommand::default(),
            have_command: false,
            data_counter: TxCounter::default(),
            battery_counter: TxCounter::default(),
            heartbeat_counter: TxCounter::default(),
            tx_data: 0,
            tx_battery: 0,
            tx_heartbeat: 0,
            monitor: TimeoutMonitor::new([
                MonitoredMessage {
                    id: ControllerCommand::ID,
                    timeout_ms: cfg.command_timeout_ms,
                    name: ControllerCommand::NAME,
                },
                MonitoredMessage {
                    id: ControllerHeartbeat::ID,
                    timeout_ms: cfg.heartbeat_timeout_ms,
                    name: ControllerHeartbeat::NAME,
                },
            ]),
            // The sensor watches the controller to report its own view of the network;
            // it has no actuator, so losing the controller makes it DEGRADED, not safe.
            node: NodeStateMachine::new(NodePolicy {
                safe_state_on_input_loss: false,
                recovery_hold_ms: cfg.safe_state_recovery_ms,
            }),
            can_sm: CanErrorStateMachine::new(cfg.bus_off),
            sim: ErrorCounterModel::new(),
            sim_active: false,
            sim_last_state: CanState::ErrorActive,
            sim_recover_due_ms: None,
            can_events: [None; CAN_EVENT_DEPTH],
            can_events_dropped: 0,
            liveness: Liveness::new(cfg.liveness_window_ms),
            error_flags: 0,
            rx_frames: 0,
            rx_unknown: 0,
            rx_queue_hwm: 0,
            echo_responses: 0,
            test_commands: 0,
            tx_sent: 0,
            tx_blocked_bus_off: 0,
            tx_tick_overruns: 0,
            wdt_feeds: 0,
            wdt_skipped: 0,
            liveness_stale_mask: 0,
            tx_faults: TxFaults::default(),
            stall_ms: [0; THREAD_COUNT],
            last_wdt_warning_ms: -10000,
        }
    }

    pub const fn config(&self) -> &Config {
        &self.cfg
    }

    /// Arm the timeout monitor and the node state machine (the supervisor's first act).
    pub fn start(&mut self, now_ms: i64) {
        self.monitor.start(now_ms);
        self.node.start(now_ms);
    }

    // --- liveness and the stall hook -------------------------------------------------------

    pub fn check_in(&mut self, thread: Thread, now_ms: i64) {
        self.liveness.check_in(thread as usize, now_ms);
    }

    /// The pending STALL_THREAD duration for a thread, cleared on reading; the
    /// adapter blocks the thread for that long, as a thread stuck in a blocking
    /// call would be.
    pub fn take_stall(&mut self, thread: Thread) -> u32 {
        core::mem::take(&mut self.stall_ms[thread as usize])
    }

    /// The adapter's receive queue high-water mark, if it has one.
    pub fn note_rx_queue_level(&mut self, queued: u32) {
        if queued > self.rx_queue_hwm {
            self.rx_queue_hwm = queued;
        }
    }

    /// A frame the adapter discarded before dispatch (extended id, remote, FD).
    pub fn note_unknown_frame(&mut self) {
        self.rx_frames = self.rx_frames.saturating_add(1);
        self.rx_unknown = self.rx_unknown.saturating_add(1);
    }

    pub fn note_tx_tick_overruns(&mut self, count: u32) {
        self.tx_tick_overruns = self.tx_tick_overruns.saturating_add(count);
    }

    // --- receive path -----------------------------------------------------------------------

    /// A received standard data frame. Returns what the adapter must do next.
    pub fn on_frame(
        &mut self,
        id: u16,
        data: &[u8],
        now_ms: i64,
        mut emit: impl FnMut(Event),
    ) -> RxAction {
        self.rx_frames = self.rx_frames.saturating_add(1);
        match id {
            EchoRequest::ID => {
                if let Some(request) = EchoRequest::from_buffer(data) {
                    let target = Node::from_raw(request.target_node);
                    if matches!(target, Some(Node::All | Node::SensorEcu)) {
                        return RxAction::Echo {
                            seq: request.seq,
                            payload: request.payload,
                        };
                    }
                }
                RxAction::None
            }
            TestControl::ID => {
                self.test_control(data, now_ms, &mut emit);
                RxAction::None
            }
            ControllerCommand::ID => {
                let (result, msg) = self.command_rx.receive(data);
                if let Some(cmd) = msg {
                    self.monitor.note_received(id, now_ms);
                    self.last_command = cmd;
                    self.have_command = true;
                }
                self.note_rx_result(result);
                RxAction::None
            }
            ControllerHeartbeat::ID => {
                let (result, msg) = self.heartbeat_rx.receive(data);
                if msg.is_some() {
                    self.monitor.note_received(id, now_ms);
                }
                self.note_rx_result(result);
                RxAction::None
            }
            _ => {
                self.rx_unknown = self.rx_unknown.saturating_add(1);
                RxAction::None
            }
        }
    }

    fn note_rx_result(&mut self, result: RxResult) {
        match result {
            RxResult::RejectedCrc | RxResult::RejectedDlc => {
                self.error_flags |= FLAG_CRC_FAULT_SEEN;
            }
            RxResult::RejectedRepeated | RxResult::RejectedOutOfSequence => {
                self.error_flags |= FLAG_COUNTER_FAULT_SEEN;
            }
            RxResult::Accepted | RxResult::AcceptedWithLoss => {}
        }
    }

    /// Answer an echo request; `turnaround_us` is the adapter's receive-to-send time.
    pub fn send_echo(
        &mut self,
        seq: u8,
        payload: u32,
        turnaround_us: u32,
        now_ms: i64,
        send: &mut dyn FnMut(&TxFrame) -> Result<(), SendError>,
    ) -> bool {
        let response = SensorEchoResponse {
            seq,
            payload,
            turnaround: turnaround_us.min(65535) as u16,
        };
        let frame = TxFrame {
            id: SensorEchoResponse::ID,
            dlc: SensorEchoResponse::DLC as u8,
            data: response.to_frame(),
        };
        let sent = self.transmit(&frame, Applied::default(), now_ms, send);
        if sent {
            self.echo_responses = self.echo_responses.saturating_add(1);
        }
        sent
    }

    // --- transmit path ----------------------------------------------------------------------

    /// Protect a message for transmission: counter, CRC and the pending
    /// transmit-side faults. All messages go through here.
    fn protect<M: Message>(
        msg: &M,
        counter: &mut TxCounter,
        faults: &mut TxFaults,
    ) -> (TxFrame, Applied) {
        let mut applied = Applied::default();
        let value = counter.next(faults, &mut applied);
        let mut data = msg.to_frame();
        e2e::protect_frame(M::ID, &mut data, value);
        if faults.corrupt_crc_remaining != 0 {
            faults.corrupt_crc_remaining -= 1;
            applied.corrupt = true;
            data[0] ^= 0x01; // any single-bit error is detected
        }
        (
            TxFrame {
                id: M::ID,
                dlc: M::DLC as u8,
                data,
            },
            applied,
        )
    }

    /// Transmit through the adapter unless the CAN error state machine forbids
    /// it (bus-off backoff/recovery, latched failure), the simulated controller
    /// is bus-off, or a test hook suppresses the message. Returns whether the
    /// frame left the node.
    fn transmit(
        &mut self,
        frame: &TxFrame,
        applied: Applied,
        now_ms: i64,
        send: &mut dyn FnMut(&TxFrame) -> Result<(), SendError>,
    ) -> bool {
        let suppressed = (frame.id == SensorHeartbeat::ID
            && now_ms < self.tx_faults.suppress_heartbeat_until_ms)
            || (frame.id == self.tx_faults.suppress_id
                && now_ms < self.tx_faults.suppress_id_until_ms);
        let mut sent = false;
        // Nothing leaves a node whose controller is bus-off or whose recovery is exhausted.
        // The order is the firmware's: the application's suppression hook decides before
        // the frame reaches the (simulated) controller, so a suppressed frame counts as
        // suppressed even while that controller is bus-off.
        if self.can_sm.tx_allowed() {
            if suppressed {
                self.tx_faults.suppressed = self.tx_faults.suppressed.saturating_add(1);
            } else if self.sim_active && self.sim.bus_off() {
                self.tx_blocked_bus_off = self.tx_blocked_bus_off.saturating_add(1);
            // what a real bus-off controller does
            } else {
                sent = send(frame).is_ok();
                if sent {
                    self.tx_sent = self.tx_sent.saturating_add(1);
                    if self.sim_active {
                        // Real transmit results also drive the simulated counters, so an
                        // injected error burst heals the way a real controller's would.
                        self.sim_apply(CounterEvent::TxSuccess, 1, now_ms);
                    }
                }
            }
        }
        // Settle the faults applied by protect(): count them if the frame left the
        // node, otherwise hand them back so they land on the next frame that does.
        let f = &mut self.tx_faults;
        if sent {
            f.corrupted = f.corrupted.saturating_add(u32::from(applied.corrupt));
            f.skipped = f.skipped.saturating_add(u32::from(applied.skip));
            f.frozen = f.frozen.saturating_add(u32::from(applied.freeze));
        } else {
            f.corrupt_crc_remaining = f
                .corrupt_crc_remaining
                .saturating_add(u32::from(applied.corrupt));
            f.skip_counter_remaining = f
                .skip_counter_remaining
                .saturating_add(u32::from(applied.skip));
            f.freeze_counter_remaining = f
                .freeze_counter_remaining
                .saturating_add(u32::from(applied.freeze));
        }
        sent
    }

    /// The TX thread's 10 ms tick: the sensor sample every tick, the battery
    /// status and the heartbeat in their 100 ms slots.
    pub fn tx_tick(
        &mut self,
        tick: u32,
        now_ms: i64,
        send: &mut dyn FnMut(&TxFrame) -> Result<(), SendError>,
    ) {
        self.model.step(now_ms);

        let mut data = SensorData::default();
        data.set_sensor_status(SensorStatus::Ok);
        data.set_speed_phys(self.model.speed_mps);
        data.set_yaw_rate_phys(self.model.yaw_rate_dps);
        data.set_accel_x_phys(self.model.accel_x_mps2);
        let (frame, applied) = Self::protect(&data, &mut self.data_counter, &mut self.tx_faults);
        if self.transmit(&frame, applied, now_ms, send) {
            self.tx_data = self.tx_data.saturating_add(1);
        }

        if tick % TICKS_PER_100MS == BATTERY_SLOT {
            let mut battery = BatteryStatus::default();
            battery.set_pack_voltage_phys(self.model.voltage_v);
            battery.set_pack_current_phys(self.model.current_a);
            battery.set_state_of_charge_phys(self.model.soc_pct);
            battery.set_temperature_phys(self.model.temperature_c);
            battery.low_voltage = u8::from(self.model.soc_pct < 25.0);
            battery.over_temperature = u8::from(self.model.temperature_c > 60.0);
            battery.charging = 0;
            let (frame, applied) =
                Self::protect(&battery, &mut self.battery_counter, &mut self.tx_faults);
            if self.transmit(&frame, applied, now_ms, send) {
                self.tx_battery = self.tx_battery.saturating_add(1);
            }
        }
        if tick % TICKS_PER_100MS == HEARTBEAT_SLOT {
            let mut hb = SensorHeartbeat::default();
            hb.set_node_state(self.node.state());
            hb.bus_off_count = self.can_sm.counters().bus_off_events.min(63) as u8;
            hb.error_flags = self.error_flags;
            hb.uptime = now_ms as u32;
            hb.set_can_state(self.can_state());
            let (frame, applied) =
                Self::protect(&hb, &mut self.heartbeat_counter, &mut self.tx_faults);
            if self.transmit(&frame, applied, now_ms, send) {
                self.tx_heartbeat = self.tx_heartbeat.saturating_add(1);
            }
        }
    }

    // --- the simulated CAN controller (test hooks) -------------------------------------------

    /// The controller state: the simulated model once a test fed it, else what a
    /// virtual bus always reports.
    pub fn can_state(&self) -> CanState {
        if self.sim_active {
            self.sim.state()
        } else {
            CanState::ErrorActive
        }
    }

    pub fn tec(&self) -> u8 {
        if self.sim_active {
            self.sim.tec()
        } else {
            0
        }
    }

    pub fn rec(&self) -> u8 {
        if self.sim_active {
            self.sim.rec()
        } else {
            0
        }
    }

    fn sim_apply(&mut self, event: CounterEvent, count: u32, now_ms: i64) {
        self.sim_active = true; // from the first injected event on, the model is the controller
        for _ in 0..count {
            self.sim.apply(event);
        }
        let state = self.sim.state();
        if state != self.sim_last_state {
            self.sim_last_state = state;
            self.push_can_event(CanEvent {
                state,
                tec: self.sim.tec(),
                rec: self.sim.rec(),
                t_ms: now_ms,
            });
        }
    }

    fn push_can_event(&mut self, event: CanEvent) {
        match self.can_events.iter_mut().find(|slot| slot.is_none()) {
            Some(slot) => *slot = Some(event),
            None => self.can_events_dropped = self.can_events_dropped.saturating_add(1),
        }
    }

    // --- supervisor ---------------------------------------------------------------------------

    /// The 5 ms supervisor tick. Returns whether the watchdog is to be fed.
    pub fn supervisor_tick(&mut self, now_ms: i64, mut emit: impl FnMut(Event)) -> bool {
        self.complete_sim_recovery(now_ms);
        self.process_can_events(now_ms, &mut emit);
        self.run_bus_off_recovery(now_ms, &mut emit);
        self.run_timeouts(now_ms, &mut emit);
        self.run_node_state(now_ms, &mut emit);
        self.run_watchdog(now_ms, &mut emit)
    }

    fn complete_sim_recovery(&mut self, now_ms: i64) {
        if let Some(due) = self.sim_recover_due_ms {
            if now_ms >= due {
                self.sim_recover_due_ms = None;
                self.sim.recover();
                self.sim_last_state = self.sim.state();
                // Stamped with the due time, when the controller became error-active (the
                // C++ HAL's delayed work item runs then), not with the tick that notices it:
                // the recovery duration and the stable window start from that instant.
                self.push_can_event(CanEvent {
                    state: CanState::ErrorActive,
                    tec: 0,
                    rec: 0,
                    t_ms: due,
                });
            }
        }
    }

    fn process_can_events(&mut self, now_ms: i64, emit: &mut impl FnMut(Event)) {
        for slot in &mut self.can_events {
            let Some(ev) = slot.take() else { continue };
            let previous = self.can_sm.controller_state();
            let before = self.can_sm.phase();
            self.can_sm.on_controller_state(ev.state, ev.t_ms);
            let after = self.can_sm.phase();
            if ev.state == CanState::BusOff {
                self.error_flags |= FLAG_BUS_OFF_SEEN;
            }
            emit(Event::CanStateChanged {
                previous,
                state: ev.state,
                tec: ev.tec,
                rec: ev.rec,
                before,
                after,
                polled: false,
            });
        }
        // Level-triggered input: one state read per tick closes the gap a
        // change callback leaves (a driver that reports only worse states).
        let polled = self.can_state();
        let previous = self.can_sm.controller_state();
        let before = self.can_sm.phase();
        self.can_sm.poll_controller_state(polled, now_ms);
        let after = self.can_sm.phase();
        let changed = after != before || self.can_sm.controller_state() != previous;
        if changed {
            if polled == CanState::BusOff {
                self.error_flags |= FLAG_BUS_OFF_SEEN;
            }
            emit(Event::CanStateChanged {
                previous,
                state: polled,
                tec: self.tec(),
                rec: self.rec(),
                before,
                after,
                polled: true,
            });
        }
    }

    fn run_bus_off_recovery(&mut self, now_ms: i64, emit: &mut impl FnMut(Event)) {
        let backoff_ms = self.can_sm.current_backoff_ms();
        if self.can_sm.tick(now_ms) != Action::StartRecovery {
            return;
        }
        let attempt = self.can_sm.counters().recovery_attempts;
        // The simulated controller recovers after the modelled 128 x 11 bit times;
        // a virtual bus has no manual recovery of its own, so without the
        // simulation the request is refused, as the SocketCAN driver refuses it.
        let accepted = self.sim_active && self.sim.bus_off();
        if accepted {
            self.sim_recover_due_ms = Some(now_ms + i64::from(self.cfg.sim_recovery_delay_ms));
        }
        self.can_sm.on_recovery_request_result(accepted, now_ms);
        emit(Event::RecoveryAttempt {
            attempt,
            backoff_ms,
            accepted,
        });
        if self.can_sm.failed() {
            emit(Event::RecoveryExhausted { attempts: attempt });
        }
    }

    fn run_timeouts(&mut self, now_ms: i64, emit: &mut impl FnMut(Event)) {
        let mut lost: [Option<MonitoredMessage>; 2] = [None; 2];
        let mut n = 0;
        self.monitor.tick(now_ms, |entry| {
            if n < lost.len() {
                lost[n] = Some(entry.cfg);
                n += 1;
            }
        });
        if n > 0 {
            self.error_flags |= FLAG_RX_TIMEOUT_SEEN;
        }
        for cfg in lost.iter().flatten() {
            // Forget the counter history, so a sender that restarts is accepted.
            if cfg.id == ControllerCommand::ID {
                self.command_rx.reset_sequence();
            } else if cfg.id == ControllerHeartbeat::ID {
                self.heartbeat_rx.reset_sequence();
            }
            emit(Event::Timeout {
                name: cfg.name,
                timeout_ms: cfg.timeout_ms,
            });
        }
    }

    fn inputs(&self) -> NodeInputs {
        let m = &self.monitor;
        NodeInputs {
            critical_inputs_alive: m.all_alive(),
            critical_inputs_lost: m.lost(ControllerCommand::ID) || m.lost(ControllerHeartbeat::ID),
            degraded_quality: self.command_rx.stats().consecutive_rejects
                >= self.cfg.quality_fault_threshold
                || self.heartbeat_rx.stats().consecutive_rejects
                    >= self.cfg.quality_fault_threshold,
            can_failed: self.can_sm.failed(),
            bus_off: self.can_sm.bus_off(),
        }
    }

    fn run_node_state(&mut self, now_ms: i64, emit: &mut impl FnMut(Event)) {
        let inputs = self.inputs();
        let before = self.node.state();
        if self.node.update(&inputs, now_ms) {
            emit(Event::NodeState {
                before,
                after: self.node.state(),
                reason: self.node.last_reason(),
            });
        }
    }

    fn run_watchdog(&mut self, now_ms: i64, emit: &mut impl FnMut(Event)) -> bool {
        let stale = self.liveness.stale_mask(now_ms);
        self.liveness_stale_mask = stale;
        if stale == 0 {
            self.wdt_feeds = self.wdt_feeds.saturating_add(1);
            return true;
        }
        self.wdt_skipped = self.wdt_skipped.saturating_add(1);
        self.error_flags |= FLAG_WATCHDOG_WARNING;
        if now_ms - self.last_wdt_warning_ms >= 1000 {
            self.last_wdt_warning_ms = now_ms;
            emit(Event::WatchdogNotFed { stale_mask: stale });
        }
        false
    }

    // --- test control ------------------------------------------------------------------------

    fn test_control(&mut self, data: &[u8], now_ms: i64, emit: &mut impl FnMut(Event)) {
        let Some(tc) = TestControl::from_buffer(data) else {
            return;
        };
        if tc.magic != TestControl::MAGIC {
            emit(Event::TestControl(TestControlEvent::WrongMagic(tc.magic)));
            return;
        }
        if !matches!(
            Node::from_raw(tc.target_node),
            Some(Node::All | Node::SensorEcu)
        ) {
            return;
        }
        self.test_commands = self.test_commands.saturating_add(1);
        self.error_flags |= FLAG_TEST_HOOK_ACTIVE;
        let count = if tc.arg0 == 0 { 1 } else { u32::from(tc.arg0) }; // "at least one" for the counting commands
        let event = match TestCommand::from_raw(tc.command) {
            Some(TestCommand::Nop) | None => return,
            Some(TestCommand::ResetFaults) => {
                self.reset_faults(now_ms);
                TestControlEvent::ResetFaults
            }
            Some(TestCommand::InjectCanError) => {
                // Transmit error events on the simulated controller (TEC += 8 each):
                // 12 reach the warning limit, 16 error-passive, 32 bus-off.
                self.sim_apply(CounterEvent::TxError, count, now_ms);
                TestControlEvent::InjectCanError {
                    count,
                    state: self.can_state(),
                    tec: self.tec(),
                    rec: self.rec(),
                }
            }
            Some(TestCommand::ForceBusOff) => {
                self.sim_apply(CounterEvent::TxError, 32, now_ms);
                TestControlEvent::ForceBusOff {
                    state: self.can_state(),
                }
            }
            Some(TestCommand::SuppressHeartbeat) => {
                self.tx_faults.suppress_heartbeat_until_ms = now_ms + i64::from(tc.arg0);
                TestControlEvent::SuppressHeartbeat {
                    ms: u32::from(tc.arg0),
                }
            }
            Some(TestCommand::CorruptCrc) => {
                self.tx_faults.corrupt_crc_remaining =
                    self.tx_faults.corrupt_crc_remaining.saturating_add(count);
                TestControlEvent::CorruptCrc { count }
            }
            Some(TestCommand::SkipCounter) => {
                self.tx_faults.skip_counter_remaining =
                    self.tx_faults.skip_counter_remaining.saturating_add(count);
                TestControlEvent::SkipCounter { count }
            }
            Some(TestCommand::FreezeCounter) => {
                self.tx_faults.freeze_counter_remaining = self
                    .tx_faults
                    .freeze_counter_remaining
                    .saturating_add(count);
                TestControlEvent::FreezeCounter { count }
            }
            Some(TestCommand::SuppressMessage) => {
                self.tx_faults.suppress_id = tc.arg0;
                self.tx_faults.suppress_id_until_ms = now_ms + i64::from(tc.arg1);
                TestControlEvent::SuppressMessage {
                    id: tc.arg0,
                    ms: u32::from(tc.arg1),
                }
            }
            Some(TestCommand::StallThread) => match Thread::from_index(u32::from(tc.arg0)) {
                Some(thread) => {
                    self.stall_ms[thread as usize] = u32::from(tc.arg1);
                    TestControlEvent::StallThread {
                        thread: u32::from(tc.arg0),
                        ms: u32::from(tc.arg1),
                    }
                }
                None => TestControlEvent::NoSuchThread(u32::from(tc.arg0)),
            },
        };
        emit(Event::TestControl(event));
    }

    /// Clear latched faults: FAULT state, the CAN recovery backoff,
    /// error_flags, the watchdog skip count, the per-message receive fault
    /// counters, pending test faults and the hook counters (cumulative tx/rx
    /// and bus-off counters are kept).
    pub fn reset_faults(&mut self, now_ms: i64) {
        self.can_sm.reset(now_ms);
        self.node.reset(now_ms);
        self.error_flags = 0;
        self.wdt_skipped = 0;
        self.tx_faults = TxFaults::default();
        self.stall_ms = [0; THREAD_COUNT];
        self.command_rx.reset_stats();
        self.heartbeat_rx.reset_stats();
    }

    // --- status -------------------------------------------------------------------------------

    fn rx_status(&self, name: &'static str, stats: &RxStats, id: u16, now_ms: i64) -> RxStatus {
        RxStatus {
            name,
            accepted: stats.accepted,
            crc_faults: stats.crc_faults,
            counter_faults: stats.repeated + stats.out_of_sequence,
            dlc_faults: stats.dlc_faults,
            skipped: stats.skipped,
            lost_frames: stats.lost_frames,
            timeouts: self.monitor.find(id).map_or(0, |e| e.timeouts),
            age_ms: self.monitor.age_ms(id, now_ms),
            alive: self.monitor.alive(id),
        }
    }

    pub fn status(&self, now_ms: i64) -> Status {
        let c = self.can_sm.counters();
        Status {
            uptime_ms: now_ms,
            node_state: self.node.state(),
            node_reason: self.node.last_reason(),
            node_state_age_ms: now_ms - self.node.entered_at_ms(),
            safe_state_entries: self.node.safe_state_entries(),
            error_flags: self.error_flags,
            can_phase: self.can_sm.phase(),
            can_state: self.can_state(),
            tec: self.tec(),
            rec: self.rec(),
            simulation_active: self.sim_active,
            bus_off_events: c.bus_off_events,
            passive_events: c.passive_events,
            recovery_attempts: c.recovery_attempts,
            recoveries_ok: c.recoveries_ok,
            recovery_failures: c.recovery_failures,
            last_recovery_duration_ms: c.last_recovery_duration_ms,
            current_backoff_ms: self.can_sm.current_backoff_ms(),
            tx_sent: self.tx_sent,
            tx_blocked_bus_off: self.tx_blocked_bus_off,
            tx_tick_overruns: self.tx_tick_overruns,
            rx_frames: self.rx_frames,
            rx_unknown: self.rx_unknown,
            rx_queue_hwm: self.rx_queue_hwm,
            rx_queue_depth: self.cfg.rx_queue_depth,
            can_events_dropped: self.can_events_dropped,
            echo_responses: self.echo_responses,
            test_commands: self.test_commands,
            wdt_timeout_ms: self.cfg.watchdog_timeout_ms,
            wdt_feeds: self.wdt_feeds,
            wdt_skipped: self.wdt_skipped,
            liveness_stale_mask: self.liveness_stale_mask,
            hooks: HookCounters {
                suppressed: self.tx_faults.suppressed,
                corrupted: self.tx_faults.corrupted,
                skipped: self.tx_faults.skipped,
                frozen: self.tx_faults.frozen,
            },
            rx: [
                self.rx_status(
                    ControllerCommand::NAME,
                    self.command_rx.stats(),
                    ControllerCommand::ID,
                    now_ms,
                ),
                self.rx_status(
                    ControllerHeartbeat::NAME,
                    self.heartbeat_rx.stats(),
                    ControllerHeartbeat::ID,
                    now_ms,
                ),
            ],
            detail: Detail {
                tx_data: self.tx_data,
                tx_battery: self.tx_battery,
                tx_heartbeat: self.tx_heartbeat,
                speed_cm_s: (self.model.speed_mps * 100.0) as i32,
                yaw_0_1deg_s: (self.model.yaw_rate_dps * 10.0) as i32,
                soc_0_5pct: (self.model.soc_pct * 2.0) as u32,
                have_command: self.have_command,
                command_mode: self.last_command.mode,
                torque_0_01nm: i32::from(self.last_command.torque_cmd),
                actuators_enabled: u32::from(self.last_command.actuators_enabled),
            },
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::messages::{ControllerMode, SensorData};
    use std::vec::Vec;

    /// A fake bus: records what the ECU sends and can be asked to answer the ECU.
    struct Bus {
        sent: Vec<TxFrame>,
    }

    impl Bus {
        fn new() -> Self {
            Self { sent: Vec::new() }
        }

        fn sender(&mut self) -> impl FnMut(&TxFrame) -> Result<(), SendError> + '_ {
            |f| {
                self.sent.push(*f);
                Ok(())
            }
        }

        fn of(&self, id: u16) -> Vec<&TxFrame> {
            self.sent.iter().filter(|f| f.id == id).collect()
        }
    }

    fn command(counter: u8, mode: ControllerMode) -> [u8; 8] {
        let mut cmd = ControllerCommand::default();
        cmd.set_mode(mode);
        let mut frame = cmd.to_frame();
        e2e::protect_frame(ControllerCommand::ID, &mut frame, counter);
        frame
    }

    fn heartbeat(counter: u8) -> [u8; 8] {
        let mut frame = ControllerHeartbeat::default().to_frame();
        e2e::protect_frame(ControllerHeartbeat::ID, &mut frame, counter);
        frame
    }

    fn test_control(command: TestCommand, arg0: u16, arg1: u16) -> [u8; 8] {
        TestControl {
            magic: TestControl::MAGIC,
            target_node: Node::SensorEcu as u8,
            command: command as u8,
            arg0,
            arg1,
        }
        .to_frame()
    }

    /// Run the ECU for `ms` milliseconds with the controller alive: commands every
    /// 20 ms, heartbeats every 100 ms, the supervisor every 5 ms, TX every 10 ms.
    fn run(
        ecu: &mut SensorEcu,
        bus: &mut Bus,
        from_ms: i64,
        ms: i64,
        counters: &mut (u8, u8),
        events: &mut Vec<Event>,
    ) {
        for t in from_ms..from_ms + ms {
            if t % 20 == 0 {
                ecu.on_frame(
                    ControllerCommand::ID,
                    &command(counters.0, ControllerMode::Normal),
                    t,
                    |_| {},
                );
                counters.0 = (counters.0 + 1) & 0xF;
            }
            if t % 100 == 0 {
                ecu.on_frame(ControllerHeartbeat::ID, &heartbeat(counters.1), t, |_| {});
                counters.1 = (counters.1 + 1) & 0xF;
            }
            if t % 10 == 0 {
                ecu.tx_tick((t / 10) as u32, t, &mut bus.sender());
            }
            if t % 5 == 0 {
                for thread in [Thread::Rx, Thread::Tx, Thread::Supervisor] {
                    ecu.check_in(thread, t);
                }
                ecu.supervisor_tick(t, |e| events.push(e));
            }
        }
    }

    #[test]
    fn boots_to_running_and_publishes_the_three_messages_protected() {
        let mut ecu = SensorEcu::new(Config::default());
        let mut bus = Bus::new();
        let mut events = Vec::new();
        ecu.start(0);
        run(&mut ecu, &mut bus, 0, 1000, &mut (0, 0), &mut events);
        assert_eq!(bus.of(SensorData::ID).len(), 100);
        assert_eq!(bus.of(BatteryStatus::ID).len(), 10);
        assert_eq!(bus.of(SensorHeartbeat::ID).len(), 10);
        for f in &bus.sent {
            assert!(
                e2e::crc_ok(f.id, f.payload()),
                "frame 0x{:03X} is protected",
                f.id
            );
        }
        let counters: Vec<u8> = bus
            .of(SensorData::ID)
            .iter()
            .map(|f| e2e::counter_of(f.payload()))
            .collect();
        assert_eq!(
            &counters[..17],
            &[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0]
        );
        assert!(matches!(
            events[0],
            Event::NodeState {
                before: NodeState::Boot,
                after: NodeState::Running,
                ..
            }
        ));
        let st = ecu.status(1000);
        assert_eq!(st.node_state, NodeState::Running);
        assert_eq!(st.rx[0].accepted, 50);
        assert_eq!(st.rx[1].accepted, 10);
        assert_eq!(st.tx_sent, 120);
        assert_eq!(st.wdt_skipped, 0);
        assert!(st.detail.have_command);
        let hb = SensorHeartbeat::from_buffer(bus.of(SensorHeartbeat::ID)[9].payload()).unwrap();
        assert_eq!(hb.node_state_enum(), Some(NodeState::Running));
        assert_eq!(hb.can_state_enum(), Some(CanState::ErrorActive));
        let data = SensorData::from_buffer(bus.of(SensorData::ID)[50].payload()).unwrap();
        assert!(
            (data.speed_phys() - 12.4).abs() < 0.2,
            "the model's speed near t=0.5 s"
        );
    }

    #[test]
    fn losing_the_command_degrades_and_the_restarted_sender_is_accepted() {
        let mut ecu = SensorEcu::new(Config::default());
        let mut bus = Bus::new();
        let mut events = Vec::new();
        let mut counters = (0u8, 0u8);
        ecu.start(0);
        run(&mut ecu, &mut bus, 0, 300, &mut counters, &mut events);
        events.clear();
        // Silence from the controller: only heartbeats keep coming.
        for t in 300..500 {
            if t % 100 == 0 {
                ecu.on_frame(ControllerHeartbeat::ID, &heartbeat(counters.1), t, |_| {});
                counters.1 = (counters.1 + 1) & 0xF;
            }
            if t % 5 == 0 {
                for thread in [Thread::Rx, Thread::Tx, Thread::Supervisor] {
                    ecu.check_in(thread, t);
                }
                ecu.supervisor_tick(t, |e| events.push(e));
            }
        }
        assert!(events.iter().any(|e| matches!(
            e,
            Event::Timeout {
                name: "CONTROLLER_COMMAND",
                timeout_ms: 70
            }
        )));
        assert!(events.iter().any(|e| matches!(
            e,
            Event::NodeState {
                after: NodeState::Degraded,
                reason: "critical input lost",
                ..
            }
        )));
        let st = ecu.status(500);
        assert_eq!(st.rx[0].timeouts, 1);
        assert_eq!(st.safe_state_entries, 0, "the sensor has no safe state");
        assert_ne!(st.error_flags & FLAG_RX_TIMEOUT_SEEN, 0);
        // The controller restarts with counter 0: accepted, not out of sequence.
        ecu.on_frame(
            ControllerCommand::ID,
            &command(0, ControllerMode::Normal),
            500,
            |_| {},
        );
        ecu.supervisor_tick(505, |e| events.push(e));
        assert!(events.iter().any(|e| matches!(
            e,
            Event::NodeState {
                after: NodeState::Running,
                ..
            }
        )));
        assert_eq!(ecu.status(505).rx[0].counter_faults, 0);
    }

    #[test]
    fn crc_and_counter_faults_are_counted_and_flagged() {
        let mut ecu = SensorEcu::new(Config::default());
        ecu.start(0);
        ecu.on_frame(
            ControllerCommand::ID,
            &command(0, ControllerMode::Normal),
            0,
            |_| {},
        );
        let mut bad = command(1, ControllerMode::Normal);
        bad[4] ^= 0x80;
        ecu.on_frame(ControllerCommand::ID, &bad, 20, |_| {});
        ecu.on_frame(
            ControllerCommand::ID,
            &command(0, ControllerMode::Normal),
            40,
            |_| {},
        );
        ecu.on_frame(
            ControllerCommand::ID,
            &command(3, ControllerMode::Normal),
            60,
            |_| {},
        );
        ecu.on_frame(0x7FF, &[0; 8], 61, |_| {});
        let st = ecu.status(70);
        // counters 0, (1 rejected), 0 repeated, 3 skipped: counters 1 and 2 were never accepted
        assert_eq!(
            (
                st.rx[0].crc_faults,
                st.rx[0].counter_faults,
                st.rx[0].skipped,
                st.rx[0].lost_frames
            ),
            (1, 1, 1, 2)
        );
        assert_eq!(
            st.error_flags & (FLAG_CRC_FAULT_SEEN | FLAG_COUNTER_FAULT_SEEN),
            FLAG_CRC_FAULT_SEEN | FLAG_COUNTER_FAULT_SEEN
        );
        assert_eq!(st.rx_unknown, 1);
        assert_eq!(st.rx_frames, 5);
    }

    #[test]
    fn the_transmit_hooks_do_what_their_names_say_and_settle_their_counts() {
        let mut ecu = SensorEcu::new(Config::default());
        let mut bus = Bus::new();
        let mut events = Vec::new();
        ecu.start(0);
        ecu.on_frame(
            TestControl::ID,
            &test_control(TestCommand::CorruptCrc, 2, 0),
            0,
            |e| events.push(e),
        );
        ecu.on_frame(
            TestControl::ID,
            &test_control(TestCommand::SkipCounter, 1, 0),
            0,
            |e| events.push(e),
        );
        ecu.on_frame(
            TestControl::ID,
            &test_control(TestCommand::SuppressHeartbeat, 1000, 0),
            0,
            |e| events.push(e),
        );
        assert_eq!(events.len(), 3);
        run(&mut ecu, &mut bus, 0, 1000, &mut (0, 0), &mut events);
        let data: Vec<&TxFrame> = bus.of(SensorData::ID);
        let bad: Vec<bool> = data
            .iter()
            .map(|f| !e2e::crc_ok(f.id, f.payload()))
            .collect();
        assert_eq!(
            bad.iter().filter(|b| **b).count(),
            2,
            "exactly two frames corrupted"
        );
        assert_eq!(
            e2e::counter_of(data[1].payload()),
            2,
            "one counter value skipped on the second frame"
        );
        assert!(
            bus.of(SensorHeartbeat::ID).is_empty(),
            "the heartbeat is suppressed for the whole second"
        );
        let st = ecu.status(1000);
        assert_eq!(
            (st.hooks.corrupted, st.hooks.skipped, st.hooks.suppressed),
            (2, 1, 10)
        );
        assert_eq!(st.test_commands, 3);
        assert_ne!(st.error_flags & FLAG_TEST_HOOK_ACTIVE, 0);
        ecu.on_frame(
            TestControl::ID,
            &test_control(TestCommand::FreezeCounter, 1, 0),
            1000,
            |_| {},
        );
        ecu.tx_tick(100, 1000, &mut bus.sender());
        ecu.tx_tick(101, 1010, &mut bus.sender());
        let last: Vec<u8> = bus
            .of(SensorData::ID)
            .iter()
            .rev()
            .take(3)
            .map(|f| e2e::counter_of(f.payload()))
            .collect();
        assert_eq!(
            last[1], last[2],
            "the frozen frame repeats the previous counter"
        );
        assert_eq!(last[0], (last[1] + 1) & 0xF);
        assert_eq!(ecu.status(1010).hooks.frozen, 1);
        // RESET_FAULTS clears the hook counters and the flags.
        ecu.on_frame(
            TestControl::ID,
            &test_control(TestCommand::ResetFaults, 0, 0),
            1020,
            |_| {},
        );
        let st = ecu.status(1020);
        assert_eq!(st.hooks, HookCounters::default());
        assert_eq!(st.error_flags & !FLAG_TEST_HOOK_ACTIVE, 0);
    }

    #[test]
    fn bus_off_through_the_hook_recovers_after_the_backoff_and_is_an_input_loss() {
        let mut ecu = SensorEcu::new(Config::default());
        let mut bus = Bus::new();
        let mut events = Vec::new();
        let mut counters = (0u8, 0u8);
        ecu.start(0);
        run(&mut ecu, &mut bus, 0, 200, &mut counters, &mut events);
        events.clear();
        ecu.on_frame(
            TestControl::ID,
            &test_control(TestCommand::ForceBusOff, 0, 0),
            200,
            |e| events.push(e),
        );
        assert!(matches!(
            events.last(),
            Some(Event::TestControl(TestControlEvent::ForceBusOff {
                state: CanState::BusOff
            }))
        ));
        let before = bus.sent.len();
        run(&mut ecu, &mut bus, 200, 100, &mut counters, &mut events);
        assert_eq!(
            bus.sent.len(),
            before,
            "nothing leaves the node while bus-off"
        );
        assert!(events.iter().any(|e| matches!(
            e,
            Event::CanStateChanged {
                state: CanState::BusOff,
                after: Phase::BusOffBackoff,
                ..
            }
        )));
        assert!(events.iter().any(|e| matches!(
            e,
            Event::NodeState {
                after: NodeState::Degraded,
                reason: "bus off",
                ..
            }
        )));
        run(&mut ecu, &mut bus, 300, 100, &mut counters, &mut events);
        assert!(events.iter().any(|e| matches!(
            e,
            Event::RecoveryAttempt {
                attempt: 1,
                backoff_ms: 100,
                accepted: true
            }
        )));
        assert!(events.iter().any(|e| matches!(
            e,
            Event::CanStateChanged {
                state: CanState::ErrorActive,
                after: Phase::Active,
                ..
            }
        )));
        let st = ecu.status(400);
        assert_eq!(
            (
                st.bus_off_events,
                st.recovery_attempts,
                st.recoveries_ok,
                st.recovery_failures
            ),
            (1, 1, 1, 0)
        );
        assert_eq!(
            st.last_recovery_duration_ms, 103,
            "100 ms backoff on the 5 ms tick + 3 ms modelled recovery, stamped at the due time as the C++ HAL does"
        );
        assert_eq!(st.node_state, NodeState::Running);
        assert!(bus.sent.len() > before, "transmission resumed");
        assert_ne!(st.error_flags & FLAG_BUS_OFF_SEEN, 0);
    }

    #[test]
    fn a_stalled_thread_withholds_the_feed_after_the_liveness_window() {
        let mut ecu = SensorEcu::new(Config::default());
        let mut events = Vec::new();
        ecu.start(0);
        ecu.on_frame(
            TestControl::ID,
            &test_control(TestCommand::StallThread, 1, 1500),
            0,
            |e| events.push(e),
        );
        assert_eq!(ecu.take_stall(Thread::Tx), 1500);
        assert_eq!(ecu.take_stall(Thread::Tx), 0, "cleared on reading");
        let mut fed = Vec::new();
        for t in (0..=400).step_by(5) {
            ecu.check_in(Thread::Rx, t);
            ecu.check_in(Thread::Supervisor, t);
            if t == 0 {
                ecu.check_in(Thread::Tx, 0);
            }
            fed.push((t, ecu.supervisor_tick(t, |e| events.push(e))));
        }
        assert!(
            fed.iter().filter(|(t, _)| *t <= 300).all(|(_, f)| *f),
            "fed while the TX thread is within the window"
        );
        assert!(
            fed.iter().filter(|(t, _)| *t > 300).all(|(_, f)| !*f),
            "withheld once it is stale"
        );
        assert!(events
            .iter()
            .any(|e| matches!(e, Event::WatchdogNotFed { stale_mask: 0b010 })));
        let st = ecu.status(400);
        assert_eq!(st.liveness_stale_mask, 0b010);
        assert!(st.wdt_skipped > 0);
        assert_ne!(st.error_flags & FLAG_WATCHDOG_WARNING, 0);
        ecu.on_frame(
            TestControl::ID,
            &test_control(TestCommand::StallThread, 9, 10),
            0,
            |e| events.push(e),
        );
        assert!(matches!(
            events.last(),
            Some(Event::TestControl(TestControlEvent::NoSuchThread(9)))
        ));
    }

    #[test]
    fn echo_requests_are_answered_with_the_payload_and_the_turnaround() {
        let mut ecu = SensorEcu::new(Config::default());
        let mut bus = Bus::new();
        ecu.start(0);
        let request = EchoRequest {
            target_node: Node::All as u8,
            seq: 42,
            payload: 0xDEADBEEF,
        };
        let action = ecu.on_frame(
            EchoRequest::ID,
            &request.to_frame()[..EchoRequest::DLC],
            10,
            |_| {},
        );
        assert_eq!(
            action,
            RxAction::Echo {
                seq: 42,
                payload: 0xDEADBEEF
            }
        );
        assert!(ecu.send_echo(42, 0xDEADBEEF, 70000, 10, &mut bus.sender()));
        let f = bus.of(SensorEchoResponse::ID)[0];
        assert_eq!(f.dlc, 7);
        let response = SensorEchoResponse::from_buffer(f.payload()).unwrap();
        assert_eq!(
            (response.seq, response.payload, response.turnaround),
            (42, 0xDEADBEEF, 65535)
        );
        let other = EchoRequest {
            target_node: Node::ControllerEcu as u8,
            seq: 1,
            payload: 0,
        };
        assert_eq!(
            ecu.on_frame(
                EchoRequest::ID,
                &other.to_frame()[..EchoRequest::DLC],
                11,
                |_| {}
            ),
            RxAction::None
        );
        assert_eq!(ecu.status(11).echo_responses, 1);
    }

    #[test]
    fn a_wrong_magic_and_another_node_s_command_are_ignored() {
        let mut ecu = SensorEcu::new(Config::default());
        let mut events = Vec::new();
        let mut tc = test_control(TestCommand::CorruptCrc, 5, 0);
        tc[0] ^= 0xFF;
        ecu.on_frame(TestControl::ID, &tc, 0, |e| events.push(e));
        assert!(matches!(
            events[0],
            Event::TestControl(TestControlEvent::WrongMagic(_))
        ));
        let controller = TestControl {
            magic: TestControl::MAGIC,
            target_node: Node::ControllerEcu as u8,
            command: TestCommand::CorruptCrc as u8,
            arg0: 5,
            arg1: 0,
        };
        ecu.on_frame(TestControl::ID, &controller.to_frame(), 0, |e| {
            events.push(e);
        });
        assert_eq!(events.len(), 1);
        assert_eq!(ecu.status(0).test_commands, 0);
    }
}
