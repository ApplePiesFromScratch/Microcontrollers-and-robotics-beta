/* emu.c  NAGARE on the C3's own instruction set (Espressif QEMU), bare: prints what desk.c prints, then times one turn of each copy */
#include <stdint.h>
#include <stddef.h>
#define REG(a) (*(volatile uint32_t *)(a))
static void uart_char(uint32_t c) { while (((REG(0x6000001C) >> 16) & 0x3FF) > 120) { } REG(0x60000000) = c; }
static void say(const char *s) { while (*s) uart_char((uint32_t)*s++); }
static void quiet_watchdogs(void) {
  REG(0x600080A8) = 0x50D83AA1u; REG(0x60008090) = 0; REG(0x600080A8) = 0; REG(0x600080B0) = 0x8F1D312Au; REG(0x600080AC) |= 1u << 31; REG(0x600080B0) = 0;
  REG(0x6001F064) = 0x50D83AA1u; REG(0x6001F048) = 0; REG(0x6001F064) = 0; REG(0x60020064) = 0x50D83AA1u; REG(0x60020048) = 0; REG(0x60020064) = 0; }
static uint32_t cyc(void) { uint32_t v; __asm__ volatile("csrr %0, 0x7e2" : "=r"(v)); return v; }
#include "../core/nagare.h"
#include "../sim/hal_sim.h"
static void put_u(uint32_t n) { char b[11]; int i = 10; b[i] = 0; do { b[--i] = (char)('0' + n % 10); n /= 10; } while (n); say(b + i); }
static void put_i(int32_t n) { if (n < 0) { say("-"); put_u((uint32_t)-n); } else put_u((uint32_t)n); }
static void put_x(uint32_t n) { char b[9]; for (int i = 7; i >= 0; i--) { int d = n & 15; b[i] = (char)(d < 10 ? '0' + d : 'a' + d - 10); n >>= 4; } b[8] = 0; say(b); }
static void put_ll(int64_t v) { if (v < 0) { say("-"); v = -v; } char b[21]; int i = 20; b[i] = 0; do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v); say(b + i); }
void main(void) {
  quiet_watchdogs(); __asm__ volatile("csrwi 0x7e0, 1\n csrwi 0x7e1, 1");
  say("EMU NAGARE\n"); nagare_begin(); uint64_t st = 0, so = 0, sd = 0; uint32_t worst = 0;
  for (int k = 1; k <= FRAMES; k++) { uint32_t c0 = cyc(); hands(); if (ghost_on) ghost(); uint32_t c1 = cyc(); text_step(); uint32_t c2 = cyc(); ours_step(); uint32_t c3 = cyc(); frame_draw(); uint32_t c4 = cyc(); frames++;
    st += c2 - c1; so += c3 - c2; sd += c4 - c3; if (c4 - c0 > worst) worst = c4 - c0;
    if (k % 50 == 0) { say("F "); put_i(k); say(" print "); put_x(fb_print()); say(" step "); put_u(ours_latch); say(" books "); put_i(ours_books_shut()); say(" div "); put_i(t_div_max);
      say(" gap "); put_ll(text_heat() - (t_start + t_fed - t_vented)); say("\n"); } }
  say("CYCLES textbook "); put_u((uint32_t)(st / FRAMES)); say(" ours "); put_u((uint32_t)(so / FRAMES)); say(" drawing "); put_u((uint32_t)(sd / FRAMES)); say(" worst frame "); put_u(worst); say("\nDONE\n"); for (;;) { } }
