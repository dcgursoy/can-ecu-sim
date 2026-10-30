#!/usr/bin/env python3
"""Record a command's terminal output, with timestamps, as an asciicast v2 file.

    python3 tools/record_cast.py OUT.cast [--cols 100] [--rows 34] [--idle-limit 3] -- COMMAND [ARGS...]

The command runs under a pseudo-terminal, so it sees a terminal (and keeps its
colours); everything it writes is forwarded to this process's stdout and stored
with the time since the start. Idle gaps longer than --idle-limit are shortened
in the recording. The file is what asciinema would record, and `agg` renders it
to a GIF (tools/record-demo.sh). Linux only (pty, inside the dev container).
"""

from __future__ import annotations

import argparse
import fcntl
import json
import os
import pty
import select
import struct
import subprocess
import sys
import termios
import time


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out")
    parser.add_argument("--cols", type=int, default=100)
    parser.add_argument("--rows", type=int, default=34)
    parser.add_argument("--idle-limit", type=float, default=3.0, help="cap idle gaps at this many seconds")
    parser.add_argument("command", nargs="*", help="the command to record, after --")
    # Everything after the first "--" is the command, whatever it looks like.
    argv = sys.argv[1:]
    if "--" in argv:
        own, command = argv[:argv.index("--")], argv[argv.index("--") + 1:]
    else:
        own, command = argv, []
    args = parser.parse_args(own)
    command = command or args.command
    if not command:
        parser.error("no command given (put it after --)")

    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", args.rows, args.cols, 0, 0))
    env = dict(os.environ, TERM="xterm-256color", COLUMNS=str(args.cols), LINES=str(args.rows), PYTHONUNBUFFERED="1")
    proc = subprocess.Popen(command, stdin=slave, stdout=slave, stderr=slave, env=env, close_fds=True,
                            start_new_session=True)
    os.close(slave)

    t0 = time.monotonic()
    last = 0.0
    shift = 0.0  # time removed from idle gaps
    with open(args.out, "w", encoding="utf-8") as f:
        header = {"version": 2, "width": args.cols, "height": args.rows, "timestamp": int(time.time()),
                  "env": {"TERM": env["TERM"], "SHELL": "/bin/bash"}, "title": " ".join(command)}
        f.write(json.dumps(header) + "\n")
        while True:
            ready, _, _ = select.select([master], [], [], 0.05)
            if master in ready:
                try:
                    data = os.read(master, 65536)
                except OSError:
                    break  # the slave side is closed: the command has exited
                if not data:
                    break
                t = time.monotonic() - t0 - shift
                if t - last > args.idle_limit:
                    shift += (t - last) - args.idle_limit
                    t = last + args.idle_limit
                last = t
                text = data.decode("utf-8", errors="replace")
                f.write(json.dumps([round(t, 6), "o", text]) + "\n")
                sys.stdout.write(text)
                sys.stdout.flush()
            elif proc.poll() is not None:
                break  # exited, and nothing left to read
    rc = proc.wait()
    os.close(master)
    print(f"\nrecorded {args.out}: {last:.1f} s of terminal time, exit status {rc}", file=sys.stderr)
    return rc


if __name__ == "__main__":
    sys.exit(main())
