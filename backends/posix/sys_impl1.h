// SPDX-License-Identifier: MIT
//
// -- this file should not be included directly --
//
// It is included by sys_impl.h and contains code adapted from Linux and
// is licensed under the MIT license (<linux-src>/LICENSES/preferred/MIT)
//
// MIT License
//
// Copyright (c) <year> <copyright holders>
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#define __same_type(a, b) __builtin_types_compatible_p(__typeof__(a), __typeof__(b))

#define _ALIGN1(x, a)          _ALIGN1_MASK(x, (__typeof__(x))(a) - 1)
#define _ALIGN1_MASK(x, mask)  (((x) + (mask)) & ~(mask))
#define ALIGN(x, a)            _ALIGN1((x), (a))
#define ALIGN_DOWN(x, a)       _ALIGN1((x) - ((a) - 1), (a))
#define PTR_ALIGN(p, a)        ((__typeof__(p))ALIGN((unsigned long)(p), (a)))
#define PTR_ALIGN_DOWN(p, a)   ((__typeof__(p))ALIGN_DOWN((unsigned long)(p), (a)))
#define IS_ALIGNED(x, a)       (((x) & ((__typeof__(x))(a) - 1)) == 0)

static inline __attribute__((__warn_unused_result__))
bool __must_check_overflow(bool overflow) {
  return UNLIKELY(overflow);
}

// a + b => d
#define check_add_overflow(a, b, d) __must_check_overflow(({  \
  __typeof__(a) __a = (a);      \
  __typeof__(b) __b = (b);      \
  __typeof__(d) __d = (d);      \
  (void) (&__a == &__b);      \
  (void) (&__a == __d);     \
  __builtin_add_overflow(__a, __b, __d);  \
}))

// a - b => d
#define check_sub_overflow(a, b, d) __must_check_overflow(({  \
  __typeof__(a) __a = (a);      \
  __typeof__(b) __b = (b);      \
  __typeof__(d) __d = (d);      \
  (void) (&__a == &__b);      \
  (void) (&__a == __d);     \
  __builtin_sub_overflow(__a, __b, __d);  \
}))

// a * b => d
#define check_mul_overflow(a, b, d) __must_check_overflow(({  \
  __typeof__(a) __a = (a);      \
  __typeof__(b) __b = (b);      \
  __typeof__(d) __d = (d);      \
  (void) (&__a == &__b);      \
  (void) (&__a == __d);     \
  __builtin_mul_overflow(__a, __b, __d);  \
}))

/*
 * Compute a*b+c, returning USIZE_MAX on overflow. Internal helper for
 * struct_size() below.
 */
static inline __attribute__((__warn_unused_result__))
usize __ab_c_size(usize a, usize b, usize c) {
  usize bytes;
  if (check_mul_overflow(a, b, &bytes))
    return USIZE_MAX;
  if (check_add_overflow(bytes, c, &bytes))
    return USIZE_MAX;
  return bytes;
}

// struct_size calculates size of structure with trailing array, checking for overflow.
// p      Pointer to the structure
// member Name of the array member
// count  Number of elements in the array
//
// Calculates size of memory needed for structure p followed by an array of count number
// of member elements.
// Returns number of bytes needed or USIZE_MAX on overflow.
#define struct_size(p, member, count)                    \
  __ab_c_size(count,                                     \
    sizeof(*(p)->member) + __must_be_array((p)->member), \
    sizeof(*(p)))

// array_size calculates size of 2-dimensional array (i.e. a * b)
// Returns number of bytes needed to represent the array or USIZE_MAX on overflow.
static inline __attribute__((__warn_unused_result__))
usize array_size(usize a, usize b) {
  usize bytes;
  if (check_mul_overflow(a, b, &bytes))
    return SIZE_MAX;
  return bytes;
}

// BUILD_BUG_ON_ZERO is a neat trick used in the Linux kernel source to force a
// compilation error if condition is true, but also produce a result
// (of value 0 and type int), so the expression can be used e.g. in a structure
// initializer (or where-ever else comma expressions aren't permitted).
#define BUILD_BUG_ON_ZERO(e) ((int)(sizeof(struct { int:(-!!(e)); })))

// ARRAY_LEN: number of elements of an array
#define __must_be_array(a) BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

// copy_{from,to}_user: memcpy for copying from/to user memory.
// return true on success;
inline static bool copy_from_user(void *to, const void *from, usize n) {
  memcpy(to, from, n);
  return true;
}
inline static bool copy_to_user(void *to, const void *from, usize n) {
  memcpy(to, from, n);
  return true;
}

// fls finds the last (most-significant) bit set
#define fls(x) (x ? sizeof(x) * 8 - __builtin_clz(x) : 0)

// ilog2 calculates the log of base 2
#define ilog2(n) (             \
  __builtin_constant_p(n) ?    \
    ((n) < 2 ? 0 :             \
    63 - __builtin_clzll(n)) : \
  fls(n)                       \
)

// ceil_pow2 rounds up n to nearest power of two. Result is undefined when n is 0.
#define ceil_pow2(n) (              \
  __builtin_constant_p(n) ? (       \
    ((n) == 1) ? 1 :                \
      (1UL << (ilog2((n) - 1) + 1)) \
    ) :                             \
    (1UL << fls(n - 1))             \
)
