#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static void wire_cmd(uint8_t c, const uint8_t *p, int n) {} static void wire_data(const void *v, int nb) {} static void wire_mark(int s) {} static void wire_hold(int s) {}
static uint32_t clock_now(void) { return 0; }
#include "../candle.h"
int main(int argc, char **argv) { int N = atoi(argv[1]); grow_camera(); grow_flame(); flame_place();
  int mi = 9999, ma = -9999, cxm = 99, cxM = -99, cym = 99, cyM = -99, dxm = 99, dxM = -99, drafts = 0, gut = 0, emn = 9999, emx = 0; long jumps = 0;
  int pcx = 0, pcy = 0;
  for (int k = 0; k < N; k++) { int dl = draft_left, gt = gutter; story_step(); if (draft_left > dl) drafts++; if (gutter > gt) gut++;
    if (inten < mi) mi = inten; if (inten > ma) ma = inten; if (cam_ox < cxm) cxm = cam_ox; if (cam_ox > cxM) cxM = cam_ox; if (cam_oy < cym) cym = cam_oy; if (cam_oy > cyM) cyM = cam_oy;
    if (DX < dxm) dxm = DX; if (DX > dxM) dxM = DX; if (expo < emn) emn = expo; if (expo > emx) emx = expo; jumps += abs(cam_ox - pcx) + abs(cam_oy - pcy); pcx = cam_ox; pcy = cam_oy;
    if (argc > 2 && k % 15 == 0) printf("%5d inten %3d expo %3d DX %2d DY %2d DZ %2d cam %3d %3d h %d sway %d\n", k, inten, expo, DX, DY, DZ, cam_ox, cam_oy, flame_h, flame_sway); }
  printf("frames %d: inten %d..%d  expo %d..%d  DX %d..%d  cam x %d..%d y %d..%d  mean camera move %.2f px/frame  drafts %d gutters %d\n", N, mi, ma, emn, emx, dxm, dxM, cxm, cxM, cym, cyM, (double)jumps / N, drafts, gut); }
