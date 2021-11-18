// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

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


// virtual file type
typedef enum vfile_type {
  VFILE_T_NULL,
  VFILE_T_WGPU_DEV,
  VFILE_T_GUI_SURF,
  VFILE_T_IORING,
  VFILE_T_USER = 1000,
} vfile_type_t;

// virtual file flags
typedef enum vfile_flag {
  VFILE_F_PIPE = 1 << 0, // allocate two fds with pipe(). vfile_open returns "internal" end.
} vfile_flag_t;

// virtual file data
typedef struct vfile {
  fd_t  fd;
  u32   type; // VFILE_T_ constant
  void* data; // data specific to type
  // event handlers
  // if set, on_close is called just before the file's fds are closed.
  err_t(*on_close)(struct vfile*);
  // if set, on_{read,write} OVERRIDES _psys_{read,write}.
  isize(*on_read)(struct vfile*, void* data, usize size);
  isize(*on_write)(struct vfile*, const void* data, usize size);
} vfile_t;

// virtual file functions
fd_t vfile_open(vfile_t** fp, vfile_type_t, vfile_flag_t);
err_t vfile_close(vfile_t*);
vfile_t* vfile_lookup(fd_t); // returns NULL if not found


// syscall implementations
err_t _psys_pipe(psysop_t, fd_t* fdp, u32 flags);
err_t _psys_close(psysop_t, fd_t);
err_t _psys_close_host(psysop_t, fd_t); // does not consider vfiles
fd_t _psys_ioring_setup(psysop_t, u32 entries, p_ioring_params_t* params);
isize _psys_ioring_enter(psysop_t, fd_t ring, u32 to_submit, u32 min_complete, u32 flags);
isize _psys_ioring_register(psysop_t, fd_t ring, u32 opcode, const void* arg, u32 nr_args);
