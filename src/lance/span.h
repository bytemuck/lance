#pragma once

#include <stdint.h>

typedef struct {
	uint32_t begin;
	uint32_t end;
} LNCSpan;

LNCSpan lncMakeSpan(uint32_t begin, uint32_t end);

LNCSpan lncAdvanceSpan(LNCSpan span);
LNCSpan lncAdvanceSpanN(LNCSpan span, uint32_t n);

LNCSpan lncEndSpan(LNCSpan span);
