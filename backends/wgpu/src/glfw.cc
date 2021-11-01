// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

#include "playwgpu_internal.h"

#include <GLFW/glfw3.h>
#include <utils/GLFWUtils.h> /* from dawn */
#include <dawn_native/DawnNative.h>

isize pwgpu_surface_read(pwgpu_surface_t* surf, void* data, usize size) {
  switch (surf->state) {
    case WGPU_SURF_STATE_INIT: {
      isize r = _pwgpu_surface_init(surf);
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


// ———————————————————————————————————————————————————————————————————_
// glfw specific


static void report_glfw_error(int code, const char* message) {
  fprintf(stderr, "GLFW error: [%d] %s\n", code, message);
}


isize _pwgpu_surface_init_oswin(
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

  // Setup the correct hints for GLFW for backends (dawn-specific)
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
  if (_pwgpu_backend_type() == wgpu::BackendType::OpenGL) {
    // Ask for OpenGL 4.4 which is what the GL backend requires for compute shaders and
    // texture views.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  } else if (_pwgpu_backend_type() == wgpu::BackendType::OpenGLES) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
  } else {
    // Without this GLFW will initialize a GL context on the window, which prevents using
    // the window with other APIs (by crashing in weird ways).
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  }

  // create window
  surf->window = glfwCreateWindow((int)width, (int)height, title, NULL, NULL);
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
      glfwSetWindowPos(surf->window, vmode->width - (int)width, vmode->height - (int)height);
    }
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

