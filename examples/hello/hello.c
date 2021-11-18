#include <playsys.h>
#include <playsys-gui.h>
#include "hello.h"


void read_file(const char* path, void* buf, usize cap) {
  fd_t fd       = open(path, p_open_ronly, 0); check_status(fd, "open");
  isize readlen = read(fd, buf, cap);            check_status(readlen, "read");
  err_t status  = close(fd);                     check_status(status, "close");
  print("read from file ", path, ": ");
  write(P_FDSTDOUT, buf, readlen);
  if (readlen > 0 && ((u8*)buf)[readlen - 1] != '\n')
    print("\n");
}


err_t read_gui_msg(fd_t gui_surf) {
  // read message header
  p_gui_msghdr_t mh;
  isize n = read(gui_surf, &mh, sizeof(mh));
  if (n < 1)
    return n;

  // read message payload
  switch (mh.type) {
  case P_GUI_MSG_SURFINFO: {
    p_gui_surfinfo_t surfinfo = {0};
    if (read(gui_surf, &surfinfo, sizeof(surfinfo)) != sizeof(surfinfo)) {
      print("p_gui_ivec2_t short read\n");
      return p_err_invalid;
    }
    print("surface size: ", surfinfo.width, "x", surfinfo.height, "px @ ",
          (u32)(surfinfo.dpscale*100), "%\n");
    if (surfinfo.width <= 0) exit(4);
    if (surfinfo.height <= 0) exit(5);
    return 0;
  }
  }
  print("p_gui_ivec2_t unexpected p_gui_msg_t ", mh.type, "\n");
  return p_err_invalid;
}


err_t read_gui_msgs(fd_t gui_surf) {
  while (1) {
    isize n = read_gui_msg(gui_surf);
    if (n < 1)
      return n;
  }
}


PUB int main(int argc, const char** argv) {
  print("start\n");

  u8* buf[256];
  read_file("/sys/uname", buf, sizeof(buf));

  // ioring
  p_ioring_params_t ringp = {0};
  fd_t ring = p_syscall_ioring_setup(/*entries*/1, &ringp);
  check_status(ring, "p_syscall_ioring_setup");
  print("p_syscall_ioring_setup OK\n");


  // map ring buffer memory
  usize sring_size = (usize)ringp.sq_off.array + ringp.sq_entries * sizeof(u32);
  usize cring_size = (usize)ringp.cq_off.cqes + ringp.cq_entries * sizeof(p_ioring_cqe_t);
  usize ring_size = sring_size > cring_size ? sring_size : cring_size; // max
  void* ring_sq_ptr = NULL;
  err_t err = p_syscall_mmap(
    &ring_sq_ptr,
    ring_size,
    p_mmap_prot_read | p_mmap_prot_write | p_mmap_shared | p_mmap_populate,
    ring,
    P_IORING_OFF_SQ_RING);
  check_status(err, "mmap P_IORING_OFF_SQ_RING");

  // WebGPU concepts
  //   device
  //     Virtual GPU device which may map to a hardware GPU adapter.
  //     You send commands to it to compute or render.
  //   surface
  //     Plaform-specific graphics surface. Framebuffer-size agnostic.
  //   swapchain
  //     Graphics framebuffer queue. Bound to a device + surface.
  //     Has a fixed size specific to the actual backing texture of the surface.
  //

  // // select a GPU device
  // fd_t wgpu_dev = open("/sys/wgpu/dev/gpu0", p_open_ronly, 0);
  // check_status(wgpu_dev, "open /sys/wgpu/dev/gpu0");
  // print("opened wgpu device (handle ", wgpu_dev, ")\n");
  // // TODO: consider open-request info with query string, e.g.
  // //       "/gpu?required-feats=texture-compression-bc"
  // // TODO: allow read device to access name, features, limits and isFallbackAdapter.
  // // TODO: allow p_open_rw on device to enable simple compute

  // // select a GPU device (alt to fs, using a syscall instead)
  // fd_t wgpu_dev = p_syscall_wgpu_opendev(0); print("wgpu_opendev:", wgpu_dev, "\n");
  // check_status(wgpu_dev, "wgpu_opendev");

  // create a graphics surface
  fd_t gui_surf = p_syscall_gui_mksurf(400, 300, /*wgpu_dev*/-1, 0);
  check_status(gui_surf, "gui_mksurf");

  // TODO: consider a gui_ctl syscall for controlling gui/wgpu resources, like setting
  // the title of a surface or assigning a different device to a surface.
  // This is how linux deals with configuration, e.g. ioctl, shmctl, msgctl, etc.

  // access WGPU resources for the graphics surface
  WGPUDevice  device  = p_gui_wgpu_device(gui_surf);  check_notnull(device);
  WGPUSurface surface = p_gui_wgpu_surface(gui_surf); check_notnull(surface);

  hello_triangle_set_device(device);
  hello_triangle_set_surface(surface);

  // runloop
  for (int i = 0; i < 20000; i++) {
    // read events from surface
    isize n = read_gui_msgs(gui_surf);
    if (n < 0) {
      if (n != p_err_end)
        check_status(n, "read(gui_surf)");
      break;
    }

    // render a frame
    hello_triangle_render();
  }

  // print("sleeping for 200ms\n");
  // sys_sleep(0, 200000000); // 200ms

  check_status(close(gui_surf), "close(wgpu_surf)");
  // check_status(close(wgpu_dev), "close(wgpu_dev)");
  check_status(close(ring), "close(ring)");

  return 0;
}

