#include "interpreter.h"
#include "diag.h"
#include "memory.h"
#include "primitives.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

[[gnu::format(printf, 2, 3)]]
static void RuntimeError(Interpreter* interp, const char* format, ...) {
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    ReportError("Runtime Error", SOURCE_LOC(nullptr, 0, 0), "%s", message);
    interp->hadError = true;
}

// ---- Slots -------------------------------------------------------------------

static Slots CreateSlots(const size_t count) {
    Slots slots = { .values = nullptr, .count = count };
    if (count > 0) {
        slots.values = ALLOCATE(Value*, count);
        memset(slots.values, 0, sizeof(Value*) * count);
    }
    return slots;
}

// Stores `value` in the slot, releasing what it held before.
static void SetSlot(Slots* slots, const size_t index, Value* value) {
    FreeValue(slots->values[index]);
    slots->values[index] = value;
}

static void FreeSlots(Slots* slots) {
    for (size_t i = 0; i < slots->count; i++) FreeValue(slots->values[i]);
    if (slots->values) FREE_ARRAY(Value*, slots->values, slots->count);
    *slots = (Slots){0};
}

// ---- Native functions --------------------------------------------------------

// One implementation per primitive operator, generated from primitives.h.
// NUMERIC and INTEGRAL operators return their operand type, COMPARISON
// operators return bool. Integer division by zero is a runtime error.
#define DEFINE_PRIMITIVE_NUMERIC(ID, NAME, OP)                                              \
    static Value* Native##ID(Interpreter* interp, size_t argc, Value** args) {              \
        (void)argc;                                                                         \
        if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT) {                         \
            if ((PRIMITIVE_OP_##ID == PRIMITIVE_OP_Div || PRIMITIVE_OP_##ID == PRIMITIVE_OP_IntDiv) && \
                args[1]->intVal == 0) {                                                     \
                RuntimeError(interp, "Division by zero in '%s'", NAME);                     \
                return nullptr;                                                             \
            }                                                                               \
            return MakeIntValue(args[0]->intVal OP args[1]->intVal);                        \
        }                                                                                   \
        if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)                       \
            return MakeFloatValue(args[0]->floatVal OP args[1]->floatVal);                  \
        RuntimeError(interp, "Invalid operands for '%s'", NAME);                            \
        return nullptr;                                                                     \
    }

#define DEFINE_PRIMITIVE_INTEGRAL(ID, NAME, OP) DEFINE_PRIMITIVE_NUMERIC(ID, NAME, OP)

#define DEFINE_PRIMITIVE_COMPARISON(ID, NAME, OP)                                           \
    static Value* Native##ID(Interpreter* interp, size_t argc, Value** args) {              \
        (void)argc;                                                                         \
        if (args[0]->kind == VAL_INT && args[1]->kind == VAL_INT)                           \
            return MakeBoolValue(args[0]->intVal OP args[1]->intVal);                       \
        if (args[0]->kind == VAL_FLOAT && args[1]->kind == VAL_FLOAT)                       \
            return MakeBoolValue(args[0]->floatVal OP args[1]->floatVal);                   \
        if (args[0]->kind == VAL_BOOL && args[1]->kind == VAL_BOOL)                         \
            return MakeBoolValue(args[0]->boolVal OP args[1]->boolVal);                     \
        RuntimeError(interp, "Invalid operands for '%s'", NAME);                            \
        return nullptr;                                                                     \
    }

#define DEFINE_PRIMITIVE(ID, NAME, CLASS, OP) DEFINE_PRIMITIVE_##CLASS(ID, NAME, OP)
LANCE_PRIMITIVE_OPERATORS(DEFINE_PRIMITIVE)
#undef DEFINE_PRIMITIVE

static const NativeFn kPrimitiveImplementations[PRIMITIVE_OP_COUNT] = {
#define PRIMITIVE_IMPLEMENTATION(ID, NAME, CLASS, OP) [PRIMITIVE_OP_##ID] = Native##ID,
    LANCE_PRIMITIVE_OPERATORS(PRIMITIVE_IMPLEMENTATION)
#undef PRIMITIVE_IMPLEMENTATION
};

// ---- Evaluation --------------------------------------------------------------

