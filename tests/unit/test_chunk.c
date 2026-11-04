// Unit tests for chunks, value arrays and the VM stack.
//
// These exercise the dynamic arrays directly. Run under `make test-asan`, any
// out-of-bounds write, use-after-free or leak in the growth code is caught.

#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
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

static void testNewChunkIsEmpty(void) {
  Chunk chunk;
  initChunk(&chunk);
  CHECK(chunk.count == 0);
  CHECK(chunk.capacity == 0);
  CHECK(chunk.code == NULL);
  CHECK(chunk.lines == NULL);
  CHECK(chunk.constants.count == 0);
  freeChunk(&chunk);
}

static void testWriteChunkGrowsAndKeepsBytes(void) {
  Chunk chunk;
  initChunk(&chunk);

  // 1000 bytes forces many reallocations: 8, 16, 32, ... 1024.
  for (int i = 0; i < 1000; i++) {
    writeChunk(&chunk, (uint8_t)(i % 256), i / 10);
  }

  CHECK(chunk.count == 1000);
  CHECK(chunk.capacity == 1024);

  int bad = 0;
  for (int i = 0; i < 1000; i++) {
    if (chunk.code[i] != (uint8_t)(i % 256)) bad++;
    if (chunk.lines[i] != i / 10) bad++;
  }
  CHECK(bad == 0);

  freeChunk(&chunk);
  CHECK(chunk.count == 0);
  CHECK(chunk.capacity == 0);
  CHECK(chunk.code == NULL);
}

static void testAddConstantReturnsIndex(void) {
  Chunk chunk;
  initChunk(&chunk);

  CHECK(addConstant(&chunk, 1.2) == 0);
  CHECK(addConstant(&chunk, 3.4) == 1);
  for (int i = 2; i < 100; i++) {
    CHECK(addConstant(&chunk, (double)i) == i);
  }

  CHECK(chunk.constants.count == 100);
  CHECK(chunk.constants.values[0] == 1.2);
  CHECK(chunk.constants.values[1] == 3.4);
  CHECK(chunk.constants.values[99] == 99.0);

  freeChunk(&chunk);
  CHECK(chunk.constants.count == 0);
  CHECK(chunk.constants.values == NULL);
}

static void testStackIsLastInFirstOut(void) {
  initVM();
  push(1.0);
  push(2.0);
  push(3.0);
  CHECK(pop() == 3.0);
  CHECK(pop() == 2.0);
  push(4.0);
  CHECK(pop() == 4.0);
  CHECK(pop() == 1.0);
  freeVM();
}

int main(void) {
  testNewChunkIsEmpty();
  testWriteChunkGrowsAndKeepsBytes();
  testAddConstantReturnsIndex();
  testStackIsLastInFirstOut();

  if (failures > 0) {
    printf("%d of %d checks failed\n", failures, checks);
    return 1;
  }
  printf("PASS (%d checks)\n", checks);
  return 0;
}
