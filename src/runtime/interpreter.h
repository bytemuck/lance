#ifndef LANCE_INTERPRETER_H
#define LANCE_INTERPRETER_H

#include "typed_ast.h"
#include "value.h"

typedef struct {
	Value **values;
	size_t	count;
} Slots;

typedef enum {
	GLOBAL_UNEVALUATED,
	GLOBAL_EVALUATING,
	GLOBAL_EVALUATED,
} GlobalState;

typedef struct {
	Slots		 values;
	GlobalState *states;
} Globals;

struct Interpreter {
	const TypedModule *module;
	Globals			   globals;
	Slots			   natives;
	bool			   hadError;
};

void InitializeInterpreter(Interpreter *interp);
void FreeInterpreter(Interpreter *interp);

Value *InterpretTypedModule(Interpreter *interp, const TypedModule *module);

#endif // LANCE_INTERPRETER_H
