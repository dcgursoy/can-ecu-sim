//! The sensor ECU on Linux SocketCAN: a thin std adapter around the `no_std`
//! `sensor_core` crate, the Rust counterpart of the Zephyr `native_sim` build.
//!
//! The adapter owns everything the firmware gets from the RTOS and the HAL:
//! five OS threads (CAN receive, 10 ms transmit tick, 5 ms supervisor tick, the
//! shell on stdin, the watchdog), the SocketCAN socket, the clock, and the
//! console. The protocol, the state machines and the application logic are
//! `sensor_core::sensor::SensorEcu`, which never does I/O: the threads feed it
//! frames and ticks and carry out what it returns.
//!
//! Console compatibility with the Zephyr build is deliberate, so the unchanged
//! system test suite (tests/) drives both: log lines in Zephyr's format,
//! `[HH:MM:SS.mmm,uuu] <lvl> module: text`, with the firmware's texts; the
//! `ecu status` and `ecu reset` shell commands with the same output; and a
//! watchdog that, like the simulator's, reboots the node in place (the process
//! re-executes itself, same pid) when the supervisor stops feeding it.
//!
//!     sensor-vcan --can-if=vcan0

use std::fmt;
use std::io::{self, BufRead, Write};
use std::os::unix::process::CommandExt;
use std::process::Command;
use std::sync::atomic::{AtomicI64, AtomicU32, Ordering};
use std::sync::{Arc, Mutex, MutexGuard};
use std::thread;
use std::time::{Duration, Instant};

use sensor_core::messages::ControllerMode;
use sensor_core::sensor::{
    Config, Event, RxAction, SendError, SensorEcu, Status, TestControlEvent, Thread, TxFrame,
    BASE_TICK_MS,
};
use socketcan::{CanFrame, CanSocket, EmbeddedFrame, Frame, Socket, StandardId};

const ROLE: &str = "sensor";
const NODE: u8 = 1; // proto::Node::kSensorEcu
const BANNER: &str = "*** Booting can-ecu-sim sensor ECU (Rust) ***";

// --- clock and log -------------------------------------------------------------------

/// Milliseconds since boot (process start), the firmware's `k_uptime_get()`.
#[derive(Clone, Copy)]
struct Clock(Instant);

impl Clock {
    fn now_ms(self) -> i64 {
        self.0.elapsed().as_millis() as i64
    }

    fn now_us(self) -> i64 {
        self.0.elapsed().as_micros() as i64
    }
}

#[derive(Clone, Copy)]
enum Level {
    Inf,
    Wrn,
    Err,
}

impl Level {
    const fn tag(self) -> &'static str {
        match self {
            Self::Inf => "inf",
            Self::Wrn => "wrn",
            Self::Err => "err",
        }
    }
}

/// One Zephyr-format log line: the uptime as hh:mm:ss.mmm,uuu, the level, the module.
fn log(clock: Clock, level: Level, module: &str, args: fmt::Arguments<'_>) {
    let us = clock.now_us();
    let s = us / 1_000_000;
    let line = format!(
        "[{:02}:{:02}:{:02}.{:03},{:03}] <{}> {}: {}",
        s / 3600,
        (s / 60) % 60,
        s % 60,
        (us / 1000) % 1000,
        us % 1000,
        level.tag(),
        module,
        args
    );
    let stdout = io::stdout();
    let mut out = stdout.lock();
    let _ = writeln!(out, "{line}");
    let _ = out.flush();
}

macro_rules! log_inf {
    ($clock:expr, $module:expr, $($arg:tt)*) => { log($clock, Level::Inf, $module, format_args!($($arg)*)) };
}
macro_rules! log_wrn {
    ($clock:expr, $module:expr, $($arg:tt)*) => { log($clock, Level::Wrn, $module, format_args!($($arg)*)) };
}
macro_rules! log_err {
    ($clock:expr, $module:expr, $($arg:tt)*) => { log($clock, Level::Err, $module, format_args!($($arg)*)) };
}

// --- shared state --------------------------------------------------------------------

/// What the C++ firmware spreads over the HAL's transmit statistics.
#[derive(Default)]
struct TxStats {
    ok: AtomicU32,
    err: AtomicU32,
    busy: AtomicU32,
    rejected: AtomicU32,
}

