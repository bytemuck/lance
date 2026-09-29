#include "compiler_internal.h"
#include "string_pool.h"

#include <string.h>

static bool PrimitiveAcceptsOperand(const PrimitiveInfo *primitive, const LanceType *operand) {
	switch (primitive->class) {
		case PRIMITIVE_CLASS_NUMERIC:
			return IsNumericType(operand);
		case PRIMITIVE_CLASS_INTEGRAL:
			return IsIntegralType(operand);
		case PRIMITIVE_CLASS_COMPARISON:
			return IsNumericType(operand) || operand == GetTypeBool();
	}
	return false;
}

static LanceType *PrimitiveResultType(const PrimitiveInfo *primitive, LanceType *operand) {
	return primitive->class == PRIMITIVE_CLASS_COMPARISON ? GetTypeBool() : operand;
}

// True if `type` is `T -> T -> R` with T accepted by the primitive and R its
// result. Primitive operators evaluate both operands, so neither is lazy.
static bool IsPrimitiveOperatorType(const PrimitiveInfo *primitive, const LanceType *type) {
	if (!type || type->kind != TYPE_FUNCTION || type->function.lazyParam)
		return false;

	const LanceType *right = type->function.returnType;
	if (!right || right->kind != TYPE_FUNCTION || right->function.lazyParam)
		return false;

	LanceType *operand = type->function.paramType;
	return TypesAreEqual(operand, right->function.paramType) && PrimitiveAcceptsOperand(primitive, operand) &&
		   TypesAreEqual(right->function.returnType, PrimitiveResultType(primitive, operand));
}

// bool -> lazy T -> lazy T -> T
static LanceType *NewIfPrimitiveType(Compiler *compiler, LanceType *branchType) {
	LanceType *elseType = CreateLazyFunctionType(&compiler->types, branchType, branchType);
	LanceType *thenType = CreateLazyFunctionType(&compiler->types, branchType, elseType);
	return NewFunctionType(compiler, GetTypeBool(), thenType);
}

static bool IsIfPrimitiveType(const LanceType *type) {
	if (!type || type->kind != TYPE_FUNCTION || type->function.lazyParam ||
		!TypesAreEqual(type->function.paramType, GetTypeBool())) {
		return false;
	}
	const LanceType *thenType = type->function.returnType;
	if (!thenType || thenType->kind != TYPE_FUNCTION || !thenType->function.lazyParam)
		return false;

	const LanceType *branch	  = thenType->function.paramType;
	const LanceType *elseType = thenType->function.returnType;
	return elseType && elseType->kind == TYPE_FUNCTION && elseType->function.lazyParam &&
		   TypesAreEqual(elseType->function.paramType, branch) && TypesAreEqual(elseType->function.returnType, branch);
}

// `f a b c` parses as `((f a) b) c`; a spine is the flattened form f [a, b, c].
typedef struct {
	const AstExpr  *root;
	const AstExpr **args;
	size_t			argCount;
} CallSpine;

static CallSpine FlattenCall(const AstExpr *expr) {
	CallSpine	   spine   = {0};
	const AstExpr *current = expr;
	while (current->kind == AST_EXPR_CALL) {
		spine.argCount++;
		current = current->call.callee;
	}
	spine.root = current;
	spine.args = ALLOCATE(const AstExpr *, spine.argCount);

	current = expr;
	for (size_t i = spine.argCount; i > 0; i--) {
		spine.args[i - 1] = current->call.argument;
		current			  = current->call.callee;
	}
	return spine;
}

static void FreeCallSpine(CallSpine *spine) {
	FREE_ARRAY(const AstExpr *, spine->args, spine->argCount);
	spine->args = nullptr;
}

static bool CheckArgumentType(Compiler *compiler, const AstExpr *expr, const LanceType *expected,
							  const TypedExpr *actual) {
	if (TypesAreEqual(expected, actual->type))
		return true;
	CompilerError(compiler, expr->line, expr->column, "Argument type mismatch (expected '%s', got '%s')",
				  TypeToString(expected), TypeToString(actual->type));
	return false;
}

