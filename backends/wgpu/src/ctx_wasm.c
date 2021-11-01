#include <playwgpu.h>

#define static_assert _Static_assert

#define strlen __builtin_strlen
#define memcpy __builtin_memcpy
#define memset __builtin_memset

struct pwgpu_ctx {
  WGPUDevice  device;
  WGPUSurface surface;
};

pwgpu_ctx_t* pwgpu_ctx_create(void* mem) {
  static_assert(PWGPU_CTX_SIZE >= sizeof(pwgpu_ctx_t), "update PWGPU_CTX_SIZE");
  memset(mem, 0, sizeof(pwgpu_ctx_t));
  pwgpu_ctx_t* ctx = (pwgpu_ctx_t*)mem;
  return ctx;
}

void pwgpu_ctx_dispose(pwgpu_ctx_t* ctx) {
}


WGPUDevice pwgpu_ctx_set_device(pwgpu_ctx_t* ctx, fd_t user_fd) {
  // TODO
  return ctx->device;
}

WGPUSurface pwgpu_ctx_set_surface(pwgpu_ctx_t* ctx, fd_t user_fd) {
  // TODO
  return ctx->surface;
}
