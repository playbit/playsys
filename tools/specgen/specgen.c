// Copyright 2021 The PlaySys Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See http://www.apache.org/licenses/LICENSE-2.0

// Note: when improving on this program it can be useful to run it as you edit:
// autorun tools/specgen/*.* spec.md include/*.in -- \
//   "tools/build.sh && lldb -o run tools/specgen/specgen" */

#include "../common.h"
#include "putil.h"
#include "md4c.h"
#include "str.h"
#include "template.h"
#include "table.h"
#include "elastictab.h"
#include <err.h>

#define PSYS_SYSCALL_MAXARGS 5

static_assert(sizeof(MD_SIZE) == sizeof(u32), "");
static_assert(sizeof(MD_CHAR) == sizeof(char), "");

static char tmpmem[4096*32];
static const void* mend = tmpmem + sizeof(tmpmem);
static bool quiet = false; // -quiet CLI flag
static const char* progname; // argv[0]


typedef struct _doc {
  table_t* tables;      // linked list of tables (first table)
  table_t* tables_tail; // linked list of tables (last table)
} doc_t;


typedef struct _mdp {
  doc_t*   doc;        // output document
  char     id[128];    // current "spec:ID" c-string
  table_t* table;      // current spec table being parsed
  int      trow, tcol; // current table row and column index
  int      nthrows;    // current table's number of head rows
} mdp_t;


static FILE* xfopen(const char* filename, const char* mode) {
  FILE* outf = fopen(filename, mode);
  if (!outf)
    err(1, "%s", filename);
  return outf;
}


// static void doc_dispose(doc_t* d) {
//   for (table_t* t = d->tables; t; ) {
//     table_t* t1 = t;
//     t = t->next;
//     table_free(t1);
//   }
// }


static table_t* doc_find_table(doc_t* doc, const char* table_name) {
  for (table_t* t = doc->tables; t; t = t->next) {
    if (strcmp(t->name, table_name) == 0)
      return t;
  }
  return NULL;
}


static int table_begin(mdp_t* p, int nrows_head, int nrows_body, int ncols) {
  // plog("begin_spec_table \"%s\"", p->id);
  table_t* t = table_new(p->id, nrows_body, ncols);

  if (p->doc->tables) {
    p->doc->tables_tail->next = t;
  } else {
    p->doc->tables = t;
  }
  p->doc->tables_tail = t;

  p->table = t;
  p->nthrows = nrows_head;
  p->trow = -1;
  p->tcol = 0;

  return 0;
}

static int table_end(mdp_t* p) {
  // plog("end_spec_table \"%s\"", p->id);
  return 0;
}


static int md_on_enter_block(MD_BLOCKTYPE type, void* detail, void* uptr) {
  mdp_t* p = (mdp_t*)uptr;
  // tables: TABLE THEAD (TR TH+)* TBODY (TR TD+)*
  switch (type) {
    case MD_BLOCK_TABLE: {
      MD_BLOCK_TABLE_DETAIL* d = detail;
      if (p->id[0] && d->head_row_count > 0 && !p->table)
        return table_begin(p, d->head_row_count, d->body_row_count, d->col_count);
      break;
    }
    // case MD_BLOCK_THEAD: plog("BLOCK_THEAD"); break;
    // case MD_BLOCK_TBODY: plog("BLOCK_TBODY"); break;
    case MD_BLOCK_TR:
      if (p->table) {
        p->tcol = -1;
        p->trow = MIN((int)p->table->nrows, p->trow + 1);
      }
      break;
    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
      if (p->table) {
        p->tcol = MIN((int)p->table->ncols, p->tcol + 1);
        // plog("row %d, col %d", p->trow, p->tcol);
        if (p->trow == 0) {
          MD_BLOCK_TD_DETAIL* d = detail;
          p->table->columns[p->tcol].align = (
            d->align == MD_ALIGN_CENTER ? 0 :
            d->align == MD_ALIGN_RIGHT ? 1 :
            -1
          );
        }
      }
      break; // MD_BLOCK_TD_DETAIL
    default:
      break;
  }
  return 0;
}


