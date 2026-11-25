/* desk.c  TIDE's own command stream decoded by an ST7789 stand-in; the ghost plays. Prints each frame's state; PPM shots at frames named. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static uint16_t PANEL[240][320]; static int cx0, cx1, cy0, cy1, wx, wy, writing;
static void desk_cmd(uint8_t c, const uint8_t *p, int n) {
  writing = 0;
  if (c == 0x2A && n >= 4) { cx0 = p[0] << 8 | p[1]; cx1 = p[2] << 8 | p[3]; }
  else if (c == 0x2B && n >= 4) { cy0 = p[0] << 8 | p[1]; cy1 = p[2] << 8 | p[3]; }
  else if (c == 0x2C) { writing = 1; wx = cx0; wy = cy0; } }
static void desk_data(const uint8_t *p, int nb) {
  if (!writing) return;
  for (int i = 0; i + 1 < nb; i += 2) { if (wy >= 0 && wy < 240 && wx >= 0 && wx < 320) PANEL[wy][wx] = (uint16_t)(p[i] << 8 | p[i + 1]); if (++wx > cx1) { wx = cx0; wy++; } } }
#define DESK 1
#include "tide.ino"
static void shot(const char *dir, int k) { char fn[128]; sprintf(fn, "%s/tide_%05d.ppm", dir, k); FILE *f = fopen(fn, "wb"); fprintf(f, "P6\n320 240\n255\n");
  for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) { uint16_t v = PANEL[y][x]; uint8_t r = (uint8_t)((v >> 11) << 3), g = (uint8_t)(((v >> 5) & 63) << 2), b = (uint8_t)((v & 31) << 3);
      /* the panel runs BGR: the sketch packs blue in the top bits */ uint8_t px3[3] = { b, g, r }; fwrite(px3, 1, 3, f); } fclose(f); }
int main(int argc, char **argv) {
  int n = argc > 1 ? atoi(argv[1]) : 2000, every = argc > 2 ? atoi(argv[2]) : 0; setup();
  for (int k = 1; k <= n; k++) { frame(); if (every && k % every == 0) shot(getenv("SHOTS") ? getenv("SHOTS") : ".", k);
    if (k % 100 == 0) { printf("F %d ", k); desk_line(); printf("\n"); } }
  desk_books(); return 0; }
