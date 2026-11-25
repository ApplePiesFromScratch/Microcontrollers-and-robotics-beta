#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static void wire_cmd(uint8_t c, const uint8_t *p, int n) {} static void wire_data(const void *v, int nb) {} static void wire_mark(int s) {} static void wire_hold(int s) {}
static uint32_t clock_now(void) { return 0; }
#include "../candle.h"
int main(int argc, char **argv) { candle_begin();
  for (int a = 1; a + 1 < argc; a += 2) { int i = atoi(argv[a]), j = atoi(argv[a + 1]);
    printf("cell %d %d: L0 %u  R %d %d %d  e %d  mat %d\n", i, j, L0[j][i], RATEX[j][i], RATEY[j][i], RATEZ[j][i], ME[j][i] >> 4, ME[j][i] & 15); } }
