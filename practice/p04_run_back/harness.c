/* harness.c: runs your step() 2000 latches out and 2000 back on a ring of 256 places. Do not edit. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define N 256
void step(const int32_t *before, const int32_t *now, int32_t *next);
static int32_t A[N], B[N], C[N], S0[N], S1[N];
int main(void) {
  for (int i = 0; i < N; i++) { int d = i - 80, e = i - 170; A[i] = (d * d < 400 ? (400 - d * d) * 40 : 0) - (e * e < 100 ? (100 - e * e) * 90 : 0); B[i] = A[i] + (i > 70 && i < 90 ? 37 : 0); }
  memcpy(S0, A, sizeof A); memcpy(S1, B, sizeof B);
  int32_t *before = A, *now = B, *next = C, *t; long moved = 0, peak = 0;
  for (int k = 0; k < 2000; k++) { step(before, now, next); t = before; before = now; now = next; next = t; }
  for (int i = 0; i < N; i++) { long d = (long)now[i] - S1[i]; if (d < 0) d = -d; if (d) moved++; if (d > peak) peak = d; }
  printf("OUT moved %ld of %d places, largest change %ld\n", moved, N, peak);
  t = before; before = now; now = t;                    /* swap the two snapshots: the same line now runs back */
  for (int k = 0; k < 2000; k++) { step(before, now, next); t = before; before = now; now = next; next = t; }
  long home = 0; for (int i = 0; i < N; i++) home += (now[i] == S0[i] && before[i] == S1[i]);
  printf("BACK %ld of %d places home exactly (both snapshots)\n", home, N);
  return 0; }
