#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Debug switches. Rather than editing this file (as the book does), build
// with `make trace`, which passes both as -D flags. That keeps debug output
// out of normal builds, where it would break the tests.
//
// DEBUG_PRINT_CODE       print the disassembled chunk after compiling
// DEBUG_TRACE_EXECUTION  print the stack and each instruction as it runs
// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION

#endif
