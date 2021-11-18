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

typedef usize psysop_t;   // syscall operation code
typedef u32   openflag_t; // flags to openat syscall
typedef u32   mmapflag_t; // flags to mmap syscall
typedef i32   err_t;      // error code (negative values)
typedef i32   fd_t;       // file descriptor (positive values)

// constants
#define P_FDSTDIN  ((fd_t)0)    // input stream
#define P_FDSTDOUT ((fd_t)1)    // main output stream
#define P_FDSTDERR ((fd_t)2)    // logging output stream
#define P_AT_FDCWD ((fd_t)-100) // "current directory" for *at file operations

// errors (possible values of type err_t)
enum p_err {
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

// open flags (possible bits of type openflag_t)
enum p_openflag {
  p_open_ronly  =  0, // Open for reading only
  p_open_wonly  =  1, // Open for writing only
  p_open_rw     =  2, // Open for both reading and writing
  p_open_append =  4, // Start writing at end (seekable files only)
  p_open_create =  8, // Create file if it does not exist
  p_open_trunc  = 16, // Set file size to zero
  p_open_excl   = 32, // fail if file exists when create and excl are set
};

// mmap flags (possible bits of type mmapflag_t)
enum p_mmapflag {
  p_mmap_prot_none  =     0, // Pages may not be accessed
  p_mmap_prot_read  =   0x1, // Pages may be read
  p_mmap_prot_write =   0x2, // Pages may be written
  p_mmap_prot_exec  =   0x4, // Pages may be executed
  p_mmap_shared     =   0x8, // Share this mapping (impl as MAP_SHARED_VALIDATE)
  p_mmap_private    =  0x10, // Create a private copy-on-write mapping
  p_mmap_fixed      =  0x40, // Place the mapping at exactly the address addr
  p_mmap_anonymous  =  0x80, // Not backed by file, contents zero-initialized, fd argument ignored.
  p_mmap_growsdown  = 0x100, // Indicate the mapping should extend downward in memory
  p_mmap_populate   = 0x200, // Populate (prefault) page tables for a mapping
  p_mmap_nonblock   = 0x400, // use with populate to not block on prefault
  p_mmap_stack      = 0x800, // Hint to allocate mapping at an address suitable for a process or thread stack
};

// syscall operations (possible values of type psysop_t)
enum p_sysop {
  p_sysop_openat          = 257, 
  p_sysop_close           = 3, 
  p_sysop_read            = 0, 
  p_sysop_write           = 1, 
  p_sysop_seek            = 8, 
  p_sysop_statat          = 262, 
  p_sysop_removeat        = 263, 
  p_sysop_renameat        = 264, 
  p_sysop_sleep           = 230, 
  p_sysop_exit            = 60, 
  p_sysop_mmap            = 9, 
  p_sysop_pipe            = 293, 
  p_sysop_test            = 10000, 
  p_sysop_wgpu_opendev    = 10001, 
  p_sysop_gui_mksurf      = 10002, 
  p_sysop_ioring_setup    = 425, 
  p_sysop_ioring_enter    = 426, 
  p_sysop_ioring_register = 427, 
};

// p_syscall calls the host system
PSYS_EXTERN isize p_syscall(psysop_t,isize,isize,isize,isize,isize) PSYS_WARN_UNUSED;

// --- ioring ---

// P_IORING_OFF_ are magic offsets for the application to mmap the data it needs
#define P_IORING_OFF_SQ_RING 0ULL
#define P_IORING_OFF_CQ_RING 0x8000000ULL
#define P_IORING_OFF_SQES    0x10000000ULL

// flags for p_ioring_sqe_t
enum p_ioring_sqeflag {
  P_IORING_SQE_FIXED_FILE    = 1U << 0, // use fixed fileset
  P_IORING_SQE_IO_DRAIN      = 1U << 1, // issue after inflight IO
  P_IORING_SQE_IO_LINK       = 1U << 2, // links next sqe
  P_IORING_SQE_IO_HARDLINK   = 1U << 3, // like LINK, but stronger
  P_IORING_SQE_ASYNC         = 1U << 4, // always go async
  P_IORING_SQE_BUFFER_SELECT = 1U << 5, // select buffer from sqe->buf_group
};

// flags for p_ioring_cqe_t
enum p_ioring_cqeflag {
  P_IORING_CQE_F_BUFFER = 1U << 0, // the upper 16 bits are the buffer ID
  P_IORING_CQE_F_MORE =   1U << 1, // parent SQE will generate more CQE entries
};

// flags for p_ioring_sqoffsets_t
enum p_ioring_sqflag {
  P_IORING_SQ_NEED_WAKEUP = 1U << 0, // needs io_uring_enter wakeup
  P_IORING_SQ_CQ_OVERFLOW = 1U << 1, // CQ ring is overflown
};

// flags for p_ioring_cqoffsets_t
enum p_ioring_cqflag {
  P_IORING_CQ_EVENTFD_DISABLED = 1U << 0, // disable eventfd notifications
};

// flags for p_ioring_params_t.flags
enum p_ioring_setupflag {
  P_IORING_SETUP_IOPOLL     = 1U << 0, // io_context is polled
  P_IORING_SETUP_SQPOLL     = 1U << 1, // SQ poll thread
  P_IORING_SETUP_SQ_AFF     = 1U << 2, // sq_thread_cpu is valid
  P_IORING_SETUP_CQSIZE     = 1U << 3, // app defines CQ size
  P_IORING_SETUP_CLAMP      = 1U << 4, // clamp SQ/CQ ring sizes
  P_IORING_SETUP_ATTACH_WQ  = 1U << 5, // attach to existing wq
  P_IORING_SETUP_R_DISABLED = 1U << 6, // start with ring disabled
};

// flags for p_ioring_params_t.features
enum p_ioring_featflag {
  P_IORING_FEAT_SINGLE_MMAP     = 1U << 0,
  P_IORING_FEAT_NODROP          = 1U << 1,
  P_IORING_FEAT_SUBMIT_STABLE   = 1U << 2,
  P_IORING_FEAT_RW_CUR_POS      = 1U << 3,
  P_IORING_FEAT_CUR_PERSONALITY = 1U << 4,
  P_IORING_FEAT_FAST_POLL       = 1U << 5,
  P_IORING_FEAT_POLL_32BITS     = 1U << 6,
  P_IORING_FEAT_SQPOLL_NONFIXED = 1U << 7,
  P_IORING_FEAT_EXT_ARG         = 1U << 8,
  P_IORING_FEAT_NATIVE_WORKERS  = 1U << 9,
  P_IORING_FEAT_RSRC_TAGS       = 1U << 10,
};

// flags for ioring_enter syscall
enum p_ioring_enterflag {
  P_IORING_ENTER_GETEVENTS = 1U << 0,
  P_IORING_ENTER_SQ_WAKEUP = 1U << 1,
  P_IORING_ENTER_SQ_WAIT =   1U << 2,
  P_IORING_ENTER_EXT_ARG =   1U << 3,
};

// ioring_register syscall opcodes and arguments
enum {
  P_IORING_REGISTER_BUFFERS       = 0,
  P_IORING_UNREGISTER_BUFFERS     = 1,
  P_IORING_REGISTER_FILES         = 2,
  P_IORING_UNREGISTER_FILES       = 3,
  P_IORING_REGISTER_EVENTFD       = 4,
  P_IORING_UNREGISTER_EVENTFD     = 5,
  P_IORING_REGISTER_FILES_UPDATE  = 6,
  P_IORING_REGISTER_EVENTFD_ASYNC = 7,
  P_IORING_REGISTER_PROBE         = 8,
  P_IORING_REGISTER_PERSONALITY   = 9,
  P_IORING_UNREGISTER_PERSONALITY = 10,
  P_IORING_REGISTER_RESTRICTIONS  = 11,
  P_IORING_REGISTER_ENABLE_RINGS  = 12,

