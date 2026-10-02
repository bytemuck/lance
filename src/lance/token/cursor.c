#include "lance/token/cursor.h"

LNCCursor lncMakeCursor(uint32_t line, uint32_t column) {
	return (LNCCursor) {
			.line	= line,
			.column = column,
	};
}

LNCCursor lncAdvanceCursor(LNCCursor cursor) {
	return lncAdvanceCursorN(cursor, 1);
}

LNCCursor lncAdvanceCursorN(LNCCursor cursor, uint32_t n) {
	return lncMakeCursor(cursor.line, cursor.column + n);
}

LNCCursor lncEndCursor(LNCCursor cursor) {
	return lncMakeCursor(cursor.line + 1, 1);
}
