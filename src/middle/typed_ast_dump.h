#ifndef LANCE_TYPED_AST_DUMP_H
#define LANCE_TYPED_AST_DUMP_H

#include <stdio.h>

#include "typed_ast.h"

typedef enum {
	TYPED_AST_DUMP_TEXT,	 // Indented tree
	TYPED_AST_DUMP_DOT,		 // Graphviz: `dot -Tsvg ast.dot -o ast.svg`
	TYPED_AST_DUMP_PLANTUML, // PlantUML object diagram: `plantuml ast.puml`
} TypedAstDumpFormat;

// Picks a format from a file name: `.dot`/`.gv`, `.puml`/`.plantuml`, anything else is text.
TypedAstDumpFormat TypedAstDumpFormatFromPath(const char *path);

// Parses "text", "dot" or "plantuml". Returns false for any other name.
bool ParseTypedAstDumpFormat(const char *name, TypedAstDumpFormat *format);

// Writes every declaration of `module` to `out`. Declarations that come from
// imported modules (their names are qualified, as in `bool.if`) are only
// written when `includeImported` is set.
void DumpTypedModule(const TypedModule *module, FILE *out, TypedAstDumpFormat format, bool includeImported);

#endif // LANCE_TYPED_AST_DUMP_H
