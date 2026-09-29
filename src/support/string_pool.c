#include "string_pool.h"
#include "memory.h"
#include "table.h"

#include <string.h>

static Table gStringPool;
static bool	 gInitialized = false;

void InitStringPool() {
	TableInit(&gStringPool);
	gInitialized = true;
}

void FreeStringPool() {
	if (!gInitialized)
		return;

	for (size_t i = 0; i < gStringPool.capacity; i++) {
		TableEntry *entry = &gStringPool.entries[i];
		if (entry->key != nullptr) {
			FREE_ARRAY(char, (char *) entry->key, strlen(entry->key) + 1);
		}
	}

	TableFree(&gStringPool);
	gInitialized = false;
}

const char *InternString(const char *chars, size_t length) {
	if (!gInitialized) {
		InitStringPool();
	}
	if (chars == nullptr)
		return nullptr;

	uint32_t	hash	 = HashString(chars, length);
	const char *interned = TableFindString(&gStringPool, chars, length, hash);
	if (interned != nullptr) {
		return interned;
	}

	char *heapChars = ALLOCATE(char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';

	TableSet(&gStringPool, heapChars, (void *) heapChars);
	return heapChars;
}

const char *InternCString(const char *str) {
	if (str == nullptr)
		return nullptr;
	return InternString(str, strlen(str));
}
