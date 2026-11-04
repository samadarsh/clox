#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"

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

#endif
