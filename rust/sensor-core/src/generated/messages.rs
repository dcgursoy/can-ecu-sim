// GENERATED FILE, do not edit.
// Source: can/network.dbc  (sha256 c239c44a1c23952d)
// Generator: can/gen_rust.py  (regenerate with `python3 can/gen_rust.py`)
// Network version: 'can-ecu-sim network 1.0'

use crate::codec;

/// Value table `ControllerMode` (4-bit signal). The struct fields carry the raw
/// value; `from_raw` returns `None` for a value outside the table.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum ControllerMode {
    Init = 0,
    Normal = 1,
    Degraded = 2,
    SafeState = 3,
}

impl ControllerMode {
    pub const fn from_raw(raw: u8) -> Option<Self> {
        match raw {
            0 => Some(Self::Init),
            1 => Some(Self::Normal),
            2 => Some(Self::Degraded),
            3 => Some(Self::SafeState),
            _ => None,
        }
    }

    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Init => "INIT",
            Self::Normal => "NORMAL",
            Self::Degraded => "DEGRADED",
            Self::SafeState => "SAFE_STATE",
        }
    }

    /// The DBC label of a raw value, `"UNKNOWN"` outside the table (the C++ `to_string`).
    pub const fn name_of(raw: u8) -> &'static str {
        match Self::from_raw(raw) {
            Some(v) => v.as_str(),
            None => "UNKNOWN",
        }
    }
}

/// Value table `SensorStatus` (4-bit signal). The struct fields carry the raw
/// value; `from_raw` returns `None` for a value outside the table.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum SensorStatus {
    Ok = 0,
    Degraded = 1,
    Fault = 2,
}

impl SensorStatus {
    pub const fn from_raw(raw: u8) -> Option<Self> {
        match raw {
            0 => Some(Self::Ok),
            1 => Some(Self::Degraded),
            2 => Some(Self::Fault),
            _ => None,
        }
    }

    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Ok => "OK",
            Self::Degraded => "DEGRADED",
            Self::Fault => "FAULT",
        }
    }

    /// The DBC label of a raw value, `"UNKNOWN"` outside the table (the C++ `to_string`).
    pub const fn name_of(raw: u8) -> &'static str {
        match Self::from_raw(raw) {
            Some(v) => v.as_str(),
            None => "UNKNOWN",
        }
    }
}

/// Value table `NodeState` (4-bit signal). The struct fields carry the raw
/// value; `from_raw` returns `None` for a value outside the table.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum NodeState {
    Boot = 0,
    Running = 1,
    Degraded = 2,
    SafeState = 3,
    Fault = 4,
}

impl NodeState {
    pub const fn from_raw(raw: u8) -> Option<Self> {
        match raw {
            0 => Some(Self::Boot),
            1 => Some(Self::Running),
            2 => Some(Self::Degraded),
            3 => Some(Self::SafeState),
            4 => Some(Self::Fault),
            _ => None,
        }
    }

    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Boot => "BOOT",
            Self::Running => "RUNNING",
            Self::Degraded => "DEGRADED",
            Self::SafeState => "SAFE_STATE",
            Self::Fault => "FAULT",
        }
    }

    /// The DBC label of a raw value, `"UNKNOWN"` outside the table (the C++ `to_string`).
    pub const fn name_of(raw: u8) -> &'static str {
        match Self::from_raw(raw) {
            Some(v) => v.as_str(),
            None => "UNKNOWN",
        }
    }
}

/// Value table `CanState` (2-bit signal). The struct fields carry the raw
/// value; `from_raw` returns `None` for a value outside the table.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum CanState {
    ErrorActive = 0,
    ErrorPassive = 1,
    BusOff = 2,
    Stopped = 3,
}

impl CanState {
    pub const fn from_raw(raw: u8) -> Option<Self> {
        match raw {
            0 => Some(Self::ErrorActive),
            1 => Some(Self::ErrorPassive),
            2 => Some(Self::BusOff),
            3 => Some(Self::Stopped),
            _ => None,
        }
    }

    pub const fn as_str(self) -> &'static str {
        match self {
            Self::ErrorActive => "ERROR_ACTIVE",
            Self::ErrorPassive => "ERROR_PASSIVE",
            Self::BusOff => "BUS_OFF",
            Self::Stopped => "STOPPED",
        }
    }

    /// The DBC label of a raw value, `"UNKNOWN"` outside the table (the C++ `to_string`).
    pub const fn name_of(raw: u8) -> &'static str {
        match Self::from_raw(raw) {
            Some(v) => v.as_str(),
            None => "UNKNOWN",
        }
    }
}

/// Value table `Node` (8-bit signal). The struct fields carry the raw
/// value; `from_raw` returns `None` for a value outside the table.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum Node {
    All = 0,
    SensorEcu = 1,
    ControllerEcu = 2,
}

impl Node {
    pub const fn from_raw(raw: u8) -> Option<Self> {
        match raw {
            0 => Some(Self::All),
            1 => Some(Self::SensorEcu),
            2 => Some(Self::ControllerEcu),
            _ => None,
        }
    }

    pub const fn as_str(self) -> &'static str {
        match self {
            Self::All => "ALL",
            Self::SensorEcu => "SENSOR_ECU",
            Self::ControllerEcu => "CONTROLLER_ECU",
        }
    }

    /// The DBC label of a raw value, `"UNKNOWN"` outside the table (the C++ `to_string`).
    pub const fn name_of(raw: u8) -> &'static str {
        match Self::from_raw(raw) {
            Some(v) => v.as_str(),
            None => "UNKNOWN",
        }
    }
}

/// Value table `TestCommand` (8-bit signal). The struct fields carry the raw
/// value; `from_raw` returns `None` for a value outside the table.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum TestCommand {
    Nop = 0,
    ResetFaults = 1,
    InjectCanError = 2,
    ForceBusOff = 3,
    SuppressHeartbeat = 4,
    CorruptCrc = 5,
    SkipCounter = 6,
    FreezeCounter = 7,
    SuppressMessage = 8,
    StallThread = 9,
}

