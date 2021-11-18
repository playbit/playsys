// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

#define SYS_DEBUG // define to enable debug logging
#include "sys_impl.h"


fd_t _psys_ioring_setup(psysop_t _, u32 entries, p_ioring_params_t* params) {
  return p_err_not_supported;
}

isize _psys_ioring_enter(psysop_t _, fd_t ring, u32 to_submit, u32 min_complete, u32 flags) {
  return p_err_not_supported;
}

isize _psys_ioring_register(psysop_t _, fd_t ring, u32 opcode, const void* arg, u32 nr_args) {
  return p_err_not_supported;
}
