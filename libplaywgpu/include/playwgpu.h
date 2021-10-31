// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

// TODO: consider moving this into playsys

#pragma once
#include <playsys.h>
#include <webgpu.h>

// PWGPU_CTX_SIZE is size in bytes needed for pwgpu_ctx_create
#ifdef __wasm__
  #define PWGPU_CTX_SIZE 8
#else
  #define PWGPU_CTX_SIZE 80
#endif

typedef struct pwgpu_ctx pwgpu_ctx_t;

PSYS_EXTERN pwgpu_ctx_t* pwgpu_ctx_create(void* mem);
PSYS_EXTERN void pwgpu_ctx_dispose(pwgpu_ctx_t*);

PSYS_EXTERN WGPUDevice pwgpu_ctx_set_device(pwgpu_ctx_t*, fd_t gpudev_fd);
PSYS_EXTERN WGPUSurface pwgpu_ctx_set_surface(pwgpu_ctx_t*, fd_t surf_fd);
