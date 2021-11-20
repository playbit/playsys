// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <playsys.h>
#include <playwgpu.h> // backend interface

#define SYS_API_VERSION 1

#define static_assert _Static_assert

#define XSTR(s) STR(s)
#define STR(s) #s

#define L1_CACHELINE_NBYTE 64
#define _p_cacheline_aligned __attribute__((__aligned__(L1_CACHELINE_NBYTE)))

#if !defined(__wasm__)
  #define HAS_LIBC 1
#endif

#ifndef NULL
  #define NULL ((void*)0)
#endif

#ifdef __cplusplus
  #define EXTERNC extern "C"
#else
  #define EXTERNC extern
#endif

#define USIZE_MAX ((usize)-1)

#if __has_attribute(musttail)
  #define MUSTTAIL __attribute__((musttail))
#else
  #define MUSTTAIL
#endif

#if __has_attribute(__designated_init__)
  #define _designated_init __attribute__((__designated_init__))
#else
  #define _designated_init
#endif

#if __has_attribute(randomize_layout)
  #define _randomize_layout __attribute__((randomize_layout))
#else
  #define _randomize_layout _designated_init
#endif

// UNLIKELY(integralexpr)->integralexpr
// Provide explicit branch prediction. Use like this:
// if (UNLIKELY(buf & 0xff))
//   error_hander("error");
#ifdef __builtin_expect
  #define UNLIKELY(x) __builtin_expect((x), 0)
  #define LIKELY(x)   __builtin_expect((x), 1)
#else
  #define UNLIKELY(x) (x)
  #define LIKELY(x)   (x)
#endif

#ifndef countof
  #define countof(x) \
    ((sizeof(x)/sizeof(0[x])) / ((usize)(!(sizeof(x) % sizeof(0[x])))))
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

#if defined(SYS_DEBUG) && defined(HAS_LIBC)
  #include <stdio.h>
  #include <string.h> // strerror
  #include <assert.h>
  #define dlog(fmt, ...) ({ \
    fprintf(stderr, "\e[1;36m" "[%s] " fmt "\e[0m\n", __func__, ##__VA_ARGS__); \
    fflush(stderr); \
  })
#else
  #define dlog(fmt, ...) ((void)0)
  #ifndef assert
    #define assert(...) ((void)0)
  #endif
#endif

#define MAX(a,b) \
  ({__typeof__(a) _a = (a); __typeof__(b) _b = (b); _a > _b ? _a : _b; })
  // turns into CMP + CMOV{L,G} on x86_64
  // turns into CMP + CSEL on arm64

#define MIN(a,b) \
  ({__typeof__(a) _a = (a); __typeof__(b) _b = (b); _a < _b ? _a : _b; })
  // turns into CMP + CMOV{L,G} on x86_64
  // turns into CMP + CSEL on arm64

#include "base1.h"


// ---------------------------------------------------
// vfile

typedef struct vfile vfile_t;
typedef struct vfile_ops vfile_ops_t;

// virtual file flags
typedef enum vfile_flag {
  // content types/tags
  VFILE_T_MASK     = 0xff,
  VFILE_T_GPUDEV,
  VFILE_T_GUI_SURF,

  // allocate two fds; vfile->fd is readable end, vfile_open returns writable end
  VFILE_PIPE_R = 1 << 1,
  // allocate two fds; vfile->fd is writable end, vfile_open returns readable end
  VFILE_PIPE_W = 1 << 2,
} vfile_flag_t;

struct vfile_ops {
  err_t (*release) (vfile_t*); // on close
  isize (*read)    (vfile_t*, char*, usize);
  isize (*write)   (vfile_t*, const char*, usize);
  err_t (*openat)  (vfile_t* at, const char*, openflag_t, usize);
  err_t (*mmap)    (vfile_t*, void**, usize len, mmapflag_t, usize offs);
} _randomize_layout;

struct vfile {
  fd_t        fd;
  u32         flags; // vfile_flag_t
  void*       data;  // use depends on flags
  const char* name;
  const vfile_ops_t* fops;
};

// virtual file functions
fd_t vfile_open(vfile_t** fp, const char* name, const vfile_ops_t*, vfile_flag_t);
err_t vfile_close(vfile_t*);
EXTERNC vfile_t* vfile_lookup(fd_t); // returns NULL if not found

// VFILE_JUMP_FOP routes a call to a vfile's fops if found for fd
#define VFILE_JUMP_FOP(FOP, fd, err_res, ...) { \
  vfile_t* f = vfile_lookup(fd);                \
  if (f)                                        \
    return f->fops->FOP ? f->fops->FOP(f, ##__VA_ARGS__) : \
             err_res; \
}


// -----------------------------------------------------------------------------------
// syscall implementations
err_t _psys_pipe(psysop_t, fd_t* fdp, u32 flags);
err_t _psys_close(psysop_t, fd_t);
err_t _psys_close_host(psysop_t, fd_t); // does not consider vfiles
fd_t _psys_ioring_setup(psysop_t, u32 entries, p_ioring_params_t* params);
isize _psys_ioring_enter(psysop_t, fd_t ring, u32 to_submit, u32 min_complete, u32 flags);
isize _psys_ioring_register(psysop_t, fd_t ring, u32 opcode, const void* arg, u32 nr_args);
