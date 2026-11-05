// Unit tests for values and heap objects.
//
// GC note: since chapter 26 any allocation can run the garbage collector,
// which frees every object it can't reach from a root. A C variable is not a
// root, so tests push the objects they keep using onto the VM stack. In the
// stress build (GC on every allocation) a missing push here would show up as
// a use-after-free.
//
// Memory note: these tests rely on `make test-asan`. On Linux, AddressSanitizer
// also runs LeakSanitizer when the program exits. LeakSanitizer reports memory
// that is still allocated but no longer reachable from any pointer. Each test
// starts with initVM() and ends with endTest(), which frees everything and then
// clears the VM's pointers (vm.objects, the tables). If freeVM() forgot to free
// an object, nothing points to it any more, so it is reported as a leak and
// the test fails.

// dup/dup2 are POSIX, not C99, so ask the headers to declare them.
#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"


#define CHECK(cond) \
    do { \
      checks++; \
      if (!(cond)) { \
        failures++; \
        fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      } \
    } while (false)

// Temporarily send stdout and stderr to /dev/null, so interpret()'s output
// and error messages don't clutter the test report.
static int savedOut, savedErr;

static void silence(void) {
  fflush(stdout);
  fflush(stderr);
  savedOut = dup(STDOUT_FILENO);
  savedErr = dup(STDERR_FILENO);
  int devNull = open("/dev/null", O_WRONLY);
  dup2(devNull, STDOUT_FILENO);
  dup2(devNull, STDERR_FILENO);
  close(devNull);
}

static void unsilence(void) {
  fflush(stdout);
  fflush(stderr);
  dup2(savedOut, STDOUT_FILENO);
  dup2(savedErr, STDERR_FILENO);
  close(savedOut);
  close(savedErr);
}

// Ends a test. freeVM() releases everything; then the VM's pointers are
// cleared WITHOUT allocating anything, so whatever freeVM() missed becomes
// unreachable. (Calling initVM() here, as this file did before chapter 24,
// now allocates the `clock` native, and the next test's initVM() would
// overwrite and leak it.)
static int failures;
static int checks;

static void endTest(void) {
  freeVM();
  // Every byte allocated through reallocate() must have been given back.
  // This also proves each caller passed the right old size: the GC's byte
  // count (vm.bytesAllocated) depends on it.
  checks++;
  if (vm.bytesAllocated != 0) {
    failures++;
    fprintf(stderr, "endTest: %zu bytes still counted as allocated\n",
            vm.bytesAllocated);
  }
  vm.objects = NULL;
  initTable(&vm.globals);
  initTable(&vm.strings);
}

static int countObjects(void) {
  int count = 0;
  for (Obj* object = vm.objects; object != NULL; object = object->next) {
    count++;
  }
  return count;
}

#ifdef NAN_BOXING
static void testValueSize(void) {
  // NaN boxing: a Value is a single 64-bit word.
  CHECK(sizeof(Value) == 8);
  CHECK(sizeof(Value) == sizeof(double));
}

// The exact bit patterns, as drawn in value.h. Only NaN boxing has them.
static void testNanBoxingBits(void) {
  CHECK(NIL_VAL   == 0x7ffc000000000001);
  CHECK(FALSE_VAL == 0x7ffc000000000002);
  CHECK(TRUE_VAL  == 0x7ffc000000000003);

  // A number is stored as its own IEEE 754 bits, unchanged.
  CHECK(NUMBER_VAL(1.0) == 0x3ff0000000000000);
  CHECK(NUMBER_VAL(-2.0) == 0xc000000000000000);

  // An object: sign bit + QNAN + the pointer in the low 48 bits.
  initVM();
  ObjString* s = copyString("boxed", 5);
  Value v = OBJ_VAL(s);
  CHECK((v & 0xfffc000000000000) == 0xfffc000000000000);
  CHECK((v & 0x0000ffffffffffff) == (uint64_t)(uintptr_t)s);
  CHECK(AS_OBJ(v) == (Obj*)s);
  endTest();
}
#else
static void testValueSize(void) {
  // 4-byte tag + 4 bytes padding (so the 8-byte union is 8-byte aligned)
  // + 8-byte union. True on the 64-bit platforms CI runs on.
  CHECK(sizeof(Value) == 16);
  CHECK(sizeof(((Value*)0)->as) == 8);
}
#endif