static int md_on_leave_block(MD_BLOCKTYPE type, void* detail, void* uptr) {
  mdp_t* p = (mdp_t*)uptr;
  int r = 0;
  switch (type) {
    case MD_BLOCK_TABLE:
      if (p->table) {
        p->table = NULL;
        r = table_end(p);
        p->id[0] = 0;
      }
      break;
    default:
      break;
  }
  return r;
}


static int md_on_enter_span(MD_SPANTYPE type, void* detail, void* uptr) {
  mdp_t* p = (mdp_t*)uptr;
  switch (type) {

  case MD_SPAN_A: {
    MD_SPAN_A_DETAIL* d = detail;
    u32 len = d->title.size;
    if (len > 1 && d->title.text[0] == ':') {
      len--;
      const char* pch = d->title.text + 1;
      if (len >= sizeof(p->id)) {
        perrlog("spec id \"%.*s\" too long", (int)len, pch);
        return 1;
      }
      memcpy(p->id, pch, len);
      p->id[len] = 0;
      // plog("marker \"%s\"", p->id);
    }
    break;
  }

  default:
    break;
  }
  return 0;
}


static int md_on_leave_span(MD_SPANTYPE type, void* detail, void* uptr) {
  return 0;
}


static int md_on_text(MD_TEXTTYPE type, const char* text, u32 size, void* uptr) {
  mdp_t* p = (mdp_t*)uptr;
  if (p->table) {
    if (p->trow == 0) {
      column_t* c = &p->table->columns[p->tcol];
      str_append(&c->title, text, size);
      // c->width = MAX(c->width, c->title.len);
    } else if (p->trow >= p->nthrows) {
      str_t* c = table_cell(p->table, p->trow - p->nthrows, p->tcol);
      str_append(c, text, size);
      p->table->columns[p->tcol].width = MAX(p->table->columns[p->tcol].width, c->len);
    }
  }
  return 0;
}


static void md_on_debug_log(const char* msg, void* uptr) {
  fprintf(stderr, "[md4c] %s\n", msg);
}


static bool parse_md(const char* input, u32 inputlen, doc_t* doc) {
  mdp_t p = {
    .doc = doc,
  };
  MD_PARSER parser = {
    .flags = MD_FLAG_TABLES,
    .enter_block = md_on_enter_block,
    .leave_block = md_on_leave_block,
    .enter_span  = md_on_enter_span,
    .leave_span  = md_on_leave_span,
    .text        = md_on_text,
    .debug_log   = md_on_debug_log,
  };
  int r = md_parse(input, inputlen, &parser, &p);
  // reverse tables
  return r == 0;
}


// ---------------------------------------------------------------------


static table_t* get_table(doc_t* doc, const char* table_name) {
  table_t* t = doc_find_table(doc, table_name);
  if (!t) {
    errx(1, "table \"%s\" not found (is there a '[](# \":%s\")' marker?)",
      table_name, table_name);
  }
  return t;
}


static bool gen_template(
  FILE* outf, const char* tplfile, tplvar_t* varv, int varc, void** mnext)
{
  char* src = preadfile(tplfile, mnext, mend);
  if (!src) {
    warn("%s", tplfile);
    return false;
  }

  usize srclen = (usize)(*mnext - (void*)src);
  bool ok = template_sub(outf, src, srclen, tplfile, varv, varc);

  for (int i = 0; i < varc; i++) {
    str_dispose(&varv[i].value);
    varv[i].value = (str_t){0};
  }

  return ok;
}


static const char* getvar(const tplvar_t* varv, int varc, const char* name, int namelen) {
  for (int i = 0; i < varc; i++) {
    if (namelen == varv[i].namelen && memcmp(name, varv[i].name, namelen) == 0)
      return varv[i].value.p;
  }
  return name;
}


static int parse_args(char* input, char** argv, int argv_cap) {
  char* sep = ", ";
  char* tok;
  char* state;
  int i = 0;
  for (tok = strtok_r(input, sep, &state); tok; tok = strtok_r(NULL, sep, &state)) {
    if (i % 2 == 0) {
      // plog("name '%s'", tok);
      argv[i] = tok;
    } else {
      if (i >= argv_cap)
        return -1;
      // plog("type '%s'", tok);
      argv[i] = tok;
    }
    i++;
  }
  if (i % 2 != 0)
    return -2;
  return i / 2;
}


