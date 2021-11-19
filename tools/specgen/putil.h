// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "../common.h"

void* pmem_alloc(void** mnext, const void* mend, usize size, usize align);

// pstrf appends printf-compatible format to dst. Returns pointer to null char.
// char* pstrf(char* dst, const char* dstend, const char* fmt, ...)
#define pstrf(dst, dstend, fmt, ...) ({                             \
  int avail = (int)((dstend) - (dst));                              \
  (dst) + MIN(avail, snprintf((dst), avail, (fmt), ##__VA_ARGS__)); \
})

// pstrc appends the character c & a null byte to dst.
// Returns pointer to null char.
inline static char* pstrc(char* dst, const char* dstend, char c) {
  if (dst+1 < dstend) {
    *dst++ = c;
    *dst = 0;
  }
  return dst;
}

// pstrcx appends the character c to dst, but does not set null.
// Returns pointer to next char.
inline static char* pstrcx(char* dst, const char* dstend, char c) {
  if (dst < dstend)
    *dst++ = c;
  return dst;
}

// pstrb appends src of length len to dst. Returns pointer to null char.
char* pstrb(char* dst, const char* dstend, const char* src, usize len);

// pstrs appends null-terminated string src to dst. Returns pointer to null char.
inline static char* pstrs(char* dst, const char* dstend, const char* src) {
  // return stpncpy(dst, src, dstend - dst);
  return pstrb(dst, dstend, src, strlen(src));
}

// readfile reads the contents of filename into memory at mnext.
// Upon return, mnext is set to the end of the loaded data.
// Returns pointer to the beginning of the loaded data (=*mnext when called)
// which is of length (*mnext - returnvalue).
// Returns NULL on failure (check errno.)
void* preadfile(const char* filename, void** mnext, const void* mend);
