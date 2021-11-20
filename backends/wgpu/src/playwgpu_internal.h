// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <dawn/webgpu_cpp.h>
#include <playwgpu.h> // note: after webgpu_cpp; needs <dawn/webgpu.h>
#include <dawn_native/DawnNative.h>
#include <memory> // unique_ptr
#include <vector>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>

#include "../../base/sys_impl.h"
#include "ringbuf.h"

#ifdef dlog
  #undef dlog
#endif
#ifdef PWGPU_DEBUG
  #ifndef DLOG_PREFIX
    #define DLOG_PREFIX "\e[1;34m[wgpu]\e[0m "
  #endif
  #define dlog(format, ...) ({ \
    fprintf(stderr, DLOG_PREFIX format " \e[2m(%s %d)\e[0m\n", \
      ##__VA_ARGS__, __FUNCTION__, __LINE__); \
    fflush(stderr); \
  })
  #define errlog(format, ...) \
    (({ fprintf(stderr, "E " format " (%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__); \
        fflush(stderr); }))
#else
  #define dlog(...) do{}while(0)
  #define errlog(format, ...) \
(({ fprintf(stderr, "E " format "\n", ##__VA_ARGS__); fflush(stderr); }))
#endif


typedef struct GLFWwindow GLFWwindow;

struct p_wgpu_dev {
  // fd is "our end" of the open("/sys/wgpu") syscall.
  // It is a read-write, non-seekable stream (UNIX socket)
  fd_t                 fd;
  dawn_native::Adapter adapter;
  wgpu::Device         device;
};

struct p_gui_surf {
  GLFWwindow*      window;
  wgpu::Surface    surface;
  wgpu::Device     device;
  p_ringbuf_t      rbuf; // messages for the user to read()
  // TODO: wbuf
  p_gui_surfinfo_t info; // framebuffer info
};


// kNativeBackendType -- Dawn backend type.
// Default to D3D12, Metal, Vulkan, OpenGL in that order as D3D12 and Metal are the
// preferred on their respective platforms, and Vulkan is preferred to OpenGL
inline static wgpu::BackendType p_wgpu_backend_type() {
  #if defined(DAWN_ENABLE_BACKEND_D3D12)
    return wgpu::BackendType::D3D12;
  #elif defined(DAWN_ENABLE_BACKEND_METAL)
    return wgpu::BackendType::Metal;
  #elif defined(DAWN_ENABLE_BACKEND_VULKAN)
    return wgpu::BackendType::Vulkan;
  #elif defined(DAWN_ENABLE_BACKEND_OPENGL)
    return wgpu::BackendType::OpenGL;
  #else
  #  error
  #endif
}

// err_t p_gui_surf_init(p_gui_surf_t*);
err_t p_gui_surf_os_init(p_gui_surf_t*, p_gui_surf_descr_t*);
void p_gui_surf_os_free(p_gui_surf_t*);
std::unique_ptr<wgpu::ChainedStruct> p_gui_surf_wgpu_descriptor(p_gui_surf_t*);
