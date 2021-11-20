// SPDX-License-Identifier: Apache-2.0
// POSIX backend using host platform libc

#define SYS_DEBUG // define to enable debug logging
#include "sys_impl.h"

#include <fcntl.h>  // open
#include <unistd.h> // close, read, write
#include <stdlib.h> // exit
#include <string.h> // memcmp
#include <time.h>   // nanosleep
#include <sys/mman.h> // mmap
#include <sys/errno.h>
#include <sys/socket.h> // socketpair
#include <assert.h>

#define SYS_SPECIAL_FS_PREFIX "/sys"

#define _CONCAT_X(a,b) a##b
#define _CONCAT(a,b) _CONCAT_X(a,b)


// vfile syscall helper macros
#define FWD_VFILE_SYSCALL(...) _VFILE_SYSCALL_DISP(_FWD_VFILE_SYSCALL,__VA_ARGS__)
#define _FWD_VFILE_SYSCALL0(fd, op) { \
  isize n = vfile_syscall(fd,op,0,0,0,0,0); \
  if (n != VFILE_SYSCALL_DEFAULT) \
    return n; \
}
#define _FWD_VFILE_SYSCALL1(fd, op, a) { \
  isize n = vfile_syscall(fd,op,((isize)a),0,0,0,0); \
  if (n != VFILE_SYSCALL_DEFAULT) \
    return n; \
}
#define _FWD_VFILE_SYSCALL2(fd, op, a, b) { \
  isize n = vfile_syscall(fd,op,((isize)a),((isize)b),0,0,0); \
  if (n != VFILE_SYSCALL_DEFAULT) \
    return n; \
}
#define _FWD_VFILE_SYSCALL3(fd, op, a, b, c) { \
  isize n = vfile_syscall(fd,op,((isize)a),((isize)b),((isize)c),0,0); \
  if (n != VFILE_SYSCALL_DEFAULT) \
    return n; \
}
#define _FWD_VFILE_SYSCALL4(fd, op, a, b, c, d) { \
  isize n = vfile_syscall(fd,op,((isize)a),((isize)b),((isize)c),((isize)d),0); \
  if (n != VFILE_SYSCALL_DEFAULT) \
    return n; \
}
#define _FWD_VFILE_SYSCALL5(fd, op, a, b, c, d, e) { \
  isize n = vfile_syscall(fd,op,((isize)a),((isize)b),((isize)c),((isize)d),((isize)e)); \
  if (n != VFILE_SYSCALL_DEFAULT) \
    return n; \
}

#define _VFILE_SYSCALL_DISP(a,...) _CONCAT(a,_VFILE_SYSCALL_NARGS(__VA_ARGS__))(__VA_ARGS__)
#define _VFILE_SYSCALL_NARGS_X(a,b,c,d,e,f,g,h,n,...) n
#define _VFILE_SYSCALL_NARGS(...) _VFILE_SYSCALL_NARGS_X(__VA_ARGS__,6,5,4,3,2,1,)


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
// vfile on pipe or socketpair

// TODO: get rid of this older vfile implementation; use new "vfile" API instead.

// virtual file type
typedef enum {
  VFILE1_NULL,
  VFILE1_WGPU_DEV,  // p_wgpu_dev_t
  VFILE1_GUI_SURF,  // p_gui_surf_t
} vfile1_type_t;

// virtual file data
typedef struct vfile1 vfile1_t;
struct vfile1 {
  fd_t         fd_user; // user's end of pipe/channel
  fd_t         fd_impl; // implementation's end of pipe/channel
  vfile1_type_t type;
  union {
    void*         ptr;
    p_wgpu_dev_t* dev;  // type VFILE1_WGPU_DEV
    p_gui_surf_t* surf; // type VFILE1_GUI_SURF
  };
  // event handlers
  isize(*on_close)(vfile1_t*);
  isize(*on_read)(vfile1_t*, void* data, usize size);
  isize(*on_write)(vfile1_t*, const void* data, usize size);
  // if set, on_close is called just before the file's fds are closed.
  // if set, on_{read,write} OVERRIDES _psys_{read,write}.
};

// storage of open virtual files
// TODO: something better than this array for storage
static vfile1_t g_vfile1v[32] = {0};
static u32      g_vfile1c = 0;

static fd_t vfile1_open(vfile1_type_t type, usize flags, vfile1_t** f_out) {
  // allocate vfile1_t struct
  if (g_vfile1c == countof(g_vfile1v))
    return p_err_invalid; // FIXME ("out of memory" or "resources")
  vfile1_t* f = &g_vfile1v[g_vfile1c++];
  memset(f, 0, sizeof(vfile1_t));
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
    g_vfile1c--;
    return err_from_errno(errno);
  }

  f->fd_user = (fd_t)fd[0];
  f->fd_impl = (fd_t)fd[1];

  *f_out = f;
  return f->fd_user;
}


