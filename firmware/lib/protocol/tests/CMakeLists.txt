find_package(GTest REQUIRED)

add_executable(protocol_tests
    test_crc8.cpp
    test_codec_util.cpp
    test_e2e.cpp
    test_messages.cpp
)
target_include_directories(protocol_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(protocol_tests PRIVATE protocol GTest::gtest_main)
set_target_properties(protocol_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

# The generated header must compile cleanly under the strictest warnings we use
# anywhere; -Werror makes a regression in the generator a build failure.
target_compile_options(protocol_tests PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion
    -Wold-style-cast -Werror
)

if(PROTOCOL_COVERAGE)
    # -fkeep-inline-functions: an inline/constexpr function that is never called
    # at run time would otherwise not be emitted at all and vanish from both the
    # numerator and the denominator, inflating the function coverage figure.
    target_compile_options(protocol_tests PRIVATE --coverage -O0 -g -fkeep-inline-functions)
    target_link_options(protocol_tests PRIVATE --coverage)
endif()

include(GoogleTest)
gtest_discover_tests(protocol_tests)
