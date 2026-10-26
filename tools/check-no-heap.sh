#!/usr/bin/env bash
# "No dynamic allocation after initialisation": the static half of the proof.
#
#   tools/check-no-heap.sh <build-dir> [nm-command]
#
# Two checks, both fail-closed (a missing tool, an empty build or a missing
# image is an error, never a pass):
#
# 1. Call sites. Every object file in the build that references an allocator
#    (an undefined-symbol reference is a call site) is listed, and the check
#    fails if any of them is outside the allowlist: the kernel's own allocator
#    layering (k_heap on top of sys_heap, which have no callers of their own in
#    this image), the C library's lock shim, the link-time guard itself
#    (src/alloc_guard.c, which wraps the entry points and panics on any call
#    after start-up: the dynamic half of the proof), and the native simulator's
#    host-side runner, which is host code outside the Zephyr image.
#
# 2. The final image. Each wrapped allocator is either absent from the linked
#    image (no reachable call path at all, so --gc-sections removed it together
#    with its wrapper: the strongest result) or present together with its
#    __wrap_ guard. An allocator that survived the link without its guard would
#    be a hole, and fails the check.
#
# The cross nm is picked automatically for Nucleo builds; native_sim builds are
# inspected with the host nm (the final image there is zephyr/zephyr.exe).
set -euo pipefail

BUILD_DIR="${1:?build directory, e.g. build/nucleo_f446re_sensor}"
if [[ $# -ge 2 ]]; then
    NM="$2"
elif [[ "$BUILD_DIR" == *nucleo* ]]; then
    NM="${ZEPHYR_SDK_INSTALL_DIR:?}/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm"
else
    NM="nm"
fi

# C allocator incl. the aligned family and strdup, C++ operator new/new[] incl.
# the C++17 align_val_t and nothrow overloads (32- and 64-bit mangling), and
# the Zephyr heap API incl. the realloc family.
PATTERN=' U (malloc|calloc|realloc|aligned_alloc|posix_memalign|memalign|strdup|_Zn[wa][jm](St11align_val_t)?(RKSt9nothrow_t)?|k_malloc|k_calloc|k_realloc|k_aligned_alloc|k_heap_alloc|k_heap_aligned_alloc|k_heap_calloc|k_heap_realloc|sys_heap_alloc|sys_heap_aligned_alloc|sys_heap_noalign_alloc|sys_heap_realloc|sys_heap_aligned_realloc)$'
ALLOWLIST='kernel\.dir/kheap\.c\.obj|kernel\.dir/mempool\.c\.obj|lib/heap/heap\.c\.obj|picolibc.*/locks\.c\.obj|alloc_guard\.c\.obj|zephyr/NSI/'
WRAPPED="k_heap_alloc k_heap_aligned_alloc k_heap_calloc k_heap_realloc k_malloc k_calloc k_realloc k_aligned_alloc sys_heap_alloc sys_heap_aligned_alloc sys_heap_noalign_alloc sys_heap_realloc sys_heap_aligned_realloc malloc calloc realloc"

command -v "$NM" >/dev/null || { echo "ERROR: nm not found: $NM" >&2; exit 2; }
cd "$BUILD_DIR"
if [[ -f zephyr/zephyr.exe ]]; then
    IMAGE=zephyr/zephyr.exe
    # The host C library's malloc is part of the simulator runtime, not of the firmware.
    WRAPPED="k_heap_alloc k_heap_aligned_alloc k_heap_calloc k_heap_realloc k_malloc k_calloc k_realloc k_aligned_alloc sys_heap_alloc sys_heap_aligned_alloc sys_heap_noalign_alloc sys_heap_realloc sys_heap_aligned_realloc"
elif [[ -f zephyr/zephyr.elf ]]; then
    IMAGE=zephyr/zephyr.elf
else
    echo "ERROR: neither zephyr/zephyr.exe nor zephyr/zephyr.elf in $BUILD_DIR (incomplete build?)" >&2
    exit 2
fi

echo "== allocator call sites in $BUILD_DIR"
mapfile -d '' OBJECTS < <(find . \( -name '*.obj' -o -name '*.o' \) -print0)
if [[ ${#OBJECTS[@]} -eq 0 ]]; then
    echo "ERROR: no object files under $BUILD_DIR" >&2
    exit 2
fi
# nm errors (unreadable object) abort through set -e; grep's "no match" is not an error.
ALL_SYMS="$(printf '%s\0' "${OBJECTS[@]}" | xargs -0 "$NM" -A)"
CALLERS="$(echo "$ALL_SYMS" | grep -E "$PATTERN" | sed 's|^\./||' || true)"
echo "  (${#OBJECTS[@]} objects scanned)"
if [[ -z "$CALLERS" ]]; then
    echo "  (none)"
else
    echo "$CALLERS" | sed 's/^/  /'
fi
BAD="$(echo "$CALLERS" | grep -vE "$ALLOWLIST" || true)"
if [[ -n "$BAD" ]]; then
    echo "FAIL: allocator referenced outside the kernel/libc allowlist:"
    echo "$BAD" | sed 's/^/  /'
    exit 1
fi

echo "== final image $IMAGE"
SYMS="$("$NM" "$IMAGE")"
if ! echo "$SYMS" | grep -qE ' [Tt] (main|z_cstart)$'; then
    echo "ERROR: $IMAGE has no main/z_cstart symbol; not a Zephyr image?" >&2
    exit 2
fi
status=0
for sym in $WRAPPED; do
    real_present=$(echo "$SYMS" | grep -cE " [TtWw] ${sym}$" || true)
    wrap_present=$(echo "$SYMS" | grep -cE " [TtWw] __wrap_${sym}$" || true)
    if [[ "$real_present" -eq 0 ]]; then
        echo "  $sym: not in the image (no reachable call path, dead-stripped)"
    elif [[ "$wrap_present" -gt 0 ]]; then
        echo "  $sym: present, guarded by __wrap_$sym (panics after start-up)"
    else
        echo "  $sym: PRESENT WITHOUT GUARD"
        status=1
    fi
done
if [[ $status -ne 0 ]]; then
    echo "FAIL: an allocator survived the link without its guard"
    exit 1
fi
echo "OK: no allocator call sites outside the allowlist; every surviving allocator is guarded"
