#include "syslib.h"


sys_ret sys_init(sys_callback_fn f, void* userdata) {
  return sys_syscall2(sys_op_init, f, userdata);
}

void sys_exit(int status) {
  SYS_UNUSED sys_ret _ = sys_syscall1(sys_op_exit, status);
  __builtin_unreachable();
}

sys_fd sys_openat(sys_fd fd, const char* path, sys_open_flags flags, u32 mode) {
  return (sys_fd)sys_syscall4(sys_op_openat, fd, path, flags, mode);
}

sys_fd sys_open(const char* path, sys_open_flags flags, u32 mode) {
  return (sys_fd)sys_syscall4(sys_op_openat, SYS_AT_FDCWD, path, flags, mode);
}

sys_fd sys_create(const char* path, u32 mode) {
  sys_open_flags fl = sys_open_create | sys_open_wonly | sys_open_trunc;
  return sys_open(path, fl, mode);
}

sys_ret sys_close(sys_fd fd) {
  return sys_syscall1(sys_op_close, fd);
}

isize sys_write(sys_fd fd, const void* data, usize size) {
  return (isize)sys_syscall3(sys_op_write, fd, data, size);
}

isize sys_read(sys_fd fd, void* data, usize size) {
  return (isize)sys_syscall3(sys_op_read, fd, data, size);
}

isize sys_sleep(usize seconds, usize nanoseconds) {
  return (isize)sys_syscall2(sys_op_sleep, seconds, nanoseconds);
}

// sys_ret sys_ring_enter(sys_fd ring_fd, u32 to_submit, u32 min_complete, u32 flags) {
//   return sys_syscall(sys_op_ring_enter, ring_fd, to_submit, min_complete, flags, 0, 0);
// }

const char* sys_errname(sys_err e) {
  switch ((enum _sys_err)e) {
    case sys_err_none:          return "(no error)";
    case sys_err_badfd:         return "badfd";
    case sys_err_invalid:       return "invalid";
    case sys_err_sys_op:        return "sys_op";
    case sys_err_bad_name:      return "bad_name";
    case sys_err_name_too_long: return "name_too_long";
    case sys_err_not_found:     return "not_found";
    case sys_err_canceled:      return "canceled";
    case sys_err_not_supported: return "not_supported";
    case sys_err_exists:        return "exists";
    case sys_err_end:           return "end";
    case sys_err_access:        return "access";
  }
  return "(unknown error)";
}
