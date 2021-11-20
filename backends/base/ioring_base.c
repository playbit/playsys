// SPDX-License-Identifier: Apache-2.0

// ioring driver
//
// Shared application/driver submission and completion ring pairs, for supporting
// fast & efficient IO. Based on and compatible with Linux io_uring.
//
// Notes on the read/write ordering memory barriers that are matched between
// the application and driver side:
//
//   After the application reads the CQ ring tail, it must use an appropriate
//   mbarrier_r() to pair with the mbarrier_w() the driver uses before writing
//   the tail. It also needs a mbarrier() before updating CQ head (ordering the
//   entry loads with the head stores), pairing with an implicit barrier
//   through a control-dependency in io_get_cqe (mbarrier_w()). Failure to do so
//   could lead to reading invalid CQ entries.
//
//   Likewise, the application must use an appropriate mbarrier_w() before
//   writing the SQ tail (ordering SQ entry stores with the tail store),
//   which pairs with mbarrier_r() in io_get_sqring (mbarrier_w() to store the
//   tail will do). And it needs a barrier ordering the SQ head load before
//   writing new SQ entries (mbarrier_r() to read head will do).
//
// When using the SQ poll thread (IORING_SETUP_SQPOLL), the application
// needs to check the SQ flags for IORING_SQ_NEED_WAKEUP *after* updating
// the SQ tail; a full memory barrier mbarrier() is needed between.

#include "sys_impl.h"
#include <stdlib.h>
#include <sys/mman.h>


#define IORING_MAX_ENTRIES              32768 // value from Linux 5.15
#define IORING_MAX_CQ_ENTRIES           (2 * IORING_MAX_ENTRIES)
#define IORING_SQPOLL_CAP_ENTRIES_VALUE 8

static_assert(IORING_MAX_ENTRIES == ceil_pow2(IORING_MAX_ENTRIES), "must be power of 2");


// flags for ioringctx_t, in addition to p_ioring_setupflag
typedef enum ioring_ctxflag {
  // starts after the last P_IORING_SETUP_ flag
  IORING_CTX_INIT = 1 << 16, // initialized
} ioring_ctxflag_t;


// head & tail of a ring
typedef struct ioring {
  u32 head _p_cacheline_aligned;
  u32 tail _p_cacheline_aligned;
} ioring_t;

// iorings_t: data shared with the application.
// The offsets to the member fields are published through struct ioring_sqoffsets_t
// when calling io_uring_setup.
typedef struct iorings {
  // sq & cq holds head and tail offsets into the ring; the offsets need to be
  // masked to get valid indices.
  // The driver      controls head of the sq ring and the tail of the cq ring,
  // the application controls tail of the sq ring and the head of the cq ring.
  ioring_t sq, cq;

  // {sq,cq}_ring_mask: bitmasks to apply to head and tail offsets
  // (constant, equals ring_entries - 1)
  u32 sq_ring_mask, cq_ring_mask;

  // Ring sizes (constant, power of 2)
  u32 sq_ring_entries, cq_ring_entries;

  // sq_dropped: number of invalid entries dropped by the driver due to invalid
  // index stored in array.
  // Written by the driver, read-only by application.
  // (i.e. get number of "new events" by comparing to cached value).
  // After a new SQ head value was read by the application this counter includes
  // all submissions dropped reaching the new SQ head (and possibly more).
  u32 sq_dropped;

  // sq_flags: runtime SQ flags
  // Written by the driver, read-only by application.
  // The application needs a full memory barrier before checking for
  // IORING_SQ_NEED_WAKEUP after updating the sq tail.
  u32 sq_flags;

  // cq_flags: runtime CQ flags
  // Written by the application, read-only by driver.
  u32 cq_flags;

  // cq_overflow: number of completion events lost because the queue was full;
  // this should be avoided by the application by making sure there are not more
  // requests pending than there is space in the completion queue.
  // Written by the driver, read-only by application.
  // (i.e. get number of "new events" by comparing to cached value).
  // As completion events come in out of order this counter is not ordered with
  // any other data.
  u32 cq_overflow;

  // cqes[...]: ring buffer of completion events.
  // The driver writes completion events fresh every time they are produced,
  // so the application is allowed to modify pending entries.
  p_ioring_cqe_t cqes[] _p_cacheline_aligned;
} iorings_t;


// ioringctx_t: ioring instance data
typedef struct p_ioringctx {
  iorings_t* rings;
  u32 flags; // enum ioring_setupflag

  // submission data
  struct {
    u32*            sq_array;
    p_ioring_sqe_t* sq_sqes;
    u32             sq_entries;
  } _p_cacheline_aligned;

  // completion data
  struct {
    u32 cq_entries;
  } _p_cacheline_aligned;
} ioringctx_t;


