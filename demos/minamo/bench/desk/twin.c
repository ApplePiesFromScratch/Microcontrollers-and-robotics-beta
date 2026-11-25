/* twin.c  MINAMO on the desk: the sketch's own command stream decoded by an ST7789 stand-in.  twin frames tag [frame...] */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define FASTF
static uint16_t PANEL[240][320]; static int cx0, cx1, cy0, cy1, px, py, cmdn, hib; static uint8_t hi; static uint32_t wire_print = 2166136261u;
static void wire_cmd(uint8_t c, const uint8_t *p, int n) { wire_print = (wire_print ^ (0x100u | c)) * 16777619u; for (int k = 0; k < n; k++) wire_print = (wire_print ^ p[k]) * 16777619u; cmdn = c;
  if ((c == 0x2A || c == 0x2B) && n == 4) { int a = p[0] << 8 | p[1], b = p[2] << 8 | p[3]; if (c == 0x2A) { cx0 = a; cx1 = b; } else { cy0 = a; cy1 = b; } }
  if (c == 0x2C) { px = cx0; py = cy0; hib = 0; } }
static void wire_data(const void *v, int nb) { const uint8_t *p = (const uint8_t *)v; for (int k = 0; k < nb; k++) wire_print = (wire_print ^ p[k]) * 16777619u; if (cmdn != 0x2C) return;
  for (int k = 0; k < nb; k++) { if (!hib) { hi = p[k]; hib = 1; continue; } hib = 0; if (py < 240 && px < 320) PANEL[py][px] = (uint16_t)(hi << 8 | p[k]); if (++px > cx1) { px = cx0; py++; } } }
static void wire_mark(int s) { (void)s; } static void wire_hold(int s) { (void)s; }
static uint32_t clock_now(void) { return 0; }
#include "../minamo.h"
static void ppm(const char *nm) { FILE *f = fopen(nm, "wb"); fprintf(f, "P6 320 240 255\n");
  for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) { uint16_t v = PANEL[y][x]; fputc(((v >> 11) & 31) * 255 / 31, f); fputc(((v >> 5) & 63) * 255 / 63, f); fputc((v & 31) * 255 / 31, f); } fclose(f); }
int main(int argc, char **argv) {
  int N = atoi(argv[1]); const char *tag = argv[2]; minamo_begin(); if (getenv("FRAMELINES")) printf("ROOM %08x\n", (unsigned)pond_print()); int every = getenv("EVERY") ? atoi(getenv("EVERY")) : 0;
  for (int k = 1; k <= N; k++) { wire_print = 2166136261u; frame();
    if (getenv("FRAMELINES")) printf("F %d story %08x wire %u\n", k, (unsigned)story_print(), (unsigned)wire_print);
    if (getenv("RAW")) { static uint8_t buf[240][320][3]; for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) { uint16_t v = PANEL[y][x]; buf[y][x][0] = (uint8_t)(((v >> 11) & 31) * 255 / 31); buf[y][x][1] = (uint8_t)(((v >> 5) & 63) * 255 / 63); buf[y][x][2] = (uint8_t)((v & 31) * 255 / 31); } fwrite(buf, 1, sizeof buf, stdout); }
    int want = every && (k % every) == 0; for (int a = 3; a < argc; a++) if (atoi(argv[a]) == k) want = 1;
    if (want) { char nm[128]; snprintf(nm, sizeof nm, "/tmp/minamo/%s_%05d.ppm", tag, k); ppm(nm); } }
  fprintf(stderr, "homes %u bad %u mirror ok %u bad %u steps %u\n", (unsigned)homes, (unsigned)home_bad, (unsigned)mirror_ok, (unsigned)mirror_bad, (unsigned)steps_n); return 0; }