static vfile1_t* vfile1_lookup(fd_t fd_user) {
  for (u32 i = 0; i < g_vfile1c; i++) {
    if (g_vfile1v[i].fd_user == fd_user)
      return &g_vfile1v[i];
  }
  return NULL;
}


static err_t vfile1_close(vfile1_t* f) {
  int r1 = close((int)f->fd_user);
  int errno1 = errno;
  int r2 = close((int)f->fd_impl);
  int errno2 = errno;
  err_t ret = f->on_close ? f->on_close(f) : 0;

  // remove f from g_vfile1v
  for (u32 i = 0; i < g_vfile1c; i++) {
    if (&g_vfile1v[i] == f) {
      i++;
      for (; i < g_vfile1c; i++)
        g_vfile1v[i - 1] = g_vfile1v[i];
      g_vfile1c--;
      break;
    }
  }

  if (r1 != 0)
    return err_from_errno(errno1);
  if (r2 != 0)
    return err_from_errno(errno2);
  return ret;
}



// ---------------------------------------------------
// sys_syscall op implementations


static err_t _psys_mmap(
  psysop_t op, void** addr, usize length, mmapflag_t flag, fd_t fd, usize offs)
{
  if (fd > -1)
    FWD_VFILE_SYSCALL(fd, op, addr, length, flag, fd, offs)

  int prot = 0;
  if (flag & p_mmap_prot_none)  prot |= PROT_NONE;
  if (flag & p_mmap_prot_read)  prot |= PROT_READ;
  if (flag & p_mmap_prot_write) prot |= PROT_WRITE;
  if (flag & p_mmap_prot_exec)  prot |= PROT_EXEC;

  int flags = 0;
  if (flag & p_mmap_shared)    flags |= MAP_SHARED;
  if (flag & p_mmap_private)   flags |= MAP_PRIVATE;
  if (flag & p_mmap_fixed)     flags |= MAP_FIXED;
  #if defined(MAP_ANON)
  if (flag & p_mmap_anonymous) flags |= MAP_ANON;
  #endif

  // unsupported flags
  if (flag & ( p_mmap_populate
             | p_mmap_nonblock
             #if !defined(MAP_ANON)
             | p_mmap_anonymous
             #endif
  )) {
    return p_err_invalid;
  }

  void* p = mmap(*addr, length, prot, flags, fd, offs);
  if (p == MAP_FAILED)
    return p_err_nomem;
  *addr = p;
  return 0;
}


static isize _psys_test(psysop_t op, isize checkop) {
  if (op > p_sysop_write)
    return p_err_not_supported;
  return 0;
}


