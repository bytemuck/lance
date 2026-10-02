#pragma once

#include <stdint.h>

typedef struct {
	uint32_t line;
	uint32_t column;
} LNCCursor;

LNCCursor lncMakeCursor(uint32_t begin, uint32_t end);

LNCCursor lncAdvanceCursor(LNCCursor cursor);
LNCCursor lncAdvanceCursorN(LNCCursor cursor, uint32_t n);
LNCCursor lncEndCursor(LNCCursor cursor);
