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
    Table pendingGlobals;    // name -> TypedDecl* of constants not evaluated yet
    Table evaluatingGlobals; // names of constants currently being evaluated (cycle detection)
    bool hadError;
};

void InitializeInterpreter(Interpreter* interp);
void FreeInterpreter(Interpreter* interp);

// Evaluates the module and returns a copy of `main` (release with FreeValue),
// or nullptr if there is no `main` or a runtime error occurred (hadError).
Value* InterpretTypedModule(Interpreter* interp, const TypedModule* module);

#endif // LANCE_INTERPRETER_H
