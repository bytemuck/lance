//
// Created by amelia on 9/28/26.
//

#include "memory.h"

#include <stdio.h>
#include <stdlib.h>

void* reallocate(void* pointer, size_t oldSize, const size_t newSize) {
	(void)oldSize;

	if (newSize == 0) {
		free(pointer);

		return NULL;
	}

	void* result = realloc(pointer, newSize);

	if (result == NULL) {
		fprintf(stderr, "Fatal Error: Out of memory (requested %zu bytes)\n", newSize);
		exit(1);
	}

	return result;
}