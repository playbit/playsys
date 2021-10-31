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
void check_notnull1(const char* str);

void print_cstr(const char* str);
void print_sint(i64 v, u32 base);
void print_uint(u64 v, u32 base);
inline static void print_sint10(i64 v) { print_sint(v, 10); }
inline static void print_uint10(u64 v) { print_uint(v, 10); }
inline static isize write_cstr(fd_t fd, const char* s) { return write(fd, s, strlen(s)); }

void check_status(isize r, const char* contextmsg);

void hello_triangle_set_device(WGPUDevice);
void hello_triangle_set_surface(WGPUSurface);
void hello_triangle_render();
