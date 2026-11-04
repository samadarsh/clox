#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
  Chunk* chunk;           // the chunk being executed
  uint8_t* ip;            // "instruction pointer": the next byte to run
  Value stack[STACK_MAX]; // the value stack
  Value* stackTop;        // points one past the top item (the next free slot)
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM(void);
void freeVM(void);
InterpretResult interpret(Chunk* chunk);
void push(Value value);
Value pop(void);

#endif
