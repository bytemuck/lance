//
// Created by amelia on 9/28/26.
//

#ifndef LANCE_STRING_POOL_H
#define LANCE_STRING_POOL_H

#include <stddef.h>
#include <stdint.h>

// Initializes the global string pool
void InitStringPool(void);

// Reclaims the string pool at compiler shutdown
void FreeStringPool(void);

// Interns a string slice [chars, chars + length) and returns an immutable interned pointer
const char* InternString(const char* chars, size_t length);

// Interns a null-terminated string
const char* InternCString(const char* str);

#endif // LANCE_STRING_POOL_H
