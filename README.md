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

- [`libplaysys`](libplaysys/) contains a C implementation of playsys

- [`libplaywgpu`](libplaywgpu/) contains a WebGPU backend based on
  [Dawn](https://dawn.googlesource.com/dawn)

- [`examples/hello`](examples/hello/) contains an example program


### Building

Building the example on macOS (>=10.15, x86_64) with WebGPU:

```sh
cd path/to/playsys
libplaywgpu/setup.sh
libplaywgpu/build.sh
examples/hello/build.sh -run out/example_mac_x64
```

> Note: If you're having issues with clang/llvm, install a non-Apple version
> from for example homebrew: `brew install llvm`.
