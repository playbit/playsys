#include <playsys.h>
#include <playwgpu.h>
#include "hello.h"


void read_file(const char* path, void* buf, usize cap) {
  sys_fd fd      = open(path, sys_open_ronly, 0); check_status(fd, "open");
  isize readlen  = read(fd, buf, cap);            check_status(readlen, "read");
  sys_ret status = close(fd);                     check_status(status, "close");
  print("read from file ", path, ": ");
  write(SYS_FD_STDOUT, buf, readlen);
  if (readlen > 0 && ((u8*)buf)[readlen - 1] != '\n')
    print("\n");
}


PUB int main(int argc, const char** argv) {
  u8* buf[256];
  read_file("/sys/uname", buf, sizeof(buf));

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

  // select a GPU device
  sys_fd pwgpu_dev = open("/sys/wgpu/dev/gpu0", sys_open_ronly, 0);
  check_status(pwgpu_dev, "open /sys/wgpu/dev/gpu0");
  print("opened wgpu device (handle ", pwgpu_dev, ")\n");
  // TODO: allow sys_open_rw on device to enable simple compute

  // create a graphics surface
  sys_fd pwgpu_surf = open("/sys/wgpu/surface", sys_open_rw, 0);
  check_status(pwgpu_surf, "open /sys/wgpu/surface");
  print("opened wgpu surface (handle ", pwgpu_surf, ")\n");
  write_cstr(pwgpu_surf,
    "width  400\n"
    "height 300\n"
    "title  Hello world\n");

  // create WebGPU API interface
  u8 ctx_mem[PWGPU_CTX_SIZE];
  pwgpu_ctx_t* ctx = pwgpu_ctx_create(ctx_mem);
  WGPUDevice device = pwgpu_ctx_set_device(ctx, pwgpu_dev); check_notnull(device);
  WGPUSurface surface = pwgpu_ctx_set_surface(ctx, pwgpu_surf); check_notnull(surface);

  hello_triangle_set_device(device);
  hello_triangle_set_surface(surface);

  // runloop
  for (int i = 0; i < 500; i++) {
    // if (i == 200) // change size
    //   write_cstr(pwgpu_surf, "width 700\n");

    hello_triangle_render();

    isize n = read(pwgpu_surf, buf, sizeof(buf));
    if (n < 1)
      break; // surface closed
  }

  // print("sleeping for 200ms\n");
  // sys_sleep(0, 200000000); // 200ms

  pwgpu_ctx_dispose(ctx);
  check_status(close(pwgpu_surf), "close(pwgpu_surf)");
  check_status(close(pwgpu_dev), "close(pwgpu_dev)");

  return 0;
}

// ------------------------------------------------------------------------
// example helper functions

void print_cstr(const char* str) {
  write(SYS_FD_STDOUT, str, strlen(str));
}

void printerr(const char* str) {
  write(SYS_FD_STDERR, str, strlen(str));
}

void check_status(sys_ret r, const char* contextmsg) {
  if (r < 0) {
    sys_err err = (sys_err)-r;
    const char* errname = sys_errname(err);
    printerr("error: "); printerr(errname);
    if (contextmsg && strlen(contextmsg)) {
      printerr(" ("); printerr(contextmsg); printerr(")\n");
    } else {
      printerr("\n");
    }
    exit(1);
  }
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
  write(SYS_FD_STDOUT, buf, len);
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
  write(SYS_FD_STDOUT, buf, (usize)len + offs);
}

// // open a new WGPU context
// sys_fd wgpu = open("/sys/wgpu", sys_open_rw, 0);
// check_status(wgpu, "open /sys/wgpu");
// print("opened wgpu interface (handle ", wgpu, ")\n");
// sys_fd device = openat(wgpu, "dev/gpu0", sys_open_ronly, 0);
// sys_fd surface = openat(device, "surface", sys_open_rw, 0);
// sys_fd swapchain = openat(surface, "swapchain", sys_open_rw, 0);
