#include "lance/span.h"

LNCSpan lncMakeSpan(uint32_t begin, uint32_t end) {
	return (LNCSpan) {
			.begin = begin,
			.end   = end,
	};
}

LNCSpan lncAdvanceSpan(LNCSpan span) {
	return lncAdvanceSpanN(span, 1);
}

LNCSpan lncAdvanceSpanN(LNCSpan span, uint32_t n) {
	return lncMakeSpan(span.begin, span.end + n);
}


LNCSpan lncEndSpan(LNCSpan span) {
	return lncMakeSpan(span.end, span.end);
}
