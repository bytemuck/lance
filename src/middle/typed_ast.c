#include "typed_ast.h"

#include <stdlib.h>

static TypedExpr *AllocateTypedExprNode(TypedExprKind kind, LanceType *type, uint32_t line, uint32_t column) {
	TypedExpr *expr = (TypedExpr *) calloc(1, sizeof(TypedExpr));
	if (expr) {
		expr->kind	 = kind;
		expr->type	 = type;
		expr->line	 = line;
		expr->column = column;
	}
	return expr;
}

TypedExpr *CreateTypedIntLitExpr(int64_t val, LanceType *type, uint32_t line, uint32_t column) {
	TypedExpr *expr = AllocateTypedExprNode(TYPED_EXPR_INT_LIT, type, line, column);
	if (!expr)
		return nullptr;
	expr->intVal = val;
	return expr;
}

TypedExpr *CreateTypedFloatLitExpr(double val, LanceType *type, uint32_t line, uint32_t column) {
	TypedExpr *expr = AllocateTypedExprNode(TYPED_EXPR_FLOAT_LIT, type, line, column);
	if (!expr)
		return nullptr;
	expr->floatVal = val;
	return expr;
}

TypedExpr *CreateTypedBoolLitExpr(bool val, LanceType *type, uint32_t line, uint32_t column) {
	TypedExpr *expr = AllocateTypedExprNode(TYPED_EXPR_BOOL_LIT, type, line, column);
	if (!expr)
		return nullptr;
	expr->boolVal = val;
	return expr;
}

TypedExpr *CreateTypedStringLitExpr(const char *val, LanceType *type, uint32_t line, uint32_t column) {
	TypedExpr *expr = AllocateTypedExprNode(TYPED_EXPR_STRING_LIT, type, line, column);
	if (!expr)
		return nullptr;
	expr->stringVal = val;
	return expr;
}

TypedExpr *CreateTypedVarExpr(const char *name, const SlotRef slot, LanceType *type, uint32_t line, uint32_t column) {
	TypedExpr *expr = AllocateTypedExprNode(TYPED_EXPR_VAR, type, line, column);
	if (!expr)
		return nullptr;
	expr->var.name = name;
	expr->var.slot = slot;
	return expr;
}

TypedExpr *CreateTypedCallExpr(TypedExpr *callee, TypedExpr *argument, LanceType *type, uint32_t line,
							   uint32_t column) {
	TypedExpr *expr = AllocateTypedExprNode(TYPED_EXPR_CALL, type, line, column);
	if (!expr)
		return nullptr;
	expr->call.callee			= callee;
	expr->call.argument			= argument;
	const LanceType *calleeType = callee ? callee->type : nullptr;
	expr->call.lazyArgument		= calleeType && calleeType->kind == TYPE_FUNCTION && calleeType->function.lazyParam;
	return expr;
}

TypedExpr *CreateTypedLetExpr(size_t slot, TypedExpr *value, TypedExpr *body, uint32_t line, uint32_t column) {
	TypedExpr *expr = AllocateTypedExprNode(TYPED_EXPR_LET, body ? body->type : nullptr, line, column);
	if (!expr)
		return nullptr;
	expr->let.slot	= slot;
	expr->let.value = value;
	expr->let.body	= body;
	return expr;
}

TypedExpr *CreateTypedIfExpr(TypedExpr *condition, TypedExpr *thenBranch, TypedExpr *elseBranch, LanceType *type,
							 uint32_t line, uint32_t column) {
	TypedExpr *expr = AllocateTypedExprNode(TYPED_EXPR_IF, type, line, column);
	if (!expr)
		return nullptr;
	expr->ifExpr.condition	= condition;
	expr->ifExpr.thenBranch = thenBranch;
	expr->ifExpr.elseBranch = elseBranch;
	return expr;
}

