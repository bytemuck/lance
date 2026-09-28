#ifndef LANCE_INTERPRETER_H
#define LANCE_INTERPRETER_H

#include "table.h"
#include "typed_ast.h"
#include "value.h"

struct Environment {
    Environment* parent;
    Table table;
};

struct Interpreter {
    Environment* globals;
    const TypedModule* module;
};

void InitializeInterpreter(Interpreter* interp);
void FreeInterpreter(Interpreter* interp);

Value* InterpretTypedModule(Interpreter* interp, const TypedModule* module);

#endif // LANCE_INTERPRETER_H