// memalloc_header_t: mem_alloc metadata
typedef struct memalloc_header {
  usize size;
} _p_cacheline_aligned memalloc_header_t;


// ioring instances as globals as most programs will only use one, maybe two, iorings.
// TODO: something more flexible
static ioringctx_t g_ioringv[8] = {0};
static u32         g_ioringc = 0;


static void* mem_alloc(usize size) {
  memalloc_header_t* h = mmap(NULL, size + sizeof(memalloc_header_t),
    PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (h == MAP_FAILED)
    return NULL;
  h->size = size;
  return &h[1];
}


static void mem_free(void* p) {
  if (!p)
    return;
  memalloc_header_t* h = ((memalloc_header_t*)p) - 1;
  munmap(h, h->size);
}


static void ioringctx_free(ioringctx_t* ctx) {
  mem_free(ctx->rings); ctx->rings = NULL;
  mem_free(ctx->sq_sqes); ctx->sq_sqes = NULL;

  ctx->flags = 0; // mark as free

  // free up entries in g_ioringv
  i32 i = (i32)(g_ioringc - 1);
  if (&g_ioringv[i] == ctx) { // unwind
    for (; i >= 0 && g_ioringv[i].flags == 0; i--)
      g_ioringc--;
  }
}


static ioringctx_t* ioringctx_alloc(p_ioring_params_t* p) {
  dlog("index %u", (u32)g_ioringc);
  if (g_ioringc >= ARRAY_LEN(g_ioringv))
    return NULL;
  ioringctx_t* ctx = &g_ioringv[g_ioringc++];
  ctx->flags = p->flags | IORING_CTX_INIT;
  return ctx;
}


static usize iorings_size(u32 sq_entries, u32 cq_entries, usize* sq_offset) {
  iorings_t* rings;
  usize offs = struct_size(rings, cqes, cq_entries);
  if (offs == USIZE_MAX)
    return USIZE_MAX;

  offs = ALIGN(offs, L1_CACHELINE_NBYTE);
  if (offs == 0)
    return USIZE_MAX;

  *sq_offset = offs;

  usize sq_array_size = array_size(sizeof(u32), sq_entries);
  if (sq_array_size == USIZE_MAX)
    return USIZE_MAX;

  // offs = offs + sq_array_size
  if (check_add_overflow(offs, sq_array_size, &offs))
    return USIZE_MAX;

  return offs;
}


// alloc_rings allocates ctx->rings and ctx->sq_sqes (submission queue entries)
static err_t alloc_rings(ioringctx_t* ctx, p_ioring_params_t* p) {
  ctx->sq_entries = p->sq_entries;
  ctx->cq_entries = p->cq_entries;

  // allocate memory for rings
  usize sq_array_offset;
  usize size = iorings_size(p->sq_entries, p->cq_entries, &sq_array_offset);
  if (size == USIZE_MAX)
    return p_err_overflow;

  iorings_t* rings = mem_alloc(size);
  if (!rings)
    return p_err_nomem;

  ctx->rings = rings;
  ctx->sq_array = (u32*)((char*)rings + sq_array_offset);
  rings->sq_ring_mask = p->sq_entries - 1;
  rings->cq_ring_mask = p->cq_entries - 1;
  rings->sq_ring_entries = p->sq_entries;
  rings->cq_ring_entries = p->cq_entries;

  // allocate memory for submission queue entries
  size = array_size(sizeof(p_ioring_sqe_t), p->sq_entries);
  if (size == USIZE_MAX)
    goto err;

  ctx->sq_sqes = mem_alloc(size);
  if (!ctx->sq_sqes)
    goto err;

  return 0;
err:
  mem_free(ctx->rings);
  ctx->rings = NULL;
  return p_err_nomem;
}


static err_t ioring_create(ioringctx_t** ctx_out, u32 entries, p_ioring_params_t* p) {
  // check for unsupported flags
  if (p->flags & ( P_IORING_SETUP_IOPOLL
                 | P_IORING_SETUP_SQPOLL
                 | P_IORING_SETUP_CQSIZE
                 | P_IORING_SETUP_SQ_AFF
                 | P_IORING_SETUP_ATTACH_WQ
  )) {
    return p_err_not_supported;
  }

  // check that entries count is within limits
  if (entries == 0) {
    return p_err_invalid;
  } else if (entries > IORING_MAX_ENTRIES) {
    if (!(p->flags & P_IORING_SETUP_CLAMP))
      return p_err_invalid;
    entries = IORING_MAX_ENTRIES;
  }

  // use twice as many entries for the CQ ring
  p->sq_entries = ceil_pow2(entries);
  p->cq_entries = 2 * p->sq_entries;

  // allocate a ioring context structure
  ioringctx_t* ctx = ioringctx_alloc(p);
  if (!ctx)
    return p_err_nomem;

  // allocate ring memory
  err_t e = alloc_rings(ctx, p);
  if (e)
    goto err;

  // Note: no support for SQPOLL, so not doing any work to set that up

  // update p with submission queue offsets
  memset(&p->sq_off, 0, sizeof(p->sq_off));
  p->sq_off.head         = offsetof(iorings_t, sq.head);
  p->sq_off.tail         = offsetof(iorings_t, sq.tail);
  p->sq_off.ring_mask    = offsetof(iorings_t, sq_ring_mask);
  p->sq_off.ring_entries = offsetof(iorings_t, sq_ring_entries);
  p->sq_off.flags        = offsetof(iorings_t, sq_flags);
  p->sq_off.dropped      = offsetof(iorings_t, sq_dropped);
  p->sq_off.array        = (char *)ctx->sq_array - (char *)ctx->rings;

  // update p with completion queue offsets
  memset(&p->cq_off, 0, sizeof(p->cq_off));
  p->cq_off.head         = offsetof(iorings_t, cq.head);
  p->cq_off.tail         = offsetof(iorings_t, cq.tail);
  p->cq_off.ring_mask    = offsetof(iorings_t, cq_ring_mask);
  p->cq_off.ring_entries = offsetof(iorings_t, cq_ring_entries);
  p->cq_off.overflow     = offsetof(iorings_t, cq_overflow);
  p->cq_off.cqes         = offsetof(iorings_t, cqes);
  p->cq_off.flags        = offsetof(iorings_t, cq_flags);

  // tell the application what features are supported
  p->features = P_IORING_FEAT_SINGLE_MMAP
              | P_IORING_FEAT_NODROP
              // | P_IORING_FEAT_SUBMIT_STABLE
              // | P_IORING_FEAT_RW_CUR_POS
              // | P_IORING_FEAT_CUR_PERSONALITY
              // | P_IORING_FEAT_FAST_POLL
              // | P_IORING_FEAT_POLL_32BITS
              // | P_IORING_FEAT_SQPOLL_NONFIXED
              // | P_IORING_FEAT_EXT_ARG
              // | P_IORING_FEAT_NATIVE_WORKERS
              // | P_IORING_FEAT_RSRC_TAGS
              ;

  *ctx_out = ctx;
  return 0;
err:
  ioringctx_free(ctx);
  return e;
}


static err_t _ioring_release(vfile_t* f) {
  ioringctx_t* ctx = f->data;
  ioringctx_free(ctx);
  return 0;
}


static err_t _ioring_mmap(vfile_t* f, void** addr, usize sz, mmapflag_t flag, usize offs) {
  ioringctx_t* ctx = f->data;
  switch (offs) {
    case P_IORING_OFF_SQ_RING:
      // TODO check sz argument
      *addr = ctx->rings;
      return 0;
    case P_IORING_OFF_SQES:
      // TODO check sz argument
      *addr = ctx->sq_sqes;
      return 0;
    default:
      return p_err_invalid;
  }
}


static const vfile_ops_t fops = {
  .release = _ioring_release,
  .mmap = _ioring_mmap,
};


fd_t _psys_ioring_setup(psysop_t _, u32 entries, p_ioring_params_t* params) {
  p_ioring_params_t p;
  if (!copy_from_user(&p, params, sizeof(p)))
    return p_err_mfault;

  // check to make sure p.resv is zeroed
  for (u32 i = 0; i < ARRAY_LEN(p.resv); i++) {
    if (p.resv[i])
      return p_err_invalid;
  }

  // create ioring
  ioringctx_t* ctx;
  err_t err = ioring_create(&ctx, entries, &p);
  if (err < 0)
    return err;

  // update user's params info
  if (!copy_to_user(params, &p, sizeof(p))) {
    ioringctx_free(ctx);
    return p_err_mfault;
  }

  // allocate a virtual file
  vfile_t* f;
  fd_t fd = vfile_open(&f, "[ioring]", &fops, 0);
  if (fd < 0) {
    ioringctx_free(ctx);
    return fd;
  }
  f->data = ctx;
  return fd;
}


isize _psys_ioring_enter(psysop_t _, fd_t ring, u32 to_submit, u32 min_complete, u32 flags) {
  return p_err_not_supported;
}


isize _psys_ioring_register(psysop_t _, fd_t ring, u32 opcode, const void* arg, u32 nr_args) {
  return p_err_not_supported;
}
