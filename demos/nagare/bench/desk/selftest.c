/* selftest.c  NAGARE's kill lines (KILL_LINES.md, amended in AMENDMENTS.md; both hashed before their reads) */
#include <stdio.h>
#include <string.h>
#include "../core/sim.h"
static int fails;
static void check(int ok, const char *what) { printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what); if (!ok) fails++; }
static int warm_centre(int ours) { static int32_t w[NC]; for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) w[AT(i, j)] = ours ? ours_warmth(AT(i, j)) : text_warmth(i, j);
  int32_t lo = 0, hi = 1 << 20; while (lo < hi) { int32_t mid = (lo + hi + 1) / 2; int c = 0; for (int k = 0; k < NC; k++) c += w[k] >= mid; if (c >= NC / 10) lo = mid; else hi = mid - 1; }
  int64_t s = 0; int n = 0; for (int k = 0; k < NC; k++) if (w[k] >= lo) { s += GH - 1 - k / GW; n++; } return n ? (int)(s * 10 / n) : 0; }
static void turning(int *pos, int *neg) { *pos = *neg = 0; for (int j = 0; j + 4 < GH; j++) for (int i = 0; i + 4 < GW; i++) { int64_t c = 0;   /* circulation round 4 x 4 loops */
    for (int t = 0; t < 4; t++) { int a = AT(i + t, j), b = AT(i + 4, j + t), d = AT(i + 4 - t, j + 4), e = AT(i, j + 4 - t);
      if (M[a] > 0 && M[b] > 0 && M[d] > 0 && M[e] > 0) c += qdiv(PX[a], M[a]) + qdiv(PY[b], M[b]) - qdiv(PX[d], M[d]) - qdiv(PY[e], M[e]); }
    if (c > 40) (*pos)++; if (c < -40) (*neg)++; } }
int main(void) {
  printf("N1 our books, 20,000 latches: puffs, the heater on and off and moved\n");
  { ours_begin(); int open = 0, first = -1;
    for (int k = 0; k < 20000; k++) { if (k % 700 == 10) puff(8 + (k / 700 * 13) % 34); if (k % 5000 == 2000) { heater_on = 1; heater_x = 10 + (k / 5000) * 9; } if (k % 5000 == 4000) heater_on = 0;
      ours_step(); if (!ours_books_shut()) { open++; if (first < 0) first = k; } }
    books_t b = ours_books(); printf("    stuff %lld, warmth %lld = %lld + fed %lld - vented %lld; momentum x %lld, y %lld; walls gave %lld, %lld\n", (long long)b.m, (long long)b.q, (long long)start_q, (long long)fed_q, (long long)vented_q, (long long)b.px, (long long)b.py, (long long)walls_px, (long long)walls_py);
    check(open == 0, "stuff, warmth and both components of momentum shut to the unit on every one of 20,000 latches"); heater_on = 0; }
  printf("N2c a puff rises (amendment A2: KL 24)\n");
  { int xs[3] = { 12, 25, 38 }, bad = 0; for (int t = 0; t < 3; t++) { ours_begin(); text_begin(); puff(xs[t]); int ho = -1, ht = -1;
      for (int k = 1; k <= 300; k++) { ours_step(); text_step(); if (ho < 0 && warm_centre(1) > 200) ho = k; if (ht < 0 && warm_centre(0) > 200) ht = k; }
      printf("    puff at %d: ours above the middle from latch %d, the textbook from step %d\n", xs[t], ho, ht); if (ho < 0) bad++; }
    check(!bad, "each puff's warmest tenth rises above the middle within 300 latches"); }
  printf("N3 turning, with the heater on and still\n");
  { ours_begin(); heater_on = 1; heater_x = GW / 2; int p = 0, q = 0; for (int k = 0; k < 3000; k++) ours_step(); turning(&p, &q); int n = (GW - 4) * (GH - 4);
    printf("    loops turning one way %d, the other %d, of %d\n", p, q, n); check(p * 20 > n && q * 20 > n, "both signs of turning, each at more than 5% of the loops, by 3,000 latches"); heater_on = 0; }
  printf("N4 the books can see a set\n");
  { ours_begin(); puff(25); spoil_at = 150; int open_at = -1; for (int k = 0; k < 300; k++) { ours_step(); if (open_at < 0 && !ours_books_shut()) open_at = k; } spoil_at = -1;
    check(open_at == 150, "one line that sets a place's warmth (latch 150) opens the books at that latch"); }
  printf("N5 the textbook, measured\n");
  { text_begin(); puff(25); int32_t dmax = 0; int64_t gmax = 0, gmin = 0;
    for (int k = 1; k <= 1200; k++) { text_step(); if (t_div_max > dmax) dmax = t_div_max; int64_t g = text_heat() - (t_start + t_fed - t_vented); if (g > gmax) gmax = g; if (g < gmin) gmin = g; }
    printf("    over 1,200 steps (JACOBI %d): continuity residual up to %.4f a unit of t; heat unaccounted from %+.1f%% to %+.1f%% of what came in\n", JACOBI, dmax / 65536.0, 100.0 * gmin / (double)(t_start + t_fed), 100.0 * gmax / (double)(t_start + t_fed)); }
  printf("%s: %d failed\n", fails ? "SELFTEST FAILED" : "SELFTEST PASSED", fails); return fails != 0; }
