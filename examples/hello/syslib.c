#include "syslib.h"


void sys_exit(int status) {
  PSYS_UNUSED isize _ = p_syscall_exit(status);
  __builtin_unreachable();
}

fd_t sys_openat(fd_t fd, const char* path, oflag_t flags, usize mode) {
  return (fd_t)p_syscall_openat(fd, path, flags, mode);
}

fd_t sys_open(const char* path, oflag_t flags, usize mode) {
  return (fd_t)p_syscall_openat(P_AT_FDCWD, path, flags, mode);
}

fd_t sys_create(const char* path, usize mode) {
  oflag_t fl = p_open_create | p_open_wonly | p_open_trunc;
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

const char* p_errname(err_t e) {
  switch ((enum _p_err)e) {
    case p_err_none          : return "no error";
    case p_err_badfd         : return "invalid file descriptor";
    case p_err_invalid       : return "invalid data or argument";
    case p_err_sys_op        : return "invalid syscall op or syscall op data";
    case p_err_bad_name      : return "invalid or misformed name";
    case p_err_not_found     : return "resource not found";
    case p_err_name_too_long : return "name too long";
    case p_err_canceled      : return "operation canceled";
    case p_err_not_supported : return "not supported";
    case p_err_exists        : return "already exists";
    case p_err_end           : return "end of resource";
    case p_err_access        : return "permission denied";
    case p_err_nomem         : return "cannot allocate memory";
  }
  return "unknown error";
}
