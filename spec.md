# playsys specification (draft)

## Types & constants

### Concrete types

type   | size
-------|-------------------------------
i8     | 8 bits
u8     | 8 bits
i16    | 16 bits
u16    | 16 bits
i32    | 32 bits
u32    | 32 bits
i64    | 64 bits
u64    | 64 bits
f32    | 32 bits
f64    | 64 bits
isize  | >=arch address
usize  | >=arch address
ptr    | arch address (`void*` in C)

### Symbolic types

name        | type       | purpose
------------|------------|--------------------------------------------------
sys_opcode  | usize      | syscall operation code
sys_ret     | isize      | syscall return value. Errors are negative values.
sys_err     | isize      | error code. Only negative values.
sys_fd      | isize      | file descriptor
cstr        | const u8[] | null-terminated array of UTF-8 text (`const char*` in C)

### Symbolic constants

name           | type    | value | purpose
---------------|---------|-------|----------------------------------------
SYS_STDIN      | sys_fd  | 0     | input stream
SYS_STDOUT     | sys_fd  | 1     | main output stream
SYS_STDERR     | sys_fd  | 2     | logging output stream
SYS_AT_FDCWD   | sys_fd  | -100  | "current directory" for `sys_op_*at` file operations


## Syscall

The playsys API interface is the single function `sys_syscall` which takes an operation code
(`sys_opcode`) specifying the operation to perform by the host platform along with arguments
which varies by operation.

    sys_syscall(sys_opcode, ...isize) → sys_ret

The return value `sys_ret` is interpreted as an error if negative and operation-specific
if positive or zero.


### Calling convention

`sys_syscall` uses architecture-native calling conventions:

Architecture | Calling convention
-------------|-----------------------------------------------------------
x86_64       | AMD64 System V (args 1-6 in RDI, RSI, RDX, RCX, R8, R9)
aarch64      | AAPCS64 (args 1-8 in R0, R1, R2, R3, R4, R5, R6, R7)
wasm         | WASM (args on stack)

#### Calling convention notes

- Linux amd64 uses R10 instead of RCX


### Syscall operations

