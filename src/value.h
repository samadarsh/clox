#ifndef clox_value_h
#define clox_value_h

#include "common.h"

// For now every Lox value is a double. Chapter 18 turns this into a tagged
// union so we can also have booleans and nil.
typedef double Value;

// A growable array of Values. A chunk uses one as its "constant pool".
typedef struct {
  int capacity;
  int count;
  Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif
