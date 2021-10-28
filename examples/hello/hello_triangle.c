#include <playsys.h>
#include <playwgpu.h>
#include "hello.h"


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
  WGPURenderPipelineDescriptor pd = {};
  WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm; // FIXME

  // Fragment state
  WGPUBlendState blend = {};
  blend.color.operation = WGPUBlendOperation_Add;
  blend.color.srcFactor = WGPUBlendFactor_One;
  blend.color.dstFactor = WGPUBlendFactor_One;
  blend.alpha.operation = WGPUBlendOperation_Add;
  blend.alpha.srcFactor = WGPUBlendFactor_One;
  blend.alpha.dstFactor = WGPUBlendFactor_One;

  WGPUColorTargetState colorTarget = {};
  colorTarget.format = swapChainFormat;
  colorTarget.blend = &blend;
  colorTarget.writeMask = WGPUColorWriteMask_All;

  WGPUFragmentState fragment = {};
  fragment.module = fsmod;
  fragment.entryPoint = "main";
  fragment.targetCount = 1;
  fragment.targets = &colorTarget;
  pd.fragment = &fragment;

  // Other state
  pd.layout = NULL;
  pd.depthStencil = NULL;

  pd.vertex.module = vsmod;
  pd.vertex.entryPoint = "main";
  pd.vertex.bufferCount = 0;
  pd.vertex.buffers = NULL;

  pd.multisample.count = 1;
  pd.multisample.mask = 0xFFFFFFFF;
  pd.multisample.alphaToCoverageEnabled = false;

  pd.primitive.frontFace = WGPUFrontFace_CCW;
  pd.primitive.cullMode = WGPUCullMode_None;
  pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pd.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;

  return wgpuDeviceCreateRenderPipeline(device, &pd);
}


void hello_triangle_set_device(WGPUDevice device) {
  wgpuDeviceReference(device);
  if (g_device)
    wgpuDeviceRelease(g_device);
  g_device = device;

  WGPUShaderModule vsmod = create_wgsl_shader(device,
    "[[stage(vertex)]] fn main(\n"
    "    [[builtin(vertex_index)]] VertexIndex : u32\n"
    ") -> [[builtin(position)]] vec4<f32> {\n"
    "    var pos = array<vec2<f32>, 3>(\n"
    "        vec2<f32>( 0.0,  0.5),\n"
    "        vec2<f32>(-0.5, -0.5),\n"
    "        vec2<f32>( 0.5, -0.5));\n"
    "    return vec4<f32>(pos[VertexIndex], 0.0, 1.0);\n"
    "}\n");

  WGPUShaderModule fsmod = create_wgsl_shader(device,
    "[[stage(fragment)]] fn main() -> [[location(0)]] vec4<f32> {\n"
    "    return vec4<f32>(1.0, 0.8, 0.0, 1.0);\n"
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

#define fabs __builtin_fabs
#define sinf __builtin_sinf
#define cosf __builtin_cosf


void hello_triangle_render() {
  static u32 fc = 0;
  fc++;

  float RED   = fabs(sinf((float)(fc*2) / 100.0f));
  float GREEN = fabs(sinf((float)(fc*2) / 50.0f));
  float BLUE  = fabs(cosf((float)(fc*2) / 80.0f));

  WGPUTextureView backbufferView = wgpuSwapChainGetCurrentTextureView(g_swapchain);
  WGPURenderPassDescriptor renderpassInfo = {};
  WGPURenderPassColorAttachment colorAttachment = {};
  {
    colorAttachment.view = backbufferView;
    colorAttachment.resolveTarget = NULL;
    // colorAttachment.clearColor = (WGPUColor){0.05f, 0.1f, 0.1f, 0.0f};
    colorAttachment.clearColor = (WGPUColor){RED, GREEN, BLUE, 0.0f};
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    renderpassInfo.colorAttachmentCount = 1;
    renderpassInfo.colorAttachments = &colorAttachment;
    renderpassInfo.depthStencilAttachment = NULL;
  }
  WGPUCommandBuffer commands;
  {
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, NULL);

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderpassInfo);
    wgpuRenderPassEncoderSetPipeline(pass, g_pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEndPass(pass);
    wgpuRenderPassEncoderRelease(pass);

    commands = wgpuCommandEncoderFinish(encoder, NULL);
    wgpuCommandEncoderRelease(encoder);
  }

  WGPUQueue queue = wgpuDeviceGetQueue(g_device);
  wgpuQueueSubmit(queue, 1, &commands);
  wgpuCommandBufferRelease(commands);
  wgpuSwapChainPresent(g_swapchain);
  wgpuTextureViewRelease(backbufferView);
}
