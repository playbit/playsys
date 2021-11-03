// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

#include "array.h"


void array_dispose(array_t* a) {
  if (a->p) {
    free(a->p);
    a->p = NULL;
  }
  a->len = 0;
  a->cap = 0;
}


bool array_grow(array_t* a, usize elemsize, usize count) {
  usize z = (elemsize * a->cap) + (elemsize * count * 2);
  // TODO: check for overflow
  void* ptr = realloc(a->p, z);
  if (!ptr)
    return false;
  a->cap += count * 2;
  a->p = ptr;
  return true;
}


bool array_append(array_t* a, const void* src, usize elemsize, usize count) {
  if (!array_makeroom(a, elemsize, count))
    return false;
  void* dst = &((char*)a->p)[a->len * elemsize];
  memcpy(dst, src, elemsize * count);
  a->len += count;
  return true;
}
