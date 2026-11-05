// Entry points for the WebAssembly build (`make wasm`), used by the browser
// playground instead of src/main.c. Everything else is the normal clox source.
//
// JavaScript (worker.js) creates a fresh instance of the module for every
// request, copies the program into the module's memory as a C string, calls
// one of the functions below, and frees the string afterwards. So JavaScript
// owns `source`; these functions only read it.
//
// Output needs nothing special: Emscripten sends everything written to stdout
// and stderr to JavaScript callbacks, one line at a time.
//
// The build defines CLOX_PLAYGROUND, which switches on the hooks declared in
// hooks.h: run() and the GC call them to count instructions and collections
// and, for the debugger, to record each step. They are defined here.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <emscripten.h>

#include "compiler.h"
#include "debug.h"
#include "hooks.h"
#include "object.h"
#include "vm.h"

// --- statistics (hooks.h) ----------------------------------------------------

uint64_t playgroundInstructions = 0;
int playgroundCollections = 0;
size_t playgroundPeakBytes = 0;
bool playgroundRecording = false;

static int exitCode(InterpretResult result) {
  // The same exit codes as the command-line clox (main.c).
  switch (result) {
    case INTERPRET_OK:            return 0;
    case INTERPRET_COMPILE_ERROR: return 65;
    case INTERPRET_RUNTIME_ERROR: return 70;
  }
  return 70; // Unreachable.
}

// EMSCRIPTEN_KEEPALIVE exports a function to JavaScript (as _name) and stops
// the linker from removing it, since no C code calls it.
EMSCRIPTEN_KEEPALIVE
int clox_run(const char* source) {
  initVM();
  InterpretResult result = interpret(source);
  freeVM();

  // Push out any output still buffered, so it arrives before we return.
  fflush(stdout);
  fflush(stderr);
  return exitCode(result);
}

// Numbers for the stats bar after a run. Returned as doubles because
// JavaScript numbers can't hold every 64-bit integer, and doubles are exact up
// to 2^53, far more than any run in a browser will reach.
EMSCRIPTEN_KEEPALIVE double clox_stat_instructions(void) {
  return (double)playgroundInstructions;
}
EMSCRIPTEN_KEEPALIVE int clox_stat_collections(void) {
  return playgroundCollections;
}
EMSCRIPTEN_KEEPALIVE double clox_stat_peak_bytes(void) {
  return (double)playgroundPeakBytes;
}

// --- every function in a program ---------------------------------------------

// The compiler stores each function as a constant of the function around it
// (the operand of OP_CLOSURE), and methods as constants of the code that
// declares the class. So starting from the script and following function
// constants finds every function, always in the same order. The bytecode tab
// and the debugger both number functions by this order.
#define MAX_FUNCTIONS 1024
static ObjFunction* functions[MAX_FUNCTIONS];
static int functionCount = 0;

static void collectFunctions(ObjFunction* script) {
  functionCount = 0;
  functions[functionCount++] = script;
  // Breadth-first: the list itself is the queue.
  for (int i = 0; i < functionCount; i++) {
    ValueArray* constants = &functions[i]->chunk.constants;
    for (int j = 0; j < constants->count; j++) {
      if (IS_FUNCTION(constants->values[j]) && functionCount < MAX_FUNCTIONS) {
        functions[functionCount++] = AS_FUNCTION(constants->values[j]);
      }
    }
  }
}

static int functionIndex(ObjFunction* function) {
  for (int i = 0; i < functionCount; i++) {
    if (functions[i] == function) return i;
  }
  return -1;
}

static const char* functionName(ObjFunction* function) {
  return function->name == NULL ? "<script>" : function->name->chars;
}

// --- disassembly for the Bytecode tab -----------------------------------------

// Compiles without running and prints the bytecode of every function, using
// the book's own disassembler (debug.c). worker.js reads the output:
//   @fn <name>             starts function number 0, 1, 2, ...
//   @op <offset> <line>    the next instruction's offset and source line,
//                          followed by the disassembler's line(s) for it
// Returns 0, or 65 with the compile errors on stderr.
EMSCRIPTEN_KEEPALIVE
int clox_disassemble(const char* source) {
  initVM();
  ObjFunction* script = compile(source);
  if (script == NULL) {
    freeVM();
    fflush(stderr);
    return 65;
  }

  // Nothing below allocates, so the GC can't run. Pushing the script anyway
  // keeps it reachable, as the rest of clox would.
  push(OBJ_VAL(script));
  collectFunctions(script);
  for (int i = 0; i < functionCount; i++) {
    Chunk* chunk = &functions[i]->chunk;
    printf("@fn %s\n", functionName(functions[i]));
    for (int offset = 0; offset < chunk->count;) {
      printf("@op %d %d\n", offset, chunk->lines[offset]);
      offset = disassembleInstruction(chunk, offset);
    }
  }
  pop();
  freeVM();
  fflush(stdout);
  return 0;
}

