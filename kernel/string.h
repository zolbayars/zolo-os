#ifndef STRING_H
#define STRING_H

/* =============================================================================
 * string.h — Kernel string and memory utilities
 * =============================================================================
 *
 * In a normal program, these come from <string.h> in libc. We don't have
 * libc, so we implement the ones our kernel needs.
 *
 * Why do we need memset and memcpy even if we don't call them directly?
 * Clang emits calls to these functions automatically for:
 *   - Initializing a struct:       my_struct s = {0};  → calls memset
 *   - Copying a struct:            s1 = s2;            → calls memcpy
 *   - Initializing a char array:   char buf[64] = {};  → calls memset
 *
 * If we don't provide them, the linker will fail with "undefined symbol".
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
