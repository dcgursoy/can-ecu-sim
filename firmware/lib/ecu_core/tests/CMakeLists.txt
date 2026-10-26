find_package(GTest REQUIRED)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../host_test_flags.cmake)

add_executable(ecu_core_tests
    test_timeout_monitor.cpp
    test_protected_rx.cpp
    test_error_counter_model.cpp
    test_can_error_sm.cpp
    test_node_state.cpp
    test_liveness.cpp
)
target_link_libraries(ecu_core_tests PRIVATE ecu_core protocol GTest::gtest_main)
can_ecu_host_test_settings(ecu_core_tests)

include(GoogleTest)
gtest_discover_tests(ecu_core_tests)
