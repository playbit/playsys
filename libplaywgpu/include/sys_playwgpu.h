// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

// for use by playsys implementations

#pragma once
#include <playsys.h>
#include <playwgpu.h>

typedef struct pwgpu_dev pwgpu_dev_t;
typedef struct pwgpu_surface pwgpu_surface_t;
typedef struct pwgpu_ctl pwgpu_ctl_t;
typedef struct pwgpu_api pwgpu_api_t;

typedef enum pwgpu_dev_flag {
  pwgpu_dev_fl_ronly    = 1, // read-only
  pwgpu_dev_fl_powhigh  = 1 << 1, // request high-perf adapter
  pwgpu_dev_fl_powlow   = 1 << 2, // request low-energy adapter
  pwgpu_dev_fl_software = 1 << 3, // force software driver to be used
} pwgpu_dev_flag_t;

typedef void* (*pwgpu_memalloc_t)(usize count, usize nbyte);
typedef void  (*pwgpu_memfree_t)(void* ptr);

// adapter_id<0 means "auto"
PSYS_EXTERN pwgpu_dev_t* pwgpu_dev_open(fd_t, fd_t fd_user, int adapter, pwgpu_dev_flag_t);
PSYS_EXTERN void pwgpu_dev_close(pwgpu_dev_t*);

PSYS_EXTERN pwgpu_surface_t* pwgpu_surface_open(fd_t, fd_t fd_user);
PSYS_EXTERN isize pwgpu_surface_read(pwgpu_surface_t*, void* data, usize size);
PSYS_EXTERN void pwgpu_surface_close(pwgpu_surface_t*);

PSYS_EXTERN isize pwgpu_ctl_open(pwgpu_ctl_t** result, fd_t);
PSYS_EXTERN isize pwgpu_ctl_read(pwgpu_ctl_t*, void* data, usize size);
PSYS_EXTERN isize pwgpu_ctl_write(pwgpu_ctl_t*, const void* data, usize size);
PSYS_EXTERN isize pwgpu_ctl_close(pwgpu_ctl_t*);


// dawn_wire impl (WIP)

#define PWGPU_API_STRUCT_SIZE 128

PSYS_EXTERN pwgpu_api_t* pwgpu_api_create(fd_t ctl_fd, void* mem);
PSYS_EXTERN void pwgpu_api_sync(pwgpu_api_t*);
PSYS_EXTERN void pwgpu_api_dispose(pwgpu_api_t*);

PSYS_EXTERN WGPUDevice pwgpu_api_alloc_device(pwgpu_api_t*);
PSYS_EXTERN void pwgpu_api_release_device(pwgpu_api_t*, WGPUDevice);

PSYS_EXTERN WGPUTexture pwgpu_api_alloc_texture(pwgpu_api_t*, WGPUDevice);
PSYS_EXTERN void pwgpu_api_release_texture(pwgpu_api_t*, WGPUTexture);

PSYS_EXTERN WGPUSwapChain pwgpu_api_alloc_swapchain(pwgpu_api_t*, WGPUDevice);
PSYS_EXTERN void pwgpu_api_release_swapchain(pwgpu_api_t*, WGPUSwapChain);
