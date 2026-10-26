# Shared compiler settings for the host unit tests of the firmware libraries.
# The library headers must compile cleanly under the strictest warnings used
# anywhere in the project; -Werror makes a regression a build failure.
function(can_ecu_host_test_settings target)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion
        -Wold-style-cast -Werror
    )
    if(CAN_ECU_COVERAGE)
        # -fkeep-inline-functions: an inline/constexpr function that is never called
        # at run time would otherwise not be emitted at all and vanish from both the
        # numerator and the denominator, inflating the function coverage figure.
        target_compile_options(${target} PRIVATE --coverage -O0 -g -fkeep-inline-functions)
        target_link_options(${target} PRIVATE --coverage)
    endif()
endfunction()
