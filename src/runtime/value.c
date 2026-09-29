//
// Created by amelia on 9/27/26.
//

#include "value.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

Value* MakeIntValue(const int64_t value) {
    Value* const v = malloc(sizeof(Value));

    if (!v) {
        return nullptr;
    }

    v->kind = VAL_INT;
    v->intVal = value;

    return v;
}

Value* MakeFloatValue(const double value) {
    Value* const v = malloc(sizeof(Value));

    if (!v) {
        return nullptr;
    }

    v->kind = VAL_FLOAT;
    v->floatVal = value;

    return v;
}

Value* MakeBoolValue(const bool value) {
    Value* const v = malloc(sizeof(Value));

    if (!v) {
        return nullptr;
    }

    v->kind = VAL_BOOL;
    v->boolVal = value;

    return v;
}

Value* MakeStringValue(const char* value) {
    Value* const v = malloc(sizeof(Value));

    if (!v) {
        return nullptr;
    }

    v->kind = VAL_STRING;
    v->stringVal = value;

    return v;
}

Value* MakeTypeValue(LanceType* type) {
    Value* const v = malloc(sizeof(Value));

    if (!v) {
        return nullptr;
    }

    v->kind = VAL_TYPE;
    v->typeVal = type;

    return v;
}

Value* MakeUnitValue(void) {
    Value* const v = malloc(sizeof(Value));

    if (!v) {
        return nullptr;
    }

    v->kind = VAL_UNIT;

    return v;
}

Value* MakeNativeFnValue(const char* name, const NativeFn fn, const size_t arity, Value** appliedArgs, const size_t appliedCount) {
    Value* const v = malloc(sizeof(Value));
    if (!v) {
        return nullptr;
    }

    v->kind = VAL_NATIVE_FN;
    v->nativeFn.name = name;
    v->nativeFn.fn = fn;
    v->nativeFn.arity = arity;
    v->nativeFn.appliedCount = appliedCount;

    if (appliedCount > 0 && appliedArgs) {
        v->nativeFn.appliedArgs = (Value**)malloc(appliedCount * sizeof(Value*));

        for (size_t i = 0; i < appliedCount; i++) {
            v->nativeFn.appliedArgs[i] = CopyValue(appliedArgs[i]);
        }
    } else {
        v->nativeFn.appliedArgs = nullptr;
    }

    return v;
}

Value* MakeStructValue(const char* typeName, StructFieldValue* fields, const size_t fieldCount) {
    Value* const v = malloc(sizeof(Value));

    if (!v) {
        return nullptr;
    }

    v->kind = VAL_STRUCT;
    v->structVal.typeName = typeName;
    v->structVal.fields = fields;
    v->structVal.fieldCount = fieldCount;

    return v;
}

Value* MakeClosureValue(const TypedDecl* decl, Value** appliedArgs, const size_t appliedCount) {
    Value* const v = malloc(sizeof(Value));

    if (!v) {
        return nullptr;
    }

    v->kind = VAL_CLOSURE;
    v->closure.decl = decl;
    v->closure.appliedCount = appliedCount;
    v->closure.totalParams = decl ? decl->paramCount : 0;

    if (appliedCount > 0 && appliedArgs) {
        v->closure.appliedArgs = (Value**) malloc(appliedCount * sizeof(Value*));

        for (size_t i = 0; i < appliedCount; i++) {
            v->closure.appliedArgs[i] = CopyValue(appliedArgs[i]);
        }
    } else {
        v->closure.appliedArgs = nullptr;
    }

    return v;
}

Value* CopyValue(const Value* value) {
    if (!value) {
        return nullptr;
    }

    switch (value->kind) {
        case VAL_INT:       return MakeIntValue(value->intVal);
        case VAL_FLOAT:     return MakeFloatValue(value->floatVal);
        case VAL_BOOL:      return MakeBoolValue(value->boolVal);
        case VAL_STRING:    return MakeStringValue(value->stringVal);
        case VAL_TYPE:      return MakeTypeValue(value->typeVal);
        case VAL_UNIT:      return MakeUnitValue();
        case VAL_NATIVE_FN:
            return MakeNativeFnValue(value->nativeFn.name, value->nativeFn.fn,
                                     value->nativeFn.arity,
                                     value->nativeFn.appliedArgs,
                                     value->nativeFn.appliedCount);

        case VAL_STRUCT: {
            StructFieldValue* const newFields = malloc(value->structVal.fieldCount * sizeof(StructFieldValue));

            for (size_t i = 0; i < value->structVal.fieldCount; i++) {
                newFields[i].name = value->structVal.fields[i].name;
                newFields[i].value = CopyValue(value->structVal.fields[i].value);
            }

            return MakeStructValue(value->structVal.typeName, newFields, value->structVal.fieldCount);
        }

        case VAL_CLOSURE: return MakeClosureValue(value->closure.decl, value->closure.appliedArgs, value->closure.appliedCount);
        default:
            return nullptr;
    }
}

void PrintValue(const Value* value) {
    if (!value) {
        printf("<null>");

        return;
    }

    switch (value->kind) {
        case VAL_INT:
            printf("%" PRId64, value->intVal);
            break;
        case VAL_FLOAT:
            printf("%f", value->floatVal);
            break;
        case VAL_BOOL:
            printf("%s", value->boolVal ? "true" : "false");
            break;
        case VAL_STRING:
            printf("\"%s\"", value->stringVal);
            break;
        case VAL_TYPE:
            printf("<type %s>", TypeToString(value->typeVal));
            break;
        case VAL_UNIT:
            printf("()");
            break;
        case VAL_NATIVE_FN:
            printf("<native function %s>", value->nativeFn.name);
            break;
        case VAL_CLOSURE:
            printf("<function %s>", value->closure.decl ? value->closure.decl->name : "anonymous");
            break;
        case VAL_STRUCT:
            printf(".{ ");

            for (size_t i = 0; i < value->structVal.fieldCount; i++) {
                printf("%s = ", value->structVal.fields[i].name);
                PrintValue(value->structVal.fields[i].value);

                if (i + 1 < value->structVal.fieldCount) {
                    printf(", ");
                }
            }

            printf(" }");

            break;
        default:
            break;
    }
}

void FreeValue(Value* value) {
    if (!value) {
        return;
    }

    if (value->kind == VAL_NATIVE_FN) {
        if (value->nativeFn.appliedArgs) {
            for (size_t i = 0; i < value->nativeFn.appliedCount; i++) {
                FreeValue(value->nativeFn.appliedArgs[i]);
            }

            free(value->nativeFn.appliedArgs);
        }
    } else if (value->kind == VAL_STRUCT) {
        if (value->structVal.fields) {
            for (size_t i = 0; i < value->structVal.fieldCount; i++) {
                FreeValue(value->structVal.fields[i].value);
            }

            free(value->structVal.fields);
        }
    } else if (value->kind == VAL_CLOSURE) {
        if (value->closure.appliedArgs) {
            for (size_t i = 0; i < value->closure.appliedCount; i++) {
                FreeValue(value->closure.appliedArgs[i]);
            }

            free(value->closure.appliedArgs);
        }
    }

    free(value);
}