// --- the debugger's recording -------------------------------------------------

// The recording is JSON text in a growable buffer. It uses the C library's
// malloc/realloc directly, not clox's reallocate(): that would count the
// recorder's memory as Lox heap and could start a garbage collection in the
// middle of an instruction.
typedef struct {
  char* data;
  size_t length;
  size_t capacity;
} Buffer;

static Buffer recording;
static int maxSteps = 0;
static int stepCount = 0;
static bool recordingFull = false;

static void bufferReserve(Buffer* buffer, size_t extra) {
  if (buffer->length + extra + 1 <= buffer->capacity) return;
  size_t capacity = buffer->capacity < 4096 ? 4096 : buffer->capacity;
  while (capacity < buffer->length + extra + 1) capacity *= 2;
  char* data = realloc(buffer->data, capacity);
  if (data == NULL) exit(1);
  buffer->data = data;
  buffer->capacity = capacity;
}

static void bufferPrintf(Buffer* buffer, const char* format, ...) {
  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);
  int needed = vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  bufferReserve(buffer, (size_t)needed);
  vsnprintf(buffer->data + buffer->length, (size_t)needed + 1, format, args);
  buffer->length += (size_t)needed;
  va_end(args);
}

// Appends `text` (at most maxLength bytes of it) as a JSON string.
static void bufferJsonString(Buffer* buffer, const char* text, int length,
                             int maxLength) {
  bool cut = length > maxLength;
  if (cut) length = maxLength;
  bufferPrintf(buffer, "\"");
  for (int i = 0; i < length; i++) {
    unsigned char c = (unsigned char)text[i];
    if (c == '"' || c == '\\') {
      bufferPrintf(buffer, "\\%c", c);
    } else if (c < 0x20) {
      bufferPrintf(buffer, "\\u%04x", c);
    } else {
      bufferPrintf(buffer, "%c", c);
    }
  }
  bufferPrintf(buffer, cut ? "...\"" : "\"");
}

// A value as the debugger shows it on the stack: like `print`, except that
// strings get quotes. Written here rather than in value.c/object.c so the
// book's code stays untouched.
static void bufferValue(Buffer* buffer, Value value) {
  char text[64];
  if (IS_BOOL(value)) {
    bufferPrintf(buffer, AS_BOOL(value) ? "\"true\"" : "\"false\"");
  } else if (IS_NIL(value)) {
    bufferPrintf(buffer, "\"nil\"");
  } else if (IS_NUMBER(value)) {
    snprintf(text, sizeof(text), "%g", AS_NUMBER(value));
    bufferJsonString(buffer, text, (int)strlen(text), 64);
  } else if (IS_STRING(value)) {
    // Quotes around the string's own characters, shortened if long.
    ObjString* string = AS_STRING(value);
    char quoted[48];
    int shown = string->length > 40 ? 40 : string->length;
    snprintf(quoted, sizeof(quoted), "\"%.*s%s\"", shown, string->chars,
             string->length > 40 ? "..." : "");
    bufferJsonString(buffer, quoted, (int)strlen(quoted), 64);
  } else {
    switch (OBJ_TYPE(value)) {
      case OBJ_BOUND_METHOD:
        snprintf(text, sizeof(text), "<fn %s>",
                 functionName(AS_BOUND_METHOD(value)->method->function));
        break;
      case OBJ_CLASS:
        snprintf(text, sizeof(text), "%s", AS_CLASS(value)->name->chars);
        break;
      case OBJ_CLOSURE:
        snprintf(text, sizeof(text), "<fn %s>",
                 functionName(AS_CLOSURE(value)->function));
        break;
      case OBJ_FUNCTION:
        snprintf(text, sizeof(text), "<fn %s>",
                 functionName(AS_FUNCTION(value)));
        break;
      case OBJ_INSTANCE:
        snprintf(text, sizeof(text), "%s instance",
                 AS_INSTANCE(value)->klass->name->chars);
        break;
      case OBJ_NATIVE:
        snprintf(text, sizeof(text), "<native fn>");
        break;
      default:
        snprintf(text, sizeof(text), "upvalue");
        break;
    }
    // Replace "<fn <script>>" with "<script>", as print does.
    if (strcmp(text, "<fn <script>>") == 0) strcpy(text, "<script>");
    bufferJsonString(buffer, text, (int)strlen(text), 64);
  }
}

