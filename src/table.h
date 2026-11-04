#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

// One bucket. Three possible states:
//   empty:      key == NULL, value == nil
//   tombstone:  key == NULL, value == true   (a deleted entry, see tableDelete)
//   in use:     key != NULL
typedef struct {
  ObjString* key;
  Value value;
} Entry;

// A hash table with open addressing and linear probing.
typedef struct {
  int count;       // entries in use PLUS tombstones (see tableSet)
  int capacity;    // number of buckets; always 0 or a power of two
  Entry* entries;  // heap array of `capacity` buckets
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars,
                           int length, uint32_t hash);

#endif
