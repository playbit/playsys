// Port of go/src/text/tabwriter/tabwriter.go (October 2021 for playsys)
//
// Copyright (c) 2009 The Go Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "elastictab.h"
#include <err.h>


static const char tabs[8] = "\t\t\t\t\t\t\t\t";
static const char* hbar = "---\n";

static_assert(sizeof(tabs) == sizeof(((etab_t*)0)->padbytes), "");


static void etab_add_line(etab_t* e) {
  if (etab_line_array_makeroom(&e->lines, 1)) {
    e->lines.p[e->lines.len].len = 0; // set the next line's cell count to 0
    e->lines.len++;
  }
}


static void etab_reset(etab_t* e) {
  e->buf.len = 0;
  e->pos = 0;
  e->cell = (etab_cell_t){0};
  e->endChar = 0;
  etab_line_array_truncate(&e->lines);
  e->widthc = 0;
  etab_add_line(e);
}


bool etab_init(
  etab_t* e, str_t* output,
  int minwidth, int tabwidth, int padding, char padchar,
  etab_flag_t flags)
{
  if (minwidth < 0 || tabwidth < 0 || padding < 0) {
    warnx("etab_init: negative minwidth, tabwidth, or padding");
    return false;
  }
  e->output = output;
  e->minwidth = minwidth;
  e->tabwidth = tabwidth;
  e->padding = padding;
  memset(e->padbytes, padchar, sizeof(e->padbytes));
  if (padchar == '\t') {
    // tab padding enforces left-alignment
    flags &= ~ETAB_ALIGN_RIGHT;
  }
  e->flags = flags;
  etab_reset(e);
  return true;
}


void etab_dispose(etab_t* e) {
  str_dispose(&e->buf);
}


