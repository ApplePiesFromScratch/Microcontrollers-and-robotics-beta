/* harness.c: calls your p02 functions and prints what the checker reads. Do not edit. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
typedef struct { int32_t n, d, acc; int64_t count; } pulse_t;
int32_t mouth(int32_t top, int32_t den);
int pulse_tick(pulse_t *p);
int main(int argc, char **argv) {
  if (argc > 1 && argv[1][0] == 'm') {               /* mouth over a grid of tops and bottoms */
    static const int32_t dens[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 16, 256, 65536 };
    for (unsigned k = 0; k < sizeof dens / sizeof dens[0]; k++)
      for (int32_t t = -4500; t <= 4500; t++) printf("%d %d %d\n", t, dens[k], mouth(t, dens[k]));
    return 0; }
  int32_t n = atoi(argv[2]), d = atoi(argv[3]); long ticks = atol(argv[4]);
  pulse_t p = { n, d, 0, 0 };
  for (long k = 1; k <= ticks; k++) { int e = pulse_tick(&p); printf("%d %d %lld\n", e, p.acc, (long long)p.count); }
  return 0; }
