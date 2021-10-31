// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICEnsE-2.0

#include "table.h"

table_t* table_new(const char* name, int nrows, int ncols) {
  // if this was "real" code we'd check for allocation-size overflow
  usize name_len = strlen(name);
  usize cols_offs = sizeof(table_t) + (sizeof(str_t) * nrows * ncols);
  usize name_offs = cols_offs + (sizeof(column_t) * ncols);
  table_t* t = calloc(1, name_offs + name_len + 1);
  t->nrows = nrows;
  t->ncols = ncols;
  t->columns = (column_t*)&((u8*)t)[cols_offs];
  t->name = &((char*)t)[name_offs];
  memcpy((void*)t->name, name, name_len);
  return t;
}


void table_free(table_t* t) {
  for (int i = 0; i < t->ncols; i++)
    str_dispose(&t->columns[i].title);
  for (int i = 0; i < t->nrows*t->ncols; i++)
    str_dispose(&t->cells[i]);
  free(t);
}


void table_fprint(table_t* t, FILE* f) {
  // head
  for (int col = 0; col < t->ncols; col++) {
    column_t* c = &t->columns[col];
    const char* fmt = (
      c->align > 0 ?
        col ? " | %*s" : "%*s" :
        col ? " | %-*s" : "%-*s"
    );
    fprintf(f, fmt, c->width, c->title.p);
  }
  fputc('\n', f);
  for (int col = 0; col < t->ncols; col++) {
    static char dash[80] = {0};
    if (!dash[0])
      memset(dash, '-', sizeof(dash));
    int width = MAX(table_colw(t, col), t->columns[col].title.len);
    width = MIN((int)sizeof(dash), width + (col ? 2 : 1));
    fprintf(f, col ? "|%.*s" : "%.*s", width, dash);
  }
  fputc('\n', f);

  // body
  for (int row = 0; row < t->nrows; row++) {
    for (int col = 0; col < t->ncols; col++) {
      str_t* cell = table_cell(t, row, col);
      const char* fmt = (
        table_align(t, col) > 0 ?
          col ? " | %*.*s" : "%*.*s" :
          col ? " | %-*.*s" : "%-*.*s"
      );
      int width = MAX(table_colw(t, col), t->columns[col].title.len);
      fprintf(f, fmt, width, cell->len, cell->p);
    }
    fputc('\n', f);
  }
}