struct Shared {
    clock: Clock,
    ecu: Mutex<SensorEcu>,
    socket: CanSocket,
    tx: TxStats,
    /// Last watchdog feed, microseconds of uptime; the watchdog thread reboots
    /// the node once it is older than the timeout.
    last_feed_us: AtomicI64,
    watchdog_timeout_ms: u32,
}

impl Shared {
    fn lock(&self) -> MutexGuard<'_, SensorEcu> {
        // A poisoned lock means a thread panicked while holding it; the state is
        // not trustworthy any more, which on the firmware is a fatal error too.
        self.ecu.lock().unwrap_or_else(|_| {
            eprintln!("fatal: ECU state lock poisoned");
            std::process::exit(2)
        })
    }

    fn check_in(&self, thread: Thread) {
        let now = self.clock.now_ms();
        self.lock().check_in(thread, now);
    }

    /// STALL_THREAD test hook: block the calling thread for the requested time,
    /// as a thread stuck in a blocking call would (outside the lock, so the
    /// other threads run on and the supervisor sees the stale check-in).
    fn maybe_stall(&self, thread: Thread) {
        let ms = self.lock().take_stall(thread);
        if ms != 0 {
            log_wrn!(
                self.clock,
                "ecu",
                "test hook: {} thread blocked for {} ms",
                thread.name(),
                ms
            );
            thread::sleep(Duration::from_millis(u64::from(ms)));
        }
    }

    /// Hand a frame to the driver. ENOBUFS/EAGAIN is the driver's "busy" (the
    /// socket's transmit queue is full), anything else a rejection.
    fn send(&self, frame: &TxFrame) -> Result<(), SendError> {
        let Some(id) = StandardId::new(frame.id) else {
            self.tx.rejected.fetch_add(1, Ordering::Relaxed);
            return Err(SendError::Rejected);
        };
        let Some(can) = CanFrame::new(id, frame.payload()) else {
            self.tx.rejected.fetch_add(1, Ordering::Relaxed);
            return Err(SendError::Rejected);
        };
        match self.socket.write_frame(&can) {
            Ok(()) => {
                self.tx.ok.fetch_add(1, Ordering::Relaxed);
                Ok(())
            }
            Err(e)
                if e.kind() == io::ErrorKind::WouldBlock
                    || e.raw_os_error() == Some(libc::ENOBUFS) =>
            {
                self.tx.busy.fetch_add(1, Ordering::Relaxed);
                Err(SendError::Busy)
            }
            Err(e) => {
                self.tx.err.fetch_add(1, Ordering::Relaxed);
                log_err!(
                    self.clock,
                    "hal_can",
                    "send 0x{:03X} failed: {}",
                    frame.id,
                    e
                );
                Err(SendError::Rejected)
            }
        }
    }

    fn feed_watchdog(&self) {
        self.last_feed_us
            .store(self.clock.now_us(), Ordering::Relaxed);
    }

    fn emit(&self, event: Event) {
        let c = self.clock;
        match event {
            Event::CanStateChanged {
                previous,
                state,
                tec,
                rec,
                before,
                after,
                polled: false,
            } => {
                log_wrn!(
                    c,
                    "ecu",
                    "can: {} -> {} (tec={} rec={}) phase {} -> {}",
                    previous.as_str(),
                    state.as_str(),
                    tec,
                    rec,
                    before.as_str(),
                    after.as_str()
                );
            }
            Event::CanStateChanged {
                previous,
                state,
                before,
                after,
                polled: true,
                ..
            } => {
                log_wrn!(
                    c,
                    "ecu",
                    "can (polled): {} -> {} phase {} -> {}",
                    previous.as_str(),
                    state.as_str(),
                    before.as_str(),
                    after.as_str()
                );
            }
            Event::RecoveryAttempt {
                attempt,
                backoff_ms,
                accepted,
            } => {
                // The errno the firmware prints: 0 when the driver took the request,
                // -ENOTSUP when it has no recovery to offer (a virtual bus never has).
                let (verdict, err) = if accepted {
                    ("requested", 0)
                } else {
                    ("refused", -libc::ENOTSUP)
                };
                log_wrn!(
                    c,
                    "ecu",
                    "bus-off recovery attempt {attempt} after {backoff_ms} ms backoff: {verdict} ({err})"
                );
            }
            Event::RecoveryExhausted { attempts } => {
                log_err!(
                    c,
                    "ecu",
                    "bus-off recovery exhausted after {attempts} attempts: node FAULT"
                );
            }
            Event::Timeout { name, timeout_ms } => {
                log_wrn!(c, "ecu", "timeout: {name} not received for {timeout_ms} ms");
            }
            Event::NodeState {
                before,
                after,
                reason,
            } => {
                log_inf!(
                    c,
                    "ecu",
                    "node_state: {} -> {} ({})",
                    before.as_str(),
                    after.as_str(),
                    reason
                );
            }
            Event::WatchdogNotFed { stale_mask } => {
                log_wrn!(c, "ecu", "watchdog not fed: stale threads 0x{stale_mask:x}");
            }
            Event::TestControl(tc) => self.emit_test_control(tc),
        }
    }

    fn emit_test_control(&self, event: TestControlEvent) {
        const M: &str = "test_ctrl";
        let c = self.clock;
        match event {
            TestControlEvent::WrongMagic(magic) => {
                log_wrn!(c, M, "TEST_CONTROL with wrong magic 0x{magic:04x} ignored");
            }
            TestControlEvent::ResetFaults => log_inf!(c, M, "TEST_CONTROL RESET_FAULTS"),
            TestControlEvent::InjectCanError {
                count,
                state,
                tec,
                rec,
            } => {
                log_inf!(
                    c,
                    M,
                    "TEST_CONTROL INJECT_CAN_ERROR x{count} -> {} tec={tec} rec={rec}",
                    state.as_str()
                );
            }
            TestControlEvent::ForceBusOff { state } => {
                log_inf!(c, M, "TEST_CONTROL FORCE_BUS_OFF -> {}", state.as_str());
            }
            TestControlEvent::SuppressHeartbeat { ms } => {
                log_inf!(c, M, "TEST_CONTROL SUPPRESS_HEARTBEAT for {ms} ms");
            }
            TestControlEvent::CorruptCrc { count } => {
                log_inf!(c, M, "TEST_CONTROL CORRUPT_CRC x{count}");
            }
            TestControlEvent::SkipCounter { count } => {
                log_inf!(c, M, "TEST_CONTROL SKIP_COUNTER x{count}");
            }
            TestControlEvent::FreezeCounter { count } => {
                log_inf!(c, M, "TEST_CONTROL FREEZE_COUNTER x{count}");
            }
            TestControlEvent::SuppressMessage { id, ms } => {
                log_inf!(c, M, "TEST_CONTROL SUPPRESS_MESSAGE 0x{id:03x} for {ms} ms");
            }
            TestControlEvent::StallThread { thread, ms } => {
                log_inf!(c, M, "TEST_CONTROL STALL_THREAD {thread} for {ms} ms");
            }
            TestControlEvent::NoSuchThread(thread) => {
                log_wrn!(c, M, "TEST_CONTROL STALL_THREAD: no thread {thread}");
            }
        }
    }
}

