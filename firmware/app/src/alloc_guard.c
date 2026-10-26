/*
 * "No dynamic allocation after initialisation", enforced.
 *
 * The allocator entry points are wrapped at link time (-Wl,--wrap, see
 * CMakeLists.txt). Until alloc_guard_arm() is called at the end of application
 * start-up every call is forwarded to the real function; afterwards any call is
 * a fatal error. Together with tools/check-no-heap.sh (which lists every object
 * that references an allocator) this turns the design rule into something a
 * test can catch: a code path that allocates at run time panics the first time
 * it executes instead of working until memory fragments.
 *
 * Why the wrappers are needed although the heaps are "off":
 * CONFIG_HEAP_MEM_POOL_SIZE=0 compiles out only the system pool and its users
 * (k_malloc, k_calloc, k_aligned_alloc, k_realloc in kernel/mempool.c), and
 * CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=0 makes malloc return NULL on the
 * Nucleo. The Zephyr heap API is unaffected by either: k_heap_alloc and
 * sys_heap_alloc allocate from whatever heap the caller passes, and a
 * subsystem's own K_HEAP_DEFINE'd heap (the shell's command history,
 * shell_history.c, disabled in prj.conf for exactly this reason) succeeds
 * normally. Wrapped here: the public kernel allocators, the k_heap API, the
 * sys_heap leaf functions they all end in, and on the Nucleo the C allocators
 * (which libstdc++'s operator new calls). The host C library is not wrapped on
 * native_sim: it belongs to the simulator runtime, so there an application
 * malloc is caught only by the static scan.
 *
 * Functions that are not compiled into the image (k_malloc with a zero pool)
 * are harmless to wrap: nothing references their __wrap_ and the linker's
 * --gc-sections drops the wrapper with its __real_ reference.
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/sys_heap.h>

#include <stddef.h>

LOG_MODULE_REGISTER(alloc_guard, LOG_LEVEL_ERR);

static bool armed;

void alloc_guard_arm(void)
{
	armed = true;
}

bool alloc_guard_armed(void)
{
	return armed;
}

static void trap(const char *what)
{
	LOG_ERR("dynamic allocation after init: %s", what);
	k_panic();
}

#define GUARDED(ret, name, params, args)                                                          \
	ret __real_##name params;                                                                 \
	ret __wrap_##name params                                                                  \
	{                                                                                         \
		if (armed) {                                                                      \
			trap(#name);                                                              \
		}                                                                                 \
		return __real_##name args;                                                        \
	}

/* --- Zephyr kernel heap API --- */
GUARDED(void *, k_heap_alloc, (struct k_heap *h, size_t bytes, k_timeout_t timeout), (h, bytes, timeout))
GUARDED(void *, k_heap_aligned_alloc, (struct k_heap *h, size_t align, size_t bytes, k_timeout_t timeout),
	(h, align, bytes, timeout))
GUARDED(void *, k_heap_calloc, (struct k_heap *h, size_t num, size_t size, k_timeout_t timeout),
	(h, num, size, timeout))
GUARDED(void *, k_heap_realloc, (struct k_heap *h, void *ptr, size_t bytes, k_timeout_t timeout),
	(h, ptr, bytes, timeout))
GUARDED(void *, k_malloc, (size_t size), (size))
GUARDED(void *, k_calloc, (size_t nmemb, size_t size), (nmemb, size))
GUARDED(void *, k_realloc, (void *ptr, size_t size), (ptr, size))
GUARDED(void *, k_aligned_alloc, (size_t align, size_t size), (align, size))

/* --- sys_heap leaf functions --- */
GUARDED(void *, sys_heap_alloc, (struct sys_heap *heap, size_t bytes), (heap, bytes))
GUARDED(void *, sys_heap_aligned_alloc, (struct sys_heap *heap, size_t align, size_t bytes),
	(heap, align, bytes))
GUARDED(void *, sys_heap_noalign_alloc, (struct sys_heap *heap, size_t align, size_t bytes),
	(heap, align, bytes))
GUARDED(void *, sys_heap_realloc, (struct sys_heap *heap, void *ptr, size_t bytes), (heap, ptr, bytes))
GUARDED(void *, sys_heap_aligned_realloc, (struct sys_heap *heap, void *ptr, size_t align, size_t bytes),
	(heap, ptr, align, bytes))

#ifndef CONFIG_NATIVE_BUILD
/* --- C library (picolibc on the Nucleo; the host libc is not wrapped) --- */
GUARDED(void *, malloc, (size_t size), (size))
GUARDED(void *, calloc, (size_t n, size_t size), (n, size))
GUARDED(void *, realloc, (void *p, size_t size), (p, size))
#endif
