# Checks `lance --dump-ast` for every output format.
#
# Usage: cmake -DLANCE=<exe> -DSOURCE=<file.lance> -DOUT_DIR=<dir> -P run_dump_test.cmake

if (NOT LANCE OR NOT SOURCE OR NOT OUT_DIR)
    message(FATAL_ERROR "LANCE, SOURCE and OUT_DIR must be set")
endif ()

file(MAKE_DIRECTORY "${OUT_DIR}")
get_filename_component(source_dir "${SOURCE}" DIRECTORY)

# expect_dump(<file name> <extra options> <text that must appear>...)
function(expect_dump file_name options)
    set(path "${OUT_DIR}/${file_name}")
    file(REMOVE "${path}")
    execute_process(
            COMMAND "${LANCE}" --dump-ast "${path}" ${options} --no-run "${SOURCE}"
            WORKING_DIRECTORY "${source_dir}"
            RESULT_VARIABLE exit_code
            OUTPUT_VARIABLE actual_stdout
            ERROR_VARIABLE actual_stderr)
    if (NOT exit_code EQUAL 0)
        message(FATAL_ERROR "${file_name}: exited with ${exit_code}.\nstderr:\n${actual_stderr}")
    endif ()
    if (NOT actual_stdout STREQUAL "")
        message(FATAL_ERROR "${file_name}: --no-run must not run the program.\nstdout:\n${actual_stdout}")
    endif ()
    if (NOT EXISTS "${path}")
        message(FATAL_ERROR "${file_name}: no file was written")
    endif ()
    file(READ "${path}" content)
    foreach (needle IN LISTS ARGN)
        string(FIND "${content}" "${needle}" position)
        if (position EQUAL -1)
            message(FATAL_ERROR "${file_name}: missing '${needle}'.\n${content}")
        endif ()
    endforeach ()
endfunction()

expect_dump(ast.txt "" "sumTo :: i32 -> i32" "If : i32" "condition:" "then:" "else:")
expect_dump(ast.dot "" "digraph TypedAst" "shape=diamond" "label=\"condition\"" "sumTo :: i32 -> i32")
expect_dump(ast.puml "" "@startuml" "@enduml" "package \"sumTo" "object \"If\"" ": condition")
expect_dump(ast.out "--ast-format=dot" "digraph TypedAst")
expect_dump(ast_all.txt "--ast-all" "bool.and :: ")

# Imported declarations are left out unless --ast-all is given.
file(READ "${OUT_DIR}/ast.txt" content)
string(FIND "${content}" "bool.and" position)
if (NOT position EQUAL -1)
    message(FATAL_ERROR "ast.txt must not contain imported declarations")
endif ()

execute_process(
        COMMAND "${LANCE}" --ast-format=bogus "${SOURCE}"
        RESULT_VARIABLE exit_code
        ERROR_VARIABLE actual_stderr)
if (exit_code EQUAL 0 OR NOT actual_stderr MATCHES "Unknown AST format")
    message(FATAL_ERROR "An unknown format must be rejected.\nstderr:\n${actual_stderr}")
endif ()
