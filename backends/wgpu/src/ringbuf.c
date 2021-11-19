// SPDX-License-Identifier: Apache-2.0

#include "ringbuf.h"

// #define P_RINGBUF_DEBUG

#ifndef memcpy
  #define memcpy __builtin_memcpy
#endif

#define MAX(a,b) \
  ({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })

#define MIN(a,b) \
  ({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })

// T align2<T>(T x, T y) rounds up n to closest boundary w (w must be a power of two)
#define align2(n,w) ({ \
  assert(((w) & ((w) - 1)) == 0); /* alignment w is not a power of two */ \
  ((n) + ((w) - 1)) & ~((w) - 1); \
})

#ifdef P_RINGBUF_DEBUG
  #include <stdio.h>
  #include <assert.h>
  #define dlog(format, ...) ({ \
    fprintf(stderr, "\e[1;35m" "ringbuf>" "\e[0m " format " \e[2m" "(%s %d)" "\e[0m\n", \
      ##__VA_ARGS__, __FUNCTION__, __LINE__); \
    fflush(stderr); \
  })
  static void p_ringbuf_dump(p_ringbuf_t* b) {
    for (u32 startrow = 0, endrow = 0; startrow < b->cap; startrow = endrow) {
      endrow = MIN(startrow + 32, b->cap);
      for (u32 row = startrow; row < endrow; row++)
        fprintf(stderr, "\e[4m%-2d\e[0m ", row);
      fprintf(stderr, "\n");

      for (u32 row = startrow; row < endrow; row++) {
        u32 i = row;
        u8 v = *(u8*)&b->p[i];
        if (b->r == b->w) {
          fprintf(stderr, "\e[31m%-2u\e[0m ", v);
        } else if (
          (b->r < b->w && i >= b->r && i <= b->w) ||
          (b->w < b->r && (i <= b->w || i >= b->r))
        ) {
          fprintf(stderr, "\e[1m%-2u\e[0m ", v);
        } else {
          fprintf(stderr, "\e[2m%-2u\e[0m ", v);
        }
      }
      fprintf(stderr, "\n");

      for (u32 row = startrow; row < endrow; row++) {
        if (row == b->r) {
          fprintf(stderr, "\e[1;7;32m" "r" "\e[0m  ");
        } else if (row == b->w) {
          fprintf(stderr, "  \e[1;7;31m" "w" "\e[0m");
        } else {
          fprintf(stderr, "   ");
        }
      }
      fprintf(stderr, "\n");

      fprintf(stderr, "r %u  w %u\n", b->r, b->w);
    }
  }
#else
  #define dlog(format, ...) ((void)0)
  #define assert(...) ((void)0)
  #define p_ringbuf_dump(b) ((void)0)
#endif


void p_ringbuf_init(p_ringbuf_t* b, void* p, u32 cap) {
  b->r = 0;
  b->w = -1;
  b->cap = cap;
  b->len = 0;
  b->p = p;
}


void p_ringbuf_move(p_ringbuf_t* b, void* newp, u32 newcap) {
  dlog("move %p %u -> %p %u", b->p, b->cap, newp, newcap);

  if (newp != b->p)
    memcpy(newp, b->p, b->cap);

  if (b->w < b->r) {
    // wraps; move the read chunk over to the end so that we don't over-write it
    u32 len = b->cap - b->r;
    u32 newr = newcap - len;
    memcpy(&newp[newr], &newp[b->r], len);
    b->r = newr;
  }

  b->cap = newcap;
  b->p = newp;
  p_ringbuf_dump(b);
}


u32 p_ringbuf_write(p_ringbuf_t* b, const void* src, u32 len) {
  len = MIN(b->cap - b->len, len);
  u32 w = b->w + 1;
  void* dst = &b->p[w % b->cap];
  if (w + len > b->cap) { // wrap
    u32 len1 = b->cap - w;
    dlog("write %u:%u+%u:%u (%u+%u)", w,w+len1, 0,len-len1, len1, len-len1);
    memcpy(dst, src, len1);
    memcpy(b->p, &((const char*)src)[len1], len - len1);
  } else {
    dlog("write %u:%u (%u)", w, w+len, len);
    assert(w + len <= b->cap);
    memcpy(dst, src, len);
  }
  b->w = (b->w + len) % b->cap;
  b->len += len;
  p_ringbuf_dump(b);
  return (isize)len;
}


u32 p_ringbuf_read(p_ringbuf_t* b, void* dst, u32 len) {
  len = MIN(b->len, len);
  if (len == 0) // protects from div by zero on zero b->cap
    return 0;
  const void* src = &b->p[b->r];
  if (b->r + len > b->cap) { // wrap
    u32 len1 = b->cap - b->r;
    dlog("read %u:%u+%u:%u (%u+%u)", b->r,b->r+len1, 0,len-len1, len1, len-len1);
    memcpy(dst, src, len1);
    memcpy(&((char*)dst)[len1], b->p, len - len1);
  } else {
    dlog("read %u:%u (%u)", b->r, b->r+len, len);
    memcpy(dst, src, len);
  }
  b->r = (b->r + len) % b->cap;
  b->len -= len;
  p_ringbuf_dump(b);
  return len;
}


u32 p_ringbuf_writex(p_ringbuf_t* b, const void* src, u32 nbyte) {
  if (b->cap - b->len < nbyte)
    return 0;
  return p_ringbuf_write(b, src, nbyte);
}


u32 p_ringbuf_readx(p_ringbuf_t* b, void* dst, u32 nbyte) {
  if (b->len < nbyte)
    return 0;
  return p_ringbuf_read(b, dst, nbyte);
}
