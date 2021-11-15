// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

// Graphical User Interface API

#pragma once
#include <playsys.h>
#include <webgpu.h>

typedef u32 p_gui_msg_t;
enum _p_gui_msg_t {
  P_GUI_MSG_SURFINFO = 1, // surface size information. payload: p_gui_surfinfo_t
};

// message header
typedef struct _p_gui_msghdr {
  p_gui_msg_t type;
  u32         size; // size in bytes of immediate payload
} p_gui_msghdr_t;

typedef struct _p_gui_surfinfo {
  u32   width, height; // framebuffer dimensions in pixels
  float dpscale;       // dp scale (1 dp = dpscale px)
} p_gui_surfinfo_t;

PSYS_EXTERN WGPUDevice p_gui_wgpu_device(fd_t gui_surf);
PSYS_EXTERN WGPUSurface p_gui_wgpu_surface(fd_t gui_surf);
