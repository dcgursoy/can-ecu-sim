/* Snapshot of the ECU state for the shell (C interface, used by shell_cmds.c). */
#ifndef APP_STATUS_H
#define APP_STATUS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ECU_STATUS_MAX_RX 4
#define ECU_STATUS_DETAIL_LEN 256

struct ecu_rx_status {
	const char *name;
	uint32_t accepted;
	uint32_t crc_faults;
	uint32_t counter_faults; /* repeated + out of sequence */
	uint32_t dlc_faults;
	uint32_t skipped;
	uint32_t lost_frames;
	uint32_t timeouts;
	int64_t age_ms;          /* -1 if never received */
	int alive;
};

struct ecu_status {
	const char *role;
	int test_hooks;
	int64_t uptime_ms;

	const char *node_state;
	const char *node_reason;
	int64_t node_state_age_ms;
	uint32_t safe_state_entries;
	uint8_t error_flags;

	const char *can_phase;
	const char *can_state;
	uint8_t tec;
	uint8_t rec;
	int manual_recovery;
	int simulation_active;
	uint32_t bus_off_events;
	uint32_t passive_events;
	uint32_t recovery_attempts;
	uint32_t recoveries_ok;
	uint32_t recovery_failures;
	int64_t last_recovery_duration_ms;
	uint32_t current_backoff_ms;

	uint32_t tx_sent;
	uint32_t tx_completed_ok;
	uint32_t tx_completed_error;
	uint32_t tx_busy;
	uint32_t tx_rejected;
	uint32_t tx_blocked_bus_off;
	uint32_t tx_tick_overruns;

	uint32_t rx_frames;
	uint32_t rx_unknown;
	uint32_t rx_queue_hwm;
	uint32_t rx_queue_depth;
	uint32_t rx_dropped;         /* frames the driver could not queue (queue full) */
	uint32_t can_events_dropped; /* controller state events lost (event queue full) */
	uint32_t echo_responses;
	uint32_t test_commands;

	int wdt_available;
	uint32_t wdt_timeout_ms;
	uint32_t wdt_feeds;
	uint32_t wdt_skipped;
	uint32_t liveness_stale_mask;

	/* Applied transmit-side test faults (zero in builds without test hooks). */
	uint32_t hook_suppressed;
	uint32_t hook_corrupted;
	uint32_t hook_skipped;
	uint32_t hook_frozen;

	size_t rx_count;
	struct ecu_rx_status rx[ECU_STATUS_MAX_RX];
	char role_detail[ECU_STATUS_DETAIL_LEN];
};

void ecu_status_get(struct ecu_status *out);
void ecu_reset_faults(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_STATUS_H */
