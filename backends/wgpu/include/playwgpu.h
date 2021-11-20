// SPDX-License-Identifier: Apache-2.0

// for use by playsys implementations

#pragma once
#include <playsys.h>
#include <playsys-gui.h>
#include <webgpu.h>

typedef struct p_wgpu_dev p_wgpu_dev_t; // WGPU device
typedef struct p_gui_surf p_gui_surf_t; // GUI surface

typedef struct _p_gui_surf_descr {
  u32         width, height; // size in dp units. 0 = host's choice
  fd_t        device; // <0 = host's choice
  u32         flags;  // currently no flags; pass 0
  const char* title;  // optional title
} p_gui_surf_descr_t;

// adapter_id<0 means "auto"
PSYS_EXTERN err_t p_wgpu_dev_open(p_wgpu_dev_t**, fd_t w, int adapter, gpudevflag_t);
PSYS_EXTERN err_t p_wgpu_dev_close(p_wgpu_dev_t*);

PSYS_EXTERN err_t p_gui_surf_open(p_gui_surf_t**, p_gui_surf_descr_t*);
PSYS_EXTERN isize p_gui_surf_read(p_gui_surf_t*, char* data, usize);
PSYS_EXTERN isize p_gui_surf_write(p_gui_surf_t*, const char* data, usize);
PSYS_EXTERN err_t p_gui_surf_close(p_gui_surf_t*);
