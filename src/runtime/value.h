#ifndef LANCE_VALUE_H
#define LANCE_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "type.h"
#include "typed_ast.h"

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_BOOL,
    VAL_STRING,
    VAL_UNIT,
    VAL_TYPE,
    VAL_STRUCT,
    VAL_CLOSURE,
    VAL_NATIVE_FN,
    VAL_THUNK,
} ValueKind;

typedef struct Value Value;
typedef struct Interpreter Interpreter;

typedef Value* (*NativeFn)(Interpreter* interpreter, size_t argc, Value** args);

typedef struct {
    const char* name;
    NativeFn fn;
    size_t arity;
    Value** appliedArgs;
    size_t appliedCount;
} NativeFunctionValue;

typedef struct {
    const char* name;
    Value* value;
} StructFieldValue;

typedef struct {
    const TypedDecl* decl;
    Value** appliedArgs;
    size_t appliedCount;
    size_t totalParams;
} ClosureValue;

// An argument passed to a `lazy` parameter: its expression plus a snapshot
// of the frame it was passed from, evaluated at most once. Copies of a thunk
// value share one Thunk, so the result is shared too.
typedef struct {
    size_t refCount;
    const TypedExpr* expr;
    Value** frame;     // Owned copies of the caller's slots; released once evaluated
    size_t frameCount;
    Value* value;      // The result, once evaluated
    bool evaluating;
} Thunk;

struct Value {
    ValueKind kind;

    union {
        int64_t intVal;
        double floatVal;
        bool boolVal;
        const char* stringVal;
        LanceType* typeVal;
        NativeFunctionValue nativeFn;

        struct {
            const char* typeName;
            StructFieldValue* fields;
            size_t fieldCount;
        } structVal;

        ClosureValue closure;
        Thunk* thunk;
    };
};

Value* MakeIntValue(int64_t value);
Value* MakeFloatValue(double value);
Value* MakeBoolValue(bool value);
Value* MakeStringValue(const char* value);
Value* MakeTypeValue(LanceType* type);
Value* MakeUnitValue(void);
Value* MakeNativeFnValue(const char* name, NativeFn fn, size_t arity, Value** appliedArgs, size_t appliedCount);
Value* MakeStructValue(const char* typeName, StructFieldValue* fields, size_t fieldCount);
Value* MakeClosureValue(const TypedDecl* decl, Value** appliedArgs, size_t appliedCount);
// Copies the `frameCount` values of `frame`.
Value* MakeThunkValue(const TypedExpr* expr, Value* const* frame, size_t frameCount);

// Stores the result of an evaluated thunk and releases its frame snapshot.
void SetThunkResult(Thunk* thunk, Value* result);

Value* CopyValue(const Value* value);
void PrintValue(const Value* value);
void FreeValue(Value* value);

#endif // LANCE_VALUE_H
