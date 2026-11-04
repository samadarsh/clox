#ifndef clox_vm_h
#define clox_vm_h

#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
// Each frame can address at most 256 slots (one-byte operands), so this many
// values always fit even with every frame in use.
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

// One function call in progress.
typedef struct {
  ObjClosure* closure;  // the function being run
  uint8_t* ip;          // where we are in *its* bytecode (the caller's ip is
                        // saved in the caller's frame, which is how a return
                        // knows where to continue)
  Value* slots;         // its window on the VM stack: slots[0] is the callee
                        // itself, slots[1..] are the arguments, then locals
} CallFrame;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frameCount;

  Value stack[STACK_MAX]; // the value stack, shared by all frames
  Value* stackTop;        // points one past the top item (the next free slot)
  Table globals;          // global variables: name -> value
  Table strings;          // every string, for interning (used as a set)
  ObjUpvalue* openUpvalues; // upvalues still pointing into the stack, sorted
                            // by stack slot, highest slot first

  size_t bytesAllocated;  // heap bytes in use, through reallocate()
  size_t nextGC;          // collect when bytesAllocated goes above this
  Obj* objects;           // head of the linked list of every heap object
  int grayCount;          // the gray stack: objects marked but not yet
  int grayCapacity;       //   scanned for references to other objects
  Obj** grayStack;
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
