#ifndef LANCE_TYPED_AST_H
#define LANCE_TYPED_AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "type.h"

typedef enum {
    TYPED_EXPR_INT_LIT,
    TYPED_EXPR_FLOAT_LIT,
    TYPED_EXPR_BOOL_LIT,
    TYPED_EXPR_STRING_LIT,
    TYPED_EXPR_VAR,
    TYPED_EXPR_CALL,
    TYPED_EXPR_LET,
    TYPED_EXPR_FIELD_ACCESS,
    TYPED_EXPR_STRUCT_INIT
} TypedExprKind;

// Where a variable's value lives at runtime. The compiler resolves every
// reference, so the interpreter indexes arrays instead of looking up names.
typedef enum {
    SLOT_LOCAL,  // Call frame: the parameters, then the `let` bindings
    SLOT_GLOBAL, // TypedModule.declarations
    SLOT_NATIVE, // Native functions, see NativeId in primitives.h
    SLOT_TYPE,   // A primitive type used as a value; has no storage
} SlotKind;

typedef struct {
    SlotKind kind;
    size_t index;
} SlotRef;

#define SLOT_REF(slotKind, slotIndex) ((SlotRef){ .kind = (slotKind), .index = (slotIndex) })

typedef struct TypedExpr TypedExpr;

typedef struct {
    const char* name;
    TypedExpr* value;
} TypedFieldValue;

struct TypedExpr {
    TypedExprKind kind;
    LanceType* type;
    uint32_t line;
    uint32_t column;
    union {
        int64_t intVal;
        double floatVal;
        bool boolVal;
        const char* stringVal;

        // Globals are created with an unresolved index and resolved by name
        // once the whole module is lowered (see ResolveGlobalSlots).
        struct {
            const char* name;
            SlotRef slot;
        } var;

        struct {
            TypedExpr* callee;
            TypedExpr* argument;
            // The callee's parameter is `lazy`: pass the argument unevaluated.
            // Derived from the callee's type by CreateTypedCallExpr.
            bool lazyArgument;
        } call;

        struct {
            size_t slot; // SLOT_LOCAL index the value is stored in
            TypedExpr* value;
            TypedExpr* body;
        } let;

        struct {
            TypedExpr* target;
            size_t fieldIndex;
            const char* fieldName;
        } fieldAccess;

        struct {
            LanceType* structType;
            TypedFieldValue* fields;
            size_t fieldCount;
        } structInit;
    };
};

typedef struct {
    const char* name;
    LanceType* type;
    const char** params;
    size_t paramCount;
    size_t frameSize; // Local slots needed by a call: the parameters plus every `let`
    TypedExpr* body;
    uint32_t line;
    uint32_t column;
} TypedDecl;

typedef struct {
    TypedDecl* declarations;
    size_t count;
    size_t capacity;
} TypedModule;

// Constructors
TypedExpr* CreateTypedIntLitExpr(int64_t val, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedFloatLitExpr(double val, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedBoolLitExpr(bool val, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedStringLitExpr(const char* val, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedVarExpr(const char* name, SlotRef slot, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedCallExpr(TypedExpr* callee, TypedExpr* argument, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedLetExpr(size_t slot, TypedExpr* value, TypedExpr* body, uint32_t line, uint32_t column);
TypedExpr* CreateTypedFieldAccessExpr(TypedExpr* target, const char* fieldName, size_t fieldIndex, LanceType* type, uint32_t line, uint32_t column);
TypedExpr* CreateTypedStructInitExpr(LanceType* structType, TypedFieldValue* fields, size_t fieldCount, uint32_t line, uint32_t column);

// Memory deallocation
void FreeTypedExpr(TypedExpr* expr);
void FreeTypedDecl(TypedDecl* decl);
void FreeTypedModule(TypedModule* module);

#endif // LANCE_TYPED_AST_H
