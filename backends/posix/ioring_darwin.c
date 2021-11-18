// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

#define SYS_DEBUG // define to enable debug logging
#include "sys_impl.h"


typedef struct p_ioring {
  int todo;
} p_ioring_t;


// ioring instances in global memory
static p_ioring_t g_ioringv[8] = {0};
static u32        g_ioringc = 0;


static err_t _ioring_on_close(vfile_t* f) {
  dlog("TODO: splice g_ioringv and subtract 1 from g_ioringc (fd %d)", f->fd);
  return 0;
}


fd_t _psys_ioring_setup(psysop_t _, u32 entries, p_ioring_params_t* params) {
  // allocate a virtual file
  vfile_t* f;
  fd_t fd = vfile_open(&f, VFILE_T_IORING, 0);
  if (fd < 0)
    return fd; // error
  f->on_close = _ioring_on_close;

  // allocate a p_ioring_t struct
  if (g_ioringc == countof(g_ioringv)) {
    vfile_close(f);
    return p_err_nomem;
  }
  p_ioring_t* ring = &g_ioringv[g_ioringc++];
  f->data = ring;

  return fd;
}


isize _psys_ioring_enter(psysop_t _, fd_t ring, u32 to_submit, u32 min_complete, u32 flags) {
  return p_err_not_supported;
}


isize _psys_ioring_register(psysop_t _, fd_t ring, u32 opcode, const void* arg, u32 nr_args) {
  return p_err_not_supported;
}
