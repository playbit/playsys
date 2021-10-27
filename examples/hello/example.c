#include <playsys.h>
#include <playwgpu.h>
#include "syslib.h"

#define PUB __attribute__((visibility("default"))) /* WASM export */
#ifndef NULL
  #define NULL ((void*)0)
#endif

#define print1(x) _Generic((x), \
  unsigned long long: print_uint10, \
  unsigned long:      print_uint10, \
  unsigned int:       print_uint10, \
  long long:          print_sint10, \
  long:               print_sint10, \
  int:                print_sint10, \
  const char*:        print_cstr, \
  char*:              print_cstr \
)(x)

#define __PRINT_NARGS_X(a,b,c,d,e,f,g,h,n,...) n
#define __PRINT_NARGS(...) __PRINT_NARGS_X(__VA_ARGS__,8,7,6,5,4,3,2,1,)

#define __PRINT_CONCAT_X(a,b) a##b
#define __PRINT_CONCAT(a,b) __PRINT_CONCAT_X(a,b)
#define __PRINT_DISP(a,...) __PRINT_CONCAT(a,__PRINT_NARGS(__VA_ARGS__))(__VA_ARGS__)

#define __print1(a) print1(a)
#define __print2(a,b) do { print1(a); print1(b); }while(0)
#define __print3(a,b,c) do { print1(a); print1(b); print1(c); }while(0)
#define __print4(a,b,c,d) do { print1(a); print1(b); print1(c); print1(d); }while(0)
#define __print5(a,b,c,d,e) do { print1(a); print1(b); print1(c); print1(d); print1(e);\
  }while(0)
#define __print6(a,b,c,d,e,f) do { print1(a); print1(b); print1(c); print1(d); print1(e);\
  print1(f); }while(0)
#define __print7(a,b,c,d,e,f,g) do { print1(a); print1(b); print1(c); print1(d); print1(e);\
  print1(f); print1(g); }while(0)
#define __print8(a,b,c,d,e,f,g,h) do { print1(a); print1(b); print1(c); print1(d); print1(e);\
  print1(f); print1(g); print1(h); }while(0)

#define print(...) __PRINT_DISP(__print,__VA_ARGS__)

#define check_notnull(x) if ((x) == NULL) { check_notnull1(#x); }
static void check_notnull1(const char* str);

static void print_cstr(const char* str);
static void print_sint(i64 v, u32 base);
static void print_uint(u64 v, u32 base);
inline static void print_sint10(i64 v) { print_sint(v, 10); }
inline static void print_uint10(u64 v) { print_uint(v, 10); }
inline static isize write_cstr(sys_fd fd, const char* s) { return write(fd, s, strlen(s)); }

static void check_status(sys_ret r, const char* contextmsg);

// ------------------------------------------------------------------------

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

  // sys_fd pwgpu_fd = open("/sys/wgpu", sys_open_rw, 0);
  // check_status(pwgpu_fd, "open /sys/wgpu");
  // print("opened wgpu connection (handle ", pwgpu_fd, ")\n");

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

  // From hereon we can use the standard WebGPU API
  WGPUSwapChainDescriptor scdesc = {
    .format = WGPUTextureFormat_RGBA8Unorm,
    .usage  = WGPUTextureUsage_RenderAttachment,
    .width  = 400, // TODO surf->fbwidth
    .height = 400, // TODO surf->fbheight
    .presentMode = WGPUPresentMode_Mailbox,
  };
  WGPUSwapChain swapchain = wgpuDeviceCreateSwapChain(device, surface, &scdesc);
  check_notnull(swapchain);

  // runloop
  for (int i = 0; i < 500; i++) {
    // if (i == 200) // change size
    //   write_cstr(pwgpu_surf, "width 700\n");

    // isize n = read(pwgpu_ctl, buf, sizeof(buf));
    // if (n < 1)
    //   break;

    isize n = read(pwgpu_surf, buf, sizeof(buf));
    if (n < 1)
      break; // surface closed
  }

  pwgpu_ctx_dispose(ctx);

  // print("sleeping for 200ms\n");
  // sys_sleep(0, 200000000); // 200ms
  print("closing wgpu handles\n");
  check_status(close(pwgpu_surf), "close(pwgpu_surf)");
  check_status(close(pwgpu_dev), "close(pwgpu_dev)");

  return 0;
}

// ------------------------------------------------------------------------
// example helper functions

static void print_cstr(const char* str) {
  write(SYS_FD_STDOUT, str, strlen(str));
}

static void printerr(const char* str) {
  write(SYS_FD_STDERR, str, strlen(str));
}

static void check_status(sys_ret r, const char* contextmsg) {
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

static void check_notnull1(const char* str) {
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

static void print_uint(u64 v, u32 base) {
  char buf[21];
  u32 len = fmtuint(buf, sizeof(buf), v, base);
  write(SYS_FD_STDOUT, buf, len);
}

static void print_sint(i64 v, u32 base) {
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
