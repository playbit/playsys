// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

#include "playwgpu_internal.h"

#include <dawn/dawn_proc.h>
#include <dawn_wire/WireServer.h>
#include <dawn_wire/WireClient.h>
#include <dawn_native/DawnNative.h>

#include <vector>
#include <map>


struct pwgpu_dev {
  // fd is "our end" of the open("/sys/wgpu") syscall.
  // It is a read-write, non-seekable stream (UNIX socket)
  sys_fd fd;

  dawn_native::Adapter adapter;
  wgpu::Device         device;

  pwgpu_surface_t* surface;
  wgpu::Surface   pwgpu_surface;

  wgpu::SwapChain swapchain;
};


struct pwgpu_dir_iterator {
};

struct pwgpu_adapter_dir_iterator : pwgpu_dir_iterator {
  std::vector<dawn_native::Adapter> adapters;
  size_t                            index = 0;

  pwgpu_adapter_dir_iterator(std::vector<dawn_native::Adapter>&& v) : adapters(v) {}
};

typedef enum {
  WGPU_RES_DEVICE,
  WGPU_RES_SURFACE,
  WGPU_RES_TEXTURE,
  WGPU_RES_SWAPCHAIN,
} pwgpu_res_type;

struct pwgpu_res_t {
  pwgpu_res_type type;
  union {
    pwgpu_dev_t*     dev;
    pwgpu_surface_t* surf;
    WGPUTexture     texture;
    WGPUSwapChain   swapchain;
  };
  pwgpu_res_t(pwgpu_dev_t* v) : type(WGPU_RES_DEVICE), dev(v) {}
  pwgpu_res_t(pwgpu_surface_t* v) : type(WGPU_RES_SURFACE), surf(v) {}
  pwgpu_res_t(WGPUTexture v) : type(WGPU_RES_TEXTURE), texture(v) {}
  pwgpu_res_t(WGPUSwapChain v) : type(WGPU_RES_SWAPCHAIN), swapchain(v) {}
};


static DawnProcTable gNativeProcs;
static dawn_native::Instance gDawnNative;

static std::map<sys_fd,pwgpu_res_t> gOpenResources;


// helper functions defined at end of file
static void logAvailableAdapters();
static const char* backend_type_name(wgpu::BackendType t);
static const char* adapter_type_name(wgpu::AdapterType t);


static void pwgpu_init() {
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;

  // Set up the native procs for the global proctable
  gNativeProcs = dawn_native::GetProcs();
  dawnProcSetProcs(&gNativeProcs);

  gDawnNative.DiscoverDefaultAdapters();

  #ifdef DEBUG
  gDawnNative.EnableBackendValidation(true);
  gDawnNative.SetBackendValidationLevel(dawn_native::BackendValidationLevel::Full);
  #elif defined(WGPU_DAWN_DISABLE_VALIDATION)
  gDawnNative.EnableBackendValidation(false);
  gDawnNative.SetBackendValidationLevel(dawn_native::BackendValidationLevel::Disabled);
  #endif

  logAvailableAdapters();
}


static bool select_adapter(pwgpu_dev_t* c, pwgpu_dev_flag_t fl) {
  // search available adapters for a good match, in the following priority order:
  const std::vector<wgpu::AdapterType> typePriority = (
    (fl & pwgpu_dev_fl_software) ? std::vector<wgpu::AdapterType>{
      wgpu::AdapterType::CPU,
    }
    : (fl & pwgpu_dev_fl_powlow) ? std::vector<wgpu::AdapterType>{
      wgpu::AdapterType::IntegratedGPU,
      wgpu::AdapterType::DiscreteGPU,
      wgpu::AdapterType::CPU,
    }
    : std::vector<wgpu::AdapterType>{
      wgpu::AdapterType::DiscreteGPU,
      wgpu::AdapterType::IntegratedGPU,
      wgpu::AdapterType::CPU,
    }
  );

  std::vector<dawn_native::Adapter> adapters = gDawnNative.GetAdapters();

  for (auto reqType : typePriority) {
    for (const dawn_native::Adapter& adapter : adapters) {
      wgpu::AdapterProperties ap;
      adapter.GetProperties(&ap);
      if (ap.adapterType == reqType &&
          (reqType == wgpu::AdapterType::CPU || ap.backendType == _pwgpu_backend_type()) )
      {
        fprintf(stderr, "selected adapter %s (device=0x%x vendor=0x%x type=%s/%s)\n",
          ap.name, ap.deviceID, ap.vendorID,
          adapter_type_name(ap.adapterType), backend_type_name(ap.backendType));
        c->adapter = adapter;
        return true;
      }
    }
  }

  c->adapter = nullptr;
  return false;
}


