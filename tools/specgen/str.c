// SPDX-License-Identifier: Apache-2.0

#include "str.h"


void str_dispose(str_t* s) {
  if (s->p) {
    free(s->p);
    s->p = NULL;
  }
  s->len = 0;
  s->cap = 0;
}


bool str_grow(str_t* s, int len) {
  s->cap = palign2(s->cap + len*2, sizeof(void*));
  void* ptr = realloc(s->p, s->cap);
  if (!ptr)
    return false;
  s->p = ptr;
  return true;
}


bool str_append(str_t* s, const char* src, int len) {
  if (!str_makeroom(s, len))
    return false;
  memcpy(&s->p[s->len], src, len);
  s->len += len;
  s->p[s->len] = 0;
  return true;
}


bool str_appendc(str_t* s, char c) {
  if (!str_makeroom(s, 1))
    return false;
  s->p[s->len++] = c;
  s->p[s->len] = 0;
  return true;
}


bool str_fmtv(str_t* s, const char* fmt, va_list ap) {
  // start by making an educated guess for space needed: 2x that of the format:
  int len = (int)MIN(0x7fffffff, (strlen(fmt) * 2) + 1);
  va_list ap2;
  while (1) {
    if (!str_makeroom(s, len))
      return false;
    va_copy(ap2,ap); // copy va_list as we might read it twice
    int n = vsnprintf(&s->p[s->len], len, fmt, ap2);
    va_end(ap2);
    if (n < len) {
      // ok; result fit in buf.
      // Theoretically vsnprintf might return -1 on error, but AFAIK no implementation does
      // unless len > INT_MAX, so we are likely fine with ignoring that case here.
      len = n;
      break;
    }
    // vsnprintf tells us how much space it needs
    len = n + 1;
  }
  // update len (vsnprintf wrote terminating \0 already)
  s->len += len;
  return true;
}


bool str_fmt(str_t* s, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  bool ok = str_fmtv(s, fmt, ap);
  va_end(ap);
  return ok;
}


void str_rtrim(str_t* s, const char* trimset) {
  for (int i = s->len - 1; i >= 0; i--) {
    if (strchr(trimset, s->p[i]) == NULL) {
      s->len = i + 1;
      return;
    }
  }
  s->len = 0;
}
