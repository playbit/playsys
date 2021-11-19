// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <playsys.h>
#include <playwgpu.h> // backend interface

#define SYS_API_VERSION 1

#define static_assert _Static_assert

#if __has_attribute(musttail)
  #define MUSTTAIL __attribute__((musttail))
#else
  #define MUSTTAIL
#endif

#ifndef countof
  #define countof(x) \
    ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#endif

#define XSTR(s) STR(s)
#define STR(s) #s

#ifdef SYS_DEBUG
  #include <stdio.h>
  #include <string.h> // strerror
  #include <assert.h>
  #define dlog(fmt, ...) ({ \
    fprintf(stderr, "\e[1;36m" "[%s] " fmt "\e[0m\n", __func__, ##__VA_ARGS__); \
    fflush(stderr); \
  })
#else
  #define dlog(fmt, ...) ((void)0)
  #define assert(...) ((void)0)
#endif

#ifndef strlen
  #define strlen __builtin_strlen
#endif
#ifndef memcpy
  #define memcpy __builtin_memcpy
#endif
#ifndef memset
  #define memset __builtin_memset
#endif
#ifndef NULL
  #define NULL ((void*)0)
#endif

#define MAX(a,b) \
  ({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
  // turns into CMP + CMOV{L,G} on x86_64
  // turns into CMP + CSEL on arm64

#define MIN(a,b) \
  ({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
  // turns into CMP + CMOV{L,G} on x86_64
  // turns into CMP + CSEL on arm64


// virtual file flags
typedef enum vfile_flag {
  // content types
  VFILE_T_MASK     = 0xff,
  VFILE_T_WGPU_DEV = 1,
  VFILE_T_GUI_SURF = 2,
  VFILE_T_IORING   = 3,

  VFILE_PIPE = 1 << 8, // allocate two fds; vfile_open returns writable end
} vfile_flag_t;

#define VFILE_SYSCALL_DEFAULT (-2147483648) // -0x80000000

// virtual file data
typedef struct vfile vfile_t;
typedef isize(*vfile_onsyscall_t)(psysop_t,isize,isize,isize,isize,isize,vfile_t*);
struct vfile {
  fd_t  fd;
  u32   flags; // vfile_flag_t
  void* data;  // data which depends on type

  // optional syscall filter; return VFILE_SYSCALL_DEFAULT to skip filtering
  vfile_onsyscall_t on_syscall;
};

// virtual file functions
fd_t vfile_open(vfile_t** fp, vfile_flag_t);
err_t vfile_close(vfile_t*);
vfile_t* vfile_lookup(fd_t); // returns NULL if not found

// syscall filtering.
// looks up vfile for fd and if found, invokes its on_syscall handler.
// returns VFILE_SYSCALL_DEFAULT if the caller should handle the syscall.
inline static isize vfile_syscall(
  fd_t fd, psysop_t op, isize a1, isize a2, isize a3, isize a4, isize a5)
{
  vfile_t* f = vfile_lookup(fd);
  if (f && f->on_syscall)
    return f->on_syscall(op, a1, a2, a3, a4, a5, f);
  return VFILE_SYSCALL_DEFAULT;
}

// helper for easier argument forwarding
#define VFILE_SYSCALL(fd, op, a1, a2, a3, a4, a5) \
  vfile_syscall(fd,op,((isize)a1),((isize)a2),((isize)a3),((isize)a4),((isize)a5))

// syscall implementations
err_t _psys_pipe(psysop_t, fd_t* fdp, u32 flags);
err_t _psys_close(psysop_t, fd_t);
err_t _psys_close_host(psysop_t, fd_t); // does not consider vfiles
fd_t _psys_ioring_setup(psysop_t, u32 entries, p_ioring_params_t* params);
isize _psys_ioring_enter(psysop_t, fd_t ring, u32 to_submit, u32 min_complete, u32 flags);
isize _psys_ioring_register(psysop_t, fd_t ring, u32 opcode, const void* arg, u32 nr_args);
