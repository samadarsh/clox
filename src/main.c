#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

// Milestone 1: there is no compiler yet, so we build a chunk by hand.
// It is the bytecode for the Lox expression  -((1.2 + 3.4) / 5.6)
// Chapter 16 replaces this with a REPL and a script runner.
int main(int argc, const char* argv[]) {
  (void)argc;  // unused until chapter 16; silences -Wunused-parameter
  (void)argv;

  initVM();

  Chunk chunk;
  initChunk(&chunk);

  int constant = addConstant(&chunk, 1.2);
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant, 123);

  constant = addConstant(&chunk, 3.4);
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant, 123);

  writeChunk(&chunk, OP_ADD, 123);

  constant = addConstant(&chunk, 5.6);
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant, 123);

  writeChunk(&chunk, OP_DIVIDE, 123);
  writeChunk(&chunk, OP_NEGATE, 123);

  writeChunk(&chunk, OP_RETURN, 123);

  disassembleChunk(&chunk, "test chunk");
  interpret(&chunk);

  freeVM();
  freeChunk(&chunk);
  return 0;
}
