#include <stdio.h>
#include <string.h>

#include "compiler.h"
#include "diag.h"
#include "interpreter.h"
#include "module.h"
#include "string_pool.h"
#include "typed_ast.h"
#include "typed_ast_dump.h"

typedef struct {
	const char		  *sourcePath;
	const char		  *dumpPath; // nullptr: no dump; "-": standard output
	TypedAstDumpFormat dumpFormat;
	bool			   dumpFormatChosen;
	bool			   dumpImported;
	bool			   run;
} Options;

static void PrintUsage(FILE *out) {
	fprintf(out,
			"Usage: lance [options] <source-file>\n"
			"\n"
			"Options:\n"
			"  --dump-ast <file>      Write the typed AST to <file> ('-' for standard output). The format follows\n"
			"                         the extension: .dot/.gv (Graphviz), .puml/.plantuml (PlantUML), else text\n"
			"  --ast-format <format>  Force the format: text, dot or plantuml\n"
			"  --ast-all              Also dump declarations of imported modules such as the standard library\n"
			"  --no-run               Compile (and dump) only; do not run the program\n"
			"  -h, --help             Show this help\n");
}

// Accepts `--name value` and `--name=value`. Returns nullptr and reports when the value is missing.
static const char *OptionValue(const int argc, const char **argv, int *index, const char *name) {
	const size_t length = strlen(name);
	if (argv[*index][length] == '=')
		return argv[*index] + length + 1;
	if (*index + 1 < argc)
		return argv[++*index];

	fprintf(stderr, "Option '%s' needs a value.\n", name);
	return nullptr;
}

static bool IsOption(const char *arg, const char *name) {
	const size_t length = strlen(name);
	return strncmp(arg, name, length) == 0 && (arg[length] == '\0' || arg[length] == '=');
}

// Returns false when the command line is invalid or only asked for help; `exitCode` tells which.
static bool ParseOptions(const int argc, const char **argv, Options *options, int *exitCode) {
	*options = (Options) {.dumpFormat = TYPED_AST_DUMP_TEXT, .run = true};
	*exitCode = 1;

	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
			PrintUsage(stdout);
			*exitCode = 0;
			return false;
		} else if (IsOption(arg, "--dump-ast")) {
			options->dumpPath = OptionValue(argc, argv, &i, "--dump-ast");
			if (!options->dumpPath)
				return false;
		} else if (IsOption(arg, "--ast-format")) {
			const char *name = OptionValue(argc, argv, &i, "--ast-format");
			if (!name)
				return false;
			if (!ParseTypedAstDumpFormat(name, &options->dumpFormat)) {
				fprintf(stderr, "Unknown AST format '%s'; expected text, dot or plantuml.\n", name);
				return false;
			}
			options->dumpFormatChosen = true;
		} else if (strcmp(arg, "--ast-all") == 0) {
			options->dumpImported = true;
		} else if (strcmp(arg, "--no-run") == 0) {
			options->run = false;
		} else if (arg[0] == '-' && arg[1] != '\0') {
			fprintf(stderr, "Unknown option '%s'.\n", arg);
			PrintUsage(stderr);
			return false;
		} else if (options->sourcePath) {
			fprintf(stderr, "Only one source file can be given.\n");
			return false;
		} else {
			options->sourcePath = arg;
		}
	}

	if (!options->sourcePath) {
		PrintUsage(stderr);
		return false;
	}
	if (options->dumpPath && !options->dumpFormatChosen)
		options->dumpFormat = TypedAstDumpFormatFromPath(options->dumpPath);
	return true;
}

static bool WriteAstDump(const Options *options, const TypedModule *module) {
	const bool toStdout = strcmp(options->dumpPath, "-") == 0;
	FILE	  *out	   = toStdout ? stdout : fopen(options->dumpPath, "w");
	if (!out) {
		fprintf(stderr, "Cannot write '%s'.\n", options->dumpPath);
		return false;
	}

	DumpTypedModule(module, out, options->dumpFormat, options->dumpImported);
	if (toStdout)
		return fflush(out) == 0;
	return fclose(out) == 0;
}

static int Run(const Options *options, const char *argv0) {
	const char *path = options->sourcePath;
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

	if (options->dumpPath && !WriteAstDump(options, typedModule)) {
		FreeTypedModule(typedModule);
		FreeCompiler(&compiler);
		FreeProgram(&program);
		return 1;
	}
	if (!options->run) {
		FreeTypedModule(typedModule);
		FreeCompiler(&compiler);
		FreeProgram(&program);
		return 0;
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
	Options options;
	int		exitCode;
	if (!ParseOptions(argc, argv, &options, &exitCode))
		return exitCode;

	InitStringPool();
	const int status = Run(&options, argv[0]);
	FreeDiagnostics();
	FreeStringPool();
	return status;
}
