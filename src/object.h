#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_STRING(value)       isObjType(value, OBJ_STRING)

#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
  OBJ_STRING,
} ObjType;

// The header shared by every heap object.
struct Obj {
  ObjType type;
  struct Obj* next;   // the next object in vm.objects, the list of all objects
};

// "Struct inheritance": the first field of ObjString is a whole Obj. So an
// ObjString* points at an Obj header, and can safely be cast to Obj* and back.
struct ObjString {
  Obj obj;
  int length;    // number of characters, not counting the '\0'
  char* chars;   // heap array of length + 1 bytes, '\0'-terminated
  uint32_t hash; // computed once at creation, used by hash tables
};

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
void printObject(Value value);

// A real function, not a macro, because `value` is used twice and a macro
// would evaluate an expression like pop() twice.
static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
