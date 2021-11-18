#pragma once
#include <playsys.h>

_Noreturn void sys_exit(int status);
fd_t sys_open(const char* path, openflag_t flags, usize mode) PSYS_WARN_UNUSED;
fd_t sys_openat(fd_t, const char* path, openflag_t flags, usize mode) PSYS_WARN_UNUSED;
fd_t sys_create(const char* path, usize mode);
err_t sys_close(fd_t fd);
isize sys_write(fd_t fd, const void* data, usize nbyte);
isize sys_read(fd_t fd, void* data, usize nbyte);

isize sys_sleep(usize seconds, usize nanoseconds);

// sys_ret sys_ring_enter(fd_t ring_fd, u32 to_submit, u32 min_complete, u32 flags);

const char* p_errname(err_t err);

#ifndef SYS_NO_SYSLIB_LIBC_API
  #define exit   sys_exit
  #define open   sys_open
  #define openat sys_openat
  #define create sys_create
  #define close  sys_close
  #define write  sys_write
  #define read   sys_read
  #define sleep(sec) sys_sleep((sec), 0)

  #ifndef strlen
    #define strlen __builtin_strlen
  #endif
  #ifndef memcpy
    #define memcpy __builtin_memcpy
  #endif
#endif
