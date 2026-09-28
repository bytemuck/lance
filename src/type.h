//
// Created by amelia on 9/27/26.
//

#ifndef LANCE_TYPE_H
#define LANCE_TYPE_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    // Primitive Numeric & Boolean Types
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_F32,
    TYPE_F64,
    TYPE_BOOL,
    TYPE_STRING,
    TYPE_UNIT, // ()

    // Meta-type: The type of types ('type')
    TYPE_TYPE,

    // Compound Types
    TYPE_FUNCTION, // param_type -> return_type
    TYPE_STRUCT    // { field :: Type, ... }
} TypeKind;

typedef struct LanceType LanceType;

typedef struct {
    const char* name;
    LanceType* type;
} StructFieldType;

struct LanceType {
    TypeKind kind;
    uint64_t id;

    union {
        // TYPE_FUNCTION
        struct {
            LanceType* paramType;
            LanceType* returnType;
        } function;

        // TYPE_STRUCT
        struct {
            const char* name;
            StructFieldType* fields;
            size_t fieldCount;
        } structType;
    };
};

LanceType* GetTypeI8();
LanceType* GetTypeI16();
LanceType* GetTypeI32();
LanceType* GetTypeI64();
LanceType* GetTypeU8();
LanceType* GetTypeU16();
LanceType* GetTypeU32();
LanceType* GetTypeU64();
LanceType* GetTypeF32();
LanceType* GetTypeF64();
LanceType* GetTypeBool();
LanceType* GetTypeString();
LanceType* GetTypeUnit();
LanceType* GetTypeType();

LanceType* GetPrimitiveTypeByName(const char* name);

LanceType* CreateFunctionType(LanceType* paramType, LanceType* returnType);
LanceType* CreateStructType(const char* name, StructFieldType* fields, size_t fieldCount);

bool TypesAreEqual(const LanceType* a, const LanceType* b);

const char* TypeToString(const LanceType* type);

void FreeType(LanceType* type);

#endif //LANCE_TYPE_H
