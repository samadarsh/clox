#include <stdlib.h>

#include "memory.h"
#include "vm.h"

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

static void freeObject(Obj* object) {
  switch (object->type) {
    case OBJ_STRING: {
      // Free what the object owns (its characters) before the object itself,
      // because once it is freed we must not read string->chars any more.
      ObjString* string = (ObjString*)object;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjString, object);
      break;
    }
  }
}

void freeObjects(void) {
  Obj* object = vm.objects;
  while (object != NULL) {
    // Save the next pointer first: after freeObject(), `object` points to
    // freed memory and reading object->next would be a use-after-free.
    Obj* next = object->next;
    freeObject(object);
    object = next;
  }
}