static isize _psys_exit(psysop_t op, isize status) {
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


static isize close_wgpu_dev(vfile1_t* f) {
  return p_wgpu_dev_close(f->dev);
}

static isize open_wgpu_dev(psysop_t op, const char* path, usize flags, isize mode) {
  if (flags & p_open_wonly)
    return p_err_invalid;
  vfile1_t* f;
  fd_t fd = vfile1_open(VFILE1_WGPU_DEV, flags, &f);
  if (fd < 0)
    return fd;
  int adapter_id = -1; // TODO: parse "path"
  p_wgpu_dev_flag_t fl = (flags & p_open_ronly) ? p_wgpu_dev_fl_ronly : 0;
  err_t e = p_wgpu_opendev(&f->dev, f->fd_impl, f->fd_user, adapter_id, fl);
  if (e < 0) {
    vfile1_close(f);
    return e;
  }
  f->on_close = close_wgpu_dev;
  return fd;
}


static isize _psys_wgpu_opendev(psysop_t op, usize flags) {
  vfile1_t* f;
  fd_t fd = vfile1_open(VFILE1_WGPU_DEV, flags, &f);
  if (fd < 0)
    return fd;
  int adapter_id = -1; // TODO allow configuration via syscall arguments
  p_wgpu_dev_flag_t fl = (p_wgpu_dev_flag_t)flags;
  err_t e = p_wgpu_opendev(&f->dev, f->fd_impl, f->fd_user, adapter_id, fl);
  if (e < 0) {
    vfile1_close(f);
    return e;
  }
  f->on_close = close_wgpu_dev;
  return fd;
}


static isize close_gui_surf(vfile1_t* f) {
  return p_gui_surf_close(f->surf);
}

static isize read_gui_surf(vfile1_t* f, void* data, usize size) {
  return p_gui_surf_read(f->surf, data, size);
}

static isize _psys_gui_mksurf(
  psysop_t op, u32 width, u32 height, fd_t device, usize flags)
{
  vfile1_t* f;
  fd_t fd = vfile1_open(VFILE1_GUI_SURF, p_open_wonly, &f);
  if (fd < 0)
    return fd;
  isize r = set_nonblock(f->fd_impl); // enable read()ing data of unknown length
  // TODO: consider using writev & readv instead.
  // TODO: consider fully virtual I/O: add buffer to p_gui_surf_t and set f->on_read.
  if (r != 0) {
    vfile1_close(f);
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
    vfile1_close(f);
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


static isize _psys_openat(
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


err_t _psys_pipe(psysop_t op, fd_t* fdp, u32 flags) {
  if (pipe(fdp) != 0)
    return err_from_errno(errno);
  return 0;
}


err_t _psys_close_host(psysop_t op, fd_t fd) {
  if (close((int)fd) != 0)
    return err_from_errno(errno);
  return 0;
}


err_t _psys_close(psysop_t op, fd_t fd) {
  vfile1_t* f1 = vfile1_lookup(fd);
  if (f1)
    return vfile1_close(f1);

  vfile_t* f = vfile_lookup(fd);
  if (f)
    return vfile_close(f);

  return _psys_close_host(0, fd);
}


static isize _psys_read(psysop_t op, fd_t fd, void* data, usize size) {
  vfile1_t* f = vfile1_lookup(fd);
  if (f && f->on_read)
    return f->on_read(f, data, size);

  FWD_VFILE_SYSCALL(fd, op, fd, data, size)

  isize n = read((int)fd, data, size);
  if (n < 0)
    return err_from_errno(errno);
  return (isize)n;
}

static isize _psys_write(psysop_t op, fd_t fd, const void* data, usize size) {
  vfile1_t* f = vfile1_lookup(fd);
  if (f && f->on_write)
    return f->on_write(f, data, size);

  FWD_VFILE_SYSCALL(fd, op, fd, data, size)

  isize n = write((int)fd, data, size);
  if (n < 0)
    return err_from_errno(errno);
  return (isize)n;
}


static isize _psys_sleep(psysop_t op, usize seconds, usize nanoseconds) {
  struct timespec rqtp = { .tv_sec = seconds, .tv_nsec = nanoseconds };
  // struct timespec remaining;
  int r = nanosleep(&rqtp, 0/*&remaining*/);
  if (r == 0)
    return 0;
  if (errno == EINTR) // interrupted
    return p_err_canceled; // FIXME TODO pass bach "remaining time" to caller
  return p_err_invalid;
}


static isize _psys_NOT_IMPLEMENTED(psysop_t op) {
  return p_err_not_supported;
}


typedef isize (*syscall_fun)(psysop_t,isize,isize,isize,isize,isize);
#define FORWARD(f) MUSTTAIL return ((syscall_fun)(f))(op,arg1,arg2,arg3,arg4,arg5)

isize p_syscall(
  psysop_t op, isize arg1, isize arg2, isize arg3, isize arg4, isize arg5)
{
  //dlog("sys_syscall %u, %ld, %ld, %ld, %ld, %ld", op,arg1,arg2,arg3,arg4,arg5);
  switch ((enum p_sysop)op) {
    case p_sysop_test:   FORWARD(_psys_test);
    case p_sysop_exit:   FORWARD(_psys_exit);
    case p_sysop_openat: FORWARD(_psys_openat);
    case p_sysop_close:  FORWARD(_psys_close);
    case p_sysop_read:   FORWARD(_psys_read);
    case p_sysop_write:  FORWARD(_psys_write);
    case p_sysop_sleep:  FORWARD(_psys_sleep);
    case p_sysop_mmap:   FORWARD(_psys_mmap);
    case p_sysop_pipe:   FORWARD(_psys_pipe);

    case p_sysop_ioring_setup:    FORWARD(_psys_ioring_setup);
    case p_sysop_ioring_enter:    FORWARD(_psys_ioring_enter);
    case p_sysop_ioring_register: FORWARD(_psys_ioring_register);

    case p_sysop_seek:     FORWARD(_psys_NOT_IMPLEMENTED);
    case p_sysop_statat:   FORWARD(_psys_NOT_IMPLEMENTED);
    case p_sysop_removeat: FORWARD(_psys_NOT_IMPLEMENTED);
    case p_sysop_renameat: FORWARD(_psys_NOT_IMPLEMENTED);

    case p_sysop_wgpu_opendev: FORWARD(_psys_wgpu_opendev);
    case p_sysop_gui_mksurf:   FORWARD(_psys_gui_mksurf);
  }
  return p_err_sys_op;
}