// `frame` holds the SLOT_LOCAL values of the call being evaluated.
static Value* EvalTypedExpr(Interpreter* interp, const TypedExpr* expr, Slots* frame);

// The value of a `lazy` argument, evaluating it on first use. Other values
// are returned as they are. Returns nullptr on a runtime error.
static const Value* Force(Interpreter* interp, const Value* value) {
    if (!value || value->kind != VAL_THUNK) return value;

    Thunk* thunk = value->thunk;
    if (thunk->value) return thunk->value;
    if (thunk->evaluating) {
        RuntimeError(interp, "Lazy argument depends on itself");
        return nullptr;
    }

    thunk->evaluating = true;
    Slots frame = { .values = thunk->frame, .count = thunk->frameCount };
    Value* result = EvalTypedExpr(interp, thunk->expr, &frame);
    thunk->evaluating = false;
    if (!result) return nullptr;

    SetThunkResult(thunk, result);
    return result;
}

// The argument of a call to a `lazy` parameter. Values that are already
// known (literals, evaluated locals) are passed as they are, a local that is
// itself a lazy argument is passed on unevaluated, and anything else is
// delayed together with a snapshot of the frame.
static Value* DelayArgument(Interpreter* interp, const TypedExpr* arg, Slots* frame) {
    switch (arg->kind) {
        case TYPED_EXPR_INT_LIT:
        case TYPED_EXPR_FLOAT_LIT:
        case TYPED_EXPR_BOOL_LIT:
        case TYPED_EXPR_STRING_LIT:
            return EvalTypedExpr(interp, arg, frame);

        case TYPED_EXPR_VAR:
            if (arg->var.slot.kind == SLOT_LOCAL && frame->values[arg->var.slot.index]) {
                return CopyValue(frame->values[arg->var.slot.index]);
            }
            break;

        default:
            break;
    }
    return MakeThunkValue(arg, frame->values, frame->count);
}

static Value* NativePrint(Interpreter* interp, size_t argc, Value** args) {
    const Value* value = argc > 0 ? Force(interp, args[0]) : nullptr;
    if (!value) return interp->hadError ? nullptr : MakeUnitValue();

    if (value->kind == VAL_STRING) {
        printf("%s\n", value->stringVal);
    } else {
        PrintValue(value);
        printf("\n");
    }
    return MakeUnitValue();
}

// if# c a b: `a` and `b` arrive unevaluated; only the chosen one is forced.
static Value* NativeIf(Interpreter* interp, size_t argc, Value** args) {
    (void)argc;
    const Value* condition = Force(interp, args[0]);
    if (!condition) return nullptr;
    return CopyValue(Force(interp, condition->boolVal ? args[1] : args[2]));
}

// Evaluates `decl` with `args` bound to its parameters, in a fresh frame.
static Value* EvalDecl(Interpreter* interp, const TypedDecl* decl, Value** args) {
    Slots frame = CreateSlots(decl->frameSize);
    for (size_t i = 0; i < decl->paramCount; i++) {
        SetSlot(&frame, i, CopyValue(args[i]));
    }
    Value* result = EvalTypedExpr(interp, decl->body, &frame);
    FreeSlots(&frame);
    return result;
}

// Constants are evaluated on first use, so they may refer to globals declared
// later in the program.
static const Value* ForceGlobal(Interpreter* interp, const size_t index) {
    Globals* globals = &interp->globals;
    const TypedDecl* decl = &interp->module->declarations[index];

    switch (globals->states[index]) {
        case GLOBAL_EVALUATED:
            return globals->values.values[index];

        case GLOBAL_EVALUATING:
            RuntimeError(interp, "Definition of '%s' depends on itself", decl->name);
            return nullptr;

        case GLOBAL_UNEVALUATED:
            break;
    }

    globals->states[index] = GLOBAL_EVALUATING;
    Value* value = EvalDecl(interp, decl, nullptr);
    if (!value) return nullptr;

    globals->states[index] = GLOBAL_EVALUATED;
    SetSlot(&globals->values, index, value);
    return value;
}