// --- threads -------------------------------------------------------------------------

/// Receive thread: every frame from the socket goes to the ECU; echo requests
/// are answered from here so the turnaround is the receive-to-send time.
fn rx_thread(shared: &Shared) {
    let mut events: Vec<Event> = Vec::with_capacity(8);
    loop {
        // Checked in before the stall hook (the firmware stalls first, then checks
        // in): the stall then begins with a fresh check-in, so the supervisor's
        // verdict falls one liveness window after the hook's log line instead of
        // one window minus a tick, and two OS threads of equal priority cannot
        // race that verdict onto either side of the window.
        shared.check_in(Thread::Rx);
        shared.maybe_stall(Thread::Rx);
        // The read timeout keeps the liveness check-in flowing when the bus is silent.
        let frame = match shared.socket.read_frame() {
            Ok(frame) => frame,
            Err(e)
                if matches!(
                    e.kind(),
                    io::ErrorKind::WouldBlock
                        | io::ErrorKind::TimedOut
                        | io::ErrorKind::Interrupted
                ) =>
            {
                continue;
            }
            Err(e) => {
                log_err!(shared.clock, "hal_can", "receive failed: {e}");
                thread::sleep(Duration::from_millis(10));
                continue;
            }
        };
        let t_rx = Instant::now();
        let now = shared.clock.now_ms();
        // Standard data frames only: no extended ids, remote frames or CAN FD.
        let CanFrame::Data(data) = frame else {
            if !matches!(frame, CanFrame::Error(_)) {
                shared.lock().note_unknown_frame();
            }
            continue;
        };
        if data.is_extended() {
            shared.lock().note_unknown_frame();
            continue;
        }
        let id = data.raw_id() as u16;
        let action = {
            let mut ecu = shared.lock();
            ecu.on_frame(id, data.data(), now, |e| events.push(e))
        };
        for event in events.drain(..) {
            shared.emit(event);
        }
        if let RxAction::Echo { seq, payload } = action {
            let turnaround_us = t_rx.elapsed().as_micros().min(u128::from(u32::MAX)) as u32;
            let mut send = |f: &TxFrame| shared.send(f);
            shared.lock().send_echo(
                seq,
                payload,
                turnaround_us,
                shared.clock.now_ms(),
                &mut send,
            );
        }
    }
}

