#include "lance/token/token.h"

void lncFreeToken(LNCToken* token) {
	lncFreeString(&token->lexeme);
	lncFreeString(&token->errorMessage);
}