// Builds `(callee lhs) rhs` for a binary operator of type `T -> T -> R`.
static TypedExpr *BuildBinaryApplication(Compiler *compiler, const AstExpr *expr, TypedExpr *callee, TypedExpr *lhs,
										 TypedExpr *rhs, LanceType *operatorType) {
	if (!callee || !lhs || !rhs || !CheckArgumentType(compiler, expr, lhs->type, rhs)) {
		FreeTypedExpr(callee);
		FreeTypedExpr(lhs);
		FreeTypedExpr(rhs);
		return nullptr;
	}

	LanceType *partialType = operatorType->function.returnType;
	TypedExpr *partial	   = CreateTypedCallExpr(callee, lhs, partialType, expr->line, expr->column);
	return CreateTypedCallExpr(partial, rhs, partialType->function.returnType, expr->line, expr->column);
}

static bool IntegerFitsType(const int64_t value, const LanceType *type) {
	switch (type->kind) {
		case TYPE_I8:
			return value >= INT8_MIN && value <= INT8_MAX;
		case TYPE_I16:
			return value >= INT16_MIN && value <= INT16_MAX;
		case TYPE_I32:
			return value >= INT32_MIN && value <= INT32_MAX;
		case TYPE_U8:
			return value >= 0 && value <= UINT8_MAX;
		case TYPE_U16:
			return value >= 0 && value <= UINT16_MAX;
		case TYPE_U32:
			return value >= 0 && value <= UINT32_MAX;
		case TYPE_U64:
			return value >= 0;
		default:
			return true;
	}
}

static TypedExpr *LowerIntLit(Compiler *compiler, const AstExpr *expr, LanceType *expectedType) {
	LanceType *type = IsIntegralType(expectedType) ? expectedType : GetTypeI32();
	if (!IntegerFitsType(expr->intVal, type)) {
		CompilerError(compiler, expr->line, expr->column, "Integer literal %lld does not fit in '%s'",
					  (long long) expr->intVal, TypeToString(type));
		return nullptr;
	}
	return CreateTypedIntLitExpr(expr->intVal, type, expr->line, expr->column);
}

static TypedExpr *LowerFloatLit(const AstExpr *expr, LanceType *expectedType) {
	LanceType *type = IsFloatType(expectedType) ? expectedType : GetTypeF32();
	return CreateTypedFloatLitExpr(expr->floatVal, type, expr->line, expr->column);
}

static TypedExpr *LowerSymbolRef(Compiler *compiler, const AstExpr *expr, const Symbol *symbol) {
	switch (symbol->kind) {
		case SYMBOL_LOCAL:
			return CreateTypedVarExpr(symbol->name, SLOT_REF(SLOT_LOCAL, symbol->slot), symbol->type, expr->line,
									  expr->column);
		case SYMBOL_VALUE:
			return CreateTypedVarExpr(symbol->globalName, SLOT_REF(SLOT_GLOBAL, 0), symbol->type, expr->line,
									  expr->column);
		case SYMBOL_TYPE:
		case SYMBOL_TYPE_FUNCTION:
		case SYMBOL_BUILTIN:
			break;
	}
	CompilerError(compiler, expr->line, expr->column, "'%s' cannot be used as a value", symbol->name);
	return nullptr;
}

