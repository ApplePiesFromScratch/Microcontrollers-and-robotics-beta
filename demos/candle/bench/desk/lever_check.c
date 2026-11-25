/* lever_check.c  the witness for the lever: does value + rates x move match the light worked out afresh with the flame moved?
   For every 7th cell hit by one ray (no edge), and each move up to the flame's largest, compare L0 + R.d (what the frame uses)
   against light_reading() run again with the flame really moved (shadows counted again too). Errors on the cell's scale. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static void wire_cmd(uint8_t c, const uint8_t *p, int n) {} static void wire_data(const void *v, int nb) {} static void wire_mark(int s) {} static void wire_hold(int s) {}
static uint32_t clock_now(void) { return 0; }
#include "../candle.h"
static int cmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
int main(void) { candle_begin();
  static const int MV[][3] = { {3, 0, 0}, {-3, 0, 0}, {0, 3, 0}, {0, 0, 3}, {3, 2, -3}, {2, 0, 0}, {-2, 0, 0} };
  for (int m = 0; m < 7; m++) {
    static double err[30000], ers[30000]; int n = 0, ns = 0; double lever_moves = 0; int counted = 0;
    for (int j = 0; j < GY; j++) for (int i = 0; i < GX; i++) {
      if ((j * GX + i) % 7) continue;
      if (!((EDGE_OFF[(j * GX + i) >> 3] >> ((j * GX + i) & 7)) & 1)) continue;
      V3 D = cell_dir(i, j, 0, 0), P, N; int obj = -1; int mat = cast(EYE, D, &P, &N, &obj); if (mat < 0) continue;
      int64_t shd = stuff_shade(mat, P);
      Rd L = light_reading(P, N, mat, obj, MV[m][0] * 10, MV[m][1] * 10, MV[m][2] * 10);   /* 1/64 dm = 10 of 1/64 cm */
      int64_t exact = (L.v * SCALE_L * shd / 256) >> FIX; if (exact > 65535) exact = 65535; if (exact < 0) exact = 0;
      int e = ME[j][i] >> 4; int64_t pred = L0[j][i] + (int64_t)(RATEX[j][i] * MV[m][0] + RATEY[j][i] * MV[m][1] + RATEZ[j][i] * MV[m][2]) * (1 << e); if (pred < 0) pred = 0; if (pred > 65535) pred = 65535;
      int64_t still = L0[j][i]; if (still < 16) continue;                                  /* cells too dark to matter on the glass */
      double moved = (double)(exact - still), miss = (double)(pred - exact);
      int lit = seen(P, obj, 0, 0, 0) == (1 << FIX) && seen(P, obj, MV[m][0] * 10, MV[m][1] * 10, MV[m][2] * 10) == (1 << FIX);
      if (lit) err[n++] = (miss < 0 ? -miss : miss) / (double)still; else ers[ns++] = (miss < 0 ? -miss : miss) / (double)still; if (moved != 0) { lever_moves += (moved < 0 ? -moved : moved); counted++; } }
    qsort(err, n, sizeof(double), cmp); qsort(ers, ns, sizeof(double), cmp);
    printf("flame moved (%2d,%2d,%2d)/64 dm | in the open, %4d cells: median %.4f 95th %.4f worst %.3f | in or near shadow, %3d cells: median %.3f 95th %.3f\n",
      MV[m][0], MV[m][1], MV[m][2], n, err[n / 2], err[n * 95 / 100], err[n - 1], ns, ns ? ers[ns / 2] : 0, ns ? ers[ns * 95 / 100] : 0); }
  return 0; }
