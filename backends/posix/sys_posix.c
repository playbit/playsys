// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

// POSIX backend using host platform libc

#include <playsys.h>
#include <playwgpu.h> // backend interface

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


extern int errno;

static err_t err_from_errno(int e) {
  // TODO
  return p_err_invalid;
}


static err_t set_nonblock(fd_t fd) {
  int flags = fcntl(fd, F_GETFL);
  if (flags < 0)
    return err_from_errno(errno);
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    return err_from_errno(errno);
  return 0;
}


// ---------------------------------------------------
// sys_syscall op implementations


static isize sys_syscall_test(psysop_t op, isize checkop) {
  if (op > p_sysop_write)
    return p_err_not_supported;
  return 0;
}


static isize sys_syscall_exit(psysop_t op, isize status) {
  exit((int)status);
  return 0;
}


static isize open_uname(psysop_t op, const char* path, usize flags, isize mode) {
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
    return err_from_errno(errno);
  const char* s = UNAME_STR " " XSTR(SYS_API_VERSION) "\n";
  int w = write(fd[1], s, strlen(s));
  close(fd[1]);
  if (w < 0) {
    close(fd[0]);
    return err_from_errno(errno);
  }
  return (isize)fd[0];
}



typedef enum {
  VFILE_NULL,
  VFILE_WGPU_DEV,  // p_wgpu_dev_t
  VFILE_GUI_SURF, // p_gui_surf_t
} vfile_type_t;

typedef struct vfile {
  fd_t         fd_user; // user's end of pipe/channel
  fd_t         fd_impl; // implementation's end of pipe/channel
  vfile_type_t type;
  union {
    void*         ptr;
    p_wgpu_dev_t* dev;  // type VFILE_WGPU_DEV
    p_gui_surf_t* surf; // type VFILE_GUI_SURF
  };
  isize(*on_close)(struct vfile*);
  isize(*on_read)(struct vfile*, void* data, usize size);
  isize(*on_write)(struct vfile*, const void* data, usize size);
  // if set, on_close is called just before the file's fds are closed.
  // if set, on_{read,write} OVERRIDES sys_syscall_{read,write}.
} vfile_t;

static vfile_t g_vfilev[32] = {0};
static u32     g_vfilec = 0;


static fd_t vfile_open(vfile_type_t type, usize flags, vfile_t** f_out) {
  // allocate vfile_t struct
  if (g_vfilec == sys_countof(g_vfilev))
    return p_err_invalid; // FIXME ("out of memory" or "resources")
  vfile_t* f = &g_vfilev[g_vfilec++];
  memset(f, 0, sizeof(vfile_t));
  f->type = type;

  // create pipe or channel
  int r;
  int fd[2];
  if (flags & p_open_rw) {
    r = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
  } else {
    r = pipe(fd); // fd[0] = readable, fd[1] writable
    if (flags & p_open_wonly) {
      // swap so that fd_user is writable and fd_impl is readable.
      // i.e. after the swap: fd[0] = writable, fd[1] readable
      int fd0 = fd[0];
      fd[0] = fd[1];
      fd[1] = fd0;
    }
  }
  if (r < 0) {
    g_vfilec--;
    return err_from_errno(errno);
  }

  f->fd_user = (fd_t)fd[0];
  f->fd_impl = (fd_t)fd[1];

  *f_out = f;
  return f->fd_user;
}


static vfile_t* vfile_lookup(fd_t fd_user) {
  for (u32 i = 0; i < g_vfilec; i++) {
    if (g_vfilev[i].fd_user == fd_user)
      return &g_vfilev[i];
  }
  return NULL;
}


static isize vfile_close(vfile_t* f) {
  int r1 = close((int)f->fd_user);
  int errno1 = errno;
  int r2 = close((int)f->fd_impl);
  int errno2 = errno;
  isize ret = f->on_close ? f->on_close(f) : 0;

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
    return err_from_errno(errno1);
  if (r2 != 0)
    return err_from_errno(errno2);
  return ret;
}


static isize close_wgpu_dev(vfile_t* f) {
  return p_wgpu_dev_close(f->dev);
}

static isize open_wgpu_dev(psysop_t op, const char* path, usize flags, isize mode) {
  if (flags & p_open_wonly)
    return p_err_invalid;
  vfile_t* f;
  fd_t fd = vfile_open(VFILE_WGPU_DEV, flags, &f);
  if (fd < 0)
    return fd;
  int adapter_id = -1; // TODO: parse "path"
  p_wgpu_dev_flag_t fl = (flags & p_open_ronly) ? p_wgpu_dev_fl_ronly : 0;
  err_t e = p_wgpu_opendev(&f->dev, f->fd_impl, f->fd_user, adapter_id, fl);
  if (e < 0) {
    vfile_close(f);
    return e;
  }
  f->on_close = close_wgpu_dev;
  return fd;
}


static isize sys_syscall_wgpu_opendev(psysop_t op, usize flags) {
  vfile_t* f;
  fd_t fd = vfile_open(VFILE_WGPU_DEV, flags, &f);
  if (fd < 0)
    return fd;
  int adapter_id = -1; // TODO allow configuration via syscall arguments
  p_wgpu_dev_flag_t fl = (p_wgpu_dev_flag_t)flags;
  err_t e = p_wgpu_opendev(&f->dev, f->fd_impl, f->fd_user, adapter_id, fl);
  if (e < 0) {
    vfile_close(f);
    return e;
  }
  f->on_close = close_wgpu_dev;
  return fd;
}


static isize close_gui_surf(vfile_t* f) {
  return p_gui_surf_close(f->surf);
}

static isize read_gui_surf(vfile_t* f, void* data, usize size) {
  return p_gui_surf_read(f->surf, data, size);
}

