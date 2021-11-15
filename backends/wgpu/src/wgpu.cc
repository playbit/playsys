// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

#include "playwgpu_internal.h"

#include <dawn/dawn_proc.h>
#include <dawn_native/DawnNative.h>

#include <vector>
#include <map>


struct p_wgpu_dir_iterator {
};

struct p_wgpu_adapter_dir_iterator : p_wgpu_dir_iterator {
  std::vector<dawn_native::Adapter> adapters;
  size_t                            index = 0;

  p_wgpu_adapter_dir_iterator(std::vector<dawn_native::Adapter>&& v) : adapters(v) {}
};

typedef enum {
  WGPU_RES_DEVICE,
  WGPU_RES_SURFACE,
  WGPU_RES_TEXTURE,
  WGPU_RES_SWAPCHAIN,
} p_wgpu_res_type;

struct p_wgpu_res_t {
  p_wgpu_res_type type;
  union {
    p_wgpu_dev_t* dev;
    p_gui_surf_t* surf;
    WGPUTexture   texture;
    WGPUSwapChain swapchain;
  };
  p_wgpu_res_t(p_wgpu_dev_t* v) : type(WGPU_RES_DEVICE), dev(v) {}
  p_wgpu_res_t(p_gui_surf_t* v) : type(WGPU_RES_SURFACE), surf(v) {}
  p_wgpu_res_t(WGPUTexture v) : type(WGPU_RES_TEXTURE), texture(v) {}
  p_wgpu_res_t(WGPUSwapChain v) : type(WGPU_RES_SWAPCHAIN), swapchain(v) {}
};


static DawnProcTable              gNativeProcs;
static dawn_native::Instance      gDawnNative;
static std::map<fd_t,p_wgpu_res_t> gOpenResources;


// helper functions defined at end of file
static void logAvailableAdapters();
static const char* backend_type_name(wgpu::BackendType t);
static const char* adapter_type_name(wgpu::AdapterType t);


static p_wgpu_dev_t* lookup_dev(fd_t user_fd) {
  auto it = gOpenResources.find(user_fd);
  if (it == gOpenResources.end() || it->second.type != WGPU_RES_DEVICE)
    return nullptr;
  return it->second.dev;
}


static p_gui_surf_t* lookup_surf(fd_t user_fd) {
  auto it = gOpenResources.find(user_fd);
  if (it == gOpenResources.end() || it->second.type != WGPU_RES_SURFACE)
    return nullptr;
  return it->second.surf;
}


