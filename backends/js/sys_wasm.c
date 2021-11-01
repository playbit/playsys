// __attribute__((import_module("module_name")))
// __attribute__((import_name("function_name")))
// __attribute__((export_name("function_name")))

// // asyncify imports
// #define WASM_IMPORT(modname,funname) \
//   __attribute__((import_module(modname))) \
//   __attribute__((import_name(funname)))

// WASM_IMPORT("asyncify", "start_unwind") void asyncify_start_unwind(int);
// WASM_IMPORT("asyncify", "stop_unwind") void asyncify_stop_unwind(void);
// WASM_IMPORT("asyncify", "start_rewind") void asyncify_start_rewind(int);
// WASM_IMPORT("asyncify", "stop_rewind") void asyncify_stop_rewind(void);

unsigned long strlen(const char *s) {
  const char* p = s;
  while (*p) { p++; }
  return (unsigned long)(p - s);
}

// void sys_wasm_main() {
//   main(0, 0);
//   asyncify_stop_unwind();
// }

// static char g_mem_buf[2048];
// static int g_mem_end = 0;

// void* malloc(unsigned long size) {
//   void* ptr = &g_mem_buf[g_mem_end];
//   g_mem_end += size;
//   return ptr;
// }

// void free(void* ptr) {
// }
