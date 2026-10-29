//! Thread liveness for the watchdog, the port of `ecu/liveness.hpp`: every
//! supervised thread checks in on each iteration of its loop; the supervisor
//! feeds the watchdog only when every thread has checked in within the window.
//! A single stuck thread therefore resets the ECU even though the supervisor
//! itself is still running, which is the property a plain "feed the watchdog
//! from a timer" would lose.

#[derive(Clone, Copy, Debug)]
pub struct Liveness<const N: usize> {
    window_ms: u32,
    last_checkin_ms: [i64; N],
}

impl<const N: usize> Liveness<N> {
    pub const fn new(window_ms: u32) -> Self {
        Self {
            window_ms,
            last_checkin_ms: [-1; N],
        }
    }

    pub fn check_in(&mut self, thread: usize, now_ms: i64) {
        if thread < N {
            self.last_checkin_ms[thread] = now_ms;
        }
    }

    /// Bit i set: thread i has not checked in within the window (or never).
    pub fn stale_mask(&self, now_ms: i64) -> u32 {
        let mut mask = 0;
        for (i, last) in self.last_checkin_ms.iter().enumerate() {
            if *last < 0 || now_ms - *last > i64::from(self.window_ms) {
                mask |= 1 << i;
            }
        }
        mask
    }

    pub fn all_alive(&self, now_ms: i64) -> bool {
        self.stale_mask(now_ms) == 0
    }

    pub const fn window_ms(&self) -> u32 {
        self.window_ms
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn nothing_is_alive_before_the_first_check_in_and_stale_threads_show_in_the_mask() {
        let mut l: Liveness<3> = Liveness::new(200);
        assert_eq!(l.stale_mask(0), 0x7);
        assert!(!l.all_alive(0));
        assert_eq!(l.window_ms(), 200);
        for t in 0..3 {
            l.check_in(t, 1000);
        }
        assert!(l.all_alive(1200));
        assert!(!l.all_alive(1201), "the window is inclusive");
        l.check_in(0, 1300);
        l.check_in(2, 1300);
        assert_eq!(l.stale_mask(1301), 0x2, "only thread 1 missed its check-in");
        l.check_in(7, 1300); // out of range is ignored
        assert_eq!(l.stale_mask(1301), 0x2);
    }
}