impl TestCommand {
    pub const fn from_raw(raw: u8) -> Option<Self> {
        match raw {
            0 => Some(Self::Nop),
            1 => Some(Self::ResetFaults),
            2 => Some(Self::InjectCanError),
            3 => Some(Self::ForceBusOff),
            4 => Some(Self::SuppressHeartbeat),
            5 => Some(Self::CorruptCrc),
            6 => Some(Self::SkipCounter),
            7 => Some(Self::FreezeCounter),
            8 => Some(Self::SuppressMessage),
            9 => Some(Self::StallThread),
            _ => None,
        }
    }

    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Nop => "NOP",
            Self::ResetFaults => "RESET_FAULTS",
            Self::InjectCanError => "INJECT_CAN_ERROR",
            Self::ForceBusOff => "FORCE_BUS_OFF",
            Self::SuppressHeartbeat => "SUPPRESS_HEARTBEAT",
            Self::CorruptCrc => "CORRUPT_CRC",
            Self::SkipCounter => "SKIP_COUNTER",
            Self::FreezeCounter => "FREEZE_COUNTER",
            Self::SuppressMessage => "SUPPRESS_MESSAGE",
            Self::StallThread => "STALL_THREAD",
        }
    }

    /// The DBC label of a raw value, `"UNKNOWN"` outside the table (the C++ `to_string`).
    pub const fn name_of(raw: u8) -> &'static str {
        match Self::from_raw(raw) {
            Some(v) => v.as_str(),
            None => "UNKNOWN",
        }
    }
}

/// What every generated message provides; the E2E and receiver code is
/// generic over it.
pub trait Message: Copy + Default + PartialEq + core::fmt::Debug {
    const ID: u16;
    const DLC: usize;
    const CYCLE_MS: u16;
    const SAFETY_RELEVANT: bool;
    const TEST_ONLY: bool;
    const NAME: &'static str;
    /// Encode the raw field values into a frame of DLC bytes.
    fn to_frame(&self) -> [u8; 8];
    /// Decode a frame; the slice must hold at least DLC bytes.
    fn from_buffer(data: &[u8]) -> Option<Self>;
}

/// Static description of one message (the C++ `MessageInfo`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct MessageInfo {
    pub id: u16,
    pub dlc: u8,
    /// 0 for event-driven messages.
    pub cycle_ms: u16,
    pub safety_relevant: bool,
    pub test_only: bool,
    pub name: &'static str,
}

/// Actuator command from the controller, 20 ms. Highest priority on the bus. mode mirrors the controller state machine so tests can observe safe-state entry on the bus.
/// Senders: CONTROLLER_ECU
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ControllerCommand {
    /// bits 0..7, raw 0..255
    pub crc: u8,
    /// bits 8..11, raw 0..15
    pub counter: u8,
    /// bits 12..15, raw 0..15, value table `ControllerMode`. Controller mode, see value table.
    pub mode: u8,
    /// bits 16..31, phys = raw * 0.01 + 0 [Nm], raw -32768..32767
    pub torque_cmd: i16,
    /// bits 32..47, phys = raw * 0.01 + 0 [deg], raw -32768..32767
    pub steering_cmd: i16,
    /// bits 48..48, raw 0..1
    pub actuators_enabled: u8,
    /// bits 49..49, raw 0..1
    pub brake_hold: u8,
    /// bits 50..50, raw 0..1
    pub fault_latched: u8,
    /// bits 56..63, phys = raw * 0.1 + 0 [m/s], raw 0..255. Speed ceiling the controller currently enforces (derated on low battery).
    pub speed_limit: u8,
}

impl ControllerCommand {
    pub const ID: u16 = 0x080;
    pub const DLC: usize = 8;
    pub const CYCLE_MS: u16 = 20;
    pub const SAFETY_RELEVANT: bool = true;
    pub const TEST_ONLY: bool = false;
    pub const NAME: &'static str = "CONTROLLER_COMMAND";

    /// `mode` as its value table, `None` for a raw value outside it.
    pub const fn mode_enum(&self) -> Option<ControllerMode> {
        ControllerMode::from_raw(self.mode)
    }
    pub fn set_mode(&mut self, value: ControllerMode) {
        self.mode = value as u8;
    }

    // Physical-unit accessors. Raw values stay the truth on the wire; the
    // setters round to the nearest raw step and saturate at the raw range.
    pub fn torque_cmd_phys(&self) -> f32 {
        codec::raw_to_phys(i64::from(self.torque_cmd), 0.01_f32, 0.0_f32)
    }
    pub fn set_torque_cmd_phys(&mut self, value: f32) {
        self.torque_cmd = codec::phys_to_raw(value, 0.01_f32, 0.0_f32, -32768, 32767) as i16;
    }
    pub fn steering_cmd_phys(&self) -> f32 {
        codec::raw_to_phys(i64::from(self.steering_cmd), 0.01_f32, 0.0_f32)
    }
    pub fn set_steering_cmd_phys(&mut self, value: f32) {
        self.steering_cmd = codec::phys_to_raw(value, 0.01_f32, 0.0_f32, -32768, 32767) as i16;
    }
    pub fn speed_limit_phys(&self) -> f32 {
        codec::raw_to_phys(i64::from(self.speed_limit), 0.1_f32, 0.0_f32)
    }
    pub fn set_speed_limit_phys(&mut self, value: f32) {
        self.speed_limit = codec::phys_to_raw(value, 0.1_f32, 0.0_f32, 0, 255) as u8;
    }

    /// Encode the raw field values into a frame of DLC bytes (the rest zero).
    pub const fn to_frame(&self) -> [u8; 8] {
        let mut d = [0u8; 8];
        codec::put(&mut d, 0, 8, self.crc as u32);
        codec::put(&mut d, 8, 4, self.counter as u32);
        codec::put(&mut d, 12, 4, self.mode as u32);
        codec::put(&mut d, 16, 16, self.torque_cmd as u32);
        codec::put(&mut d, 32, 16, self.steering_cmd as u32);
        codec::put(&mut d, 48, 1, self.actuators_enabled as u32);
        codec::put(&mut d, 49, 1, self.brake_hold as u32);
        codec::put(&mut d, 50, 1, self.fault_latched as u32);
        codec::put(&mut d, 56, 8, self.speed_limit as u32);
        d
    }

