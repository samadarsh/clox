// Unit tests for the garbage collector (memory.c, chapter 26).
//
// A C variable is not a GC root: anything a test keeps must be pushed on the
// VM stack (or stored in a global) or a collection may free it. These tests
// run in all builds, including the stress build where the GC runs before
// every allocation.

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
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

static int countObjects(void) {
  int count = 0;
  for (Obj* object = vm.objects; object != NULL; object = object->next) {
    count++;
  }
  return count;
}

static bool isInObjectList(Obj* target) {
  for (Obj* object = vm.objects; object != NULL; object = object->next) {
    if (object == target) return true;
  }
  return false;
}

static ObjString* findInterned(const char* chars) {
  // Recompute the hash the same way copyString does, via a throwaway lookup
  // that never allocates: hash the characters with FNV-1a.
  uint32_t hash = 2166136261u;
  int length = (int)strlen(chars);
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)chars[i];
    hash *= 16777619;
  }
  return tableFindString(&vm.strings, chars, length, hash);
}

static bool getGlobal(const char* name, Value* value) {
  ObjString* key = findInterned(name);
  if (key == NULL) return false;
  return tableGet(&vm.globals, key, value);
}

// Ends a test: frees everything, checks every allocated byte was given back,
// then clears the VM's pointers without allocating (see test_object.c).
static void endTest(void) {
  freeVM();
  CHECK(vm.bytesAllocated == 0);
  vm.objects = NULL;
  initTable(&vm.globals);
  initTable(&vm.strings);
}

static void testUnreachableObjectsAreFreed(void) {
  initVM();
  int before = countObjects();
  size_t bytesBefore = vm.bytesAllocated;

  copyString("garbage one", 11);   // nothing refers to these
  copyString("garbage two", 11);
  CHECK(countObjects() >= before);  // (the stress build may already have freed them)

  collectGarbage();
  CHECK(countObjects() == before);
  CHECK(vm.bytesAllocated == bytesBefore);
  endTest();
}

static void testRootsSurvive(void) {
  initVM();
  int before = countObjects();

  ObjString* kept = copyString("kept", 4);
  push(OBJ_VAL(kept));               // the stack is a root
  copyString("dropped", 7);

  collectGarbage();
  CHECK(countObjects() == before + 1);
  CHECK(isInObjectList((Obj*)kept));
  CHECK(strcmp(kept->chars, "kept") == 0);   // still valid memory

  // The `clock` native and its name are reachable through vm.globals.
  Value clockFn;
  CHECK(getGlobal("clock", &clockFn) && IS_NATIVE(clockFn));

  pop();
  collectGarbage();
  CHECK(countObjects() == before);   // no root any more: freed
  endTest();
}

static void testStringTableIsWeak(void) {
  initVM();
  ObjString* weak = copyString("only in the intern table", 24);
  CHECK(findInterned("only in the intern table") == weak);

  // vm.strings refers to the string, but it doesn't count as a root...
  collectGarbage();
  // ...so it was freed, and tableRemoveWhite took it out of the table first.
  // (Otherwise the table would now hold a dangling pointer.)
  CHECK(findInterned("only in the intern table") == NULL);

  // Creating it again makes a brand new, valid string.
  ObjString* again = copyString("only in the intern table", 24);
  push(OBJ_VAL(again));
  CHECK(findInterned("only in the intern table") == again);
  CHECK(strcmp(again->chars, "only in the intern table") == 0);
  endTest();
}

static void testMarksClearedAndGrayStackEmpty(void) {
  initVM();
  push(OBJ_VAL(copyString("a", 1)));
  push(OBJ_VAL(copyString("b", 1)));
  collectGarbage();

  // After a collection every survivor is white again, ready for the next one.
  int marked = 0;
  for (Obj* object = vm.objects; object != NULL; object = object->next) {
    if (object->isMarked) marked++;
  }
  CHECK(marked == 0);
  CHECK(vm.grayCount == 0);
  endTest();
}

static void testNextGCDoublesLiveSize(void) {
  initVM();
#ifndef DEBUG_STRESS_GC
  // First threshold: 1 MB. (In the stress build, collections already ran
  // inside initVM() while it created the `clock` native, and each one sets
  // nextGC again.)
  CHECK(vm.nextGC == 1024 * 1024);
#endif
  push(OBJ_VAL(copyString("live data", 9)));
  collectGarbage();
  CHECK(vm.nextGC == vm.bytesAllocated * 2);
  endTest();
}

static void testGlobalsAreRoots(void) {
  initVM();
  // "a" and "b" are constants of the script, which is garbage once it has
  // run. "ab" is reachable from the global `keep`.
  CHECK(interpret("var keep = \"a\" + \"b\";") == INTERPRET_OK);
  collectGarbage();

  Value keep;
  CHECK(getGlobal("keep", &keep));
  CHECK(IS_STRING(keep) && strcmp(AS_CSTRING(keep), "ab") == 0);
  CHECK(findInterned("ab") == AS_STRING(keep));
  CHECK(findInterned("a") == NULL);   // freed with the script's constants
  CHECK(findInterned("b") == NULL);
  endTest();
}

