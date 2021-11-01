// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#if __has_attribute(warn_unused_result)
  #define PSYS_WARN_UNUSED __attribute__((warn_unused_result))
#else
  #define PSYS_WARN_UNUSED
#endif

#if __has_attribute(unused)
  #define PSYS_UNUSED __attribute__((unused))
#else
  #define PSYS_UNUSED
#endif

#ifdef __cplusplus
  #define PSYS_EXTERN extern "C"
#else
  #define PSYS_EXTERN extern
#endif

// types
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

typedef usize psysop_t; // syscall operation code
typedef usize oflag_t;  // flags to openat syscall
typedef isize err_t;    // error code. Only negative values.
typedef isize fd_t;     // file descriptor

// constants
#define P_FDSTDIN  ((fd_t)(0))    // input stream
#define P_FDSTDOUT ((fd_t)(1))    // main output stream
#define P_FDSTDERR ((fd_t)(2))    // logging output stream
#define P_AT_FDCWD ((fd_t)(-100)) // "current directory" for *at file operations

// errors (possible values of type err_t)
enum _p_err {
  p_err_none          =   0, // no error
  p_err_badfd         =  -1, // invalid file descriptor
  p_err_invalid       =  -2, // invalid data or argument
  p_err_sys_op        =  -3, // invalid syscall op or syscall op data
  p_err_bad_name      =  -4, // invalid or misformed name
  p_err_not_found     =  -5, // resource not found
  p_err_name_too_long =  -6,
  p_err_canceled      =  -7, // operation canceled
  p_err_not_supported =  -8, // functionality not supported
  p_err_exists        =  -9, // already exists
  p_err_end           = -10, // end of resource
  p_err_access        = -11, // permission denied
};

// open flags (possible bits of type oflag_t)
enum _p_oflag {
  p_open_ronly  = 0,  // Open for reading only
  p_open_wonly  = 1,  // Open for writing only
  p_open_rw     = 2,  // Open for both reading and writing
  p_open_append = 4,  // Start writing at end (seekable files only)
  p_open_create = 8,  // Create file if it does not exist
  p_open_trunc  = 16, // Set file size to zero
  p_open_excl   = 32, // fail if file exists when create and excl are set
};

// syscall operations (possible values of type psysop_t)
enum _p_sysop {
  p_sysop_openat   = 257,   // base fd, path cstr, flags oflag, mode usize
  p_sysop_close    = 3,     // fd fd
  p_sysop_read     = 0,     // fd fd, data mutptr, nbyte usize
  p_sysop_write    = 1,     // fd fd, data ptr, nbyte usize
  p_sysop_seek     = 8,     // TODO
  p_sysop_statat   = 262,   // TODO (newfstatat in linux, alt: statx 332)
  p_sysop_removeat = 263,   // base fd, path cstr, flags usize
  p_sysop_renameat = 264,   // oldbase fd, oldpath cstr, newbase fd, newpath cstr
  p_sysop_sleep    = 230,   // seconds usize, nanoseconds usize
  p_sysop_exit     = 60,    // status_code i32
  p_sysop_test     = 10000, // op psysop
};

// p_syscall calls the host system
PSYS_EXTERN isize p_syscall(psysop_t,isize,isize,isize,isize,isize) PSYS_WARN_UNUSED;
static isize p_syscall_openat(fd_t base, const char* path, oflag_t flags, usize mode);
static isize p_syscall_close(fd_t fd);
static isize p_syscall_read(fd_t fd, void* data, usize nbyte);
static isize p_syscall_write(fd_t fd, const void* data, usize nbyte);
static isize p_syscall_removeat(fd_t base, const char* path, usize flags);
static isize p_syscall_renameat(fd_t oldbase, const char* oldpath, fd_t newbase,
  const char* newpath);
static isize p_syscall_sleep(usize seconds, usize nanoseconds);
static isize p_syscall_exit(i32 status_code);
static isize p_syscall_test(psysop_t op);

#define _p_syscall0 \
  ((isize(*)(psysop_t))p_syscall)
#define _p_syscall1 \
  ((isize(*)(psysop_t,isize))p_syscall)
#define _p_syscall2 \
  ((isize(*)(psysop_t,isize,isize))p_syscall)
#define _p_syscall3 \
  ((isize(*)(psysop_t,isize,isize,isize))p_syscall)
#define _p_syscall4 \
  ((isize(*)(psysop_t,isize,isize,isize,isize))p_syscall)
#define _p_syscall5 \
  ((isize(*)(psysop_t,isize,isize,isize,isize,isize))p_syscall)

inline static isize p_syscall_openat(fd_t base, const char* path, oflag_t flags,
  usize mode) {
  return _p_syscall4(p_sysop_openat, (isize)base, (isize)path, (isize)flags, (isize)mode);
}
inline static isize p_syscall_close(fd_t fd) {
  return _p_syscall1(p_sysop_close, (isize)fd);
}
inline static isize p_syscall_read(fd_t fd, void* data, usize nbyte) {
  return _p_syscall3(p_sysop_read, (isize)fd, (isize)data, (isize)nbyte);
}
inline static isize p_syscall_write(fd_t fd, const void* data, usize nbyte) {
  return _p_syscall3(p_sysop_write, (isize)fd, (isize)data, (isize)nbyte);
}
inline static isize p_syscall_removeat(fd_t base, const char* path, usize flags) {
  return _p_syscall3(p_sysop_removeat, (isize)base, (isize)path, (isize)flags);
}
inline static isize p_syscall_renameat(fd_t oldbase, const char* oldpath, fd_t newbase,
  const char* newpath) {
  return _p_syscall4(p_sysop_renameat, (isize)oldbase, (isize)oldpath, (isize)newbase,
    (isize)newpath);
}
inline static isize p_syscall_sleep(usize seconds, usize nanoseconds) {
  return _p_syscall2(p_sysop_sleep, (isize)seconds, (isize)nanoseconds);
}
inline static isize p_syscall_exit(i32 status_code) {
  return _p_syscall1(p_sysop_exit, (isize)status_code);
}
inline static isize p_syscall_test(psysop_t op) {
  return _p_syscall1(p_sysop_test, (isize)op);
}

// Note: this file is generated from spec.md; edit with caution