    /// Decode a frame of DLC bytes.
    pub const fn from_frame(d: &[u8; 8]) -> Self {
        Self {
            crc: codec::get_unsigned(d, 0, 8) as u8,
            counter: codec::get_unsigned(d, 8, 4) as u8,
            mode: codec::get_unsigned(d, 12, 4) as u8,
            torque_cmd: codec::get_signed(d, 16, 16) as i16,
            steering_cmd: codec::get_signed(d, 32, 16) as i16,
            actuators_enabled: codec::get_unsigned(d, 48, 1) as u8,
            brake_hold: codec::get_unsigned(d, 49, 1) as u8,
            fault_latched: codec::get_unsigned(d, 50, 1) as u8,
            speed_limit: codec::get_unsigned(d, 56, 8) as u8,
        }
    }

    /// Decode from a received slice; `None` if it is shorter than DLC.
    pub fn from_buffer(data: &[u8]) -> Option<Self> {
        if data.len() < Self::DLC {
            return None;
        }
        let mut d = [0u8; 8];
        d[..Self::DLC].copy_from_slice(&data[..Self::DLC]);
        Some(Self::from_frame(&d))
    }
}

impl Message for ControllerCommand {
    const ID: u16 = Self::ID;
    const DLC: usize = Self::DLC;
    const CYCLE_MS: u16 = Self::CYCLE_MS;
    const SAFETY_RELEVANT: bool = Self::SAFETY_RELEVANT;
    const TEST_ONLY: bool = Self::TEST_ONLY;
    const NAME: &'static str = Self::NAME;
    fn to_frame(&self) -> [u8; 8] {
        Self::to_frame(self)
    }
    fn from_buffer(data: &[u8]) -> Option<Self> {
        Self::from_buffer(data)
    }
}

/// Simulated vehicle sensor sample, 10 ms.
/// Senders: SENSOR_ECU
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SensorData {
    /// bits 0..7, raw 0..255
    pub crc: u8,
    /// bits 8..11, raw 0..15
    pub counter: u8,
    /// bits 12..15, raw 0..15, value table `SensorStatus`. Sensor health, see value table.
    pub sensor_status: u8,
    /// bits 16..31, phys = raw * 0.01 + 0 [m/s], raw 0..65535
    pub speed: u16,
    /// bits 32..47, phys = raw * 0.1 + 0 [deg/s], raw -32768..32767
    pub yaw_rate: i16,
    /// bits 48..63, phys = raw * 0.01 + 0 [m/s^2], raw -32768..32767
    pub accel_x: i16,
}

impl SensorData {
    pub const ID: u16 = 0x100;
    pub const DLC: usize = 8;
    pub const CYCLE_MS: u16 = 10;
    pub const SAFETY_RELEVANT: bool = true;
    pub const TEST_ONLY: bool = false;
    pub const NAME: &'static str = "SENSOR_DATA";

    /// `sensor_status` as its value table, `None` for a raw value outside it.
    pub const fn sensor_status_enum(&self) -> Option<SensorStatus> {
        SensorStatus::from_raw(self.sensor_status)
    }
    pub fn set_sensor_status(&mut self, value: SensorStatus) {
        self.sensor_status = value as u8;
    }

    // Physical-unit accessors. Raw values stay the truth on the wire; the
    // setters round to the nearest raw step and saturate at the raw range.
    pub fn speed_phys(&self) -> f32 {
        codec::raw_to_phys(i64::from(self.speed), 0.01_f32, 0.0_f32)
    }
    pub fn set_speed_phys(&mut self, value: f32) {
        self.speed = codec::phys_to_raw(value, 0.01_f32, 0.0_f32, 0, 65535) as u16;
    }
    pub fn yaw_rate_phys(&self) -> f32 {
        codec::raw_to_phys(i64::from(self.yaw_rate), 0.1_f32, 0.0_f32)
    }
    pub fn set_yaw_rate_phys(&mut self, value: f32) {
        self.yaw_rate = codec::phys_to_raw(value, 0.1_f32, 0.0_f32, -32768, 32767) as i16;
    }
    pub fn accel_x_phys(&self) -> f32 {
        codec::raw_to_phys(i64::from(self.accel_x), 0.01_f32, 0.0_f32)
    }
    pub fn set_accel_x_phys(&mut self, value: f32) {
        self.accel_x = codec::phys_to_raw(value, 0.01_f32, 0.0_f32, -32768, 32767) as i16;
    }

    /// Encode the raw field values into a frame of DLC bytes (the rest zero).
    pub const fn to_frame(&self) -> [u8; 8] {
        let mut d = [0u8; 8];
        codec::put(&mut d, 0, 8, self.crc as u32);
        codec::put(&mut d, 8, 4, self.counter as u32);
        codec::put(&mut d, 12, 4, self.sensor_status as u32);
        codec::put(&mut d, 16, 16, self.speed as u32);
        codec::put(&mut d, 32, 16, self.yaw_rate as u32);
        codec::put(&mut d, 48, 16, self.accel_x as u32);
        d
    }

    /// Decode a frame of DLC bytes.
    pub const fn from_frame(d: &[u8; 8]) -> Self {
        Self {
            crc: codec::get_unsigned(d, 0, 8) as u8,
            counter: codec::get_unsigned(d, 8, 4) as u8,
            sensor_status: codec::get_unsigned(d, 12, 4) as u8,
            speed: codec::get_unsigned(d, 16, 16) as u16,
            yaw_rate: codec::get_signed(d, 32, 16) as i16,
            accel_x: codec::get_signed(d, 48, 16) as i16,
        }
    }

    /// Decode from a received slice; `None` if it is shorter than DLC.
    pub fn from_buffer(data: &[u8]) -> Option<Self> {
        if data.len() < Self::DLC {
            return None;
        }
        let mut d = [0u8; 8];
        d[..Self::DLC].copy_from_slice(&data[..Self::DLC]);
        Some(Self::from_frame(&d))
    }
}

