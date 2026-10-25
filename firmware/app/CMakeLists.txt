# Zephyr application: one codebase, two ECU roles (selected via Kconfig from
# Phase 2 on). Built as a freestanding application against the west workspace
# baked into the dev container (ZEPHYR_BASE).
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
)

# Platform-independent protocol library (header-only; generated from can/network.dbc).
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/protocol/include)
