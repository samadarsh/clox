// Hooks for the browser playground. Only the WebAssembly build defines
// CLOX_PLAYGROUND and puts playground/ on the include path (Makefile, `make
// wasm`), so the normal clox never sees this file: in src/, every use of it is
// inside `#ifdef CLOX_PLAYGROUND`, which the preprocessor removes otherwise.
//
// The hooks only observe. They read the VM's state and never change it, so a
// program behaves exactly the same with them as without. The one exception is
// deliberate: the debugger stops a program after recording its step limit.
//
// The counters and recorder are defined in clox_wasm.c.

#ifndef clox_playground_hooks_h
#define clox_playground_hooks_h

#include "vm.h"

extern uint64_t playgroundInstructions;  // instructions executed
extern int playgroundCollections;        // garbage collections
extern size_t playgroundPeakBytes;       // highest vm.bytesAllocated seen
extern bool playgroundRecording;         // is the debugger recording?

// Records one step for the debugger. Returns false when the recording is
// full, which stops the program.
bool playgroundRecord(CallFrame* frame, uint8_t* ip);

// Called by run() before each instruction. `ip` points at its opcode (run()
// keeps the current ip in a local variable, so frame->ip may be out of date).
static inline bool playgroundStep(CallFrame* frame, uint8_t* ip) {
  playgroundInstructions++;
  if (!playgroundRecording) return true;
  return playgroundRecord(frame, ip);
}

// Called by reallocate() after it updates vm.bytesAllocated.
static inline void playgroundNoteHeap(void) {
  if (vm.bytesAllocated > playgroundPeakBytes) {
    playgroundPeakBytes = vm.bytesAllocated;
  }
}

// Called at the start of every collection.
static inline void playgroundNoteCollection(void) {
  playgroundCollections++;
}

#endif