static void str_append_tr(etab_t* etab, table_t* t, int row, const char* fmt, ...) {
  int i = 0;
  int fmtlen = strlen(fmt);
  int chunk = 0; // start of current chunk
  va_list ap;
  va_start(ap, fmt);
  char buf[32];
  int etab_col = 0;

  for (; i < fmtlen; i++) {
    char c = fmt[i];
    switch (c) {
      case '\\': {
        etab_write(etab, &fmt[chunk], i - chunk);
        i++;
        chunk = i;
        break;
      }
      case '\t':
        etab_col++;
        break;
      case '{': {
        etab_write(etab, &fmt[chunk], i - chunk);
        i++; // skip '{'
        if (i+1 >= fmtlen)
          errx(1, "malformed { in %s", __FUNCTION__);
        char c = fmt[i++]; // save & then skip formatting char
        if (fmt[i] == '>') {
          etab->ralign[etab_col] = true;
          i++;
        }
        if (fmt[i] != '}' || i+1 >= fmtlen)
          errx(1, "malformed { in %s", __FUNCTION__);

        switch (c) {
          case 'r': { // row
            int n = snprintf(buf, sizeof(buf), "%d", row);
            etab_write(etab, buf, n);
            break;
          }
          case 'R': // -row
            if (row == 0) {
              etab_write(etab, "0", 1);
            } else {
              int n = snprintf(buf, sizeof(buf), row ? "-%d" : "%d", row);
              etab_write(etab, buf, n);
            }
            break;
          case 's': { // str arg
            const char* cstr = va_arg(ap, const char*);
            etab_write(etab, cstr, strlen(cstr));
            break;
          }
          case '0' ... '9': {
            int col = c - '0';
            if (col >= MIN(t->ncols, ETAB_MAX_COLS))
              errx(1, "column %d out of bounds in %s", col, __FUNCTION__);
            // etab->ralign[etab_col] = (table_align(t, col) == table_align_end);
            str_t* val = table_cell(t, row, col);
            etab_write(etab, val->p, val->len);
            break;
          }
          default:
            errx(1, "invalid format {%c} in %s", c, __FUNCTION__);
        }
        chunk = i + 1;
        break;
      }
    }
  }
  etab_write(etab, &fmt[chunk], i - chunk);
  va_end(ap);
}


typedef struct _fmt_table_entry {
  const char* var;
  const char* table;
  const char* fmt;
} fmt_table_entry_t;


static int fmt_table_entries(
  doc_t* spec, tplvar_t* vars, int varcap, fmt_table_entry_t* entryv, int entryc)
{
  assert(varcap >= entryc);
  int varc = 0;
  etab_t etab = {0}; // elastic tab writer

  for (int i = 0; i < entryc; i++) {
    vars[varc].name = entryv[i].var;
    vars[varc].namelen = strlen(entryv[i].var);
    str_t* s = &vars[varc++].value;
    table_t* t = get_table(spec, entryv[i].table);
    etab_init(&etab, s, 2, 8, 1, ' ', 0);
    for (int row = 0; row < t->nrows; row++) {
      str_append_tr(&etab, t, row, entryv[i].fmt);
    }
    etab_flush(&etab);
    str_rtrim(s, "\n");
  }

  etab_dispose(&etab);
  return varc;
}


