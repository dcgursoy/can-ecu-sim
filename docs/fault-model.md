# Fault model: what the simulated faults stand for

This project has two places to inject faults, and both are software:

* **The gateway** (`gateway/`) sits between the two ECUs' bus segments
  (`vcan0` for the sensor, `vcan1` for the controller) and acts on frames in
  flight: loss, latency, duplication, reordering, corruption and flooding. The
  firmware is not involved and cannot tell the gateway from a bus; this is the
  black-box view a test bench with a programmable bus disturber would have.
* **The firmware hooks** (`TEST_CONTROL`, id 0x7E0, `firmware/app/src/app/test_control.cpp`)
  make a node misbehave from the inside: a sender that goes quiet, computes a
  wrong CRC, loses or repeats a counter value, or a thread that blocks. These
  are faults a bus cannot produce. They run through the production transmit
  path (`app::protect`, `app::transmit`) and exist only in builds with
  `CONFIG_APP_TEST_HOOKS` (on by default for `native_sim`, off for the Nucleo,
  where the message is counted as unknown and nothing on the bus can alter the
  firmware's behaviour).

Nothing here is a model of the physical layer. The table at the end lists what
is not reproduced. Every number quoted in the phase logs comes from a simulated
bus on a desktop OS and is labelled as such.

## 1. Bus-level faults (gateway)

The gateway forwards each direction through a FIFO link with a configurable
latency; faults are decided per frame by a seeded random generator, so a
scenario is reproducible from its seed (`--seed`). `ids` limits a direction's
faults to chosen identifiers. `gateway/faults.py` is the whole decision logic
and is unit-tested with a fake clock (`gateway/test_faults.py`).

| Simulated fault | What it stands for on real hardware | What the receiver sees, and the defence that catches it |
|---|---|---|
| **drop** (frame not forwarded, probability per frame) | A frame that never reaches the application: receive FIFO overrun in a busy node, a gateway or router dropping on a full buffer, a sender that went bus-off mid-stream, a listen-only node that cannot acknowledge, a frame destroyed by an error burst after the sender exhausted retransmission (single-shot mode). Plain CAN retransmits automatically, so sporadic single losses on one bus are rarer than in this model; losses cluster around bus-off and overload. | The rolling counter arrives with a delta of 2..8: *accepted with loss* (`skipped`, `lost` in `ecu status`), the application knows how many frames it missed. A run long enough to exceed the message timeout (three SENSOR_DATA frames: 40 ms > 35 ms) is *silence* to the timeout monitor: SAFE_STATE for a critical input, DEGRADED for a non-critical one. The `drop-random` scenario checks that `dropped == counted lost + frames inside timeout gaps` exactly, from the captured frames. |
| **delay + jitter** (fixed latency plus uniform jitter, FIFO) | Queueing in a gateway, arbitration delay under bus load (a message waits for every higher-priority frame that is pending, at most one maximum-length frame per higher-priority id), scheduling latency in the sender's RTOS. | Nothing, by design: the timeout monitor measures the gap between consecutive accepted frames, not the age of the data, so a constant latency is invisible and only jitter larger than `timeout - period` is noticed. Detecting latency would need a sender timestamp in the payload (the heartbeats carry `uptime`; the data messages do not). The `delay-jitter` scenario proves that 15 +/- 5 ms on a 10 ms message produces no fault. |
| **duplicate** (frame forwarded twice) | The double-receive case of ISO 11898-1: an error in the last bit of the end-of-frame is seen by the transmitter but not by every receiver, so the transmitter retransmits a frame some receivers already accepted. Also a gateway retransmitting after a lost acknowledgement. | Counter delta 0: *repeated*, rejected, never used twice; `error_flags` bit COUNTER_FAULT_SEEN is published in the heartbeat. |
| **reorder** (a frame held back and released right after the next frame of the same id) | A controller with several transmit mailboxes serving frames of the same id out of request order (bxCAN transmits by identifier priority and then by mailbox number, not by request time), a gateway with per-priority queues, a sender re-queuing a frame after an abort. On a single bus a single mailbox cannot reorder. | The newer frame arrives first with delta 2 (*accepted with loss*), then the stale frame with delta 15 (*out of sequence*): rejected, the reference does not move, so stale data never replaces newer data. The `reorder` scenario checks both counts equal the number of swaps. |
| **corrupt** (one bit flipped in one data byte) | What the end-to-end CRC exists for: corruption *outside* the CAN frame CRC's reach. The CAN CRC-15 covers the wire, but data can be damaged before it (a wrong buffer, a race in the sender's memory, DMA, a gateway that re-encodes payloads, a protocol-version mismatch) or escape it (the residual undetected-error rate of CAN is on the order of 4.7e-11 per bit error). The `corrupt_crc_only` variant damages the CRC byte itself, which is what a sender with a wrong CRC implementation produces. | CRC-8H2F mismatch: rejected (`crc` in `ecu status`). Three consecutive rejects of any message are a quality fault (DEGRADED); a run of rejected SENSOR_DATA frames longer than the timeout is silence (SAFE_STATE). Any single-bit error is detected (HD 4 over the protected bits, measured in Phase 1). |
| **flood** (one identifier at a fixed rate on one segment) | A babbling node: stuck transmit logic, a runaway task, a misconfigured high-rate message, or a deliberate denial of service. On a real bus the flooded id 0x001 wins every arbitration, so lower-priority traffic is delayed or starved and the bus load rises (2000 frames/s of 8-byte frames is roughly 260 kbit/s, half of a 500 kbit/s bus). | Only the receive path is exercised: driver interrupt, queue, the RX thread counting the frames as `unknown`, and `dropped` if the 32-frame queue overflows. vcan has no arbitration and no bandwidth limit, so **starvation and bus load are not reproduced**; on real hardware the first defence is the controller's acceptance filter (the Nucleo build could narrow its `can_add_rx_filter` masks so the frames never reach software), the second is a bus guardian. The flood stays on one segment because the gateway's socket does not see its own frames. |

