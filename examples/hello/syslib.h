#pragma once
#include <playsys.h>

sys_ret sys_init(sys_callback_fn, void* userdata);
_Noreturn void sys_exit(int status);
sys_fd sys_open(const char* path, sys_open_flags flags, u32 mode) SYS_WUNUSED;
sys_fd sys_openat(sys_fd, const char* path, sys_open_flags flags, u32 mode) SYS_WUNUSED;
sys_fd sys_create(const char* path, u32 mode);
sys_ret sys_close(sys_fd fd);
isize sys_write(sys_fd fd, const void* data, usize size);
isize sys_read(sys_fd fd, void* data, usize size);

isize sys_sleep(usize seconds, usize nanoseconds);

// sys_ret sys_ring_enter(sys_fd ring_fd, u32 to_submit, u32 min_complete, u32 flags);

const char* sys_errname(sys_err err);

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
