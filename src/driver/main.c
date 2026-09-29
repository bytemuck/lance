#include <stdio.h>

#include "compiler.h"
#include "diag.h"
#include "interpreter.h"
#include "module.h"
#include "string_pool.h"
#include "typed_ast.h"

static int Run(const char *path, const char *argv0) {
	Program program;
	if (!LoadProgram(&program, path, argv0)) {
		fprintf(stderr, "Parsing failed with %zu error(s).\n", DiagErrorCount());
		FreeProgram(&program);
		return 1;
	}

	Compiler compiler;
	InitializeCompiler(&compiler);
	TypedModule *typedModule = CompileProgram(&compiler, program.modules.items, program.modules.count);
	if (!typedModule) {
		fprintf(stderr, "Compilation failed with %zu error(s).\n", DiagErrorCount());
		FreeCompiler(&compiler);
		FreeProgram(&program);
		return 1;
	}

	Interpreter interpreter;
	InitializeInterpreter(&interpreter);
	Value	  *result		= InterpretTypedModule(&interpreter, typedModule);
	const bool runtimeError = interpreter.hadError;

	if (result) {
		PrintValue(result);
		printf("\n");
		FreeValue(result);
	}

	FreeInterpreter(&interpreter);
	FreeTypedModule(typedModule);
	FreeCompiler(&compiler);
	FreeProgram(&program);
	return runtimeError ? 1 : 0;
}

int main(const int argc, const char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: lance <source-file>\n");
		return 1;
	}

	InitStringPool();
	const int status = Run(argv[1], argv[0]);
	FreeDiagnostics();
	FreeStringPool();
	return status;
}
