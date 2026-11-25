/* probe.c  both sides, no screen: books, plume height, turning, crowding, speed; PNG-able PPM of the two warmth fields */
#include <stdio.h>
#include <stdlib.h>
#include "../core/sim.h"
static int plume_y_ours(void) { /* centre height (rows from the floor, x10) of the warmest tenth */ int32_t w[NC]; for (int k = 0; k < NC; k++) w[k] = ours_warmth(k);
  int32_t lo = 0, hi = 1 << 20; while (lo < hi) { int32_t mid = (lo + hi + 1) / 2; int c = 0; for (int k = 0; k < NC; k++) c += w[k] >= mid; if (c >= NC / 10) lo = mid; else hi = mid - 1; }
  int64_t s = 0; int n = 0; for (int k = 0; k < NC; k++) if (w[k] >= lo) { s += GH - 1 - k / GW; n++; } return n ? (int)(s * 10 / n) : 0; }
static int plume_y_text(void) { int32_t w[NC]; for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) w[AT(i, j)] = text_warmth(i, j);
  int32_t lo = 0, hi = 1 << 20; while (lo < hi) { int32_t mid = (lo + hi + 1) / 2; int c = 0; for (int k = 0; k < NC; k++) c += w[k] >= mid; if (c >= NC / 10) lo = mid; else hi = mid - 1; }
  int64_t s = 0; int n = 0; for (int k = 0; k < NC; k++) if (w[k] >= lo) { s += GH - 1 - k / GW; n++; } return n ? (int)(s * 10 / n) : 0; }
static void turning_ours(int *pos, int *neg) { *pos = *neg = 0; for (int j = 0; j + 4 < GH; j++) for (int i = 0; i + 4 < GW; i++) { int64_t c = 0;
    for (int t = 0; t < 4; t++) { int a = AT(i + t, j), b = AT(i + 4, j + t), d = AT(i + 4 - t, j + 4), e = AT(i, j + 4 - t);
      if (M[a] > 0 && M[b] > 0 && M[d] > 0 && M[e] > 0) c += qdiv(PX[a], M[a]) + qdiv(PY[b], M[b]) - qdiv(PX[d], M[d]) - qdiv(PY[e], M[e]); }
    if (c > 40) (*pos)++; if (c < -40) (*neg)++; } }
static void dump(const char *fn) { FILE *f = fopen(fn, "wb"); fprintf(f, "P6\n%d %d\n255\n", GW * 2 + 2, GH); for (int j = 0; j < GH; j++) { for (int s = 0; s < 2; s++) { for (int i = 0; i < GW; i++) { int32_t w = s ? ours_warmth(AT(i, j)) : text_warmth(i, j); int v = w * 255 / 256; if (v < 0) v = 0; if (v > 255) v = 255;
          unsigned char c[3] = { (unsigned char)(v > 128 ? 255 : v * 2), (unsigned char)(v > 128 ? (v - 128) * 2 : 0), (unsigned char)(v < 64 ? v * 2 + 40 : v > 192 ? (v - 192) * 4 : 40) }; fwrite(c, 1, 3, f); } if (!s) { unsigned char g[6] = { 80, 80, 80, 80, 80, 80 }; fwrite(g, 1, 6, f); } } } fclose(f); }
int main(int argc, char **argv) {
  int n = argc > 1 ? atoi(argv[1]) : 3000, every = argc > 2 ? atoi(argv[2]) : 500; ours_begin(); text_begin(); int open = 0;
  for (int k = 1; k <= n; k++) { ours_step(); text_step(); if (!ours_books_shut()) { if (!open) printf("books open at latch %d\n", k); open++; }
    if (k % every == 0) { int p, q; turning_ours(&p, &q); int32_t mn = 1 << 30, mx = 0, vmax = 0; for (int c = 0; c < NC; c++) { if (M[c] < mn) mn = M[c]; if (M[c] > mx) mx = M[c]; int32_t u = M[c] > 0 ? qdiv(PX[c], M[c]) : 0, v = M[c] > 0 ? qdiv(PY[c], M[c]) : 0; if (u < 0) u = -u; if (v < 0) v = -v; if (u > vmax) vmax = u; if (v > vmax) vmax = v; }
      int64_t th = text_heat(); double gap = 100.0 * (double)(th - (t_start + t_fed - t_vented)) / (double)(t_start + t_fed);
      printf("%6d OURS plume %4.1f turning +%d -%d crowd %d..%d vmax %d/256 | TEXT plume %4.1f div %.4f heatgap %+.2f%%\n", k, plume_y_ours() / 10.0, p, q, mn, mx, vmax, plume_y_text() / 10.0, t_div_max / 65536.0, gap);
      char fn[64]; sprintf(fn, "shots/probe_%05d.ppm", k); dump(fn); } }
  printf("books open on %d of %d latches\n", open, n); return 0; }
