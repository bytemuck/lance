# Golden-file test runner for Lance.
#
# A test is a .lance file with expectations written as comments:
#   -- expect: <line>           the program prints <line> (in order) and exits 0
#   -- expect-error: <text>     the program exits non-zero and stderr contains <text>
#
# Usage: cmake -DLANCE=<exe> -DTEST_FILE=<file.lance> -P run_test.cmake

if (NOT LANCE OR NOT TEST_FILE)
    message(FATAL_ERROR "LANCE and TEST_FILE must be set")
endif ()

file(STRINGS "${TEST_FILE}" lines)
set(expected_stdout "")
set(expected_errors "")
foreach (line IN LISTS lines)
    if (line MATCHES "^-- expect: (.*)$")
        string(APPEND expected_stdout "${CMAKE_MATCH_1}\n")
    elseif (line MATCHES "^-- expect-error: (.*)$")
        list(APPEND expected_errors "${CMAKE_MATCH_1}")
    endif ()
endforeach ()

get_filename_component(test_dir "${TEST_FILE}" DIRECTORY)
execute_process(
        COMMAND "${LANCE}" "${TEST_FILE}"
        WORKING_DIRECTORY "${test_dir}"
        RESULT_VARIABLE exit_code
        OUTPUT_VARIABLE actual_stdout
        ERROR_VARIABLE actual_stderr)

if (expected_errors)
    if (exit_code EQUAL 0)
        message(FATAL_ERROR "Expected failure, but exited with 0.\nstdout:\n${actual_stdout}")
    endif ()
    if (actual_stderr MATCHES "Sanitizer")
        message(FATAL_ERROR "Sanitizer report:\n${actual_stderr}")
    endif ()
    foreach (err IN LISTS expected_errors)
        string(FIND "${actual_stderr}" "${err}" position)
        if (position EQUAL -1)
            message(FATAL_ERROR "Missing expected error '${err}'.\nstderr:\n${actual_stderr}")
        endif ()
    endforeach ()
    return()
endif ()

if (NOT exit_code EQUAL 0)
    message(FATAL_ERROR "Exited with ${exit_code}.\nstderr:\n${actual_stderr}")
endif ()
if (NOT actual_stdout STREQUAL expected_stdout)
    message(FATAL_ERROR "Output mismatch.\n--- expected\n${expected_stdout}--- actual\n${actual_stdout}--- stderr\n${actual_stderr}")
endif ()
