// SPDX-License-Identifier: Apache-2.0
// ioring driver
// implements the following syscall handlers: (declared in base.h)
//   _psys_ioring_setup
//   _psys_ioring_enter
//   _psys_ioring_register

#include "base.h"

#if defined(__linux__)
  #include "ioring_linux.c"
#else
  #include "ioring_base.c"
  #if defined(__wasm__)
    #include "ioring_wasm.c"
  #elif defined(__MACH__) && defined(__APPLE__)
    #include "ioring_darwin.c"
  #else
    #error ioring not available for target platform
  #endif
#endif