impl Message for SensorData {
    const ID: u16 = Self::ID;
    const DLC: usize = Self::DLC;
    const CYCLE_MS: u16 = Self::CYCLE_MS;
    const SAFETY_RELEVANT: bool = Self::SAFETY_RELEVANT;
    const TEST_ONLY: bool = Self::TEST_ONLY;
    const NAME: &'static str = Self::NAME;
    fn to_frame(&self) -> [u8; 8] {
        Self::to_frame(self)
    }
    fn from_buffer(data: &[u8]) -> Option<Self> {
        Self::from_buffer(data)
    }
}

/// Battery pack status, 100 ms.
/// Senders: SENSOR_ECU
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct BatteryStatus {
    /// bits 0..7, raw 0..255
    pub crc: u8,
    /// bits 8..11, raw 0..15
    pub counter: u8,
    /// bits 12..12, raw 0..1
    pub charging: u8,
    /// bits 13..13, raw 0..1
    pub low_voltage: u8,
    /// bits 14..14, raw 0..1
    pub over_temperature: u8,
    /// bits 16..31, phys = raw * 0.01 + 0 [V], raw 0..65535
    pub pack_voltage: u16,
    /// bits 32..47, phys = raw * 0.1 + 0 [A], raw -32768..32767
    pub pack_current: i16,
    /// bits 48..55, phys = raw * 0.5 + 0 [%], raw 0..200
    pub state_of_charge: u8,
    /// bits 56..63, phys = raw * 1 + -40 [degC], raw 0..255
    pub temperature: u8,
}

impl BatteryStatus {
    pub const ID: u16 = 0x110;
    pub const DLC: usize = 8;
    pub const CYCLE_MS: u16 = 100;
    pub const SAFETY_RELEVANT: bool = true;
    pub const TEST_ONLY: bool = false;
    pub const NAME: &'static str = "BATTERY_STATUS";

    // Physical-unit accessors. Raw values stay the truth on the wire; the
    // setters round to the nearest raw step and saturate at the raw range.
    pub fn pack_voltage_phys(&self) -> f32 {
        codec::raw_to_phys(i64::from(self.pack_voltage), 0.01_f32, 0.0_f32)
    }
    pub fn set_pack_voltage_phys(&mut self, value: f32) {
        self.pack_voltage = codec::phys_to_raw(value, 0.01_f32, 0.0_f32, 0, 65535) as u16;
    }
    pub fn pack_current_phys(&self) -> f32 {
        codec::raw_to_phys(i64::from(self.pack_current), 0.1_f32, 0.0_f32)
    }
    pub fn set_pack_current_phys(&mut self, value: f32) {
        self.pack_current = codec::phys_to_raw(value, 0.1_f32, 0.0_f32, -32768, 32767) as i16;
    }
    pub fn state_of_charge_phys(&self) -> f32 {
        codec::raw_to_phys(i64::from(self.state_of_charge), 0.5_f32, 0.0_f32)
    }
    pub fn set_state_of_charge_phys(&mut self, value: f32) {
        self.state_of_charge = codec::phys_to_raw(value, 0.5_f32, 0.0_f32, 0, 200) as u8;
    }
    pub fn temperature_phys(&self) -> f32 {
        codec::raw_to_phys(i64::from(self.temperature), 1.0_f32, -40.0_f32)
    }
    pub fn set_temperature_phys(&mut self, value: f32) {
        self.temperature = codec::phys_to_raw(value, 1.0_f32, -40.0_f32, 0, 255) as u8;
    }

    /// Encode the raw field values into a frame of DLC bytes (the rest zero).
    pub const fn to_frame(&self) -> [u8; 8] {
        let mut d = [0u8; 8];
        codec::put(&mut d, 0, 8, self.crc as u32);
        codec::put(&mut d, 8, 4, self.counter as u32);
        codec::put(&mut d, 12, 1, self.charging as u32);
        codec::put(&mut d, 13, 1, self.low_voltage as u32);
        codec::put(&mut d, 14, 1, self.over_temperature as u32);
        codec::put(&mut d, 16, 16, self.pack_voltage as u32);
        codec::put(&mut d, 32, 16, self.pack_current as u32);
        codec::put(&mut d, 48, 8, self.state_of_charge as u32);
        codec::put(&mut d, 56, 8, self.temperature as u32);
        d
    }

    /// Decode a frame of DLC bytes.
    pub const fn from_frame(d: &[u8; 8]) -> Self {
        Self {
            crc: codec::get_unsigned(d, 0, 8) as u8,
            counter: codec::get_unsigned(d, 8, 4) as u8,
            charging: codec::get_unsigned(d, 12, 1) as u8,
            low_voltage: codec::get_unsigned(d, 13, 1) as u8,
            over_temperature: codec::get_unsigned(d, 14, 1) as u8,
            pack_voltage: codec::get_unsigned(d, 16, 16) as u16,
            pack_current: codec::get_signed(d, 32, 16) as i16,
            state_of_charge: codec::get_unsigned(d, 48, 8) as u8,
            temperature: codec::get_unsigned(d, 56, 8) as u8,
        }
    }

    /// Decode from a received slice; `None` if it is shorter than DLC.
    pub fn from_buffer(data: &[u8]) -> Option<Self> {
        if data.len() < Self::DLC {
            return None;
        }
        let mut d = [0u8; 8];
        d[..Self::DLC].copy_from_slice(&data[..Self::DLC]);
        Some(Self::from_frame(&d))
    }
}

impl Message for BatteryStatus {
    const ID: u16 = Self::ID;
    const DLC: usize = Self::DLC;
    const CYCLE_MS: u16 = Self::CYCLE_MS;
    const SAFETY_RELEVANT: bool = Self::SAFETY_RELEVANT;
    const TEST_ONLY: bool = Self::TEST_ONLY;
    const NAME: &'static str = Self::NAME;
    fn to_frame(&self) -> [u8; 8] {
        Self::to_frame(self)
    }
    fn from_buffer(data: &[u8]) -> Option<Self> {
        Self::from_buffer(data)
    }
}

