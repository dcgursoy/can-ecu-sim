//! Cross-language conformance of the codec and the protection scheme: the Rust
//! implementation against the same cantools / e2e_ref.py vectors the C++
//! library is checked against (`can/gen_test_vectors.py` collects them once,
//! `can/gen_rust.py` writes them here).

use sensor_core::e2e;
use sensor_core::messages::{
    find_message, BatteryStatus, ControllerMode, Message, NodeState, SensorData, TestCommand,
    TestControl, MESSAGES,
};

// Generated data: the literals are cantools' doubles as printed, the import is
// the generator's; the file is pasted here, so its allows live on the module.
#[allow(
    dead_code,
    clippy::wildcard_imports,
    clippy::excessive_precision,
    clippy::approx_constant
)]
mod vectors {
    include!("generated/vectors.rs");
}

/// A message round-trips through to_frame/from_frame and matches cantools'
/// bytes and scaled decode.
fn check<M>(
    name: &str,
    table: &[vectors::MessageVector<M>],
    phys_of: impl Fn(&M, usize) -> Option<f32>,
) where
    M: Message + PartialEq + std::fmt::Debug,
{
    for (i, v) in table.iter().enumerate() {
        assert_eq!(
            v.expected.to_frame(),
            v.bytes,
            "{name} vector {i}: encoding"
        );
        let decoded = M::from_buffer(&v.bytes[..M::DLC]).expect("DLC");
        assert_eq!(decoded, v.expected, "{name} vector {i}: decoding");
        assert!(
            M::from_buffer(&v.bytes[..M::DLC - 1]).is_none(),
            "{name}: a short buffer is refused"
        );
        for (k, expected) in v.phys.iter().enumerate() {
            if let Some(actual) = phys_of(&decoded, k) {
                let tolerance = (expected.abs() as f32 * 1e-5).max(1e-4);
                assert!(
                    (actual - *expected as f32).abs() <= tolerance,
                    "{name} vector {i} signal {k}: phys {actual} vs {expected}"
                );
            }
        }
    }
}

#[test]
fn messages_match_cantools() {
    // Physical accessors exist only for scaled signals; the index is the DBC signal order.
    check(
        "CONTROLLER_COMMAND",
        &vectors::CONTROLLER_COMMAND,
        |m, k| match k {
            3 => Some(m.torque_cmd_phys()),
            4 => Some(m.steering_cmd_phys()),
            8 => Some(m.speed_limit_phys()),
            _ => None,
        },
    );
    check("SENSOR_DATA", &vectors::SENSOR_DATA, |m, k| match k {
        3 => Some(m.speed_phys()),
        4 => Some(m.yaw_rate_phys()),
        5 => Some(m.accel_x_phys()),
        _ => None,
    });
    check("BATTERY_STATUS", &vectors::BATTERY_STATUS, |m, k| match k {
        5 => Some(m.pack_voltage_phys()),
        6 => Some(m.pack_current_phys()),
        7 => Some(m.state_of_charge_phys()),
        8 => Some(m.temperature_phys()),
        _ => None,
    });
    check("SENSOR_HEARTBEAT", &vectors::SENSOR_HEARTBEAT, |_, _| None);
    check(
        "CONTROLLER_HEARTBEAT",
        &vectors::CONTROLLER_HEARTBEAT,
        |_, _| None,
    );
    check("ECHO_REQUEST", &vectors::ECHO_REQUEST, |_, _| None);
    check(
        "SENSOR_ECHO_RESPONSE",
        &vectors::SENSOR_ECHO_RESPONSE,
        |_, _| None,
    );
    check(
        "CONTROLLER_ECHO_RESPONSE",
        &vectors::CONTROLLER_ECHO_RESPONSE,
        |_, _| None,
    );
    check("TEST_CONTROL", &vectors::TEST_CONTROL, |_, _| None);
}

#[test]
fn physical_setters_round_trip_cantools_values() {
    for v in &vectors::SENSOR_DATA {
        let mut m = SensorData::default();
        m.set_speed_phys(v.phys[3] as f32);
        m.set_yaw_rate_phys(v.phys[4] as f32);
        m.set_accel_x_phys(v.phys[5] as f32);
        assert_eq!(
            (m.speed, m.yaw_rate, m.accel_x),
            (v.expected.speed, v.expected.yaw_rate, v.expected.accel_x)
        );
    }
    for v in &vectors::BATTERY_STATUS {
        let mut m = BatteryStatus::default();
        m.set_pack_voltage_phys(v.phys[5] as f32);
        m.set_pack_current_phys(v.phys[6] as f32);
        m.set_state_of_charge_phys(v.phys[7] as f32);
        m.set_temperature_phys(v.phys[8] as f32);
        assert_eq!(
            (
                m.pack_voltage,
                m.pack_current,
                m.state_of_charge,
                m.temperature
            ),
            (
                v.expected.pack_voltage,
                v.expected.pack_current,
                v.expected.state_of_charge,
                v.expected.temperature
            )
        );
    }
}

#[test]
fn crc_matches_the_python_reference() {
    for (id, bytes, crc) in &vectors::CRC {
        assert_eq!(e2e::frame_crc(*id, bytes), *crc, "id 0x{id:03X}");
    }
}

#[test]
fn protect_matches_the_python_reference_and_every_single_bit_error_is_detected() {
    for (id, plain, counter, protected) in &vectors::E2E {
        let mut frame = *plain;
        e2e::protect_frame(*id, &mut frame, *counter);
        assert_eq!(&frame, protected, "id 0x{id:03X} counter {counter}");
        assert!(e2e::crc_ok(*id, protected));
        assert_eq!(e2e::counter_of(protected), *counter);
        for bit in 8..64 {
            let mut corrupted = *protected;
            corrupted[bit / 8] ^= 1 << (bit % 8);
            assert!(!e2e::crc_ok(*id, &corrupted), "bit {bit} of id 0x{id:03X}");
        }
        for bit in 0..11 {
            assert!(!e2e::crc_ok(*id ^ (1 << bit), protected), "id bit {bit}");
        }
    }
}

#[test]
fn enums_name_every_value_of_their_tables_and_nothing_else() {
    for e in &vectors::ENUMS {
        for raw in 0..(1u32 << e.bits) {
            let expected = e
                .values
                .iter()
                .find(|(v, _)| u32::from(*v) == raw)
                .map_or("UNKNOWN", |(_, label)| *label);
            assert_eq!(
                vectors::enum_name_of(e.name, raw as u8),
                expected,
                "{} value {raw}",
                e.name
            );
        }
    }
    assert_eq!(NodeState::from_raw(3), Some(NodeState::SafeState));
    assert_eq!(TestCommand::from_raw(9), Some(TestCommand::StallThread));
    assert!(ControllerMode::from_raw(7).is_none());
}

#[test]
fn the_message_table_is_ordered_by_identifier_and_complete() {
    let ids: Vec<u16> = MESSAGES.iter().map(|m| m.id).collect();
    let mut sorted = ids.clone();
    sorted.sort_unstable();
    assert_eq!(ids, sorted);
    assert_eq!(MESSAGES.len(), 9);
    assert_eq!(find_message(0x100).unwrap().name, "SENSOR_DATA");
    assert!(find_message(0x7FF).is_none());
    assert!(MESSAGES
        .iter()
        .filter(|m| m.safety_relevant)
        .all(|m| m.dlc == 8));
    assert_eq!(TestControl::MAGIC, 0xC0DE);
}
