#ifndef LANCE_MODULE_H
#define LANCE_MODULE_H

#include <stdbool.h>

#include "arena.h"
#include "ast.h"
#include "vec.h"

typedef struct {
    const char* path;   // Canonical, interned path
    const char* source; // Owned file contents
} SourceFile;

// A program is the entry file plus everything it imports, transitively. Each
// file is its own AstModule with its own namespace; `modules.items[0]` is the
// entry module.
typedef struct {
    Arena astArena; // Owns every AST node of every module
    VEC(AstModule*) modules;
    VEC(SourceFile) files; // Parallel to `modules`
    VEC(char*) searchPaths; // Directories searched for imports after the importer's own
    bool hadError;
} Program;

// Loads `entryPath` and its imports. Import search order:
//   1. the directory of the importing file
//   2. each directory in $LANCE_PATH (colon-separated)
//   3. `std/` next to the executable, then `../resources/std` next to it
//   4. `resources/std` relative to the working directory
// `import "util.lance"` makes the module's declarations visible both as
// `name` and as `util.name`. Imports are not re-exported.
// Returns false (and reports why) if any file fails to load or parse.
bool LoadProgram(Program* program, const char* entryPath, const char* argv0);
void FreeProgram(Program* program);

#endif // LANCE_MODULE_H