pwgpu_dev_t* pwgpu_dev_open(sys_fd fd, sys_fd fd_user, int adapter_id, pwgpu_dev_flag_t fl) {
  pwgpu_init();

  pwgpu_dev_t* c = new pwgpu_dev_t();
  c->fd = fd;

  // adapter
  if (!select_adapter(c, fl)) {
    delete c;
    return NULL;
  }

  // device
  WGPUDevice device = c->adapter.CreateDevice();
  if (!device) {
    errlog("failed to create wgpu device");
    delete c;
    return NULL;
  }
  c->device = wgpu::Device::Acquire(device);
  // _device.SetDeviceLostCallback(onWGPUDeviceLost, this);
  // _device.SetLoggingCallback(onWGPULog, this);
  // _device.SetUncapturedErrorCallback(onWGPUDeviceError, this);

  dlog("pwgpu_dev_open %p fd_user=%ld", c, fd_user);
  gOpenResources.emplace(fd_user, c);

  return c;
}


void pwgpu_dev_close(pwgpu_dev_t* c) {
  if (c->device)
    c->device.Release();
  gOpenResources.erase(c->fd);
  delete c;
}


pwgpu_surface_t* pwgpu_surface_open(sys_fd fd, sys_fd fd_user) {
  pwgpu_init();
  pwgpu_surface_t* surf = new pwgpu_surface_t();
  memset(surf, 0, sizeof(pwgpu_surface_t));
  surf->fd = fd;
  dlog("pwgpu_surface_open %p fd_user=%ld", surf, fd_user);
  gOpenResources.emplace(fd_user, surf);
  return surf;
}


void pwgpu_surface_close(pwgpu_surface_t* surf) {
  if (surf->window) {
    _pwgpu_surface_free_oswin(surf);
    surf->window = NULL;
  }
  gOpenResources.erase(surf->fd);
  delete surf;
}


sys_ret _pwgpu_surface_init(pwgpu_surface_t* surf) {
  u32 width = 640;
  u32 height = 480;
  const char* title = "";

  // in case of failure, leave surf in a closed state
  surf->state = WGPU_SURF_STATE_CLOSED;

  // parse config, e.g. "width 400\nheight 300\ntitle Hello world\n"
  u8 buf[128] = {0};
  isize len = read(surf->fd, buf, sizeof(buf));
  if (len < 0)
    return -sys_err_not_supported;
  // dlog("read: '%.*s'", (int)len, buf);
  const u8* bufend = buf + len;
  u8* bufnext = buf;
  const char* key;
  const char* value;
  while ((bufnext = _pwgpu_parse_next_keyvalue(bufnext, bufend, &key, &value))) {
    // dlog(">> '%s' => '%s'", key, value);
    if (strcmp("width", key) == 0) {
      long n = strtol(value, NULL, 10);
      if (n < 0 || n > 0x7FFFFFFF)
        return -sys_err_not_supported;
      width = (u32)n;
    } else if (strcmp("height", key) == 0) {
      long n = strtol(value, NULL, 10);
      if (n < 0 || n > 0x7FFFFFFF)
        return -sys_err_not_supported;
      height = (u32)n;
    } else if (strcmp("title", key) == 0) {
      title = value;
    }
  }

  // create host OS window or surface (e.g. glfwCreateWindow)
  sys_ret r = _pwgpu_surface_init_oswin(surf, width, height, title);
  if (r != 0)
    return r;

  // create wgpu surface
  std::unique_ptr<wgpu::ChainedStruct> sd1 = _pwgpu_surface_descriptor(surf);
  wgpu::SurfaceDescriptor descriptor;
  descriptor.nextInChain = sd1.get();
  WGPUInstance instance = gDawnNative.Get();
  dlog("instance %p", instance);
  surf->surface = wgpu::Instance(instance).CreateSurface(&descriptor);
  dlog("D");
  if (!surf->surface) {
    errlog("dawn: failed to create surface for dawn-native instance");
    return -sys_err_not_supported;
  }

  // // create wgpu swapchain
  // wgpu::SwapChainDescriptor desc = {
  //   .format = wgpu::TextureFormat::BGRA8Unorm,
  //   .usage  = wgpu::TextureUsage::RenderAttachment,
  //   .width  = surf->fbwidth,
  //   .height = surf->fbheight,
  //   .presentMode = wgpu::PresentMode::Mailbox,
  // };
  // surf->swapchain = _device.CreateSwapChain(_surface, &desc);

  surf->state = WGPU_SURF_STATE_ACTIVE;
  return 0;
}