static void p_wgpu_init() {
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


static bool select_adapter(p_wgpu_dev_t* c, p_wgpu_dev_flag_t fl) {
  // search available adapters for a good match, in the following priority order:
  const std::vector<wgpu::AdapterType> typePriority = (
    (fl & p_wgpu_dev_fl_software) ? std::vector<wgpu::AdapterType>{
      wgpu::AdapterType::CPU,
    }
    : (fl & p_wgpu_dev_fl_powlow) ? std::vector<wgpu::AdapterType>{
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
          (reqType == wgpu::AdapterType::CPU || ap.backendType == p_wgpu_backend_type()) )
      {
        dlog("selected adapter %s (device=0x%x vendor=0x%x type=%s/%s)",
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


static err_t dev_select_device(p_wgpu_dev_t* dev, int adapter_id, p_wgpu_dev_flag_t fl) {
  p_wgpu_dev_flag_t devfl = p_wgpu_dev_fl_none;
  if (!select_adapter(dev, devfl))
    return p_err_not_supported;
  WGPUDevice device = dev->adapter.CreateDevice();
  if (!device) {
    // TODO: reture suitable descriptive error code
    errlog("failed to create wgpu device");
    return p_err_canceled;
  }
  dev->device = wgpu::Device::Acquire(device);
  // dev->device.SetDeviceLostCallback(onWGPUDeviceLost, this);
  // dev->device.SetLoggingCallback(onWGPULog, this);
  // dev->device.SetUncapturedErrorCallback(onWGPUDeviceError, this);
  return 0;
}


err_t p_wgpu_opendev(
  p_wgpu_dev_t** devp, fd_t fd, fd_t fd_user, int adapter_id, p_wgpu_dev_flag_t fl)
{
  p_wgpu_init();
  p_wgpu_dev_t* dev = new p_wgpu_dev_t();
  dev->fd = fd;
  err_t e = dev_select_device(dev, adapter_id, fl);
  if (e < 0) {
    delete dev;
    return e;
  }
  dlog("p_wgpu_dev_open %p fd_user=%ld", dev, fd_user);
  gOpenResources.emplace(fd_user, dev);
  *devp = dev;
  return 0;
}


err_t p_wgpu_dev_close(p_wgpu_dev_t* dev) {
  if (dev->device)
    dev->device.Release();
  gOpenResources.erase(dev->fd);
  delete dev;
  return 0;
}


err_t p_gui_surf_close(p_gui_surf_t* surf) {
  if (surf->device)
    surf->device.Release();
  if (surf->window) {
    p_gui_surf_os_free(surf);
    surf->window = NULL;
  }
  free(surf->rbuf.p);
  gOpenResources.erase(surf->fd);
  delete surf;
  return 0;
}


err_t p_gui_surf_open(
  p_gui_surf_t** surfp, fd_t fd, fd_t fd_user, p_gui_surf_descr_t* descr)
{
  p_wgpu_init();

  // create surface struct
  p_gui_surf_t* surf = new p_gui_surf_t();
  memset(surf, 0, sizeof(p_gui_surf_t));
  surf->fd = fd;
  u32 rbufcap = 4096;
  p_ringbuf_init(&surf->rbuf, malloc(rbufcap), rbufcap);
  if (surf->rbuf.p == NULL) { // malloc failed
    delete surf;
    return p_err_nomem;
  }

  // create host OS window or surface (e.g. glfwCreateWindow)
  isize r = p_gui_surf_os_init(surf, descr);
  if (r != 0) {
    delete surf;
    return r;
  }

  // create wgpu surface
  std::unique_ptr<wgpu::ChainedStruct> sd1 = p_gui_surf_wgpu_descriptor(surf);
  wgpu::SurfaceDescriptor descriptor;
  descriptor.nextInChain = sd1.get();
  surf->surface = wgpu::Instance(gDawnNative.Get()).CreateSurface(&descriptor);
  if (!surf->surface) {
    errlog("dawn: failed to create surface for dawn-native instance");
    delete surf;
    return p_err_not_supported;
  }

  // set device
  if (descr->device > -1) {
    p_wgpu_dev_t* dev = lookup_dev(descr->device);
    if (!dev) {
      errlog("wgpu_mksurf: invalid device file descriptor");
      delete surf;
      return p_err_badfd;
    }
    surf->device = dev->device;
  } else {
    // auto-select device
    p_wgpu_dev_t dev;
    int adapter_id = -1;
    err_t e = dev_select_device(&dev, adapter_id, p_wgpu_dev_fl_none);
    if (e < 0) {
      delete surf;
      return e;
    }
    surf->device = std::move(dev.device);
  }

  gOpenResources.emplace(fd_user, surf);
  *surfp = surf;
  return 0;
}


WGPUDevice p_gui_wgpu_device(fd_t gui_surf_fd) {
  p_gui_surf_t* surf = lookup_surf(gui_surf_fd);
  return surf ? surf->device.Get() : nullptr;
}

WGPUSurface p_gui_wgpu_surface(fd_t gui_surf_fd) {
  p_gui_surf_t* surf = lookup_surf(gui_surf_fd);
  return surf ? surf->surface.Get() : nullptr;
}

err_t p_gui_surfinfo(fd_t gui_surf_fd, p_gui_surfinfo_t* si) {
  p_gui_surf_t* surf = lookup_surf(gui_surf_fd);
  if (!surf)
    return p_err_badfd;
  *si = surf->info;
  return 0;
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
