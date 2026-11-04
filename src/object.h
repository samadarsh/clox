#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)

#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_STRING,
  OBJ_UPVALUE
} ObjType;

// The header shared by every heap object.
struct Obj {
  ObjType type;
  struct Obj* next;   // the next object in vm.objects, the list of all objects
};

// A compiled function: its bytecode and what's needed to call it. Created by
// the compiler. At runtime it is always wrapped in an ObjClosure.
typedef struct {
  Obj obj;
  int arity;          // number of parameters
  int upvalueCount;   // number of variables it captures from outer functions
  Chunk chunk;        // its own bytecode, constants and line numbers
  ObjString* name;    // NULL for the top-level script
} ObjFunction;

// A function written in C. It receives the arguments as a pointer to the
// first one on the VM stack, and returns the result.
typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;

// "Struct inheritance": the first field of ObjString is a whole Obj. So an
// ObjString* points at an Obj header, and can safely be cast to Obj* and back.
struct ObjString {
  Obj obj;
  int length;    // number of characters, not counting the '\0'
  char* chars;   // heap array of length + 1 bytes, '\0'-terminated
  uint32_t hash; // computed once at creation, used by hash tables
};

// A variable captured by a closure.
//   open:   the variable still lives on the VM stack, and `location` points
//           at its stack slot.
//   closed: the variable's stack slot is gone. Its value was copied into
//           `closed`, and `location` now points at that field instead.
// Code that reads or writes through `location` works the same either way.
typedef struct ObjUpvalue {
  Obj obj;
  Value* location;
  Value closed;
  struct ObjUpvalue* next;   // next open upvalue in vm.openUpvalues
} ObjUpvalue;

// A function plus the variables it captured. Many closures can share one
// ObjFunction (same code); each has its own upvalues (different variables).
typedef struct {
  Obj obj;
  ObjFunction* function;
  ObjUpvalue** upvalues;   // heap array of upvalueCount pointers
  int upvalueCount;
} ObjClosure;

ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction(void);
ObjNative* newNative(NativeFn function);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
ObjUpvalue* newUpvalue(Value* slot);
void printObject(Value value);

// A real function, not a macro, because `value` is used twice and a macro
// would evaluate an expression like pop() twice.
static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
