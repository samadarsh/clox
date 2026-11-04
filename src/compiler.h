#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

// Compiles `source` into a function representing the top-level script.
// Returns NULL if there was a compile error (already reported on stderr).
ObjFunction* compile(const char* source);
// Marks the functions currently being compiled, which the GC can't otherwise
// reach (nothing points to them until compiling finishes).
void markCompilerRoots(void);

#endif
