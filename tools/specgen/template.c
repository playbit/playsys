// SPDX-License-Identifier: Apache-2.0

#include "template.h"

bool template_sub(
  FILE* outf,
  const char* src, usize srclen, const char* srcfile,
  const tplvar_t* varv, int varc)
{
  const char* next = src;
  const char* end = src + srclen;
  const char* name = NULL;
  const char* chunk; // start of current chunk
  int line = 1;
  int len = 0;
  int nest = 0;
  int nmissing_vars = 0;

proot:
  chunk = next;
  while (next < end) {
    switch (*next) {
      case '\n': line++; break;
      case '\\': next++; break; // don't check next byte
      case '$':
        fwrite(chunk, next - chunk, 1, outf);
        goto startname;
    }
    next++;
  }
  fwrite(chunk, next - chunk, 1, outf);
  return nmissing_vars == 0;

startname:
  chunk = next; // used in case lookup fails
  next++;
  name = next;
  nest = 0;
  while (next < end) {
    switch (*next) {
      case '{':
        if (nest)
          goto endname;
        nest++;
        name++;
        break;
      case '}':
        if (nest)
          next++; // consume
        goto endname;
      case ':' ... '@':
      case '[' ... '^':
      case '`':
        goto endname;
      default:
        if (*next < '0' || *next > 'z')
          goto endname;
        break;
    }
    next++;
  }
endname:
  len = (int)(next - name) - nest;
  // printf("\n---- name \"%.*s\" ---\n", len, name);
  for (int i = 0; i < varc; i++) {
    if (len == varv[i].namelen && memcmp(name, varv[i].name, len) == 0) {
      // printf("  var found\n");
      fwrite(varv[i].value.p, varv[i].value.len, 1, outf);
      goto proot;
    }
  }
  fwrite(chunk, next - chunk, 1, outf);
  fprintf(stderr, "%s:%d: variable \"%.*s\" not found\n", srcfile, line, len, name);
  nmissing_vars++;
  goto proot;
}
