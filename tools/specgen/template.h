// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

#pragma once
#include "../common.h"
#include "str.h"

typedef struct _tplvar {
  const char* name;
  int         namelen;
  str_t       value;
} tplvar_t;

bool template_sub(
  FILE* outf,
  const char* src, usize srclen, const char* srcfile,
  const tplvar_t* varv, int varc);