static TypedExpr *LowerIdent(Compiler *compiler, const AstExpr *expr, const SymbolTable *scope,
							 LanceType *expectedType) {
	const char *name = expr->identName;

	if (strcmp(name, LANCE_PRINT_NAME) == 0) {
		LanceType *printType = expectedType ? expectedType : NewFunctionType(compiler, GetTypeString(), GetTypeUnit());
		return CreateTypedVarExpr(InternCString(LANCE_PRINT_NAME), SLOT_REF(SLOT_NATIVE, NATIVE_PRINT), printType,
								  expr->line, expr->column);
	}

	if (strcmp(name, LANCE_IF_NAME) == 0) {
		if (!IsIfPrimitiveType(expectedType)) {
			CompilerError(compiler, expr->line, expr->column,
						  "Primitive '%s' needs a condition and both branches to know its type", name);
			return nullptr;
		}
		return CreateTypedVarExpr(InternCString(LANCE_IF_NAME), SLOT_REF(SLOT_NATIVE, NATIVE_IF), expectedType,
								  expr->line, expr->column);
	}

	const PrimitiveInfo *primitive = LookupPrimitiveOperator(name);
	if (primitive) {
		if (!IsPrimitiveOperatorType(primitive, expectedType)) {
			CompilerError(compiler, expr->line, expr->column,
						  "Primitive operator '%s' requires a concrete numeric type", name);
			return nullptr;
		}
		return CreateTypedVarExpr(InternCString(primitive->name), SLOT_REF(SLOT_NATIVE, primitive->op), expectedType,
								  expr->line, expr->column);
	}

	const size_t length = strlen(name);
	if (length > 0 && name[length - 1] == '#') {
		CompilerError(compiler, expr->line, expr->column, "Unknown primitive operator '%s'", name);
		return nullptr;
	}

	if (GetPrimitiveTypeByName(name)) {
		return CreateTypedVarExpr(name, SLOT_REF(SLOT_TYPE, 0), GetTypeType(), expr->line, expr->column);
	}

	const Symbol *symbol = SymbolTableLookup(scope, name);
	if (!symbol) {
		if (SymbolTableIsAmbiguous(scope, name)) {
			CompilerError(compiler, expr->line, expr->column,
						  "'%s' is declared by several imported modules; qualify it as module.%s", name, name);
		} else {
			CompilerError(compiler, expr->line, expr->column, "Undefined identifier '%s'", name);
		}
		return nullptr;
	}

	return LowerSymbolRef(compiler, expr, symbol);
}

static const SymbolTable *QualifyingModule(const AstExpr *expr, const SymbolTable *scope) {
	if (expr->kind != AST_EXPR_FIELD_ACCESS || expr->fieldAccess.target->kind != AST_EXPR_IDENT)
		return nullptr;
	return SymbolTableFindModule(scope, expr->fieldAccess.target->identName);
}

static const Symbol *LookupQualifiedName(Compiler *compiler, const AstExpr *expr, const SymbolTable *module) {
	const Symbol *symbol = SymbolTableLookupCurrentScope(module, expr->fieldAccess.fieldName);
	if (!symbol) {
		CompilerError(compiler, expr->line, expr->column, "Module '%s' has no declaration '%s'",
					  expr->fieldAccess.target->identName, expr->fieldAccess.fieldName);
	}
	return symbol;
}

static TypedExpr *LowerFieldAccess(Compiler *compiler, const AstExpr *expr, const SymbolTable *scope) {
	const SymbolTable *module = QualifyingModule(expr, scope);
	if (module) {
		const Symbol *symbol = LookupQualifiedName(compiler, expr, module);
		return symbol ? LowerSymbolRef(compiler, expr, symbol) : nullptr;
	}

	const char *fieldName = expr->fieldAccess.fieldName;
	TypedExpr  *target	  = LowerExpr(compiler, expr->fieldAccess.target, scope, nullptr);
	if (!target)
		return nullptr;

	if (!target->type || target->type->kind != TYPE_STRUCT) {
		CompilerError(compiler, expr->line, expr->column, "Field access '%s' on non-struct type '%s'", fieldName,
					  TypeToString(target->type));
		FreeTypedExpr(target);
		return nullptr;
	}

	for (size_t i = 0; i < target->type->structType.fieldCount; i++) {
		if (strcmp(target->type->structType.fields[i].name, fieldName) == 0) {
			LanceType *fieldType = target->type->structType.fields[i].type;
			return CreateTypedFieldAccessExpr(target, fieldName, i, fieldType, expr->line, expr->column);
		}
	}

	CompilerError(compiler, expr->line, expr->column, "Struct '%s' has no field '%s'", TypeToString(target->type),
				  fieldName);
	FreeTypedExpr(target);
	return nullptr;
}

