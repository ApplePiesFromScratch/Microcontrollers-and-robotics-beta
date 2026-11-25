#include <stdint.h>
typedef struct { int32_t n, d, acc; int64_t count; } pulse_t;
int32_t mouth(int32_t top, int32_t den) {
  int32_t a = top < 0 ? -top : top, q = a / den, r = a % den;
  if (2 * (int64_t)r > den || (2 * (int64_t)r == den && (q & 1))) q++;
  return top < 0 ? -q : q; }
static double fa; int pulse_tick(pulse_t *p) { int e = 0; double r = (double)p->n / p->d; fa += r; while (fa >= 1.0) { fa -= 1.0; e++; p->count++; } p->acc = (int32_t)(p->n * 0 + (p->count * 0)); return e; }
