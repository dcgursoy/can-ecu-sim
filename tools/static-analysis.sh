#!/usr/bin/env bash
# Static analysis of the firmware: clang-tidy over the application sources
# (both roles, as the native_sim builds compile them) and the library headers
# with their host unit tests; cppcheck over the library headers and their
# unit tests.
#
#   tools/static-analysis.sh [--clang-tidy-only | --cppcheck-only] [--fix]
#
# Needs the native_sim builds of both roles (tools/build-ecu.sh native_sim
# <role>) and the host unit-test build (tools/run-host-tests.sh); it builds the
# missing ones. Reports land in build/static-analysis/: clang-tidy.log and
# cppcheck.xml / cppcheck.txt. Every finding is an error (clang-tidy's
# WarningsAsErrors is '*' in .clang-tidy, cppcheck runs with --error-exitcode),
# so a clean run is the only way through CI. Suppressions, where a tool is
# wrong, are inline with their reason. Runs inside the dev container
# (clang-tidy 18, cppcheck 2.13).
#
# Why cppcheck stops at the libraries: its preprocessor cannot expand Zephyr's
# IS_ENABLED() (the comma-pasting trick in sys/util_internal.h) and skips every
# unit it fails on, which is every application unit; analysing the compiler's
# preprocessed output instead loses the line markers' accuracy. The
# application is clang-tidy's alone, which parses it with clang. DESIGN.md
# section 15 has the evidence.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
OUT="build/static-analysis"
RUN_TIDY=1
RUN_CPPCHECK=1
FIX=()
for arg in "$@"; do
    case "$arg" in
        --clang-tidy-only) RUN_CPPCHECK=0 ;;
        --cppcheck-only) RUN_TIDY=0 ;;
        --fix) FIX=(-fix) ;;
        *) echo "unknown option $arg" >&2; exit 2 ;;
    esac
done

for role in sensor controller; do
    if [[ ! -f "build/native_sim_${role}/compile_commands.json" ]]; then
        tools/build-ecu.sh native_sim "$role"
    fi
done
if [[ ! -f build/host-tests/compile_commands.json ]]; then
    cmake -S firmware/lib -B build/host-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCAN_ECU_COVERAGE=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
fi

mkdir -p "$OUT"
python3 tools/static_analysis_db.py "$OUT" \
    build/native_sim_sensor/compile_commands.json \
    build/native_sim_controller/compile_commands.json \
    build/host-tests/compile_commands.json

# The translation units clang-tidy analyses: everything in the merged database.
# The library headers are analysed through the units that include them (the
# application and the unit tests); HeaderFilterRegex in .clang-tidy selects them.
FILES=$(python3 -c "
import json; print('\n'.join(sorted({e['file'] for e in json.load(open('$OUT/compile_commands.json'))})))")
status=0

if [[ "$RUN_TIDY" -eq 1 ]]; then
    echo "== clang-tidy ($(echo "$FILES" | wc -l) translation units, $(nproc) jobs)"
    # run-clang-tidy exits non-zero on any diagnostic; -quiet keeps the log to the findings.
    # shellcheck disable=SC2086
    if run-clang-tidy -p "$OUT" -quiet -j "$(nproc)" ${FIX[@]+"${FIX[@]}"} $FILES > "$OUT/clang-tidy.log" 2>&1; then
        echo "clang-tidy: clean"
    else
        status=1
        grep -E "(warning|error):" "$OUT/clang-tidy.log" | sort | uniq -c | sort -rn | head -n 60
        echo "clang-tidy: findings above (full log: $OUT/clang-tidy.log)"
    fi
fi

if [[ "$RUN_CPPCHECK" -eq 1 ]]; then
    echo "== cppcheck (library headers through their unit tests)"
    # GoogleTest's TEST() is given a shape cppcheck can parse; the assertion
    # macros read as function calls, which is enough. useStlAlgorithm is off:
    # the libraries are constexpr and C++17's algorithms are not, so the raw
    # loops it points at cannot be replaced.
    if cppcheck --project="$OUT/cppcheck/compile_commands.json" \
            -D'TEST(suite,name)=void suite##_##name()' \
            --enable=warning,style,performance,portability --std=c++17 --platform=unix64 \
            --inline-suppr --suppress=missingIncludeSystem --suppress=useStlAlgorithm \
            --suppress=unmatchedSuppression \
            --error-exitcode=1 -j "$(nproc)" --quiet \
            --xml --output-file="$OUT/cppcheck.xml" 2>"$OUT/cppcheck.txt"; then
        echo "cppcheck: clean"
    else
        status=1
        python3 - "$OUT/cppcheck.xml" <<'EOF'
import sys, xml.etree.ElementTree as ET
errors = list(ET.parse(sys.argv[1]).getroot().iter("error"))
print(f"cppcheck findings: {len(errors)}")
for e in errors:
    loc = e.find("location")
    where = f"{loc.get('file')}:{loc.get('line')}" if loc is not None else "-"
    print(f"  {where}: {e.get('severity')} [{e.get('id')}] {e.get('msg')}")
EOF
    fi
fi
exit $status
