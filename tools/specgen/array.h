// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

// simple dynamic array

#pragma once
#include "../common.h"

typedef struct _array {
  void* p;
  usize len; // number of used elements at p
  usize cap; // number of elements that p can hold
} array_t;

// array_dispose frees memory at array_t.p if non-null. Sets all members to zero.
void array_dispose(array_t*);

// array_append copies elemsize*count bytes from src to the end of the array.
// Returns false if malloc failed while growing the array.
bool array_append(array_t*, const void* src, usize elemsize, usize count);

// array_truncate sets the length to zero
inline static void array_truncate(array_t* a) { a->len = 0; }

// array_grow increases capacity by at least elemsize*count bytes.
// Returns false if malloc failed.
bool array_grow(array_t*, usize elemsize, usize count);

// array_makeroom ensures there's elemsize*count bytes available, growing if needed.
// Returns false if malloc failed.
inline static bool array_makeroom(array_t* a, usize elemsize, usize count) {
  if (a->cap - a->len < count)
    return array_grow(a, elemsize, count);
  return true;
}

// array_at accesses the i th element of type elemtype
#define array_at(array_t_ptr, elemtype, i) \
  ((elemtype*)&((char*)(a)->p)[sizeof(elemtype) * i])

// DEF_ARRAY_TYPE defines a new array type with a type-specific interface to
// array_* functions and allows direct indexing on to its p members.
// e.g. DEF_ARRAY_TYPE(int_array, int)
#define DEF_ARRAY_TYPE(NAME, ELEMT)                              \
  typedef struct _##NAME { ELEMT* p; usize len, cap; } NAME##_t; \
  DEF_ARRAY_IMPL(NAME##_t, NAME, ELEMT)

// static void NAME##_dispose(STRUCTTYPE*);
// static bool NAME##_append(STRUCTTYPE*, const ELEMT* src, usize count);
// static void NAME##_truncate(STRUCTTYPE*);
// static ELEMT* NAME##_at(STRUCTTYPE*, usize i);
// static bool NAME##_makeroom(STRUCTTYPE*, usize count);
#define DEF_ARRAY_IMPL(STRUCTTYPE, NAME, ELEMT)                                      \
  static_assert( sizeof(STRUCTTYPE) >= sizeof(array_t), "");                         \
  static_assert( offsetof(STRUCTTYPE,p) == offsetof(array_t,p), "");                 \
  static_assert( offsetof(STRUCTTYPE,len) == offsetof(array_t,len), "");             \
  static_assert( offsetof(STRUCTTYPE,cap) == offsetof(array_t,cap), "");             \
  inline static void NAME##_dispose(STRUCTTYPE* a) { array_dispose((array_t*)a); }   \
  inline static bool NAME##_append(STRUCTTYPE* a, const ELEMT* src, usize count) {   \
    return array_append((array_t*)a, src, sizeof(ELEMT), count); }                   \
  inline static void NAME##_truncate(STRUCTTYPE* a) { array_truncate((array_t*)a); } \
  inline static ELEMT* NAME##_at(STRUCTTYPE* a, usize i) { return &a->p[i]; }        \
  inline static bool NAME##_makeroom(STRUCTTYPE* a, usize count) {                   \
    return array_makeroom((array_t*)a, sizeof(ELEMT), count); }                      \


DEF_ARRAY_TYPE(int_array, int)

