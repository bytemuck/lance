#pragma once

#include "lance/span.h"

#include <stdint.h>

typedef struct {
	const char* items;	// Owned; NUL-terminated
	uint32_t	length; // Byte count, excluding the NUL terminator
} LNCString;

typedef struct {
	const char* items; // Borrowed; Not necessarily NUL-terminated
	uint32_t	length;
} LNCStringView;

LNCString lncMakeString(const char* items);
void	  lncFreeString(LNCString* string);

LNCString lncCopyString(LNCString string);
LNCString lncCopyStringView(LNCStringView view);
LNCString lncCopySubstring(LNCString string, LNCSpan span);

LNCStringView lncViewString(LNCString string);
LNCStringView lncSubstringView(LNCStringView view, LNCSpan span);
