// SPDX-License-Identifier: Apache-2.0
// This file is conditionally included by syscall.c

// POSIX backend using host-platform libc

#include <fcntl.h>  // open
#include <unistd.h> // close, read, write
#include <stdlib.h> // exit
#include <string.h> // memcmp
#include <time.h>   // nanosleep
#include <sys/mman.h> // mmap
#include <sys/errno.h>
#include <sys/socket.h> // socketpair
#include <assert.h>


extern int errno;


static err_t err_from_errno(int e) {
  // TODO
  return p_err_invalid;
}


static err_t _psys_mmap(
  psysop_t op, void** addr, usize length, mmapflag_t flag, fd_t fd, usize offs)
{
  VFILE_JUMP_FOP(mmap, fd, p_err_not_supported, addr, length, flag, offs)

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


static isize _psys_exit(psysop_t op, isize status) {
  exit((int)status);
  return 0;
}


static isize open_special_uname(const char* path, usize flags, isize mode) {
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


static isize open_special(psysop_t op, const char* path, usize flags, isize mode) {
  path = path + strlen(SPECIAL_FS_PREFIX) + 1; // "/sys/foo/bar" => "foo/bar"
  usize pathlen = strlen(path);

  #define ROUTE(matchpath,fun) \
    if (pathlen == strlen(matchpath) && memcmp(path,(matchpath),strlen(matchpath)) == 0) \
      return (fun)(path, flags, mode)

  ROUTE("uname", open_special_uname);

  #undef ROUTE
  return p_err_not_found;
}


static isize _psys_openat(
  psysop_t op, fd_t atfd, const char* path, usize flags, isize mode)
{
  if (atfd == P_AT_FDCWD) {
    atfd = (fd_t)AT_FDCWD;
  } else {
    VFILE_JUMP_FOP(openat, atfd, p_err_not_supported, path, flags, mode)
  }

  if (strlen(path) > strlen(SPECIAL_FS_PREFIX) &&
      memcmp(path, SPECIAL_FS_PREFIX "/", 5) == 0)
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


static isize _psys_read(psysop_t op, fd_t fd, void* data, usize size) {
  VFILE_JUMP_FOP(read, fd, p_err_not_supported, data, size)
  isize n = read((int)fd, data, size);
  if (n < 0)
    return err_from_errno(errno);
  return (isize)n;
}

static isize _psys_write(psysop_t op, fd_t fd, const void* data, usize size) {
  VFILE_JUMP_FOP(write, fd, p_err_not_supported, data, size)
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