/// Sensor ECU alive message, 100 ms. Loss drives the controller to its safe state.
/// Senders: SENSOR_ECU
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SensorHeartbeat {
    /// bits 0..7, raw 0..255
    pub crc: u8,
    /// bits 8..11, raw 0..15
    pub counter: u8,
    /// bits 12..15, raw 0..15, value table `NodeState`
    pub node_state: u8,
    /// bits 16..47 [ms], raw 0..4294967295
    pub uptime: u32,
    /// bits 48..49, raw 0..3, value table `CanState`. Local CAN controller state as reported by the driver, see value table. The driver's error-warning level (an error counter at or above 96) is still error-active per ISO 11898-1 and is reported as ERROR_ACTIVE; the counters, not the state, show the warning level.
    pub can_state: u8,
    /// bits 50..55, raw 0..63
    pub bus_off_count: u8,
    /// bits 56..63, raw 0..255. Latched diagnostic bits: bit 0 crc_fault_seen, bit 1 counter_fault_seen, bit 2 rx_timeout_seen, bit 3 bus_off_seen, bit 4 watchdog_warning, bit 5 test_hook_active, bits 6-7 reserved.
    pub error_flags: u8,
}

impl SensorHeartbeat {
    pub const ID: u16 = 0x200;
    pub const DLC: usize = 8;
    pub const CYCLE_MS: u16 = 100;
    pub const SAFETY_RELEVANT: bool = true;
    pub const TEST_ONLY: bool = false;
    pub const NAME: &'static str = "SENSOR_HEARTBEAT";

    /// `node_state` as its value table, `None` for a raw value outside it.
    pub const fn node_state_enum(&self) -> Option<NodeState> {
        NodeState::from_raw(self.node_state)
    }
    pub fn set_node_state(&mut self, value: NodeState) {
        self.node_state = value as u8;
    }

    /// `can_state` as its value table, `None` for a raw value outside it.
    pub const fn can_state_enum(&self) -> Option<CanState> {
        CanState::from_raw(self.can_state)
    }
    pub fn set_can_state(&mut self, value: CanState) {
        self.can_state = value as u8;
    }

    /// Encode the raw field values into a frame of DLC bytes (the rest zero).
    pub const fn to_frame(&self) -> [u8; 8] {
        let mut d = [0u8; 8];
        codec::put(&mut d, 0, 8, self.crc as u32);
        codec::put(&mut d, 8, 4, self.counter as u32);
        codec::put(&mut d, 12, 4, self.node_state as u32);
        codec::put(&mut d, 16, 32, self.uptime);
        codec::put(&mut d, 48, 2, self.can_state as u32);
        codec::put(&mut d, 50, 6, self.bus_off_count as u32);
        codec::put(&mut d, 56, 8, self.error_flags as u32);
        d
    }

    /// Decode a frame of DLC bytes.
    pub const fn from_frame(d: &[u8; 8]) -> Self {
        Self {
            crc: codec::get_unsigned(d, 0, 8) as u8,
            counter: codec::get_unsigned(d, 8, 4) as u8,
            node_state: codec::get_unsigned(d, 12, 4) as u8,
            uptime: codec::get_unsigned(d, 16, 32),
            can_state: codec::get_unsigned(d, 48, 2) as u8,
            bus_off_count: codec::get_unsigned(d, 50, 6) as u8,
            error_flags: codec::get_unsigned(d, 56, 8) as u8,
        }
    }

    /// Decode from a received slice; `None` if it is shorter than DLC.
    pub fn from_buffer(data: &[u8]) -> Option<Self> {
        if data.len() < Self::DLC {
            return None;
        }
        let mut d = [0u8; 8];
        d[..Self::DLC].copy_from_slice(&data[..Self::DLC]);
        Some(Self::from_frame(&d))
    }
}

impl Message for SensorHeartbeat {
    const ID: u16 = Self::ID;
    const DLC: usize = Self::DLC;
    const CYCLE_MS: u16 = Self::CYCLE_MS;
    const SAFETY_RELEVANT: bool = Self::SAFETY_RELEVANT;
    const TEST_ONLY: bool = Self::TEST_ONLY;
    const NAME: &'static str = Self::NAME;
    fn to_frame(&self) -> [u8; 8] {
        Self::to_frame(self)
    }
    fn from_buffer(data: &[u8]) -> Option<Self> {
        Self::from_buffer(data)
    }
}

/// Controller ECU alive message, 100 ms.
/// Senders: CONTROLLER_ECU
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ControllerHeartbeat {
    /// bits 0..7, raw 0..255
    pub crc: u8,
    /// bits 8..11, raw 0..15
    pub counter: u8,
    /// bits 12..15, raw 0..15, value table `NodeState`
    pub node_state: u8,
    /// bits 16..47 [ms], raw 0..4294967295
    pub uptime: u32,
    /// bits 48..49, raw 0..3, value table `CanState`. Local CAN controller state as reported by the driver, see value table. The driver's error-warning level is reported as ERROR_ACTIVE, see SENSOR_HEARTBEAT.can_state.
    pub can_state: u8,
    /// bits 50..55, raw 0..63
    pub bus_off_count: u8,
    /// bits 56..63, raw 0..255. Latched diagnostic bits: bit 0 crc_fault_seen, bit 1 counter_fault_seen, bit 2 rx_timeout_seen, bit 3 bus_off_seen, bit 4 watchdog_warning, bit 5 test_hook_active, bits 6-7 reserved.
    pub error_flags: u8,
}

impl ControllerHeartbeat {
    pub const ID: u16 = 0x201;
    pub const DLC: usize = 8;
    pub const CYCLE_MS: u16 = 100;
    pub const SAFETY_RELEVANT: bool = true;
    pub const TEST_ONLY: bool = false;
    pub const NAME: &'static str = "CONTROLLER_HEARTBEAT";

    /// `node_state` as its value table, `None` for a raw value outside it.
    pub const fn node_state_enum(&self) -> Option<NodeState> {
        NodeState::from_raw(self.node_state)
    }
    pub fn set_node_state(&mut self, value: NodeState) {
        self.node_state = value as u8;
    }

    /// `can_state` as its value table, `None` for a raw value outside it.
    pub const fn can_state_enum(&self) -> Option<CanState> {
        CanState::from_raw(self.can_state)
    }
    pub fn set_can_state(&mut self, value: CanState) {
        self.can_state = value as u8;
    }