// Both representations must agree on these.
static void testSpecialNumbers(void) {
  double zero = 0.0;
  Value nan = NUMBER_VAL(zero / zero);    // the CPU's own NaN
  Value inf = NUMBER_VAL(1.0 / zero);
  Value negZero = NUMBER_VAL(-zero);

  // A real NaN, infinity and -0 are numbers, never nil/bool/object. (With
  // NaN boxing that depends on QNAN including bit 50, see value.h.)
  CHECK(IS_NUMBER(nan) && !IS_OBJ(nan) && !IS_NIL(nan) && !IS_BOOL(nan));
  CHECK(IS_NUMBER(inf) && !IS_OBJ(inf));
  CHECK(IS_NUMBER(negZero));
  CHECK(IS_NUMBER(NUMBER_VAL(-zero / zero)));   // NaN with the sign bit set

  // Lox equality on numbers is C's ==: NaN isn't equal to itself, even with
  // the very same bits, and 0 equals -0 although the bits differ.
  CHECK(!valuesEqual(nan, nan));
  CHECK(valuesEqual(negZero, NUMBER_VAL(0)));
  CHECK(valuesEqual(inf, inf));

  // Pointers survive the round trip through a Value.
  initVM();
  ObjString* s = copyString("ptr", 3);
  push(OBJ_VAL(s));
  CHECK(IS_OBJ(vm.stackTop[-1]) && AS_OBJ(vm.stackTop[-1]) == (Obj*)s);
  CHECK(IS_STRING(vm.stackTop[-1]) && AS_STRING(vm.stackTop[-1]) == s);
  pop();
  endTest();
}

static void testTagsAndPayloads(void) {
  Value t = BOOL_VAL(true);
  Value n = NUMBER_VAL(1.5);
  Value nil = NIL_VAL;

  CHECK(IS_BOOL(t) && !IS_NUMBER(t) && !IS_NIL(t) && !IS_OBJ(t));
  CHECK(AS_BOOL(t) == true);
  CHECK(IS_NUMBER(n) && AS_NUMBER(n) == 1.5);
  CHECK(IS_NIL(nil) && !IS_BOOL(nil));
}

static void testValuesEqual(void) {
  CHECK(valuesEqual(NUMBER_VAL(1), NUMBER_VAL(1)));
  CHECK(!valuesEqual(NUMBER_VAL(1), NUMBER_VAL(2)));
  CHECK(valuesEqual(NIL_VAL, NIL_VAL));
  CHECK(valuesEqual(BOOL_VAL(false), BOOL_VAL(false)));
  CHECK(!valuesEqual(BOOL_VAL(true), BOOL_VAL(false)));
  // Different types are never equal, even "similar" ones.
  CHECK(!valuesEqual(NIL_VAL, BOOL_VAL(false)));
  CHECK(!valuesEqual(NUMBER_VAL(0), BOOL_VAL(false)));
  CHECK(!valuesEqual(NUMBER_VAL(0), NIL_VAL));
}

static void testCopyStringOwnsItsCharacters(void) {
  initVM();

  char source[] = "hello world";
  ObjString* s = copyString(source, 5);   // just "hello"
  CHECK(s->length == 5);
  CHECK(strcmp(s->chars, "hello") == 0);   // '\0'-terminated
  CHECK(s->chars != source);               // a copy, not a pointer into source

  source[0] = 'J';                          // changing the source...
  CHECK(s->chars[0] == 'h');               // ...does not change the string

  endTest();
}

static void testCopyStringInterns(void) {
  initVM();
  // initVM() itself creates objects (the "clock" name and native), so
  // count relative to what's there after it.
  int before = countObjects();

  ObjString* a = copyString("lox", 3);
  push(OBJ_VAL(a));  // keep it alive across the allocations below
  ObjString* b = copyString("lox", 3);
  ObjString* c = copyString("lo", 2);
  CHECK(a == b);               // same characters -> the very same object
  CHECK(a != c);
  CHECK(countObjects() == before + 2);  // the second "lox" allocated nothing
  CHECK(valuesEqual(OBJ_VAL(a), OBJ_VAL(b)));
  CHECK(!valuesEqual(OBJ_VAL(a), OBJ_VAL(c)));
  CHECK(!valuesEqual(OBJ_VAL(a), NIL_VAL));

  // A slice of a longer buffer interns to the same string too.
  const char buffer[] = "xxloxxx";
  CHECK(copyString(&buffer[2], 3) == a);  // the 3 chars starting at index 2

  endTest();
}

static void testTakeStringFreesDuplicate(void) {
  initVM();
  int before = countObjects();

  ObjString* original = copyString("abc", 3);
  push(OBJ_VAL(original));

  // takeString owns `chars`. When the string already exists, it must free
  // `chars` and return the existing object. If it didn't free it, ASan's leak
  // check would report these 4 bytes; if it freed it twice, ASan would report
  // a double free.
  char* chars = ALLOCATE(char, 4);
  memcpy(chars, "abc", 4);
  ObjString* taken = takeString(chars, 3);
  CHECK(taken == original);
  CHECK(countObjects() == before + 1);

  endTest();
}

