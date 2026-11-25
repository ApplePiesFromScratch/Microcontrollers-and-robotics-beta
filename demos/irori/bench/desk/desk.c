/* desk.c  the desk twin: IRORI on the simulated bench, the demo ghost as the hand. Prints a print of the screen every 50 latches
   (the chip prints the same), and writes screenshots: desk N L1 L2 ... -> shots/irori_L.ppm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/irori.h"
#include "../sim/hal_sim.h"
static void shot(uint32_t k) {
  char fn[64]; sprintf(fn, "shots/irori_%05u.ppm", (unsigned)k); FILE *f = fopen(fn, "wb"); if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", SW, SH); for (int i = 0; i < SW * SH; i++) fwrite(PALRGB[FB[i]], 1, 3, f); fclose(f); }
int main(int argc, char **argv) {
  uint32_t n = argc > 1 ? (uint32_t)atoi(argv[1]) : 3000;
  os_begin();
#ifdef START_PROFILE
  profile_start(START_PROFILE);
#endif
  ghost_on = 1;                                            /* the hand from latch 1: the test drives the UI the way the demo does */
  for (uint32_t k = 1; k <= n; k++) {
    sim_latch = k; os_latch();
    if (k % 50 == 0) printf("F %u print %08x fg %d running %04x focus %d kb %d\n", (unsigned)k, (unsigned)fb_print(), fg, (unsigned)running, home_focus, kb_on);
    for (int a = 2; a < argc; a++) if ((uint32_t)atoi(argv[a]) == k) shot(k); }
  printf("SERIAL %s", sim_serial_log); printf("\nEND saves %u mends %u\n", (unsigned)saves, (unsigned)mends);
  return 0; }
