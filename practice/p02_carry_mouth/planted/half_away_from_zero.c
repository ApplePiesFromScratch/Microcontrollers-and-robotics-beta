#include <stdint.h>
typedef struct { int32_t n, d, acc; int64_t count; } pulse_t;
int32_t mouth(int32_t top, int32_t den) {
  int32_t a = top < 0 ? -top : top, q = a / den, r = a % den;
  if (2 * (int64_t)r >= den) q++;
  return top < 0 ? -q : q; }
int pulse_tick(pulse_t *p) { int e = 0; p->acc += p->n; while (p->acc >= p->d) { p->acc -= p->d; e++; p->count++; } return e; }
