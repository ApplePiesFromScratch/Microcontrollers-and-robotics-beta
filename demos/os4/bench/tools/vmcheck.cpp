// vmcheck: the kernel's machine against OS 2's and OS 3's own step code, on the factory files and on random programs.
// Each run records every slot after every step and every dot/line/number drawn, as a trace print.
//   kernel TOP16 vs OS 2  : must agree exactly (same machine, same width)
//   kernel TOP32 vs OS 3  : must agree exactly
//   kernel TOP16 vs TOP32 : must agree on every step until TOP16 refuses WIDE (exact fractions do not depend on width)
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
static uint64_t TR; static void tr(uint64_t v) { TR = (TR ^ v) * 0x100000001b3ull; }
static uint16_t SC; static uint16_t scat(void) { uint16_t x = SC; x ^= x << 7; x ^= x >> 9; x ^= x << 8; SC = x; return x; }
static uint8_t CODE[98];
static int32_t mouth_ref(int64_t top, int64_t den) { int64_t q = top / den, r = top - q * den, a = r < 0 ? -r : r; if (2 * a > den || (2 * a == den && (q & 1))) q += top < 0 ? -1 : 1; return (int32_t)q; }

// ---------------- OS 2 as it is
namespace os2 {
#define PC_THETA (-1)
static int16_t pc_mouth(int32_t t, int32_t d) { return (int16_t)mouth_ref(t, d); }
static int32_t pc_mouth32(int32_t t, int32_t d) { return mouth_ref(t, d); }
static int8_t pc_way(int16_t x) { return (int8_t)((x > 0) - (x < 0)); }
static int16_t pc_scatter(int16_t lo, int16_t hi) { return (int16_t)(lo + (int16_t)(scat() % (uint16_t)(hi - lo))); }
typedef struct { int16_t v, r, d; } Rd;
typedef struct { Rd reg[8]; uint16_t base; uint8_t pc, n, state, file, budget, used, sleep, cut; int8_t pen_x, pen_y; } Proc;
#define ST_FREE 0
#define ST_RUN 1
#define ST_STOP 2
#define ST_END 3
#define ST_THETA 4
#define ST_WIDE 5
#define ST_LOST 6
#include "op_defs.inc"
static int8_t rk_sx = 0, rk_sy = 0; static uint8_t rk_held = 0; static Rd cell[4];
static uint8_t RK_EE_RD(uint16_t at) { return CODE[at - 1]; }   // base 0: the file's byte i is at base + 1 + i
static void win_cls(void) { tr(0xC15); }
static void win_dot(int16_t x, int16_t y) { if (x < 0 || x > 61 || y < 0 || y > 25) return; tr(0xD07); tr((uint16_t)x); tr((uint16_t)y); }
static void win_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) { if (x0 < -99 || x0 > 160 || x1 < -99 || x1 > 160 || y0 < -99 || y0 > 125 || y1 < -99 || y1 > 125) return; tr(0x11E); tr((uint16_t)x0); tr((uint16_t)y0); tr((uint16_t)x1); tr((uint16_t)y1); }
#include "os2_vm.inc"
#undef ST_FREE
#undef ST_RUN
#undef ST_STOP
#undef ST_END
#undef ST_THETA
#undef ST_WIDE
#undef ST_LOST
#undef PC_THETA
}
// OS 2's OP_NUM drew digits with win_count; the harness traces the reading it was given instead (patched below by name)
// ---------------- OS 3 as it is
namespace os3 {
typedef struct { int32_t v, r, d; } Rd;
typedef struct { Rd reg[8]; uint8_t pc, n, state, file, budget, used, sleep, cut, win; int16_t pen_x, pen_y; uint32_t steps; uint8_t code[98]; } Proc;
typedef struct { int jx, jy; } Pad;
#define ST_FREE 0
#define ST_RUN 1
#define ST_STOP 2
#define ST_WAIT 3
#define ST_END 4
#define ST_THETA 5
#define ST_WIDE 6
#define ST_LOST 7
#include "op_defs.inc"
#define C_INK 32
static int32_t mouth(int32_t t, int32_t d) { return mouth_ref(t, d); }
static int64_t mouth64(int64_t top, int64_t den) { int64_t q = top / den, r = top - q * den, a = r < 0 ? -r : r; if (2 * a > den || (2 * a == den && (q & 1))) q += top < 0 ? -1 : 1; return q; }
static uint32_t scatter(void) { return scat(); }
static Pad pad; static int down; static Rd cell[4]; static uint8_t *pw_p = (uint8_t *)1; static const int PW = 124, PH = 60, C_WIN = 21;
static void pw_dot(int32_t x, int32_t y, uint8_t c) { (void)c; if (x < 0 || x > 61 || y < 0 || y > 25) return; tr(0xD07); tr((uint16_t)x); tr((uint16_t)y); }
static void pw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c) { (void)c; if (x0 < -99 || x0 > 160 || x1 < -99 || x1 > 160 || y0 < -99 || y0 > 125 || y1 < -99 || y1 > 125) return; tr(0x11E); tr((uint16_t)x0); tr((uint16_t)y0); tr((uint16_t)x1); tr((uint16_t)y1); }
static void pw_num(int x, int y, const Rd *a, uint8_t c) { (void)c; tr(0x2B); tr((uint64_t)(int64_t)a->v); tr((uint64_t)(int64_t)a->d); tr((uint64_t)(x / 8 + 16 * (y / 12))); }
static void memset_(void *, int, int) { tr(0xC15); }
#define memset(p, c, n) memset_(p, c, n)
#include "os3_vm.inc"
#undef memset
}
// ---------------- the kernel, both widths
#define KDECL(NS) \
  static uint8_t k_code(Proc *p, uint8_t i) { (void)p; return CODE[i]; } \
  static void k_enter(uint8_t s) { (void)s; } \
  static void k_cls(void) { tr(0xC15); } \
  static void k_dot(int32_t x, int32_t y) { if (x < 0 || x > 61 || y < 0 || y > 25) return; tr(0xD07); tr((uint16_t)x); tr((uint16_t)y); } \
  static void k_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1) { if (x0 < -99 || x0 > 160 || x1 < -99 || x1 > 160 || y0 < -99 || y0 > 125 || y1 < -99 || y1 > 125) return; tr(0x11E); tr((uint16_t)x0); tr((uint16_t)y0); tr((uint16_t)x1); tr((uint16_t)y1); } \
  static void k_num(const Rd *a, uint8_t at) { tr(0x2B); tr((uint64_t)(int64_t)a->v); tr((uint64_t)(int64_t)a->d); tr(at); } \
  static void k_stick(Rd *a, Rd *b) { a->v = 0; a->r = 0; a->d = 1; b->v = 0; b->r = 0; b->d = 1; } \
  static uint8_t k_btn(void) { return 0; } \
  static int32_t k_rand(int32_t n) { return (int32_t)(scat() % (uint32_t)n); }
