find_package(GTest REQUIRED)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../host_test_flags.cmake)

add_executable(protocol_tests
    test_crc8.cpp
    test_codec_util.cpp
    test_e2e.cpp
    test_messages.cpp
)
target_include_directories(protocol_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(protocol_tests PRIVATE protocol GTest::gtest_main)
can_ecu_host_test_settings(protocol_tests)

include(GoogleTest)
gtest_discover_tests(protocol_tests)
