// c3_desk: an ST7789 stand-in that decodes the sketch's own command stream, then the sketch itself (DESK build), N frames.
// Each frame: the panel must equal the composed picture (every changed tile was sent). Writes chosen frames as PPM, and a line a frame.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static uint16_t PANEL[240][320]; static int cx0, cx1, cy0, cy1, px, py, cmd_now, argn; static uint8_t args[8], hi; static int hib;
static void desk_cmd(uint8_t c, const uint8_t *p, int n) {
  cmd_now = c;
  if ((c == 0x2A || c == 0x2B) && n == 4) { int a = p[0] << 8 | p[1], b = p[2] << 8 | p[3]; if (c == 0x2A) { cx0 = a; cx1 = b; } else { cy0 = a; cy1 = b; } }
  if (c == 0x2C) { px = cx0; py = cy0; hib = 0; } }
static void desk_data(const uint8_t *p, int nb) {
  if (cmd_now != 0x2C) return;
  for (int k = 0; k < nb; k++) { if (!hib) { hi = p[k]; hib = 1; continue; } hib = 0; uint16_t v = (uint16_t)(hi << 8 | p[k]);
    if (py < 240 && px < 320) PANEL[py][px] = v; if (++px > cx1) { px = cx0; py++; } } }
#define DESK
#include SKETCH
static void ppm(const char *name) { FILE *f = fopen(name, "wb"); fprintf(f, "P6 320 240 255\n");
  for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) { uint16_t v = PANEL[y][x]; uint8_t r = (uint8_t)(((v >> 11) & 31) * 255 / 31), g = (uint8_t)(((v >> 5) & 63) * 255 / 63), b = (uint8_t)((v & 31) * 255 / 31); fputc(r, f); fputc(g, f); fputc(b, f); }
  fclose(f); }
int main(int argc, char **argv) {
  int N = argc > 1 ? atoi(argv[1]) : 600; const char *tag = argc > 2 ? argv[2] : "x"; setup(); long bad = 0;
  for (int k = 1; k <= N; k++) { frame();
    int diff = 0; for (int y = 0; y < 240 && !diff; y++) for (int x = 0; x < 320; x++) { uint16_t w = PAL[FB[y * 320 + x]]; w = (uint16_t)((w >> 8) | (w << 8)); if (PANEL[y][x] != w) { diff = 1; break; } }
    bad += diff;
    for (int a = 3; a < argc; a++) if (atoi(argv[a]) == k) { char nm[64]; snprintf(nm, sizeof nm, "/tmp/%s_%04d.ppm", tag, k); ppm(nm); }
    printf("F %d glass %08x bytes %u tiles %u%s\n", k, (unsigned)glass_print(), (unsigned)f_bytes, (unsigned)f_tiles, diff ? " PANEL!=PICTURE" : ""); }
  fprintf(stderr, "%s: %d frames, panel != picture on %ld; heap held %u late %u theta %u bad %u\n", tag, N, bad, (unsigned)held_n, (unsigned)late_n, (unsigned)refused, (unsigned)(bad_moves + keep_bad));
#ifdef KTOP32
  fprintf(stderr, "  flux: latches %u recon %u leaks %u field %u/%u\n", (unsigned)k_latches, (unsigned)k_recon, (unsigned)k_leaks, (unsigned)field, (unsigned)rules.G0);
#endif
  return 0; }
