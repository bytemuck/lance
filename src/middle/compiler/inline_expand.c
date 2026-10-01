#include "compiler_internal.h"

#include <string.h>

#define NOT_BOUND ((size_t) -1)

const TypedDecl *FindTypedDecl(const TypedModule *module, const char *name) {
	if (!module || !name)
		return nullptr;

	for (size_t i = 0; i < module->count; i++) {
		if (strcmp(module->declarations[i].name, name) == 0)
			return &module->declarations[i];
	}
	return nullptr;
}

bool ValidateInlineBody(Compiler *compiler, const char *displayName, const TypedDecl *decl) {
	bool			 valid		= true;
	const LanceType *signature = decl->type;

	for (size_t p = 0; p < decl->paramCount && signature && signature->kind == TYPE_FUNCTION; p++) {
		// Call-by-need would need a thunk to share the value between the uses.
		if (signature->function.lazyParam && CountLocalSlotUses(decl->body, p) > 1) {
			CompilerError(compiler, decl->line, decl->column,
						  "Lazy parameter '%s' of inline function '%s' is used more than once", decl->params[p],
						  displayName);
			valid = false;
		}
		signature = signature->function.returnType;
	}
	return valid;
}

static bool IsLiteral(const TypedExpr *expr) {
	switch (expr->kind) {
		case TYPED_EXPR_INT_LIT:
		case TYPED_EXPR_FLOAT_LIT:
		case TYPED_EXPR_BOOL_LIT:
		case TYPED_EXPR_STRING_LIT:
			return true;
		default:
			return false;
	}
}

// The subexpression a body evaluates before anything else can happen.
static const TypedExpr *FirstEvaluated(const TypedExpr *expr) {
	switch (expr->kind) {
		case TYPED_EXPR_IF:
			return FirstEvaluated(expr->ifExpr.condition);
		case TYPED_EXPR_LET:
			return FirstEvaluated(expr->let.value);
		case TYPED_EXPR_FIELD_ACCESS:
			return FirstEvaluated(expr->fieldAccess.target);
		case TYPED_EXPR_CALL:
			if (expr->call.callee->kind == TYPED_EXPR_CALL)
				return FirstEvaluated(expr->call.callee);
			return expr->call.lazyArgument ? expr : FirstEvaluated(expr->call.argument);
		default:
			return expr;
	}
}

// The strict parameter whose argument can replace its only use without changing
// when, or in which order, the arguments are evaluated: the one the body evaluates first.
static size_t FindInlinableHead(const TypedDecl *decl, TypedExpr *const *args, const bool *lazy) {
	const TypedExpr *first = FirstEvaluated(decl->body);
	if (first->kind != TYPED_EXPR_VAR || first->var.slot.kind != SLOT_LOCAL ||
		first->var.slot.index >= decl->paramCount) {
		return NOT_BOUND;
	}

	const size_t head = first->var.slot.index;
	if (lazy[head] || CountLocalSlotUses(decl->body, head) != 1)
		return NOT_BOUND;

	// Arguments that stay bound with `let` run before the body, so they must come first.
	for (size_t p = head + 1; p < decl->paramCount; p++) {
		if (!lazy[p] && !IsLiteral(args[p]))
			return NOT_BOUND;
	}
	return head;
}

TypedExpr *ExpandInlineCall(Compiler *compiler, const TypedDecl *decl, TypedExpr **args, const uint32_t line,
							const uint32_t column) {
	const size_t paramCount = decl->paramCount;

	TypedExpr **replacements = ALLOCATE(TypedExpr *, paramCount);
	size_t	   *letSlots	 = ALLOCATE(size_t, paramCount);
	bool	   *lazy		 = ALLOCATE(bool, paramCount);

	const LanceType *signature = decl->type;
	for (size_t p = 0; p < paramCount; p++) {
		lazy[p]	  = signature && signature->kind == TYPE_FUNCTION && signature->function.lazyParam;
		signature = signature && signature->kind == TYPE_FUNCTION ? signature->function.returnType : nullptr;
	}

	// Lazy arguments and literals are substituted as they are. A lazy argument is
	// used at most once (see ValidateInlineBody), so it is still evaluated at most once.
	// Any other argument is bound by a `let` so that it is evaluated exactly once, in call order.
	const size_t head		= FindInlinableHead(decl, args, lazy);
	size_t		 nextSlot	= compiler->frameSize;
	for (size_t p = 0; p < paramCount; p++) {
		if (lazy[p] || IsLiteral(args[p]) || p == head) {
			replacements[p] = args[p];
			letSlots[p]		= NOT_BOUND;
		} else {
			letSlots[p]		= nextSlot++;
			replacements[p] = CreateTypedVarExpr(decl->params[p], SLOT_REF(SLOT_LOCAL, letSlots[p]), args[p]->type,
												 line, column);
		}
	}

	// The body's own `let` slots move past the caller's, which grows the caller's frame.
	const size_t localBase = nextSlot;
	nextSlot += decl->frameSize - paramCount;
	compiler->frameSize = nextSlot;

	const TypedSubstitution substitution = {
			.paramCount = paramCount,
			.arguments	= replacements,
			.localBase	= localBase,
	};
	TypedExpr *result = SubstituteTypedExpr(decl->body, &substitution);

	for (size_t p = paramCount; p > 0; p--) {
		if (letSlots[p - 1] == NOT_BOUND) {
			FreeTypedExpr(args[p - 1]);
		} else {
			FreeTypedExpr(replacements[p - 1]);
			result = CreateTypedLetExpr(letSlots[p - 1], args[p - 1], result, line, column);
		}
	}

	FREE_ARRAY(TypedExpr *, replacements, paramCount);
	FREE_ARRAY(size_t, letSlots, paramCount);
	FREE_ARRAY(bool, lazy, paramCount);
	return result;
}
