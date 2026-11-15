# Runs breakout_replay on one file and checks its exit code and all it
# printed. Called by ctest as
#
#   cmake -DPROGRAM=<breakout_replay> -DREPLAY=<replay file> -DEXIT_CODE=<n>
#         [-DEXPECTED_STDOUT=<file>] [-DEXPECTED_STDERR=<file>] -P run_replay.cmake
#
# A stream with no file given must be empty. Carriage returns are dropped from
# both sides, so a Windows console or a CRLF checkout makes no difference.

execute_process(
    COMMAND "${PROGRAM}" "${REPLAY}"
    RESULT_VARIABLE actual_exit
    OUTPUT_VARIABLE actual_stdout
    ERROR_VARIABLE actual_stderr)

set(failed FALSE)

if(NOT actual_exit STREQUAL EXIT_CODE)
    message(SEND_ERROR "exit code ${actual_exit}, expected ${EXIT_CODE}")
    set(failed TRUE)
endif()

foreach(stream stdout stderr)
    string(TOUPPER "${stream}" upper)
    set(expected "")
    if(DEFINED EXPECTED_${upper})
        file(READ "${EXPECTED_${upper}}" expected)
    endif()

    set(actual "${actual_${stream}}")
    string(REPLACE "\r" "" actual "${actual}")
    string(REPLACE "\r" "" expected "${expected}")

    if(NOT actual STREQUAL expected)
        message(SEND_ERROR "${stream} was:\n${actual}\nexpected:\n${expected}")
        set(failed TRUE)
    endif()
endforeach()

if(failed)
    message(FATAL_ERROR "breakout_replay ${REPLAY} did not do what was expected")
endif()
