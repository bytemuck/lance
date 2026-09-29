#ifndef LANCE_ARENA_H
#define LANCE_ARENA_H

#include <stddef.h>

typedef struct ArenaChunk ArenaChunk;

typedef struct {
	ArenaChunk *head;
} Arena;

void InitArena(Arena *arena);
void FreeArena(Arena *arena);

void *ArenaAlloc(Arena *arena, size_t size);

#define ARENA_NEW(arena, type) ((type *) ArenaAlloc((arena), sizeof(type)))
#define ARENA_ARRAY(arena, type, count) ((type *) ArenaAlloc((arena), sizeof(type) * (count)))

#endif // LANCE_ARENA_H