// C language generator
static bool gen_c(FILE* outf, doc_t* spec, const char* tplfile, void** mnext) {
  tplvar_t vars[128] = {0};
  int varc = 0;
  str_t* s;
  table_t* t;

  #define ALLOCVAR(NAME) ({            \
    vars[varc].name = (NAME);          \
    vars[varc].namelen = strlen(NAME); \
    &vars[varc++].value;               \
  })
  #define VARS(STR_T_P) getvar(vars, varc, (STR_T_P)->p, (STR_T_P)->len)
  #define VAR(CSTR) getvar(vars, varc, (CSTR), strlen(CSTR))
  #define TYPE_SUFFIX "_t"
  #define NS "P_"
  #define ns "p_"
  #define sysop_prefix "sysop_"

  const int wrapcol = 90;
  const char* NS2 = "PSYS_";

  str_appendcstr(ALLOCVAR("ns"), ns);
  str_appendcstr(ALLOCVAR("NS"), NS);
  str_appendcstr(ALLOCVAR("NS2"), NS2);
  str_appendcstr(ALLOCVAR("cstr"), "const char*");
  str_appendcstr(ALLOCVAR("ptr"), "const void*");
  str_appendcstr(ALLOCVAR("mutptr"), "void*");

  // add types to set of vars
  t = get_table(spec, "types");
  if (t->ncols > 1) for (int row = 0; row < t->nrows; row++) {
    str_t* name = table_cell(t, row, 0);
    str_fmt(ALLOCVAR(name->p), "%s%s", name->p, TYPE_SUFFIX);
  }

  fmt_table_entry_t entries[] = {
    {"TYPES",      "types",      "typedef {1}\t{0}" TYPE_SUFFIX ";\t// {2}\n"},
    {"CONSTANTS",  "constants",  "#define " NS "{0}\t(({1}" TYPE_SUFFIX "){2})\t// {3}\n"},
    {"ERR_ENUM",   "errors",     "  " ns "err_{0}\t=\t{R>},\t// {1}\n"},
    {"ERR_SWITCH", "errors",     "  case " ns "err_{0}:\treturn \"{0}\";\n"},
    {"OFLAG_ENUM", "open_flags", "  " ns "open_{0}\t=\t{1>},\t// {2}\n"},
    {"SYSOP_ENUM", "sysops",     "  " ns sysop_prefix "{0}\t=\t{1>},\t// {2}\n"},
  };
  varc += fmt_table_entries(
    spec, &vars[varc], countof(vars)-varc, entries, countof(entries));

  s = ALLOCVAR("SYSCALL_FN_PROTOTYPES");
  t = get_table(spec, "sysops");
  int i = 0;
  const char* res_typ = "isize"; //VAR("res");
  char* argv[PSYS_SYSCALL_MAXARGS][2]; // [name, type], [name, type] ...
  if (t->ncols > 2) for (int row = 0; row < t->nrows; row++) {
    str_t* name = table_cell(t, row, 0);
    str_t* args = table_cell(t, row, 2);
    if (args->len == 0 || (args->len > 3 && memcmp(args->p, "TODO", 4) == 0))
      continue;
    // parse args
    char* input = strdup(args->p);
    int argc = parse_args(input, (char**)argv, countof(argv)*2);
    if (argc < 0) {
      if (argc == -2)
        errx(1, "malformed arguments: \"%s\" (missing name or type)", args->p);
      errx(1, "too many args for syscall op %s", name->p);
    }
    // format
    if (i++) str_appendcstr(s, "\n");
    int linestart = s->len;
    str_fmt(s, "static %s %ssyscall_%s(", res_typ, ns, name->p);
    for (int i = 0; i < argc; i++) {
      const char* typ = VAR(argv[i][1]);
      int addlen = (int)(strlen(typ) + strlen(argv[i][0]) + 3);
      if (i) {
        if (s->len - linestart + addlen >= wrapcol) {
          str_appendcstr(s, ",\n  ");
          linestart = s->len - 2;
        } else {
          str_appendcstr(s, ", ");
        }
      }
      str_fmt(s, "%s %s", typ, argv[i][0]);
    }
    str_appendcstr(s, ");");
  }

  s = ALLOCVAR("SYSCALL_FN_IMPLS");
  i = 0;
  if (t->ncols > 2) for (int row = 0; row < t->nrows; row++) {
    str_t* name = table_cell(t, row, 0);
    str_t* args = table_cell(t, row, 2);
    if (args->len == 0 || (args->len > 3 && memcmp(args->p, "TODO", 4) == 0))
      continue;
    // parse args
    char* input = strdup(args->p);
    int argc = parse_args(input, (char**)argv, countof(argv)*2);
    if (argc < 0)
      errx(1, "too many args for syscall op %s", name->p);
    // format
    if (i++) str_appendcstr(s, "\n");
    int linestart = s->len;
    str_fmt(s, "inline static %s %ssyscall_%s(", res_typ, ns, name->p);
    for (int i = 0; i < argc; i++) {
      const char* typ = VAR(argv[i][1]);
      int addlen = (int)(strlen(typ) + strlen(argv[i][0]) + 3);
      if (i) {
        if (s->len - linestart + addlen >= wrapcol) {
          str_appendcstr(s, ",\n  ");
          linestart = s->len - 2;
        } else {
          str_appendcstr(s, ", ");
        }
      }
      str_fmt(s, "%s %s", typ, argv[i][0]);
    }
    str_appendcstr(s, ") {\n");
    linestart = s->len;
    str_fmt(s, "  return _%ssyscall%d(%s%s%s", ns, argc, ns, sysop_prefix, name->p);
    const char* argcast = "(isize)";
    for (int i = 0; i < argc; i++) {
      int addlen = (int)(strlen(argv[i][0]) + 2 + strlen(argcast));
      if (s->len - linestart + addlen >= wrapcol) {
        str_appendcstr(s, ",\n    ");
        linestart = s->len - 4;
      } else {
        str_appendcstr(s, ", ");
      }
      str_appendcstr(s, argcast);
      str_appendcstr(s, argv[i][0]);
    }
    str_appendcstr(s, ");\n");
    str_appendcstr(s, "}");
  }

  #undef ALLOCVAR
  #undef VAR
  #undef TYPE_SUFFIX
  #undef NS
  #undef ns
  #undef sysop_prefix

  // note: gen_template calls str_dispose on all vars' value
  return gen_template(outf, tplfile, vars, countof(vars), mnext);
}


// JS language generator
static bool gen_js(FILE* outf, doc_t* spec, const char* tplfile, void** mnext) {
  fmt_table_entry_t entries[] = {
    {"TYPES",      "types",      "export type {0}_t\t=\t{1}\t// {2}\n"},
    {"CONSTANTS",  "constants",  "export const {0}\t:{1}_t\t=\t{2>}\t// {3}\n"},
    {"ERR_ENUM",   "errors",     "  {0}\t=\t{R>},\t// {1}\n"},
    {"OFLAG_ENUM", "open_flags", "  {0}\t=\t{1>},\t// {2}\n"},
    {"SYSOP_ENUM", "sysops",     "  {0}\t=\t{1>},\t// {2}\n"},
  };
  tplvar_t vars[countof(entries)] = {0};
  int varc = fmt_table_entries(spec, vars, countof(vars), entries, countof(entries));
  // note: gen_template calls str_dispose on all vars' value
  return gen_template(outf, tplfile, vars, varc, mnext);
}


int main(int argc, const char** argv) {
  void* mnext = tmpmem;
  quiet = argc > 1 && strcmp(argv[1], "-quiet") == 0;
  progname = strrchr(argv[0], '/');
  progname = progname ? progname + 1 : argv[0];

  // files
  const char* spec_infile = "spec.md";
  const char* c_infile = "include/playsys.h.in";
  const char* c_outfile = "include/playsys.h";
  const char* js_infile = "backends/js/syscall.ts.in";
  const char* js_outfile = "backends/js/syscall.ts";

  // parse markdown document
  if (!quiet) printf("parse %s\n", spec_infile);
  char* src = preadfile(spec_infile, &mnext, mend);
  if (!src) err(1, "%s", spec_infile);
  u32 srclen = (u32)(mnext - (void*)src);
  //plog("remaining memory: %zu bytes", (usize)(mend - mnext));
  doc_t spec = {0};
  if (!parse_md(src, srclen, &spec))
    return 1;

  // print tables for debugging
  if (!quiet) for (table_t* t = spec.tables; t; t = t->next) {
    fprintf(stdout, "table \"%s\"\n", t->name);
    table_fprint(t, stdout);
    fputc('\n', stdout);
  }

  // generate C
  mnext = tmpmem; // reclaim memory
  FILE* outf = xfopen(c_outfile, "w");
  if (!gen_c(outf, &spec, c_infile, &mnext))
    return 1;
  fclose(outf);
  printf("%s: wrote %s\n", progname, c_outfile);

  // generate JS
  mnext = tmpmem; // reclaim memory
  outf = xfopen(js_outfile, "w");
  if (!gen_js(outf, &spec, js_infile, &mnext))
    return 1;
  fclose(outf);
  printf("%s: wrote %s\n", progname, js_outfile);

  // doc_dispose(&spec);
  return 0;
}