    /// Encode the raw field values into a frame of DLC bytes (the rest zero).
    pub const fn to_frame(&self) -> [u8; 8] {
        let mut d = [0u8; 8];
        codec::put(&mut d, 0, 8, self.crc as u32);
        codec::put(&mut d, 8, 4, self.counter as u32);
        codec::put(&mut d, 12, 4, self.node_state as u32);
        codec::put(&mut d, 16, 32, self.uptime);
        codec::put(&mut d, 48, 2, self.can_state as u32);
        codec::put(&mut d, 50, 6, self.bus_off_count as u32);
        codec::put(&mut d, 56, 8, self.error_flags as u32);
        d
    }

    /// Decode a frame of DLC bytes.
    pub const fn from_frame(d: &[u8; 8]) -> Self {
        Self {
            crc: codec::get_unsigned(d, 0, 8) as u8,
            counter: codec::get_unsigned(d, 8, 4) as u8,
            node_state: codec::get_unsigned(d, 12, 4) as u8,
            uptime: codec::get_unsigned(d, 16, 32),
            can_state: codec::get_unsigned(d, 48, 2) as u8,
            bus_off_count: codec::get_unsigned(d, 50, 6) as u8,
            error_flags: codec::get_unsigned(d, 56, 8) as u8,
        }
    }

    /// Decode from a received slice; `None` if it is shorter than DLC.
    pub fn from_buffer(data: &[u8]) -> Option<Self> {
        if data.len() < Self::DLC {
            return None;
        }
        let mut d = [0u8; 8];
        d[..Self::DLC].copy_from_slice(&data[..Self::DLC]);
        Some(Self::from_frame(&d))
    }
}

impl Message for ControllerHeartbeat {
    const ID: u16 = Self::ID;
    const DLC: usize = Self::DLC;
    const CYCLE_MS: u16 = Self::CYCLE_MS;
    const SAFETY_RELEVANT: bool = Self::SAFETY_RELEVANT;
    const TEST_ONLY: bool = Self::TEST_ONLY;
    const NAME: &'static str = Self::NAME;
    fn to_frame(&self) -> [u8; 8] {
        Self::to_frame(self)
    }
    fn from_buffer(data: &[u8]) -> Option<Self> {
        Self::from_buffer(data)
    }
}

/// Round-trip latency probe from the host. The addressed node (or every node for target_node 0) answers with its own echo response (SENSOR_ECHO_RESPONSE or CONTROLLER_ECHO_RESPONSE) carrying the same seq and payload. Each responder has its own identifier because two nodes transmitting one identifier would collide in the data field on a real bus. Not safety relevant, no CRC.
/// Senders: TEST_HOST
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EchoRequest {
    /// bits 0..7, raw 0..255, value table `Node`
    pub target_node: u8,
    /// bits 8..15, raw 0..255
    pub seq: u8,
    /// bits 16..47, raw 0..4294967295
    pub payload: u32,
}

impl EchoRequest {
    pub const ID: u16 = 0x300;
    pub const DLC: usize = 6;
    pub const CYCLE_MS: u16 = 0;
    pub const SAFETY_RELEVANT: bool = false;
    pub const TEST_ONLY: bool = false;
    pub const NAME: &'static str = "ECHO_REQUEST";

    /// `target_node` as its value table, `None` for a raw value outside it.
    pub const fn target_node_enum(&self) -> Option<Node> {
        Node::from_raw(self.target_node)
    }
    pub fn set_target_node(&mut self, value: Node) {
        self.target_node = value as u8;
    }

    /// Encode the raw field values into a frame of DLC bytes (the rest zero).
    pub const fn to_frame(&self) -> [u8; 8] {
        let mut d = [0u8; 8];
        codec::put(&mut d, 0, 8, self.target_node as u32);
        codec::put(&mut d, 8, 8, self.seq as u32);
        codec::put(&mut d, 16, 32, self.payload);
        d
    }

    /// Decode a frame of DLC bytes.
    pub const fn from_frame(d: &[u8; 8]) -> Self {
        Self {
            target_node: codec::get_unsigned(d, 0, 8) as u8,
            seq: codec::get_unsigned(d, 8, 8) as u8,
            payload: codec::get_unsigned(d, 16, 32),
        }
    }

    /// Decode from a received slice; `None` if it is shorter than DLC.
    pub fn from_buffer(data: &[u8]) -> Option<Self> {
        if data.len() < Self::DLC {
            return None;
        }
        let mut d = [0u8; 8];
        d[..Self::DLC].copy_from_slice(&data[..Self::DLC]);
        Some(Self::from_frame(&d))
    }
}

impl Message for EchoRequest {
    const ID: u16 = Self::ID;
    const DLC: usize = Self::DLC;
    const CYCLE_MS: u16 = Self::CYCLE_MS;
    const SAFETY_RELEVANT: bool = Self::SAFETY_RELEVANT;
    const TEST_ONLY: bool = Self::TEST_ONLY;
    const NAME: &'static str = Self::NAME;
    fn to_frame(&self) -> [u8; 8] {
        Self::to_frame(self)
    }
    fn from_buffer(data: &[u8]) -> Option<Self> {
        Self::from_buffer(data)
    }
}

/// Sensor ECU's answer to ECHO_REQUEST. turnaround is the responder's own receive-to-transmit time, which lets the host separate ECU processing time from bus and OS latency.
/// Senders: SENSOR_ECU
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SensorEchoResponse {
    /// bits 0..7, raw 0..255
    pub seq: u8,
    /// bits 8..39, raw 0..4294967295
    pub payload: u32,
    /// bits 40..55 [us], raw 0..65535
    pub turnaround: u16,
}

impl SensorEchoResponse {
    pub const ID: u16 = 0x301;
    pub const DLC: usize = 7;
    pub const CYCLE_MS: u16 = 0;
    pub const SAFETY_RELEVANT: bool = false;
    pub const TEST_ONLY: bool = false;
    pub const NAME: &'static str = "SENSOR_ECHO_RESPONSE";