static Value* LookupVariable(Interpreter* interp, const TypedExpr* expr, const Slots* frame) {
    const SlotRef slot = expr->var.slot;
    const Value* value = nullptr;

    switch (slot.kind) {
        case SLOT_LOCAL:  value = Force(interp, frame->values[slot.index]); break;
        case SLOT_GLOBAL: value = ForceGlobal(interp, slot.index); break;
        case SLOT_NATIVE: value = interp->natives.values[slot.index]; break;
        case SLOT_TYPE:   return MakeTypeValue(GetPrimitiveTypeByName(expr->var.name));
    }

    if (!value && !interp->hadError) {
        RuntimeError(interp, "Undefined variable '%s'", expr->var.name);
    }
    return CopyValue(value);
}

// Arguments applied so far, plus the new one. Callables are curried: they
// collect arguments until their arity is reached.
static Value** CollectArguments(Value** applied, const size_t appliedCount, const Value* arg) {
    Value** args = ALLOCATE(Value*, appliedCount + 1);
    for (size_t i = 0; i < appliedCount; i++) {
        args[i] = CopyValue(applied[i]);
    }
    args[appliedCount] = CopyValue(arg);
    return args;
}

static void FreeArguments(Value** args, const size_t count) {
    for (size_t i = 0; i < count; i++) FreeValue(args[i]);
    FREE_ARRAY(Value*, args, count);
}

static Value* ApplyFunction(Interpreter* interp, const Value* callee, const Value* arg) {
    Value* result = nullptr;

    if (callee->kind == VAL_NATIVE_FN) {
        const NativeFunctionValue* fn = &callee->nativeFn;
        const size_t count = fn->appliedCount + 1;
        Value** args = CollectArguments(fn->appliedArgs, fn->appliedCount, arg);

        result = count < fn->arity
            ? MakeNativeFnValue(fn->name, fn->fn, fn->arity, args, count)
            : fn->fn(interp, count, args);

        FreeArguments(args, count);
        return result;
    }

    if (callee->kind == VAL_CLOSURE) {
        const ClosureValue* closure = &callee->closure;
        const size_t count = closure->appliedCount + 1;
        Value** args = CollectArguments(closure->appliedArgs, closure->appliedCount, arg);

        result = count < closure->totalParams
            ? MakeClosureValue(closure->decl, args, count)
            : EvalDecl(interp, closure->decl, args);

        FreeArguments(args, count);
        return result;
    }

    RuntimeError(interp, "Attempted to call a non-function value");
    return nullptr;
}

static Value* EvalFieldAccess(Interpreter* interp, const TypedExpr* expr, Slots* frame) {
    Value* target = EvalTypedExpr(interp, expr->fieldAccess.target, frame);
    if (!target) return nullptr;

    Value* result = nullptr;
    if (target->kind != VAL_STRUCT) {
        RuntimeError(interp, "Field access '%s' on a non-struct value", expr->fieldAccess.fieldName);
    } else if (expr->fieldAccess.fieldIndex < target->structVal.fieldCount) {
        result = CopyValue(target->structVal.fields[expr->fieldAccess.fieldIndex].value);
    }

    FreeValue(target);
    return result;
}

static Value* EvalStructInit(Interpreter* interp, const TypedExpr* expr, Slots* frame) {
    const size_t count = expr->structInit.fieldCount;
    StructFieldValue* fields = ALLOCATE(StructFieldValue, count);

    for (size_t i = 0; i < count; i++) {
        fields[i].name = expr->structInit.fields[i].name;
        fields[i].value = EvalTypedExpr(interp, expr->structInit.fields[i].value, frame);
    }

    const LanceType* structType = expr->structInit.structType;
    const char* typeName = structType && structType->kind == TYPE_STRUCT ? structType->structType.name : nullptr;
    Value* result = MakeStructValue(typeName, fields, count);

    if (interp->hadError) {
        FreeValue(result);
        return nullptr;
    }
    return result;
}

static Value* EvalCall(Interpreter* interp, const TypedExpr* expr, Slots* frame) {
    Value* callee = EvalTypedExpr(interp, expr->call.callee, frame);
    Value* arg = !callee                 ? nullptr
               : expr->call.lazyArgument ? DelayArgument(interp, expr->call.argument, frame)
                                         : EvalTypedExpr(interp, expr->call.argument, frame);

    Value* result = callee && arg ? ApplyFunction(interp, callee, arg) : nullptr;

    FreeValue(callee);
    FreeValue(arg);
    return result;
}

