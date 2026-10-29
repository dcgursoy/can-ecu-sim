//! Message timeout monitor, the port of `ecu/timeout_monitor.hpp`: tracks when
//! each expected message was last received and reports loss and recovery
//! transitions. Time is passed in (milliseconds since boot), so the logic is
//! tested without a clock.
//!
//! Semantics: every entry starts with a deadline of start + timeout, so a
//! message that never arrives after boot is reported lost exactly once, after
//! its timeout. A received frame renews the deadline; the first frame after a
//! loss is reported as a recovery.

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct MonitoredMessage {
    pub id: u16,
    pub timeout_ms: u32,
    pub name: &'static str,
}

#[derive(Clone, Copy, Debug)]
pub struct Entry {
    pub cfg: MonitoredMessage,
    /// Receive by this time or be declared lost.
    pub deadline_ms: i64,
    /// -1 until the first frame.
    pub last_rx_ms: i64,
    /// True between a reception and its timeout.
    pub alive: bool,
    /// The current loss has been reported.
    pub lost_reported: bool,
    pub received: u32,
    /// Loss events (including "never arrived").
    pub timeouts: u32,
    pub last_loss_ms: i64,
}

#[derive(Clone, Copy, Debug)]
pub struct TimeoutMonitor<const N: usize> {
    entries: [Entry; N],
}

impl<const N: usize> TimeoutMonitor<N> {
    pub const fn new(cfg: [MonitoredMessage; N]) -> Self {
        let mut entries = [Entry {
            cfg: MonitoredMessage {
                id: 0,
                timeout_ms: 0,
                name: "",
            },
            deadline_ms: 0,
            last_rx_ms: -1,
            alive: false,
            lost_reported: false,
            received: 0,
            timeouts: 0,
            last_loss_ms: -1,
        }; N];
        let mut i = 0;
        while i < N {
            entries[i].cfg = cfg[i];
            i += 1;
        }
        Self { entries }
    }

    /// Arm every deadline relative to `now_ms` (call once at startup).
    pub fn start(&mut self, now_ms: i64) {
        for e in &mut self.entries {
            e.deadline_ms = now_ms + i64::from(e.cfg.timeout_ms);
            e.alive = false;
            e.lost_reported = false;
        }
    }

    /// Record a reception. Returns true if this frame ends a loss (recovery).
    pub fn note_received(&mut self, id: u16, now_ms: i64) -> bool {
        let Some(e) = self.entries.iter_mut().find(|e| e.cfg.id == id) else {
            return false;
        };
        let recovered = e.lost_reported;
        e.last_rx_ms = now_ms;
        e.deadline_ms = now_ms + i64::from(e.cfg.timeout_ms);
        e.alive = true;
        e.lost_reported = false;
        e.received = e.received.saturating_add(1);
        recovered
    }

    /// Advance time; `on_lost` is called once per newly lost message.
    pub fn tick(&mut self, now_ms: i64, mut on_lost: impl FnMut(&Entry)) {
        for e in &mut self.entries {
            if !e.lost_reported && now_ms > e.deadline_ms {
                e.alive = false;
                e.lost_reported = true;
                e.timeouts = e.timeouts.saturating_add(1);
                e.last_loss_ms = now_ms;
                on_lost(e);
            }
        }
    }

    pub fn alive(&self, id: u16) -> bool {
        self.find(id).is_some_and(|e| e.alive)
    }

    /// True once the message's deadline has passed without a reception (the
    /// loss was reported) and no frame has arrived since. False while still
    /// waiting for the first frame after `start`.
    pub fn lost(&self, id: u16) -> bool {
        self.find(id).is_some_and(|e| e.lost_reported)
    }

    pub fn all_alive(&self) -> bool {
        self.entries.iter().all(|e| e.alive)
    }

    /// Milliseconds since the last reception, or -1 if nothing was received yet.
    pub fn age_ms(&self, id: u16, now_ms: i64) -> i64 {
        match self.find(id) {
            Some(e) if e.last_rx_ms >= 0 => now_ms - e.last_rx_ms,
            _ => -1,
        }
    }

    pub fn find(&self, id: u16) -> Option<&Entry> {
        self.entries.iter().find(|e| e.cfg.id == id)
    }

    pub const fn entries(&self) -> &[Entry; N] {
        &self.entries
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const CFG: [MonitoredMessage; 2] = [
        MonitoredMessage {
            id: 0x100,
            timeout_ms: 35,
            name: "SENSOR_DATA",
        },
        MonitoredMessage {
            id: 0x200,
            timeout_ms: 350,
            name: "SENSOR_HEARTBEAT",
        },
    ];

    #[test]
    fn a_message_that_never_arrives_is_lost_once_after_its_timeout() {
        let mut m = TimeoutMonitor::new(CFG);
        m.start(1000);
        let mut lost = std::vec::Vec::new();
        m.tick(1035, |e| lost.push(e.cfg.id)); // deadline is inclusive: not yet lost
        assert!(lost.is_empty());
        m.tick(1036, |e| lost.push(e.cfg.id));
        assert_eq!(lost, [0x100]);
        m.tick(1200, |e| lost.push(e.cfg.id)); // not reported again
        assert_eq!(lost.len(), 1);
        m.tick(1351, |e| lost.push(e.cfg.id));
        assert_eq!(lost, [0x100, 0x200]);
        assert_eq!(m.find(0x100).unwrap().timeouts, 1);
        assert!(!m.alive(0x100));
        assert!(m.lost(0x100));
        assert_eq!(m.age_ms(0x100, 2000), -1);
    }

    #[test]
    fn waiting_is_neither_alive_nor_lost_and_reception_renews_the_deadline() {
        let mut m = TimeoutMonitor::new(CFG);
        m.start(0);
        assert!(!m.alive(0x100) && !m.lost(0x100));
        m.tick(20, |_| {});
        assert!(!m.lost(0x100), "still inside the first deadline");
        m.tick(36, |_| {});
        assert!(m.lost(0x100));
        assert!(
            m.note_received(0x100, 40),
            "the first frame after a loss is a recovery"
        );
        assert!(m.alive(0x100) && !m.lost(0x100));
        for t in (50..=140).step_by(10) {
            assert!(!m.note_received(0x100, t));
            m.tick(t + 5, |_| panic!("no loss while frames arrive"));
        }
        assert_eq!(m.find(0x100).unwrap().received, 11);
        assert_eq!(m.age_ms(0x100, 145), 5);
        m.tick(175, |_| panic!("deadline is inclusive"));
        let mut lost = 0;
        m.tick(176, |_| lost += 1);
        assert_eq!(lost, 1);
        assert_eq!(m.find(0x100).unwrap().last_loss_ms, 176);
    }

    #[test]
    fn all_alive_and_unknown_ids() {
        let mut m = TimeoutMonitor::new(CFG);
        m.start(0);
        assert!(!m.all_alive());
        m.note_received(0x100, 1);
        m.note_received(0x200, 1);
        assert!(m.all_alive());
        assert!(!m.note_received(0x7E0, 1), "not monitored");
        assert!(m.find(0x7E0).is_none());
        assert!(!m.alive(0x7E0));
        assert_eq!(m.age_ms(0x7E0, 10), -1);
        assert_eq!(m.entries()[1].cfg.name, "SENSOR_HEARTBEAT");
    }
}
