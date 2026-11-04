#include <stdlib.h>

#include "memory.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  // oldSize is unused until the garbage collector needs it (chapter 26).
  // The cast silences -Wunused-parameter, which -Werror would turn into a
  // build failure.
  (void)oldSize;

  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}