static isize sys_syscall_gui_mksurf(
  psysop_t op, u32 width, u32 height, fd_t device, usize flags)
{
  vfile_t* f;
  fd_t fd = vfile_open(VFILE_GUI_SURF, p_open_wonly, &f);
  if (fd < 0)
    return fd;
  isize r = set_nonblock(f->fd_impl); // enable read()ing data of unknown length
  // TODO: consider using writev & readv instead.
  // TODO: consider fully virtual I/O: add buffer to p_gui_surf_t and set f->on_read.
  if (r != 0) {
    vfile_close(f);
    return r;
  }
  p_gui_surf_descr_t d = {
    .width = width,
    .height = height,
    .flags = flags,
    .device = device,
  };
  r = p_gui_surf_open(&f->surf, f->fd_impl, f->fd_user, &d);
  if (r < 0) {
    vfile_close(f);
    return r;
  }
  f->on_close = close_gui_surf;
  f->on_read = read_gui_surf;
  return fd;
}


static isize open_special(psysop_t op, const char* path, usize flags, isize mode) {
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
  ROUTE_PREFIX("wgpu/dev/gpu", open_wgpu_dev);

  #undef ROUTE
  return p_err_not_found;
}


static isize sys_syscall_openat(
  psysop_t op, fd_t atfd, const char* path, usize flags, isize mode)
{
  if (atfd == P_AT_FDCWD) {
    if (P_AT_FDCWD != (fd_t)AT_FDCWD)
      atfd = (fd_t)AT_FDCWD; // libc
  }

  if (strlen(path) > strlen(SYS_SPECIAL_FS_PREFIX) &&
      memcmp(path, SYS_SPECIAL_FS_PREFIX "/", 5) == 0)
  {
    return open_special(op, path, flags, mode);
  }

  static const int oflag_map[3] = {
    [p_open_ronly] = O_RDONLY,
    [p_open_wonly] = O_WRONLY,
    [p_open_rw]    = O_RDWR,
  };
  int oflag = oflag_map[flags & 3]; // first two bits is ro/wo/rw
  if (flags & p_open_append) oflag |= O_APPEND;
  if (flags & p_open_create) oflag |= O_CREAT;
  if (flags & p_open_trunc)  oflag |= O_TRUNC;
  if (flags & p_open_excl)   oflag |= O_EXCL;

  int fd = openat((int)atfd, path, oflag, (mode_t)mode);
  if (fd < 0) {
    // dlog("open failed => %d (errno %d %s)", fd, errno, strerror(errno));
    return err_from_errno(errno);
  }

  return (isize)fd;
}


static isize sys_syscall_close(psysop_t op, fd_t fd) {
  vfile_t* f = vfile_lookup(fd);
  if (f)
    return vfile_close(f);

  if (close((int)fd) != 0)
    return err_from_errno(errno);

  return 0;
}


static isize sys_syscall_read(psysop_t op, fd_t fd, void* data, usize size) {
  vfile_t* f = vfile_lookup(fd);
  if (f && f->on_read)
    return f->on_read(f, data, size);

  isize n = read((int)fd, data, size);
  if (n < 0)
    return err_from_errno(errno);
  return (isize)n;
}


static isize sys_syscall_write(psysop_t op, fd_t fd, const void* data, usize size) {
  vfile_t* f = vfile_lookup(fd);
  if (f && f->on_write)
    return f->on_write(f, data, size);

  isize n = write((int)fd, data, size);
  if (n < 0)
    return err_from_errno(errno);
  return (isize)n;
}


static isize sys_syscall_sleep(psysop_t op, usize seconds, usize nanoseconds) {
  struct timespec rqtp = { .tv_sec = seconds, .tv_nsec = nanoseconds };
  // struct timespec remaining;
  int r = nanosleep(&rqtp, 0/*&remaining*/);
  if (r == 0)
    return 0;
  if (errno == EINTR) // interrupted
    return p_err_canceled; // FIXME TODO pass bach "remaining time" to caller
  return p_err_invalid;
}


static isize sys_syscall_NOT_IMPLEMENTED(psysop_t op) {
  return p_err_not_supported;
}


typedef isize (*syscall_fun)(psysop_t,isize,isize,isize,isize,isize);
#define FORWARD(f) MUSTTAIL return ((syscall_fun)(f))(op,arg1,arg2,arg3,arg4,arg5)

isize p_syscall(
  psysop_t op, isize arg1, isize arg2, isize arg3, isize arg4, isize arg5)
{
  //dlog("sys_syscall %u, %ld, %ld, %ld, %ld, %ld", op,arg1,arg2,arg3,arg4,arg5);
  switch ((enum _p_sysop)op) {
    case p_sysop_test:   FORWARD(sys_syscall_test);
    case p_sysop_exit:   FORWARD(sys_syscall_exit);
    case p_sysop_openat: FORWARD(sys_syscall_openat);
    case p_sysop_close:  FORWARD(sys_syscall_close);
    case p_sysop_read:   FORWARD(sys_syscall_read);
    case p_sysop_write:  FORWARD(sys_syscall_write);
    case p_sysop_sleep:  FORWARD(sys_syscall_sleep);

    case p_sysop_seek:     FORWARD(sys_syscall_NOT_IMPLEMENTED);
    case p_sysop_statat:   FORWARD(sys_syscall_NOT_IMPLEMENTED);
    case p_sysop_removeat: FORWARD(sys_syscall_NOT_IMPLEMENTED);
    case p_sysop_renameat: FORWARD(sys_syscall_NOT_IMPLEMENTED);

    case p_sysop_wgpu_opendev: FORWARD(sys_syscall_wgpu_opendev);
    case p_sysop_gui_mksurf:   FORWARD(sys_syscall_gui_mksurf);
  }
  return p_err_sys_op;
}
