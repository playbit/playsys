// SPDX-License-Identifier: Apache-2.0
#include "base.h"

#define SPECIAL_FS_PREFIX "/sys"


// implementations
#if defined(__linux__)
  #include "syscall_linux.c"
#elif defined(__wasm__)
  #include "syscall_wasm.c"
#else
  #include "syscall_posix.c"
#endif
// rest of this file is common to all implementations



static isize _psys_test(psysop_t op, isize checkop) {
  if (op > p_sysop_write)
    return p_err_not_supported;
  return 0;
}


err_t _psys_close(psysop_t op, fd_t fd) {
  vfile_t* f = vfile_lookup(fd);
  if (f)
    return vfile_close(f);
  return _psys_close_host(0, fd);
}


static err_t wgpu_dev_release(vfile_t* f) {
  return p_wgpu_dev_close(f->data);
}


static fd_t _psys_gpudev(psysop_t op, gpudevflag_t flags) {
  static const vfile_ops_t fops = {
    .release = wgpu_dev_release,
  };
  vfile_t* f;
  fd_t wfd = vfile_open(&f, "[gpudev]", &fops, VFILE_T_GPUDEV | VFILE_PIPE_R);
  if (wfd < 0)
    return wfd;

  int adapter_id = -1;
  err_t e = p_wgpu_dev_open((p_wgpu_dev_t**)&f->data, wfd, adapter_id, flags);
  if (e < 0) {
    vfile_close(f);
    return e;
  }

  return f->fd;
}


static err_t gui_surf_release(vfile_t* f) {
  return p_gui_surf_close(f->data);
}

static isize gui_surf_read(vfile_t* f, char* data, usize size) {
  return p_gui_surf_read(f->data, data, size);
}

static isize gui_surf_write(vfile_t* f, const char* data, usize size) {
  return p_gui_surf_write(f->data, data, size);
}

static fd_t _psys_gui_mksurf(psysop_t op, u32 width, u32 height, fd_t device, u32 flags) {
  static const vfile_ops_t fops = {
    .release = gui_surf_release,
    .read = gui_surf_read,
    .write = gui_surf_write,
  };

  vfile_t* f;
  fd_t fd = vfile_open(&f, "[guisurf]", &fops, VFILE_T_GUI_SURF);
  if (fd < 0)
    return fd;

  p_gui_surf_descr_t d = {
    .width = width,
    .height = height,
    .flags = flags,
    .device = device,
  };
  err_t err = p_gui_surf_open((p_gui_surf_t**)&f->data, &d);
  if (err < 0) {
    vfile_close(f);
    return err;
  }

  return fd;
}


static err_t _psys_NOT_IMPLEMENTED(psysop_t op) {
  return p_err_not_supported;
}


typedef isize (*syscall_fun)(psysop_t,isize,isize,isize,isize,isize);
#define FORWARD(f) MUSTTAIL return ((syscall_fun)(f))(op,arg1,arg2,arg3,arg4,arg5)

isize p_syscall(
  psysop_t op, isize arg1, isize arg2, isize arg3, isize arg4, isize arg5)
{
  //dlog("sys_syscall %u, %ld, %ld, %ld, %ld, %ld", op,arg1,arg2,arg3,arg4,arg5);
  switch ((enum p_sysop)op) {
    case p_sysop_test:   FORWARD(_psys_test);
    case p_sysop_exit:   FORWARD(_psys_exit);
    case p_sysop_openat: FORWARD(_psys_openat);
    case p_sysop_close:  FORWARD(_psys_close);
    case p_sysop_read:   FORWARD(_psys_read);
    case p_sysop_write:  FORWARD(_psys_write);
    case p_sysop_sleep:  FORWARD(_psys_sleep);
    case p_sysop_mmap:   FORWARD(_psys_mmap);
    case p_sysop_pipe:   FORWARD(_psys_pipe);

    case p_sysop_ioring_setup:    FORWARD(_psys_ioring_setup);
    case p_sysop_ioring_enter:    FORWARD(_psys_ioring_enter);
    case p_sysop_ioring_register: FORWARD(_psys_ioring_register);

    case p_sysop_seek:     FORWARD(_psys_NOT_IMPLEMENTED);
    case p_sysop_statat:   FORWARD(_psys_NOT_IMPLEMENTED);
    case p_sysop_removeat: FORWARD(_psys_NOT_IMPLEMENTED);
    case p_sysop_renameat: FORWARD(_psys_NOT_IMPLEMENTED);

    case p_sysop_gpudev:     FORWARD(_psys_gpudev);
    case p_sysop_gui_mksurf: FORWARD(_psys_gui_mksurf);
  }
  return p_err_sys_op;
}
