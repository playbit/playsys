#ifndef DAWN_ENABLE_BACKEND_METAL
#error !DAWN_ENABLE_BACKEND_METAL
#endif

#include "playwgpu_internal.h"
#include <memory>
#include <dawn/webgpu_cpp.h>
#include <QuartzCore/CAMetalLayer.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

std::unique_ptr<wgpu::ChainedStruct> p_gui_surf_wgpu_descriptor(p_gui_surf_t* surf) {
  NSWindow* nswin = glfwGetCocoaWindow(surf->window);
  NSView* view = [nswin contentView];

  [view setWantsLayer:YES];
  [view setLayer:[CAMetalLayer layer]];
  [[view layer] setContentsScale:[nswin backingScaleFactor]];

  std::unique_ptr<wgpu::SurfaceDescriptorFromMetalLayer> desc =
      std::make_unique<wgpu::SurfaceDescriptorFromMetalLayer>();
  desc->layer = [view layer];
  return std::move(desc);
}