static void testClosuresAndUpvaluesSurvive(void) {
  initVM();
  CHECK(interpret(
      "fun makeCounter() {\n"
      "  var count = 0;\n"
      "  fun inc() { count = count + 1; return count; }\n"
      "  return inc;\n"
      "}\n"
      "var c = makeCounter();\n"
      "var first = c();\n") == INTERPRET_OK);

  // makeCounter's frame is long gone; `count` lives only in a closed
  // upvalue, reachable as: globals -> closure `c` -> upvalue -> value.
  collectGarbage();
  collectGarbage();

  CHECK(interpret("var second = c(); var third = c();") == INTERPRET_OK);
  Value third;
  CHECK(getGlobal("third", &third) && IS_NUMBER(third) && AS_NUMBER(third) == 3);
  endTest();
}

static int countType(ObjType type) {
  int count = 0;
  for (Obj* object = vm.objects; object != NULL; object = object->next) {
    if (object->type == type) count++;
  }
  return count;
}

static void testClassObjectsMarkedAndFreed(void) {
  initVM();
  CHECK(interpret(
      "class Base { hello() { return \"hi\"; } }\n"
      "class Kid < Base { init() { this.field = \"f\" + \"!\"; } }\n"
      "var keepBound = Kid().hello;\n"       // bound method -> instance, closure
      "{ var dropped = Kid(); var b = dropped.hello; }\n"
      "var keepInstance = Kid();\n") == INTERPRET_OK);
  collectGarbage();

  // Reachable: Base and Kid (globals), the two kept instances, one bound
  // method. The dropped instance and its bound method are gone.
  CHECK(countType(OBJ_CLASS) == 2);
  CHECK(countType(OBJ_INSTANCE) == 2);
  CHECK(countType(OBJ_BOUND_METHOD) == 1);

  // What they point to survived: a field value built at runtime, and the
  // copied-down method called through the bound method.
  Value instance;
  CHECK(getGlobal("keepInstance", &instance) && IS_INSTANCE(instance));
  Value field;
  CHECK(tableGet(&AS_INSTANCE(instance)->fields, findInterned("field"),
                 &field));
  CHECK(IS_STRING(field) && strcmp(AS_CSTRING(field), "f!") == 0);
  CHECK(interpret("var r = keepBound();") == INTERPRET_OK);
  Value r;
  CHECK(getGlobal("r", &r) && IS_STRING(r) && strcmp(AS_CSTRING(r), "hi") == 0);

  // Drop everything: the instances, bound method and classes all go.
  CHECK(interpret("keepBound = nil; keepInstance = nil;"
                  "Kid = nil; Base = nil;") == INTERPRET_OK);
  collectGarbage();
  CHECK(countType(OBJ_CLASS) == 0);
  CHECK(countType(OBJ_INSTANCE) == 0);
  CHECK(countType(OBJ_BOUND_METHOD) == 0);
  // "init" is still interned: the VM keeps it as a root (vm.initString).
  CHECK(findInterned("init") == vm.initString);
  endTest();   // also checks every byte (fields/method tables too) came back
}

static void testGarbageLoopStaysBounded(void) {
  initVM();
  // Each batch builds a 1000-character string one character at a time:
  // ~1000 strings averaging 500 bytes, about 0.5 MB, of which only the final
  // string stays reachable. Ten batches allocate about 5 MB in total.
  CHECK(interpret("var s;") == INTERPRET_OK);
  size_t highest = 0;
  for (int batch = 0; batch < 10; batch++) {
    CHECK(interpret(
        "s = \"\";\n"
        "for (var i = 0; i < 1000; i = i + 1) s = s + \"x\";\n")
        == INTERPRET_OK);
    if (vm.bytesAllocated > highest) highest = vm.bytesAllocated;
  }

  // Without a collector the heap would hold all ~5 MB by now. With it, it
  // never holds much more than the 1 MB first threshold, and after a
  // collection only the live string and a few objects remain.
  CHECK(highest < 1500 * 1000);
  collectGarbage();
  CHECK(vm.bytesAllocated < 64 * 1024);
  CHECK(countObjects() < 20);
  endTest();
}

int main(void) {
  testUnreachableObjectsAreFreed();
  testRootsSurvive();
  testStringTableIsWeak();
  testMarksClearedAndGrayStackEmpty();
  testNextGCDoublesLiveSize();
  testGlobalsAreRoots();
  testClosuresAndUpvaluesSurvive();
  testClassObjectsMarkedAndFreed();
  testGarbageLoopStaysBounded();

  if (failures > 0) {
    printf("%d of %d checks failed\n", failures, checks);
    return 1;
  }
  printf("PASS (%d checks)\n", checks);
  return 0;
}
