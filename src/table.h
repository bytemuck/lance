//
// Created by amelia on 9/28/26.
//

#ifndef LANCE_TABLE_H
#define LANCE_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char* key;   // Interned string pointer
    void* value;       // Generic pointer payload
} TableEntry;

typedef struct {
    size_t count;      // Active entries + tombstones
    size_t capacity;   // Total bucket count
    TableEntry* entries;
} Table;

void TableInit(Table* table);
void TableFree(Table* table);

// O(1) Lookup: compares interned key pointers directly
void* TableGet(const Table* table, const char* key);

// O(1) Insertion: returns true if a new key was added, false if overwritten
bool TableSet(Table* table, const char* key, void* value);

// O(1) Deletion: marks entry with a tombstone
bool TableDelete(Table* table, const char* key);

// Copies all entries from 'from' table to 'to' table
void TableAddAll(const Table* from, Table* to);

// Finds an existing interned string in the table by character slice and hash
const char* TableFindString(const Table* table, const char* chars, size_t length, uint32_t hash);

// Hash computation helper for string slices
uint32_t HashString(const char* key, size_t length);

#endif // LANCE_TABLE_H
