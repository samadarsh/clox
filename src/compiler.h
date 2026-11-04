#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

// Compiles `source` into a function representing the top-level script.
// Returns NULL if there was a compile error (already reported on stderr).
ObjFunction* compile(const char* source);

#endif
