#include "lance/token/tokenize.h"

#include <stdio.h>

int main() {
	LNCString	 string	   = lncMakeString("main :: i32\n"
										   "main = {{{}}} letlet 42\n");
	LNCTokenizer tokenizer = lncMakeTokenizer(lncViewString(string));

	LNCToken token = lncTokenize(&tokenizer);
	while (token.kind != LNC_TOKEN_END_OF_FILE) {
		printf("TokenID: '%d' with Lexeme: '%s' and span: (%d:%d)\n", token.kind, token.lexeme.items, token.span.begin, token.span.end);

		lncFreeToken(&token);
		token = lncTokenize(&tokenizer);
	}

	lncFreeToken(&token);
	lncFreeString(&string);
}