// ————————————————————————————————————————————————————————————————————————————————————

sys_ret pwgpu_ctl_open(pwgpu_ctl_t** result, sys_fd fd) {
  pwgpu_init();
  pwgpu_ctl_t* ctl = new pwgpu_ctl(fd, &gNativeProcs);
  ctl->inbuf.reserve(4096 * 32);
  ctl->outbuf.reserve(4096);
  *result = ctl;
  return 0;
}


sys_ret pwgpu_ctl_close(pwgpu_ctl_t* ctl) {
  delete ctl;
  return 0;
}


sys_ret pwgpu_ctl_read(pwgpu_ctl_t* ctl, void* data, usize size) {
  // handle incoming commands from client, previously queued with write()
  size_t inlen = ctl->inbuf.size();
  if (inlen > 0) {
    if (ctl->wire_server.HandleCommands((const char*)ctl->inbuf.data(), inlen) == nullptr) {
      dlog("wire_server.HandleCommands FAILED");
      // close();
      return -sys_err_canceled;
    }
    ctl->inbuf.resize(0);
    // at this point
    // TODO: send back changes
    // if (!_proto.Flush()) {
    //   dlog("_proto.Flush() FAILED");
    //   return -sys_err_canceled;
    // }
  }

  return 0;
}


sys_ret pwgpu_ctl_write(pwgpu_ctl_t*, const void* data, usize size) {
  // TODO
  return size;
}


// dawn_wire::CommandSerializer impl
size_t pwgpu_ctl::GetMaximumAllocationSize() const {
  return outbuf.capacity();
}

void* pwgpu_ctl::GetCmdSpace(size_t size) {
  size_t len = outbuf.size();
  size_t avail = outbuf.capacity() - len;
  if (avail < size) {
    dlog("pwgpu_ctl::GetCmdSpace FAILED (not enough space)");
    return nullptr; // not enough space
  }
  u8* ptr = &outbuf[len];
  outbuf.resize(len + size);
  return ptr;
}

bool pwgpu_ctl::Flush() {
  dlog("pwgpu_ctl::Flush %zu", outbuf.size());
  outbuf.resize(0);
  return true;
}


// ————————————————————————————————————————————————————————————————————————————————————
// pwgpu_ctx

struct pwgpu_ctx {
  std::vector<WGPUDevice>    devices;
  std::vector<WGPUTexture>   textures;
  std::vector<WGPUSwapChain> swapchains;

  pwgpu_ctx() {}
  ~pwgpu_ctx() {}
};

pwgpu_ctx_t* pwgpu_ctx_create(void* mem) {
  dlog("sizeof(pwgpu_ctx) %zu", sizeof(pwgpu_ctx));
  static_assert(PWGPU_CTX_SIZE >= sizeof(pwgpu_ctx), "update PWGPU_CTX_SIZE");
  // void* mem = allocf(1, sizeof(pwgpu_ctx));
  memset(mem, 0, sizeof(pwgpu_ctx));
  pwgpu_ctx* ctx = new(mem) pwgpu_ctx();
  return ctx;
}

void pwgpu_ctx_dispose(pwgpu_ctx_t* ctx) {
  ctx->~pwgpu_ctx();
  // memset(ctx, 0, sizeof(pwgpu_ctx));
}


