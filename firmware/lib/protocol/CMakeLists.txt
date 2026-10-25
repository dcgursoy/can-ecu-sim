# Platform-independent protocol library (header-only) and its host unit tests.
#
#   cmake -S firmware/lib/protocol -B build/host-tests -DPROTOCOL_COVERAGE=ON
#   cmake --build build/host-tests && ctest --test-dir build/host-tests
#
# The Zephyr application does not use this file; it adds the include directory
# directly (firmware/app/CMakeLists.txt) so the headers are compiled with the
# cross toolchain and Zephyr's own flags.
cmake_minimum_required(VERSION 3.20)
project(can_ecu_protocol LANGUAGES CXX)

add_library(protocol INTERFACE)
target_include_directories(protocol INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(protocol INTERFACE cxx_std_17)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(PROTOCOL_COVERAGE "Instrument the unit tests for gcov/gcovr" OFF)
    enable_testing()
    add_subdirectory(tests)
endif()
