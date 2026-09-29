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

Value* CopyValue(const Value* value);
void PrintValue(const Value* value);
void FreeValue(Value* value);

#endif // LANCE_VALUE_H
