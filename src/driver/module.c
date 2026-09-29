#include "module.h"
#include "diag.h"
#include "file.h"
#include "lexer.h"
#include "memory.h"
#include "parser.h"
#include "string_pool.h"

#include <stdlib.h>
#include <string.h>

static void AddSearchPath(Program* program, char* directory) {
    if (directory) VEC_PUSH(program->searchPaths, directory);
}

static void InitSearchPaths(Program* program, const char* argv0) {
    const char* lancePath = getenv("LANCE_PATH");
    if (lancePath && *lancePath) {
        char* paths = CopyString(lancePath);
        for (char* entry = strtok(paths, ":"); entry; entry = strtok(nullptr, ":")) {
            AddSearchPath(program, CopyString(entry));
        }
        free(paths);
    }

    char* executableDirectory = ExecutableDirectory(argv0);
    if (executableDirectory) {
        AddSearchPath(program, JoinPath(executableDirectory, "std"));
        AddSearchPath(program, JoinPath(executableDirectory, "../resources/std"));
        free(executableDirectory);
    }

    AddSearchPath(program, CopyString("resources/std"));
}

// Returns the canonical, interned path of `modulePath` imported from `importerPath`.
static const char* ResolveImport(const Program* program, const char* importerPath, const char* modulePath) {
    char* candidate = nullptr;

    char* importerDirectory = DirectoryOf(importerPath);
    candidate = JoinPath(importerDirectory, modulePath);
    free(importerDirectory);

    for (size_t i = 0; !PathExists(candidate) && i < program->searchPaths.count; i++) {
        free(candidate);
        candidate = JoinPath(program->searchPaths.items[i], modulePath);
    }

    if (!PathExists(candidate)) {
        free(candidate);
        return nullptr;
    }

    char* canonical = CanonicalPath(candidate);
    const char* interned = InternCString(canonical);
    free(canonical);
    free(candidate);
    return interned;
}

static bool IsLoaded(const Program* program, const char* path) {
    for (size_t i = 0; i < program->files.count; i++) {
        if (program->files.items[i].path == path) return true;
    }
    return false;
}

static void AppendDeclarations(AstModule* destination, AstModule* source) {
    if (source->count > 0) {
        destination->declarations = GROW_ARRAY(AstDecl, destination->declarations,
                                               destination->count, destination->count + source->count);
        memcpy(destination->declarations + destination->count, source->declarations,
               source->count * sizeof(AstDecl));
        destination->count += source->count;
    }
    free(source->declarations);
    free(source);
}

// Parses `path` (canonical, interned) and, recursively, what it imports.
static void LoadFile(Program* program, const char* path, const char* displayName) {
    const char* source = ReadFile(path);
    if (!source) {
        program->hadError = true;
        return;
    }

    VEC_PUSH(program->files, ((SourceFile){ .path = path, .source = source }));
    DiagRegisterSource(displayName, source);

    Lexer lexer;
    InitializeLexer(&lexer, source);
    Parser parser;
    InitializeParser(&parser, &lexer, displayName);
    AstModule* parsed = ParseModule(&parser);
    if (parser.hadError) program->hadError = true;

    const size_t ownStart = program->module->count;
    AppendDeclarations(program->module, parsed);
    const size_t ownEnd = program->module->count;

    // Loading an import appends to (and may move) the declaration array, so
    // this file's declarations are visited by index.
    for (size_t i = ownStart; i < ownEnd; i++) {
        const AstDecl* decl = &program->module->declarations[i];
        if (decl->kind != AST_DECL_IMPORT || !decl->modulePath) continue;

        const char* importPath = ResolveImport(program, path, decl->modulePath);
        if (!importPath) {
            ReportError("Error", SOURCE_LOC(displayName, decl->line, decl->column),
                        "Could not resolve imported module '%s'", decl->modulePath);
            program->hadError = true;
            continue;
        }

        if (!IsLoaded(program, importPath)) {
            LoadFile(program, importPath, importPath);
        }
    }
}

bool LoadProgram(Program* program, const char* entryPath, const char* argv0) {
    *program = (Program){0};
    program->module = calloc(1, sizeof(AstModule));
    InitSearchPaths(program, argv0);

    // The entry file keeps the name it was given for diagnostics; imports are
    // reported by their canonical path.
    char* canonical = CanonicalPath(entryPath);
    const char* path = InternCString(canonical);
    free(canonical);

    LoadFile(program, path, InternCString(entryPath));
    return !program->hadError;
}

void FreeProgram(Program* program) {
    FreeModuleAst(program->module);
    program->module = nullptr;

    for (size_t i = 0; i < program->files.count; i++) {
        free((void*)program->files.items[i].source);
    }
    VEC_FREE(program->files);

    for (size_t i = 0; i < program->searchPaths.count; i++) {
        free(program->searchPaths.items[i]);
    }
    VEC_FREE(program->searchPaths);
}
