#ifndef clox_compiler_h
#define clox_compiler_h

#include "vm.h"

// Compiles `source` into bytecode, appending it to `chunk`.
// Returns false if there was a compile error (already reported on stderr).
bool compile(const char* source, Chunk* chunk);

#endif
