/* float_pond.c  the ablation: the same pond, the same pushes, stepped in floats (single, then double) forward 2480 steps and back.
   The whole-number pond comes home to exactly zero (the board checks it). This prints how far the float ponds miss. */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
static void wire_cmd(uint8_t c, const uint8_t *p, int n){(void)c;(void)p;(void)n;}
static void wire_data(const void *v, int nb){(void)v;(void)nb;}
static void wire_mark(int s){(void)s;} static void wire_hold(int s){(void)s;} static uint32_t clock_now(void){return 0;}
#include "../minamo.h"
#define RUN(T, NAME) { static T A[NCELL], B[NCELL]; static int16_t S0[NCELL]; T *o = A, *n = B; for (int i = 0; i < NCELL; i++) A[i] = B[i] = 0; \
  for (int dir = 0; dir < 2; dir++) for (int k = 0; k < SMAX; k++) { int32_t t = dir == 0 ? k : SMAX - 1 - k; T *X = dir == 0 ? o : n, *Y = dir == 0 ? n : o; \
    for (int y = 1; y < GH - 1; y++) for (int x = 1; x < GW - 1; x++) { int i = y * GW + x; T c = Y[i]; \
      T L = 4 * (Y[i - 1] + Y[i + 1] + Y[i - GW] + Y[i + GW]) + (Y[i - GW - 1] + Y[i - GW + 1] + Y[i + GW - 1] + Y[i + GW + 1]) - 20 * c; int ky = KY[y], kk = KX[x] < ky ? KX[x] : ky; \
      X[i] = 2 * c - X[i] + L * (T)kk / (T)2048; } \
    for (int i = 0; i < NCELL; i++) S0[i] = 0; sources(S0, t); for (int i = 0; i < NCELL; i++) X[i] += S0[i]; \
    T *tmp = o; o = n; n = tmp; } \
  double mx = 0, sum = 0; for (int i = 0; i < NCELL; i++) { double v = fabs((double)A[i]) > fabs((double)B[i]) ? fabs((double)A[i]) : fabs((double)B[i]); if (v > mx) mx = v; sum += fabs((double)A[i]) + fabs((double)B[i]); } \
  printf("%-8s home: largest height left %.6g, summed %.6g (a drop's dip is about 700)\n", NAME, mx, sum); }
int main(void) { grow_ring(); for (int x = 0; x < GW; x++) { int32_t e = imin(x, GW - 1 - x); KX[x] = (uint8_t)(e >= 16 ? 130 : 6 + e * e * 124 / 256); }
  for (int y = 0; y < GH; y++) { int32_t e = imin(y, GH - 1 - y); KY[y] = (uint8_t)(e >= 16 ? 130 : 6 + e * e * 124 / 256); }
  RUN(float, "float"); RUN(double, "double");
  minamo_begin(); for (int p = 0; p < FILM; p++) move_film(1); printf("integer  home: %s (homes %u, bad %u)\n", pond_flat() ? "every cell zero" : "NOT flat", (unsigned)homes, (unsigned)home_bad); return 0; }
