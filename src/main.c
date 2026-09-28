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

#include <stdbool.h>
#include <string.h>

typedef struct {
    const char** paths;
    size_t count;
    size_t capacity;
} ModuleLoadState;

static char* CopyPath(const char* path) {
    const size_t length = strlen(path);
    char* copy = (char*)malloc(length + 1);
    if (copy) {
        memcpy(copy, path, length + 1);
    }
    return copy;
}

static bool PathExists(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

static char* JoinPath(const char* directory, const char* name) {
    const size_t directoryLength = strlen(directory);
    const size_t nameLength = strlen(name);
    const bool needsSeparator = directoryLength > 0 && directory[directoryLength - 1] != '/';
    char* path = (char*)malloc(directoryLength + (needsSeparator ? 1 : 0) + nameLength + 1);
    if (!path) return nullptr;

    memcpy(path, directory, directoryLength);
    size_t offset = directoryLength;
    if (needsSeparator) path[offset++] = '/';
    memcpy(path + offset, name, nameLength + 1);
    return path;
}

static char* DirectoryOf(const char* path) {
    const char* separator = strrchr(path, '/');
    if (!separator) return CopyPath(".");
    if (separator == path) return CopyPath("/");

    const size_t length = (size_t)(separator - path);
    char* directory = (char*)malloc(length + 1);
    if (directory) {
        memcpy(directory, path, length);
        directory[length] = '\0';
    }
    return directory;
}

static char* ResolveModulePath(const char* importingPath, const char* modulePath) {
    char* directory = DirectoryOf(importingPath);
    char* candidate = directory ? JoinPath(directory, modulePath) : nullptr;
    free(directory);
    if (candidate && PathExists(candidate)) return candidate;
    free(candidate);

    candidate = CopyPath(modulePath);
    if (candidate && PathExists(candidate)) return candidate;
    free(candidate);

    candidate = JoinPath("resources/std", modulePath);
    if (candidate && PathExists(candidate)) return candidate;
    free(candidate);
    return nullptr;
}

static bool ModuleWasLoaded(const ModuleLoadState* state, const char* path) {
    for (size_t i = 0; i < state->count; i++) {
        if (strcmp(state->paths[i], path) == 0) return true;
    }
    return false;
}

static bool RememberModule(ModuleLoadState* state, const char* path) {
    if (ModuleWasLoaded(state, path)) return true;
    if (state->count >= state->capacity) {
        const size_t newCapacity = state->capacity == 0 ? 8 : state->capacity * 2;
        const char** paths = (const char**)realloc((void*)state->paths, newCapacity * sizeof(const char*));
        if (!paths) return false;
        state->paths = paths;
        state->capacity = newCapacity;
    }

    state->paths[state->count++] = CopyPath(path);
    return state->paths[state->count - 1] != nullptr;
}

static bool AppendModule(AstModule* destination, AstModule* imported) {
    if (imported->count == 0) return true;
    AstDecl* declarations = (AstDecl*)realloc(destination->declarations,
                                               (destination->count + imported->count) * sizeof(AstDecl));
    if (!declarations) return false;

    memcpy(declarations + destination->count, imported->declarations,
           imported->count * sizeof(AstDecl));
    destination->declarations = declarations;
    destination->count += imported->count;
    free(imported->declarations);
    imported->declarations = nullptr;
    imported->count = 0;
    return true;
}

static bool LoadImports(AstModule* module, const char* modulePath, ModuleLoadState* state) {
    const size_t initialCount = module->count;
    for (size_t i = 0; i < initialCount; i++) {
        AstDecl* import = &module->declarations[i];
        if (import->kind != AST_DECL_IMPORT) continue;

        char* importedPath = ResolveModulePath(modulePath, import->modulePath);
        if (!importedPath) {
            fprintf(stderr, "Error: Could not resolve imported module '%s' from '%s'\n",
                    import->modulePath, modulePath);
            return false;
        }

        if (ModuleWasLoaded(state, importedPath)) {
            free(importedPath);
            continue;
        }

        if (!RememberModule(state, importedPath)) {
            free(importedPath);
            return false;
        }

        const char* source = ReadFile(importedPath);
        if (!source) {
            free(importedPath);
            return false;
        }

        Lexer lexer;
        InitializeLexer(&lexer, source);
        Parser parser;
        InitializeParser(&parser, &lexer);
        AstModule* imported = ParseModule(&parser);
        if (parser.hadError || !imported || !LoadImports(imported, importedPath, state) ||
            !AppendModule(module, imported)) {
            if (imported) FreeModuleAst(imported);
            free((void*)source);
            free(importedPath);
            return false;
        }

        free(imported);
        free((void*)source);
        free(importedPath);
    }
    return true;
}

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

    ModuleLoadState moduleState = {0};
    const bool importsLoaded = RememberModule(&moduleState, argv[1]) &&
                                !parser.hadError && LoadImports(astModule, argv[1], &moduleState);

    if (parser.hadError || !importsLoaded) {
        fprintf(stderr, "Parsing failed with errors.\n");
        FreeModuleAst(astModule);
        free((void*)source);
        FreeStringPool();
        return 1;
    }

    for (size_t i = 0; i < moduleState.count; i++) free((void*)moduleState.paths[i]);
    free((void*)moduleState.paths);

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
