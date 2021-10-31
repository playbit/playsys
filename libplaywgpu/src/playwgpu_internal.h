// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

#pragma once
#include <dawn/webgpu_cpp.h>
#include <sys_playwgpu.h> // note: after webgpu_cpp; needs <dawn/webgpu.h>
#include <dawn_wire/WireServer.h>
#include <memory> // unique_ptr
#include <vector>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>

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


#define MAX(a,b) \
  ({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })

#define MIN(a,b) \
  ({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })


typedef enum {
  WGPU_SURF_STATE_INIT,   // waiting to be initialized; was just opened
  WGPU_SURF_STATE_ACTIVE, // active and available to pwgpu_surface_read() from
  WGPU_SURF_STATE_CLOSED, // closed
} pwgpu_surface_state_t;

typedef struct GLFWwindow GLFWwindow;

struct pwgpu_surface {
  fd_t                  fd; // "our end" of the read-write, non-seekable stream
  pwgpu_surface_state_t state;
  GLFWwindow*           window;
  wgpu::Surface         surface;

  u32   fbwidth, fbheight; // dimensions in pixels
  float fbscale;           // 1 dp = fbscale px
};


struct pwgpu_ctl : public dawn_wire::CommandSerializer {
  fd_t fd; // "our end" of the read-write, non-seekable stream

  dawn_wire::WireServer wire_server;

  std::vector<u8> inbuf;  // client -> server
  std::vector<u8> outbuf; // server -> client

  pwgpu_ctl(fd_t fd_, const DawnProcTable* procs)
    : fd(fd_)
    , wire_server({ .procs = procs, .serializer = this })
  {}

  // dawn_wire::CommandSerializer
  size_t GetMaximumAllocationSize() const override;
  void* GetCmdSpace(size_t size) override;
  bool Flush() override;
};


// kNativeBackendType -- Dawn backend type.
// Default to D3D12, Metal, Vulkan, OpenGL in that order as D3D12 and Metal are the
// preferred on their respective platforms, and Vulkan is preferred to OpenGL
inline static wgpu::BackendType _pwgpu_backend_type() {
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

isize _pwgpu_surface_init(pwgpu_surface_t*);
isize _pwgpu_surface_init_oswin(pwgpu_surface_t*, u32 width, u32 height, const char* title);
void _pwgpu_surface_free_oswin(pwgpu_surface_t*);
std::unique_ptr<wgpu::ChainedStruct> _pwgpu_surface_descriptor(pwgpu_surface_t*);

u8* _pwgpu_parse_next_keyvalue(
  u8* bufstart, const u8* bufend, const char** key, const char** value);
