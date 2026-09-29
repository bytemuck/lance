#ifndef LANCE_PRIMITIVES_H
#define LANCE_PRIMITIVES_H

#include <stddef.h>
#include <string.h>

#define LANCE_PRIMITIVE_OPERATORS(X)                                                                                   \
	X(Add, "+#", NUMERIC, +)                                                                                           \
	X(Sub, "-#", NUMERIC, -)                                                                                           \
	X(Mul, "*#", NUMERIC, *)                                                                                           \
	X(Div, "/#", NUMERIC, /)                                                                                           \
	X(IntDiv, "//#", INTEGRAL, /)                                                                                      \
	X(Eq, "==#", COMPARISON, ==)                                                                                       \
	X(Neq, "!=#", COMPARISON, !=)                                                                                      \
	X(Lt, "<#", COMPARISON, <)                                                                                         \
	X(Lte, "<=#", COMPARISON, <=)                                                                                      \
	X(Gt, ">#", COMPARISON, >)                                                                                         \
	X(Gte, ">=#", COMPARISON, >=)

typedef enum {
	PRIMITIVE_CLASS_NUMERIC,
	PRIMITIVE_CLASS_INTEGRAL,
	PRIMITIVE_CLASS_COMPARISON,
} PrimitiveClass;

typedef enum {
#define LANCE_PRIMITIVE_ENUM(ID, NAME, CLASS, OP) PRIMITIVE_OP_##ID,
	LANCE_PRIMITIVE_OPERATORS(LANCE_PRIMITIVE_ENUM)
#undef LANCE_PRIMITIVE_ENUM
			PRIMITIVE_OP_COUNT
} PrimitiveOp;

typedef struct {
	PrimitiveOp op;
	const char *name;
	PrimitiveClass class;
} PrimitiveInfo;

static const PrimitiveInfo kPrimitiveOperators[PRIMITIVE_OP_COUNT] = {
#define LANCE_PRIMITIVE_INFO(ID, NAME, CLASS, OP)                                                                      \
	{.op = PRIMITIVE_OP_##ID, .name = NAME, .class = PRIMITIVE_CLASS_##CLASS},
		LANCE_PRIMITIVE_OPERATORS(LANCE_PRIMITIVE_INFO)
#undef LANCE_PRIMITIVE_INFO
};

static inline const PrimitiveInfo *LookupPrimitiveOperator(const char *name) {
	if (!name)
		return nullptr;
	for (size_t i = 0; i < PRIMITIVE_OP_COUNT; i++) {
		if (strcmp(kPrimitiveOperators[i].name, name) == 0)
			return &kPrimitiveOperators[i];
	}
	return nullptr;
}

#define LANCE_PRINT_NAME "print"
#define LANCE_IF_NAME "if#"

// Every native function has a fixed SLOT_NATIVE index: the primitive
// operators first (a PrimitiveOp is its own index), then the others.
typedef enum { NATIVE_PRINT = PRIMITIVE_OP_COUNT, NATIVE_IF, NATIVE_COUNT } NativeId;

#endif // LANCE_PRIMITIVES_H