namespace k16 {
#define NPL 1
#define PROC_EXTRA
#include "../kernel.h"
KDECL(k16)
}
#undef KTOP_MAX
#undef KHEAD_MAX
namespace k32 {
#define KTOP32
#include "../kernel.h"
KDECL(k32)
}
// ---------------- runs
template <class P> static void load(P &p) { (void)p; }
static void load(os3::Proc &p) { memcpy(p.code, CODE, 98); }
typedef struct { int st; uint8_t pc; int64_t v[8], r[8], d[8]; uint64_t tr; int steps; } End;
template <class P> static void snap(const P &p, uint64_t *h) { for (int i = 0; i < 8; i++) { tr((uint64_t)(int64_t)p.reg[i].v); tr((uint64_t)(int64_t)p.reg[i].r); tr((uint64_t)(int64_t)p.reg[i].d); } tr(p.pc); (void)h; }
#define RUN(NS, STMAP, CALL)                                                             \
  static End run_##NS(int max) {                                                           \
    NS::Proc p; memset(&p, 0, sizeof p); load(p); p.n = CODE[0] > 48 ? 48 : CODE[0];                \
    for (int i = 0; i < 8; i++) p.reg[i].d = 1; for (int c = 0; c < 4; c++) { memset(&NS::cell[c], 0, sizeof NS::cell[c]); NS::cell[c].d = 1; } \
    TR = 1469598103934665603ull; SC = 0xACE1; int k = 0; p.state = 1;                      \
    for (; k < max && p.state == 1; k++) { CALL; if (p.sleep) p.sleep = 0; snap(p, 0); }   \
    End e; e.st = STMAP(p.state); e.pc = p.pc; e.tr = TR; e.steps = k;                     \
    for (int i = 0; i < 8; i++) { e.v[i] = p.reg[i].v; e.r[i] = p.reg[i].r; e.d[i] = p.reg[i].d; } return e; }
