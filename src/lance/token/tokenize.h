#pragma once

#include "lance/span.h"

#include "lance/token/cursor.h"
#include "lance/token/token.h"

typedef struct {
	LNCStringView source;
	LNCCursor	  cursor;
	LNCSpan		  span;
} LNCTokenizer;

LNCTokenizer lncMakeTokenizer(LNCStringView source);

LNCToken lncTokenize(LNCTokenizer* tokenizer);
