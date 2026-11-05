#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)

#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
  OBJ_BOUND_METHOD,
  OBJ_CLASS,
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_INSTANCE,
  OBJ_NATIVE,
  OBJ_STRING,
  OBJ_UPVALUE
} ObjType;

// The header shared by every heap object.
struct Obj {
  ObjType type;
  bool isMarked;      // reached during the current garbage collection?
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

// A class: its name and its methods. `methods` maps each method name to the
// ObjClosure for it. With inheritance, the superclass's methods are copied in
// when the class is created ("copy-down"), so a lookup never walks up a chain.
typedef struct {
  Obj obj;
  ObjString* name;
  Table methods;
} ObjClass;

// An object made from a class. Each instance has its own `fields` table, so
// fields can be added at any time just by assigning to them.
typedef struct {
  Obj obj;
  ObjClass* klass;  // "klass" because "class" is a C++ keyword (as in the book)
  Table fields;
} ObjInstance;

// A method that has been taken out of an instance (`var f = obj.method;`).
// It remembers which instance it came from, so that when f is called later,
// `this` is that instance.
typedef struct {
  Obj obj;
  Value receiver;       // the instance: what `this` will be
  ObjClosure* method;   // the method's code
} ObjBoundMethod;

ObjBoundMethod* newBoundMethod(Value receiver,
                               ObjClosure* method);
ObjClass* newClass(ObjString* name);
ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction(void);
ObjInstance* newInstance(ObjClass* klass);
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
