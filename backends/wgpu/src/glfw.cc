// SPDX-License-Identifier: Apache-2.0

#include "playwgpu_internal.h"

#include <GLFW/glfw3.h>
#include <utils/GLFWUtils.h> /* from dawn */
#include <dawn_native/DawnNative.h>


// struct guimsg_t {
//   p_gui_msghdr_t hdr;
//   union {
//     p_gui_surfinfo_t surfinfo;
//   };
// };



static err_t grow_rbuf(p_ringbuf_t* dst, u32 mingrow) {
  u32 newcap = MIN(dst->cap + mingrow, dst->cap * 2);
  const u32 maxcap = 4096*128; // 512Kib
  if (newcap > maxcap)
    return p_err_nomem;
  void* p = realloc(dst->p, newcap);
  if (!p)
    return p_err_nomem;
  p_ringbuf_move(dst, p, newcap);
  return 0;
}


template <typename T>
static void surf_post_guimsg(p_gui_surf_t& surf, p_gui_msg_t type, const T& msg) {
  p_gui_msghdr_t h = { .type = type, .size = sizeof(T) };

  if (surf.rbuf.cap - surf.rbuf.len < sizeof(p_gui_msghdr_t) + sizeof(T)) {
    err_t e = grow_rbuf(&surf.rbuf, sizeof(p_gui_msghdr_t) + sizeof(T));
    if (e < 0)
      errlog("write_guimsg failed (err_t %s)", p_err_str(e));
  }

  p_ringbuf_write(&surf.rbuf, &h, sizeof(p_gui_msghdr_t));
  if (sizeof(T) > 0)
    p_ringbuf_write(&surf.rbuf, &msg, sizeof(T));
}


isize p_gui_surf_read(p_gui_surf_t* surf, void* data, usize nbyte) {
  if (!surf->window)
    return p_err_end;

  // flush buffered messages
  u32 rn = p_ringbuf_read(&surf->rbuf, data, (u32)nbyte);
  if (rn != 0) {
    if (surf->rbuf.len > 0) // more to read?
      return (isize)rn;
    nbyte -= (usize)rn; // for second p_ringbuf_read call
    data = (char*)data + rn;
  }

  if (glfwWindowShouldClose(surf->window)) {
    glfwDestroyWindow(surf->window);
    surf->window = NULL;
    return p_err_end;
  }

  // read input that the user wrote with write()
  u8 buf[128] = {0};
  isize len = read(surf->fd, buf, sizeof(buf));
  if (len > 0)
    dlog("TODO handle command from userland");

  // process OS events
  glfwWaitEvents();

  // read any queued messages
  return (isize)(rn + p_ringbuf_read(&surf->rbuf, data, nbyte));
}


void p_gui_surf_os_free(p_gui_surf_t* surf) {
  assert(surf->window != NULL);
  glfwDestroyWindow(surf->window);
}


static void report_glfw_error(int code, const char* message) {
  fprintf(stderr, "GLFW error: [%d] %s\n", code, message);
}


// onFramebufferResize is called when a window's framebuffer has changed size.
// width & height are in pixels (the framebuffer size)
static void onFramebufferResize(GLFWwindow* window, int width, int height) {
  dlog("onFramebufferResize");
  p_gui_surf_t& surf = *(p_gui_surf_t*)glfwGetWindowUserPointer(window);
  float yscale;
  glfwGetWindowContentScale(window, &surf.info.dpscale, &yscale);
  surf.info.width = (u32)width;
  surf.info.height = (u32)height;
  surf_post_guimsg(surf, P_GUI_MSG_SURFINFO, surf.info);
}


static void set_win_hints() {
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
  if (p_wgpu_backend_type() == wgpu::BackendType::OpenGL) {
    // Ask for OpenGL 4.4 which is what the GL backend requires for compute shaders and
    // texture views.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  } else if (p_wgpu_backend_type() == wgpu::BackendType::OpenGLES) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
  } else {
    // Without this GLFW will initialize a GL context on the window, which prevents using
    // the window with other APIs (by crashing in weird ways).
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  }
}


err_t p_gui_surf_os_init(p_gui_surf_t* surf, p_gui_surf_descr_t* descr) {
  static bool is_init = false;
  if (!is_init) {
    is_init = true;
    if (!glfwInit()) // Note: Safe to call multiple times
      dlog("GLFW failed to initialize");
    glfwSetErrorCallback(report_glfw_error);
    dlog("GLFW %s", glfwGetVersionString());
  }

  assert(surf->window == NULL);

  // Setup the correct hints for GLFW for backends (dawn-specific)
  set_win_hints();

  // check dimensions
  if (descr->width > 0x7fffffff || descr->height > 0x7fffffff)
    return p_err_invalid;
  if (descr->width == 0)
    descr->width = 640;
  if (descr->height == 0)
    descr->height = 480;
  int width = (int)descr->width;
  int height = (int)descr->height;

  // create window
  const char* title = descr->title ? descr->title : ""; // must not be null
  surf->window = glfwCreateWindow(width, height, title, NULL, NULL);
  if (!surf->window)
    return p_err_invalid;
  glfwSetWindowUserPointer(surf->window, surf);

  // [rsms] move window to bottom right corner of screen
  if (getenv("RSMS_DEV_SETUP")) {
    GLFWmonitor* monitor = glfwGetWindowMonitor(surf->window);
    if (!monitor)
      monitor = glfwGetPrimaryMonitor();
    if (monitor) {
      const GLFWvidmode* vmode = glfwGetVideoMode(monitor);
      glfwSetWindowPos(surf->window, vmode->width - width, vmode->height - height);
    }
  }

  // get framebuffer info
  float yscale = 1.0;
  glfwGetFramebufferSize(surf->window, (int*)&surf->info.width, (int*)&surf->info.height);
  glfwGetWindowContentScale(surf->window, &surf->info.dpscale, &yscale);
  surf_post_guimsg(*surf, P_GUI_MSG_SURFINFO, surf->info);

  // hook up GLFW event callbacks
  glfwSetFramebufferSizeCallback(surf->window, onFramebufferResize);
  // // glfwSetWindowSizeCallback(surf->window, onWindowResize);
  // glfwSetCursorPosCallback(surf->window, onCursorPos);
  // glfwSetMouseButtonCallback(surf->window, onMouseButton);
  // glfwSetKeyCallback(surf->window, onKeyboard);
  // glfwSetScrollCallback(surf->window, onScroll);
  // glfwSetWindowIconifyCallback(surf->window, onWindowIconify);

  return 0;
}