name                            | sys_opcode | arguments
--------------------------------|------:|---------------------------------------------
[sys_op_openat](#sys_op_openat) |   257 | base sys_fd, path cstr, flags usize, mode isize
[sys_op_close](#sys_op_close)   |     3 | sys_fd
[sys_op_read](#sys_op_read)     |     0 | sys_fd, data ptr, usize
[sys_op_write](#sys_op_write)   |     1 | sys_fd, data const ptr, usize
[sys_op_seek](#sys_op_seek)     |     8 | _TODO_
[sys_op_statat](#sys_op_statat) |   262 | _TODO_ (newfstatat in linux, alt: statx 332)
[sys_op_removeat](#sys_op_removeat)|263 | base sys_fd, path cstr, flags usize
[sys_op_renameat](#sys_op_renameat)|264 | oldbase sys_fd, oldpath cstr, newbase sys_fd, newpath cstr
[sys_op_sleep](#sys_op_sleep)   |   230 | seconds usize, nanoseconds usize
[sys_op_exit](#sys_op_exit)     |    60 | status_code i32
[sys_op_test](#sys_op_test)     | 10000 | op sys_opcode

##### Syscall operations under consideration

name                            | sys_opcode | comments
--------------------------------|------:|---------------------------------------------
sys_op_mkdirat                  |   258 | can we use openat with a flag instead?
sys_op_ioring_setup             |   425 | count u32, \*io_uring_params_t
sys_op_ioring_enter             |   426 | ring sys_fd, to_submit u32, min_complete u32, flags u32, \*sigset_t
sys_op_ioring_register          |   427 | ring sys_fd, opcode u32, arg ptr, nr_args u32
sys_op_readv                    |    19 | Needed for ioring
sys_op_writev                   |    20 | Needed for ioring
sys_op_memmap                   |     9 | Needed for ioring
sys_op_memunmap                 |    11 | Needed for ioring
sys_op_ioctl                    |    16 | Needed to set nonblock flags on FDs
sys_op_symlinkat                |   265 | create symbolic file link
sys_op_readlinkat               |   267 | query symbolic file link
sys_op_chmodat                  |   268 | change file mode
sys_op_chownat                  |   260 | change [file ownership](#file-ownership)
sys_op_mount                    |   165 | mount a filesystem
sys_op_unmount                  |   166 | unmount a filesystem

##### Syscall operations notes

- `sys_opcode` values match the corresponding Linux syscall numbers to make implementation
  for Linux simpler. `sys_opcode >= 10000` are used for playsys-specific operations.

- `sys_op_ioring_*` is a proposal to implement async I/O like Linux's
  [io_uring](https://kernel.dk/io_uring.pdf) interface. Programs running on
  Linux could be directly interfacing with io_uring for ideal performance
  while async-only platforms like Web would benefit from an async API to avoid
  rewriting WASM code with e.g. `wasm-opt --asyncify`.
  See [linux:tools/io_uring/syscall.c](https://github.com/torvalds/linux/blob/v5.14/tools/io_uring/syscall.c) for the Linux implementation.

- `sys_op_sleep` may be better modeled like Linux's `clock_nanosleep`, specifically
  in regards to a "remainder" return value.
  See `musl/src/time/clock_nanosleep.c`


#### sys_op_test

Test if an operation is supported.

    sys_op_test → 0 | sys_err_not_supported
      op sys_opcode


For example, on a platform like WASM which lacks the ability to terminate a process,
`sys_syscall(sys_op_test, sys_op_exit)` returns `sys_err_not_supported`.
Similarly, attempting to perform an unsupported operation yeilds the same result:
`sys_syscall(sys_op_exit, 123)` also returns `sys_err_not_supported`.


#### sys_op_openat

Open a file at `path` relative to `base`.

    sys_op_openat → sys_fd | sys_err
      base  sys_fd
      path  cstr
      flags usize
      mode  isize


If the path is not absolute it is interpreted as relative to `base`.
An absolue path is one that begins with the byte `"/"`.
Use `SYS_AT_FDCWD` as `base` to resolve paths relative to the program process's
"current working directory".

The `flags` argument specifies conditions and result. See [open flags](#open-flags)

The `mode` argument specifies the file mode of files created and is only used when the `sys_open_create` flag is set.

##### open flags

name            | value | effect
----------------|------:|---------------------------------------------
sys_open_ronly  |     0 | Open for reading only
sys_open_wonly  |     1 | Open for writing only
sys_open_rw     |     2 | Open for both reading and writing
sys_open_append |     4 | Start writing at end (seekable files only)
sys_open_create |     8 | Create file if it does not exist
sys_open_trunc  |    16 | Set file size to zero
sys_open_excl   |    32 | If both `sys_open_create` and `sys_open_excl` are set, then openat fails if the specified file already exists.

##### file mode

_[TODO]_
See [\<sys/stat.h>](https://pubs.opengroup.org/onlinepubs/007904875/basedefs/sys/stat.h.html)



## Filesystems

While the syscall API provides the basis for interfacing with the playsys platform,
the filesystem gives access to a wider set of features and resources.

The filesystem API is modeled on top of the file-oriented syscalls
openat, close, read, write, etc.

Example:

```c
// read uname and print results to console
char buf[128];
sys_fd fd = openat(0, "/sys/uname", sys_open_ronly, 0);
isize n = read(fd, buf, sizeof(buf));
close(fd);
write(SYS_FD_STDOUT, buf, n);
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
  sys_fd wgpu_dev = open("/sys/wgpu/dev/gpu0", sys_open_ronly, 0);

  // create a graphics surface (e.g. a window or HTMLCanvas object)
  sys_fd wgpu_surf = open("/sys/wgpu/surface", sys_open_rw, 0);
  write_cstr(wgpu_surf,
    "width  400\n"
    "height 300\n"
    "title  Hello world\n");

  // configure a WebGPU API interface
  wgpu_ctx_t* ctx = wgpu_ctx_create(ctx_mem);
  WGPUDevice device = wgpu_ctx_set_device(ctx, wgpu_dev);
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
  close(wgpu_dev);  // release GPU device handle

  return 0;
}
```
