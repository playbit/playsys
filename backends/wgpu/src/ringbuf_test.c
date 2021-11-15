// clang -I../../../include -o ringbuf_test ringbuf_test.c ringbuf.c && ./ringbuf_test
#include "ringbuf.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char** argv) {

  u32 initcap = 4;
  char buf1[4];
  char buf2[8];
  char buf3[16];
  char buf4[32];
  char* bufv[4] = {buf1,buf2,buf3,buf4};
  usize bufc[4] = {sizeof(buf1),sizeof(buf2),sizeof(buf3),sizeof(buf4)};
  u32 bufi = 1;

  p_ringbuf_t b = {0};
  p_ringbuf_init(&b, bufv[0], bufc[0]);
  assert(b.p != NULL);

  int count = 5;
  for (; count < 7; count++) {

    u8 input[3] = { 11,22,33 };

    for (int i = 0; i < count; i++) {
      if (b.cap - b.len < sizeof(input)) {
        // p_ringbuf_move(&b, realloc(b.p, b.cap * 2), b.cap * 2);
        p_ringbuf_move(&b, bufv[bufi], bufc[bufi]);
        bufi++;
      }
      u32 n = p_ringbuf_write(&b, &input, sizeof(input));
      assert(n == sizeof(input));
    }

    for (int i = 0; i < count; i++) {
      u8 output[3] = {0};
      assert(sizeof(input) == sizeof(output));
      u32 n = p_ringbuf_read(&b, &output, sizeof(output));
      assert(n == sizeof(output));
      if (memcmp(input, output, sizeof(output)) != 0) {
        printf("i=%d count=%d\n", i, count);
        printf("input  %4u %4u %4u\n", input[0], input[1], input[2]);
        printf("output %4u %4u %4u\n", output[0], output[1], output[2]);
      }
      assert(memcmp(input, output, sizeof(output)) == 0);
    }
  }

  return 0;
}
