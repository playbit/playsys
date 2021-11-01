# PlaySys

The Playbit System interface

PlaySys defines an OS-like computing platform which can be implemented on
a wide range of hosts like Linux, BSD, Web/WASM, macOS and Windows.
This is accomplished via two mechanisms:

1. A `syscall` for communication between a program and its host play system.
   This is just a single function call and does not depend on any specific
   programming language.

2. The filesystem for accessing resources like graphics surfaces and
   network connections. This is modeled on top of `syscall` operations.

Specification: [spec.md](spec.md)


## Source

- [`include/playsys.h`](include/playsys.h) C API
- [`examples/hello`](examples/hello/) contains an example program
- [`backends`](backends/) implementations of the playsys API for host platforms
  - [`posix`](backends/posix/)
    C implementation of playsys functions on POSIX host's libc
  - [`wgpu`](backends/wgpu/) WebGPU implementation for mac, win & linux based on
    [Dawn](https://dawn.googlesource.com/dawn)
  - [`js`](backends/js/) JavaScript implementation for web browsers


### Building

Building the "hello" example on macOS (>=10.15, x86_64) with WebGPU:

```sh
cd path/to/playsys
backends/wgpu/setup.sh
backends/wgpu/build.sh
examples/hello/build.sh -run out/hello_mac_x64
```

> Note: If you're having issues with clang/llvm, install a non-Apple version
> from for example homebrew: `brew install llvm`.

Building the "hello" example for web browsers:

_TODO_


## Concept

PlaySys is one step in a three-part strategy to birth a new software platform:

#### Step 1
PlaySys — a syscall-like API that is the interface betweet a program and the
OS/environment. It is how the program percieves and experiences reality.
Filesystem acts as an arbitrary extension to a minimal syscall API, which
includes an io_uring-like interface.
Capability based: e.g. does this system support pointer input?
GPU compute? Writable filesystem?<br>
_An "OS" from a program's perspective_<br>

#### Step 2
Libraries to make GUI & CLI development easier on top of PlaySys.<br>
_The API from an app developer's perspective_<br>

#### Step 3
Implementations of PlaySys:
- an OS using the Linux kernel providing a window manager<br>
  _An "OS" from a user's perspective_
- an implementation for web platform:<br>
  run PlaySys programs in a web browser.<br>
  _An "app" from a user's perspective_
