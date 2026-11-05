#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Number of values a uint8_t can hold (0..255), e.g. the most locals a
// one-byte operand can address.
#define UINT8_COUNT (UINT8_MAX + 1)

// NAN_BOXING: store every Value in one 64-bit word (value.h). The book turns
// it on here with `#define NAN_BOXING`; this project passes it from the
// Makefile instead (on by default, NAN_BOXING=0 to turn it off), so that both
// representations are built and tested.
//
// Debug switches. Rather than editing this file (as the book does), build
// with `make trace`, which passes both as -D flags. That keeps debug output
// out of normal builds, where it would break the tests.
//
// DEBUG_PRINT_CODE       print the disassembled chunk after compiling
// DEBUG_TRACE_EXECUTION  print the stack and each instruction as it runs
// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION

#endif