The gateway itself adds latency (a Python relay on a desktop OS; the
`baseline` scenario measures it from the two captures) and models each
direction as a FIFO, so variable latency alone never reorders frames; only the
reorder fault does. Extended, remote and error frames are not forwarded.

## 2. Node-level faults (firmware hooks)

| TEST_CONTROL command | What it stands for | What the peer sees |
|---|---|---|
| **SUPPRESS_HEARTBEAT** `arg0` ms | An application task that stopped running while the node is otherwise alive (deadlock, a task that crashed and was not restarted, a scheduler starved by a higher-priority loop); the CAN driver keeps working, other messages keep flowing. | Heartbeat timeout (350 ms): the controller enters SAFE_STATE and publishes `mode=SAFE_STATE`; recovery 500 ms after the heartbeat returns. |
| **SUPPRESS_MESSAGE** `arg0` id, `arg1` ms | The same for one message, e.g. a control loop that hangs while the heartbeat task keeps going: the classic case the heartbeat alone does not catch and the per-message timeout does. | The sensor times out CONTROLLER_COMMAND (70 ms) and degrades (it has no actuator, so no safe state), then returns to RUNNING. |
| **CORRUPT_CRC** `arg0` frames | A sender with a wrong CRC: a mismatched protocol version or polynomial, a frame modified after its CRC was computed. | Rejected by CRC. Two in a row are tolerated (counted); five in a row are 40 ms or more without an accepted SENSOR_DATA frame: timeout and safe state. |
| **SKIP_COUNTER** `arg0` frames | The sender drops frames internally: its transmit queue overflowed, a mailbox was busy and the frame was abandoned, a scheduling slot was missed. The counter moved, the frame never left. | Delta 2: accepted with loss; `lost` counts one frame per skip. No rejection, no timeout. |
| **FREEZE_COUNTER** `arg0` frames | Stale data: the task that fills the message stopped, but the periodic transmission keeps repeating the last buffer. Without the counter the peer would consume stale values forever. | Delta 0: rejected as repeated. Two in a row are tolerated; three would be a quality fault and, for SENSOR_DATA, a timeout. |
| **STALL_THREAD** `arg0` thread, `arg1` ms | A thread blocked in a call that never returns: deadlock, priority inversion, waiting on a dead peripheral. The other threads, including the supervisor, keep running. | Nothing on the bus from that thread (the TX thread case silences every periodic message). Inside the node: the liveness monitor sees the thread's check-in go stale after 300 ms, the supervisor withholds the watchdog feed, and the watchdog (1000 ms) resets the node: on the Nucleo the independent watchdog (IWDG, its own LSI clock), on `native_sim` the counter-based watchdog followed by the simulator re-executing itself. The reset needs a stall longer than the liveness window plus the watchdog timeout (about 1.3 s; 1.0 to 2.2 s on the Nucleo with the LSI tolerance below); the feed decision is level-triggered, so a shorter stall only sets the watchdog-warning flag and counts skipped feeds. A stalled supervisor feeds nothing, so for it the watchdog timeout alone decides. The peer sees a safe-state entry and a recovery. |
| **INJECT_CAN_ERROR** `arg0` events, **FORCE_BUS_OFF** (Phase 2) | Transmit errors: a failing transceiver, a dominant bit stuck on the bus, wrong bit timing on one node, missing termination. Each event is TEC +8 in the simulated controller behind the HAL; SocketCAN/vcan has no error counters of its own. | Error-passive after 16 events, bus-off after 32: the CAN error state machine runs its backoff and recovery; on the Nucleo the real bxCAN counters drive the same state machine. |

