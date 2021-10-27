// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

// POSIX backend

// macOS does not have a stable syscall ABI, instead its libc (libSystem) is
// the stable kernel API interface.

#include <playsys.h>
#include <sys_playwgpu.h> // backend interface

#include <fcntl.h>  // open
#include <unistd.h> // close, read, write
#include <stdlib.h> // exit
#include <time.h>   // nanosleep
#include <sys/errno.h>
#include <sys/socket.h> // socketpair
#include <assert.h>

#define SYS_API_VERSION       1 // used by uname
#define SYS_SPECIAL_FS_PREFIX "/sys"

#define static_assert _Static_assert

#if __has_attribute(musttail)
  #define MUSTTAIL __attribute__((musttail))
#else
  #define MUSTTAIL
#endif

#define sys_countof(x) \
  ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#define XSTR(s) STR(s)
#define STR(s) #s

#define DEBUG
#ifdef DEBUG
  #include <stdio.h>
  #include <string.h> // strerror
  #define dlog(fmt, ...) fprintf(stderr, "[%s] " fmt "\n", __func__, ##__VA_ARGS__)
#else
  #define dlog(fmt, ...) ((void)0)
#endif


#define MAX(a,b) \
  ({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
  // turns into CMP + CMOV{L,G} on x86_64
  // turns into CMP + CSEL on arm64

#define MIN(a,b) \
  ({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
  // turns into CMP + CMOV{L,G} on x86_64
  // turns into CMP + CSEL on arm64

// read and write relies on sys_ret being able to represent isize values
static_assert(sizeof(sys_ret) >= sizeof(isize), "");

extern int errno;

static sys_err sys_err_from_errno(int e) {
  // TODO
  return sys_err_invalid;
}

static sys_ret sys_ret_from_errno(int e) {
  return -((sys_ret)sys_err_from_errno(e));
}


static sys_ret set_nonblock(sys_fd fd) {
  int flags = fcntl(fd, F_GETFL);
  if (flags < 0)
    return sys_ret_from_errno(errno);
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    return sys_ret_from_errno(errno);
  return 0;
}


static struct {
  sys_callback_fn callback;
  void*           userdata;
} g_caller_state = {0};

// ---------------------------------------------------
// sys_syscall op implementations


static sys_ret sys_syscall_init(sys_opcode op, sys_callback_fn f, void* userdata) {
  g_caller_state.callback = f;
  g_caller_state.userdata = userdata;
  return 0;
}


static sys_ret sys_syscall_test(sys_opcode op, isize checkop) {
  if (op > sys_op_write)
    return -sys_err_not_supported;
  return 0;
}


static sys_ret sys_syscall_exit(sys_opcode op, isize status) {
  exit((int)status);
  return 0;
}


static sys_ret open_uname(sys_opcode op, const char* path, usize flags, isize mode) {
  #if defined(__i386) || defined(__i386__) || defined(_M_IX86)
    #define UNAME_STR "macos-x86"
  #elif defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
    #define UNAME_STR "macos-x64"
  #elif defined(__arm64__) || defined(__aarch64__)
    #define UNAME_STR "macos-arm64"
  #elif defined(__arm__) || defined(__arm) || defined(__ARM__) || defined(__ARM)
    #define UNAME_STR "macos-arm32"
  #elif defined(__ppc__) || defined(__ppc) || defined(__PPC__)
    #define UNAME_STR "macos-ppc"
  #else
    #error
  #endif
  int fd[2];
  if (pipe(fd) != 0)
    return sys_ret_from_errno(errno);
  const char* s = UNAME_STR " " XSTR(SYS_API_VERSION) "\n";
  int w = write(fd[1], s, strlen(s));
  close(fd[1]);
  if (w < 0) {
    close(fd[0]);
    return sys_ret_from_errno(errno);
  }
  return (sys_ret)fd[0];
}



typedef enum {
  VFILE_NULL,
  VFILE_WGPU_DEV,  // pwgpu_dev_t
  VFILE_WGPU_SURF, // pwgpu_surface_t
  VFILE_WGPU_CTL,  // pwgpu_ctl_t
} vfile_type_t;

typedef struct vfile {
  sys_fd       fd_user; // user's end of pipe/channel
  sys_fd       fd_impl; // implementation's end of pipe/channel
  vfile_type_t type;
  union {
    void*           ptr;
    pwgpu_dev_t*     dev;  // type VFILE_WGPU_DEV
    pwgpu_surface_t* surf; // type VFILE_WGPU_SURF
    pwgpu_ctl_t*     ctl;  // type VFILE_WGPU_CTL
  };
  sys_ret(*on_close)(struct vfile*);
  sys_ret(*on_read)(struct vfile*, void* data, usize size);
  sys_ret(*on_write)(struct vfile*, const void* data, usize size);
  // if set, on_close is called just before the file's fds are closed.
  // if set, on_{read,write} OVERRIDES sys_syscall_{read,write}.
} vfile_t;

static vfile_t g_vfilev[32] = {0};
static u32     g_vfilec = 0;


static sys_fd vfile_open(vfile_type_t type, usize flags, vfile_t** f_out) {
  // allocate vfile_t struct
  if (g_vfilec == sys_countof(g_vfilev))
    return -sys_err_invalid; // FIXME ("out of memory" or "resources")
  vfile_t* f = &g_vfilev[g_vfilec++];
  memset(f, 0, sizeof(vfile_t));
  f->type = type;

  // create pipe or channel
  int r;
  int fd[2];
  if (flags & sys_open_rw) {
    r = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
  } else {
    r = pipe(fd); // fd[0] = readable, fd[1] writable
    if (flags & sys_open_wonly) {
      // swap so that fd_user is writable and fd_impl is readable.
      // i.e. after the swap: fd[0] = writable, fd[1] readable
      int fd0 = fd[0];
      fd[0] = fd[1];
      fd[1] = fd0;
    }
  }
  if (r < 0) {
    g_vfilec--;
    return sys_ret_from_errno(errno);
  }

  f->fd_user = (sys_fd)fd[0];
  f->fd_impl = (sys_fd)fd[1];

  *f_out = f;
  return f->fd_user;
}


static vfile_t* vfile_lookup(sys_fd fd_user) {
  for (u32 i = 0; i < g_vfilec; i++) {
    if (g_vfilev[i].fd_user == fd_user)
      return &g_vfilev[i];
  }
  return NULL;
}


static sys_ret vfile_close(vfile_t* f) {
  int r1 = close((int)f->fd_user);
  int errno1 = errno;
  int r2 = close((int)f->fd_impl);
  int errno2 = errno;
  sys_ret ret = f->on_close ? f->on_close(f) : 0;

  // remove f from g_vfilev
  for (u32 i = 0; i < g_vfilec; i++) {
    if (&g_vfilev[i] == f) {
      i++;
      for (; i < g_vfilec; i++)
        g_vfilev[i - 1] = g_vfilev[i];
      g_vfilec--;
      break;
    }
  }

  if (r1 != 0)
    return sys_ret_from_errno(errno1);
  if (r2 != 0)
    return sys_ret_from_errno(errno2);
  return ret;
}


static sys_ret close_pwgpu_surf(vfile_t* f) {
  pwgpu_surface_close(f->surf);
  return 0;
}

static sys_ret read_pwgpu_surf(vfile_t* f, void* data, usize size) {
  return pwgpu_surface_read(f->surf, data, size);
}

static sys_ret open_pwgpu_surf(sys_opcode op, const char* path, usize flags, isize mode) {
  if ((flags & sys_open_rw) == 0)
    return -sys_err_invalid;

  // swap rw mode flag for vfile_open so we get a pipe instead of socketpair since
  // reads are handled by the virtual read implemenation read_pwgpu_surf.
  flags = (flags & ~sys_open_rw) | sys_open_wonly;

  vfile_t* f;
  sys_fd fd = vfile_open(VFILE_WGPU_SURF, flags, &f);
  if (fd > -1) {
    sys_ret r = set_nonblock(f->fd_impl); // enable read()ing data of unknown length
    // TODO: consider using writev & readv instead.
    // TODO: consider fully virtual I/O: add buffer to pwgpu_surface_t and set f->on_read.
    if (r != 0) {
      vfile_close(f);
      return r;
    }
    f->surf = pwgpu_surface_open(f->fd_impl, f->fd_user);
    if (!f->surf) {
      vfile_close(f);
      return -sys_err_invalid;
    }
    f->on_close = close_pwgpu_surf;
    f->on_read = read_pwgpu_surf;
  }
  return fd;
}


static sys_ret close_pwgpu_ctl(vfile_t* f) {
  return pwgpu_ctl_close(f->ctl);
}

static sys_ret read_pwgpu_ctl(vfile_t* f, void* data, usize size) {
  return pwgpu_ctl_read(f->ctl, data, size);
}

static sys_ret write_pwgpu_ctl(vfile_t* f, const void* data, usize size) {
  return pwgpu_ctl_write(f->ctl, data, size);
}

static sys_ret open_pwgpu_ctl(sys_opcode op, const char* path, usize flags, isize mode) {
  if ((flags & sys_open_rw) == 0)
    return -sys_err_invalid;

  vfile_t* f;
  sys_fd fd = vfile_open(VFILE_WGPU_CTL, 0, &f);
  if (fd > -1) {
    sys_ret r = set_nonblock(f->fd_impl); // enable read()ing data of unknown length
    if (r == 0)
      r = pwgpu_ctl_open(&f->ctl, f->fd_impl);
    if (r != 0) {
      vfile_close(f);
      return -sys_err_invalid;
    }
    f->on_close = close_pwgpu_ctl;
    f->on_read = read_pwgpu_ctl;
    f->on_write = write_pwgpu_ctl;
  }
  return fd;
}


static sys_ret close_pwgpu_dev(vfile_t* f) {
  pwgpu_dev_close(f->dev);
  return 0;
}

static sys_ret open_pwgpu_dev(sys_opcode op, const char* path, usize flags, isize mode) {
  if (flags & sys_open_wonly)
    return -sys_err_invalid;
  vfile_t* f;
  sys_fd fd = vfile_open(VFILE_WGPU_DEV, flags, &f);
  if (fd > -1) {
    int adapter_id = -1; // TODO: parse "path"
    pwgpu_dev_flag_t fl = (flags & sys_open_ronly) ? pwgpu_dev_fl_ronly : 0;
    f->dev = pwgpu_dev_open(f->fd_impl, f->fd_user, adapter_id, fl);
    if (!f->dev) { // no suitable adapter found
      vfile_close(f);
      return -sys_err_not_supported;
    }
    f->on_close = close_pwgpu_dev;
  }
  return fd;
}


static sys_ret open_special(sys_opcode op, const char* path, usize flags, isize mode) {
  path = path + strlen(SYS_SPECIAL_FS_PREFIX) + 1; // "/sys/foo/bar" => "foo/bar"
  //dlog("path \"%s\"", path);
  usize pathlen = strlen(path);

  #define ROUTE(matchpath,fun) \
    if (pathlen == strlen(matchpath) && memcmp(path,(matchpath),strlen(matchpath)) == 0) \
      MUSTTAIL return (fun)(op, path, flags, mode)

  #define ROUTE_PREFIX(matchpath, fun) \
    if (pathlen >= strlen(matchpath) && memcmp(path,(matchpath),strlen(matchpath)) == 0) \
      MUSTTAIL return (fun)(op, path, flags, mode)

  ROUTE("uname", open_uname);
  ROUTE_PREFIX("wgpu/dev/gpu", open_pwgpu_dev);
  ROUTE("wgpu/surface", open_pwgpu_surf);
  ROUTE("wgpu/ctl", open_pwgpu_ctl);

  #undef ROUTE
  return -sys_err_not_found;
}


static sys_ret sys_syscall_openat(
  sys_opcode op, sys_fd atfd, const char* path, usize flags, isize mode)
{
  if (atfd == SYS_AT_FDCWD) {
    if (SYS_AT_FDCWD != (sys_fd)AT_FDCWD)
      atfd = (sys_fd)AT_FDCWD; // libc
  }

  if (strlen(path) > strlen(SYS_SPECIAL_FS_PREFIX) &&
      memcmp(path, SYS_SPECIAL_FS_PREFIX "/", 5) == 0)
  {
    return open_special(op, path, flags, mode);
  }

  static const int oflag_map[3] = {
    [sys_open_ronly] = O_RDONLY,
    [sys_open_wonly] = O_WRONLY,
    [sys_open_rw]    = O_RDWR,
  };
  int oflag = oflag_map[flags & 3]; // first two bits is ro/wo/rw
  if (flags & sys_open_append) oflag |= O_APPEND;
  if (flags & sys_open_create) oflag |= O_CREAT;
  if (flags & sys_open_trunc)  oflag |= O_TRUNC;
  if (flags & sys_open_excl)   oflag |= O_EXCL;

  int fd = openat((int)atfd, path, oflag, (mode_t)mode);
  if (fd < 0) {
    // dlog("open failed => %d (errno %d %s)", fd, errno, strerror(errno));
    return sys_ret_from_errno(errno);
  }

  return (sys_ret)fd;
}


static sys_ret sys_syscall_close(sys_opcode op, sys_fd fd) {
  vfile_t* f = vfile_lookup(fd);
  if (f)
    return vfile_close(f);

  if (close((int)fd) != 0)
    return sys_ret_from_errno(errno);

  return 0;
}


static sys_ret sys_syscall_read(sys_opcode op, sys_fd fd, void* data, usize size) {
  vfile_t* f = vfile_lookup(fd);
  if (f && f->on_read)
    return f->on_read(f, data, size);

  isize n = read((int)fd, data, size);
  if (n < 0)
    return sys_ret_from_errno(errno);
  return (sys_ret)n;
}


static sys_ret sys_syscall_write(sys_opcode op, sys_fd fd, const void* data, usize size) {
  vfile_t* f = vfile_lookup(fd);
  if (f && f->on_write)
    return f->on_write(f, data, size);

  isize n = write((int)fd, data, size);
  if (n < 0)
    return sys_ret_from_errno(errno);
  return (sys_ret)n;
}


static sys_ret sys_syscall_sleep(sys_opcode op, usize seconds, usize nanoseconds) {
  struct timespec rqtp = { .tv_sec = seconds, .tv_nsec = nanoseconds };
  // struct timespec remaining;
  int r = nanosleep(&rqtp, 0/*&remaining*/);
  if (r == 0)
    return 0;
  if (errno == EINTR) // interrupted
    return -sys_err_canceled; // FIXME TODO pass bach "remaining time" to caller
  return -sys_err_invalid;
}


typedef sys_ret (*syscall_fun)(sys_opcode,isize,isize,isize,isize,isize);
#define FORWARD(f) MUSTTAIL return ((syscall_fun)(f))(op,arg1,arg2,arg3,arg4,arg5)

sys_ret sys_syscall(
  sys_opcode op, isize arg1, isize arg2, isize arg3, isize arg4, isize arg5)
{
  //dlog("sys_syscall %u, %ld, %ld, %ld, %ld, %ld", op,arg1,arg2,arg3,arg4,arg5);
  switch ((enum _sys_opcode)op) {
    case sys_op_init:   FORWARD(sys_syscall_init);
    case sys_op_test:   FORWARD(sys_syscall_test);
    case sys_op_exit:   FORWARD(sys_syscall_exit);
    case sys_op_openat: FORWARD(sys_syscall_openat);
    case sys_op_close:  FORWARD(sys_syscall_close);
    case sys_op_read:   FORWARD(sys_syscall_read);
    case sys_op_write:  FORWARD(sys_syscall_write);
    case sys_op_sleep:  FORWARD(sys_syscall_sleep);
  }
  return -sys_err_sys_op;
}
