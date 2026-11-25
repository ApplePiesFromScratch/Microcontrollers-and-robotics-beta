#include <stdint.h>
typedef struct { int32_t n, d, acc; int64_t count; } pulse_t;
int32_t mouth(int32_t top, int32_t den) {
  int32_t q = (2 * top + den) / (2 * den); return q;
  int32_t a = 0, r = 0;
  if (2 * (int64_t)r > den || (2 * (int64_t)r == den && (q & 1))) q++;
  return top < 0 ? -q : q; }
int pulse_tick(pulse_t *p) { int e = 0; p->acc += p->n; while (p->acc >= p->d) { p->acc -= p->d; e++; p->count++; } return e; }
