#pragma once
#if defined(__gnu_linux__) || defined(__linux__)
  #define _GNU_SOURCE 1
#endif
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>

typedef signed char            i8;
typedef unsigned char          u8;
typedef signed short int       i16;
typedef unsigned short int     u16;
typedef signed int             i32;
typedef unsigned int           u32;
typedef signed long long int   i64;
typedef unsigned long long int u64;
typedef float                  f32;
typedef double                 f64;
typedef unsigned int           uint;
typedef signed long            isize;
typedef unsigned long          usize;
typedef signed long            intptr_t;
typedef unsigned long          uintptr_t;

// typedef void* (*p_malloc_t)(usize nbyte);
// typedef void  (*p_free_t)(void*);

// compiler feature test macros
#ifndef __has_attribute
  #define __has_attribute(x)  0
#endif
#ifndef __has_extension
  #define __has_extension   __has_feature
#endif
#ifndef __has_feature
  #define __has_feature(x)  0
#endif
#ifndef __has_include
  #define __has_include(x)  0
#endif
#ifndef __has_builtin
  #define __has_builtin(x)  0
#endif

// nullability
#if defined(__clang__) && __has_feature(nullability)
  #define ASSUME_NONNULL_BEGIN _Pragma("clang assume_nonnull begin")
  #define ASSUME_NONNULL_END   _Pragma("clang assume_nonnull end")
  #define __NULLABILITY_PRAGMA_PUSH _Pragma("clang diagnostic push")  \
    _Pragma("clang diagnostic ignored \"-Wnullability-completeness\"")
  #define __NULLABILITY_PRAGMA_POP _Pragma("clang diagnostic pop")
#else
  #define _Nullable
  #define _Nonnull
  #define _Null_unspecified
  #define __NULLABILITY_PRAGMA_PUSH
  #define __NULLABILITY_PRAGMA_POP
  #define ASSUME_NONNULL_BEGIN
  #define ASSUME_NONNULL_END
#endif
#define nullable      _Nullable
#define nonull        _Nonnull
#define nonnullreturn __attribute__((returns_nonnull))

#define _DIAGNOSTIC_IGNORE_PUSH(x) _Pragma("GCC diagnostic push") _Pragma(#x)
#define DIAGNOSTIC_IGNORE_PUSH(x)  _DIAGNOSTIC_IGNORE_PUSH(GCC diagnostic ignored #x)
#define DIAGNOSTIC_IGNORE_POP      _Pragma("GCC diagnostic pop")

#ifdef __cplusplus
  #define NORETURN noreturn
#else
  #define NORETURN      _Noreturn
  #define auto          __auto_type
  #define static_assert _Static_assert
#endif

#if __has_attribute(fallthrough)
  #define FALLTHROUGH __attribute__((fallthrough))
#else
  #define FALLTHROUGH
#endif

#if __has_attribute(musttail)
  #define MUSTTAIL __attribute__((musttail))
#else
  #define MUSTTAIL
#endif

#ifndef thread_local
  #define thread_local _Thread_local
#endif

#ifdef __cplusplus
  #define EXTERN_C extern "C"
#else
  #define EXTERN_C
#endif

// ATTR_FORMAT(archetype, string-index, first-to-check)
// archetype determines how the format string is interpreted, and should be printf, scanf,
// strftime or strfmon.
// string-index specifies which argument is the format string argument (starting from 1),
// while first-to-check is the number of the first argument to check against the format string.
// For functions where the arguments are not available to be checked (such as vprintf),
// specify the third parameter as zero.
#if __has_attribute(format)
  #define ATTR_FORMAT(...) __attribute__((format(__VA_ARGS__)))
#else
  #define ATTR_FORMAT(...)
#endif

#if __has_attribute(always_inline)
  #define ALWAYS_INLINE __attribute__((always_inline)) inline
#else
  #define ALWAYS_INLINE inline
#endif

#if __has_attribute(noinline)
  #define NO_INLINE __attribute__((noinline))
#else
  #define NO_INLINE
#endif

#if __has_attribute(unused)
  #define UNUSED __attribute__((unused))
#else
  #define UNUSED
#endif

#if __has_attribute(warn_unused_result)
  #define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
  #define WARN_UNUSED_RESULT
#endif

#if __has_feature(address_sanitizer)
  // https://clang.llvm.org/docs/AddressSanitizer.html
  #define ASAN_ENABLED 1
  #define ASAN_DISABLE_ADDR_ATTR __attribute__((no_sanitize("address"))) /* function attr */
#else
  #define ASAN_DISABLE_ADDR_ATTR
#endif

#ifndef offsetof
  #if __has_builtin(__builtin_offsetof)
    #define offsetof __builtin_offsetof
  #else
    #define offsetof(st, m) ((size_t)&(((st*)0)->m))
  #endif
#endif

#ifndef alignof
  #define alignof _Alignof
#endif

#ifndef alignas
  #define alignas _Alignas
#endif

#ifndef countof
  #define countof(x) \
    ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#endif

#define MAX(a,b) \
  ({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
  // turns into CMP + CMOV{L,G} on x86_64
  // turns into CMP + CSEL on arm64

#define MIN(a,b) \
  ({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
  // turns into CMP + CMOV{L,G} on x86_64
  // turns into CMP + CSEL on arm64

// T align2<T>(T x, T y) rounds up n to closest boundary w (w must be a power of two)
// E.g.
//   palign2(0, 4) => 0
//   palign2(1, 4) => 4
//   palign2(2, 4) => 4
//   palign2(3, 4) => 4
#define palign2(n,w) ({ \
  assert(((w) & ((w) - 1)) == 0); /* alignment w is not a power of two */ \
  ((n) + ((w) - 1)) & ~((w) - 1); \
})

#define plog(format, ...) ({ \
  fprintf(stderr, "\e[1;34m[plog]\e[0m " format " \e[2m(%s %d)\e[0m\n", \
    ##__VA_ARGS__, __FUNCTION__, __LINE__); \
  fflush(stderr); \
})

#define perrlog(format, ...) (({ \
  fprintf(stderr, "error: " format " (%s:%d)\n", \
    ##__VA_ARGS__, __FILE__, __LINE__); \
  fflush(stderr); \
}))

#define fabs  __builtin_fabs
#define sinf  __builtin_sinf
#define cosf  __builtin_cosf
#define floor __builtin_floor
#define ceil  __builtin_ceil
