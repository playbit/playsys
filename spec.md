# playsys specification (draft)

## Types & constants

### Concrete types

type   | size
-------|-------------------------------
i8     | 8 bit signed integer
u8     | 8 bit unsigned integer
i16    | 16 bit signed integer
u16    | 16 bit unsigned integer
i32    | 32 bit signed integer
u32    | 32 bit unsigned integer
i64    | 64 bit signed integer
u64    | 64 bit unsigned integer
f32    | 32 bit IEEE 754 floating-point number
f64    | 64 bit IEEE 754 floating-point number
isize  | arch-address sized signed integer
usize  | arch-address sized unsigned integer
ptr    | memory address (a pointer; `void*` in C)
cstr   | array of bytes with a 0 terminator byte (UTF-8)


### Symbolic types

[](# ":types")

name       | type | purpose
-----------|------|---------------------------------------------------------
err        | i32  | error code (negative values)
fd         | i32  | file descriptor (positive values)
psysop     | u32  | syscall operation code
openflag   | u32  | flags to openat syscall
mmapflag   | u32  | flags to mmap syscall
gpudevflag | u32  | flags to gpudev syscall


### Symbolic constants

[](# ":constants")

name       | type | value | purpose
-----------|------|-------|---------------------------------------------------
FDSTDIN    | fd   | 0     | input stream
FDSTDOUT   | fd   | 1     | main output stream
FDSTDERR   | fd   | 2     | logging output stream
AT_FDCWD   | fd   | -100  | "current directory" for `*at` file operations


### Errors

Errors are of type `err` and are negative values, with the exception of `none`.

[](# ":errors")

name           | meaning
---------------|--------------------------------------------------------------
none           | no error
invalid        | invalid data or argument
sys_op         | invalid syscall op or syscall op data
badfd          | invalid file descriptor
bad_name       | invalid or misformed name
not_found      | resource not found
name_too_long  |
canceled       | operation canceled
not_supported  | not supported
exists         | already exists
end            | end of resource
access         | permission denied
nomem          | cannot allocate memory
mfault         | bad memory address
overflow       | value too large for defined data type


## Syscall

The playsys API interface is the single function `sys_syscall` which takes an operation code
(`psysop`) specifying the operation to perform by the host platform along with arguments
which varies by operation.

    sys_syscall(psysop, ...isize) → isize

The return value is interpreted as an error if negative and operation-specific
if positive or zero.


### Calling convention

`sys_syscall` uses architecture-native calling conventions:

Architecture | Calling convention
-------------|----------------------------------------------------------------
x86_64       | AMD64 System V (args 1-6 in RDI, RSI, RDX, RCX, R8, R9)
aarch64      | AAPCS64 (args 1-8 in R0, R1, R2, R3, R4, R5, R6, R7)
wasm         | WASM (args on stack)

#### Calling convention notes

- Linux amd64 uses R10 instead of RCX


### Syscall operations

[](# ":sysops")

name                      | psysop | arguments
--------------------------|-------:|--------------------------------------------
[openat](#openat)         |    257 | base fd, path cstr, flags openflag, mode usize -> fd
[close](#close)           |      3 | fd fd -> err
[read](#read)             |      0 | fd fd, data mutptr, nbyte usize
[write](#write)           |      1 | fd fd, data ptr, nbyte usize
[seek](#seek)             |      8 | _TODO_
[statat](#statat)         |    262 | _TODO_ (newfstatat in linux, alt: statx 332) -> err
[removeat](#removeat)     |    263 | base fd, path cstr, flags u32 -> err
[renameat](#renameat)     |    264 | oldbase fd, oldpath cstr, newbase fd, newpath cstr -> err
[sleep](#sleep)           |    230 | seconds usize, nanoseconds usize
[exit](#exit)             |     60 | status_code i32 -> err
[mmap](#mmap)             |      9 | addr \*ptr, length usize, flag mmapflag, fd fd, offs usize -> err
[pipe](#pipe)             |    293 | fdv \*fd, flags u32 -> err
[test](#test)             |  10000 | op psysop -> err
[gpudev](#gpudev)         |  10001 | flags gpudevflag -> fd
[gui_mksurf](#gui_mksurf) |  10002 | width u32, height u32, device fd, flags u32 -> fd
[ioring_setup](#ioring_setup)       | 425 | entries u32, params \*ioring_params -> fd
[ioring_enter](#ioring_enter)       | 426 | ring fd, to_submit u32, min_complete u32, flags u32
[ioring_register](#ioring_register) | 427 | ring fd, opcode u32, arg ptr, nr_args u32

Wherever possible, syscalls should match
[Linux](https://github.com/torvalds/linux/blob/v5.15/arch/ia64/kernel/syscalls/syscall.tbl)
to make the implementation on Linux simpler.

#### mmap

Map files or devices into memory, or allocate memory

    mmap → err
      addr    ptrptr
      length  usize
      flag    mmap_flag
      fd      fd
      offs    usize

##### mmap flags

[](# ":mmap_flags")

name       |  value | effect
-----------|-------:|--------------------------------------------------------------
prot_none  |      0 | Pages may not be accessed
prot_read  |    0x1 | Pages may be read
prot_write |    0x2 | Pages may be written
prot_exec  |    0x4 | Pages may be executed
shared     |    0x8 | Share this mapping (impl as MAP_SHARED_VALIDATE)
private    |   0x10 | Create a private copy-on-write mapping
fixed      |   0x40 | Place the mapping at exactly the address `addr`
anonymous  |   0x80 | Not backed by file, contents zero-initialized, fd argument ignored.
populate   |  0x100 | Populate (prefault) page tables for a mapping
nonblock   |  0x200 | use with `populate` to not block on prefault


#### ioring_setup

Create an I/O ring context

    ioring_setup → fd | err
      entries u32             Queue size
      params  *ioring_params

Adapted from
[Linux's io_uring](https://github.com/torvalds/linux/blob/v5.15/include/uapi/linux/io_uring.h)
([kernel impl](https://github.com/torvalds/linux/blob/v5.15/fs/io_uring.c))



#### gpudev

Allocate handle to a WGPU device

    gpudev → fd | err
      flags gpudevflag

##### gpudev flags

[](# ":gpudev_flags")

name       |  value | effect
-----------|-------:|--------------------------------------------------------------
powhigh    |    0x1 | Request high-performance adapter
powlow     |    0x2 | Request low-energy adapter
software   |    0x4 | Force software driver to be used


#### gui_mksurf

Creates a graphics surface

    gui_mksurf → fd | err
      width  u32   Width in dp units. 0 to let the host decide.
      height u32   Height in dp units. 0 to let the host decide.
      device fd    GPU device. -1 to let the host decide.
      flags  u32   (Currently no flags. Pass 0)


#### test

Test if an operation is supported.

    test → 0 | err_not_supported
      op psysop


For example, on a platform like WASM which lacks the ability to terminate a process,
`sys_syscall(test, exit)` returns `err_not_supported`.
Similarly, attempting to perform an unsupported operation yeilds the same result:
`sys_syscall(exit, 123)` also returns `err_not_supported`.


#### openat

Open a file at `path` relative to `base`.

    openat → fd | err
      base  fd
      path  cstr
      flags usize
      mode  isize


If the path is not absolute it is interpreted as relative to `base`.
An absolue path is one that begins with the byte `"/"`.
Use `SYS_AT_FDCWD` as `base` to resolve paths relative to the program process's
"current working directory".

The `flags` argument specifies conditions and result. See [open flags](#open-flags)

The `mode` argument specifies the file mode of files created and is only used
when the `create` open flag is set.

##### open flags

[](# ":open_flags")

name   | value | effect
-------|------:|--------------------------------------------------------------
ronly  |     0 | Open for reading only
wonly  |     1 | Open for writing only
rw     |     2 | Open for both reading and writing
append |     4 | Start writing at end (seekable files only)
create |     8 | Create file if it does not exist
trunc  |    16 | Set file size to zero
excl   |    32 | fail if file exists when `create` and `excl` are set

##### file mode

_[TODO]_
See [\<sys/stat.h>](https://pubs.opengroup.org/onlinepubs/007904875/basedefs/sys/stat.h.html)




##### Syscall operations under consideration

name              | psysop | comments
------------------|-------:|-------------------------------------
mkdirat           |    258 | can we use openat with a flag instead?
readv             |     19 | Needed for ioring
writev            |     20 | Needed for ioring
mmap              |      9 | Needed for ioring
munmap            |     11 | Needed for ioring
ioctl             |     16 | Needed to set nonblock flags on FDs
symlinkat         |    265 | create symbolic file link
readlinkat        |    267 | query symbolic file link
chmodat           |    268 | change file mode
chownat           |    260 | change [file ownership](#file-ownership)
mount             |    165 | mount a filesystem
unmount           |    166 | unmount a filesystem

##### Syscall operations notes

- `psysop` values match the corresponding Linux syscall numbers to make implementation
  for Linux simpler. `psysop >= 10000` are used for playsys-specific operations.

- `ioring_*` is a proposal to implement async I/O like Linux's
  [io_uring](https://kernel.dk/io_uring.pdf) interface. Programs running on
  Linux could be directly interfacing with io_uring for ideal performance
  while async-only platforms like Web would benefit from an async API to avoid
  rewriting WASM code with e.g. `wasm-opt --asyncify`.
  See [linux:tools/io_uring/syscall.c](https://github.com/torvalds/linux/blob/v5.14/tools/io_uring/syscall.c) for the Linux implementation.

- `sleep` may be better modeled like Linux's `clock_nanosleep`, specifically
  in regards to a "remainder" return value.
  See `musl/src/time/clock_nanosleep.c`



## Filesystems

While the syscall API provides the basis for interfacing with the playsys platform,
the filesystem gives access to a wider set of features and resources.

The filesystem API is modeled on top of the file-oriented syscalls
openat, close, read, write, etc.

Example:

```c
// read uname and print results to console
char buf[128];
fd_t fd = openat(0, "/sys/uname", p_open_ronly, 0);
isize n = read(fd, buf, sizeof(buf));
close(fd);
write(P_FD_STDOUT, buf, n);
```


### File ownership

Let's consider having _no file owners_ in playsys.
Namespaces and capabilities might be a better way to manage resources.


### /sys/wgpu

Graphics with a WebGPU interface.

Example

```c
#include <playsys/playsys.h>
#include <webgpu.h>

int main(int argc, const char** argv) {
  // select a GPU device
  fd_t gpudev = open("/sys/wgpu/dev/gpu0", p_open_ronly, 0);

  // create a graphics surface (e.g. a window or HTMLCanvas object)
  fd_t wgpu_surf = open("/sys/wgpu/surface", p_open_rw, 0);
  write_cstr(wgpu_surf,
    "width  400\n"
    "height 300\n"
    "title  Hello world\n");

  // configure a WebGPU API interface
  wgpu_ctx_t* ctx = wgpu_ctx_create(ctx_mem);
  WGPUDevice device = wgpu_ctx_set_device(ctx, gpudev);
  WGPUSurface surface = wgpu_ctx_set_surface(ctx, wgpu_surf);

  // From here on we can use the standard WebGPU API <webgpu.h>
  WGPUSwapChainDescriptor scdesc = {
    .format = WGPUTextureFormat_RGBA8Unorm,
    .usage  = WGPUTextureUsage_RenderAttachment,
    .width  = 400, // TODO surf->fbwidth
    .height = 400, // TODO surf->fbheight
    .presentMode = WGPUPresentMode_Mailbox,
  };
  WGPUSwapChain sc = wgpuDeviceCreateSwapChain(device, surface, &scdesc);

  for (;;) {
    // read input events from the surface; waits for surface vsync
    isize n = read(wgpu_surf, buf, sizeof(buf));
    if (n < 1) // surface gone (e.g. window closed)
      break;
    // Optionally parse events in buf.
    // Here we can execute wgpu pipelines, which writes command buffers
    // to configured devices and surfaces.
  }

  wgpu_ctx_dispose(ctx);
  close(wgpu_surf); // close the graphic surface
  close(gpudev);  // release GPU device handle

  return 0;
}
```
