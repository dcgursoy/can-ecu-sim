# Platform-independent ECU logic (header-only): message timeout monitoring,
# protected-message reception policy, CAN error-state handling with bus-off
# recovery, node state machine, thread liveness, simulated error counters.
# No Zephyr dependency, so it is unit tested on the host.
add_library(ecu_core INTERFACE)
target_include_directories(ecu_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ecu_core INTERFACE protocol)
target_compile_features(ecu_core INTERFACE cxx_std_17)

if(CAN_ECU_BUILD_TESTS)
    add_subdirectory(tests)
endif()