WGPUDevice pwgpu_ctx_set_device(pwgpu_ctx_t* ctx, sys_fd user_fd) {
  auto it = gOpenResources.find(user_fd);
  if (it == gOpenResources.end() || it->second.type != WGPU_RES_DEVICE)
    return nullptr;
  return it->second.dev->device.Get();
}

WGPUSurface pwgpu_ctx_set_surface(pwgpu_ctx_t* ctx, sys_fd user_fd) {
  auto it = gOpenResources.find(user_fd);
  if (it == gOpenResources.end() || it->second.type != WGPU_RES_SURFACE)
    return nullptr;

  pwgpu_surface_t* surf = it->second.surf;
  if (surf->state == WGPU_SURF_STATE_INIT) {
    sys_ret r = _pwgpu_surface_init(surf);
    if (r != 0)
      return nullptr;
  }

  return surf->surface.Get();
}


// ————————————————————————————————————————————————————————————————————————————————————
// pwgpu_api

struct pwgpu_api : public dawn_wire::CommandSerializer {
  dawn_wire::WireClient wire_client;
  std::vector<u8> outbuf; // client -> server
  std::vector<dawn_wire::ReservedDevice> devices;
  std::vector<dawn_wire::ReservedTexture> textures;
  std::vector<dawn_wire::ReservedSwapChain> swapchains;

  pwgpu_api()
    : wire_client({ .serializer = this })
  {
    outbuf.reserve(4096 * 32);
  }
  ~pwgpu_api() {}

  // dawn_wire::CommandSerializer
  size_t GetMaximumAllocationSize() const override;
  void* GetCmdSpace(size_t size) override;
  bool Flush() override;
};

pwgpu_api_t* pwgpu_api_create(sys_fd fd, void* mem) {
  dlog("sizeof(pwgpu_api) %zu", sizeof(pwgpu_api));
  static_assert(PWGPU_API_STRUCT_SIZE >= sizeof(pwgpu_api), "update PWGPU_API_STRUCT_SIZE");
  // void* mem = allocf(1, sizeof(pwgpu_api));
  memset(mem, 0, sizeof(pwgpu_api));
  pwgpu_api* api = new(mem) pwgpu_api();
  return api;
}

void pwgpu_api_dispose(pwgpu_api_t* api) {
  api->~pwgpu_api();
  // memset(api, 0, sizeof(pwgpu_api));
  //freef(api);
}

WGPUDevice pwgpu_api_alloc_device(pwgpu_api_t* api) {
  auto dev = api->devices.emplace_back(api->wire_client.ReserveDevice());
  return dev.device;
}

void pwgpu_api_release_device(pwgpu_api_t* api, WGPUDevice device) {
  for (auto it = api->devices.begin(); it != api->devices.end(); ++it) {
    if (it->device == device) {
      api->wire_client.ReclaimDeviceReservation(*it);
      it = api->devices.erase(it);
      break;
    }
  }
}

WGPUTexture pwgpu_api_alloc_texture(pwgpu_api_t* api, WGPUDevice device) {
  auto dev = api->textures.emplace_back(api->wire_client.ReserveTexture(device));
  return dev.texture;
}

void pwgpu_api_release_texture(pwgpu_api_t* api, WGPUTexture texture) {
  for (auto it = api->textures.begin(); it != api->textures.end(); ++it) {
    if (it->texture == texture) {
      api->wire_client.ReclaimTextureReservation(*it);
      it = api->textures.erase(it);
      break;
    }
  }
}

WGPUSwapChain pwgpu_api_alloc_swapchain(pwgpu_api_t* api, WGPUDevice device) {
  auto dev = api->swapchains.emplace_back(api->wire_client.ReserveSwapChain(device));
  return dev.swapchain;
}

void pwgpu_api_release_swapchain(pwgpu_api_t* api, WGPUSwapChain swapchain) {
  for (auto it = api->swapchains.begin(); it != api->swapchains.end(); ++it) {
    if (it->swapchain == swapchain) {
      api->wire_client.ReclaimSwapChainReservation(*it);
      it = api->swapchains.erase(it);
      break;
    }
  }
}