static LanceType *LookupStructFieldType(const LanceType *structType, const char *fieldName) {
	for (size_t i = 0; i < structType->structType.fieldCount; i++) {
		if (strcmp(structType->structType.fields[i].name, fieldName) == 0) {
			return structType->structType.fields[i].type;
		}
	}
	return nullptr;
}

static TypedExpr *LowerStructValue(Compiler *compiler, const AstExpr *expr, const SymbolTable *scope,
								   LanceType *expectedType) {
	if (!expectedType || expectedType->kind != TYPE_STRUCT) {
		CompilerError(compiler, expr->line, expr->column, "Cannot determine struct type for literal");
		return nullptr;
	}

	const size_t	 count	= expr->structValue.fieldCount;
	TypedFieldValue *fields = ALLOCATE(TypedFieldValue, count);

	for (size_t i = 0; i < count; i++) {
		const char *fieldName = expr->structValue.fields[i].name;
		LanceType  *fieldType = LookupStructFieldType(expectedType, fieldName);
		if (!fieldType) {
			CompilerError(compiler, expr->line, expr->column, "Struct '%s' has no field '%s'",
						  TypeToString(expectedType), fieldName);
		}

		TypedExpr *value = LowerExpr(compiler, expr->structValue.fields[i].value, scope, fieldType);
		if (value && fieldType && !TypesAreEqual(value->type, fieldType)) {
			CompilerError(compiler, expr->line, expr->column, "Field '%s' type mismatch (expected '%s', got '%s')",
						  fieldName, TypeToString(fieldType), TypeToString(value->type));
		}

		fields[i] = (TypedFieldValue) {.name = fieldName, .value = value};
	}

	return CreateTypedStructInitExpr(expectedType, fields, count, expr->line, expr->column);
}

static TypedExpr *LowerPrintCall(Compiler *compiler, const AstExpr *expr, const SymbolTable *scope) {
	TypedExpr *arg = LowerExpr(compiler, expr->call.argument, scope, nullptr);
	if (!arg)
		return nullptr;

	LanceType *printType = NewFunctionType(compiler, arg->type, GetTypeUnit());
	TypedExpr *callee	 = CreateTypedVarExpr(InternCString(LANCE_PRINT_NAME), SLOT_REF(SLOT_NATIVE, NATIVE_PRINT),
											  printType, expr->call.callee->line, expr->call.callee->column);
	return CreateTypedCallExpr(callee, arg, GetTypeUnit(), expr->line, expr->column);
}

static TypedExpr *LowerPrimitiveBinary(Compiler *compiler, const AstExpr *expr, const CallSpine *spine,
									   const PrimitiveInfo *primitive, const SymbolTable *scope,
									   LanceType *expectedType) {
	LanceType *operandHint =
			primitive->class != PRIMITIVE_CLASS_COMPARISON && IsNumericType(expectedType) ? expectedType : nullptr;

	TypedExpr *lhs = LowerExpr(compiler, spine->args[0], scope, operandHint);
	if (!lhs)
		return nullptr;

	if (!PrimitiveAcceptsOperand(primitive, lhs->type)) {
		CompilerError(compiler, expr->line, expr->column, "Primitive operator '%s' does not support type '%s'",
					  primitive->name, TypeToString(lhs->type));
		FreeTypedExpr(lhs);
		return nullptr;
	}

	LanceType *operatorType = NewBinaryOperatorType(compiler, lhs->type, PrimitiveResultType(primitive, lhs->type));
	TypedExpr *callee		= LowerExpr(compiler, spine->root, scope, operatorType);
	TypedExpr *rhs			= LowerExpr(compiler, spine->args[1], scope, lhs->type);
	return BuildBinaryApplication(compiler, expr, callee, lhs, rhs, operatorType);
}

