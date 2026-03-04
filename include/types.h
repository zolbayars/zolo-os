#ifndef TYPES_H
#define TYPES_H

/* =============================================================================
 * types.h — Fundamental type definitions
 * =============================================================================
 *
 * WHY THIS FILE EXISTS
 *
 * In a normal C program you'd write `#include <stdint.h>` and get types like
 * uint8_t and uint32_t for free. Those headers are provided by the C standard
 * library (libc) — the same library that also gives you printf, malloc, etc.
 *
 * But libc is a program that runs ON TOP of an operating system. It calls the
 * OS for memory, for file access, for everything. We ARE the operating system,
 * so there's nobody above us to call. We can't use libc.
 *
 * ANALOGY: It's like being the first person to settle a new island. You can't
 * order from Amazon because there's no delivery infrastructure yet — you have
 * to build everything from scratch. This file is us building our own toolbox
 * before we can build anything else.
 *
 * WHY FIXED-WIDTH TYPES MATTER
 *
 * In regular app code, `int` is fine because you don't care if it's 32 or 64
 * bits — the OS and hardware handle the details. But when we write to a hardware
 * register at a specific memory address, the CPU cares deeply. Writing 4 bytes
 * to a register that expects 1 byte corrupts neighbouring registers.
 *
 * ANALOGY: Imagine a recipe that says "add sugar." A baker might add a cup,
 * a tablespoon, or a gram — ambiguous. "Add 5 grams" is precise. uint8_t means
 * exactly 8 bits, every time, on every machine. No ambiguity.
 *
 * The sizes below are chosen for the x86 32-bit ABI (Application Binary
 * Interface) — the agreed-upon contract for how types map to memory on x86:
 *   char       = 8 bits
 *   short      = 16 bits
 *   int        = 32 bits
 *   long       = 32 bits  (on 32-bit x86; would be 64 on 64-bit)
 *   long long  = 64 bits
 */

/* ---------------------------------------------------------------------------
 * Fixed-width integer types
 *
 * The naming convention: u = unsigned (no negative values), int = integer,
 * the number = exact bit width, _t = "type" (a C convention for typedefs).
 *
 * So uint8_t  = unsigned integer, exactly 8 bits  → values 0 to 255
 *    uint16_t = unsigned integer, exactly 16 bits → values 0 to 65535
 *    int32_t  = SIGNED integer, exactly 32 bits   → values -2B to +2B
 * --------------------------------------------------------------------------- */
typedef unsigned char       uint8_t;    /*  8-bit unsigned: 0 to 255           */
typedef unsigned short      uint16_t;   /* 16-bit unsigned: 0 to 65535         */
typedef unsigned int        uint32_t;   /* 32-bit unsigned: 0 to 4,294,967,295 */
typedef unsigned long long  uint64_t;   /* 64-bit unsigned                     */

typedef signed char         int8_t;     /*  8-bit signed: -128 to 127          */
typedef signed short        int16_t;    /* 16-bit signed: -32768 to 32767      */
typedef signed int          int32_t;    /* 32-bit signed: -2B to 2B            */
typedef signed long long    int64_t;    /* 64-bit signed                       */

/* ---------------------------------------------------------------------------
 * size_t — used for sizes and counts (e.g. memset, memcpy arguments)
 *
 * Represents "how big is this thing in bytes?" or "how many items are there?"
 * It's defined to be large enough to hold any possible memory size on this
 * platform. On 32-bit x86, the largest address is 32 bits, so size_t is too.
 * --------------------------------------------------------------------------- */
typedef uint32_t size_t;

/* ---------------------------------------------------------------------------
 * uintptr_t — an integer large enough to hold a memory address
 *
 * Pointers in C are memory addresses, but you can't do arithmetic directly
 * on a pointer type (e.g. "is this address divisible by 4096?"). Casting to
 * uintptr_t converts the address to a plain integer so you can do math on it.
 *
 * Example: (uintptr_t)ptr % 4096 == 0 checks if ptr is 4 KiB aligned.
 * --------------------------------------------------------------------------- */
typedef uint32_t uintptr_t;

/* ---------------------------------------------------------------------------
 * bool, true, false
 *
 * C99 added these via <stdbool.h>. We replicate them here.
 * --------------------------------------------------------------------------- */
typedef uint8_t bool;
#define true  1
#define false 0

/* ---------------------------------------------------------------------------
 * NULL — the null pointer constant
 * --------------------------------------------------------------------------- */
#define NULL ((void*)0)

#endif /* TYPES_H */
