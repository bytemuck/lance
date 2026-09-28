#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "compiler.h"
#include "file.h"
#include "interpreter.h"
#include "lexer.h"
#include "parser.h"
#include "string_pool.h"
#include "typed_ast.h"

int main(const int argc, const char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: lance <source-file>\n");
        return 1;
    }

    InitStringPool();

    const char* source = ReadFile(argv[1]);
    if (source == nullptr) {
        fprintf(stderr, "Error: Could not read file: '%s'\n", argv[1]);
        FreeStringPool();
        return 1;
    }

    Lexer lexer;
    InitializeLexer(&lexer, source);

    Parser parser;
    InitializeParser(&parser, &lexer);

    AstModule* astModule = ParseModule(&parser);

    if (parser.hadError) {
        fprintf(stderr, "Parsing failed with errors.\n");
        FreeModuleAst(astModule);
        free((void*)source);
        FreeStringPool();
        return 1;
    }

    Compiler compiler;
    InitializeCompiler(&compiler);

    TypedModule* typedModule = CompileModule(&compiler, astModule);

    if (!typedModule || compiler.hadError) {
        fprintf(stderr, "Compilation failed with errors.\n");
        FreeCompiler(&compiler);
        FreeModuleAst(astModule);
        free((void*)source);
        FreeStringPool();
        return 1;
    }

    Interpreter interpreter;
    InitializeInterpreter(&interpreter);

    Value* result = InterpretTypedModule(&interpreter, typedModule);

    if (result) {
        PrintValue(result);
        printf("\n");
        FreeValue(result);
    }

    FreeInterpreter(&interpreter);
    FreeCompiler(&compiler);
    FreeTypedModule(typedModule);
    FreeModuleAst(astModule);
    free((void*)source);
    FreeStringPool();

    return 0;
}
