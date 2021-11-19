// SPDX-License-Identifier: Apache-2.0

// ioring implementation
//
// Shared application/kernel submission and completion ring pairs, for supporting
// fast & efficient IO. Based on and compatible with Linux io_uring.
//
// A note on the read/write ordering memory barriers that are matched between
// the application and kernel side.
//
// After the application reads the CQ ring tail, it must use an
// appropriate smp_rmb() to pair with the smp_wmb() the kernel uses
// before writing the tail (using smp_load_acquire to read the tail will
// do). It also needs a smp_mb() before updating CQ head (ordering the
// entry load(s) with the head store), pairing with an implicit barrier
// through a control-dependency in io_get_cqe (smp_store_release to
// store head will do). Failure to do so could lead to reading invalid
// CQ entries.
//
// Likewise, the application must use an appropriate smp_wmb() before
// writing the SQ tail (ordering SQ entry stores with the tail store),
// which pairs with smp_load_acquire in io_get_sqring (smp_store_release
// to store the tail will do). And it needs a barrier ordering the SQ
// head load before writing new SQ entries (smp_load_acquire to read
// head will do).
//
// When using the SQ poll thread (IORING_SETUP_SQPOLL), the application
// needs to check the SQ flags for IORING_SQ_NEED_WAKEUP *after*
// updating the SQ tail; a full memory barrier smp_mb() is needed
// between.

p_membarrier_r_acq

#define SYS_DEBUG // define to enable debug logging
#include "sys_impl.h"
#include <stdlib.h>

typedef struct ioring {
  u32 head ____cacheline_aligned_in_smp;
  u32 tail ____cacheline_aligned_in_smp;
} ioring_t;

// flags for p_ioringctx_t
typedef enum p_ioring_ctxflag {
  P_IORING_CTX_ALIVE = 1 << 0,
} p_ioring_ctxflag_t;

// ioring instance data
typedef struct p_ioringctx {
  u32 flags;
  void* sq_p;
  void* sqes_p;
} p_ioringctx_t;


// ioring instances as globals as most programs will only use one, maybe two, iorings.
// TODO: something more flexible
static p_ioringctx_t g_ioringv[8] = {0};
static u32           g_ioringc = 0;


static err_t _ioring_close(vfile_t* f) {
  p_ioringctx_t* ctx = f->data;

  ctx->flags = 0; // mark as free
  if (ctx->sq_p) {
    free(ctx->sq_p);
    ctx->sq_p = NULL;
  }
  if (ctx->sqes_p) {
    free(ctx->sqes_p);
    ctx->sqes_p = NULL;
  }

  // free up entries in g_ioringv
  i32 i = (i32)(g_ioringc - 1);
  if (&g_ioringv[i] == ctx) { // unwind
    for (; i >= 0 && g_ioringv[i].flags == 0; i--)
      g_ioringc--;
  }
  return 0;
}


static err_t _ioring_mmap(
  vfile_t* f, void** addr, usize length, mmapflag_t flag, fd_t fd, usize offs)
{
  p_ioringctx_t* ctx = f->data;
  void* p = malloc(length);
  if (!p)
    return p_err_nomem;
  switch (offs) {
    case P_IORING_OFF_SQ_RING:
      ctx->sq_p = p;
      break;
    case P_IORING_OFF_SQES:
      ctx->sqes_p = p;
      break;
    default:
      free(p);
      return p_err_invalid;
  }
  *addr = p;
  return 0;
}


static isize _ioring_on_syscall(
  psysop_t op, isize a1, isize a2, isize a3, isize a4, isize a5, vfile_t* f)
{
  switch (op) {
  case p_sysop_close:
    return _ioring_close(f);
  case p_sysop_mmap:
    return _ioring_mmap(f, (void**)a1, (usize)a2, (mmapflag_t)a3, (fd_t)a4, (usize)a5);
  default:
    return VFILE_SYSCALL_DEFAULT;
  }
}


fd_t _psys_ioring_setup(psysop_t _, u32 entries, p_ioring_params_t* params) {
  // allocate a virtual file
  vfile_t* f;
  fd_t fd = vfile_open(&f, VFILE_T_IORING);
  if (fd < 0)
    return fd; // error
  f->on_syscall = _ioring_on_syscall;

  // allocate a p_ioringctx_t struct
  if (g_ioringc == countof(g_ioringv)) {
    vfile_close(f);
    return p_err_nomem;
  }
  dlog("index %u", (u32)g_ioringc);
  p_ioringctx_t* ctx = &g_ioringv[g_ioringc++];
  ctx->flags = P_IORING_CTX_ALIVE;
  f->data = ctx;

  return fd;
}


isize _psys_ioring_enter(psysop_t _, fd_t ring, u32 to_submit, u32 min_complete, u32 flags) {
  return p_err_not_supported;
}


isize _psys_ioring_register(psysop_t _, fd_t ring, u32 opcode, const void* arg, u32 nr_args) {
  return p_err_not_supported;
}