static TypedExpr *LowerPrimitiveIf(Compiler *compiler, const AstExpr *expr, const CallSpine *spine,
								   const SymbolTable *scope, LanceType *expectedType) {
	TypedExpr *condition  = LowerExpr(compiler, spine->args[0], scope, GetTypeBool());
	TypedExpr *thenBranch = LowerExpr(compiler, spine->args[1], scope, expectedType);
	TypedExpr *elseBranch = LowerExpr(compiler, spine->args[2], scope, thenBranch ? thenBranch->type : expectedType);
	if (!condition || !thenBranch || !elseBranch)
		goto invalid;

	if (!TypesAreEqual(condition->type, GetTypeBool())) {
		CompilerError(compiler, spine->args[0]->line, spine->args[0]->column,
					  "Condition of '%s' must be 'bool', got '%s'", LANCE_IF_NAME, TypeToString(condition->type));
		goto invalid;
	}
	if (!TypesAreEqual(thenBranch->type, elseBranch->type)) {
		CompilerError(compiler, expr->line, expr->column,
					  "Branches of '%s' must have the same type (got '%s' and '%s')", LANCE_IF_NAME,
					  TypeToString(thenBranch->type), TypeToString(elseBranch->type));
		goto invalid;
	}

	LanceType *ifType = NewIfPrimitiveType(compiler, thenBranch->type);
	TypedExpr *result = LowerExpr(compiler, spine->root, scope, ifType);
	TypedExpr *args[] = {condition, thenBranch, elseBranch};
	for (size_t i = 0; i < 3; i++) {
		result = CreateTypedCallExpr(result, args[i], result->type->function.returnType, expr->line, expr->column);
	}
	return result;

invalid:
	FreeTypedExpr(condition);
	FreeTypedExpr(thenBranch);
	FreeTypedExpr(elseBranch);
	return nullptr;
}

static LanceType *FinalReturnType(LanceType *type) {
	while (type && type->kind == TYPE_FUNCTION)
		type = type->function.returnType;
	return type;
}

static TypedExpr *LowerInterfaceBinary(Compiler *compiler, const AstExpr *expr, const CallSpine *spine,
									   const SymbolTable *scope, LanceType *expectedType, bool *handled) {
	const char *methodName = spine->root->identName;
	*handled			   = true;

	// When the method returns its operand type (e.g. `+`), the expected
	// result type is also the operand type, which types literals like `1.0`.
	LanceType *operandHint = nullptr;
	if (expectedType) {
		const InstanceMethodLookup probe = LookupInstanceMethodForType(compiler, expectedType, methodName);
		if (probe.methodType && TypesAreEqual(FinalReturnType(probe.methodType), expectedType)) {
			operandHint = expectedType;
		}
	}

	TypedExpr *lhs = LowerExpr(compiler, spine->args[0], scope, operandHint);
	if (!lhs)
		return nullptr;

	const InstanceMethodLookup lookup = LookupInstanceMethodForType(compiler, lhs->type, methodName);
	if (!lookup.methodExpr) {
		FreeTypedExpr(lhs);
		*handled = false;
		return nullptr;
	}

	LanceType *operatorType =
			lookup.methodType ? lookup.methodType : NewBinaryOperatorType(compiler, lhs->type, lhs->type);
	if (operatorType->kind != TYPE_FUNCTION || !operatorType->function.returnType ||
		operatorType->function.returnType->kind != TYPE_FUNCTION) {
		CompilerError(compiler, expr->line, expr->column, "Interface method '%s' is not a binary function", methodName);
		FreeTypedExpr(lhs);
		return nullptr;
	}

	// The method belongs to the instance's module, not to the caller's.
	const CompilerModule *callerModule = compiler->module;
	EnterModule(compiler, lookup.module);
	TypedExpr *callee = LowerExpr(compiler, lookup.methodExpr, compiler->globals, operatorType);
	EnterModule(compiler, callerModule);

	TypedExpr *rhs = LowerExpr(compiler, spine->args[1], scope, operatorType->function.returnType->function.paramType);
	return BuildBinaryApplication(compiler, expr, callee, lhs, rhs, operatorType);
}

