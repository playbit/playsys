#include <playsys.h>
#include <playwgpu.h>
#include "hello.h"

// we have no libc, but llvm has most math functions built-in
#define fabs __builtin_fabs
#define sinf __builtin_sinf
#define cosf __builtin_cosf

static WGPUDevice         g_device = NULL;
static WGPUSurface        g_surface = NULL;
static WGPURenderPipeline g_pipeline = NULL;
static WGPUSwapChain      g_swapchain = NULL;


static WGPUShaderModule create_wgsl_shader(WGPUDevice device, const char* source) {
  WGPUShaderModuleWGSLDescriptor wgsl = {
    .chain = { .sType = WGPUSType_ShaderModuleWGSLDescriptor },
    .source = source,
  };
  WGPUShaderModuleDescriptor d = { .nextInChain = &wgsl.chain };
  return wgpuDeviceCreateShaderModule(device, &d);
}


static WGPURenderPipeline create_pipeline(
  WGPUDevice device, WGPUShaderModule vsmod, WGPUShaderModule fsmod)
{
  check_notnull(device);
  check_notnull(vsmod);
  check_notnull(fsmod);
  WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm; // FIXME

  // Fragment state
  WGPUBlendState blend = {
    .color = {
      .operation = WGPUBlendOperation_Add,
      .srcFactor = WGPUBlendFactor_One,
      .dstFactor = WGPUBlendFactor_One,
    },
    .alpha = {
      .operation = WGPUBlendOperation_Add,
      .srcFactor = WGPUBlendFactor_One,
      .dstFactor = WGPUBlendFactor_One,
    },
  };
  WGPUColorTargetState colorTarget = {
    .format = swapChainFormat,
    .blend = &blend,
    .writeMask = WGPUColorWriteMask_All,
  };
  WGPUFragmentState fragment = {
    .module = fsmod,
    .entryPoint = "main",
    .targetCount = 1,
    .targets = &colorTarget,
  };

  WGPURenderPipelineDescriptor pd = {
    .vertex = {
      .module = vsmod,
      .entryPoint = "main",
    },
    .primitive = {
      .frontFace = WGPUFrontFace_CCW,
      .cullMode = WGPUCullMode_None,
      .topology = WGPUPrimitiveTopology_TriangleList,
      .stripIndexFormat = WGPUIndexFormat_Undefined,
    },
    .multisample = {
      .count = 1,
      .mask = 0xFFFFFFFF,
      .alphaToCoverageEnabled = false,
    },
    .fragment = &fragment,
  };

  return wgpuDeviceCreateRenderPipeline(device, &pd);
}


void hello_triangle_set_device(WGPUDevice device) {
  wgpuDeviceReference(device);
  if (g_device)
    wgpuDeviceRelease(g_device);
  g_device = device;

  WGPUShaderModule vsmod = create_wgsl_shader(device,
    "[[stage(vertex)]] fn main(\n"
    "  [[builtin(vertex_index)]] VertexIndex : u32\n"
    ") -> [[builtin(position)]] vec4<f32>\n"
    "{\n"
    "  var pos = array<vec2<f32>, 3>(\n"
    "    vec2<f32>( 0.0,  0.5),\n"
    "    vec2<f32>(-0.5, -0.5),\n"
    "    vec2<f32>( 0.5, -0.5));\n"
    "  return vec4<f32>(pos[VertexIndex], 0.0, 1.0);\n"
    "}\n");

  WGPUShaderModule fsmod = create_wgsl_shader(device,
    "[[stage(fragment)]] fn main() -> [[location(0)]] vec4<f32> {\n"
    "  return vec4<f32>(1.0, 0.8, 0.0, 1.0);\n"
    "}\n");

  if (g_pipeline)
    wgpuRenderPipelineRelease(g_pipeline);
  g_pipeline = create_pipeline(device, vsmod, fsmod);

  wgpuShaderModuleRelease(vsmod);
  wgpuShaderModuleRelease(fsmod);
}


void hello_triangle_set_surface(WGPUSurface surface) {
  wgpuSurfaceReference(surface);
  if (g_surface)
    wgpuSurfaceRelease(g_surface);
  g_surface = surface;

  static WGPUSwapChainDescriptor scdesc = {
    .usage  = WGPUTextureUsage_RenderAttachment,
    .format = WGPUTextureFormat_BGRA8Unorm,
    .width  = 400*2,
    .height = 300*2,
    .presentMode = WGPUPresentMode_Mailbox,
  };
  if (g_swapchain)
    wgpuSwapChainRelease(g_swapchain);
  g_swapchain = wgpuDeviceCreateSwapChain(g_device, surface, &scdesc);
}


void hello_triangle_render() {
  static u32 fc = 0;
  fc++;

  float RED   = fabs(sinf((float)(fc*2) / 100.0f));
  float GREEN = fabs(sinf((float)(fc*2) / 50.0f));
  float BLUE  = fabs(cosf((float)(fc*2) / 80.0f));

  WGPUTextureView backbufferView = wgpuSwapChainGetCurrentTextureView(g_swapchain);
  WGPURenderPassColorAttachment colorAttachment = {
    .view = backbufferView,
    .clearColor = (WGPUColor){RED, GREEN, BLUE, 0.0f},
    .loadOp = WGPULoadOp_Clear,
    .storeOp = WGPUStoreOp_Store,
  };
  WGPURenderPassDescriptor renderpassInfo = {
    .colorAttachmentCount = 1,
    .colorAttachments = &colorAttachment,
    .depthStencilAttachment = NULL,
  };

  // acquire a command encoder
  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, NULL);

  // execute a render pass, writing commands to encoder
  WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderpassInfo);
  wgpuRenderPassEncoderSetPipeline(pass, g_pipeline);
  wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
  wgpuRenderPassEncoderEndPass(pass);
  wgpuRenderPassEncoderRelease(pass);

  // get commands buffered in the encoder
  WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, NULL);
  wgpuCommandEncoderRelease(encoder);

  // send commands to device
  WGPUQueue queue = wgpuDeviceGetQueue(g_device);
  wgpuQueueSubmit(queue, 1, &commands);
  wgpuCommandBufferRelease(commands);

  // show the new frame we drew on screen
  wgpuSwapChainPresent(g_swapchain);

  wgpuTextureViewRelease(backbufferView);
}
