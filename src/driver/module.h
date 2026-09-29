#ifndef LANCE_MODULE_H
#define LANCE_MODULE_H

#include <stdbool.h>

#include "arena.h"
#include "ast.h"
#include "vec.h"

typedef struct {
	const char *path;
	const char *source;
} SourceFile;

typedef struct {
	Arena astArena;
	VEC(AstModule *) modules;
	VEC(SourceFile) files;
	VEC(char *) searchPaths;
	bool hadError;
} Program;

bool LoadProgram(Program *program, const char *entryPath, const char *argv0);
void FreeProgram(Program *program);

#endif // LANCE_MODULE_H
