//
// Created by amelia on 9/28/26.
//

#ifndef LANCE_MEMORY_H
#define LANCE_MEMORY_H

#include <stddef.h>

// Dynamic capacity growth calculation: starts at 8, then doubles
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

// Type-safe dynamic array resizing
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

// Type-safe buffer cleanup
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) \
    reallocate(pointer, sizeof(type), 0)

// Core reallocation primitive: handles malloc, realloc, and free uniformly
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

#endif // LANCE_MEMORY_H
