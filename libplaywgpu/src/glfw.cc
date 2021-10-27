// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

#include "playwgpu_internal.h"

#include <GLFW/glfw3.h>
#include <utils/GLFWUtils.h> /* from dawn */

#include <dawn/dawn_proc.h>
#include <dawn_wire/WireServer.h>
#include <dawn_native/DawnNative.h>



sys_ret pwgpu_surface_read(pwgpu_surface_t* surf, void* data, usize size) {
  switch (surf->state) {
    case WGPU_SURF_STATE_INIT: {
      sys_ret r = _pwgpu_surface_init(surf);
      if (r != 0)
        return r;
      break;
    }
    case WGPU_SURF_STATE_ACTIVE:
      break;
    case WGPU_SURF_STATE_CLOSED:
      return 0;
  }

  if (glfwWindowShouldClose(surf->window)) {
    glfwDestroyWindow(surf->window);
    surf->window = NULL;
    surf->state = WGPU_SURF_STATE_CLOSED;
    return 0;
  }

  // read input that the user wrote with write()
  u8 buf[128] = {0};
  isize len = read(surf->fd, buf, sizeof(buf));
  if (len > 0) {
    dlog("TODO handle command from userland");
  }

  // process events
  glfwWaitEvents();

  if (size == 0)
    return 0;
  ((u8*)data)[0] = '1';
  return 1;
}


void _pwgpu_surface_free_oswin(pwgpu_surface_t* surf) {
  assert(surf->window != NULL);
  glfwDestroyWindow(surf->window);
}


std::unique_ptr<wgpu::ChainedStruct> _pwgpu_surface_descriptor(pwgpu_surface_t* surf) {
  assert(surf->window != NULL);
  return utils::SetupWindowAndGetSurfaceDescriptorForTesting(surf->window);
}

// ———————————————————————————————————————————————————————————————————_
// glfw specific


static void report_glfw_error(int code, const char* message) {
  fprintf(stderr, "GLFW error: [%d] %s\n", code, message);
}


sys_ret _pwgpu_surface_init_oswin(
  pwgpu_surface_t* surf, u32 width, u32 height, const char* title)
{
  static bool is_init = false;
  if (!is_init) {
    is_init = true;
    if (!glfwInit()) // Note: Safe to call multiple times
      dlog("GLFW failed to initialize");
    glfwSetErrorCallback(report_glfw_error);
    dlog("GLFW %s", glfwGetVersionString());
  }

  assert(surf->window == NULL);

  // [dawn] Setup the correct hints for GLFW for backends
  utils::SetupGLFWWindowHintsForBackend(_pwgpu_backend_type());

  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);

  // create window
  surf->window = glfwCreateWindow((int)width, (int)height, title, NULL, NULL);
  if (!surf->window)
    return -sys_err_invalid;

  // make it possible to refer to the app struct from glfw window callbacks
  glfwSetWindowUserPointer(surf->window, surf);

  // // log some monitor information
  // // TODO: map window to monitor by comparing window pos to glfwGetMonitors() values.
  // // TODO: register window move callback GLFWwindowposfun and update io.pxPerMM
  // {
  //   GLFWmonitor* monitor = fullscreenMonitor ? fullscreenMonitor : glfwGetPrimaryMonitor();
  //   assert(monitor != NULL);
  //   int monWidthMM, monHeightMM;
  //   glfwGetMonitorPhysicalSize(monitor, &monWidthMM, &monHeightMM);
  //   // TODO: use CGDisplayScreenSize on mac for more accurate readout of physical size.
  //   const GLFWvidmode* vmode = glfwGetVideoMode(monitor);
  //   float xscale = 1.0, yscale = 1.0;
  //   glfwGetMonitorContentScale(monitor, &xscale, &yscale);
  //   _host.io().pxPerMM = (float)( ((double)vmode->width * xscale) / (double)monWidthMM );
  //   dlog("glfw monitor: \"%s\"\n"
  //        "  %dx%dmm  %ux%upx  %.4f mm/px  %.2f px/mm  %d Hz  %d bit/ch  %.2fx%.2f scale",
  //     glfwGetMonitorName(monitor),
  //     monWidthMM, monHeightMM,
  //     (u32)vmode->width * (u32)xscale, (u32)vmode->height * (u32)yscale,
  //     1.0/_host.io().pxPerMM,
  //     _host.io().pxPerMM,
  //     vmode->refreshRate,
  //     vmode->greenBits,
  //     xscale, yscale);
  // }

  // [rsms] move window to bottom right corner of screen
  if (getenv("RSMS_DEV_SETUP")) {
    GLFWmonitor* monitor = glfwGetWindowMonitor(surf->window);
    const GLFWvidmode* vmode = glfwGetVideoMode(monitor);
    glfwSetWindowPos(surf->window, vmode->width - (int)width, vmode->height - (int)height);
  }

  // get framebuffer info
  float yscale = 1.0;
  glfwGetFramebufferSize(surf->window, (int*)&surf->fbwidth, (int*)&surf->fbheight);
  glfwGetWindowContentScale(surf->window, &surf->fbscale, &yscale);

  // // GLFW event callbacks
  // glfwSetFramebufferSizeCallback(surf->window, onWindowFramebufferResize);
  // // glfwSetWindowSizeCallback(surf->window, onWindowResize);
  // glfwSetCursorPosCallback(surf->window, onCursorPos);
  // glfwSetMouseButtonCallback(surf->window, onMouseButton);
  // glfwSetKeyCallback(surf->window, onKeyboard);
  // glfwSetScrollCallback(surf->window, onScroll);
  // glfwSetWindowIconifyCallback(surf->window, onWindowIconify);

  return 0;
}

