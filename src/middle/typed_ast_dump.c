#include "typed_ast_dump.h"

#include <string.h>

#define MAX_VALUE_LENGTH 40

typedef struct {
	const char	*label; // The edge from the parent, such as "condition"; nullptr for a root
	TypedExpr	*expr;
	bool		 lazy; // The argument is passed unevaluated
} ChildRef;

typedef struct {
	const char *kind;
	char		value[96]; // A literal, a name, a slot; may be empty
	char		extra[48]; // Additional remark, may be empty
	const char *color;	   // Graphviz fill color
} NodeInfo;

typedef struct {
	FILE  *out;
	size_t nextId;
} DumpContext;

static bool ChildAt(const TypedExpr *expr, const size_t index, ChildRef *child) {
	*child = (ChildRef) {};

	switch (expr->kind) {
		case TYPED_EXPR_CALL:
			if (index == 0) {
				*child = (ChildRef) {.label = "callee", .expr = expr->call.callee};
			} else if (index == 1) {
				*child = (ChildRef) {.label = expr->call.lazyArgument ? "lazy argument" : "argument",
									 .expr	= expr->call.argument,
									 .lazy	= expr->call.lazyArgument};
			} else {
				return false;
			}
			return true;

		case TYPED_EXPR_LET:
			if (index == 0) {
				*child = (ChildRef) {.label = "value", .expr = expr->let.value};
			} else if (index == 1) {
				*child = (ChildRef) {.label = "body", .expr = expr->let.body};
			} else {
				return false;
			}
			return true;

		case TYPED_EXPR_IF:
			if (index == 0) {
				*child = (ChildRef) {.label = "condition", .expr = expr->ifExpr.condition};
			} else if (index == 1) {
				*child = (ChildRef) {.label = "then", .expr = expr->ifExpr.thenBranch};
			} else if (index == 2) {
				*child = (ChildRef) {.label = "else", .expr = expr->ifExpr.elseBranch};
			} else {
				return false;
			}
			return true;

		case TYPED_EXPR_FIELD_ACCESS:
			if (index != 0)
				return false;
			*child = (ChildRef) {.label = "target", .expr = expr->fieldAccess.target};
			return true;

		case TYPED_EXPR_STRUCT_INIT:
			if (index >= expr->structInit.fieldCount)
				return false;
			*child = (ChildRef) {.label = expr->structInit.fields[index].name,
								 .expr	= expr->structInit.fields[index].value};
			return true;

		default:
			return false;
	}
}

// Copies `text` with C escapes, so that a value is always a single readable line.
static void FormatString(char *buffer, const size_t size, const char *text) {
	size_t length = 0;
	buffer[length++] = '"';
	for (size_t i = 0; text && text[i]; i++) {
		if (i >= MAX_VALUE_LENGTH) {
			for (int dot = 0; dot < 3 && length + 1 < size; dot++)
				buffer[length++] = '.';
			break;
		}
		const char *escape = nullptr;
		switch (text[i]) {
			case '\n':
				escape = "\\n";
				break;
			case '\t':
				escape = "\\t";
				break;
			case '\r':
				escape = "\\r";
				break;
			case '"':
				escape = "\\\"";
				break;
			case '\\':
				escape = "\\\\";
				break;
			default:
				break;
		}
		if (escape) {
			if (length + 3 >= size)
				break;
			buffer[length++] = escape[0];
			buffer[length++] = escape[1];
		} else {
			if (length + 2 >= size)
				break;
			buffer[length++] = text[i];
		}
	}
	buffer[length++] = '"';
	buffer[length]	 = '\0';
}

static const char *SlotKindName(const SlotKind kind) {
	switch (kind) {
		case SLOT_LOCAL:
			return "local";
		case SLOT_GLOBAL:
			return "global";
		case SLOT_NATIVE:
			return "native";
		case SLOT_TYPE:
			return "type";
	}
	return "?";
}