void etab_dump(etab_t* e) {
  int pos = 0;
  for (int i = 0; i < (int)e->lines.len; i++) {
    etab_line_t* line = &e->lines.p[i]; // etab_cell_t[ETAB_MAX_COLS]
    fprintf(stderr, "(%d) ", i);
    for (int ci = 0; ci < line->len; ci++) {
      etab_cell_t c = line->cells[ci];
      fprintf(stderr, "[%.*s]", c.size, &e->buf.p[pos]);
      pos += c.size;
    }
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n");
}


static bool etab_cell_append(etab_t* e, const char* buf, int buflen) {
  e->cell.size += buflen;
  return str_append(&e->buf, buf, buflen);
}


static void etab_update_width(etab_t* e) {
  // e->cell.width += utf8.RuneCount(e->buf[e->pos:]) // TODO
  e->cell.width += e->buf.len - e->pos;
  e->pos = e->buf.len;
}


// Terminate the current cell by adding it to the list of cells of the
// current line. Returns the number of cells in that line.
static int etab_terminate_cell(etab_t* e, bool htab) {
  e->cell.htab = htab;
  int lineidx = e->lines.len - 1;
  etab_line_t* line = &e->lines.p[lineidx];
  int cellidx = line->len++;
  if (cellidx == ETAB_MAX_COLS) {
    // no more space; replace last column
    cellidx--;
    line->len--;
  }
  line->cells[cellidx] = e->cell;
  e->cell = (etab_cell_t){0};
  return cellidx;
}


static bool etab_write0(etab_t* e, const char* buf, int buflen) {
  return str_append(e->output, buf, buflen);
}


static bool etab_writen(etab_t* e, const char* src, int srclen, int n) {
  str_makeroom(e->output, n);
  while (n > srclen && etab_write0(e, src, srclen)) {
    n -= srclen;
  }
  return etab_write0(e, src, n);
}


static bool etab_write_padding(etab_t* e, int textw, int cellw) {
  if (e->padbytes[0] == '\t') {
    // padding is done with tabs
    if (e->tabwidth == 0)
      return true; // tabs have no width - can't do any padding
    // make cellw the smallest multiple of e->tabwidth
    cellw = (cellw + e->tabwidth - 1) / e->tabwidth * e->tabwidth;
    int n = cellw - textw; // amount of padding
    assert(n >= 0);
    return etab_writen(e, tabs, strlen(tabs), (n + e->tabwidth - 1) / e->tabwidth);
  }

  // padding is done with non-tab characters
  return etab_writen(e, e->padbytes, sizeof(e->padbytes), cellw - textw);
}


static void etab_start_escape(etab_t* e) {
  e->endChar = ETAB_ESCAPE;
}


// Terminate escaped mode. If the escaped text was an HTML tag, its width
// is assumed to be zero for formatting purposes; if it was an HTML entity,
// its width is assumed to be one. In all other cases, the width is the
// unicode width of the text.
static void etab_end_escape(etab_t* e) {
  etab_update_width(e);
  if ((e->flags & ETAB_STRIP_ESCAPE) == 0) {
    e->cell.width -= 2; // don't count the ETAB_ESCAPE chars
  }
  e->pos = e->buf.len;
  e->endChar = 0;
}


bool etab_write(etab_t* e, const char* buf, int buflen) {
  // split text into cells
  int n = 0;
  for (int i = 0; i < buflen; i++) {
    char ch = buf[i];
    if (e->endChar == 0) {
      // outside escape
      switch (ch) {
        case '\t': case '\v': case '\n': case '\f': {
          // end of cell
          if (!etab_cell_append(e, &buf[n], i - n))
            return false;
          etab_update_width(e);
          n = i + 1; // ch consumed
          int ncells = etab_terminate_cell(e, ch == '\t');
          if (ch == '\n' || ch == '\f') {
            // terminate line
            etab_add_line(e);
            if (ch == '\f' || ncells == 1) {
              // A '\f' always forces a flush. Otherwise, if the previous
              // line has only one cell which does not have an impact on
              // the formatting of the following lines (the last cell per
              // line is ignored by format()), thus we can flush the
              // Writer contents.
              etab_flush(e);
              if (ch == '\f' && (e->flags & ETAB_DEBUG)) {
                // indicate section break
                if (!etab_write0(e, hbar, strlen(hbar)))
                  return false;
              }
            }
          }
          break;
        }

        case ETAB_ESCAPE:
          // start of escaped sequence
          if (!etab_cell_append(e, &buf[n], i - n))
            return false;
          etab_update_width(e);
          n = i;
          if (e->flags & ETAB_STRIP_ESCAPE)
            n++; // strip ETAB_ESCAPE
          etab_start_escape(e);
          break;
      }

    } else {
      // inside escape
      if (ch == e->endChar) {
        // end of tag/entity
        int j = i + 1;
        if (ch == ETAB_ESCAPE && e->flags & ETAB_STRIP_ESCAPE)
          j = i; // strip ETAB_ESCAPE
        if (!etab_cell_append(e, &buf[n], j - n))
          return false;
        n = i + 1; // ch consumed
        etab_end_escape(e);
      }
    }
  }

  // append leftover text
  return etab_cell_append(e, &buf[n], buflen - n);
}


static int etab_write_lines(etab_t* e, int pos0, int line0, int line1) {
  int pos = pos0;
  int align_prev = -2;
  for (int i = line0; i < line1; i++) {
    etab_line_t* line = &e->lines.p[i];

    for (int j = 0; j < line->len; j++) {
      etab_cell_t c = line->cells[j];
      if (j > 0 && e->flags & ETAB_DEBUG)
        str_appendc(e->output, '|'); // indicate column break

      int align_left = ((e->flags & ETAB_ALIGN_RIGHT) == 0) - e->ralign[j];

      if (c.size == 0) {
        // empty cell
        if (j < e->widthc)
          etab_write_padding(e, c.width, e->widthv[j]);
      } else {
        // non-empty cell
        int align_diff = align_prev < -1 ? 0 : abs(align_prev - align_left); // 1 if diff
        if (align_left) {
          if (align_diff * (int)e->ralign[j])
            str_appendc(e->output, e->padbytes[0]);
          etab_write0(e, &e->buf.p[pos], c.size);
          pos += c.size;
          if (j < e->widthc)
            etab_write_padding(e, c.width, e->widthv[j]);
        } else { // align right
          if (j < e->widthc)
            etab_write_padding(e, c.width, e->widthv[j] - align_diff);
          etab_write0(e, &e->buf.p[pos], c.size);
          pos += c.size;
          // if (j < e->widthc)
          // TODO this condition is not right when last col is e->ralign[j]
          str_append(e->output, e->padbytes, align_diff);
        }
      }
      align_prev = align_left;
    }

    if (((usize)i+1 == e->lines.len)) {
      // last buffered line - we don't have a newline, so just write
      // any outstanding buffered data
      etab_write0(e, &e->buf.p[pos], e->cell.size);
      pos += e->cell.size;
    } else {
      // not the last line - write newline
      str_appendc(e->output, '\n');
    }
  }
  return pos;
}


// Format the text between line0 and line1 (excluding line1); pos is the buffer position
// corresponding to the beginning of line0.
// Returns the buffer position corresponding to the beginning of line1
static int etab_format(etab_t* e, int pos0, int line0, int line1) {
  int pos = pos0;
  int column = e->widthc;
  for (int this = line0; this < line1; this++) {
    etab_line_t* line = &e->lines.p[this];

    if (column >= line->len - 1)
      continue;

    // cell exists in this column => this line has more cells than the previous line
    // (the last cell per line is ignored because cells are tab-terminated; the last
    // cell per line describes the text before the newline/formfeed and does not belong
    // to a column)

    // print unprinted lines until beginning of block
    pos = etab_write_lines(e, pos, line0, this);
    line0 = this;

    // column block begin
    int width = e->minwidth; // minimal column width
    bool discardable = true; // true if all cells in this column are empty and "soft"
    for (; this < line1; this++) {
      line = &e->lines.p[this];
      if (column >= line->len - 1)
        break;
      // cell exists in this column
      etab_cell_t c = line->cells[column];
      // update width
      int w = c.width + e->padding;
      if (w > width)
        width = w;
      // update discardable
      if (c.width > 0 || c.htab)
        discardable = false;
    }
    // column block end

    // discard empty columns if necessary
    if (discardable && e->flags & ETAB_SKIP_EMPTY_COL)
      width = 0;

    // format and print all columns to the right of this column
    // (we know the widths of this column and all columns to the left)
    assert(e->widthc < (int)countof(e->widthv));
    e->widthv[e->widthc++] = width; // push width
    pos = etab_format(e, pos, line0, this);
    e->widthc--; // pop width
    line0 = this;
  }

  // print unprinted lines until end
  return etab_write_lines(e, pos, line0, line1);
}


void etab_flush(etab_t* e) {
  // add current cell if not empty
  if (e->cell.size > 0) {
    if (e->endChar) {
      // inside escape - terminate it even if incomplete
      etab_end_escape(e);
    }
    etab_terminate_cell(e, false);
  }

  // format contents of buffer
  etab_format(e, 0, 0, e->lines.len);
  etab_reset(e);
}

// static void etab_example() {
//   etab_t etab = {0};
//   str_t out = {0};
//
//   // etab_init: e, minwidth, tabwidth, padding, padchar, flags
//   etab_init(&etab, &out, 5, 8, 1, ' ', 0);
//   etab_writestr(&etab, "a\tb\tc\td\t.\n");
//   etab_writestr(&etab, "123\t12345\t1234567\t123456789\t.\n");
//   etab_writestr(&etab, "foo\t\tbar\tcat\t.\n");
//   etab_flush(&etab);
//   printf("etab:\n%.*s\n", out.len, out.p);
//
//   out.len = 0;
//   etab_init(&etab, &out, 5, 8, 1, ' ', ETAB_ALIGN_RIGHT);
//   etab.ralign[2] = true;
//   etab_writestr(&etab, "a\tb\tc\td\t.\n");
//   etab_writestr(&etab, "123\t12345\t1234567\t123456789\t.\n");
//   etab_writestr(&etab, "foo\t\tbar\tcat\t.\n");
//   etab_flush(&etab);
//   printf("\n%.*s\n", out.len, out.p);
//
//   out.len = 0;
//   etab_init(&etab, &out, 5, 8, 1, ' ', 0);
//   etab.ralign[2] = true;
//   etab_writestr(&etab, "a\tb\tc\td\t.\n");
//   etab_writestr(&etab, "123\t12345\t1234567\t123456789\t.\n");
//   etab_writestr(&etab, "foo\t\tbar\tcat\t.\n");
//   etab_writestr(&etab, "really long line\t\tbar\tcat\t.\n");
//   etab_flush(&etab);
//   printf("\n%.*s\n", out.len, out.p);
//
//   etab_dispose(&etab);
//   str_dispose(&out);
// }