static bool IsGeneric(const Symbol *symbol) { return symbol && symbol->kind == SYMBOL_VALUE && symbol->generic; }

static const AstType *StripLazy(const AstType *type) {
	return type && type->kind == AST_TYPE_LAZY ? type->lazy.inner : type;
}

// The type parameter a generic returns after `argCount` arguments, such as
// `T` in `bool -> lazy T -> lazy T -> T`, or null.
static const char *ResultTypeParam(const AstType *generic, const size_t argCount) {
	const AstType *type = generic->constrained.targetType;
	for (size_t i = 0; i < argCount && type && type->kind == AST_TYPE_FUNCTION; i++) {
		type = type->function.returnType;
	}
	if (!type || type->kind != AST_TYPE_NAMED)
		return nullptr;

	for (size_t i = 0; i < generic->constrained.constraintCount; i++) {
		if (strcmp(generic->constrained.constraints[i].typeParam, type->named.name) == 0)
			return type->named.name;
	}
	return nullptr;
}

// `square x` where `square :: (Numeric T) => T -> T`: lower the arguments,
// specialize for their types, and call the specialization.
static TypedExpr *LowerGenericCall(Compiler *compiler, const AstExpr *expr, const CallSpine *spine,
								   const Symbol *symbol, const SymbolTable *scope, LanceType *expectedType) {
	// Arguments typed by the result's type parameter get the expected type as a hint.
	const char	  *resultParam = expectedType ? ResultTypeParam(symbol->generic, spine->argCount) : nullptr;
	const AstType *paramTypes  = symbol->generic->constrained.targetType;

	TypedExpr **args	 = ALLOCATE(TypedExpr *, spine->argCount);
	bool		argError = false;
	for (size_t i = 0; i < spine->argCount; i++) {
		const AstType *paramType = paramTypes && paramTypes->kind == AST_TYPE_FUNCTION
										   ? StripLazy(paramTypes->function.paramType)
										   : nullptr;
		const bool	   hinted	 = resultParam && paramType && paramType->kind == AST_TYPE_NAMED &&
							strcmp(paramType->named.name, resultParam) == 0;
		args[i] = LowerExpr(compiler, spine->args[i], scope, hinted ? expectedType : nullptr);
		if (!args[i])
			argError = true;
		paramTypes = paramTypes && paramTypes->kind == AST_TYPE_FUNCTION ? paramTypes->function.returnType : nullptr;
	}

	const Symbol *specialization =
			argError ? nullptr
					 : SpecializeGenericFunction(compiler, symbol, args, spine->argCount, expr->line, expr->column);

	if (!specialization || !specialization->type) {
		for (size_t i = 0; i < spine->argCount; i++)
			FreeTypedExpr(args[i]);
		FREE_ARRAY(TypedExpr *, args, spine->argCount);
		return nullptr;
	}

	TypedExpr *result = CreateTypedVarExpr(specialization->globalName, SLOT_REF(SLOT_GLOBAL, 0), specialization->type,
										   spine->root->line, spine->root->column);
	LanceType *signature = specialization->type;
	size_t	   applied	 = 0;
	for (; applied < spine->argCount; applied++) {
		if (!signature || signature->kind != TYPE_FUNCTION) {
			CompilerError(compiler, expr->line, expr->column, "Too many arguments in call to '%s'", symbol->name);
			break;
		}
		if (!CheckArgumentType(compiler, spine->args[applied], signature->function.paramType, args[applied])) {
			break;
		}
		result = CreateTypedCallExpr(result, args[applied], signature->function.returnType, expr->line, expr->column);
		signature = signature->function.returnType;
	}

	if (applied < spine->argCount) {
		FreeTypedExpr(result);
		for (size_t i = applied; i < spine->argCount; i++)
			FreeTypedExpr(args[i]);
		result = nullptr;
	}

	FREE_ARRAY(TypedExpr *, args, spine->argCount);
	return result;
}

