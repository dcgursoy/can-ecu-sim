# Platform-independent protocol library (header-only, generated from the DBC).
# Built and tested on the host through firmware/lib/CMakeLists.txt.
add_library(protocol INTERFACE)
target_include_directories(protocol INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(protocol INTERFACE cxx_std_17)

if(CAN_ECU_BUILD_TESTS)
    add_subdirectory(tests)
endif()