static Value* EvalLet(Interpreter* interp, const TypedExpr* expr, Slots* frame) {
    Value* value = EvalTypedExpr(interp, expr->let.value, frame);
    if (!value) return nullptr;
    SetSlot(frame, expr->let.slot, value);
    return EvalTypedExpr(interp, expr->let.body, frame);
}

static Value* EvalTypedExpr(Interpreter* interp, const TypedExpr* expr, Slots* frame) {
    if (!expr || interp->hadError) return nullptr;

    switch (expr->kind) {
        case TYPED_EXPR_INT_LIT:      return MakeIntValue(expr->intVal);
        case TYPED_EXPR_FLOAT_LIT:    return MakeFloatValue(expr->floatVal);
        case TYPED_EXPR_BOOL_LIT:     return MakeBoolValue(expr->boolVal);
        case TYPED_EXPR_STRING_LIT:   return MakeStringValue(expr->stringVal);
        case TYPED_EXPR_VAR:          return LookupVariable(interp, expr, frame);
        case TYPED_EXPR_FIELD_ACCESS: return EvalFieldAccess(interp, expr, frame);
        case TYPED_EXPR_STRUCT_INIT:  return EvalStructInit(interp, expr, frame);
        case TYPED_EXPR_CALL:         return EvalCall(interp, expr, frame);
        case TYPED_EXPR_LET:          return EvalLet(interp, expr, frame);
    }

    return nullptr;
}

// ---- Entry points ------------------------------------------------------------

void InitializeInterpreter(Interpreter* interp) {
    *interp = (Interpreter){0};
}

void FreeInterpreter(Interpreter* interp) {
    FreeSlots(&interp->globals.values);
    if (interp->globals.states) FREE_ARRAY(GlobalState, interp->globals.states, interp->module->count);
    FreeSlots(&interp->natives);
    *interp = (Interpreter){0};
}

static void CreateNatives(Interpreter* interp) {
    interp->natives = CreateSlots(NATIVE_COUNT);
    for (size_t i = 0; i < PRIMITIVE_OP_COUNT; i++) {
        SetSlot(&interp->natives, i,
                MakeNativeFnValue(kPrimitiveOperators[i].name, kPrimitiveImplementations[i], 2, nullptr, 0));
    }
    SetSlot(&interp->natives, NATIVE_PRINT, MakeNativeFnValue(LANCE_PRINT_NAME, NativePrint, 1, nullptr, 0));
    SetSlot(&interp->natives, NATIVE_IF, MakeNativeFnValue(LANCE_IF_NAME, NativeIf, 3, nullptr, 0));
}

// Functions become closures right away; constants are evaluated lazily.
static void CreateGlobals(Interpreter* interp) {
    const TypedModule* module = interp->module;
    interp->globals.values = CreateSlots(module->count);
    interp->globals.states = ALLOCATE(GlobalState, module->count);

    for (size_t i = 0; i < module->count; i++) {
        const TypedDecl* decl = &module->declarations[i];
        if (decl->paramCount > 0) {
            SetSlot(&interp->globals.values, i, MakeClosureValue(decl, nullptr, 0));
            interp->globals.states[i] = GLOBAL_EVALUATED;
        } else {
            interp->globals.states[i] = GLOBAL_UNEVALUATED;
        }
    }
}

Value* InterpretTypedModule(Interpreter* interp, const TypedModule* module) {
    if (!module) return nullptr;

    interp->module = module;
    CreateNatives(interp);
    CreateGlobals(interp);

    // Force the constants in source order, so their effects (such as `print`)
    // happen in a predictable order.
    for (size_t i = 0; i < module->count && !interp->hadError; i++) {
        if (module->declarations[i].paramCount == 0) ForceGlobal(interp, i);
    }

    if (interp->hadError) return nullptr;

    for (size_t i = 0; i < module->count; i++) {
        if (strcmp(module->declarations[i].name, "main") == 0) {
            return CopyValue(interp->globals.values.values[i]);
        }
    }
    return nullptr;
}
