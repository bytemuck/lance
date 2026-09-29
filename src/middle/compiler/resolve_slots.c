#include "compiler_internal.h"
#include "string_pool.h"
#include "table.h"

// Global references are lowered before every global (and every generic
// specialization) has a declaration index, so they are fixed up here, by
// their program-wide name, once the whole program is lowered.

static void ResolveExpr(Compiler* compiler, const Table* globals, const TypedModule* module, TypedExpr* expr) {
    if (!expr) return;

    switch (expr->kind) {
        case TYPED_EXPR_VAR: {
            if (expr->var.slot.kind != SLOT_GLOBAL) return;
            const TypedDecl* decl = TableGet(globals, InternCString(expr->var.name));
            if (!decl) {
                CompilerError(compiler, expr->line, expr->column, "'%s' has no definition", expr->var.name);
                return;
            }
            expr->var.slot.index = (size_t)(decl - module->declarations);
            return;
        }

        case TYPED_EXPR_CALL:
            ResolveExpr(compiler, globals, module, expr->call.callee);
            ResolveExpr(compiler, globals, module, expr->call.argument);
            return;

        case TYPED_EXPR_LET:
            ResolveExpr(compiler, globals, module, expr->let.value);
            ResolveExpr(compiler, globals, module, expr->let.body);
            return;

        case TYPED_EXPR_FIELD_ACCESS:
            ResolveExpr(compiler, globals, module, expr->fieldAccess.target);
            return;

        case TYPED_EXPR_STRUCT_INIT:
            for (size_t i = 0; i < expr->structInit.fieldCount; i++) {
                ResolveExpr(compiler, globals, module, expr->structInit.fields[i].value);
            }
            return;

        case TYPED_EXPR_INT_LIT:
        case TYPED_EXPR_FLOAT_LIT:
        case TYPED_EXPR_BOOL_LIT:
        case TYPED_EXPR_STRING_LIT:
            return;
    }
}

void ResolveGlobalSlots(Compiler* compiler, TypedModule* module) {
    Table globals;
    TableInit(&globals);
    for (size_t i = 0; i < module->count; i++) {
        TableSet(&globals, InternCString(module->declarations[i].name), &module->declarations[i]);
    }

    for (size_t i = 0; i < module->count; i++) {
        ResolveExpr(compiler, &globals, module, module->declarations[i].body);
    }

    TableFree(&globals);
}