static void testHashIsFnv1a(void) {
  initVM();
  // Published FNV-1a 32-bit test vectors.
  CHECK(copyString("", 0)->hash == 2166136261u);
  CHECK(copyString("a", 1)->hash == 0xe40c292cu);
  CHECK(copyString("foobar", 6)->hash == 0xbf9cf968u);
  endTest();
}

static void testEveryObjectIsInTheList(void) {
  initVM();
  Obj* oldHead = vm.objects;   // the objects initVM() created
  int before = countObjects();

  ObjString* first = copyString("one", 3);
  push(OBJ_VAL(first));
  ObjString* second = copyString("two", 3);
  push(OBJ_VAL(second));
  char* chars = ALLOCATE(char, 6);
  memcpy(chars, "three", 6);
  ObjString* third = takeString(chars, 5);  // takes ownership of `chars`

  CHECK(countObjects() == before + 3);
  // New objects are pushed on the front, so the list is newest first.
  CHECK(vm.objects == (Obj*)third);
  CHECK(vm.objects->next == (Obj*)second);
  CHECK(vm.objects->next->next == (Obj*)first);
  CHECK(vm.objects->next->next->next == oldHead);

  endTest();
}

// Exact object counts only hold when no collection runs in between, so this
// test is skipped in the stress build (GC on every allocation). The custom
// .lox GC tests and test_gc.c cover what happens when collections do run.
static void testInterpretTracksRuntimeStrings(void) {
#ifdef DEBUG_STRESS_GC
  return;
#endif
  initVM();
  int before = countObjects();

  // Compiling creates the script's ObjFunction and 3 constant strings.
  // Running creates the script's ObjClosure, and 2 strings, one per +.
  // All 7 must be in the list so freeVM() can free them.
  silence();
  InterpretResult ok = interpret("print \"a\" + \"b\" + \"c\";");
  unsilence();
  CHECK(ok == INTERPRET_OK);
  CHECK(countObjects() == before + 7);

  // Objects survive a runtime error too, and are still freed by freeVM().
  // New: a function, the string "x" and a closure.
  silence();
  InterpretResult error = interpret("print \"x\" + 1;");
  unsilence();
  CHECK(error == INTERPRET_RUNTIME_ERROR);
  CHECK(countObjects() == before + 10);

  // Interning: running the first program again reuses all 5 strings, so
  // only a new function and closure are created.
  silence();
  interpret("print \"a\" + \"b\" + \"c\";");
  unsilence();
  CHECK(countObjects() == before + 12);

  endTest();
}

static int countType(ObjType type) {
  int count = 0;
  for (Obj* object = vm.objects; object != NULL; object = object->next) {
    if (object->type == type) count++;
  }
  return count;
}

static void testClosuresAndUpvalues(void) {
  initVM();

  // Two closures capture one variable: they must share ONE ObjUpvalue.
  // After make() returns, nothing is left open (the upvalue was closed).
  silence();
  InterpretResult result = interpret(
      "var get; var set;\n"
      "fun make() {\n"
      "  var shared = 1;\n"
      "  fun g() { return shared; }\n"
      "  fun s(v) { shared = v; }\n"
      "  get = g; set = s;\n"
      "}\n"
      "make();\n"
      "set(42);\n"
      "print get();\n");
  unsilence();
  CHECK(result == INTERPRET_OK);
  CHECK(countType(OBJ_UPVALUE) == 1);
  CHECK(vm.openUpvalues == NULL);

  // Find that upvalue: closed means `location` points at its own `closed`
  // field, which now holds the variable's last value.
  ObjUpvalue* upvalue = NULL;
  for (Obj* object = vm.objects; object != NULL; object = object->next) {
    if (object->type == OBJ_UPVALUE) upvalue = (ObjUpvalue*)object;
  }
  CHECK(upvalue != NULL);
  CHECK(upvalue->location == &upvalue->closed);
  CHECK(IS_NUMBER(upvalue->closed) && AS_NUMBER(upvalue->closed) == 42);

  // Four closures (script, make, g, s) and their 4 functions exist.
  CHECK(countType(OBJ_CLOSURE) == 4);
  CHECK(countType(OBJ_FUNCTION) == 4);

  endTest();
}

int main(void) {
  testValueSize();
#ifdef NAN_BOXING
  testNanBoxingBits();
#endif
  testSpecialNumbers();
  testTagsAndPayloads();
  testValuesEqual();
  testCopyStringOwnsItsCharacters();
  testCopyStringInterns();
  testTakeStringFreesDuplicate();
  testHashIsFnv1a();
  testEveryObjectIsInTheList();
  testInterpretTracksRuntimeStrings();
  testClosuresAndUpvalues();

  if (failures > 0) {
    printf("%d of %d checks failed\n", failures, checks);
    return 1;
  }
  printf("PASS (%d checks)\n", checks);
  return 0;
}
