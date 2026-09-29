#include "table.h"
#include "memory.h"

#include <string.h>

#define TABLE_MAX_LOAD 0.75

uint32_t HashString(const char *key, size_t length) {
	uint32_t hash = 2166136261u;
	for (size_t i = 0; i < length; i++) {
		hash ^= (uint8_t) key[i];
		hash *= 16777619;
	}
	return hash;
}

static TableEntry *FindEntry(TableEntry *entries, const size_t capacity, const char *key) {
	if (capacity == 0)
		return nullptr;

	uint32_t	index	  = HashString(key, strlen(key)) & (capacity - 1);
	TableEntry *tombstone = nullptr;

	while (true) {
		TableEntry *entry = &entries[index];
		if (entry->key == nullptr) {
			if (entry->value == nullptr) {
				return tombstone != nullptr ? tombstone : entry;
			}

			if (tombstone == nullptr)
				tombstone = entry;
		} else if (entry->key == key || strcmp(entry->key, key) == 0) {
			return entry;
		}

		index = (index + 1) & (capacity - 1);
	}
}

void TableInit(Table *table) {
	table->count	= 0;
	table->capacity = 0;
	table->entries	= nullptr;
}

void TableFree(Table *table) {
	if (table->entries != nullptr) {
		FREE_ARRAY(TableEntry, table->entries, table->capacity);
	}
	TableInit(table);
}

static void AdjustCapacity(Table *table, size_t capacity) {
	TableEntry *entries = ALLOCATE(TableEntry, capacity);
	for (size_t i = 0; i < capacity; i++) {
		entries[i].key	 = nullptr;
		entries[i].value = nullptr;
	}

	table->count = 0;
	for (size_t i = 0; i < table->capacity; i++) {
		TableEntry *entry = &table->entries[i];
		if (entry->key == nullptr)
			continue;

		TableEntry *dest = FindEntry(entries, capacity, entry->key);
		dest->key		 = entry->key;
		dest->value		 = entry->value;
		table->count++;
	}

	if (table->entries != nullptr) {
		FREE_ARRAY(TableEntry, table->entries, table->capacity);
	}
	table->entries	= entries;
	table->capacity = capacity;
}

void *TableGet(const Table *table, const char *key) {
	if (table->count == 0 || table->capacity == 0 || key == nullptr)
		return nullptr;

	TableEntry *entry = FindEntry(table->entries, table->capacity, key);
	if (entry == nullptr || entry->key == nullptr)
		return nullptr;

	return entry->value;
}

bool TableSet(Table *table, const char *key, void *value) {
	if (key == nullptr)
		return false;

	if (table->count + 1 > (size_t) (table->capacity * TABLE_MAX_LOAD)) {
		size_t capacity = GROW_CAPACITY(table->capacity);
		AdjustCapacity(table, capacity);
	}

	TableEntry *entry	 = FindEntry(table->entries, table->capacity, key);
	bool		isNewKey = (entry->key == nullptr);
	if (isNewKey && entry->value == nullptr) {
		table->count++;
	}

	entry->key	 = key;
	entry->value = value;
	return isNewKey;
}

bool TableDelete(Table *table, const char *key) {
	if (table->count == 0 || table->capacity == 0 || key == nullptr)
		return false;

	TableEntry *entry = FindEntry(table->entries, table->capacity, key);
	if (entry == nullptr || entry->key == nullptr)
		return false;

	// Place a tombstone
	entry->key	 = nullptr;
	entry->value = (void *) 1;
	return true;
}

void TableAddAll(const Table *from, Table *to) {
	for (size_t i = 0; i < from->capacity; i++) {
		TableEntry *entry = &from->entries[i];
		if (entry->key != nullptr) {
			TableSet(to, entry->key, entry->value);
		}
	}
}

const char *TableFindString(const Table *table, const char *chars, size_t length, uint32_t hash) {
	if (table->count == 0 || table->capacity == 0)
		return nullptr;

	uint32_t index = hash & (table->capacity - 1);
	while (true) {
		TableEntry *entry = &table->entries[index];
		if (entry->key == nullptr) {
			// Stop if an empty non-tombstone entry is found
			if (entry->value == nullptr)
				return nullptr;
		} else if (strlen(entry->key) == length && memcmp(entry->key, chars, length) == 0) {
			// Found matching interned string
			return entry->key;
		}

		index = (index + 1) & (table->capacity - 1);
	}
}