// Ordinary application `f a`: the callee's parameter type types the argument.
static TypedExpr *LowerPlainCall(Compiler *compiler, const AstExpr *expr, const SymbolTable *scope) {
	TypedExpr *callee = LowerExpr(compiler, expr->call.callee, scope, nullptr);
	if (!callee)
		return nullptr;

	if (!callee->type || callee->type->kind != TYPE_FUNCTION) {
		CompilerError(compiler, expr->line, expr->column, "Attempted to call non-function of type '%s'",
					  TypeToString(callee->type));
		FreeTypedExpr(callee);
		return nullptr;
	}

	LanceType *paramType = callee->type->function.paramType;
	TypedExpr *arg		 = LowerExpr(compiler, expr->call.argument, scope, paramType);
	if (!arg || !CheckArgumentType(compiler, expr, paramType, arg)) {
		FreeTypedExpr(callee);
		FreeTypedExpr(arg);
		return nullptr;
	}

	return CreateTypedCallExpr(callee, arg, callee->type->function.returnType, expr->line, expr->column);
}

static TypedExpr *LowerCall(Compiler *compiler, const AstExpr *expr, const SymbolTable *scope,
							LanceType *expectedType) {
	const AstExpr *callee = expr->call.callee;
	if (callee->kind == AST_EXPR_IDENT && strcmp(callee->identName, LANCE_PRINT_NAME) == 0) {
		return LowerPrintCall(compiler, expr, scope);
	}

	CallSpine  spine   = FlattenCall(expr);
	TypedExpr *result  = nullptr;
	bool	   handled = false;

	if (spine.root->kind == AST_EXPR_IDENT) {
		const char			*name	   = spine.root->identName;
		const PrimitiveInfo *primitive = LookupPrimitiveOperator(name);

		if (strcmp(name, LANCE_IF_NAME) == 0 && spine.argCount == 3) {
			result	= LowerPrimitiveIf(compiler, expr, &spine, scope, expectedType);
			handled = true;
		} else if (primitive && spine.argCount == 2) {
			result	= LowerPrimitiveBinary(compiler, expr, &spine, primitive, scope, expectedType);
			handled = true;
		} else if (spine.argCount == 2 && HasInstanceMethod(compiler, name)) {
			result = LowerInterfaceBinary(compiler, expr, &spine, scope, expectedType, &handled);
		}
	}

	if (!handled) {
		const SymbolTable *module = QualifyingModule(spine.root, scope);
		const Symbol	  *symbol = module ? SymbolTableLookupCurrentScope(module, spine.root->fieldAccess.fieldName)
									: spine.root->kind == AST_EXPR_IDENT ? SymbolTableLookup(scope, spine.root->identName)
																		 : nullptr;
		if (IsGeneric(symbol)) {
			result	= LowerGenericCall(compiler, expr, &spine, symbol, scope, expectedType);
			handled = true;
		}
	}

	FreeCallSpine(&spine);
	return handled ? result : LowerPlainCall(compiler, expr, scope);
}

// `let name = value in body`: the value gets the next free slot of the call frame.
static TypedExpr *LowerLet(Compiler *compiler, const AstExpr *expr, const SymbolTable *scope, LanceType *expectedType) {
	TypedExpr *value = LowerExpr(compiler, expr->let.value, scope, nullptr);
	if (!value)
		return nullptr;

	const size_t slot	  = compiler->frameSize++;
	SymbolTable *letScope = CreateSymbolTable((SymbolTable *) scope);
	SymbolTableInsertLocal(letScope, expr->let.name, value->type, slot);
	TypedExpr *body = LowerExpr(compiler, expr->let.body, letScope, expectedType);
	FreeSymbolTable(letScope);

	if (!body) {
		FreeTypedExpr(value);
		return nullptr;
	}
	return CreateTypedLetExpr(slot, value, body, expr->line, expr->column);
}

