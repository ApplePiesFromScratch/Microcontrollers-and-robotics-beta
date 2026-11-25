#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static void wire_cmd(uint8_t c, const uint8_t *p, int n) {} static void wire_data(const void *v, int nb) {} static void wire_mark(int s) {} static void wire_hold(int s) {}
static uint32_t clock_now(void) { return 0; }
#include "../candle.h"
int main(void) { candle_begin();
  long sum[M_N] = {0}, cnt[M_N] = {0}, mx[M_N] = {0};
  for (int j = 0; j < GY; j++) for (int i = 0; i < GX; i++) { int m = ME[j][i] & 15; sum[m] += L0[j][i]; cnt[m]++; if (L0[j][i] > mx[m]) mx[m] = L0[j][i]; }
  const char *nm[] = {"wood","wall","wax","brass","glass","cloth","pages"};
  for (int m = 0; m < M_N; m++) if (cnt[m]) printf("%-6s cells %6ld  mean L0 %6ld  max %6ld\n", nm[m], cnt[m], sum[m] / cnt[m], mx[m]);
  FILE *f = fopen("/tmp/candle/L0.pgm", "wb"); fprintf(f, "P5 %d %d 255\n", GX, GY);
  for (int j = 0; j < GY; j++) for (int i = 0; i < GX; i++) { int v = L0[j][i]; int t = v ? (int)(255.0 * 0) : 0; t = 0; int q = 0; while ((1 << q) < v) q++; fputc(q * 16 > 255 ? 255 : q * 16, f); } fclose(f);
  f = fopen("/tmp/candle/mat.pgm", "wb"); fprintf(f, "P5 %d %d 255\n", GX, GY); for (int j = 0; j < GY; j++) for (int i = 0; i < GX; i++) fputc((ME[j][i] & 15) * 36, f); fclose(f);
  printf("flame base on screen %d %d\n", FB_X, FB_Y);
  return 0; }
