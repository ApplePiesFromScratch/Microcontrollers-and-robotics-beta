/* emu.c  WARD on the C3's own instruction set (Espressif QEMU), bare: prints what ward.py trace prints, plus cycles */
#include <stdint.h>
#include <stddef.h>
#define REG(a) (*(volatile uint32_t *)(a))
static void uart_char(uint32_t c) { while (((REG(0x6000001C) >> 16) & 0x3FF) > 120) { } REG(0x60000000) = c; }
static void say(const char *s) { while (*s) uart_char((uint32_t)*s++); }
static void quiet_watchdogs(void) {
  REG(0x600080A8) = 0x50D83AA1u; REG(0x60008090) = 0; REG(0x600080A8) = 0; REG(0x600080B0) = 0x8F1D312Au; REG(0x600080AC) |= 1u << 31; REG(0x600080B0) = 0;
  REG(0x6001F064) = 0x50D83AA1u; REG(0x6001F048) = 0; REG(0x6001F064) = 0; REG(0x60020064) = 0x50D83AA1u; REG(0x60020048) = 0; REG(0x60020064) = 0; }
static uint32_t cyc(void) { uint32_t v; __asm__ volatile("csrr %0, 0x7e2" : "=r"(v)); return v; }
#include "../core/irori.h"
#ifdef START_PROFILE
#define START_IT profile_start(START_PROFILE)
#else
#define START_IT (void)0
#endif
#include "../sim/hal_sim.h"
static void put_u(uint32_t n) { char b[11]; int i = 10; b[i] = 0; do { b[--i] = (char)('0' + n % 10); n /= 10; } while (n); say(b + i); }
static void put_i(int32_t n) { if (n < 0) { say("-"); put_u((uint32_t)-n); } else put_u((uint32_t)n); }
static void put_x(uint32_t n) { char b[9]; for (int i = 7; i >= 0; i--) { int d = n & 15; b[i] = (char)(d < 10 ? '0' + d : 'a' + d - 10); n >>= 4; } b[8] = 0; say(b); }
static void put_x4(uint32_t n) { char b[5]; for (int i = 3; i >= 0; i--) { int d = n & 15; b[i] = (char)(d < 10 ? '0' + d : 'a' + d - 10); n >>= 4; } b[4] = 0; say(b); }
void main(void) {
  quiet_watchdogs(); __asm__ volatile("csrwi 0x7e0, 1\n csrwi 0x7e1, 1");
  say("EMU IRORI\n"); os_begin(); START_IT; ghost_on = 1; uint32_t csum = 0, cmax = 0;
  for (uint32_t k = 1; k <= FRAMES; k++) { sim_latch = k; uint32_t c0 = cyc(); os_latch(); uint32_t d = cyc() - c0; csum += d; if (d > cmax) cmax = d;
    if (k % 50 == 0) { say("F "); put_u(k); say(" print "); put_x(fb_print()); say(" fg "); put_i(fg); say(" running "); put_x4(running); say(" focus "); put_i(home_focus); say(" kb "); put_i(kb_on); say("\n"); } }
  say("CYCLES per latch avg "); put_u(csum / FRAMES); say(" worst "); put_u(cmax); say("\nDONE\n"); for (;;) { } }
