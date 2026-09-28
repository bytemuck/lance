cmake_minimum_required(VERSION 4.4)
project(lance C)

set(CMAKE_C_STANDARD 23)

add_executable(lance src/main.c
        src/file.c
        src/file.h
        src/token.c
        src/token.h
        src/lexer.c
        src/lexer.h
        src/parser.c
        src/parser.h
        src/ast.c
        src/ast.h
        src/type.c
        src/type.h
        src/symbol.c
        src/symbol.h
        src/typed_ast.c
        src/typed_ast.h
        src/compiler.c
        src/compiler.h
        src/value.c
        src/value.h
        src/interpreter.c
        src/interpreter.h
        src/memory.c
        src/memory.h
        src/string_pool.c
        src/string_pool.h
        src/table.c
        src/table.h
)
