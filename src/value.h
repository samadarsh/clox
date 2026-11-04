#ifndef clox_value_h
#define clox_value_h

#include "common.h"

// Defined in object.h. Declared here so a Value can hold a pointer to one
// without value.h and object.h including each other.
typedef struct Obj Obj;
typedef struct ObjString ObjString;

// The "tag": which kind of value is stored in the union below.
typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ
} ValueType;

// A tagged union. `type` says which member of `as` is valid. The members of a
// union share the same bytes, so the union is only as big as its largest
// member (the 8-byte double or pointer), not the sum of them.
typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Obj* obj;     // anything bigger than 8 bytes lives on the heap
  } as;
} Value;

// Check the tag.
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

// Read the payload. Only valid after checking the tag with IS_...
#define AS_OBJ(value)     ((value).as.obj)
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)

// Wrap a C value in a Lox Value.
#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})

// A growable array of Values. A chunk uses one as its "constant pool".
typedef struct {
  int capacity;
  int count;
  Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif
