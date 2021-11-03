// Port of go/src/text/tabwriter/tabwriter.go (October 2021 for playsys)
// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the implementation file elastictab.c
//
// etab--Elastic Tabs--implements a writer that translates tabbed columns in
// input into properly aligned text. It uses the Elastic Tabstops algorithm described at
// http://nickgravgaard.com/elastictabstops/index.html.

#pragma once
#include "../common.h"
#include "array.h"
#include "str.h"

#define ETAB_MAX_COLS 80

// To escape a text segment, bracket it with an ETAB_ESCAPE byte.
// For instance, the tab in this string "Ignore this tab: \xff\t\xff"
// does not terminate a cell and constitutes a single character of
// width one for formatting purposes.
//
// The value 0xff was chosen because it cannot appear in a valid UTF-8 sequence.
//
#define ETAB_ESCAPE '\xff'

typedef enum {
  ETAB_ALIGN_RIGHT = 1,

  // Handle empty columns as if they were not present in the input in the first place
  ETAB_SKIP_EMPTY_COL = 2,

  // Strip Escape characters bracketing escaped text segments
  // instead of passing them through unchanged with the text.
  ETAB_STRIP_ESCAPE = 4,

  ETAB_DEBUG = 4096, // insert "|"
} etab_flag_t;

typedef struct _etab_cell {
  int  size;  // cell size in bytes
  int  width; // cell width in runes
  bool htab;  // true if the cell is terminated by an htab ('\t')
} etab_cell_t;

typedef struct _etab_line {
  etab_cell_t cells[ETAB_MAX_COLS];
  int         len; // number of cells
} etab_line_t;

typedef struct _etab_line_array {
  etab_line_t* p;
  usize len; // number of used elements at p
  usize cap; // number of elements that p can hold
} etab_line_array_t;
static void etab_line_array_dispose(etab_line_array_t*);
static bool etab_line_array_append(etab_line_array_t*, const etab_line_t* src, usize count);
static void etab_line_array_truncate(etab_line_array_t*);
static etab_line_t* etab_line_array_at(etab_line_array_t*, usize i);
static bool etab_line_array_makeroom(etab_line_array_t*, usize count);
DEF_ARRAY_IMPL(etab_line_array_t, etab_line_array, etab_line_t)

typedef struct _etab {
  str_t*      output;
  int         minwidth;
  int         tabwidth;
  int         padding;
  char        padbytes[8];
  etab_flag_t flags;

  // current state
  str_t buf; // collected text excluding tabs or line breaks
  int   pos; // buffer position up to which cell.width of incomplete cell has been computed
  etab_cell_t cell;
    // current incomplete cell; cell.width is up to buf[pos] excluding ignored sections
  char endChar;
    // terminating char of escaped sequence
    // (Escape for escapes, '>', ';' for HTML tags/entities, or 0)

  // list of lines; each line is a list of cells
  etab_line_array_t lines;

  // etab_cell_t linev[4][ETAB_MAX_COLS];
  // int         linec;        // number of lines at linesv
  // int         linecellc[4]; // number of cells at a line

  // list of column widths
  int widthv[ETAB_MAX_COLS];
  int widthc;

  // per-column alignment reversal.
  // true causes the alignment of the column to be reversed, i.e. left aligned if
  // ETAB_ALIGN_RIGHT is set, else right aligned.
  bool ralign[ETAB_MAX_COLS];
} etab_t;


// etab_init initializes a tab writer for use.
// The etab_t stucture is expected to be zero-initialized on first use.
//
//  out       output buffer
//  minwidth  minimal cell width including any padding
//  tabwidth  width of tab characters (equivalent number of spaces)
//  padding   padding added to a cell before computing its width
//  padchar   ASCII char used for padding
//            if padchar == '\t', the Writer will assume that the width of a '\t' in
//            the formatted output is tabwidth, and cells are left-aligned independent
//            of ETAB_ALIGN_RIGHT (for correct-looking results, tabwidth must
//            correspond to the tab width in the viewer displaying the result).
//  flags     formatting control
bool etab_init(etab_t*, str_t* out,
  int minwidth, int tabwidth, int padding, char padchar, etab_flag_t);

// etab_dispose frees internal memory
void etab_dispose(etab_t*);

// etab_write appends some text. It's like a write() call.
bool etab_write(etab_t*, const char* s, int len);

// etab_flush writes any outstanding buffered data to etab_t.output
void etab_flush(etab_t*);

// etab_writestr is a convenience around etab_write that calls strlen for you
inline static bool etab_writestr(etab_t* e, const char* cstr) {
  return etab_write(e, cstr, strlen(cstr));
}

// etab_dump writes the internal state to stderr
void etab_dump(etab_t*);
