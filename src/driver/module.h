#ifndef LANCE_MODULE_H
#define LANCE_MODULE_H

#include <stdbool.h>

#include "ast.h"
#include "vec.h"

typedef struct {
    const char* path;   // Canonical, interned path
    const char* source; // Owned file contents
} SourceFile;

// A program is the entry file plus everything it imports, transitively.
// Declarations of all files are merged into one module (imports after the
// importing file), so every file currently shares one namespace.
typedef struct {
    AstModule* module;
    VEC(SourceFile) files;
    VEC(char*) searchPaths; // Directories searched for imports after the importer's own
    bool hadError;
} Program;

// Loads `entryPath` and its imports. Import search order:
//   1. the directory of the importing file
//   2. each directory in $LANCE_PATH (colon-separated)
//   3. `std/` next to the executable, then `../resources/std` next to it
//   4. `resources/std` relative to the working directory
// Returns false (and reports why) if any file fails to load or parse.
bool LoadProgram(Program* program, const char* entryPath, const char* argv0);
void FreeProgram(Program* program);

#endif // LANCE_MODULE_H
