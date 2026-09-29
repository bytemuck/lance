#ifndef LANCE_ARENA_H
#define LANCE_ARENA_H

#include <stddef.h>

// Bump-pointer allocator. Everything allocated from an arena is released at
// once by FreeArena, so objects with a shared lifetime (e.g. all types of one
// compilation) need no individual ownership tracking.
typedef struct ArenaChunk ArenaChunk;

typedef struct {
    ArenaChunk* head;
} Arena;

void InitArena(Arena* arena);
void FreeArena(Arena* arena);

// Returns zero-initialized memory aligned for any object type.
void* ArenaAlloc(Arena* arena, size_t size);

#define ARENA_NEW(arena, type) ((type*)ArenaAlloc((arena), sizeof(type)))
#define ARENA_ARRAY(arena, type, count) ((type*)ArenaAlloc((arena), sizeof(type) * (count)))

#endif // LANCE_ARENA_H
