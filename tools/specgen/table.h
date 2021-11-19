// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "../common.h"
#include "str.h"

typedef enum {
  table_align_start  = -1,
  table_align_center = 0,
  table_align_end    = 1,
} table_align_t;

typedef struct _column {
  str_t         title;
  int           width; // max row width
  table_align_t align;
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
static int table_colw(table_t* t, int col);
static table_align_t table_align(table_t* t, int col);
static void table_set_align(table_t* t, int col, table_align_t);
void table_fprint(table_t* t, FILE* f);


// inline implementations

inline static str_t* table_cell(table_t* t, int row, int col) {
  return &t->cells[(row * t->ncols) + col];
}

inline static int table_colw(table_t* t, int col) {
  return t->columns[col].width;
}

inline static table_align_t table_align(table_t* t, int col) {
  return t->columns[col].align;
}

inline static void table_set_align(table_t* t, int col, table_align_t align) {
  t->columns[col].align = align;
}
