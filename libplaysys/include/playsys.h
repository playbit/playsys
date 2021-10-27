// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#if __has_attribute(warn_unused_result)
  #define SYS_WUNUSED __attribute__((warn_unused_result))
#else
  #define SYS_WUNUSED
#endif

#if __has_attribute(unused)
  #define SYS_UNUSED __attribute__((unused))
#else
  #define SYS_UNUSED
#endif

#ifdef __cplusplus
  #define SYS_EXTERN extern "C"
#else
  #define SYS_EXTERN extern
#endif

#define SYS_MSG_ALIGN 64 /* should match line cache */
#define SYS_MSG_STRUCT_ATTR  __attribute__((aligned(SYS_MSG_ALIGN)))

// ---------------------------------------------------------------

typedef signed char        i8;
typedef unsigned char      u8;
typedef signed short       i16;
typedef unsigned short     u16;
typedef signed int         i32;
typedef unsigned int       u32;
typedef signed long long   i64;
typedef unsigned long long u64;
typedef signed long        isize;
typedef unsigned long      usize;
typedef float              f32;
typedef double             f64;

// ---------------------------------------------------------------

typedef isize sys_ret;
typedef isize sys_fd;
typedef void (*sys_callback_fn)(sys_ret result, void* userdata);

typedef u32 sys_opcode;
enum _sys_opcode {
  sys_op_init,   // sys_callback f, void* userdata
  sys_op_test,   // sys_opcode op
  sys_op_exit,   // int status_code
  sys_op_openat, // sys_fd fd, const char* path, usize flags, isize mode
  sys_op_close,  // sys_fd fd
  sys_op_read,   // sys_fd fd, void* data, usize size
  sys_op_write,  // sys_fd fd, const void* data, usize size
  sys_op_sleep,  // usize seconds, usize nanoseconds
};

typedef u32 sys_err;
enum _sys_err {
  sys_err_none,      // no error
  sys_err_badfd,     // invalid file descriptor
  sys_err_invalid,   // invalid data or argument
  sys_err_sys_op,    // invalid syscall op or syscall op data
  sys_err_bad_name,
  sys_err_not_found,
  sys_err_name_too_long,
  sys_err_canceled,      // operation canceled
  sys_err_not_supported, // functionality not supported
  sys_err_exists,        // already exists
  sys_err_end,           // e.g. EOF
  sys_err_access,        // permission denied
};

typedef u32 sys_open_flags;
enum _sys_open_flags {
  sys_open_ronly  = 0,
  sys_open_wonly  = 1,
  sys_open_rw     = 2,
  sys_open_append = 1 << 2,
  sys_open_create = 1 << 3,
  sys_open_trunc  = 1 << 4,
  sys_open_excl   = 1 << 5,
};

sys_ret sys_syscall(sys_opcode, isize, isize, isize, isize, isize) SYS_WUNUSED;

// ---------------------------------------------------------------

#define SYS_FD_STDIN  ((sys_fd)0)
#define SYS_FD_STDOUT ((sys_fd)1)
#define SYS_FD_STDERR ((sys_fd)2)

#define SYS_AT_FDCWD -100 // value from musl

// syscall call helpers
#define sys_syscall0(op) \
  ((sys_ret(*)(sys_opcode))sys_syscall)(op)
#define sys_syscall1(op, arg1) \
  ((sys_ret(*)(sys_opcode,isize))sys_syscall)(op, (isize)(arg1))
#define sys_syscall2(op, arg1, arg2) \
  ((sys_ret(*)(sys_opcode,isize,isize))sys_syscall)(op, \
    (isize)(arg1), (isize)(arg2))
#define sys_syscall3(op, arg1, arg2, arg3) \
  ((sys_ret(*)(sys_opcode,isize,isize,isize))sys_syscall)(op, \
    (isize)(arg1), (isize)(arg2), (isize)(arg3))
#define sys_syscall4(op, arg1, arg2, arg3, arg4) \
  ((sys_ret(*)(sys_opcode,isize,isize,isize,isize))sys_syscall)(op, \
    (isize)(arg1), (isize)(arg2), (isize)(arg3), (isize)(arg4))
#define sys_syscall5(op, arg1, arg2, arg3, arg4, arg5) \
  ((sys_ret(*)(sys_opcode,isize,isize,isize,isize,isize))sys_syscall)(op, \
    (isize)(arg1), (isize)(arg2), (isize)(arg3), (isize)(arg4), (isize)(arg5))
