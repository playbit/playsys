// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

// simple dynamic string functions

#pragma once
#include "../common.h"

typedef struct _str {
  char* p; // NULL if empty, else a null-terminated "C string" of len length
  int   len;
  int   cap;
} str_t;

// functions that return bool returns false if malloc failed

static void str_dispose(str_t* s); // free memory at s->p if needed
       bool str_grow(str_t* s, int len); // grow by at least len
static bool str_makeroom(str_t* s, int len); // grow by at least len if needed
       bool str_append(str_t* s, const char* src, int len); // copy src to end of s
       bool str_appendc(str_t* s, char c); // append char c to end of s
static bool str_appendcstr(str_t* s, const char* cstr);
       bool str_fmtv(str_t* s, const char* fmt, va_list ap); // append fmt with arguments
       bool str_fmt(str_t* s, const char* fmt, ...) ATTR_FORMAT(printf, 2, 3);


// implementations of trivial functions

inline static void str_dispose(str_t* s) {
  if (s->p)
    free(s->p);
}

inline static bool str_makeroom(str_t* s, int len) {
  int avail = s->cap - s->len - 1;
  if (avail < len)
    return str_grow(s, len);
  return true;
}

inline static bool str_appendcstr(str_t* s, const char* cstr) {
  return str_append(s, cstr, strlen(cstr));
}
