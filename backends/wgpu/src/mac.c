#ifdef PWGPU_STATIC_LIB

// HERE BE DRAGONS!
// On macos dawn/src/dawn_native/metal/ShaderModuleMTL.mm uses the @available ObjC
// feature which expects a runtime symbol that is AFAIK only provided by Apple's
// version of clang.
// extern "C"
int __isPlatformVersionAtLeast(
  long unkn, long majv, long minv, long buildv)
{
  // <= 10.15.x
  return majv < 10 || (majv == 10 && minv <= 15);
}

#endif
