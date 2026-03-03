#ifndef TYPES_H
#define TYPES_H

/* =============================================================================
 * types.h — Fundamental type definitions
 * =============================================================================
 *
 * In a normal C program you'd do:
 *   #include <stdint.h>   for uint8_t, uint32_t, etc.
 *   #include <stddef.h>   for size_t, NULL
 *   #include <stdbool.h>  for bool, true, false
 *
 * But those headers are part of the C standard library, which doesn't exist
 * in our kernel — WE are the operating system. So we define everything here.
 *
 * The underlying types are chosen based on the x86 32-bit ABI:
 *   char       = 8 bits
 *   short      = 16 bits
 *   int        = 32 bits
 *   long       = 32 bits  (on 32-bit x86)
 *   long long  = 64 bits
 */

/* ---------------------------------------------------------------------------
 * Fixed-width integer types
 *
 * These guarantee an exact bit width regardless of platform. This matters
 * a lot in OS code — when you write to a hardware register at a specific
 * memory address, the CPU cares whether you're writing 8, 16, or 32 bits.
 * Using `int` would be ambiguous; uint8_t is precise.
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
 * On 32-bit x86, pointers are 32 bits, so size_t is 32 bits too.
 * --------------------------------------------------------------------------- */
typedef uint32_t size_t;

/* ---------------------------------------------------------------------------
 * uintptr_t — an integer type large enough to hold a pointer value
 *
 * Useful when you need to do arithmetic on addresses (e.g. "is this address
 * 4 KiB aligned?"). Casting a pointer to uintptr_t lets you do math on it.
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
