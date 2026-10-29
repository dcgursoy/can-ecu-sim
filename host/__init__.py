"""Host-side tooling for the CAN network (Phase 4).

    python3 -m host.logger    record an interface to a candump log and a decoded CSV
    python3 -m host.decoder   decode frames live (stream or table) or from a log
    python3 -m host.echo      send ECHO_REQUEST probes and time the ECUs' responses
    python3 -m host.analysis  bus load, period/jitter and echo round-trip percentiles of a capture

Everything is built on python-can (bus access, log formats) and cantools (the
DBC), so it runs unchanged against a real SocketCAN interface. Every timing
number taken from the simulated network is labelled as measured on a simulated
bus on a desktop OS, not on real-time hardware.
"""