// How many lines the program has printed so far: worker.js counts them in
// its print callback, which Emscripten calls as each line is completed.
EM_JS(int, printedLines, (void), {
  return Module.printedLines | 0;
});

// At most this many stack slots are recorded per step, the ones nearest the
// top. Deep stacks are rare in the small programs a debugger is for.
#define MAX_RECORDED_SLOTS 64

bool playgroundRecord(CallFrame* frame, uint8_t* ip) {
  if (stepCount == 0) collectFunctions(vm.frames[0].closure->function);
  if (stepCount >= maxSteps) {
    recordingFull = true;
    return false;
  }

  ObjFunction* function = frame->closure->function;
  int offset = (int)(ip - function->chunk.code);
  bufferPrintf(&recording, "%s{\"f\":%d,\"o\":%d,\"l\":%d,\"p\":%d,",
               stepCount == 0 ? "" : ",", functionIndex(function), offset,
               function->chunk.lines[offset], printedLines());

  // The value stack, bottom to top ("sb" is the index of the first slot
  // recorded, if the bottom had to be left out).
  int depth = (int)(vm.stackTop - vm.stack);
  int first = depth > MAX_RECORDED_SLOTS ? depth - MAX_RECORDED_SLOTS : 0;
  bufferPrintf(&recording, "\"sb\":%d,\"s\":[", first);
  for (int i = first; i < depth; i++) {
    if (i > first) bufferPrintf(&recording, ",");
    bufferValue(&recording, vm.stack[i]);
  }

  // The call frames, outermost first: [function, line, first stack slot].
  // For the frame that is running, `ip` is current; the others saved theirs
  // when they made their call, and it points just past that call.
  bufferPrintf(&recording, "],\"c\":[");
  for (int i = 0; i < vm.frameCount; i++) {
    CallFrame* f = &vm.frames[i];
    ObjFunction* fn = f->closure->function;
    int at = (f == frame) ? offset : (int)(f->ip - fn->chunk.code) - 1;
    bufferPrintf(&recording, "%s[%d,%d,%d]", i == 0 ? "" : ",",
                 functionIndex(fn), fn->chunk.lines[at],
                 (int)(f->slots - vm.stack));
  }
  bufferPrintf(&recording, "]}");
  stepCount++;
  return true;
}

// Runs a program while recording up to `limit` steps (instructions). The
// program stops when the recording is full. Read the result with
// clox_recording() and clox_recording_full().
EMSCRIPTEN_KEEPALIVE
int clox_debug(const char* source, int limit) {
  maxSteps = limit;
  stepCount = 0;
  recordingFull = false;
  recording.length = 0;
  bufferPrintf(&recording, "{\"steps\":[");

  playgroundRecording = true;
  initVM();
  InterpretResult result = interpret(source);
  playgroundRecording = false;

  // The function names, in the order the steps refer to them. collectFunctions
  // ran at the first step; with a compile error there were no steps.
  bufferPrintf(&recording, "],\"functions\":[");
  for (int i = 0; stepCount > 0 && i < functionCount; i++) {
    if (i > 0) bufferPrintf(&recording, ",");
    const char* name = functionName(functions[i]);
    bufferJsonString(&recording, name, (int)strlen(name), 64);
  }
  bufferPrintf(&recording, "]}");

  freeVM();
  fflush(stdout);
  fflush(stderr);
  return exitCode(result);
}

// The recording as a '\0'-terminated JSON string. It stays owned by this
// module; it lives until the module instance is thrown away.
EMSCRIPTEN_KEEPALIVE const char* clox_recording(void) {
  return recording.data;
}

EMSCRIPTEN_KEEPALIVE int clox_recording_full(void) {
  return recordingFull;
}
