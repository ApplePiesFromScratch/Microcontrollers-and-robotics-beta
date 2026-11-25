/* c3k.c  the shared kernel on the ESP32-C3's own instruction set (bare, in Espressif's QEMU) and on the desk, from one source.
   Four places run OS 3's files ROOT, E, CUBE and a hog (COUNT) through the flux for N latches; then 2,000 random programs run alone.
   Every latch prints the flux readings; every random program prints a print of its end state. chip == desk is these lines, compared. */
#include <stdint.h>
typedef uint32_t u32; typedef int32_t i32;
#define KTOP32
#define NPL 4
#define PROC_EXTRA uint8_t code[98];
#include "../kernel.h"
#ifdef DESK
#include <stdio.h>
static void out(const char *s) { fputs(s, stdout); }
#else
#define REG(a) (*(volatile u32 *)(a))
static void quiet_watchdogs(void) {
  REG(0x600080A8) = 0x50D83AA1u; REG(0x60008090) = 0; REG(0x600080A8) = 0; REG(0x600080B0) = 0x8F1D312Au; REG(0x600080AC) |= 1u << 31; REG(0x600080B0) = 0;
  REG(0x6001F064) = 0x50D83AA1u; REG(0x6001F048) = 0; REG(0x6001F064) = 0; REG(0x60020064) = 0x50D83AA1u; REG(0x60020048) = 0; REG(0x60020064) = 0; }
static void uart_char(u32 c) { while (((REG(0x6000001C) >> 16) & 0x3FF) > 120) { } REG(0x60000000) = c; }
static void out(const char *s) { while (*s) uart_char((u32)*s++); }
#endif
static void out_u(u32 n) { char b[11]; i32 i = 10; b[i] = 0; do { b[--i] = (char)('0' + n % 10); n /= 10; } while (n); out(b + i); }
static void out_x(u32 n) { char b[9]; for (i32 i = 7; i >= 0; i--) { u32 d = n & 15; b[i] = (char)(d < 10 ? '0' + d : 'a' + d - 10); n >>= 4; } b[8] = 0; out(b); }
static u32 TR; static void tr(u32 v) { TR = (TR ^ v) * 0x01000193u; }
static uint16_t SC = 0xACE1;
static uint8_t k_code(Proc *p, uint8_t i) { return p->code[i]; }
static void k_enter(uint8_t s) { tr(0xE0 + s); }
static void k_cls(void) { tr(0xC15); }
static u32 cyc(void) {
#ifdef DESK
  return 0;
#else
  u32 v; __asm__ volatile("csrr %0, 0x7e2" : "=r"(v)); return v;
#endif
}
static u32 cmax, csum;
static void k_dot(int32_t x, int32_t y) { tr(0xD07); tr((u32)x); tr((u32)y); }
static void k_line(int32_t a, int32_t b, int32_t c, int32_t d) { tr(0x11E); tr((u32)a); tr((u32)b); tr((u32)c); tr((u32)d); }
static void k_num(const Rd *a, uint8_t at) { tr(0x2B); tr((u32)a->v); tr((u32)a->d); tr(at); }
static void k_stick(Rd *a, Rd *b) { a->v = b->v = 0; a->r = b->r = 0; a->d = b->d = 1; }
static uint8_t k_btn(void) { return 0; }
static int32_t k_rand(int32_t n) { uint16_t x = SC; x ^= x << 7; x ^= x >> 9; x ^= x << 8; SC = x; return x % n; }
static const uint8_t DEMO[8][98] = {
#include "../tools/demo8.inc"
};
static const uint8_t COUNT[98] = {5, 8, 0, 9, 1, 40, 1, 200, 0, 152, 2};
static void load(int s, const uint8_t *c) { uint8_t *q = (uint8_t *)&proc[s]; for (unsigned i = 0; i < sizeof(Proc); i++) q[i] = 0;
  for (int i = 0; i < 98; i++) proc[s].code[i] = c[i]; proc[s].n = c[0]; for (int i = 0; i < 8; i++) proc[s].reg[i].d = 1; proc[s].state = ST_RUN; }
static u32 rs = 12345; static u32 rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static void run(void) {
  k_begin(); rules.S = 3000; for (int c = 0; c < 4; c++) cell[c].d = 1;
  load(0, DEMO[1]); load(1, DEMO[5]); load(2, DEMO[6]); load(3, COUNT);
  for (u32 l = 1; l <= 400; l++) { if (l == 150) load(2, DEMO[0]); u32 c0 = cyc(); k_latch(); u32 dc = cyc() - c0; csum += dc; if (dc > cmax) cmax = dc;
    out("L "); out_u(l); out(" beats "); out_u(k_beats); for (int s = 0; s < 4; s++) { out(" "); out_u(flow[s].took); out("/"); out_u(flow[s].L); }
    out(" field "); out_u(field); out(" recon "); out_u(k_recon); out(" books "); out(k_books() ? "shut" : "OPEN"); out(" trace "); out_x(TR);
    TR = 0; for (int s = 0; s < 4; s++) for (int i = 0; i < 8; i++) { tr((u32)proc[s].reg[i].v); tr((u32)proc[s].reg[i].r); tr((u32)proc[s].reg[i].d); } out(" slots "); out_x(TR); out("\n"); }
  u32 all = 0x811c9dc5u, wide = 0;
  for (int n = 0; n < 2000; n++) { uint8_t c[98] = {0}; int ns = 4 + (int)(rnd() % 30); c[0] = (uint8_t)ns;
    for (int i = 0; i < ns; i++) { uint8_t op = (uint8_t)(rnd() % 32); if (op == 17 || op == 27) op = 7; c[1 + 2 * i] = (uint8_t)(op << 3 | (rnd() & 7)); uint8_t b1 = (uint8_t)rnd(); if (op == 18 || op == 19) b1 = (uint8_t)(rnd() % ns); if (op == 1 || op == 2 || op == 3) b1 = (uint8_t)((int8_t)(rnd() % 13) - 6); c[2 + 2 * i] = b1; }
    load(0, c); TR = 0x811c9dc5u; SC = 0xACE1;
    for (int k = 0; k < 600 && proc[0].state == ST_RUN; k++) { vm_step(&proc[0]); proc[0].sleep = 0; }
    for (int i = 0; i < 8; i++) { tr((u32)proc[0].reg[i].v); tr((u32)proc[0].reg[i].r); tr((u32)proc[0].reg[i].d); } tr(proc[0].state); tr(proc[0].pc);
    if (proc[0].state == ST_WIDE) wide++; all = (all ^ TR) * 0x01000193u; }
  out("C latch cycles avg "); out_u(csum / 400); out(" worst "); out_u(cmax); out("\n"); out("RANDOM 2000 programs print "); out_x(all); out(" wide "); out_u(wide); out("\nLEAKS "); out_u(k_leaks); out("\nDONE\n"); }
#ifdef DESK
int main(void) { run(); return 0; }
#else
void *memset(void *d, int c, unsigned n) { uint8_t *p = (uint8_t *)d; while (n--) *p++ = (uint8_t)c; return d; }
void *memcpy(void *d, const void *s, unsigned n) { uint8_t *p = (uint8_t *)d; const uint8_t *q = (const uint8_t *)s; while (n--) *p++ = *q++; return d; }
int main(void) { quiet_watchdogs(); __asm__ volatile("csrwi 0x7e0, 1\n csrwi 0x7e1, 1"); out("OS4 kernel on the C3\n"); run(); for (;;) { } }
#endif
