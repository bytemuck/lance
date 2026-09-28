//
// Created by amelia on 9/27/26.
//

#include "type.h"

#include <stdlib.h>
#include <string.h>

static LanceType gTypeI8 = { .kind = TYPE_I8, .id = 0 };
static LanceType gTypeI16 = { .kind = TYPE_I16, .id = 1 };
static LanceType gTypeI32 = { .kind = TYPE_I32, .id = 2 };
static LanceType gTypeI64 = { .kind = TYPE_I64, .id = 3 };

static LanceType gTypeU8 = { .kind = TYPE_U8, .id = 4 };
static LanceType gTypeU16 = { .kind = TYPE_U16, .id = 5 };
static LanceType gTypeU32 = { .kind = TYPE_U32, .id = 6 };
static LanceType gTypeU64 = { .kind = TYPE_U64, .id = 7 };

static LanceType gTypeF32 = { .kind = TYPE_F32, .id = 8 };
static LanceType gTypeF64 = { .kind = TYPE_F64, .id = 9 };

static LanceType gTypeBool = { .kind = TYPE_BOOL, .id = 10 };
static LanceType gTypeString = { .kind = TYPE_STRING, .id = 11 };
static LanceType gTypeUnit = { .kind = TYPE_UNIT, .id = 12 };

static LanceType gTypeType = { .kind = TYPE_TYPE, .id = 13 };

#define INITIAL_NEXT_ID 15
static uint64_t gNextTypeId = INITIAL_NEXT_ID;

LanceType* GetTypeI8() { return &gTypeI8; }
LanceType* GetTypeI16() { return &gTypeI16; }
LanceType* GetTypeI32() { return &gTypeI32; }
LanceType* GetTypeI64() { return &gTypeI64; }
LanceType* GetTypeU8() { return &gTypeU8; }
LanceType* GetTypeU16() { return &gTypeU16; }
LanceType* GetTypeU32() { return &gTypeU32; }
LanceType* GetTypeU64() { return &gTypeU64; }
LanceType* GetTypeF32() { return &gTypeF32; }
LanceType* GetTypeF64() { return &gTypeF64; }
LanceType* GetTypeBool() { return &gTypeBool; }
LanceType* GetTypeString() { return &gTypeString; }
LanceType* GetTypeUnit() { return &gTypeUnit; }
LanceType* GetTypeType() { return &gTypeType; }

LanceType* GetPrimitiveTypeByName(const char* name) {
    if (!name) return nullptr;

    if (strcmp(name, "i8") == 0) return &gTypeI8;
    if (strcmp(name, "i16") == 0) return &gTypeI16;
    if (strcmp(name, "i32") == 0) return &gTypeI32;
    if (strcmp(name, "i64") == 0) return &gTypeI64;
    if (strcmp(name, "u8") == 0) return &gTypeU8;
    if (strcmp(name, "u16") == 0) return &gTypeU16;
    if (strcmp(name, "u32") == 0) return &gTypeU32;
    if (strcmp(name, "u64") == 0) return &gTypeU64;
    if (strcmp(name, "f32") == 0) return &gTypeF32;
    if (strcmp(name, "f64") == 0) return &gTypeF64;
    if (strcmp(name, "bool") == 0) return &gTypeBool;
    if (strcmp(name, "string") == 0) return &gTypeString;
    if (strcmp(name, "()") == 0) return &gTypeUnit;
    if (strcmp(name, "type") == 0 || strcmp(name, "Type") == 0) return &gTypeType;

    return nullptr;
}

LanceType* CreateFunctionType(LanceType* paramType, LanceType* returnType) {
    const auto type = (LanceType*)malloc(sizeof(LanceType));
    if (!type) return nullptr;

    type->kind = TYPE_FUNCTION;
    type->id = gNextTypeId++;
    type->function.paramType = paramType;
    type->function.returnType = returnType;

    return type;
}

LanceType* CreateStructType(const char* name, StructFieldType* fields, const size_t fieldCount) {
    const auto type = (LanceType*)malloc(sizeof(LanceType));
    if (!type) return nullptr;

    type->kind = TYPE_STRUCT;
    type->id = gNextTypeId++;
    type->structType.name = name;
    type->structType.fields = fields;
    type->structType.fieldCount = fieldCount;

    return type;
}

bool TypesAreEqual(const LanceType *a, const LanceType *b) {
    if (a == b) {
        return true;
    }

    if (!a || !b) {
        return false;
    }

    if (a->kind != b->kind) {
        return false;
    }

    switch (a->kind) {
        case TYPE_FUNCTION:
            return TypesAreEqual(a->function.paramType, b->function.paramType) &&
                   TypesAreEqual(a->function.returnType, b->function.returnType);

        case TYPE_STRUCT:
            return a->id == b->id;

        default:
            return true;
    }
}


const char* TypeToString(const LanceType* type) {
    if (!type) return "<null>";

    switch (type->kind) {
        case TYPE_I8: return "i8";
        case TYPE_I16: return "i16";
        case TYPE_I32: return "i32";
        case TYPE_I64: return "i64";
        case TYPE_U8: return "u8";
        case TYPE_U16: return "u16";
        case TYPE_U32: return "u32";
        case TYPE_U64: return "u64";
        case TYPE_F32: return "f32";
        case TYPE_F64: return "f64";
        case TYPE_BOOL: return "bool";
        case TYPE_STRING: return "string";
        case TYPE_UNIT: return "()";
        case TYPE_TYPE: return "type";
        case TYPE_FUNCTION: return "<function>";
        case TYPE_STRUCT:
            if (type->structType.name) return type->structType.name;
            return "{ ... }";
    }

    return "<unknown>";
}

void FreeType(LanceType* type) {
    if (!type) {
        return;
    }

    if (type->id < INITIAL_NEXT_ID) {
        return;
    }

    if (type->kind == TYPE_FUNCTION) {
        FreeType(type->function.paramType);
        FreeType(type->function.returnType);
    } else if (type->kind == TYPE_STRUCT) {
        if (type->structType.fields) {
            for (size_t i = 0; i < type->structType.fieldCount; i++) {
                FreeType(type->structType.fields[i].type);
            }

            free(type->structType.fields);
        }
    }

    free(type);
}