static NodeInfo Describe(const TypedExpr *expr) {
	NodeInfo info = {.kind = "?", .color = "white"};

	switch (expr->kind) {
		case TYPED_EXPR_INT_LIT:
			info.kind  = "Int";
			info.color = "lightyellow";
			snprintf(info.value, sizeof(info.value), "%lld", (long long) expr->intVal);
			break;
		case TYPED_EXPR_FLOAT_LIT:
			info.kind  = "Float";
			info.color = "lightyellow";
			snprintf(info.value, sizeof(info.value), "%g", expr->floatVal);
			break;
		case TYPED_EXPR_BOOL_LIT:
			info.kind  = "Bool";
			info.color = "lightyellow";
			snprintf(info.value, sizeof(info.value), "%s", expr->boolVal ? "true" : "false");
			break;
		case TYPED_EXPR_STRING_LIT:
			info.kind  = "String";
			info.color = "lightyellow";
			FormatString(info.value, sizeof(info.value), expr->stringVal);
			break;
		case TYPED_EXPR_VAR:
			info.kind  = "Var";
			info.color = "lightgrey";
			snprintf(info.value, sizeof(info.value), "%s", expr->var.name ? expr->var.name : "?");
			if (expr->var.slot.kind == SLOT_TYPE) {
				snprintf(info.extra, sizeof(info.extra), "slot: type");
			} else {
				snprintf(info.extra, sizeof(info.extra), "slot: %s #%zu", SlotKindName(expr->var.slot.kind),
						 expr->var.slot.index);
			}
			break;
		case TYPED_EXPR_CALL:
			info.kind  = "Call";
			info.color = "lightblue";
			break;
		case TYPED_EXPR_LET:
			info.kind  = "Let";
			info.color = "lightgreen";
			snprintf(info.value, sizeof(info.value), "slot #%zu", expr->let.slot);
			break;
		case TYPED_EXPR_IF:
			info.kind  = "If";
			info.color = "lightsalmon";
			break;
		case TYPED_EXPR_FIELD_ACCESS:
			info.kind  = "FieldAccess";
			info.color = "plum";
			snprintf(info.value, sizeof(info.value), ".%s (#%zu)", expr->fieldAccess.fieldName,
					 expr->fieldAccess.fieldIndex);
			break;
		case TYPED_EXPR_STRUCT_INIT:
			info.kind  = "StructInit";
			info.color = "plum";
			break;
	}
	return info;
}

// Writes `text` as part of a quoted label. The two graph formats each have their own special characters.
static void WriteEscaped(FILE *out, const TypedAstDumpFormat format, const char *text) {
	for (; *text; text++) {
		switch (*text) {
			case '"':
				fputs(format == TYPED_AST_DUMP_DOT ? "\\\"" : "'", out);
				break;
			case '\\':
				fputs(format == TYPED_AST_DUMP_DOT ? "\\\\" : "<U+005C>", out);
				break;
			case '\n':
				fputs(format == TYPED_AST_DUMP_DOT ? "\\n" : " ", out);
				break;
			default:
				fputc(*text, out);
				break;
		}
	}
}

static void WriteNodeTitle(FILE *out, const TypedAstDumpFormat format, const NodeInfo *info) {
	WriteEscaped(out, format, info->kind);
	if (info->value[0]) {
		fputc(' ', out);
		WriteEscaped(out, format, info->value);
	}
}

static void WriteIndent(FILE *out, const int depth) {
	for (int i = 0; i < depth; i++)
		fputs("  ", out);
}

// ----- Text -----

static void EmitText(FILE *out, const TypedExpr *expr, const char *label, const int depth) {
	if (!expr)
		return;

	const NodeInfo info = Describe(expr);
	WriteIndent(out, depth);
	if (label)
		fprintf(out, "%s: ", label);
	fprintf(out, "%s", info.kind);
	if (info.value[0])
		fprintf(out, " %s", info.value);
	fprintf(out, " : %s", TypeToString(expr->type));
	if (info.extra[0])
		fprintf(out, "  [%s]", info.extra);
	fputc('\n', out);

	ChildRef child;
	for (size_t i = 0; ChildAt(expr, i, &child); i++) {
		EmitText(out, child.expr, child.label, depth + 1);
	}
}

