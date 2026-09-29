#ifndef LANCE_STRING_POOL_H
#define LANCE_STRING_POOL_H

#include <stddef.h>

void InitStringPool();
void FreeStringPool();

const char *InternString(const char *chars, size_t length);
const char *InternCString(const char *str);

#endif // LANCE_STRING_POOL_H
