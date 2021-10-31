// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICEnsE-2.0

#pragma once
#include "../common.h"
#include "str.h"

typedef struct _column {
  str_t title;
  int   width; // max row width
  int   align; // -1 start, 0 center, 1 end
} column_t;


typedef struct _table {
  struct _table* next; // list link
  const char*    name;
  int            nrows, ncols;
  column_t*      columns; // array of ncols length
  str_t          cells[/*nrows*ncols*/]; // row0 col0, row0 col1, row0 col2, row1 col0 ...
} table_t;


     table_t* table_new(const char* name, int nrows, int ncols);
         void table_free(table_t* t);
static str_t* table_cell(table_t* t, int row, int col);
static    int table_colw(table_t* t, int col);
static    int table_align(table_t* t, int col); // -1 start, 0 center, 1 end
         void table_fprint(table_t* t, FILE* f);


// inline implementations

inline static str_t* table_cell(table_t* t, int row, int col) {
  return &t->cells[(row * t->ncols) + col];
}

inline static int table_colw(table_t* t, int col) {
  return t->columns[col].width;
}

inline static int table_align(table_t* t, int col) { // -1 start, 0 center, 1 end
  return t->columns[col].align;
}