// ----- Graphviz -----

static size_t EmitDotNode(DumpContext *context, const TypedExpr *expr) {
	FILE		*out  = context->out;
	const size_t id	  = context->nextId++;
	const NodeInfo info = Describe(expr);

	const char *shape = "box";
	if (expr->kind == TYPED_EXPR_IF) {
		shape = "diamond";
	} else if (expr->kind == TYPED_EXPR_INT_LIT || expr->kind == TYPED_EXPR_FLOAT_LIT ||
			   expr->kind == TYPED_EXPR_BOOL_LIT || expr->kind == TYPED_EXPR_STRING_LIT ||
			   expr->kind == TYPED_EXPR_VAR) {
		shape = "ellipse";
	}

	fprintf(out, "    n%zu [shape=%s, style=filled, fillcolor=%s, label=\"", id, shape, info.color);
	WriteNodeTitle(out, TYPED_AST_DUMP_DOT, &info);
	fprintf(out, "\\n: ");
	WriteEscaped(out, TYPED_AST_DUMP_DOT, TypeToString(expr->type));
	if (info.extra[0]) {
		fprintf(out, "\\n");
		WriteEscaped(out, TYPED_AST_DUMP_DOT, info.extra);
	}
	fprintf(out, "\"];\n");

	ChildRef child;
	for (size_t i = 0; ChildAt(expr, i, &child); i++) {
		if (!child.expr)
			continue;
		const size_t childId = EmitDotNode(context, child.expr);
		fprintf(out, "    n%zu -> n%zu [label=\"", id, childId);
		WriteEscaped(out, TYPED_AST_DUMP_DOT, child.label);
		fprintf(out, "\"%s];\n", child.lazy ? ", style=dashed" : "");
	}
	return id;
}

static void EmitDotDecl(DumpContext *context, const TypedDecl *decl, const size_t index) {
	FILE *out = context->out;
	fprintf(out, "  subgraph cluster_%zu {\n    label=\"", index);
	WriteEscaped(out, TYPED_AST_DUMP_DOT, decl->name);
	fprintf(out, " :: ");
	WriteEscaped(out, TYPED_AST_DUMP_DOT, TypeToString(decl->type));
	fprintf(out, "\\nparams: ");
	for (size_t i = 0; i < decl->paramCount; i++) {
		fprintf(out, "%s", i > 0 ? ", " : "");
		WriteEscaped(out, TYPED_AST_DUMP_DOT, decl->params[i]);
	}
	fprintf(out, "%s\\nframe size: %zu\";\n    style=rounded;\n", decl->paramCount == 0 ? "-" : "", decl->frameSize);
	if (decl->body)
		EmitDotNode(context, decl->body);
	fprintf(out, "  }\n");
}

// ----- PlantUML -----

static size_t EmitPlantUmlNode(DumpContext *context, const TypedExpr *expr) {
	FILE		*out  = context->out;
	const size_t id	  = context->nextId++;
	const NodeInfo info = Describe(expr);

	fprintf(out, "  object \"");
	WriteNodeTitle(out, TYPED_AST_DUMP_PLANTUML, &info);
	fprintf(out, "\" as n%zu {\n    type = ", id);
	WriteEscaped(out, TYPED_AST_DUMP_PLANTUML, TypeToString(expr->type));
	fputc('\n', out);
	if (info.extra[0]) {
		fputs("    ", out);
		WriteEscaped(out, TYPED_AST_DUMP_PLANTUML, info.extra);
		fputc('\n', out);
	}
	fprintf(out, "  }\n");

	ChildRef child;
	for (size_t i = 0; ChildAt(expr, i, &child); i++) {
		if (!child.expr)
			continue;
		const size_t childId = EmitPlantUmlNode(context, child.expr);
		fprintf(out, "  n%zu %s n%zu : ", id, child.lazy ? "..>" : "-->", childId);
		WriteEscaped(out, TYPED_AST_DUMP_PLANTUML, child.label);
		fputc('\n', out);
	}
	return id;
}

