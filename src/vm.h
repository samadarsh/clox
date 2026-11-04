#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "table.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
  Chunk* chunk;           // the chunk being executed
  uint8_t* ip;            // "instruction pointer": the next byte to run
  Value stack[STACK_MAX]; // the value stack
  Value* stackTop;        // points one past the top item (the next free slot)
  Table globals;          // global variables: name -> value
  Table strings;          // every string, for interning (used as a set)
  Obj* objects;           // head of the linked list of every heap object
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

// The one global VM, defined in vm.c. `extern` lets other files (object.c,
// memory.c) use it.
extern VM vm;

void initVM(void);
void freeVM(void);
InterpretResult interpret(const char* source);
void push(Value value);
Value pop(void);

#endif