TypedExpr *CreateTypedFieldAccessExpr(TypedExpr *target, const char *fieldName, size_t fieldIndex, LanceType *type,
									  uint32_t line, uint32_t column) {
	TypedExpr *expr = AllocateTypedExprNode(TYPED_EXPR_FIELD_ACCESS, type, line, column);
	if (!expr)
		return nullptr;
	expr->fieldAccess.target	 = target;
	expr->fieldAccess.fieldName	 = fieldName;
	expr->fieldAccess.fieldIndex = fieldIndex;
	return expr;
}

TypedExpr *CreateTypedStructInitExpr(LanceType *structType, TypedFieldValue *fields, size_t fieldCount, uint32_t line,
									 uint32_t column) {
	TypedExpr *expr = AllocateTypedExprNode(TYPED_EXPR_STRUCT_INIT, structType, line, column);
	if (!expr)
		return nullptr;
	expr->structInit.structType = structType;
	expr->structInit.fields		= fields;
	expr->structInit.fieldCount = fieldCount;
	return expr;
}

static TypedExpr *CopyTypedExpr(const TypedExpr *expr, const TypedSubstitution *substitution) {
	if (!expr)
		return nullptr;

	switch (expr->kind) {
		case TYPED_EXPR_INT_LIT:
			return CreateTypedIntLitExpr(expr->intVal, expr->type, expr->line, expr->column);
		case TYPED_EXPR_FLOAT_LIT:
			return CreateTypedFloatLitExpr(expr->floatVal, expr->type, expr->line, expr->column);
		case TYPED_EXPR_BOOL_LIT:
			return CreateTypedBoolLitExpr(expr->boolVal, expr->type, expr->line, expr->column);
		case TYPED_EXPR_STRING_LIT:
			return CreateTypedStringLitExpr(expr->stringVal, expr->type, expr->line, expr->column);

		case TYPED_EXPR_VAR: {
			SlotRef slot = expr->var.slot;
			if (substitution && slot.kind == SLOT_LOCAL) {
				if (slot.index < substitution->paramCount)
					return CopyTypedExpr(substitution->arguments[slot.index], nullptr);
				slot.index = substitution->localBase + slot.index - substitution->paramCount;
			}
			return CreateTypedVarExpr(expr->var.name, slot, expr->type, expr->line, expr->column);
		}

		case TYPED_EXPR_CALL: {
			TypedExpr *copy = CreateTypedCallExpr(CopyTypedExpr(expr->call.callee, substitution),
												  CopyTypedExpr(expr->call.argument, substitution), expr->type,
												  expr->line, expr->column);
			if (copy)
				copy->call.lazyArgument = expr->call.lazyArgument;
			return copy;
		}

		case TYPED_EXPR_LET: {
			size_t slot = expr->let.slot;
			if (substitution)
				slot = substitution->localBase + slot - substitution->paramCount;
			TypedExpr *copy = CreateTypedLetExpr(slot, CopyTypedExpr(expr->let.value, substitution),
												 CopyTypedExpr(expr->let.body, substitution), expr->line, expr->column);
			if (copy)
				copy->type = expr->type;
			return copy;
		}

		case TYPED_EXPR_IF:
			return CreateTypedIfExpr(CopyTypedExpr(expr->ifExpr.condition, substitution),
									 CopyTypedExpr(expr->ifExpr.thenBranch, substitution),
									 CopyTypedExpr(expr->ifExpr.elseBranch, substitution), expr->type, expr->line,
									 expr->column);

		case TYPED_EXPR_FIELD_ACCESS:
			return CreateTypedFieldAccessExpr(CopyTypedExpr(expr->fieldAccess.target, substitution),
											  expr->fieldAccess.fieldName, expr->fieldAccess.fieldIndex, expr->type,
											  expr->line, expr->column);

		case TYPED_EXPR_STRUCT_INIT: {
			const size_t	 count	= expr->structInit.fieldCount;
			TypedFieldValue *fields = count > 0 ? (TypedFieldValue *) calloc(count, sizeof(TypedFieldValue)) : nullptr;
			for (size_t i = 0; i < count; i++) {
				fields[i].name	= expr->structInit.fields[i].name;
				fields[i].value = CopyTypedExpr(expr->structInit.fields[i].value, substitution);
			}
			return CreateTypedStructInitExpr(expr->structInit.structType, fields, count, expr->line, expr->column);
		}
	}

	return nullptr;
}

TypedExpr *CloneTypedExpr(const TypedExpr *expr) { return CopyTypedExpr(expr, nullptr); }

TypedExpr *SubstituteTypedExpr(const TypedExpr *expr, const TypedSubstitution *substitution) {
	return CopyTypedExpr(expr, substitution);
}

size_t CountLocalSlotUses(const TypedExpr *expr, const size_t slot) {
	if (!expr)
		return 0;

	switch (expr->kind) {
		case TYPED_EXPR_INT_LIT:
		case TYPED_EXPR_FLOAT_LIT:
		case TYPED_EXPR_BOOL_LIT:
		case TYPED_EXPR_STRING_LIT:
			return 0;

		case TYPED_EXPR_VAR:
			return expr->var.slot.kind == SLOT_LOCAL && expr->var.slot.index == slot ? 1 : 0;

		case TYPED_EXPR_CALL:
			return CountLocalSlotUses(expr->call.callee, slot) + CountLocalSlotUses(expr->call.argument, slot);

		case TYPED_EXPR_LET:
			return CountLocalSlotUses(expr->let.value, slot) + CountLocalSlotUses(expr->let.body, slot);

		case TYPED_EXPR_IF:
			return CountLocalSlotUses(expr->ifExpr.condition, slot) +
				   CountLocalSlotUses(expr->ifExpr.thenBranch, slot) +
				   CountLocalSlotUses(expr->ifExpr.elseBranch, slot);

		case TYPED_EXPR_FIELD_ACCESS:
			return CountLocalSlotUses(expr->fieldAccess.target, slot);

		case TYPED_EXPR_STRUCT_INIT: {
			size_t uses = 0;
			for (size_t i = 0; i < expr->structInit.fieldCount; i++) {
				uses += CountLocalSlotUses(expr->structInit.fields[i].value, slot);
			}
			return uses;
		}
	}

	return 0;
}

void FreeTypedExpr(TypedExpr *expr) {
	if (!expr)
		return;

	switch (expr->kind) {
		case TYPED_EXPR_INT_LIT:
		case TYPED_EXPR_FLOAT_LIT:
		case TYPED_EXPR_BOOL_LIT:
		case TYPED_EXPR_STRING_LIT:
		case TYPED_EXPR_VAR:
			break;

		case TYPED_EXPR_CALL:
			FreeTypedExpr(expr->call.callee);
			FreeTypedExpr(expr->call.argument);
			break;

		case TYPED_EXPR_LET:
			FreeTypedExpr(expr->let.value);
			FreeTypedExpr(expr->let.body);
			break;

		case TYPED_EXPR_IF:
			FreeTypedExpr(expr->ifExpr.condition);
			FreeTypedExpr(expr->ifExpr.thenBranch);
			FreeTypedExpr(expr->ifExpr.elseBranch);
			break;

		case TYPED_EXPR_FIELD_ACCESS:
			FreeTypedExpr(expr->fieldAccess.target);
			break;

		case TYPED_EXPR_STRUCT_INIT:
			if (expr->structInit.fields) {
				for (size_t i = 0; i < expr->structInit.fieldCount; i++) {
					FreeTypedExpr(expr->structInit.fields[i].value);
				}
				free(expr->structInit.fields);
			}
			break;
	}

	free(expr);
}

void FreeTypedDecl(TypedDecl *decl) {
	if (!decl)
		return;
	if (decl->params) {
		free((void *) decl->params);
		decl->params = nullptr;
	}
	FreeTypedExpr(decl->body);
	decl->body = nullptr;
}

void FreeTypedModule(TypedModule *module) {
	if (!module)
		return;
	if (module->declarations) {
		for (size_t i = 0; i < module->count; i++) {
			FreeTypedDecl(&module->declarations[i]);
		}
		free(module->declarations);
	}
	free(module);
}