static void EmitPlantUmlDecl(DumpContext *context, const TypedDecl *decl) {
	FILE *out = context->out;
	fprintf(out, "package \"");
	WriteEscaped(out, TYPED_AST_DUMP_PLANTUML, decl->name);
	fprintf(out, " :: ");
	WriteEscaped(out, TYPED_AST_DUMP_PLANTUML, TypeToString(decl->type));
	fprintf(out, " (frame size %zu)\" {\n", decl->frameSize);
	if (decl->body)
		EmitPlantUmlNode(context, decl->body);
	fprintf(out, "}\n\n");
}

// ----- Entry points -----

static bool HasSuffix(const char *path, const char *suffix) {
	const size_t pathLength	  = strlen(path);
	const size_t suffixLength = strlen(suffix);
	return pathLength >= suffixLength && strcmp(path + pathLength - suffixLength, suffix) == 0;
}

TypedAstDumpFormat TypedAstDumpFormatFromPath(const char *path) {
	if (HasSuffix(path, ".dot") || HasSuffix(path, ".gv"))
		return TYPED_AST_DUMP_DOT;
	if (HasSuffix(path, ".puml") || HasSuffix(path, ".plantuml"))
		return TYPED_AST_DUMP_PLANTUML;
	return TYPED_AST_DUMP_TEXT;
}

bool ParseTypedAstDumpFormat(const char *name, TypedAstDumpFormat *format) {
	if (strcmp(name, "text") == 0) {
		*format = TYPED_AST_DUMP_TEXT;
	} else if (strcmp(name, "dot") == 0) {
		*format = TYPED_AST_DUMP_DOT;
	} else if (strcmp(name, "plantuml") == 0 || strcmp(name, "puml") == 0) {
		*format = TYPED_AST_DUMP_PLANTUML;
	} else {
		return false;
	}
	return true;
}

static bool IsImported(const TypedDecl *decl) { return strchr(decl->name, '.') != nullptr; }

void DumpTypedModule(const TypedModule *module, FILE *out, const TypedAstDumpFormat format,
					 const bool includeImported) {
	DumpContext context = {.out = out, .nextId = 0};

	switch (format) {
		case TYPED_AST_DUMP_DOT:
			fprintf(out, "digraph TypedAst {\n  rankdir=TB;\n  node [fontname=\"Helvetica\"];\n"
						 "  edge [fontname=\"Helvetica\", fontsize=10];\n");
			break;
		case TYPED_AST_DUMP_PLANTUML:
			fprintf(out, "@startuml\nhide empty members\nskinparam shadowing false\n\n");
			break;
		case TYPED_AST_DUMP_TEXT:
			break;
	}

	for (size_t i = 0; module && i < module->count; i++) {
		const TypedDecl *decl = &module->declarations[i];
		if (!includeImported && IsImported(decl))
			continue;

		switch (format) {
			case TYPED_AST_DUMP_DOT:
				EmitDotDecl(&context, decl, i);
				break;
			case TYPED_AST_DUMP_PLANTUML:
				EmitPlantUmlDecl(&context, decl);
				break;
			case TYPED_AST_DUMP_TEXT:
				fprintf(out, "%s :: %s  (params: %zu, frame size: %zu)\n", decl->name, TypeToString(decl->type),
						decl->paramCount, decl->frameSize);
				EmitText(out, decl->body, nullptr, 1);
				fputc('\n', out);
				break;
		}
	}

	switch (format) {
		case TYPED_AST_DUMP_DOT:
			fprintf(out, "}\n");
			break;
		case TYPED_AST_DUMP_PLANTUML:
			fprintf(out, "@enduml\n");
			break;
		case TYPED_AST_DUMP_TEXT:
			break;
	}
}