// ------------------------------------------------------------------------
// example helper functions

void print_cstr(const char* str) {
  write(P_FDSTDOUT, str, strlen(str));
}

void printerr(const char* str) {
  write(P_FDSTDERR, str, strlen(str));
}

void check_status(isize r, const char* contextmsg) {
  if (r >= 0)
    return;
  const char* errname = p_errname((err_t)r);
  printerr("error: ");
  printerr(p_err_str((err_t)r));
  printerr("; ");
  printerr(errname);
  if (contextmsg && strlen(contextmsg)) {
    printerr(" ("); printerr(contextmsg); printerr(")\n");
  } else {
    printerr("\n");
  }
  exit(1);
}

void check_notnull1(const char* str) {
  printerr("error: "); printerr(str);
  printerr(" is NULL\n");
  exit(1);
}

static u32 fmtuint(char* buf, usize bufsize, u64 v, u32 base) {
  char rbuf[20]; // 18446744073709551615 (0xFFFFFFFFFFFFFFFF)
  static const char chars[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  if (base > 62)
    base = 62;
  char* p = rbuf;
  do {
    *p++ = chars[v % base];
    v /= base;
  } while (v);
  u32 len = (u32)(p - rbuf);
  p--;
  char* dst = buf;
  char* end = buf + bufsize;
  while (rbuf <= p && buf < end) {
    *dst++ = *p--;
  }
  return len;
}

void print_uint(u64 v, u32 base) {
  char buf[21];
  u32 len = fmtuint(buf, sizeof(buf), v, base);
  write(P_FDSTDOUT, buf, len);
}

void print_sint(i64 v, u32 base) {
  char buf[21];
  usize offs = 0;
  u64 u = (u64)v;
  if (v < 0) {
    buf[offs++] = '-';
    u = (u64)-v;
  }
  u32 len = fmtuint(&buf[offs], sizeof(buf) - offs, u, base);
  write(P_FDSTDOUT, buf, (usize)len + offs);
}

// // open a new WGPU context
// fd_t wgpu = open("/sys/wgpu", p_open_rw, 0);
// check_status(wgpu, "open /sys/wgpu");
// print("opened wgpu interface (handle ", wgpu, ")\n");
// fd_t device = openat(wgpu, "dev/gpu0", p_open_ronly, 0);
// fd_t surface = openat(device, "surface", p_open_rw, 0);
// fd_t swapchain = openat(surface, "swapchain", p_open_rw, 0);
