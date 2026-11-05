// Entry point for the WebAssembly build (`make wasm`), used by the browser
// playground instead of src/main.c. Everything else is the normal clox source.
//
// JavaScript (worker.js) creates a fresh instance of the module for every
// Run, copies the program into the module's memory as a C string, calls
// clox_run(), and frees the string afterwards. So JavaScript owns `source`;
// this function only reads it.
//
// Output needs nothing special: Emscripten sends everything written to stdout
// and stderr to JavaScript callbacks, one line at a time.

#include <stdio.h>

#include <emscripten.h>

#include "vm.h"

// EMSCRIPTEN_KEEPALIVE exports the function to JavaScript (as _clox_run) and
// stops the linker from removing it, since no C code calls it.
EMSCRIPTEN_KEEPALIVE
int clox_run(const char* source) {
  initVM();
  InterpretResult result = interpret(source);
  freeVM();

  // Push out any output still buffered, so it arrives before we return.
  fflush(stdout);
  fflush(stderr);

  // The same exit codes as the command-line clox (main.c).
  switch (result) {
    case INTERPRET_OK:            return 0;
    case INTERPRET_COMPILE_ERROR: return 65;
    case INTERPRET_RUNTIME_ERROR: return 70;
  }
  return 70; // Unreachable.
}