/// A periodic timer with the semantics of Zephyr's `k_timer_status_sync`: the
/// wait returns the number of expiries since the last wait, so a late thread
/// learns how many ticks it missed instead of catching up on all of them.
struct Ticker {
    period: Duration,
    next: Instant,
}

impl Ticker {
    fn new(period_ms: u32) -> Self {
        let period = Duration::from_millis(u64::from(period_ms));
        Self {
            period,
            next: Instant::now() + period,
        }
    }

    fn wait(&mut self) -> u32 {
        let now = Instant::now();
        if now < self.next {
            thread::sleep(self.next - now);
            self.next += self.period;
            return 1;
        }
        let late = now - self.next;
        let missed = (late.as_nanos() / self.period.as_nanos()) as u32;
        self.next += self.period * (missed + 1);
        missed + 1
    }
}

/// Transmit thread: the 10 ms base tick of the TX scheduler.
fn tx_thread(shared: &Shared) {
    shared.check_in(Thread::Tx); // before the first tick, so the supervisor never sees a never-started thread
    let mut ticker = Ticker::new(BASE_TICK_MS);
    let mut tick: u32 = 0;
    loop {
        let expiries = ticker.wait();
        if expiries > 1 {
            shared.lock().note_tx_tick_overruns(expiries - 1);
        }
        shared.check_in(Thread::Tx); // before the stall hook, as in the receive thread
        shared.maybe_stall(Thread::Tx);
        let now = shared.clock.now_ms();
        let mut send = |f: &TxFrame| shared.send(f);
        shared.lock().tx_tick(tick, now, &mut send);
        tick = tick.wrapping_add(1);
    }
}

/// Supervisor thread: the 5 ms tick that runs the CAN error handling, the
/// timeouts, the node state machine and the watchdog decision.
fn supervisor_thread(shared: &Shared) {
    let tick_ms = shared.lock().config().supervisor_tick_ms;
    {
        let now = shared.clock.now_ms();
        shared.lock().start(now);
    }
    shared.check_in(Thread::Supervisor);
    let mut ticker = Ticker::new(tick_ms);
    let mut events: Vec<Event> = Vec::with_capacity(16);
    loop {
        ticker.wait();
        shared.maybe_stall(Thread::Supervisor);
        let now = shared.clock.now_ms();
        let feed = shared.lock().supervisor_tick(now, |e| events.push(e));
        for event in events.drain(..) {
            shared.emit(event);
        }
        if feed {
            shared.feed_watchdog();
        }
        // Checked in after the work, so the next tick judges this one.
        shared.check_in(Thread::Supervisor);
    }
}

/// Watchdog thread: the hardware watchdog of the firmware, in software. When
/// the feed is older than the timeout the node reboots: the process replaces
/// itself with a fresh image (same pid, same console, same arguments), which is
/// what the simulator does on a watchdog reset.
fn watchdog_thread(shared: &Shared) {
    let timeout_us = i64::from(shared.watchdog_timeout_ms) * 1000;
    loop {
        thread::sleep(Duration::from_millis(1));
        let age = shared.clock.now_us() - shared.last_feed_us.load(Ordering::Relaxed);
        if age >= timeout_us {
            log_err!(
                shared.clock,
                "hal_wdt",
                "watchdog expired, {} ms without a feed: rebooting the node",
                shared.watchdog_timeout_ms
            );
            reboot();
        }
    }
}