void pwgpu_api_sync(pwgpu_api_t* api) {
  //
}

// dawn_wire::CommandSerializer impl
size_t pwgpu_api::GetMaximumAllocationSize() const {
  return outbuf.capacity();
}

void* pwgpu_api::GetCmdSpace(size_t size) {
  dlog("pwgpu_api::GetCmdSpace %zu", size);
  size_t len = outbuf.size();
  size_t avail = outbuf.capacity() - len;
  if (avail < size) {
    dlog("pwgpu_api::GetCmdSpace FAILED (not enough space)");
    return nullptr; // not enough space
  }
  u8* ptr = &outbuf[len];
  outbuf.resize(len + size);
  return ptr;
}

bool pwgpu_api::Flush() {
  dlog("pwgpu_api::Flush %zu", outbuf.size());
  outbuf.resize(0);
  return true;
}



// ——————————————————————————————————————————————————————————————————————————————————


static isize index_byte(const u8* data, isize len, u8 findbyte) {
  for (isize i = 0; i < len; i++) {
    if (data[i] == findbyte)
      return i;
  }
  return -1;
}


u8* _pwgpu_parse_next_keyvalue(
  u8* bufstart, const u8* bufend, const char** key, const char** value)
{
  isize len = (isize)(bufend - bufstart);
  isize end = index_byte(bufstart, len, '\n');
  if (end == -1)
    return NULL;
  bufstart[end] = '\0'; // replace LF with NUL

  // trim leading whitespace of key
  while (*bufstart == ' ') {
    bufstart++;
    end--;
  }
  *key = (const char*)bufstart;

  // find key-value separator SP
  isize sp = index_byte(bufstart, len, ' ');
  if (sp > -1) {
    bufstart[sp] = '\0'; // replace SP with NUL
    const char* valptr = (const char*)&bufstart[sp + 1];
    // trim leading whitespace of value
    while (*valptr == ' ')
      valptr++;
    *value = valptr;
  } else {
    *value = "";
  }

  return &bufstart[end + 1];
}


static const char* backend_type_name(wgpu::BackendType t) {
  switch (t) {
    case wgpu::BackendType::Null:     return "Null";
    case wgpu::BackendType::WebGPU:   return "WebGPU";
    case wgpu::BackendType::D3D11:    return "D3D11";
    case wgpu::BackendType::D3D12:    return "D3D12";
    case wgpu::BackendType::Metal:    return "Metal";
    case wgpu::BackendType::Vulkan:   return "Vulkan";
    case wgpu::BackendType::OpenGL:   return "OpenGL";
    case wgpu::BackendType::OpenGLES: return "OpenGLES";
  }
  return "?";
}

static const char* adapter_type_name(wgpu::AdapterType t) {
  switch (t) {
    case wgpu::AdapterType::DiscreteGPU:   return "DiscreteGPU";
    case wgpu::AdapterType::IntegratedGPU: return "IntegratedGPU";
    case wgpu::AdapterType::CPU:           return "CPU";
    case wgpu::AdapterType::Unknown:       return "Unknown";
  }
  return "?";
}

// logAvailableAdapters prints a list of all adapters and their properties
static void logAvailableAdapters() {
  fprintf(stderr, "Available GPU adapters:\n");
  for (auto&& a : gDawnNative.GetAdapters()) {
    wgpu::AdapterProperties p;
    a.GetProperties(&p);
    fprintf(stderr, "  %s (%s)\n"
      "    deviceID=%u, vendorID=0x%x, BackendType::%s, AdapterType::%s\n",
      p.name, p.driverDescription,
      p.deviceID, p.vendorID,
      backend_type_name(p.backendType), adapter_type_name(p.adapterType));
  }
}


#ifdef __APPLE__
// HERE BE DRAGONS!
// On macos dawn/src/dawn_native/metal/ShaderModuleMTL.mm uses the @available ObjC
// feature which expects a runtime symbol that is AFAIK only provided by Apple's
// version of clang.
extern "C" int __isPlatformVersionAtLeast(
  long unkn, long majv, long minv, long buildv)
{
  // <= 10.15.x
  return majv < 10 || (majv == 10 && minv <= 15);
}
#endif
