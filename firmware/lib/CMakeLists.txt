# Host build of the platform-independent libraries and their unit tests.
#
#   cmake -S firmware/lib -B build/host-tests -DCAN_ECU_COVERAGE=ON
#   cmake --build build/host-tests && ctest --test-dir build/host-tests
#
# The Zephyr application does not use this file; it adds the include directories
# directly (firmware/app/CMakeLists.txt) so the headers are compiled with the
# cross toolchain and Zephyr's own flags.
cmake_minimum_required(VERSION 3.20)
project(can_ecu_libs LANGUAGES CXX)

option(CAN_ECU_COVERAGE "Instrument the unit tests for gcov/gcovr" OFF)
set(CAN_ECU_BUILD_TESTS ON)
enable_testing()

add_subdirectory(protocol)
add_subdirectory(ecu_core)
