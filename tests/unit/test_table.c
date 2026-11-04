// Unit tests for the hash table (table.c).
//
// A table only reads a key's `hash` field and compares key *pointers* (strings
// are interned, so equal strings share one object). That lets these tests use
// hand-made ObjString keys with any hash they like, which makes collisions,
// wrap-around and tombstones easy to set up on purpose.
//
// The keys live in a static array, not on the heap, so the only heap memory
// here is each table's bucket array. ASan checks every access to it, and
// freeTable() must release it.

#include <stdio.h>
#include <string.h>

#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) \
    do { \
      checks++; \
      if (!(cond)) { \
        failures++; \
        fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      } \
    } while (false)

#define MAX_KEYS 1100

static ObjString keys[MAX_KEYS];
static char names[MAX_KEYS][16];

// Makes key number i, named "k<i>", with the given hash.
static ObjString* key(int i, uint32_t hash) {
  snprintf(names[i], sizeof(names[i]), "k%d", i);
  keys[i].obj.type = OBJ_STRING;
  keys[i].obj.next = NULL;
  keys[i].length = (int)strlen(names[i]);
  keys[i].chars = names[i];
  keys[i].hash = hash;
  return &keys[i];
}

// A hash that spreads keys out (Knuth's multiplicative hash).
static uint32_t spread(int i) {
  return (uint32_t)i * 2654435761u;
}

static bool isTombstone(Entry* entry) {
  return entry->key == NULL && IS_BOOL(entry->value) && AS_BOOL(entry->value);
}

static int countLive(Table* table) {
  int n = 0;
  for (int i = 0; i < table->capacity; i++) {
    if (table->entries[i].key != NULL) n++;
  }
  return n;
}

static int countTombstones(Table* table) {
  int n = 0;
  for (int i = 0; i < table->capacity; i++) {
    if (isTombstone(&table->entries[i])) n++;
  }
  return n;
}

static bool getNumber(Table* table, ObjString* k, double* out) {
  Value value;
  if (!tableGet(table, k, &value)) return false;
  *out = AS_NUMBER(value);
  return true;
}

static void testEmptyTable(void) {
  Table table;
  initTable(&table);
  Value value;
  CHECK(!tableGet(&table, key(0, 1), &value));
  CHECK(!tableDelete(&table, key(0, 1)));
  CHECK(tableFindString(&table, "k0", 2, 1) == NULL);
  CHECK(table.capacity == 0 && table.entries == NULL);
  freeTable(&table);
}

static void testInsertFindOverwrite(void) {
  Table table;
  initTable(&table);
  ObjString* a = key(0, spread(0));
  ObjString* b = key(1, spread(1));

  CHECK(tableSet(&table, a, NUMBER_VAL(1)) == true);   // new key
  CHECK(tableSet(&table, b, NUMBER_VAL(2)) == true);
  CHECK(tableSet(&table, a, NUMBER_VAL(10)) == false); // overwrite
  CHECK(table.count == 2);
  CHECK(table.capacity == 8);  // first allocation is 8 buckets

  double n;
  CHECK(getNumber(&table, a, &n) && n == 10);
  CHECK(getNumber(&table, b, &n) && n == 2);
  CHECK(!getNumber(&table, key(2, spread(2)), &n));    // never inserted

  freeTable(&table);
  CHECK(table.count == 0 && table.capacity == 0 && table.entries == NULL);
}

static void testSameHashDifferentKey(void) {
  // Equal hashes don't make keys equal: lookup compares pointers, so a
  // different key with the same hash must probe past and report "missing".
  Table table;
  initTable(&table);
  tableSet(&table, key(0, 5), NUMBER_VAL(1));
  double n;
  CHECK(!getNumber(&table, key(1, 5), &n));
  freeTable(&table);
}

static void testCollisionsUseLinearProbing(void) {
  Table table;
  initTable(&table);
  // Three keys with hash 3: in an 8-bucket table they all want bucket 3,
  // so they end up in 3, 4 and 5, in insertion order.
  ObjString* a = key(0, 3);
  ObjString* b = key(1, 3);
  ObjString* c = key(2, 3);
  tableSet(&table, a, NUMBER_VAL(1));
  tableSet(&table, b, NUMBER_VAL(2));
  tableSet(&table, c, NUMBER_VAL(3));

  CHECK(table.capacity == 8);
  CHECK(table.entries[3].key == a);
  CHECK(table.entries[4].key == b);
  CHECK(table.entries[5].key == c);

  double n;
  CHECK(getNumber(&table, a, &n) && n == 1);
  CHECK(getNumber(&table, b, &n) && n == 2);
  CHECK(getNumber(&table, c, &n) && n == 3);
  freeTable(&table);
}