TypedExpr *LowerExpr(Compiler *compiler, const AstExpr *expr, const SymbolTable *scope, LanceType *expectedType) {
	if (!expr)
		return nullptr;

	switch (expr->kind) {
		case AST_EXPR_INT_LIT:
			return LowerIntLit(compiler, expr, expectedType);
		case AST_EXPR_FLOAT_LIT:
			return LowerFloatLit(expr, expectedType);
		case AST_EXPR_BOOL_LIT:
			return CreateTypedBoolLitExpr(expr->boolVal, GetTypeBool(), expr->line, expr->column);
		case AST_EXPR_STRING_LIT:
			return CreateTypedStringLitExpr(expr->stringVal, GetTypeString(), expr->line, expr->column);
		case AST_EXPR_IDENT:
			return LowerIdent(compiler, expr, scope, expectedType);
		case AST_EXPR_FIELD_ACCESS:
			return LowerFieldAccess(compiler, expr, scope);
		case AST_EXPR_STRUCT_VALUE:
			return LowerStructValue(compiler, expr, scope, expectedType);
		case AST_EXPR_CALL:
			return LowerCall(compiler, expr, scope, expectedType);
		case AST_EXPR_LET:
			return LowerLet(compiler, expr, scope, expectedType);
		case AST_EXPR_COMPTIME:
			return LowerExpr(compiler, expr->comptime.inner, scope, expectedType);

		case AST_EXPR_TYPE:
			CompilerError(compiler, expr->line, expr->column, "A type cannot be used as a value here");
			return nullptr;
	}

	return nullptr;
}

bool LowerBinding(Compiler *compiler, const char *name, const char *const *params, const size_t paramCount,
				  LanceType *signature, const AstExpr *body, const uint32_t line, const uint32_t column) {
	// Bindings nest when lowering a body specializes a generic function.
	const size_t outerFrameSize = compiler->frameSize;
	compiler->frameSize			= paramCount;

	SymbolTable *localScope	 = CreateSymbolTable(compiler->globals);
	LanceType	*bodyType	 = signature;
	TypedExpr	*loweredBody = nullptr;

	for (size_t p = 0; p < paramCount; p++) {
		if (!bodyType || bodyType->kind != TYPE_FUNCTION) {
			CompilerError(compiler, line, column, "Binding '%s' has more parameters than its type '%s' allows", name,
						  TypeToString(signature));
			goto failed;
		}
		SymbolTableInsertLocal(localScope, params[p], bodyType->function.paramType, p);
		bodyType = bodyType->function.returnType;
	}

	loweredBody = LowerExpr(compiler, body, localScope, bodyType);
	if (!loweredBody)
		goto failed;

	if (bodyType && !TypesAreEqual(loweredBody->type, bodyType)) {
		CompilerError(compiler, line, column, "In binding '%s', expected '%s' but got '%s'", name,
					  TypeToString(bodyType), TypeToString(loweredBody->type));
		goto failed;
	}

	const char **paramsCopy = nullptr;
	if (paramCount > 0) {
		paramsCopy = ALLOCATE(const char *, paramCount);
		for (size_t p = 0; p < paramCount; p++)
			paramsCopy[p] = params[p];
	}

	AppendTypedDecl(compiler->typedModule, (TypedDecl) {
												   .name	   = name,
												   .type	   = signature,
												   .params	   = paramsCopy,
												   .paramCount = paramCount,
												   .frameSize  = compiler->frameSize,
												   .body	   = loweredBody,
												   .line	   = line,
												   .column	   = column,
										   });
	FreeSymbolTable(localScope);
	compiler->frameSize = outerFrameSize;
	return true;

failed:
	FreeTypedExpr(loweredBody);
	FreeSymbolTable(localScope);
	compiler->frameSize = outerFrameSize;
	return false;
}
