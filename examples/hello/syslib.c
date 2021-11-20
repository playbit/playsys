#include "syslib.h"


void sys_exit(int status) {
  PSYS_UNUSED isize _ = p_syscall_exit(status);
  __builtin_unreachable();
}

fd_t sys_openat(fd_t fd, const char* path, openflag_t flags, usize mode) {
  return (fd_t)p_syscall_openat(fd, path, flags, mode);
}

fd_t sys_open(const char* path, openflag_t flags, usize mode) {
  return (fd_t)p_syscall_openat(P_AT_FDCWD, path, flags, mode);
}

fd_t sys_create(const char* path, usize mode) {
  openflag_t fl = p_open_create | p_open_wonly | p_open_trunc;
  return sys_open(path, fl, mode);
}

err_t sys_close(fd_t fd) {
  return (err_t)p_syscall_close(fd);
}

isize sys_write(fd_t fd, const void* data, usize nbyte) {
  return p_syscall_write(fd, data, nbyte);
}

isize sys_read(fd_t fd, void* data, usize nbyte) {
  return p_syscall_read(fd, data, nbyte);
}

isize sys_sleep(usize seconds, usize nanoseconds) {
  return p_syscall_sleep(seconds, nanoseconds);
}

// sys_ret sys_ring_enter(fd_t ring_fd, u32 to_submit, u32 min_complete, u32 flags) {
//   return sys_syscall(sys_op_ring_enter, ring_fd, to_submit, min_complete, flags, 0, 0);
// }