## 3. How the scenarios are judged

`tools/fault_scenarios.py` runs every scenario against one network (both ECUs
and the gateway stay up; `RESET_FAULTS` clears latched state in between) and
judges each one from three independent sources:

1. the delta of each ECU's `ecu status` counters across the scenario
   (`crc`, `counter`, `skipped`, `lost`, `timeouts`, `safe_state_entries`,
   the hook counters, the node state),
2. the gateway's own counters (`received`, `forwarded`, `dropped`,
   `duplicated`, `reordered`, `corrupted`, per id),
3. the frames captured with `candump` on both segments (kernel timestamps),
   decoded with the DBC.

Where a random fault can take more than one legitimate course, the expected
outcome is computed from the capture rather than assumed: a run of lost
SENSOR_DATA frames is a timeout only if the gap exceeds 35 ms, a run of
corrupted frames only if it is three or more. The checks then compare the
firmware's counters against those expectations exactly. A gap that falls
within 4 ms of a timeout limit is reported and not judged, because the
firmware's own clock decides it.

Timing checks allow for the mechanics of the firmware: the 350 ms heartbeat
timeout is noticed by the 5 ms supervisor tick and published by the next
20 ms command, so SAFE_STATE is expected 340..420 ms after the last
heartbeat; recovery is the 500 ms hold plus the same two steps.

Host stalls are classified as in the Phase 2 demos: a gap in an ECU's own
output on its own segment that no scenario explains means the ECU process was
not scheduled, a gap in the relayed copy only means the gateway process was
not; either makes the run INCONCLUSIVE rather than FAIL, and CI repeats it.

## 4. What is not simulated

* The physical layer: shorts to ground or supply, open wires, missing or
  double termination, transceiver failures, EMI bursts, ground offsets.
* Bit timing and sample-point errors, clock drift between nodes, CAN FD.
* Arbitration, bus load and priority inversion on the bus; error frames,
  automatic retransmission and the resulting latency; overload frames.
* The CAN controller's acceptance filters (the `native_sim` driver accepts
  every standard frame) and its error counters (simulated behind the HAL).
* Watchdog timing tolerance: the Nucleo's IWDG runs from the LSI oscillator,
  whose frequency is specified between 17 and 47 kHz on the STM32F446, so a
  nominal 1000 ms timeout is anywhere between about 0.7 and 1.9 s on real
  silicon. The liveness window (300 ms) and the timeout are chosen with that
  margin in mind.
* Real-time behaviour in general: both ECUs and the gateway are desktop
  processes; their numbers describe the firmware logic, not an MCU's timing.
