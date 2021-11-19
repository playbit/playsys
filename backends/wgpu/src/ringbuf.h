// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <playsys.h>

typedef struct p_ringbuf {
  u32 r, w;     // read & write offsets into p
  u32 cap, len; // capacity and used length of p, respectively (in bytes)
  char* p;      // memory buffer of size cap
} p_ringbuf_t;

// p_ringbuf_init initializes b with memory at p with capacity of cap bytes
PSYS_EXTERN void p_ringbuf_init(p_ringbuf_t* b, void* p, u32 cap);

// p_ringbuf_move moves the buffer to a different memory location.
// For example to grow the buffer. newp may be the same address as b->p.
PSYS_EXTERN void p_ringbuf_move(p_ringbuf_t* b, void* newp, u32 newcap);

// p_ringbuf_write behaves like write(); return number of bytes written
PSYS_EXTERN u32 p_ringbuf_write(p_ringbuf_t*, const void* src, u32 nbyte);

// p_ringbuf_read behaves like read(); return number of bytes read
PSYS_EXTERN u32 p_ringbuf_read(p_ringbuf_t*, void* dst, u32 nbyte);

// p_ringbuf_writex either writes entire src, or writes nothing if there's no space
PSYS_EXTERN u32 p_ringbuf_writex(p_ringbuf_t*, const void* src, u32 nbyte);

// p_ringbuf_readx either reads nbytes, or nothing if p->len < nbyte
PSYS_EXTERN u32 p_ringbuf_readx(p_ringbuf_t*, void* dst, u32 nbyte);
