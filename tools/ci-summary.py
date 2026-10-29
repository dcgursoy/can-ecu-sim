#!/usr/bin/env python3
"""Markdown summary of a CI run from the files the steps leave in build/: test
counts of every suite (JUnit), the coverage figures, the static-analysis
verdict and the system suite's measured values. Printed to stdout; the workflow
appends it to $GITHUB_STEP_SUMMARY. Missing files are reported as missing, so a
run that failed early still produces a summary. Runs on the runner (plain
Python 3, standard library only).
"""

from __future__ import annotations

import pathlib
import re
import sys
import xml.etree.ElementTree as ET

REPO = pathlib.Path(__file__).resolve().parents[1]
BUILD = REPO / "build"


def junit(path: pathlib.Path) -> str:
    if not path.exists():
        return "no report"
    root = ET.parse(path).getroot()
    suites = [root] if root.tag == "testsuite" else list(root.iter("testsuite"))
    tests = sum(int(s.get("tests", 0)) for s in suites)
    failures = sum(int(s.get("failures", 0)) + int(s.get("errors", 0)) for s in suites)
    skipped = sum(int(s.get("skipped", 0)) for s in suites)
    time = sum(float(s.get("time", 0)) for s in suites)
    verdict = "passed" if failures == 0 else f"**{failures} failed**"
    return f"{tests} tests, {verdict}" + (f", {skipped} skipped" if skipped else "") + f", {time:.0f} s"


def coverage(path: pathlib.Path) -> str:
    if not path.exists():
        return "no report"
    m = re.search(r"^TOTAL\s+(\d+)\s+(\d+)\s+(\d+)%", path.read_text(), re.M)
    return f"{m.group(3)} % of lines ({m.group(2)} of {m.group(1)})" if m else "unreadable"


def static_analysis() -> str:
    out = BUILD / "static-analysis"
    tidy = out / "clang-tidy.log"
    cpp = [out / "cppcheck.xml"] if (out / "cppcheck.xml").exists() else []
    if not tidy.exists() and not cpp:
        return "not run"
    parts = []
    if tidy.exists():
        n = len(re.findall(r"(warning|error): ", tidy.read_text(errors="replace")))
        parts.append("clang-tidy clean" if n == 0 else f"**clang-tidy: {n} findings**")
    if cpp:
        try:
            n = sum(sum(1 for _ in ET.parse(p).getroot().iter("error")) for p in cpp)
        except ET.ParseError:
            n = -1
        parts.append("cppcheck clean" if n == 0 else ("**cppcheck: report unreadable**" if n < 0 else f"**cppcheck: {n} findings**"))
    return ", ".join(parts)


def main() -> int:
    rows = [
        ("Python unit tests (reference, gateway engine, host tools)", junit(BUILD / "test-results" / "unit-tests.xml")),
        ("C++ unit tests (protocol, ecu_core)", junit(BUILD / "host-tests" / "host-tests.xml")),
        ("Library header coverage (GoogleTest)", coverage(BUILD / "host-tests" / "coverage.txt")),
        ("System test suite on vcan", junit(BUILD / "test-results" / "system-tests.xml")),
        ("System test suite on vcan, Rust sensor ECU in the sensor's place (cross-language conformance)",
         junit(BUILD / "test-results-rust" / "system-tests.xml")),
        ("Firmware coverage under the system suite", coverage(BUILD / "test-results" / "firmware-coverage.txt")),
        ("Static analysis", static_analysis()),
    ]
    print("## can-ecu-sim CI summary\n")
    print("| What | Result |\n|---|---|")
    for what, result in rows:
        print(f"| {what} | {result} |")
    for results, title in (("test-results", "C++ sensor and controller"), ("test-results-rust", "Rust sensor, C++ controller")):
        measurements = BUILD / results / "measurements.md"
        if measurements.exists():
            text = measurements.read_text()
            table = text[text.index("| test |"):] if "| test |" in text else text
            print(f"\n<details><summary>Measured values of the system suite, {title} (simulated bus on a desktop OS)</summary>\n")
            print(table.rstrip())
            print("\n</details>")
    return 0


if __name__ == "__main__":
    sys.exit(main())
