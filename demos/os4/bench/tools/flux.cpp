#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#define KTOP32
#define NPL 4
#define PROC_EXTRA uint8_t code[98];
#include "../kernel.h"
static uint16_t SC = 0xACE1;
static uint8_t k_code(Proc *p, uint8_t i) { return p->code[i]; }
static void k_enter(uint8_t s) { (void)s; }
static void k_cls(void) {} static void k_dot(int32_t, int32_t) {} static void k_line(int32_t, int32_t, int32_t, int32_t) {} static void k_num(const Rd *, uint8_t) {}
static void k_stick(Rd *a, Rd *b) { a->v = b->v = 0; a->r = b->r = 0; a->d = b->d = 1; }
static uint8_t k_btn(void) { return 0; }
static int32_t k_rand(int32_t n) { uint16_t x = SC; x ^= x << 7; x ^= x >> 9; x ^= x << 8; SC = x; return x % n; }
static const uint8_t DEMO[8][98] = {
#include "demo8.inc"
};
// SPIN: A = 1; loop: B += A; JMP loop   (never shows: a hog)
static const uint8_t SPIN[98] = {3, 1 << 3 | 0, 1, 5 << 3 | 1, 0, 19 << 3, 1};
static void load(int s, const uint8_t *c) { memset(&proc[s], 0, sizeof proc[s]); memcpy(proc[s].code, c, 98); proc[s].n = c[0]; for (int i = 0; i < 8; i++) proc[s].reg[i].d = 1; proc[s].state = ST_RUN; }
int main(int argc, char **argv) {
  int N = argc > 1 ? atoi(argv[1]) : 600; k_begin(); if (argc > 2) rules.S = (uint16_t)atoi(argv[2]);
  for (int c = 0; c < 4; c++) cell[c].d = 1;
  load(0, DEMO[0]); load(1, DEMO[2]); load(2, DEMO[3]); load(3, SPIN);
  long tk[4] = {0}, shows[4] = {0};
  for (int l = 1; l <= N; l++) { k_latch(); for (int s = 0; s < 4; s++) tk[s] += flow[s].took;
    if (l <= 6 || l % 100 == 0) printf("latch %4d steps %3d | took %3d %3d %3d %3d | L %4d %4d %4d %4d | field %4d recon %u books %s\n", l, k_steps, flow[0].took, flow[1].took, flow[2].took, flow[3].took, flow[0].L, flow[1].L, flow[2].L, flow[3].L, field, (unsigned)k_recon, k_books() ? "shut" : "OPEN"); }
  printf("share of %ld steps: BOUNCE %ld  SLOPE %ld  TURN %ld  SPIN %ld | leaks %u spent %u\n", tk[0] + tk[1] + tk[2] + tk[3], tk[0], tk[1], tk[2], tk[3], (unsigned)k_leaks, (unsigned)k_spent);
}
