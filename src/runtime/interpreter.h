#ifndef LANCE_INTERPRETER_H
#define LANCE_INTERPRETER_H

#include "typed_ast.h"
#include "value.h"

// A fixed number of value slots, addressed by the indices the compiler
// resolved (SlotRef). Each slot owns its value, or is empty (nullptr).
typedef struct {
    Value** values;
    size_t count;
} Slots;

// Constants are evaluated on first use; the state detects definitions that
// depend on themselves.
typedef enum {
    GLOBAL_UNEVALUATED,
    GLOBAL_EVALUATING,
    GLOBAL_EVALUATED,
} GlobalState;

// SLOT_GLOBAL storage, parallel to TypedModule.declarations.
typedef struct {
    Slots values;
    GlobalState* states;
} Globals;

struct Interpreter {
    const TypedModule* module;
    Globals globals;
    Slots natives; // SLOT_NATIVE: one native function value per NativeId
    bool hadError;
};

void InitializeInterpreter(Interpreter* interp);
void FreeInterpreter(Interpreter* interp);

// Evaluates the module and returns a copy of `main` (release with FreeValue),
// or nullptr if there is no `main` or a runtime error occurred (hadError).
Value* InterpretTypedModule(Interpreter* interp, const TypedModule* module);

#endif // LANCE_INTERPRETER_H
