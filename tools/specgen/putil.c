// SPDX-License-Identifier: Apache-2.0

#include "putil.h"
#include <sys/stat.h>
#include <errno.h>


void* pmem_alloc(void** mnext, const void* mend, usize size, usize align) {
  u8* p = (u8*)palign2((uintptr_t)*mnext, align);
  void* next = p + size;
  if (next > mend)
    return NULL;
  *mnext = next;
  return p;
}


// pstrb appends src of length len to dst. Returns pointer to null char.
char* pstrb(char* dst, const char* dstend, const char* src, usize len) {
  if (dst >= dstend)
    return dst;
  len = MIN(len, (usize)(dstend - dst) - 1);
  memcpy(dst, src, len);
  dst[len] = 0;
  return &dst[len];
}


void* preadfile(const char* filename, void** mnext, const void* mend) {
  int fd = open(filename, O_RDONLY);
  if (fd < 0)
    return NULL;

  struct stat st;
  if (fstat(fd, &st) != 0)
    goto fail;
  void* dst = *mnext;
  usize len = MIN((usize)(mend - dst), (usize)st.st_size);
  isize n = read(fd, dst, len);
  if (n < 0)
    goto fail;
  close(fd);
  *mnext = dst + n;
  return dst;

fail: {}
  int _errno = errno;
  close(fd);
  errno = _errno;
  return NULL;
}
