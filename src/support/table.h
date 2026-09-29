#ifndef LANCE_TABLE_H
#define LANCE_TABLE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	const char *key;   // Interned string pointer
	void	   *value; // Generic pointer payload
} TableEntry;

typedef struct {
	size_t		count;	  // Active entries + tombstones
	size_t		capacity; // Total bucket count
	TableEntry *entries;
} Table;

void TableInit(Table *table);
void TableFree(Table *table);

void *TableGet(const Table *table, const char *key);
bool  TableSet(Table *table, const char *key, void *value);
bool  TableDelete(Table *table, const char *key);

void TableAddAll(const Table *from, Table *to);

const char *TableFindString(const Table *table, const char *chars, size_t length, uint32_t hash);

uint32_t HashString(const char *key, size_t length);

#endif // LANCE_TABLE_H
