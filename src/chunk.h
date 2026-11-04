#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// Each instruction starts with a one-byte operation code ("opcode").
typedef enum {
  OP_CONSTANT,  // operand: 1-byte index into the constant pool
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NOT,
  OP_NEGATE,
  OP_RETURN,
} OpCode;

// A chunk is a sequence of bytecode plus the data it needs.
typedef struct {
  int count;          // bytes in use
  int capacity;       // bytes allocated
  uint8_t* code;      // the bytecode itself
  int* lines;         // lines[i] = source line of code[i], for error messages
  ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);

#endif
