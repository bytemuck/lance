#include "arena.h"
#include "memory.h"

#include <string.h>

#define ARENA_CHUNK_SIZE (16 * 1024)

struct ArenaChunk {
	ArenaChunk *next;
	size_t		used;
	size_t		capacity;
	alignas(max_align_t) unsigned char data[];
};

static ArenaChunk *NewChunk(ArenaChunk *next, const size_t capacity) {
	ArenaChunk *chunk = (ArenaChunk *) reallocate(nullptr, 0, sizeof(ArenaChunk) + capacity);
	chunk->next		  = next;
	chunk->used		  = 0;
	chunk->capacity	  = capacity;
	return chunk;
}

void InitArena(Arena *arena) { arena->head = nullptr; }

void FreeArena(Arena *arena) {
	ArenaChunk *chunk = arena->head;
	while (chunk) {
		ArenaChunk *next = chunk->next;
		reallocate(chunk, sizeof(ArenaChunk) + chunk->capacity, 0);
		chunk = next;
	}
	arena->head = nullptr;
}

void *ArenaAlloc(Arena *arena, const size_t size) {
	const size_t alignment	 = alignof(max_align_t);
	const size_t alignedSize = (size + alignment - 1) & ~(alignment - 1);

	if (!arena->head || arena->head->used + alignedSize > arena->head->capacity) {
		const size_t capacity = alignedSize > ARENA_CHUNK_SIZE ? alignedSize : ARENA_CHUNK_SIZE;
		arena->head			  = NewChunk(arena->head, capacity);
	}

	void *memory = arena->head->data + arena->head->used;
	arena->head->used += alignedSize;
	memset(memory, 0, size);
	return memory;
}
