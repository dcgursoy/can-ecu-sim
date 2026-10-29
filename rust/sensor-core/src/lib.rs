//! The sensor ECU of can-ecu-sim, without an operating system underneath.
//!
//! This crate is `no_std` and allocation-free: it holds everything about the
//! sensor ECU that does not touch a kernel, a socket or a clock, which is the
//! same split the C++ firmware makes between `firmware/lib` (host-tested
//! libraries) and `firmware/app` (Zephyr threads and HAL):
//!
//! * [`messages`]: the message codec generated from `can/network.dbc`
//!   (`can/gen_rust.py`), one struct per message with the raw signal values;
//! * [`codec`]: the bit-level helpers the generated code uses;
//! * [`e2e`]: CRC-8H2F and the 4-bit rolling counter, sender and receiver side;
//! * [`timeout_monitor`], [`protected_rx`], [`node_state`], [`can_error_sm`],
//!   [`error_counter_model`], [`liveness`]: the state machines, each a port of
//!   its C++ counterpart with the same semantics and the same unit tests;
//! * [`sensor`]: the sensor application itself, as a "sans-IO" state machine:
//!   the adapter feeds it received frames, ticks and the time, and it hands
//!   back frames to transmit, events to log and the decision whether to feed
//!   the watchdog.
//!
//! `rust/sensor-vcan` is the thin `std` adapter that runs this crate on a
//! Linux SocketCAN interface; the same crate cross-compiles for
//! `thumbv7em-none-eabihf` (CI proves it), where an RTOS or bare-metal adapter
//! would take the adapter's place.

#![cfg_attr(not(test), no_std)]

pub mod can_error_sm;
pub mod codec;
pub mod e2e;
pub mod error_counter_model;
pub mod liveness;
pub mod node_state;
pub mod protected_rx;
pub mod sensor;
pub mod timeout_monitor;

// The scale factors are printed as the DBC gives them, to more digits than an f32 holds.
#[allow(clippy::excessive_precision)]
pub mod messages {
    //! Generated from `can/network.dbc`; do not edit by hand.
    include!("generated/messages.rs");
}
