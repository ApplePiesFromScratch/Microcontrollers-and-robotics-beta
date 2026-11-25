/* desk_panel.c: a desk twin for any sketch in this repo that has a DESK build (the ACE family: TIDE, HIGH NOON, ACE 4).
   An ST7789 stand-in decodes the sketch's own command stream (0x2A columns, 0x2B rows, 0x2C write); then the sketch runs
   its own setup() and frame(). Each 50th frame prints the frame count and an FNV hash of the whole panel, so two builds
   (two compilers, two optimisation levels, desk and emulated chip) can be compared line for line.
     gcc -O2 -w -DSKETCH='"demos/highnoon/highnoon.ino"' -o /tmp/hn tools/desk_panel.c && /tmp/hn 600
   With SHOTS=dir set, writes a PPM of the panel at every 50th frame (the panel runs BGR unless RGB_ORDER). */
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
#include SKETCH
static uint32_t panel_print(void) { uint32_t h = 0x811c9dc5u; for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) { h = (h ^ (PANEL[y][x] & 255)) * 0x01000193u; h = (h ^ (PANEL[y][x] >> 8)) * 0x01000193u; } return h; }
static void shot(const char *dir, int k) { char fn[256]; snprintf(fn, sizeof fn, "%s/frame_%05d.ppm", dir, k); FILE *f = fopen(fn, "wb"); if (!f) return; fprintf(f, "P6\n320 240\n255\n");
  for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) { uint16_t v = PANEL[y][x]; uint8_t r = (uint8_t)((v >> 11) << 3), g = (uint8_t)(((v >> 5) & 63) << 2), b = (uint8_t)((v & 31) << 3);
      uint8_t px3[3] = { b, g, r }; fwrite(px3, 1, 3, f); } fclose(f); }
int main(int argc, char **argv) {
  int n = argc > 1 ? atoi(argv[1]) : 600; const char *dir = getenv("SHOTS"); setup();
  for (int k = 1; k <= n; k++) { frame(); if (k % 50 == 0) { printf("F %d panel %08x\n", k, (unsigned)panel_print()); if (dir) shot(dir, k); } }
  return 0; }
