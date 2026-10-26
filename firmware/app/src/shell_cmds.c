/*
 * Shell commands: `ecu status` prints the node, CAN, traffic and watchdog state
 * with every error counter; `ecu reset` clears latched faults. Written in C
 * because Zephyr's shell macros rely on C designated initialisers; the data
 * comes from the C++ application through app/status.h.
 */
#include <zephyr/shell/shell.h>

#include <inttypes.h>

#include "app/status.h"

static int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	static struct ecu_status s; /* static: too large for the shell thread's stack */

	ecu_status_get(&s);

	shell_print(sh, "role=%s test_hooks=%d uptime_ms=%" PRId64, s.role, s.test_hooks, s.uptime_ms);
	shell_print(sh, "node: state=%s reason=\"%s\" since_ms=%" PRId64 " safe_state_entries=%" PRIu32
		    " error_flags=0x%02x",
		    s.node_state, s.node_reason, s.node_state_age_ms, s.safe_state_entries, s.error_flags);
	shell_print(sh, "can: phase=%s state=%s tec=%u rec=%u manual_recovery=%d simulated=%d",
		    s.can_phase, s.can_state, s.tec, s.rec, s.manual_recovery, s.simulation_active);
	shell_print(sh, "can: bus_off=%" PRIu32 " passive=%" PRIu32 " recovery_attempts=%" PRIu32
		    " recoveries_ok=%" PRIu32 " recovery_failures=%" PRIu32 " last_recovery_ms=%" PRId64
		    " backoff_ms=%" PRIu32,
		    s.bus_off_events, s.passive_events, s.recovery_attempts, s.recoveries_ok,
		    s.recovery_failures, s.last_recovery_duration_ms, s.current_backoff_ms);
	shell_print(sh, "tx: sent=%" PRIu32 " ok=%" PRIu32 " err=%" PRIu32 " busy=%" PRIu32 " rejected=%" PRIu32
		    " blocked_bus_off=%" PRIu32 " tick_overruns=%" PRIu32,
		    s.tx_sent, s.tx_completed_ok, s.tx_completed_error, s.tx_busy, s.tx_rejected,
		    s.tx_blocked_bus_off, s.tx_tick_overruns);
	shell_print(sh, "rx: frames=%" PRIu32 " unknown=%" PRIu32 " queue_hwm=%" PRIu32 "/%" PRIu32
		    " dropped=%" PRIu32 " events_dropped=%" PRIu32 " echo_responses=%" PRIu32
		    " test_commands=%" PRIu32,
		    s.rx_frames, s.rx_unknown, s.rx_queue_hwm, s.rx_queue_depth, s.rx_dropped,
		    s.can_events_dropped, s.echo_responses, s.test_commands);
	for (size_t i = 0; i < s.rx_count; ++i) {
		const struct ecu_rx_status *r = &s.rx[i];

		shell_print(sh, "rx %s: alive=%d age_ms=%" PRId64 " ok=%" PRIu32 " crc=%" PRIu32 " counter=%" PRIu32
			    " dlc=%" PRIu32 " skipped=%" PRIu32 " lost=%" PRIu32 " timeouts=%" PRIu32,
			    r->name, r->alive, r->age_ms, r->accepted, r->crc_faults, r->counter_faults,
			    r->dlc_faults, r->skipped, r->lost_frames, r->timeouts);
	}
	shell_print(sh, "wdt: available=%d timeout_ms=%" PRIu32 " feeds=%" PRIu32 " skipped=%" PRIu32
		    " stale_threads=0x%" PRIx32,
		    s.wdt_available, s.wdt_timeout_ms, s.wdt_feeds, s.wdt_skipped, s.liveness_stale_mask);
	shell_print(sh, "%s: %s", s.role, s.role_detail);
	return 0;
}

static int cmd_reset(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	ecu_reset_faults();
	shell_print(sh, "faults cleared: FAULT state, CAN recovery backoff, error_flags, watchdog skip count, "
		    "per-message rx fault counters (cumulative tx/rx/bus-off counters kept)");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ecu,
	SHELL_CMD(status, NULL, "Node, CAN, traffic and watchdog status with error counters", cmd_status),
	SHELL_CMD(reset, NULL,
		  "Clear latched faults (FAULT state, exhausted CAN recovery), error_flags and per-message "
		  "RX fault counters",
		  cmd_reset),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(ecu, &sub_ecu, "ECU diagnostics", NULL);
