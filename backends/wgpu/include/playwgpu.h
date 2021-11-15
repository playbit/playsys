// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

// for use by playsys implementations

#pragma once
#include <playsys.h>
#include <playsys-gui.h>
#include <webgpu.h>

typedef struct p_wgpu_dev p_wgpu_dev_t; // WGPU device
typedef struct p_gui_surf p_gui_surf_t; // GUI surface

typedef enum p_wgpu_dev_flag {
  p_wgpu_dev_fl_none     = 0,
  p_wgpu_dev_fl_ronly    = 1,      // read-only (unused)
  p_wgpu_dev_fl_powhigh  = 1 << 1, // request high-perf adapter
  p_wgpu_dev_fl_powlow   = 1 << 2, // request low-energy adapter
  p_wgpu_dev_fl_software = 1 << 3, // force software driver to be used
} p_wgpu_dev_flag_t;

typedef struct _p_gui_surf_descr {
  u32         width, height; // size in dp units. 0 = host's choice
  fd_t        device; // <0 = host's choice
  usize       flags;
  const char* title; // optional title
} p_gui_surf_descr_t;

// adapter_id<0 means "auto"
PSYS_EXTERN err_t p_wgpu_opendev(
  p_wgpu_dev_t**, fd_t, fd_t fd_user, int adapter, p_wgpu_dev_flag_t);
PSYS_EXTERN err_t p_wgpu_dev_close(p_wgpu_dev_t*);

PSYS_EXTERN err_t p_gui_surf_open(p_gui_surf_t**, fd_t, fd_t fd_user, p_gui_surf_descr_t*);
PSYS_EXTERN isize p_gui_surf_read(p_gui_surf_t*, void* data, usize size);
PSYS_EXTERN err_t p_gui_surf_close(p_gui_surf_t*);
