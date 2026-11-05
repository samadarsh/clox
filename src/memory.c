#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"

#ifdef CLOX_PLAYGROUND
// Browser playground build only (`make wasm`): see playground/hooks.h.
#include "hooks.h"
#endif

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

// After a collection, the next one happens when the heap has grown to this
// many times the size of what survived.
#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  // Callers must pass the true old size: the running total depends on it.
  // (size_t is unsigned, so shrinking wraps around, and the next addition
  // wraps back. The total still comes out right.)
  vm.bytesAllocated += newSize - oldSize;
#ifdef CLOX_PLAYGROUND
  playgroundNoteHeap();   // peak heap size, for the playground's stats bar
#endif

  // Only collect when asking for MORE memory: freeing can't run out.
  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    // Collect on every allocation. Slow, but any object that isn't properly
    // reachable gets freed immediately, so GC bugs show up at once instead
    // of rarely and far from their cause.
    collectGarbage();
#endif

    if (vm.bytesAllocated > vm.nextGC) {
      collectGarbage();
    }
  }

  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}

// Marks an object as reachable (white -> gray) and queues it on the gray
// stack, so traceReferences() will later mark everything it points to.
void markObject(Obj* object) {
  if (object == NULL) return;
  // Already gray or black: don't process twice. This also stops cycles
  // (an object that, through others, points back to itself) looping forever.
  if (object->isMarked) return;

#ifdef DEBUG_LOG_GC
  printf("%p mark ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif

  object->isMarked = true;

  if (vm.grayCapacity < vm.grayCount + 1) {
    vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
    // Plain realloc, not reallocate(): growing the gray stack must not
    // start another collection in the middle of this one.
    vm.grayStack = (Obj**)realloc(vm.grayStack,
                                  sizeof(Obj*) * vm.grayCapacity);
    if (vm.grayStack == NULL) exit(1);
  }

  vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
  // Numbers, booleans and nil live inside the Value itself: nothing to mark.
  if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void markArray(ValueArray* array) {
  for (int i = 0; i < array->count; i++) {
    markValue(array->values[i]);
  }
}

// Gray -> black: mark everything this object refers to. The object stays
// marked (isMarked), and is no longer on the gray stack.
static void blackenObject(Obj* object) {
#ifdef DEBUG_LOG_GC
  printf("%p blacken ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif

  switch (object->type) {
    case OBJ_BOUND_METHOD: {
      ObjBoundMethod* bound = (ObjBoundMethod*)object;
      markValue(bound->receiver);
      markObject((Obj*)bound->method);
      break;
    }
    case OBJ_CLASS: {
      ObjClass* klass = (ObjClass*)object;
      markObject((Obj*)klass->name);
      markTable(&klass->methods);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      markObject((Obj*)closure->function);
      for (int i = 0; i < closure->upvalueCount; i++) {
        markObject((Obj*)closure->upvalues[i]);
      }
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      markObject((Obj*)function->name);
      markArray(&function->chunk.constants);
      break;
    }
    case OBJ_INSTANCE: {
      // The class keeps the methods alive; the fields table keeps every
      // field name and value alive.
      ObjInstance* instance = (ObjInstance*)object;
      markObject((Obj*)instance->klass);
      markTable(&instance->fields);
      break;
    }
    case OBJ_UPVALUE:
      // A closed upvalue owns its variable's value. (An open one points into
      // the stack, which is marked as a root anyway.)
      markValue(((ObjUpvalue*)object)->closed);
      break;
    case OBJ_NATIVE:
    case OBJ_STRING:
      // No references to other objects.
      break;
  }
}

static void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void*)object, object->type);
#endif

  switch (object->type) {
    case OBJ_BOUND_METHOD:
      // Doesn't own the receiver or the method: they are objects of their
      // own in vm.objects, which the GC frees when they become unreachable.
      FREE(ObjBoundMethod, object);
      break;
    case OBJ_CLASS: {
      // The class owns its methods *table* (the bucket array), but not the
      // closures in it, which are separate objects.
      ObjClass* klass = (ObjClass*)object;
      freeTable(&klass->methods);
      FREE(ObjClass, object);
      break;
    }
    case OBJ_CLOSURE: {
      // Free the array of upvalue pointers, but not the upvalues themselves:
      // other closures may share them, and they are objects in vm.objects,
      // so they get freed on their own.
      ObjClosure* closure = (ObjClosure*)object;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues,
                 closure->upvalueCount);
      FREE(ObjClosure, object);
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      freeChunk(&function->chunk);
      FREE(ObjFunction, object);
      break;
    }
    case OBJ_INSTANCE: {
      // Same idea: free the fields table's bucket array, not the values.
      ObjInstance* instance = (ObjInstance*)object;
      freeTable(&instance->fields);
      FREE(ObjInstance, object);
      break;
    }
    case OBJ_NATIVE:
      FREE(ObjNative, object);
      break;
    case OBJ_STRING: {
      // Free what the object owns (its characters) before the object itself,
      // because once it is freed we must not read string->chars any more.
      ObjString* string = (ObjString*)object;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjString, object);
      break;
    }
    case OBJ_UPVALUE:
      // An upvalue doesn't own the variable it points at.
      FREE(ObjUpvalue, object);
      break;
  }
}

