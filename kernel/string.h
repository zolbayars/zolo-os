#ifndef STRING_H
#define STRING_H

/* =============================================================================
 * string.h — Kernel string and memory utilities
 * =============================================================================
 *
 * WHY WE REIMPLEMENT THESE
 *
 * Any C developer knows memset, memcpy, and strcmp — they're in <string.h>.
 * But that <string.h> is part of libc, which runs on top of an OS. Since we
 * ARE the OS, there's no libc available. We have to provide these ourselves.
 *
 * ANALOGY: It's like building the first road in a new city. You can't use a
 * delivery truck to bring construction materials — because trucks need roads
 * to drive on. You have to build the road by hand first, then the trucks can
 * come. memset and memcpy are our "first roads."
 *
 * WHY THE COMPILER NEEDS THEM EVEN IF WE DON'T CALL THEM EXPLICITLY
 *
 * Clang is smart: when you write `struct Foo f = {0}`, it doesn't emit a
 * loop — it calls memset under the hood (faster on real hardware). Same for
 * struct copies. If we don't provide these functions, the linker fails with
 * "undefined symbol: memset" even though we never wrote memset() in our code.
 */

#include "types.h"

/* Memory functions */
void* memset(void* dest, int val, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int   memcmp(const void* a, const void* b, size_t n);

/* String functions */
size_t strlen(const char* str);
int    strcmp(const char* a, const char* b);
int    strncmp(const char* a, const char* b, size_t n);
char*  strcpy(char* dest, const char* src);
char*  strncpy(char* dest, const char* src, size_t n);

#endif /* STRING_H */
