# Zephyr application: one codebase, two ECU roles (Kconfig choice APP_ROLE).
# Built as a freestanding application against the west workspace baked into the
# dev container (ZEPHYR_BASE).
cmake_minimum_required(VERSION 3.20.0)

# The dev image bakes firmware/west.yml into its west workspace at image build
# time, so an edited manifest changes nothing until the image is rebuilt. Make
# that mistake loud instead of silent.
if(DEFINED ENV{ZEPHYR_WORKSPACE} AND EXISTS "$ENV{ZEPHYR_WORKSPACE}/manifest/west.yml")
    file(READ "$ENV{ZEPHYR_WORKSPACE}/manifest/west.yml" baked_manifest)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/../west.yml" repo_manifest)
    if(NOT baked_manifest STREQUAL repo_manifest)
        message(FATAL_ERROR
            "firmware/west.yml differs from the manifest baked into the dev image; "
            "rebuild the image with: tools/dev.sh --build")
    endif()
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(can_ecu LANGUAGES C CXX)

target_sources(app PRIVATE
    src/main.cpp
    src/app/ecu.cpp
    src/app/test_control.cpp
    src/hal/can_bus.cpp
    src/hal/watchdog.cpp
    src/shell_cmds.c
    src/alloc_guard.c
)
target_sources_ifdef(CONFIG_APP_ROLE_SENSOR app PRIVATE src/app/sensor_role.cpp)
target_sources_ifdef(CONFIG_APP_ROLE_CONTROLLER app PRIVATE src/app/controller_role.cpp)

# No dynamic allocation after initialisation: route the public kernel and libc
# allocator entry points and the sys_heap leaf functions through
# src/alloc_guard.c, which panics once the application is up. The host C library
# is left alone on native_sim (the simulator runtime itself uses it). Symbols
# that are not compiled into the image are harmless to wrap (--gc-sections drops
# the unreferenced wrapper).
set(alloc_wrapped_symbols
    k_heap_alloc k_heap_aligned_alloc k_heap_calloc k_heap_realloc
    k_malloc k_calloc k_realloc k_aligned_alloc
    sys_heap_alloc sys_heap_aligned_alloc sys_heap_noalign_alloc sys_heap_realloc sys_heap_aligned_realloc)
if(NOT CONFIG_NATIVE_BUILD)
    list(APPEND alloc_wrapped_symbols malloc calloc realloc)
endif()
foreach(sym ${alloc_wrapped_symbols})
    zephyr_ld_options(-Wl,--wrap=${sym})
endforeach()

# Platform-independent libraries (header-only): the generated protocol codec and
# the ECU state machines, both unit tested on the host (firmware/lib).
target_include_directories(app PRIVATE
    src
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/protocol/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/ecu_core/include
)
