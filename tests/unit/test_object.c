// Unit tests for values and heap objects.
//
// Memory note: these tests rely on `make test-asan`. On Linux, AddressSanitizer
// also runs LeakSanitizer when the program exits. LeakSanitizer reports memory
// that is still allocated but no longer reachable from any pointer. Each test
// ends with freeVM() and then initVM(), which sets vm.objects = NULL. If
// freeVM() forgot to free an object, nothing points to it any more, so it is
// reported as a leak and the test fails.

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

static int failures = 0;
static int checks = 0;

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

static int countObjects(void) {
  int count = 0;
  for (Obj* object = vm.objects; object != NULL; object = object->next) {
    count++;
  }
  return count;
}

static void testValueIsSixteenBytes(void) {
  // 4-byte tag + 4 bytes padding (so the 8-byte union is 8-byte aligned)
  // + 8-byte union. True on the 64-bit platforms CI runs on.
  CHECK(sizeof(Value) == 16);
  CHECK(sizeof(((Value*)0)->as) == 8);
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

  freeVM();
  initVM();  // drop the list head so anything left would show as a leak
}

static void testCopyStringInterns(void) {
  initVM();

  ObjString* a = copyString("lox", 3);
  ObjString* b = copyString("lox", 3);
  ObjString* c = copyString("lo", 2);
  CHECK(a == b);               // same characters -> the very same object
  CHECK(a != c);
  CHECK(countObjects() == 2);  // the second "lox" allocated nothing
  CHECK(valuesEqual(OBJ_VAL(a), OBJ_VAL(b)));
  CHECK(!valuesEqual(OBJ_VAL(a), OBJ_VAL(c)));
  CHECK(!valuesEqual(OBJ_VAL(a), NIL_VAL));

  // A slice of a longer buffer interns to the same string too.
  const char buffer[] = "xxloxxx";
  CHECK(copyString(&buffer[2], 3) == a);  // the 3 chars starting at index 2

  freeVM();
  initVM();
}

static void testTakeStringFreesDuplicate(void) {
  initVM();

  ObjString* original = copyString("abc", 3);

  // takeString owns `chars`. When the string already exists, it must free
  // `chars` and return the existing object. If it didn't free it, ASan's leak
  // check would report these 4 bytes; if it freed it twice, ASan would report
  // a double free.
  char* chars = ALLOCATE(char, 4);
  memcpy(chars, "abc", 4);
  ObjString* taken = takeString(chars, 3);
  CHECK(taken == original);
  CHECK(countObjects() == 1);

  freeVM();
  initVM();
}

static void testHashIsFnv1a(void) {
  initVM();
  // Published FNV-1a 32-bit test vectors.
  CHECK(copyString("", 0)->hash == 2166136261u);
  CHECK(copyString("a", 1)->hash == 0xe40c292cu);
  CHECK(copyString("foobar", 6)->hash == 0xbf9cf968u);
  freeVM();
  initVM();
}

static void testEveryObjectIsInTheList(void) {
  initVM();
  CHECK(vm.objects == NULL);

  ObjString* first = copyString("one", 3);
  ObjString* second = copyString("two", 3);
  char* chars = ALLOCATE(char, 6);
  memcpy(chars, "three", 6);
  ObjString* third = takeString(chars, 5);  // takes ownership of `chars`

  CHECK(countObjects() == 3);
  // New objects are pushed on the front, so the list is newest first.
  CHECK(vm.objects == (Obj*)third);
  CHECK(vm.objects->next == (Obj*)second);
  CHECK(vm.objects->next->next == (Obj*)first);
  CHECK(vm.objects->next->next->next == NULL);

  freeVM();
  initVM();
}

static void testInterpretTracksRuntimeStrings(void) {
  initVM();

  // Compiling creates 3 constant strings; running creates 2 more, one per +.
  // All 5 must be in the list so freeVM() can free them.
  silence();
  InterpretResult ok = interpret("print \"a\" + \"b\" + \"c\";");
  unsilence();
  CHECK(ok == INTERPRET_OK);
  CHECK(countObjects() == 5);

  // Objects survive a runtime error too, and are still freed by freeVM().
  silence();
  InterpretResult error = interpret("print \"x\" + 1;");
  unsilence();
  CHECK(error == INTERPRET_RUNTIME_ERROR);
  CHECK(countObjects() == 6);

  // Interning: running the first program again reuses all 5 strings.
  silence();
  interpret("print \"a\" + \"b\" + \"c\";");
  unsilence();
  CHECK(countObjects() == 6);

  freeVM();
  initVM();
}

int main(void) {
  testValueIsSixteenBytes();
  testTagsAndPayloads();
  testValuesEqual();
  testCopyStringOwnsItsCharacters();
  testCopyStringInterns();
  testTakeStringFreesDuplicate();
  testHashIsFnv1a();
  testEveryObjectIsInTheList();
  testInterpretTracksRuntimeStrings();

  if (failures > 0) {
    printf("%d of %d checks failed\n", failures, checks);
    return 1;
  }
  printf("PASS (%d checks)\n", checks);
  return 0;
}
