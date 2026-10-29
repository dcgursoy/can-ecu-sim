#!/usr/bin/env python3
"""Write the compile databases clang-tidy and cppcheck read, from the ones the
builds leave behind.

    python3 tools/static_analysis_db.py <out-dir> <compile_commands.json>...

Each input database (a Zephyr native_sim build, the host unit-test build) is
reduced to the translation units under firmware/.

<out-dir>/compile_commands.json, for clang-tidy, holds every unit with its
command line rewritten so that clang accepts it: the compiler is replaced by
clang++/clang (the driver only matters for argument parsing), the gcc-only
options clang rejects are dropped, and warnings about options clang does not
know are silenced. Everything that decides what the code means (defines,
include paths, the -imacros autoconf.h of the Zephyr build, -std, -m32) is
kept, so the analysis sees the same code the firmware compiler saw. A file
that appears in several builds (ecu.cpp in the sensor and in the controller
build) is kept once per build, so both roles' preprocessing is analysed.

<out-dir>/cppcheck/compile_commands.json, for cppcheck, holds only the units
of builds that do not include Zephyr's headers (the host unit tests of the
libraries), with their original gcc command lines. cppcheck 2.13's own
preprocessor cannot expand Zephyr's IS_ENABLED() (the comma-pasting trick in
sys/util_internal.h) and skips every unit it fails on, and feeding it the
compiler's preprocessed output mis-tracks the line markers, so the
application is clang-tidy's alone (DESIGN.md section 15).
"""

from __future__ import annotations

import json
import pathlib
import shlex
import sys

REPO = pathlib.Path(__file__).resolve().parents[1]

# gcc options clang does not accept (it rejects unknown -f/-m options, and
# --param). Options clang merely does not know as warnings (-Wno-...) are left
# in and silenced with -Wno-unknown-warning-option.
DROP_EXACT = {"-fno-reorder-functions", "-fno-defer-pop", "-fcheck-new", "-fno-printf-return-value",
              "-fno-freestanding", "-fdiagnostics-color=always"}
DROP_PREFIXES = ("--param=", "-specs=", "-fmacro-prefix-map=")
EXTRA = ["-Wno-unknown-warning-option", "-Wno-unused-command-line-argument", "-Wno-ignored-optimization-argument"]
TAKES_ARGUMENT = {"-imacros", "-include", "-I", "-isystem", "-D", "-U", "-o", "-MF", "-MT", "-MQ"}


def relative(entry: dict) -> pathlib.Path | None:
    path = pathlib.Path(entry["file"])
    try:
        rel = path.resolve().relative_to(REPO)
    except ValueError:
        return None
    return rel if rel.parts[0] == "firmware" else None


def arguments(entry: dict) -> list[str]:
    return shlex.split(entry["command"]) if "command" in entry else list(entry["arguments"])


def for_clang_tidy(entry: dict) -> dict:
    path = pathlib.Path(entry["file"])
    args = arguments(entry)
    is_cxx = path.suffix in (".cpp", ".cc", ".cxx")
    out = ["clang++" if is_cxx else "clang"]
    skip_next = False
    for a in args[1:]:
        if skip_next:
            skip_next = False
            out.append(a)
            continue
        if a in DROP_EXACT or a.startswith(DROP_PREFIXES):
            continue
        if a in TAKES_ARGUMENT:
            skip_next = True
        out.append(a)
    out += EXTRA
    return {"directory": entry["directory"], "file": str(path), "arguments": out}


def includes_zephyr(entry: dict) -> bool:
    return any(a == "-DKERNEL" or a.startswith("-D__ZEPHYR__") for a in arguments(entry))


def main(argv: list[str]) -> int:
    if len(argv) < 3:
        print(__doc__)
        return 2
    out_dir = pathlib.Path(argv[1]).resolve()
    tidy: list[dict] = []
    cppcheck: list[dict] = []
    for db in argv[2:]:
        entries = json.loads(pathlib.Path(db).read_text())
        kept = 0
        for e in entries:
            if relative(e) is None:
                continue
            kept += 1
            tidy.append(for_clang_tidy(e))
            if not includes_zephyr(e):
                cppcheck.append({"directory": e["directory"], "file": e["file"], "arguments": arguments(e)})
        print(f"{db}: {kept} of {len(entries)} translation units under firmware/")
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "compile_commands.json").write_text(json.dumps(tidy, indent=1) + "\n")
    (out_dir / "cppcheck").mkdir(exist_ok=True)
    (out_dir / "cppcheck" / "compile_commands.json").write_text(json.dumps(cppcheck, indent=1) + "\n")
    print(f"wrote {out_dir / 'compile_commands.json'} ({len(tidy)} entries for clang-tidy) and "
          f"cppcheck/compile_commands.json ({len(cppcheck)} entries without Zephyr headers)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