    /// Encode the raw field values into a frame of DLC bytes (the rest zero).
    pub const fn to_frame(&self) -> [u8; 8] {
        let mut d = [0u8; 8];
        codec::put(&mut d, 0, 8, self.seq as u32);
        codec::put(&mut d, 8, 32, self.payload);
        codec::put(&mut d, 40, 16, self.turnaround as u32);
        d
    }

    /// Decode a frame of DLC bytes.
    pub const fn from_frame(d: &[u8; 8]) -> Self {
        Self {
            seq: codec::get_unsigned(d, 0, 8) as u8,
            payload: codec::get_unsigned(d, 8, 32),
            turnaround: codec::get_unsigned(d, 40, 16) as u16,
        }
    }

    /// Decode from a received slice; `None` if it is shorter than DLC.
    pub fn from_buffer(data: &[u8]) -> Option<Self> {
        if data.len() < Self::DLC {
            return None;
        }
        let mut d = [0u8; 8];
        d[..Self::DLC].copy_from_slice(&data[..Self::DLC]);
        Some(Self::from_frame(&d))
    }
}

impl Message for SensorEchoResponse {
    const ID: u16 = Self::ID;
    const DLC: usize = Self::DLC;
    const CYCLE_MS: u16 = Self::CYCLE_MS;
    const SAFETY_RELEVANT: bool = Self::SAFETY_RELEVANT;
    const TEST_ONLY: bool = Self::TEST_ONLY;
    const NAME: &'static str = Self::NAME;
    fn to_frame(&self) -> [u8; 8] {
        Self::to_frame(self)
    }
    fn from_buffer(data: &[u8]) -> Option<Self> {
        Self::from_buffer(data)
    }
}

/// Controller ECU's answer to ECHO_REQUEST, same layout as SENSOR_ECHO_RESPONSE.
/// Senders: CONTROLLER_ECU
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ControllerEchoResponse {
    /// bits 0..7, raw 0..255
    pub seq: u8,
    /// bits 8..39, raw 0..4294967295
    pub payload: u32,
    /// bits 40..55 [us], raw 0..65535
    pub turnaround: u16,
}

impl ControllerEchoResponse {
    pub const ID: u16 = 0x302;
    pub const DLC: usize = 7;
    pub const CYCLE_MS: u16 = 0;
    pub const SAFETY_RELEVANT: bool = false;
    pub const TEST_ONLY: bool = false;
    pub const NAME: &'static str = "CONTROLLER_ECHO_RESPONSE";

    /// Encode the raw field values into a frame of DLC bytes (the rest zero).
    pub const fn to_frame(&self) -> [u8; 8] {
        let mut d = [0u8; 8];
        codec::put(&mut d, 0, 8, self.seq as u32);
        codec::put(&mut d, 8, 32, self.payload);
        codec::put(&mut d, 40, 16, self.turnaround as u32);
        d
    }

    /// Decode a frame of DLC bytes.
    pub const fn from_frame(d: &[u8; 8]) -> Self {
        Self {
            seq: codec::get_unsigned(d, 0, 8) as u8,
            payload: codec::get_unsigned(d, 8, 32),
            turnaround: codec::get_unsigned(d, 40, 16) as u16,
        }
    }

    /// Decode from a received slice; `None` if it is shorter than DLC.
    pub fn from_buffer(data: &[u8]) -> Option<Self> {
        if data.len() < Self::DLC {
            return None;
        }
        let mut d = [0u8; 8];
        d[..Self::DLC].copy_from_slice(&data[..Self::DLC]);
        Some(Self::from_frame(&d))
    }
}

impl Message for ControllerEchoResponse {
    const ID: u16 = Self::ID;
    const DLC: usize = Self::DLC;
    const CYCLE_MS: u16 = Self::CYCLE_MS;
    const SAFETY_RELEVANT: bool = Self::SAFETY_RELEVANT;
    const TEST_ONLY: bool = Self::TEST_ONLY;
    const NAME: &'static str = Self::NAME;
    fn to_frame(&self) -> [u8; 8] {
        Self::to_frame(self)
    }
    fn from_buffer(data: &[u8]) -> Option<Self> {
        Self::from_buffer(data)
    }
}

/// Test-only control message. Accepted only by firmware built with the test hooks enabled, and only when magic equals 0xC0DE. Drives the software fault-injection hooks (Phase 3).
/// Senders: TEST_HOST
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct TestControl {
    /// bits 0..15, raw 0..65535
    pub magic: u16,
    /// bits 16..23, raw 0..255, value table `Node`
    pub target_node: u8,
    /// bits 24..31, raw 0..255, value table `TestCommand`. See value table. Arguments: NOP, RESET_FAULTS none. INJECT_CAN_ERROR arg0 = number of simulated transmit error events (TEC +8 each; 16 reach error-passive, 32 bus-off). FORCE_BUS_OFF none. SUPPRESS_HEARTBEAT arg0 = duration in ms during which the node's heartbeat is not transmitted. CORRUPT_CRC arg0 = number of upcoming protected frames transmitted with a corrupted CRC byte. SKIP_COUNTER arg0 = number of upcoming protected frames whose rolling counter skips one value. FREEZE_COUNTER arg0 = number of upcoming protected frames that repeat the previous counter. SUPPRESS_MESSAGE arg0 = frame identifier, arg1 = duration in ms during which that message is not transmitted. STALL_THREAD arg0 = thread (0 receive, 1 transmit, 2 supervisor), arg1 = ms the thread blocks, as if stuck in a blocking call. The supervisor stops feeding the watchdog once the thread's check-in is older than the liveness window (300 ms) and feeds again when it checks in, so the node resets only if the stall outlasts the liveness window plus the watchdog timeout (about 1.3 s nominal; 1.0 to 2.2 s on the Nucleo, whose IWDG runs from the 17 to 47 kHz LSI); a shorter stall only sets the watchdog-warning flag and counts skipped feeds. A stalled supervisor feeds nothing, so the watchdog timeout alone decides its reset.
    pub command: u8,
    /// bits 32..47, raw 0..65535
    pub arg0: u16,
    /// bits 48..63, raw 0..65535
    pub arg1: u16,
}

