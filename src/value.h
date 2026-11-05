#ifndef clox_value_h
#define clox_value_h

#include <string.h>

#include "common.h"

// Defined in object.h. Declared here so a Value can hold a pointer to one
// without value.h and object.h including each other.
typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

// NaN boxing (chapter 30): every Value is one 64-bit word.
//
// A double whose exponent bits are all 1 and whose top mantissa bit is set is
// a "quiet NaN". Real arithmetic produces only one or two NaN bit patterns,
// which leaves the other 51 bits of a quiet NaN free. So:
//   - a Value that is NOT a quiet NaN (with our QNAN bits) is a number, stored
//     as its own bits, unchanged;
//   - a quiet NaN with the sign bit clear and a small tag in the lowest bits
//     is nil, false or true;
//   - a quiet NaN with the sign bit set holds an Obj* in its low 48 bits
//     (pointers on 64-bit machines today only use 48 bits).
//
//   bit 63      62..52      51 50  49 ........................... 0
//   sign | exponent (11) | quiet | Intel | payload (50 bits)
#define SIGN_BIT ((uint64_t)0x8000000000000000)
// The 11 exponent bits, the quiet bit (51) and bit 50. Bit 50 is set too so
// our values never look like the "indefinite" NaN that Intel CPUs produce
// for 0/0, which must stay a number.
#define QNAN     ((uint64_t)0x7ffc000000000000)

// The lowest two bits of a QNAN value without the sign bit.
#define TAG_NIL   1 // 01.
#define TAG_FALSE 2 // 10.
#define TAG_TRUE  3 // 11.

typedef uint64_t Value;

// true is ...11 and false is ...10: setting the lowest bit turns false into
// true, so "is it either?" is one OR and one compare.
#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)
#define IS_NIL(value)       ((value) == NIL_VAL)
// Anything that isn't one of our quiet NaNs is a number.
#define IS_NUMBER(value)    (((value) & QNAN) != QNAN)
#define IS_OBJ(value) \
    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)      ((value) == TRUE_VAL)
#define AS_NUMBER(value)    valueToNum(value)
// Clear the sign and QNAN bits: what's left is the pointer.
#define AS_OBJ(value) \
    ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL))
#define NUMBER_VAL(num) numToValue(num)
#define OBJ_VAL(obj) \
    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

// Reinterpret the 8 bytes of a double as a uint64_t and back. memcpy is the
// way C allows this ("type punning" through a pointer cast is undefined
// behaviour). Compilers turn it into a plain register move, no copy.
static inline double valueToNum(Value value) {
  double num;
  memcpy(&num, &value, sizeof(Value));
  return num;
}

static inline Value numToValue(double num) {
  Value value;
  memcpy(&value, &num, sizeof(double));
  return value;
}

#else

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

#endif

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