static void testProbingWrapsAround(void) {
  Table table;
  initTable(&table);
  // 0xFFFFFFFF % 8 == 7, the last bucket. The 2nd and 3rd key must wrap
  // around to buckets 0 and 1.
  ObjString* a = key(0, 0xFFFFFFFFu);
  ObjString* b = key(1, 0xFFFFFFFFu);
  ObjString* c = key(2, 0xFFFFFFFFu);
  tableSet(&table, a, NUMBER_VAL(1));
  tableSet(&table, b, NUMBER_VAL(2));
  tableSet(&table, c, NUMBER_VAL(3));

  CHECK(table.entries[7].key == a);
  CHECK(table.entries[0].key == b);
  CHECK(table.entries[1].key == c);

  double n;
  CHECK(getNumber(&table, c, &n) && n == 3);
  freeTable(&table);
}

static void testDeleteLeavesTombstone(void) {
  Table table;
  initTable(&table);
  ObjString* a = key(0, 3);
  ObjString* b = key(1, 3);
  ObjString* c = key(2, 3);
  tableSet(&table, a, NUMBER_VAL(1));
  tableSet(&table, b, NUMBER_VAL(2));
  tableSet(&table, c, NUMBER_VAL(3));

  // Delete the middle of the probe chain 3 -> 4 -> 5.
  CHECK(tableDelete(&table, b) == true);
  CHECK(isTombstone(&table.entries[4]));
  CHECK(countTombstones(&table) == 1);
  CHECK(table.count == 3);  // tombstones still count (see tableSet)

  double n;
  CHECK(!getNumber(&table, b, &n));             // gone
  // The crucial part: c sits *after* the tombstone in the chain. If delete
  // had made bucket 4 plain empty, the lookup would stop there and miss c.
  CHECK(getNumber(&table, c, &n) && n == 3);
  CHECK(getNumber(&table, a, &n) && n == 1);
  CHECK(tableDelete(&table, b) == false);       // deleting twice

  // A new key with the same hash reuses the tombstone's bucket, and since
  // the tombstone was already counted, count doesn't grow.
  ObjString* d = key(3, 3);
  CHECK(tableSet(&table, d, NUMBER_VAL(4)) == true);
  CHECK(table.entries[4].key == d);
  CHECK(countTombstones(&table) == 0);
  CHECK(table.count == 3);
  CHECK(getNumber(&table, d, &n) && n == 4);

  freeTable(&table);
}

static void testLookupTerminatesWhenAllDeleted(void) {
  Table table;
  initTable(&table);
  // 6 keys fill an 8-bucket table to its limit (6 <= 8 * 0.75).
  for (int i = 0; i < 6; i++) tableSet(&table, key(i, spread(i)), NUMBER_VAL(i));
  CHECK(table.capacity == 8);
  for (int i = 0; i < 6; i++) CHECK(tableDelete(&table, &keys[i]));

  CHECK(countLive(&table) == 0);
  CHECK(countTombstones(&table) == 6);
  // Because tombstones count towards the load, at least 2 buckets are truly
  // empty, so these lookups must stop instead of looping forever.
  double n;
  CHECK(!getNumber(&table, key(50, spread(50)), &n));
  CHECK(!getNumber(&table, &keys[0], &n));
  freeTable(&table);
}

static void testGrowingDropsTombstones(void) {
  Table table;
  initTable(&table);
  for (int i = 0; i < 6; i++) tableSet(&table, key(i, spread(i)), NUMBER_VAL(i));
  for (int i = 0; i < 3; i++) tableDelete(&table, &keys[i]);
  CHECK(table.count == 6 && countTombstones(&table) == 3);

  // The 7th entry pushes past 75% of 8, so the table grows to 16 buckets.
  // Re-inserting copies only live entries, so the tombstones disappear.
  tableSet(&table, key(6, spread(6)), NUMBER_VAL(6));
  CHECK(table.capacity == 16);
  CHECK(countTombstones(&table) == 0);
  CHECK(table.count == 4);  // keys 3, 4, 5 and 6

  double n;
  for (int i = 3; i < 7; i++) CHECK(getNumber(&table, &keys[i], &n) && n == i);
  for (int i = 0; i < 3; i++) CHECK(!getNumber(&table, &keys[i], &n));
  freeTable(&table);
}

static void testGrowsPastLoadFactor(void) {
  Table table;
  initTable(&table);
  int bad = 0;
  int lastCapacity = 0;
  int grows = 0;

  for (int i = 0; i < 1000; i++) {
    tableSet(&table, key(i, spread(i)), NUMBER_VAL(i));
    if (table.capacity != lastCapacity) {
      grows++;
      lastCapacity = table.capacity;
    }
    // Never more than 75% full, and capacity is always a power of two.
    if (table.count > table.capacity * 0.75) bad++;
    if ((table.capacity & (table.capacity - 1)) != 0) bad++;
  }
  CHECK(bad == 0);
  CHECK(table.count == 1000);
  CHECK(table.capacity == 2048);  // 1024 * 0.75 = 768 < 1000, so one more doubling
  CHECK(grows == 9);              // 8, 16, 32, ..., 2048

  // Every key survived all the rehashing, with the right value.
  double n;
  int missing = 0;
  for (int i = 0; i < 1000; i++) {
    if (!getNumber(&table, &keys[i], &n) || n != i) missing++;
  }
  CHECK(missing == 0);
  freeTable(&table);
}

static void testManyKeysInOneBucket(void) {
  Table table;
  initTable(&table);
  // 200 keys, all with hash 42: every insert and lookup starts at the same
  // bucket and has to probe past all the others. Slow, but must be correct.
  for (int i = 0; i < 200; i++) tableSet(&table, key(i, 42), NUMBER_VAL(i * 2));
  CHECK(table.count == 200);

  double n;
  int missing = 0;
  for (int i = 0; i < 200; i++) {
    if (!getNumber(&table, &keys[i], &n) || n != i * 2) missing++;
  }
  CHECK(missing == 0);

  // Delete every even key: the odd ones, scattered after tombstones in one
  // long probe chain, must all still be found.
  for (int i = 0; i < 200; i += 2) CHECK(tableDelete(&table, &keys[i]));
  int wrong = 0;
  for (int i = 0; i < 200; i++) {
    bool found = getNumber(&table, &keys[i], &n);
    if (found != (i % 2 == 1)) wrong++;
  }
  CHECK(wrong == 0);
  CHECK(countLive(&table) == 100 && countTombstones(&table) == 100);
  freeTable(&table);
}

static void testAddAllCopiesLiveEntries(void) {
  Table from, to;
  initTable(&from);
  initTable(&to);
  for (int i = 0; i < 20; i++) tableSet(&from, key(i, spread(i)), NUMBER_VAL(i));
  tableDelete(&from, &keys[5]);

  tableAddAll(&from, &to);
  double n;
  CHECK(countLive(&to) == 19);
  CHECK(getNumber(&to, &keys[7], &n) && n == 7);
  CHECK(!getNumber(&to, &keys[5], &n));  // tombstones are not copied
  freeTable(&from);
  freeTable(&to);
}

static void testFindStringComparesContent(void) {
  Table table;
  initTable(&table);
  ObjString* a = key(0, 9);   // "k0"
  ObjString* b = key(1, 9);   // "k1", same hash
  tableSet(&table, a, NIL_VAL);
  tableSet(&table, b, NIL_VAL);

  // A *different* buffer with the same characters finds the stored key.
  char buffer[] = "k1";
  CHECK(tableFindString(&table, buffer, 2, 9) == b);
  CHECK(tableFindString(&table, "k0", 2, 9) == a);
  CHECK(tableFindString(&table, "k2", 2, 9) == NULL);  // same hash, other chars
  CHECK(tableFindString(&table, "k1", 2, 8) == NULL);  // other hash
  CHECK(tableFindString(&table, "k", 1, 9) == NULL);   // other length

  // It also probes past tombstones.
  tableDelete(&table, a);
  CHECK(tableFindString(&table, "k0", 2, 9) == NULL);
  CHECK(tableFindString(&table, "k1", 2, 9) == b);
  freeTable(&table);
}

int main(void) {
  // Table arrays are allocated through reallocate(), which may run the GC
  // (on every allocation in the stress build), so the VM must be set up.
  // The GC never touches these test tables: they aren't roots, and their
  // static keys aren't heap objects.
  initVM();
  testEmptyTable();
  testInsertFindOverwrite();
  testSameHashDifferentKey();
  testCollisionsUseLinearProbing();
  testProbingWrapsAround();
  testDeleteLeavesTombstone();
  testLookupTerminatesWhenAllDeleted();
  testGrowingDropsTombstones();
  testGrowsPastLoadFactor();
  testManyKeysInOneBucket();
  testAddAllCopiesLiveEntries();
  testFindStringComparesContent();

  freeVM();

  if (failures > 0) {
    printf("%d of %d checks failed\n", failures, checks);
    return 1;
  }
  printf("PASS (%d checks)\n", checks);
  return 0;
}
