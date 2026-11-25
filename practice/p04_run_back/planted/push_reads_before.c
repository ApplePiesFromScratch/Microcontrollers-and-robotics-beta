#include <stdint.h>
#define N 256
static int32_t mouth(int32_t t, int32_t d) { int32_t a = t < 0 ? -t : t, q = a / d, r = a % d; if (2 * r > d || (2 * r == d && (q & 1))) q++; return t < 0 ? -q : q; }
static int32_t push(const int32_t *now, int i) {
  int32_t l = now[(i + N - 1) % N], m = now[i], r = now[(i + 1) % N];
  int32_t hard = m > 20000 ? -mouth(m - 20000, 8) : m < -20000 ? mouth(-20000 - m, 8) : 0;   /* a stiffer wall past 20000: not linear */
  return mouth(l + r - 2 * m, 4) + hard; }
void step(const int32_t *before, const int32_t *now, int32_t *next) {
  for (int i = 0; i < N; i++) next[i] = 2 * now[i] - before[i] + push(before, i); }