  // extended with tagging
  P_IORING_REGISTER_FILES2         = 13,
  P_IORING_REGISTER_FILES_UPDATE2  = 14,
  P_IORING_REGISTER_BUFFERS2       = 15,
  P_IORING_REGISTER_BUFFERS_UPDATE = 16,

  // set/clear io-wq thread affinities
  P_IORING_REGISTER_IOWQ_AFF   = 17,
  P_IORING_UNREGISTER_IOWQ_AFF = 18,

  // set/get max number of io-wq workers
  P_IORING_REGISTER_IOWQ_MAX_WORKERS = 19,

  // this goes last
  P_IORING_REGISTER_LAST
};

// p_ioring_sqoffsets_t describes an ioring submission queue
typedef struct _p_ioring_sqoffsets {
  u32 head;
  u32 tail;
  u32 ring_mask;
  u32 ring_entries;
  u32 flags; // P_IORING_SQ_ flags
  u32 dropped;
  u32 array;
  u32 resv1;
  u64 resv2;
} p_ioring_sqoffsets_t;

// p_ioring_cqoffsets_t describes an ioring completion queue
typedef struct _p_ioring_cqoffsets {
  u32 head;
  u32 tail;
  u32 ring_mask;
  u32 ring_entries;
  u32 overflow;
  u32 cqes;
  u32 flags; // P_IORING_CQ_ flags
  u32 resv1;
  u64 resv2;
} p_ioring_cqoffsets_t;

// ioring configuration, passed to ioring_setup. Updated with info on success.
typedef struct _p_ioring_params {
  u32 sq_entries;
  u32 cq_entries;
  u32 flags; // P_IORING_SETUP_ flags
  u32 sq_thread_cpu;
  u32 sq_thread_idle;
  u32 features; // P_IORING_FEAT_ flags
  u32 wq_fd;
  u32 resv[3];
  p_ioring_sqoffsets_t sq_off;
  p_ioring_cqoffsets_t cq_off;
} p_ioring_params_t;

// ioring submission queue entry ("SQE")
typedef struct _p_ioring_sqe {
  u8   opcode; // type of operation for this sqe
  u8   flags;  // P_IORING_SQE_ flags
  u16  ioprio; // ioprio for the request
  fd_t fd;     // file descriptor to do IO on
  union {
    u64 off;  // offset into file
    u64 addr2;
  };
  union {
    u64 addr; // pointer to buffer or iovecs
    u64 splice_off_in;
  };
  u32 len; // buffer size or number of iovecs
  union {
    int rw_flags; // kernel_rwf_t (see linux/fs.h)
    u32 fsync_flags;
    u16 poll_events;   // compatibility
    u32 poll32_events; // word-reversed for BE
    u32 sync_range_flags;
    u32 msg_flags;
    u32 timeout_flags;
    u32 accept_flags;
    u32 cancel_flags;
    u32 open_flags;
    u32 statx_flags;
    u32 fadvise_advice;
    u32 splice_flags;
    u32 rename_flags;
    u32 unlink_flags;
    u32 hardlink_flags;
  };
  u64 user_data;  // data to be passed back at completion time
  // pack this to avoid bogus arm OABI complaints
  union {
    u16 buf_index; // index into fixed buffers, if used
    u16 buf_group; // for grouped buffer selection
  } __attribute__((packed));
  u16 personality; // personality to use, if used
  union {
    fd_t splice_fd_in;
    u32  file_index;
  };
  u64 __pad2[2];
} p_ioring_sqe_t;

// ioring completion queue entry ("CQE")
typedef struct _p_ioring_cqe {
  u64 user_data; // sqent->user_data
  i32 res;       // result code for this event
  u32 flags;     // P_IORING_CQE_ flags
} p_ioring_cqe_t;


// --- syscall interface functions ---

static fd_t p_syscall_openat(fd_t base, const char* path, openflag_t flags, usize mode);
static err_t p_syscall_close(fd_t fd);
static isize p_syscall_read(fd_t fd, void* data, usize nbyte);
static isize p_syscall_write(fd_t fd, const void* data, usize nbyte);
static err_t p_syscall_removeat(fd_t base, const char* path, usize flags);
static err_t p_syscall_renameat(fd_t oldbase, const char* oldpath, fd_t newbase,
  const char* newpath);
static isize p_syscall_sleep(usize seconds, usize nanoseconds);
static err_t p_syscall_exit(i32 status_code);
static err_t p_syscall_mmap(void** addr, usize length, mmapflag_t flag, fd_t fd,
  usize offs);
static err_t p_syscall_pipe(fd_t* fdv, u32 flags);
static err_t p_syscall_test(psysop_t op);
static fd_t p_syscall_wgpu_opendev(usize flags);
static fd_t p_syscall_gui_mksurf(u32 width, u32 height, fd_t device, usize flags);
static fd_t p_syscall_ioring_setup(u32 entries, p_ioring_params_t* params);
static isize p_syscall_ioring_enter(fd_t ring, u32 to_submit, u32 min_complete, u32 flags);
static isize p_syscall_ioring_register(fd_t ring, u32 opcode, const void* arg,
  u32 nr_args);

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

inline static fd_t p_syscall_openat(fd_t base, const char* path, openflag_t flags,
  usize mode) {
  return (fd_t)_p_syscall4(p_sysop_openat, (isize)base, (isize)path, (isize)flags,
    (isize)mode);
}
inline static err_t p_syscall_close(fd_t fd) {
  return (err_t)_p_syscall1(p_sysop_close, (isize)fd);
}
inline static isize p_syscall_read(fd_t fd, void* data, usize nbyte) {
  return _p_syscall3(p_sysop_read, (isize)fd, (isize)data, (isize)nbyte);
}
inline static isize p_syscall_write(fd_t fd, const void* data, usize nbyte) {
  return _p_syscall3(p_sysop_write, (isize)fd, (isize)data, (isize)nbyte);
}
inline static err_t p_syscall_removeat(fd_t base, const char* path, usize flags) {
  return (err_t)_p_syscall3(p_sysop_removeat, (isize)base, (isize)path, (isize)flags);
}
inline static err_t p_syscall_renameat(fd_t oldbase, const char* oldpath, fd_t newbase,
  const char* newpath) {
  return (err_t)_p_syscall4(p_sysop_renameat, (isize)oldbase, (isize)oldpath,
    (isize)newbase, (isize)newpath);
}
inline static isize p_syscall_sleep(usize seconds, usize nanoseconds) {
  return _p_syscall2(p_sysop_sleep, (isize)seconds, (isize)nanoseconds);
}
inline static err_t p_syscall_exit(i32 status_code) {
  return (err_t)_p_syscall1(p_sysop_exit, (isize)status_code);
}
inline static err_t p_syscall_mmap(void** addr, usize length, mmapflag_t flag, fd_t fd,
  usize offs) {
  return (err_t)_p_syscall5(p_sysop_mmap, (isize)addr, (isize)length, (isize)flag,
    (isize)fd, (isize)offs);
}
inline static err_t p_syscall_pipe(fd_t* fdv, u32 flags) {
  return (err_t)_p_syscall2(p_sysop_pipe, (isize)fdv, (isize)flags);
}
inline static err_t p_syscall_test(psysop_t op) {
  return (err_t)_p_syscall1(p_sysop_test, (isize)op);
}
inline static fd_t p_syscall_wgpu_opendev(usize flags) {
  return (fd_t)_p_syscall1(p_sysop_wgpu_opendev, (isize)flags);
}
inline static fd_t p_syscall_gui_mksurf(u32 width, u32 height, fd_t device, usize flags) {
  return (fd_t)_p_syscall4(p_sysop_gui_mksurf, (isize)width, (isize)height, (isize)device,
    (isize)flags);
}
inline static fd_t p_syscall_ioring_setup(u32 entries, p_ioring_params_t* params) {
  return (fd_t)_p_syscall2(p_sysop_ioring_setup, (isize)entries, (isize)params);
}
inline static isize p_syscall_ioring_enter(fd_t ring, u32 to_submit, u32 min_complete,
  u32 flags) {
  return _p_syscall4(p_sysop_ioring_enter, (isize)ring, (isize)to_submit,
    (isize)min_complete, (isize)flags);
}
inline static isize p_syscall_ioring_register(fd_t ring, u32 opcode, const void* arg,
  u32 nr_args) {
  return _p_syscall4(p_sysop_ioring_register, (isize)ring, (isize)opcode, (isize)arg,
    (isize)nr_args);
}

// p_err_str returns the symbolic name of an error as a string
inline static const char* p_err_str(err_t e) {
  switch ((enum p_err)e) {
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