// Roots: references the VM can reach directly, without going through another
// object. Everything reachable from a root is kept alive.
static void markRoots(void) {
  // Values on the stack: locals, temporaries, the functions being called.
  for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(*slot);
  }

  // The closure of each active call. (Also in stack slot 0 of its frame, but
  // the book marks them here too, so the VM doesn't rely on that.)
  for (int i = 0; i < vm.frameCount; i++) {
    markObject((Obj*)vm.frames[i].closure);
  }

  // Upvalues still pointing into the stack.
  for (ObjUpvalue* upvalue = vm.openUpvalues;
       upvalue != NULL;
       upvalue = upvalue->next) {
    markObject((Obj*)upvalue);
  }

  // Global variables: both the name strings and the values.
  markTable(&vm.globals);

  // Functions the compiler is in the middle of building. A collection can
  // run during compiling, since the compiler allocates strings and constants.
  markCompilerRoots();

  // The interned string "init", which the VM looks up on every class call.
  markObject((Obj*)vm.initString);
}

// Keep turning gray objects black until no gray ones are left. At that point
// every object reachable from a root is marked, and the rest are garbage.
static void traceReferences(void) {
  while (vm.grayCount > 0) {
    Obj* object = vm.grayStack[--vm.grayCount];
    blackenObject(object);
  }
}

// Walk every object: free the unmarked (white) ones, unlink them from the
// list, and clear the mark on survivors so the next collection starts fresh.
static void sweep(void) {
  Obj* previous = NULL;
  Obj* object = vm.objects;
  while (object != NULL) {
    if (object->isMarked) {
      object->isMarked = false;
      previous = object;
      object = object->next;
    } else {
      Obj* unreached = object;
      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm.objects = object;
      }

      freeObject(unreached);
    }
  }
}

void collectGarbage(void) {
#ifdef CLOX_PLAYGROUND
  playgroundNoteCollection();   // for the playground's stats bar
#endif
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
  size_t before = vm.bytesAllocated;
#endif

  markRoots();
  traceReferences();
  // The intern table must not keep strings alive (it's a weak reference),
  // but it must not keep pointers to strings about to be freed either.
  tableRemoveWhite(&vm.strings);
  sweep();

  // Self-adjusting threshold: the next collection waits until the heap has
  // doubled from what is live now.
  vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
  printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
         before - vm.bytesAllocated, before, vm.bytesAllocated,
         vm.nextGC);
#endif
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

  // The gray stack was allocated with plain realloc, so plain free.
  free(vm.grayStack);
}