fn reboot() -> ! {
    let mut args = std::env::args_os();
    let exe = args.next().unwrap_or_default();
    // Re-execute through the kernel's own link to the running image: the path
    // in argv[0] may be relative to a directory this process has since left.
    let err = Command::new("/proc/self/exe").arg0(exe).args(args).exec();
    eprintln!("fatal: reboot failed: {err}");
    std::process::exit(3)
}

// --- shell ---------------------------------------------------------------------------

fn print_status(out: &mut impl Write, st: &Status, tx: &TxStats) -> io::Result<()> {
    writeln!(out, "role={ROLE} test_hooks=1 uptime_ms={}", st.uptime_ms)?;
    writeln!(
        out,
        "node: state={} reason=\"{}\" since_ms={} safe_state_entries={} error_flags=0x{:02x}",
        st.node_state.as_str(),
        st.node_reason,
        st.node_state_age_ms,
        st.safe_state_entries,
        st.error_flags
    )?;
    writeln!(
        out,
        "can: phase={} state={} tec={} rec={} manual_recovery=0 simulated={}",
        st.can_phase.as_str(),
        st.can_state.as_str(),
        st.tec,
        st.rec,
        u8::from(st.simulation_active)
    )?;
    writeln!(
        out,
        "can: bus_off={} passive={} recovery_attempts={} recoveries_ok={} recovery_failures={} \
         last_recovery_ms={} backoff_ms={}",
        st.bus_off_events,
        st.passive_events,
        st.recovery_attempts,
        st.recoveries_ok,
        st.recovery_failures,
        st.last_recovery_duration_ms,
        st.current_backoff_ms
    )?;
    writeln!(
        out,
        "tx: sent={} ok={} err={} busy={} rejected={} blocked_bus_off={} tick_overruns={}",
        st.tx_sent,
        tx.ok.load(Ordering::Relaxed),
        tx.err.load(Ordering::Relaxed),
        tx.busy.load(Ordering::Relaxed),
        tx.rejected.load(Ordering::Relaxed),
        st.tx_blocked_bus_off,
        st.tx_tick_overruns
    )?;
    writeln!(
        out,
        "rx: frames={} unknown={} queue_hwm={}/{} dropped=0 events_dropped={} echo_responses={} test_commands={}",
        st.rx_frames,
        st.rx_unknown,
        st.rx_queue_hwm,
        st.rx_queue_depth,
        st.can_events_dropped,
        st.echo_responses,
        st.test_commands
    )?;
    for r in &st.rx {
        writeln!(
            out,
            "rx {}: alive={} age_ms={} ok={} crc={} counter={} dlc={} skipped={} lost={} timeouts={}",
            r.name,
            u8::from(r.alive),
            r.age_ms,
            r.accepted,
            r.crc_faults,
            r.counter_faults,
            r.dlc_faults,
            r.skipped,
            r.lost_frames,
            r.timeouts
        )?;
    }
    writeln!(
        out,
        "wdt: available=1 timeout_ms={} feeds={} skipped={} stale_threads=0x{:x}",
        st.wdt_timeout_ms, st.wdt_feeds, st.wdt_skipped, st.liveness_stale_mask
    )?;
    writeln!(
        out,
        "hooks: suppressed={} corrupted={} skipped={} frozen={}",
        st.hooks.suppressed, st.hooks.corrupted, st.hooks.skipped, st.hooks.frozen
    )?;
    let d = &st.detail;
    writeln!(
        out,
        "{ROLE}: tx data={} battery={} heartbeat={} speed_cm_s={} yaw_0.1deg_s={} soc_0.5pct={} \
         cmd: have={} mode={} torque_0.01Nm={} enabled={}",
        d.tx_data,
        d.tx_battery,
        d.tx_heartbeat,
        d.speed_cm_s,
        d.yaw_0_1deg_s,
        d.soc_0_5pct,
        u8::from(d.have_command),
        ControllerMode::name_of(d.command_mode),
        d.torque_0_01nm,
        d.actuators_enabled
    )
}

