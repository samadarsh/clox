#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "object.h"

// Allocate a new array of `count` elements of `type`.
#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

// Free a single object of `type`.
#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

// Grow a dynamic array's capacity: start at 8, then double.
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

// Resize an array of `type` from oldCount to newCount elements.
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

// Release an array by "resizing" it to zero elements.
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

// The single function all of clox's dynamic memory goes through.
// Funnelling every allocation through one place is what will later let the
// garbage collector (chapter 26) track how much memory is in use.
void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void freeObjects(void);

#endif
