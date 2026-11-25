#include <stdio.h>
#include "../core/sim.h"
static int warm_centre(int ours) { int32_t w[NC]; for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) w[AT(i, j)] = ours ? ours_warmth(AT(i, j)) : text_warmth(i, j);
  int32_t lo = 0, hi = 1 << 20; while (lo < hi) { int32_t mid = (lo + hi + 1) / 2; int c = 0; for (int k = 0; k < NC; k++) c += w[k] >= mid; if (c >= NC / 10) lo = mid; else hi = mid - 1; }
  int64_t s = 0; int n = 0; for (int k = 0; k < NC; k++) if (w[k] >= lo) { s += GH - 1 - k / GW; n++; } return n ? (int)(s * 10 / n) : 0; }
int main(void) { int xs[3] = { 12, 25, 38 }, fails = 0;
  for (int t = 0; t < 3; t++) { ours_begin(); text_begin(); puff(xs[t]); int hit_o = -1, hit_t = -1;
    for (int k = 1; k <= 300; k++) { ours_step(); text_step(); if (hit_o < 0 && warm_centre(1) > 200) hit_o = k; if (hit_t < 0 && warm_centre(0) > 200) hit_t = k; }
    printf("N2b puff at x %d: ours' warm centre above the middle from latch %d; textbook from step %d; books %s\n", xs[t], hit_o, hit_t, ours_books_shut() ? "shut" : "OPEN");
    if (hit_o < 0) fails++; }
  printf("N2b %s\n", fails ? "FAILED" : "PASSED"); return fails; }
