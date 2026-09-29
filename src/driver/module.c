#include "module.h"
#include "diag.h"
#include "file.h"
#include "lexer.h"
#include "memory.h"
#include "parser.h"
#include "string_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void AddSearchPath(Program *program, char *directory) {
	if (directory)
		VEC_PUSH(program->searchPaths, directory);
}

static void InitSearchPaths(Program *program, const char *argv0) {
	const char *lancePath = getenv("LANCE_PATH");
	if (lancePath && *lancePath) {
		char *paths = CopyString(lancePath);
		for (char *entry = strtok(paths, ":"); entry; entry = strtok(nullptr, ":")) {
			AddSearchPath(program, CopyString(entry));
		}
		free(paths);
	}

	char *executableDirectory = ExecutableDirectory(argv0);
	if (executableDirectory) {
		AddSearchPath(program, JoinPath(executableDirectory, "std"));
		AddSearchPath(program, JoinPath(executableDirectory, "../resources/std"));
		free(executableDirectory);
	}

	AddSearchPath(program, CopyString("resources/std"));
}

static const char *ResolveImport(const Program *program, const char *importerPath, const char *modulePath) {
	char *candidate = nullptr;

	char *importerDirectory = DirectoryOf(importerPath);
	candidate				= JoinPath(importerDirectory, modulePath);
	free(importerDirectory);

	for (size_t i = 0; !PathExists(candidate) && i < program->searchPaths.count; i++) {
		free(candidate);
		candidate = JoinPath(program->searchPaths.items[i], modulePath);
	}

	if (!PathExists(candidate)) {
		free(candidate);
		return nullptr;
	}

	char	   *canonical = CanonicalPath(candidate);
	const char *interned  = InternCString(canonical);
	free(canonical);
	free(candidate);
	return interned;
}

static AstModule *FindLoaded(const Program *program, const char *path) {
	for (size_t i = 0; i < program->files.count; i++) {
		if (program->files.items[i].path == path)
			return program->modules.items[i];
	}
	return nullptr;
}

static const char *ModuleBaseName(const char *modulePath) {
	const char *start = strrchr(modulePath, '/');
	start			  = start ? start + 1 : modulePath;
	const char *end	  = strrchr(start, '.');
	if (!end || end == start)
		end = start + strlen(start);
	return InternString(start, (uint32_t) (end - start));
}

static bool IsModuleNameTaken(const Program *program, const char *name) {
	for (size_t i = 0; i < program->modules.count; i++) {
		if (program->modules.items[i]->name == name)
			return true;
	}
	return false;
}

static const char *UniqueModuleName(const Program *program, const char *path) {
	const char *base = ModuleBaseName(path);
	const char *name = base;
	for (unsigned suffix = 2; IsModuleNameTaken(program, name); suffix++) {
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "%s~%u", base, suffix);
		name = InternCString(buffer);
	}
	return name;
}

static AstModule *LoadFile(Program *program, const char *path, const char *displayName);

static void LoadImports(Program *program, AstModule *module, const char *path) {
	VEC(AstImport) imports = {0};

	for (size_t i = 0; i < module->count; i++) {
		const AstDecl *decl = &module->declarations[i];
		if (decl->kind != AST_DECL_IMPORT || !decl->modulePath)
			continue;
		const SourceLoc loc = SOURCE_LOC(module->file, decl->line, decl->column);

		const char *importPath = ResolveImport(program, path, decl->modulePath);
		if (!importPath) {
			ReportError("Error", loc, "Could not resolve imported module '%s'", decl->modulePath);
			program->hadError = true;
			continue;
		}

		AstModule *target = FindLoaded(program, importPath);
		if (!target)
			target = LoadFile(program, importPath, importPath);
		if (!target || target == module)
			continue;

		const char *alias	  = ModuleBaseName(decl->modulePath);
		bool		duplicate = false;
		for (size_t j = 0; j < imports.count; j++) {
			if (imports.items[j].alias != alias)
				continue;
			duplicate = true;
			if (imports.items[j].module != target) {
				ReportError("Error", loc, "Module alias '%s' already refers to another module", alias);
				program->hadError = true;
			}
		}
		if (!duplicate)
			VEC_PUSH(imports, ((AstImport) {.alias = alias, .module = target}));
	}

	module->importCount = imports.count;
	module->imports		= ARENA_ARRAY(&program->astArena, AstImport, imports.count);
	if (imports.count)
		memcpy(module->imports, imports.items, imports.count * sizeof(AstImport));
	VEC_FREE(imports);
}

static AstModule *LoadFile(Program *program, const char *path, const char *displayName) {
	const char *source = ReadFile(path);
	if (!source) {
		program->hadError = true;
		return nullptr;
	}
	DiagRegisterSource(displayName, source);

	Lexer lexer;
	InitializeLexer(&lexer, source);
	Parser parser;
	InitializeParser(&parser, &lexer, displayName, &program->astArena);
	AstModule *module = ParseModule(&parser);
	if (parser.hadError)
		program->hadError = true;

	module->name = program->modules.count == 0 ? nullptr : UniqueModuleName(program, path);
	VEC_PUSH(program->modules, module);
	VEC_PUSH(program->files, ((SourceFile) {.path = path, .source = source}));

	LoadImports(program, module, path);
	return module;
}

bool LoadProgram(Program *program, const char *entryPath, const char *argv0) {
	*program = (Program) {0};
	InitArena(&program->astArena);
	InitSearchPaths(program, argv0);

	char	   *canonical = CanonicalPath(entryPath);
	const char *path	  = InternCString(canonical);
	free(canonical);

	LoadFile(program, path, InternCString(entryPath));
	return !program->hadError;
}

void FreeProgram(Program *program) {
	FreeArena(&program->astArena);
	VEC_FREE(program->modules);

	for (size_t i = 0; i < program->files.count; i++) {
		free((void *) program->files.items[i].source);
	}
	VEC_FREE(program->files);

	for (size_t i = 0; i < program->searchPaths.count; i++) {
		free(program->searchPaths.items[i]);
	}
	VEC_FREE(program->searchPaths);
}
