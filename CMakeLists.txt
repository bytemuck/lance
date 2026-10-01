cmake_minimum_required(VERSION 3.20)
project(lance C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LANCE_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(LANCE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# Every .c file below src/ is part of the interpreter. Adding a new source
# file only requires re-running CMake, not editing this list.
file(GLOB_RECURSE LANCE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")

add_executable(lance ${LANCE_SOURCES})

target_include_directories(lance PRIVATE
        src/support
        src/frontend
        src/middle
        src/middle/compiler
        src/runtime
        src/driver
)

target_compile_options(lance PRIVATE -Wall -Wextra -Wpedantic)
if (LANCE_WARNINGS_AS_ERRORS)
    target_compile_options(lance PRIVATE -Werror)
endif ()

if (LANCE_ENABLE_SANITIZERS)
    target_compile_options(lance PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(lance PRIVATE -fsanitize=address,undefined)
endif ()

# The standard library is looked up next to the executable at runtime.
add_custom_command(TARGET lance POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/resources/std"
                "$<TARGET_FILE_DIR:lance>/std")

enable_testing()
file(GLOB LANCE_TESTS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/cases/*.lance")
foreach (test_file ${LANCE_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_test(NAME ${test_name}
            COMMAND ${CMAKE_COMMAND}
                    -DLANCE=$<TARGET_FILE:lance>
                    -DTEST_FILE=${test_file}
                    -P ${CMAKE_SOURCE_DIR}/tests/run_test.cmake)
endforeach ()

add_test(NAME dump_ast
        COMMAND ${CMAKE_COMMAND}
                -DLANCE=$<TARGET_FILE:lance>
                -DSOURCE=${CMAKE_SOURCE_DIR}/tests/dump/sum_to.lance
                -DOUT_DIR=${CMAKE_BINARY_DIR}/dump_ast_test
                -P ${CMAKE_SOURCE_DIR}/tests/run_dump_test.cmake)