/// The shell on stdin: `ecu status` and `ecu reset`, the two commands the
/// tooling uses, with the firmware's output. Runs until stdin closes.
fn shell_thread(shared: &Shared) {
    let stdin = io::stdin();
    for line in stdin.lock().lines() {
        let Ok(line) = line else { break };
        let words: Vec<&str> = line.split_whitespace().collect();
        let stdout = io::stdout();
        let mut out = stdout.lock();
        let result = match words.as_slice() {
            [] => Ok(()),
            ["ecu", "status"] => {
                let now = shared.clock.now_ms();
                let st = shared.lock().status(now);
                print_status(&mut out, &st, &shared.tx)
            }
            ["ecu", "reset"] => {
                let now = shared.clock.now_ms();
                shared.lock().reset_faults(now);
                writeln!(
                    out,
                    "faults cleared: FAULT state, CAN recovery backoff, error_flags, watchdog skip count, \
                     per-message rx fault counters, pending test faults and hook counters \
                     (cumulative tx/rx/bus-off counters kept)"
                )
            }
            ["ecu", ..] | ["help"] => writeln!(out, "ecu - ECU diagnostics\n  status  Node, CAN, traffic and watchdog status with error counters\n  reset   Clear latched faults, error_flags, per-message RX fault counters, pending test faults and hook counters"),
            [cmd, ..] => writeln!(out, "{cmd}: command not found"),
        };
        if result.and_then(|()| out.flush()).is_err() {
            break; // the console is gone; the ECU runs on without a shell
        }
    }
}

// --- entry point ---------------------------------------------------------------------

fn usage() -> ! {
    eprintln!("usage: sensor-vcan --can-if=<interface>   (e.g. --can-if=vcan0)");
    std::process::exit(64)
}

fn main() {
    let clock = Clock(Instant::now());
    let mut iface: Option<String> = None;
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        if let Some(value) = arg.strip_prefix("--can-if=") {
            iface = Some(value.to_string());
        } else if arg == "--can-if" {
            iface = args.next();
        } else {
            usage();
        }
    }
    let Some(iface) = iface else { usage() };

    println!("{BANNER}");
    let cfg = Config::default();
    let socket = match CanSocket::open(&iface) {
        Ok(socket) => socket,
        Err(e) => {
            log_err!(
                clock,
                "hal_can",
                "CAN init failed: cannot open {iface}: {e}"
            );
            std::process::exit(1);
        }
    };
    if let Err(e) =
        socket.set_read_timeout(Duration::from_millis(u64::from(cfg.liveness_window_ms / 4)))
    {
        log_err!(clock, "hal_can", "CAN init failed: read timeout: {e}");
        std::process::exit(1);
    }
    log_inf!(
        clock,
        "hal_can",
        "CAN {iface} ready (manual recovery off, test hooks on)"
    );
    log_inf!(
        clock,
        "hal_wdt",
        "watchdog software armed, {} ms",
        cfg.watchdog_timeout_ms
    );

    let shared = Arc::new(Shared {
        clock,
        ecu: Mutex::new(SensorEcu::new(cfg)),
        socket,
        tx: TxStats::default(),
        last_feed_us: AtomicI64::new(clock.now_us()),
        watchdog_timeout_ms: cfg.watchdog_timeout_ms,
    });

    let spawn = |name: &str, f: fn(&Shared)| {
        let shared = Arc::clone(&shared);
        thread::Builder::new()
            .name(name.to_string())
            .spawn(move || f(&shared))
            .unwrap_or_else(|e| {
                eprintln!("fatal: cannot start thread {name}: {e}");
                std::process::exit(1)
            })
    };
    spawn("can_rx", rx_thread);
    spawn("can_tx", tx_thread);
    spawn("supervisor", supervisor_thread);
    spawn("watchdog", watchdog_thread);
    let shell = spawn("shell", shell_thread);
    log_inf!(
        clock,
        "ecu",
        "ecu role={ROLE} node={NODE} threads=rx,tx,supervisor,watchdog,shell rx_queue={} sup_tick={} ms wdt={} ms",
        cfg.rx_queue_depth,
        cfg.supervisor_tick_ms,
        cfg.watchdog_timeout_ms
    );
    log_inf!(clock, "main", "{ROLE} ECU running");

    // The ECU runs until it is terminated; the shell thread ends when stdin
    // closes, the others never do. Parking main keeps the process alive without
    // spinning (a joined shell thread would end the process on a closed console).
    let _ = shell.join();
    loop {
        thread::park();
    }
}