static int map2(int s) { return s == 3 ? 4 : s == 4 ? 5 : s == 5 ? 6 : s == 6 ? 7 : s; }   // OS 2's state numbers onto OS 3's
static int id(int s) { return s; }
RUN(os2, map2, os2::vm_step(&p))
RUN(os3, id, os3::vm_step(&p, 0))
RUN(k16, id, k16::vm_step(&p))
RUN(k32, id, k32::vm_step(&p))
static int same(const End &a, const End &b) { if (a.st != b.st || a.pc != b.pc || a.tr != b.tr || a.steps != b.steps) return 0; for (int i = 0; i < 8; i++) if (a.v[i] != b.v[i] || a.r[i] != b.r[i] || a.d[i] != b.d[i]) return 0; return 1; }
static const uint8_t DEMO[8][98] = {
#include "demo8.inc"
};
static const char *NAME[8] = {"BOUNCE", "ROOT", "SLOPE", "TURN", "MEET", "E", "CUBE", "NEW"};
static uint32_t RS = 12345; static uint32_t rnd(void) { RS ^= RS << 13; RS ^= RS >> 17; RS ^= RS << 5; return RS; }
static void dbg(int n) { RS = 12345; for (int m = 0; m <= n; m++) { memset(CODE, 0, 98); int ns = 4 + (int)(rnd() % 30); CODE[0] = (uint8_t)ns;
    for (int i = 0; i < ns; i++) { uint8_t op = (uint8_t)(rnd() % 32); if (op == 17 || op == 27) op = 7; CODE[1 + 2 * i] = (uint8_t)(op << 3 | (rnd() & 7)); uint8_t b1 = (uint8_t)rnd(); if (op == 18 || op == 19) b1 = (uint8_t)(rnd() % ns); if (op == 1 || op == 2 || op == 3) b1 = (uint8_t)((int8_t)(rnd() % 13) - 6); CODE[2 + 2 * i] = b1; } }
  for (int k = 1; k < 600; k++) { End a = run_os2(k), b = run_k16(k); if (!same(a, b)) { int pc = run_os2(k - 1).pc; printf("first differs at step %d, pc %d op %d b0 %d b1 %d; os2 st %d k16 st %d\n", k, pc, CODE[1+2*pc]>>3, CODE[1+2*pc], CODE[2+2*pc], a.st, b.st);
      for (int i = 0; i < 8; i++) if (a.v[i]!=b.v[i]||a.r[i]!=b.r[i]||a.d[i]!=b.d[i]) printf(" slot %d os2 (%lld;%lld)/%lld k16 (%lld;%lld)/%lld\n", i, (long long)a.v[i],(long long)a.r[i],(long long)a.d[i],(long long)b.v[i],(long long)b.r[i],(long long)b.d[i]);
      End c = run_os2(k-1); for (int i = 0; i < 8; i++) printf("  before slot %d (%lld;%lld)/%lld\n", i, (long long)c.v[i],(long long)c.r[i],(long long)c.d[i]); return; } } }
int main(int argc, char **argv) { if (argc > 2) { dbg(atoi(argv[2])); return 0; }
  int N = argc > 1 ? atoi(argv[1]) : 20000, MAX = 600;
  long bad2 = 0, bad3 = 0, badw = 0, wide16 = 0, beyond = 0, okw = 0;
  for (int f = 0; f < 8; f++) { memcpy(CODE, DEMO[f], 98);
    End a = run_os2(MAX), b = run_k16(MAX), c = run_os3(MAX), d = run_k32(MAX);
    printf("%-7s os2==k16 %s  os3==k32 %s  | k16 %s at step %d pc %d   k32 %s at step %d\n", NAME[f], f < 5 ? (same(a, b) ? "yes" : "NO") : "(not an OS 2 file)", same(c, d) ? "yes" : "NO",
      b.st == 6 ? "WIDE" : b.st == 1 ? "running" : b.st == 4 ? "END" : "stop", b.steps, b.pc, d.st == 6 ? "WIDE" : d.st == 1 ? "running" : d.st == 4 ? "END" : "stop", d.steps); }
  for (int n = 0; n < N; n++) {
    memset(CODE, 0, 98); int ns = 4 + (int)(rnd() % 30); CODE[0] = (uint8_t)ns;
    for (int i = 0; i < ns; i++) { uint8_t op = (uint8_t)(rnd() % 32); if (op == 17 || op == 27) op = 7;   // IFBTN / STICK read hands: kept out (all four read nothing the same way)
      CODE[1 + 2 * i] = (uint8_t)(op << 3 | (rnd() & 7)); uint8_t b1 = (uint8_t)rnd(); if (op == 18 || op == 19) b1 = (uint8_t)(rnd() % ns); if (op == 1 || op == 2 || op == 3) b1 = (uint8_t)((int8_t)(rnd() % 13) - 6); CODE[2 + 2 * i] = b1; }
    End a = run_os2(MAX), b = run_k16(MAX), c = run_os3(MAX), d = run_k32(MAX);
    if (!same(a, b)) { bad2++; if (bad2 < 4) printf("os2!=k16 on random %d\n", n); }
    if (!same(c, d)) { bad3++; if (bad3 < 4) printf("os3!=k32 on random %d\n", n); }
    // width: up to the step where TOP16 stopped, TOP32 must have walked the same path. Rerun TOP32 for exactly that many steps.
    if (b.st == 6) { wide16++; End e = run_k32(b.steps - 1); End g = run_k16(b.steps - 1);
      if (!same(e, g)) { badw++; if (badw < 4) printf("width differs before WIDE on random %d\n", n); } else okw++;
      if (d.steps > b.steps) beyond++; }
    else if (!same(b, d)) { badw++; if (badw < 4) printf("k16 != k32 with no WIDE on random %d\n", n); } else okw++; }
  printf("random programs %d: os2!=k16 %ld  os3!=k32 %ld  | TOP16 refused WIDE on %ld; TOP32 went further on %ld; width disagreement before any WIDE: %ld (agreed %ld)\n", N, bad2, bad3, wide16, beyond, badw, okw);
  return (bad2 || bad3 || badw) ? 1 : 0; }
