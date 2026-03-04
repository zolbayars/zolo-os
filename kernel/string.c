#include "string.h"

/* =============================================================================
 * string.c — Kernel string and memory utility implementations
 * =============================================================================
 *
 * All implementations here are intentionally simple: byte-by-byte loops,
 * easy to read and verify. A production kernel like Linux uses hand-tuned
 * assembly that processes 16 or 32 bytes at a time using SIMD instructions
 * (think: special CPU instructions that work like parallel lanes on a highway).
 * For a learning OS, correctness beats performance every time.
 */

/* ---------------------------------------------------------------------------
 * memset — Fill a block of memory with a repeated byte value
 *
 * Classic use: zero out a buffer before use to avoid garbage values
 *   memset(my_buffer, 0, sizeof(my_buffer));
 *
 * Also used by the compiler automatically when you write:
 *   struct foo s = {0};   // zeroes all fields
 *
 * @dest: start address to write to
 * @val:  byte value to fill with (passed as int but only low 8 bits used)
 * @n:    number of bytes to fill
 * Returns: dest (allows chaining, e.g. passing memset's result to another fn)
 * --------------------------------------------------------------------------- */
void* memset(void* dest, int val, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    while (n--) {
        *d++ = (uint8_t)val;
    }
    return dest;
}

/* ---------------------------------------------------------------------------
 * memcpy — Copy n bytes from src to dest (non-overlapping regions only)
 *
 * Used by the compiler when copying structs by value:
 *   struct foo a = b;   // calls memcpy
 *
 * IMPORTANT: src and dest must NOT overlap. If they might overlap, use
 * memmove instead (it handles overlaps safely).
 *
 * @dest: destination address
 * @src:  source address
 * @n:    number of bytes to copy
 * Returns: dest
 * --------------------------------------------------------------------------- */
void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t*       d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

/* ---------------------------------------------------------------------------
 * memmove — Copy n bytes from src to dest, handles overlapping regions
 *
 * ANALOGY: Imagine sliding books along a shelf to close a gap. If you move
 * each book left one slot, you must start from the LEFT side — otherwise
 * you'd overwrite books you haven't moved yet. If you move them RIGHT, you
 * start from the RIGHT. memmove figures out which direction is safe.
 *
 * We use this in the VGA driver when scrolling the screen: the entire buffer
 * shifts up by one row (row 1 → row 0, row 2 → row 1, ...). Because source
 * and destination overlap, memcpy would corrupt data mid-copy.
 *
 *   - dest < src: copy forward (left-to-right, safe — we read before we overwrite)
 *   - dest > src: copy backward (right-to-left, safe — same reason, other direction)
 * --------------------------------------------------------------------------- */
void* memmove(void* dest, const void* src, size_t n) {
    uint8_t*       d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    if (d < s) {
        /* dest is before src — safe to copy forward */
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        /* dest is after src — copy backward to avoid overwriting src data */
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    /* if d == s, nothing to do */
    return dest;
}

/* ---------------------------------------------------------------------------
 * memcmp — Compare two blocks of memory byte by byte
 *
 * Returns:
 *   0   if the first n bytes of a and b are identical
 *  <0   if the first differing byte in a is less than in b
 *  >0   if the first differing byte in a is greater than in b
 * --------------------------------------------------------------------------- */
int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* pb = (const uint8_t*)b;
    while (n--) {
        if (*pa != *pb) {
            return (int)*pa - (int)*pb;
        }
        pa++;
        pb++;
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * strlen — Count the number of characters in a string (not including '\0')
 *
 * Example: strlen("hello") == 5
 * --------------------------------------------------------------------------- */
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

/* ---------------------------------------------------------------------------
 * strcmp — Compare two null-terminated strings lexicographically
 *
 * Returns:
 *   0   if strings are equal
 *  <0   if a comes before b
 *  >0   if a comes after b
 *
 * We'll use this in the shell to match command names.
 * --------------------------------------------------------------------------- */
int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* ---------------------------------------------------------------------------
 * strncmp — Like strcmp but compares at most n characters
 *
 * Safer than strcmp when you can't guarantee null termination, or when
 * you only want to compare a prefix.
 * --------------------------------------------------------------------------- */
int strncmp(const char* a, const char* b, size_t n) {
    while (n && *a && *b && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* ---------------------------------------------------------------------------
 * strcpy — Copy a null-terminated string (including the '\0')
 *
 * WARNING: Does not check bounds. dest must be large enough to hold src.
 * --------------------------------------------------------------------------- */
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++) != '\0') {}
    return dest;
}

/* ---------------------------------------------------------------------------
 * strncpy — Copy at most n characters from src to dest
 *
 * If src is shorter than n, pads the rest of dest with '\0'.
 * If src is n or more characters, dest will NOT be null-terminated —
 * the caller must ensure dest[n] = '\0' if needed.
 * --------------------------------------------------------------------------- */
char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n > 0 && *src != '\0') {
        *d++ = *src++;
        n--;
    }
    while (n > 0) {
        *d++ = '\0';
        n--;
    }
    return dest;
}
