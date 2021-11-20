// SPDX-License-Identifier: Apache-2.0
// ioring impl on Linux io_uring
// This file is conditionally included by ioring.c

fd_t _psys_ioring_setup(psysop_t _, u32 entries, p_ioring_params_t* params) {
  // TODO
  return p_err_not_supported;
}

isize _psys_ioring_enter(psysop_t _, fd_t ring, u32 to_submit, u32 min_complete, u32 flags) {
  // TODO
  return p_err_not_supported;
}

isize _psys_ioring_register(psysop_t _, fd_t ring, u32 opcode, const void* arg, u32 nr_args) {
  // TODO
  return p_err_not_supported;
}
