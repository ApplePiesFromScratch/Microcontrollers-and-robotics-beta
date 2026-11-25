/* desk.c  NAGARE on the desk: the demo hand plays; the screen's print every 50 frames; shots at frames named */
#include <stdio.h>
#include <stdlib.h>
#include "../core/nagare.h"
#include "../sim/hal_sim.h"
static void shot(int k) { char fn[64]; sprintf(fn, "shots/nagare_%05d.ppm", k); FILE *f = fopen(fn, "wb"); fprintf(f, "P6\n%d %d\n255\n", SW, SH); for (int i = 0; i < SW * SH; i++) fwrite(PALRGB[FB[i]], 1, 3, f); fclose(f); }
int main(int argc, char **argv) {
  int n = argc > 1 ? atoi(argv[1]) : 1500; nagare_begin();
  for (int k = 1; k <= n; k++) { nagare_frame();
    if (k % 50 == 0) printf("F %d print %08x step %u books %d div %d gap %lld\n", k, fb_print(), (unsigned)ours_latch, ours_books_shut(), (int)t_div_max, (long long)(text_heat() - (t_start + t_fed - t_vented)));
    for (int a = 2; a < argc; a++) if (atoi(argv[a]) == k) shot(k); }
  return 0; }
