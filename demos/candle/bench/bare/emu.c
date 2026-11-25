/* emu.c  CANDLE on the chip's own instruction set, bare (Espressif QEMU): no panel, the wire's bytes printed.
   At boot: the room's print. Every frame: the flicker's print, a print of every byte down the wire, and the instructions each part took. */
#include <stdint.h>
#include <stddef.h>
void *memset(void *d, int c, size_t n) { uint8_t *p = (uint8_t *)d; uint32_t w = (uint8_t)c * 0x01010101u;
  while (n && ((uintptr_t)p & 3)) { *p++ = (uint8_t)c; n--; } while (n >= 4) { *(uint32_t *)p = w; p += 4; n -= 4; } while (n--) *p++ = (uint8_t)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { uint8_t *a = (uint8_t *)d; const uint8_t *b = (const uint8_t *)s;
  if ((((uintptr_t)a | (uintptr_t)b) & 3) == 0) { while (n >= 4) { *(uint32_t *)a = *(const uint32_t *)b; a += 4; b += 4; n -= 4; } } while (n--) *a++ = *b++; return d; }
void *memmove(void *d, const void *s, size_t n) { uint8_t *a = (uint8_t *)d; const uint8_t *b = (const uint8_t *)s; if (a <= b || a >= b + n) return memcpy(d, s, n);
  a += n; b += n; if ((((uintptr_t)a | (uintptr_t)b) & 3) == 0) { while (n >= 4) { a -= 4; b -= 4; *(uint32_t *)a = *(const uint32_t *)b; n -= 4; } } while (n--) *--a = *--b; return d; }
#define REG(a) (*(volatile uint32_t *)(a))
static void uart_char(uint32_t c) { while (((REG(0x6000001C) >> 16) & 0x3FF) > 120) { } REG(0x60000000) = c; }
static void say(const char *s) { while (*s) uart_char((uint32_t)*s++); }
static void quiet_watchdogs(void) {
  REG(0x600080A8) = 0x50D83AA1u; REG(0x60008090) = 0; REG(0x600080A8) = 0; REG(0x600080B0) = 0x8F1D312Au; REG(0x600080AC) |= 1u << 31; REG(0x600080B0) = 0;
  REG(0x6001F064) = 0x50D83AA1u; REG(0x6001F048) = 0; REG(0x6001F064) = 0; REG(0x60020064) = 0x50D83AA1u; REG(0x60020048) = 0; REG(0x60020064) = 0; }
static uint32_t cyc(void) { uint32_t v; __asm__ volatile("csrr %0, 0x7e2" : "=r"(v)); return v; }
#define FASTF
static uint32_t wire_print = 2166136261u, cyc_hash;
static void wire_cmd(uint8_t c, const uint8_t *p, int n) { wire_print = (wire_print ^ (0x100u | c)) * 16777619u; for (int k = 0; k < n; k++) wire_print = (wire_print ^ p[k]) * 16777619u; }
static void wire_data(const void *v, int nb) { uint32_t c0 = cyc(); const uint8_t *p = (const uint8_t *)v; for (int k = 0; k < nb; k++) wire_print = (wire_print ^ p[k]) * 16777619u; cyc_hash += cyc() - c0; }
static void wire_mark(int s) { (void)s; } static void wire_hold(int s) { (void)s; }
static uint32_t clock_now(void) { return cyc(); }
#include "../candle.h"
static void put_u(uint32_t n) { char b[11]; int i = 10; b[i] = 0; do { b[--i] = (char)('0' + n % 10); n /= 10; } while (n); say(b + i); }
static void put_x(uint32_t n) { char b[9]; for (int i = 7; i >= 0; i--) { int d = n & 15; b[i] = (char)(d < 10 ? '0' + d : 'a' + d - 10); n >>= 4; } b[8] = 0; say(b); }
#ifndef FRAMES
#define FRAMES 600
#endif
void main(void) {
  quiet_watchdogs(); __asm__ volatile("csrwi 0x7e0, 1\n csrwi 0x7e1, 1");
  say("EMU CANDLE\n"); uint32_t b0 = cyc(); candle_begin(); uint32_t b1 = cyc();
  say("ROOM "); put_x(room_print()); say(" cells "); put_u(boot_cells); say(" boot "); put_u(b1 - b0); say("\n");
  for (uint32_t k = 1; k <= FRAMES; k++) { wire_print = 2166136261u; cyc_hash = 0; uint32_t c0 = cyc(); frame(); uint32_t c1 = cyc() - cyc_hash;
    say("F "); put_u(k); say(" story "); put_x(candle_print()); say(" wire "); put_u(wire_print); say(" cycles "); put_u(c1 - c0); say(" half "); put_u(cyc_half); say(" full "); put_u(cyc_full - cyc_hash); say("\n"); }
  say("DONE\n"); for (;;) { } }
