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
#define P_FDSTDIN  ((fd_t)0)    // input stream
#define P_FDSTDOUT ((fd_t)1)    // main output stream
#define P_FDSTDERR ((fd_t)2)    // logging output stream
#define P_AT_FDCWD ((fd_t)-100) // "current directory" for *at file operations

// errors (possible values of type err_t)
enum _p_err {
  p_err_none          =   0, // no error
  p_err_invalid       =  -1, // invalid data or argument
  p_err_sys_op        =  -2, // invalid syscall op or syscall op data
  p_err_badfd         =  -3, // invalid file descriptor
  p_err_bad_name      =  -4, // invalid or misformed name
  p_err_not_found     =  -5, // resource not found
  p_err_name_too_long =  -6, // 
  p_err_canceled      =  -7, // operation canceled
  p_err_not_supported =  -8, // not supported
  p_err_exists        =  -9, // already exists
  p_err_end           = -10, // end of resource
  p_err_access        = -11, // permission denied
  p_err_nomem         = -12, // cannot allocate memory
};

// open flags (possible bits of type oflag_t)
enum _p_oflag {
  p_open_ronly  =  0, // Open for reading only
  p_open_wonly  =  1, // Open for writing only
  p_open_rw     =  2, // Open for both reading and writing
  p_open_append =  4, // Start writing at end (seekable files only)
  p_open_create =  8, // Create file if it does not exist
  p_open_trunc  = 16, // Set file size to zero
  p_open_excl   = 32, // fail if file exists when create and excl are set
};

// syscall operations (possible values of type psysop_t)
enum _p_sysop {
  p_sysop_openat       =   257, // base fd, path cstr, flags oflag, mode usize
  p_sysop_close        =     3, // fd fd
  p_sysop_read         =     0, // fd fd, data mutptr, nbyte usize
  p_sysop_write        =     1, // fd fd, data ptr, nbyte usize
  p_sysop_seek         =     8, // TODO
  p_sysop_statat       =   262, // TODO (newfstatat in linux, alt: statx 332)
  p_sysop_removeat     =   263, // base fd, path cstr, flags usize
  p_sysop_renameat     =   264, // oldbase fd, oldpath cstr, newbase fd, newpath cstr
  p_sysop_sleep        =   230, // seconds usize, nanoseconds usize
  p_sysop_exit         =    60, // status_code i32
  p_sysop_test         = 10000, // op psysop
  p_sysop_wgpu_opendev = 10001, // flags usize
  p_sysop_gui_mksurf   = 10002, // width u32, height u32, device fd, flags usize
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
static isize p_syscall_wgpu_opendev(usize flags);
static isize p_syscall_gui_mksurf(u32 width, u32 height, fd_t device, usize flags);

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
inline static isize p_syscall_wgpu_opendev(usize flags) {
  return _p_syscall1(p_sysop_wgpu_opendev, (isize)flags);
}
inline static isize p_syscall_gui_mksurf(u32 width, u32 height, fd_t device, usize flags) {
  return _p_syscall4(p_sysop_gui_mksurf, (isize)width, (isize)height, (isize)device,
    (isize)flags);
}

// p_err_str returns the symbolic name of an error as a string
inline static const char* p_err_str(err_t e) {
  switch ((enum _p_err)e) {
  case p_err_none:          return "none";
  case p_err_invalid:       return "invalid";
  case p_err_sys_op:        return "sys_op";
  case p_err_badfd:         return "badfd";
  case p_err_bad_name:      return "bad_name";
  case p_err_not_found:     return "not_found";
  case p_err_name_too_long: return "name_too_long";
  case p_err_canceled:      return "canceled";
  case p_err_not_supported: return "not_supported";
  case p_err_exists:        return "exists";
  case p_err_end:           return "end";
  case p_err_access:        return "access";
  case p_err_nomem:         return "nomem";
  }
  return "?";
}

// Note: this file is generated from spec.md; edit with caution