impl TestControl {
    pub const ID: u16 = 0x7E0;
    pub const DLC: usize = 8;
    pub const CYCLE_MS: u16 = 0;
    pub const SAFETY_RELEVANT: bool = false;
    pub const TEST_ONLY: bool = true;
    pub const NAME: &'static str = "TEST_CONTROL";
    pub const MAGIC: u16 = 0xC0DE;

    /// `target_node` as its value table, `None` for a raw value outside it.
    pub const fn target_node_enum(&self) -> Option<Node> {
        Node::from_raw(self.target_node)
    }
    pub fn set_target_node(&mut self, value: Node) {
        self.target_node = value as u8;
    }

    /// `command` as its value table, `None` for a raw value outside it.
    pub const fn command_enum(&self) -> Option<TestCommand> {
        TestCommand::from_raw(self.command)
    }
    pub fn set_command(&mut self, value: TestCommand) {
        self.command = value as u8;
    }

    /// Encode the raw field values into a frame of DLC bytes (the rest zero).
    pub const fn to_frame(&self) -> [u8; 8] {
        let mut d = [0u8; 8];
        codec::put(&mut d, 0, 16, self.magic as u32);
        codec::put(&mut d, 16, 8, self.target_node as u32);
        codec::put(&mut d, 24, 8, self.command as u32);
        codec::put(&mut d, 32, 16, self.arg0 as u32);
        codec::put(&mut d, 48, 16, self.arg1 as u32);
        d
    }

    /// Decode a frame of DLC bytes.
    pub const fn from_frame(d: &[u8; 8]) -> Self {
        Self {
            magic: codec::get_unsigned(d, 0, 16) as u16,
            target_node: codec::get_unsigned(d, 16, 8) as u8,
            command: codec::get_unsigned(d, 24, 8) as u8,
            arg0: codec::get_unsigned(d, 32, 16) as u16,
            arg1: codec::get_unsigned(d, 48, 16) as u16,
        }
    }

    /// Decode from a received slice; `None` if it is shorter than DLC.
    pub fn from_buffer(data: &[u8]) -> Option<Self> {
        if data.len() < Self::DLC {
            return None;
        }
        let mut d = [0u8; 8];
        d[..Self::DLC].copy_from_slice(&data[..Self::DLC]);
        Some(Self::from_frame(&d))
    }
}

impl Message for TestControl {
    const ID: u16 = Self::ID;
    const DLC: usize = Self::DLC;
    const CYCLE_MS: u16 = Self::CYCLE_MS;
    const SAFETY_RELEVANT: bool = Self::SAFETY_RELEVANT;
    const TEST_ONLY: bool = Self::TEST_ONLY;
    const NAME: &'static str = Self::NAME;
    fn to_frame(&self) -> [u8; 8] {
        Self::to_frame(self)
    }
    fn from_buffer(data: &[u8]) -> Option<Self> {
        Self::from_buffer(data)
    }
}

/// All messages of the network, ordered by identifier (bus priority).
pub const MESSAGES: [MessageInfo; 9] = [
    MessageInfo { id: ControllerCommand::ID, dlc: ControllerCommand::DLC as u8, cycle_ms: ControllerCommand::CYCLE_MS, safety_relevant: ControllerCommand::SAFETY_RELEVANT, test_only: ControllerCommand::TEST_ONLY, name: ControllerCommand::NAME },
    MessageInfo { id: SensorData::ID, dlc: SensorData::DLC as u8, cycle_ms: SensorData::CYCLE_MS, safety_relevant: SensorData::SAFETY_RELEVANT, test_only: SensorData::TEST_ONLY, name: SensorData::NAME },
    MessageInfo { id: BatteryStatus::ID, dlc: BatteryStatus::DLC as u8, cycle_ms: BatteryStatus::CYCLE_MS, safety_relevant: BatteryStatus::SAFETY_RELEVANT, test_only: BatteryStatus::TEST_ONLY, name: BatteryStatus::NAME },
    MessageInfo { id: SensorHeartbeat::ID, dlc: SensorHeartbeat::DLC as u8, cycle_ms: SensorHeartbeat::CYCLE_MS, safety_relevant: SensorHeartbeat::SAFETY_RELEVANT, test_only: SensorHeartbeat::TEST_ONLY, name: SensorHeartbeat::NAME },
    MessageInfo { id: ControllerHeartbeat::ID, dlc: ControllerHeartbeat::DLC as u8, cycle_ms: ControllerHeartbeat::CYCLE_MS, safety_relevant: ControllerHeartbeat::SAFETY_RELEVANT, test_only: ControllerHeartbeat::TEST_ONLY, name: ControllerHeartbeat::NAME },
    MessageInfo { id: EchoRequest::ID, dlc: EchoRequest::DLC as u8, cycle_ms: EchoRequest::CYCLE_MS, safety_relevant: EchoRequest::SAFETY_RELEVANT, test_only: EchoRequest::TEST_ONLY, name: EchoRequest::NAME },
    MessageInfo { id: SensorEchoResponse::ID, dlc: SensorEchoResponse::DLC as u8, cycle_ms: SensorEchoResponse::CYCLE_MS, safety_relevant: SensorEchoResponse::SAFETY_RELEVANT, test_only: SensorEchoResponse::TEST_ONLY, name: SensorEchoResponse::NAME },
    MessageInfo { id: ControllerEchoResponse::ID, dlc: ControllerEchoResponse::DLC as u8, cycle_ms: ControllerEchoResponse::CYCLE_MS, safety_relevant: ControllerEchoResponse::SAFETY_RELEVANT, test_only: ControllerEchoResponse::TEST_ONLY, name: ControllerEchoResponse::NAME },
    MessageInfo { id: TestControl::ID, dlc: TestControl::DLC as u8, cycle_ms: TestControl::CYCLE_MS, safety_relevant: TestControl::SAFETY_RELEVANT, test_only: TestControl::TEST_ONLY, name: TestControl::NAME },
];

pub fn find_message(id: u16) -> Option<&'static MessageInfo> {
    MESSAGES.iter().find(|m| m.id == id)
}
