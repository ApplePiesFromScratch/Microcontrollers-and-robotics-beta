// os4.ino  RULE OS 4 for the Arduino Uno R3. ONE FILE. Paste all of it into an empty Arduino IDE sketch, pick board "Arduino Uno", upload.
// SH1106 128x64 OLED on SPI (SCK 13, MOSI 11, CS 10, DC 9, RST 8) and a KY-023 stick (VRx A0, VRy A1, SW 2), as OS 2.
// This is RuleKit.h, the shared kernel and the sketch joined, with every type first (the IDE needs that). Made by tools/make_ino.py.
// MAKE IT YOURS: the kernel's readings (C THETA G0 S KAPPA VENT DRAG) are edited on the RULES screen and kept; the programs are
// edited on the EDIT screen and kept; the starting readings are RULES_FIRST in the kernel; the factory programs are DEMO below.
#include <Arduino.h>
#include <stdint.h>
#define RK_FILES 5
#define RK_FILE_BYTES 98

// ---- every type, before any function ----
typedef struct { int8_t way; uint16_t quot, rem, den; } PcRate;
typedef struct { int16_t v, r; } PcPair;
typedef struct { int16_t p, q; } PcHeading;
typedef struct { int16_t c, s, d; } PcUnit;
typedef struct { void (*turn)(void); uint8_t every, wait, running; uint16_t turns; } RkTask;
#ifndef KTOP16
#define KTOP32                                       // 32-bit tops on the Uno: the same answers as the ESP32-C3 build (see README)
#endif
#define NPL 4
#define PROC_EXTRA uint16_t base;
// ================================================================ KERNEL (shared by the Uno and the ESP32-C3 builds, and by the desk)
// The machine is OS 2's: two-byte steps, 32 moves, eight slots, a slot a reading (value; rate) over one shared bottom in lowest terms.
// The flux is new. There is no budget and no priority number. Each place carries a LOADED HISTORY L (useful pattern, clause 1):
// every step it takes moves one unit out of the FIELD into its L. The place that runs next is the one whose next step is due soonest
// (OR = min, clause 6's tropical rule), and taking a step pushes its due mark on by C + L + drag (AND = +): so a place propagates at
// C / (C + L + drag) of the supply (clauses 2, 3). Drag is the L of every other place that touched the same shared cell this latch or
// the last (a relation found by interaction, never declared). A place that gives the machine back (SHOW, WAIT, END, a refusal) vents a
// share of its L back into the field (all of it, if it ended or was refused); a place whose L passes THETA reconfigures: it keeps KAPPA/16 of it as its core, vents the rest,
// and starts again from the front of the queue (clause 4: the vented simplicity outruns its source).
// Books, every latch: field + every L = G0. Nothing is made or lost; a leak is shown.
// Needs, defined before this file: KTOP32 (or not), NPL, PROC_EXTRA, and the hooks declared at the end of this block.
#include <stdint.h>
#ifdef KTOP32
typedef int32_t ktop; typedef int64_t kwide; typedef uint64_t kuwide;
#define KTOP_MAX 2147483647L
#define KHEAD_MAX 30000
#else
typedef int16_t ktop; typedef int32_t kwide; typedef uint32_t kuwide;
#define KTOP_MAX 32767
#define KHEAD_MAX 127
#endif
typedef struct { ktop v, r, d; } Rd;
typedef struct { Rd reg[8]; uint8_t pc, n, state, file, sleep; int16_t pen_x, pen_y; PROC_EXTRA } Proc;
typedef struct { uint32_t when; uint16_t L, took; uint8_t mask, last_mask, out; } Flow;   // a place's place in the flux
typedef struct { uint16_t C, THETA, G0, S; uint8_t KAPPA, VENT, DRAG, spare; } Rules;    // the kernel's own alphabet: 12 bytes, kept in a file


// RuleKit.h   one header for an Arduino Uno R3 + SH1106 128x64 OLED on hardware SPI (Jaycar XC3728) + KY-023 joystick.
// Whole numbers only: no float anywhere. Everything is `static`, so what your sketch does not use costs nothing.
//
//   #include <RuleKit.h>
//   void setup() { rk_begin(); }
//   void loop()  { rk_input(); oled_clear(); say(0, 0, P("HELLO")); rk_pointer(); rk_show(40); }
//
// OLED: SCK D13, MOSI D11, CS D10, DC (A0) D9, RST D8, VCC 5V, GND.   Joystick: +5V, GND, VRx A0, VRy A1, SW D2.
// PART 1 the engine (pc_...)   PART 2 the screen and words (oled_..., PC_PLOT, pc_line, put, say, num)   PART 3 the kit (rk_...)
#ifndef RULEKIT_H
#define RULEKIT_H
#ifdef __AVR__
#include <Arduino.h>
#endif
#include <stdint.h>

// ======================================================================================================================
// PART 1. The engine.
// ======================================================================================================================
/* pc.h   process calc for 8-bit boards (ATmega328: Uno, Nano).
   Integers only: int16 kept, int32 scratch. No float. No sine. No 64-bit. No table of waves.

   What replaces what, in a converted sketch:
     sine / cosine / "carrier wave" / phase_table   ->  PcUnit from a ring of headings: c*c + s*s == d*d exactly
     x / 16, x >> 4, (x + 500) / 1000               ->  tops kept over a named bottom; ONE pc_mouth at the screen
     float, sqrt                                    ->  fractions; pc_root with its leftover stated
     x += vx; if (wall) vx = -vx                    ->  PcPair (value; rate): pc_latch, pc_flip (flip the cut)
     phase = clock >> 2                             ->  pc_carry: the remainder is carried, not dropped
     (dx > 0) ? 1 : -1                              ->  pc_way: three answers. Still is not backward.
     random(), randomSeed(analogRead(0))            ->  pc_scatter: a stated rule from a stated seed, same on desk and board
     a / b with b possibly 0                        ->  pc_over: refuses with PC_THETA                                  */
#include <stdint.h>
#ifdef __AVR__
#include <avr/pgmspace.h>
#define PC_ROM PROGMEM
#define PC_ROM8(a) ((int8_t)pgm_read_byte(a))
#else
#define PC_ROM
#define PC_ROM8(a) (*(a))
#endif
#define PC_THETA (-1)
#define PC_SUB 16            /* world positions are tops over this shared bottom: sixteenths of a pixel */

/* ---- the mouth. An exact fraction top/den goes to the nearest whole number only here, on the way to a pixel.
        An exact half goes to the even neighbour, and -x gives exactly -(the answer for x).  den > 0.        ---- */
static inline int32_t pc_mouth32(int32_t top, int32_t den) {
  int32_t q = top / den, r = top - q * den;        /* C division: toward zero, so r carries top's direction */
  int32_t a = r < 0 ? -r : r;
  if (2 * a > den || (2 * a == den && (q & 1))) q += (top < 0) ? -1 : 1;
  return q;
}
static inline int16_t pc_mouth(int32_t top, int32_t den) { return (int16_t)pc_mouth32(top, den); }   /* for answers that fit a pixel count */

/* ---- which way: three answers, because a still thing is not a backward thing ---- */
static inline int8_t pc_way(int16_t x) { return (int8_t)((x > 0) - (x < 0)); }

/* ---- a rate answer: size and direction kept apart. quot + rem/den, all three sizes. ---- */
static inline int8_t pc_over(int16_t top, int16_t bot, PcRate *o) {
  if (bot == 0) return PC_THETA;                   /* nothing to land on */
  uint16_t a = (uint16_t)(top < 0 ? -top : top), b = (uint16_t)(bot < 0 ? -bot : bot);
  o->way = (int8_t)(pc_way(top) * pc_way(bot)); o->quot = a / b; o->rem = a % b; o->den = b;
  return 0;
}

/* ---- the carry: add n each cut, emit one pulse per whole d. acc is the remainder, the only state. ---- */
static inline uint8_t pc_carry(uint16_t *acc, uint16_t n, uint16_t d) {
  uint8_t k = 0; *acc += n;
  while (*acc >= d) { *acc -= d; k++; }
  return k;
}

/* ---- (value; rate), one slot. latch = one padd of the rate into the value. flip = the cut pointed the other way. ---- */
static inline void pc_latch(PcPair *a) { a->v += a->r; }
static inline void pc_flip(PcPair *a) { a->r = -a->r; }
static inline PcPair pc_mix(PcPair a, PcPair b) { PcPair o; o.v = a.v * b.v; o.r = a.r * b.v + a.v * b.r; return o; }

/* ---- heading (P : Q) and the unit it builds:  c = Q*Q - P*P,  s = 2*P*Q,  d = P*P + Q*Q.
        c*c + s*s == d*d for every pair, so every turn below is exact. |P|,|Q| <= 127 keeps c, s, d in int16. ---- */
static inline int8_t pc_unit(PcHeading h, PcUnit *u) {
  if (h.p == 0 && h.q == 0) return PC_THETA;       /* (0:0) is not a heading */
  u->c = h.q * h.q - h.p * h.p; u->s = 2 * h.p * h.q; u->d = h.q * h.q + h.p * h.p; return 0;
}
static inline int32_t pc_gcd(int32_t a, int32_t b) { if (a < 0) a = -a; if (b < 0) b = -b; while (b) { int32_t t = a % b; a = b; b = t; } return a; }
/* combine two headings (workbook Unit 14). If the result outgrows 127 it is snapped once, through the mouth,
   so (P:Q) and (-P:-Q) stay one heading. The snap moves the direction a hair; the unit pattern is untouched. */
static inline PcHeading pc_turn(PcHeading a, PcHeading b) {
  int32_t p = (int32_t)a.p * b.q + (int32_t)b.p * a.q, q = (int32_t)a.q * b.q - (int32_t)a.p * b.p, g = pc_gcd(p, q);
  if (g > 1) { p /= g; q /= g; }
  int32_t m = (p < 0 ? -p : p) > (q < 0 ? -q : q) ? (p < 0 ? -p : p) : (q < 0 ? -q : q);
  if (m > 127) { int32_t k = (m + 126) / 127; p = pc_mouth(p, k); q = pc_mouth(q, k); }
  PcHeading h; h.p = (int16_t)p; h.q = (int16_t)q; return h;
}

/* ---- rings: evenly stepped headings, found on the desk by an integer search (desk/ring_search.py):
        "the step whose n-th combine lands nearest the quarter turn", then the nearest small pair to each exact
        combine. The quarter turn itself is the heading (1:1): c = 0, s = 1, exactly.
        pc_unit64(k): k/64 of a full turn.  pc_unit12, pc_unit20 likewise.  The index wraps by itself.        ---- */
static const int8_t pc_ring16[32] PC_ROM = { 0,1, 3,61, 7,71, 15,101, 25,126, 1,4, 24,79, 24,67, 17,41, 9,19, 38,71, 3,5, 85,127, 26,35, 60,73, 69,76 };
static const int8_t pc_ring3[6]   PC_ROM = { 0,1, 7,26, 15,26 };
static const int8_t pc_ring5[10]  PC_ROM = { 0,1, 3,19, 13,40, 25,49, 8,11 };
static inline PcUnit pc_unit_ring(const int8_t *ring, uint8_t per_quarter, uint16_t k) {
  uint8_t i = (uint8_t)(k % per_quarter), quarter = (uint8_t)((k / per_quarter) & 3);
  PcHeading h; h.p = PC_ROM8(ring + 2 * i); h.q = PC_ROM8(ring + 2 * i + 1);
  PcUnit u = {1, 0, 1}; pc_unit(h, &u);
  while (quarter--) { int16_t t = u.c; u.c = -u.s; u.s = t; }      /* the quarter turn, exact */
  return u;
}
static inline PcUnit pc_unit64(uint16_t k) { return pc_unit_ring(pc_ring16, 16, k); }
static inline PcUnit pc_unit12(uint16_t k) { return pc_unit_ring(pc_ring3, 3, k); }
static inline PcUnit pc_unit20(uint16_t k) { return pc_unit_ring(pc_ring5, 5, k); }

/* ---- turn a point (x, z) by a unit. Outputs TOPS over the bottom u.d: nothing is rounded here. ---- */
static inline void pc_spin(PcUnit u, int16_t x, int16_t z, int32_t *xt, int32_t *zt) {
  *xt = (int32_t)x * u.c - (int32_t)z * u.s; *zt = (int32_t)x * u.s + (int32_t)z * u.c;
}

/* ---- the whole-number root, by the workbook's averaging rule run on whole numbers.
        Returns r with r*r <= n < (r+1)*(r+1), and the leftover n - r*r so the gap is stated, not hidden. ---- */
static inline uint16_t pc_root(uint32_t n, uint32_t *left) {
  uint32_t x = n, y = (n + 1) / 2;
  if (n < 2) { if (left) *left = 0; return (uint16_t)n; }
  while (y < x) { x = y; y = (x + n / x) / 2; }
  if (left) *left = n - x * x;
  return (uint16_t)x;
}

/* ---- scatter: where the old sketches called random(). A stated rule (three shifts) from a stated seed,
        so the desk run and the board run are the same run. pc_scatter(lo, hi) lands in lo .. hi-1.        ---- */
static uint16_t pc_scatter_state = 0xACE1u;
static inline int16_t pc_scatter(int16_t lo, int16_t hi) {
  uint16_t x = pc_scatter_state; x ^= x << 7; x ^= x >> 9; x ^= x << 8; pc_scatter_state = x;
  return (int16_t)(lo + (int16_t)(x % (uint16_t)(hi - lo)));       /* the remainder IS the answer here: which place in the span */
}

// ======================================================================================================================
// PART 2. The screen, lines, and words.
// ======================================================================================================================
/* pc_oled.h   the screen. 128x64 OLED on hardware SPI (SCK D13, MOSI D11, CS D10/PB2, DC D9/PB1, RST D8/PB0), 1024-byte frame.
   Written for the Jaycar XC3728 (SH1106: its memory is 132 columns wide, so each page starts at column 2).
   Borrowed, and named: the SPI port, the display's start-up bytes, delay() for the reset pulse. None of it is maths.
   On the desk (no __AVR__) the hardware calls vanish and the frame is read straight out of pc_frame.            */
#include <stdint.h>
uint8_t pc_frame[1024];
#define PC_PLOT(x, y) do { if ((unsigned)(x) < 128 && (unsigned)(y) < 64) pc_frame[(x) + ((y) >> 3) * 128] |= (uint8_t)(1 << ((y) & 7)); } while (0)
static inline void oled_clear(void) { for (uint16_t i = 0; i < 1024; i++) pc_frame[i] = 0; }
#ifdef __AVR__
#include <Arduino.h>
#include <avr/io.h>
static inline void oled_byte(uint8_t b) { SPDR = b; while (!(SPSR & (1 << SPIF))) { } }
static inline void oled_begin(void) {
  DDRB |= (1 << PB2) | (1 << PB1) | (1 << PB0);
  PORTB |= (1 << PB0); delay(10); PORTB &= ~(1 << PB0); delay(40); PORTB |= (1 << PB0); delay(50);
  DDRB |= (1 << PB3) | (1 << PB5); SPCR = (1 << SPE) | (1 << MSTR); SPSR = (1 << SPI2X);
  static const uint8_t init[] PROGMEM = { 0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xAF };
  PORTB &= ~((1 << PB1) | (1 << PB2));
  for (uint8_t i = 0; i < sizeof(init); i++) oled_byte(pgm_read_byte(init + i));
  PORTB |= (1 << PB2);
}
static inline void oled_flush(void) {
  for (uint8_t page = 0; page < 8; page++) {
    PORTB &= ~((1 << PB1) | (1 << PB2)); oled_byte(0xB0 + page); oled_byte(0x02); oled_byte(0x10); PORTB |= (1 << PB2);
    PORTB |= (1 << PB1); PORTB &= ~(1 << PB2);
    for (uint8_t col = 0; col < 128; col++) oled_byte(pc_frame[(uint16_t)page * 128 + col]);
    PORTB |= (1 << PB2);
  }
}
#else
static inline void oled_begin(void) { }
static inline void oled_flush(void) { }
static inline void delay(unsigned long ms) { (void)ms; }
#endif

/* The line is the carry rule (pc_carry) drawn: step along the long side, add the short side to the remainder,
   and step sideways once per whole long side. No slope is ever divided out.
   style 0: every 4th dot. 1: every 2nd. 2: solid. 3: solid and thick.                                         */
static inline void pc_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t style) {
  int16_t dx = x1 - x0, dy = y1 - y0;
  int8_t sx = (int8_t)((dx > 0) - (dx < 0)), sy = (int8_t)((dy > 0) - (dy < 0));
  uint16_t ax = (uint16_t)(dx < 0 ? -dx : dx), ay = (uint16_t)(dy < 0 ? -dy : dy);
  uint8_t steep = ay > ax; uint16_t d_long = steep ? ay : ax, d_short = steep ? ax : ay, acc = d_long >> 1;
  for (uint16_t step = 0; step <= d_long; step++) {
    if (style >= 2 || (style == 1 && !(step & 1)) || (style == 0 && !(step & 3))) {
      PC_PLOT(x0, y0);
      if (style == 3) { PC_PLOT(x0 + 1, y0); PC_PLOT(x0, y0 + 1); }
    }
    if (steep) y0 += sy; else x0 += sx;
    acc += d_short;
    if (acc >= d_long) { acc -= d_long; if (steep) x0 += sx; else y0 += sy; }
  }
}

#ifdef __AVR__
#define P(s) PSTR(s)
#define RD8(p) pgm_read_byte(p)
#define RD16(p) pgm_read_word(p)
extern char __data_load_end, __data_start, __bss_end;       // set by the linker: the true size of this image
#define FLASH_USED ((uint16_t)&__data_load_end)
#define RAM_STATIC ((uint16_t)(&__bss_end - &__data_start))
#else
#define P(s) (s)
#define RD8(p) (*(const uint8_t *)(p))
#define RD16(p) (*(const uint16_t *)(p))
#define PROGMEM
#define FLASH_USED 0
#define RAM_STATIC 0
static unsigned long desk_ms = 0;
static unsigned long millis(void) { return desk_ms += 83; }
static unsigned long micros(void) { return desk_ms * 1000; }
#endif
#ifdef DESK_LOG
#include <stdio.h>
#define DESK_LOG_START(w) printf("\n%d.", (int)(w))
#define DESK_LOG_DIGIT(d) putchar('0' + (d))
#else
#define DESK_LOG_START(w)
#define DESK_LOG_DIGIT(d)
#endif
#define PC_UNPLOT(x, y) do { if ((unsigned)(x) < 128 && (unsigned)(y) < 64) pc_frame[(x) + ((y) >> 3) * 128] &= (uint8_t)~(1 << ((y) & 7)); } while (0)

// ---------------------------------------------------------------- words on the screen: 3 x 5 dots a letter
static const uint16_t GLYPH[] PROGMEM = {
  0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7292, 0x7BEF, 0x7BCF,                 // 0-9
  0x2BED, 0x6BAE, 0x3923, 0x6B6E, 0x79A7, 0x79A4, 0x396B, 0x5BED, 0x7497, 0x126A, 0x5BAD, 0x4927, 0x5FED,   // A-M
  0x6B6D, 0x2B6A, 0x6BA4, 0x2B73, 0x6BAD, 0x388E, 0x7492, 0x5B6F, 0x5B6A, 0x5BFD, 0x5AAD, 0x5A92, 0x72A7,   // N-Z
  0x0000, 0x0002, 0x0410, 0x01C0, 0x05D0, 0x12A4, 0x52A5, 0x0E38, 0x2922, 0x2252 };                // space . : - + / % = ( )
static uint8_t glyph_of(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 10;
  switch (ch) { case '.': return 37; case ':': return 38; case '-': return 39; case '+': return 40; case '/': return 41; case '%': return 42; case '=': return 43; case '(': return 44; case ')': return 45; }
  return 36;
}
static uint8_t put(uint8_t x, uint8_t y, char ch) {
  uint16_t g = RD16(&GLYPH[glyph_of(ch)]);
  for (uint8_t r = 0; r < 5; r++) for (uint8_t c = 0; c < 3; c++) if (g & (0x4000 >> (r * 3 + c))) PC_PLOT(x + c, y + r);
  return x + 4;
}
static uint8_t say(uint8_t x, uint8_t y, const char *s) { char ch; while ((ch = (char)RD8(s++))) x = put(x, y, ch); return x; }
static uint8_t num(uint8_t x, uint8_t y, uint32_t v) {
  char b[10]; uint8_t n = 0;
  do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v);
  while (n) x = put(x, y, b[--n]);
  return x;
}


// ======================================================================================================================
// PART 3. The kit: ring, stick, pointer and buttons, tasks, files, the view rule, the frame.
// ======================================================================================================================
#ifndef RK_PIN_X
#define RK_PIN_X A0                                 // KY-023 joystick: VRx
#endif
#ifndef RK_PIN_Y
#define RK_PIN_Y A1                                 // VRy
#endif
#ifndef RK_PIN_SW
#define RK_PIN_SW 2                                 // SW (the press)
#endif
#ifndef RK_FLIP_X
#define RK_FLIP_X 0                                 // set to 1 before the #include if left and right come out swapped
#endif
#ifndef RK_FLIP_Y
#define RK_FLIP_Y 0
#endif

// ---- the ring: 64 exact headings round a full turn, landed once on a grid of 256 so the rest is whole-number adds and shifts
static int16_t RK_C[65], RK_S[65];
static void rk_ring_begin(void) {
  for (uint8_t k = 0; k < 64; k++) { PcUnit u = pc_unit64(k); RK_C[k] = pc_mouth(256L * u.c, u.d); RK_S[k] = pc_mouth(256L * u.s, u.d); }
  RK_C[64] = RK_C[0]; RK_S[64] = RK_S[0];
}
// A part of a turn (65536 = once round) to a unit on 256, filled in between ring neighbours. Call rk_ring_begin() first.
static void rk_unit(uint16_t part, int16_t *c, int16_t *s) {
  uint8_t i = (uint8_t)(part >> 10), f = (uint8_t)(part >> 2);
  *c = RK_C[i] + (((RK_C[i + 1] - RK_C[i]) * (int16_t)f) >> 8); *s = RK_S[i] + (((RK_S[i + 1] - RK_S[i]) * (int16_t)f) >> 8);
}

// ---- the stick: -4 .. 4 each way, with a dead middle so a resting stick reads 0
static int8_t rk_stick(uint8_t pin, uint8_t flip) {
  int16_t v = (int16_t)analogRead(pin) - 512;
  if (flip) v = -v;
  if (v > -70 && v < 70) return 0;
  return (int8_t)(v / 110);
}

// ---- pointer and buttons. Call rk_input() once at the top of loop(); then every rk_button() draws itself and tells you if it was clicked.
static int16_t rk_px = 256, rk_py = 128;            // the pointer, in quarter dots
static int8_t rk_sx = 0, rk_sy = 0;                 // the stick as read this frame
static uint8_t rk_clicked = 0, rk_held = 0, rk_was_down = 1, rk_settle = 20;
static void rk_input(void) {
  rk_sx = rk_stick(RK_PIN_X, RK_FLIP_X); rk_sy = rk_stick(RK_PIN_Y, RK_FLIP_Y);
  rk_px += 3 * rk_sx; rk_py += 3 * rk_sy;
  if (rk_px < 0) rk_px = 0; if (rk_px > 508) rk_px = 508; if (rk_py < 0) rk_py = 0; if (rk_py > 252) rk_py = 252;
  uint8_t down = digitalRead(RK_PIN_SW) == 0;
  if (rk_settle) { rk_settle--; down = 1; }         // the button is ignored for the first moments after power on
  rk_clicked = down && !rk_was_down; rk_held = down && !rk_settle; rk_was_down = down;
}
static void rk_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { for (uint8_t i = 0; i < w; i++) { PC_PLOT(x + i, y); PC_PLOT(x + i, y + h - 1); } for (uint8_t i = 0; i < h; i++) { PC_PLOT(x, y + i); PC_PLOT(x + w - 1, y + i); } }
static uint8_t rk_over(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { int16_t px = rk_px >> 2, py = rk_py >> 2; return px >= x && px < x + w && py >= y && py < y + h; }
static uint8_t rk_button(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char *label) __attribute__((noinline));
static uint8_t rk_button(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char *label) {
  uint8_t n = 0; while (RD8(label + n)) n++;
  rk_box(x, y, w, h); say((uint8_t)(x + (w - 4 * n + 1) / 2), (uint8_t)(y + (h - 5) / 2), label);
  uint8_t on = rk_over(x, y, w, h);
  if (on) { if (x && y) rk_box(x - 1, y - 1, w + 2, h + 2); else for (uint8_t i = 1; i + 1 < w; i++) PC_PLOT(x + i, y + h - 2); }
  return on && rk_clicked;
}
static const uint8_t RK_ARROW[7] PROGMEM = { 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x06, 0x0C };
static void rk_pointer(void) {                      // draw it last, so it is on top. It carries a dark edge so it shows on anything.
  int16_t x = rk_px >> 2, y = rk_py >> 2;
  for (int8_t r = -1; r <= 7; r++) for (int8_t c = -1; c <= 5; c++) {
    uint8_t in = (r >= 0 && r < 7 && c >= 0 && c < 5) ? (RD8(&RK_ARROW[r]) >> c) & 1 : 0;
    if (in) { PC_PLOT(x + c, y + r); continue; }
    uint8_t near = 0;
    for (int8_t rr = r - 1; rr <= r + 1 && !near; rr++) for (int8_t cc = c - 1; cc <= c + 1; cc++) if (rr >= 0 && rr < 7 && cc >= 0 && cc < 5 && ((RD8(&RK_ARROW[rr]) >> cc) & 1)) { near = 1; break; }
    if (near) PC_UNPLOT(x + c, y + r);
  }
}

// ---- tasks: a table of jobs. Each gets one turn every `every` frames while `running` is 1. A turn must be short and must return.
static void rk_take_turns(RkTask *task, uint8_t count) {
  for (uint8_t t = 0; t < count; t++) if (task[t].running && --task[t].wait == 0) { task[t].wait = task[t].every; task[t].turns++; task[t].turn(); }
}

// ---- files in the chip's lasting memory (EEPROM, 1,024 bytes on an Uno). RK_FILES files of RK_FILE_BYTES each, each kept twice.
#ifndef RK_FILES
#define RK_FILES 4
#endif
#ifndef RK_FILE_BYTES
#define RK_FILE_BYTES 64
#endif
#ifdef __AVR__
#include <avr/eeprom.h>
#define RK_EE_RD(a) eeprom_read_byte((const uint8_t *)(uint16_t)(a))
#define RK_EE_WR(a, v) eeprom_update_byte((uint8_t *)(uint16_t)(a), (v))
#else
static uint8_t rk_desk_lasting[1024]; static uint8_t rk_desk_lasting_ready = 0;
static uint8_t RK_EE_RD(uint16_t a) { if (!rk_desk_lasting_ready) { for (uint16_t i = 0; i < 1024; i++) rk_desk_lasting[i] = 0xFF; rk_desk_lasting_ready = 1; } return rk_desk_lasting[a]; }
static void RK_EE_WR(uint16_t a, uint8_t v) { RK_EE_RD(0); rk_desk_lasting[a] = v; }
#endif
#define RK_EMPTY 0
#define RK_SOUND 1
#define RK_BAD_A 2
#define RK_BAD_B 3
#define RK_LOST 4
#define RK_COPY (RK_FILE_BYTES + 2)                 // one copy: a "used" mark, the bytes, a check sum
static uint16_t rk_copy_at(uint8_t f, uint8_t b) { return (uint16_t)f * (2 * RK_COPY) + (b ? RK_COPY : 0); }
static uint8_t rk_sum_step(uint8_t c, uint8_t v) { return (uint8_t)(((c << 1) | (c >> 7)) ^ v); }
static uint8_t rk_copy_good(uint16_t at) {
  if (RK_EE_RD(at) != 0xA5) return 0;
  uint8_t c = 0x5A; for (uint8_t i = 0; i < RK_FILE_BYTES; i++) c = rk_sum_step(c, RK_EE_RD(at + 1 + i));
  return c == RK_EE_RD(at + 1 + RK_FILE_BYTES);
}
static uint8_t rk_file_state(uint8_t f) {
  uint16_t a = rk_copy_at(f, 0), b = rk_copy_at(f, 1); uint8_t ga = rk_copy_good(a), gb = rk_copy_good(b);
  if (ga && gb) { for (uint8_t i = 0; i < RK_COPY; i++) if (RK_EE_RD(a + i) != RK_EE_RD(b + i)) return RK_BAD_B; return RK_SOUND; }
  if (ga) return RK_BAD_B;
  if (gb) return RK_BAD_A;
  return (RK_EE_RD(a) == 0xA5 || RK_EE_RD(b) == 0xA5) ? RK_LOST : RK_EMPTY;
}
static void rk_file_write(uint8_t f, const uint8_t *data) {               // copy A first, then B: lose power in between and the keeper finishes from A
  for (uint8_t b = 0; b < 2; b++) {
    uint16_t at = rk_copy_at(f, b); uint8_t c = 0x5A;
    RK_EE_WR(at, 0xA5);
    for (uint8_t i = 0; i < RK_FILE_BYTES; i++) { RK_EE_WR(at + 1 + i, data[i]); c = rk_sum_step(c, data[i]); }
    RK_EE_WR(at + 1 + RK_FILE_BYTES, c);
  }
}
static uint8_t rk_file_read(uint8_t f, uint8_t *data) {                   // 1 if there was something to read
  uint8_t st = rk_file_state(f); if (st == RK_EMPTY || st == RK_LOST) return 0;
  uint16_t at = rk_copy_at(f, st == RK_BAD_A ? 1 : 0);
  for (uint8_t i = 0; i < RK_FILE_BYTES; i++) data[i] = RK_EE_RD(at + 1 + i);
  return 1;
}
static uint8_t rk_keeper_file = 0, rk_mended = 0;
static void rk_keeper_turn(void) {                                        // give this to a task: it looks at one file a turn and mends it from its good copy
  uint8_t f = rk_keeper_file; rk_keeper_file = (uint8_t)((rk_keeper_file + 1) % RK_FILES);
  uint8_t st = rk_file_state(f);
  if (st == RK_BAD_B) { for (uint8_t i = 0; i < RK_COPY; i++) RK_EE_WR(rk_copy_at(f, 1) + i, RK_EE_RD(rk_copy_at(f, 0) + i)); rk_mended++; }
  else if (st == RK_BAD_A) { for (uint8_t i = 0; i < RK_COPY; i++) RK_EE_WR(rk_copy_at(f, 0) + i, RK_EE_RD(rk_copy_at(f, 1) + i)); rk_mended++; }
  else if (st == RK_LOST) { RK_EE_WR(rk_copy_at(f, 0), 0xFF); RK_EE_WR(rk_copy_at(f, 1), 0xFF); }
}

// ---- the view rule, for 3D:  across = middle + 96 * side / depth     down = horizon + 96 * (eye height - height) / depth
static uint16_t rk_inv_of(int16_t depth) { return (uint16_t)(24576u / (uint16_t)(depth >> 4)); }        // one divide per thing. depth must be 16 or more.
static int16_t rk_proj(int16_t v, uint16_t inv) { return (int16_t)(((int32_t)(v >> 4) * inv) >> 8); }   // 96 * v / depth

// ---- start and finish
static unsigned long rk_last_ms = 0;
static void rk_begin(void) { oled_begin(); pinMode(RK_PIN_SW, INPUT_PULLUP); rk_ring_begin(); }
static void rk_show(uint8_t frame_ms) {             // send the picture, then wait so frames are no faster than frame_ms apart (40 = 25 a second)
  oled_flush();
#ifndef NO_PACE
  while ((uint16_t)(millis() - rk_last_ms) < frame_ms) { }
#endif
  (void)frame_ms; rk_last_ms = millis();
}
#endif

#define ST_FREE 0
#define ST_RUN 1
#define ST_STOP 2
#define ST_WAIT 3
#define ST_END 4
#define ST_THETA 5
#define ST_WIDE 6
#define ST_LOST 7
#define OP_END 0
#define OP_SET 1
#define OP_ISO 2
#define OP_SEED 3
#define OP_MOV 4
#define OP_ADD 5
#define OP_SUB 6
#define OP_MIX 7
#define OP_DIV 8
#define OP_RATE 9
#define OP_WHEN 10
#define OP_VAL 11
#define OP_LATCH 12
#define OP_FLIP 13
#define OP_LAND 14
#define OP_IFLT 15
#define OP_IFEQ 16
#define OP_IFBTN 17
#define OP_LOOP 18
#define OP_JMP 19
#define OP_SHOW 20
#define OP_CLS 21
#define OP_DOT 22
#define OP_PEN 23
#define OP_LINE 24
#define OP_NUM 25
#define OP_HEAD 26
#define OP_STICK 27
#define OP_RAND 28
#define OP_PUT 29
#define OP_GET 30
#define OP_WAIT 31

// hooks: each board says how a step reaches its glass, its hands and its program bytes
static uint8_t k_code(Proc *p, uint8_t i);
static void k_enter(uint8_t s);
static void k_cls(void);
static void k_dot(int32_t x, int32_t y);
static void k_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
static void k_num(const Rd *a, uint8_t at);
static void k_stick(Rd *a, Rd *b);
static uint8_t k_btn(void);
static int32_t k_rand(int32_t n);

static Proc proc[NPL];
static Flow flow[NPL];
static Rd cell[4];                                   // four shared cells: PUT and GET, the way one program hands a reading to another
static Rules rules;
static uint16_t field;                               // the gradient not yet carried by any place
static uint32_t k_latches, k_recon, k_spent, k_steps_all, k_leaks;
static uint16_t k_steps, k_beats;                    // steps, and beats, given out this latch

// ---------------------------------------------------------------- readings: exact fractions over one shared bottom
#ifdef __AVR__
#define K_NOINLINE __attribute__((noinline))
#else
#define K_NOINLINE
#endif
static kwide KV, KR, KD;                             // the three tops being landed: one scratch, not a copy on the stack at every depth
static K_NOINLINE kuwide k_gcd(kuwide a, kuwide b) { // by halving and taking away only; the 32-bit path when both fit (most of the time)
  if (!a) return b; if (!b) return a;
  if (a <= 0xFFFFFFFFu && b <= 0xFFFFFFFFu) {
    uint32_t x = (uint32_t)a, y = (uint32_t)b; uint8_t tw = 0;
    while (!((x | y) & 1)) { x >>= 1; y >>= 1; tw++; }
    while (!(x & 1)) x >>= 1;
    while (y) { while (!(y & 1)) y >>= 1; if (x > y) { uint32_t t = x; x = y; y = t; } y -= x; }
    return (kuwide)x << tw; }
  uint8_t tw = 0; while (!((a | b) & 1)) { a >>= 1; b >>= 1; tw++; }
  while (!(a & 1)) a >>= 1;
  while (b) { while (!(b & 1)) b >>= 1; if (a > b) { kuwide t = a; a = b; b = t; } b -= a; }
  return a << tw; }
static kuwide k_abs(kwide v) { return (kuwide)(v < 0 ? -v : v); }
static K_NOINLINE uint8_t rd_land(Rd *o) {          // KV KR KD into a slot, lowest terms; a top that does not fit is WIDE and the slot is left alone
  if (KD < 0) { KV = -KV; KR = -KR; KD = -KD; }
  if (KD > 1) { kuwide g = k_gcd((kuwide)KD, k_abs(KV)); if (g > 1) g = k_gcd(g, k_abs(KR)); if (g > 1) { KV /= (kwide)g; KR /= (kwide)g; KD /= (kwide)g; } }
  if (KV > KTOP_MAX || KV < -KTOP_MAX || KR > KTOP_MAX || KR < -KTOP_MAX || KD > KTOP_MAX) return ST_WIDE;
  o->v = (ktop)KV; o->r = (ktop)KR; o->d = (ktop)KD; return 0; }
static uint8_t rd_fit(kwide v, kwide r, kwide d, Rd *o) { KV = v; KR = r; KD = d; return rd_land(o); }
static K_NOINLINE uint8_t rd_mix(const Rd *a, const Rd *b, Rd *o) { KV = (kwide)a->v * b->v; KR = (kwide)a->r * b->v + (kwide)a->v * b->r; KD = (kwide)a->d * b->d; return rd_land(o); }
static K_NOINLINE uint8_t rd_add(const Rd *a, const Rd *b, int8_t way, Rd *o) {
  if (a->d == b->d) { KV = (kwide)a->v + (kwide)way * b->v; KR = (kwide)a->r + (kwide)way * b->r; KD = a->d; }
  else { KV = (kwide)a->v * b->d + way * ((kwide)b->v * a->d); KR = (kwide)a->r * b->d + way * ((kwide)b->r * a->d); KD = (kwide)a->d * b->d; }
  return rd_land(o); }
static K_NOINLINE uint8_t rd_back(const Rd *b, Rd *o) { if (b->v == 0) return ST_THETA; KV = (kwide)b->d * b->v; KR = -((kwide)b->r * b->d); KD = (kwide)b->v * b->v; return rd_land(o); }
static K_NOINLINE kwide k_mouth(kwide top, kwide den) {   // the only landing: nearest whole, half to even, symmetric about zero; den > 0 (wide: LAND only)
  kwide q = top / den, r = top - q * den, a = r < 0 ? -r : r;
  if (2 * a > den || (2 * a == den && (q & 1))) q += top < 0 ? -1 : 1;
  return q; }
static int32_t k_mouth32(int32_t top, int32_t den) { // the same landing, 32 bits: every count and every dot
  int32_t q = top / den, r = top - q * den, a = r < 0 ? -r : r;
  if (2 * a > den || (2 * a == den && (q & 1))) q += top < 0 ? -1 : 1;
  return q; }
static int32_t rd_whole(const Rd *a) { return k_mouth32(a->v, a->d); }

// ---------------------------------------------------------------- one step of one program; 1 when its turn is over for this latch
static uint8_t k_touch;                              // the cells this step touched (drag is found from these)
static K_NOINLINE uint8_t vm_step(Proc *p) {
  if (p->pc >= p->n) { p->state = ST_LOST; return 1; }
  uint8_t b0 = k_code(p, (uint8_t)(1 + 2 * p->pc)), b1 = k_code(p, (uint8_t)(2 + 2 * p->pc)), no = 0, next = (uint8_t)(p->pc + 1), over = 0;
  Rd *a = &p->reg[b0 & 7], *b = &p->reg[b1 & 7], t; int8_t imm = (int8_t)b1;
  switch (b0 >> 3) {
    case OP_END: p->state = ST_END; return 1;
    case OP_SET: a->v = imm; a->r = 0; a->d = 1; break;
    case OP_ISO: a->v = imm; a->r = 1; a->d = 1; break;
    case OP_SEED: no = rd_fit(a->v, (kwide)imm * a->d, a->d, a); break;
    case OP_MOV: *a = *b; break;
    case OP_ADD: no = rd_add(a, b, 1, a); break;
    case OP_SUB: no = rd_add(a, b, -1, a); break;
    case OP_MIX: no = rd_mix(a, b, a); break;
    case OP_DIV: no = rd_back(b, &t); if (!no) no = rd_mix(a, &t, a); break;
    case OP_RATE: if (b->r == 0) no = ST_THETA; else no = rd_fit((kwide)a->r * b->d, 0, (kwide)a->d * b->r, a); break;   // rate over rate: no value read
    case OP_WHEN: if (b->r == 0) no = ST_THETA; else no = rd_fit(-(kwide)b->v, 0, b->r, a); break;                        // latches until b runs out
    case OP_VAL: a->v = b->v; a->d = b->d; a->r = 0; break;
    case OP_LATCH: no = rd_fit((kwide)a->v + a->r, a->r, a->d, a); break;
    case OP_FLIP: a->r = (ktop)-a->r; break;
    case OP_LAND: if (imm <= 0) no = ST_THETA; else no = rd_fit(k_mouth((kwide)a->v * imm, a->d), k_mouth((kwide)a->r * imm, a->d), imm, a); break;
    case OP_IFLT: if (!((kwide)a->v * b->d < (kwide)b->v * a->d)) next++; break;                                           // by crossing, never dividing
    case OP_IFEQ: if ((kwide)a->v * b->d != (kwide)b->v * a->d) next++; break;
    case OP_IFBTN: if (!k_btn()) next++; break;
    case OP_LOOP: no = rd_fit((kwide)a->v - a->d, a->r, a->d, a); if (!no && a->v > 0) next = b1; break;
    case OP_JMP: next = b1; break;
    case OP_SHOW: over = 1; break;
    case OP_WAIT: p->sleep = b1; over = 1; break;
    case OP_CLS: k_cls(); break;
    case OP_DOT: k_dot(rd_whole(a), rd_whole(b)); break;
    case OP_PEN: p->pen_x = (int16_t)rd_whole(a); p->pen_y = (int16_t)rd_whole(b); break;
    case OP_LINE: { int32_t x = rd_whole(a), y = rd_whole(b); k_line(p->pen_x, p->pen_y, x, y); p->pen_x = (int16_t)x; p->pen_y = (int16_t)y; } break;
    case OP_NUM: k_num(a, b1); break;
    case OP_HEAD: { int32_t hp = rd_whole(a), hq = rd_whole(b);                                                               // heading (P : Q) to its exact unit
      if (hp == 0 && hq == 0) no = ST_THETA; else if (hp > KHEAD_MAX || hp < -KHEAD_MAX || hq > KHEAD_MAX || hq < -KHEAD_MAX) no = ST_WIDE;
      else { kwide bot = (kwide)hp * hp + (kwide)hq * hq; rd_fit((kwide)hq * hq - (kwide)hp * hp, 0, bot, a); rd_fit(2 * (kwide)hp * hq, 0, bot, b); } } break;
    case OP_STICK: k_stick(a, b); break;
    case OP_RAND: if (imm <= 0) no = ST_THETA; else { a->v = (ktop)k_rand(imm); a->r = 0; a->d = 1; } break;
    case OP_PUT: cell[b1 & 3] = *a; k_touch |= (uint8_t)(1 << (b1 & 3)); break;
    case OP_GET: *a = cell[b1 & 3]; k_touch |= (uint8_t)(1 << (b1 & 3)); break; }
  if (no) { p->state = no; return 1; }
  p->pc = next; return over; }

// ---------------------------------------------------------------- what a step costs: its weight in beats (256 cycles of the Uno)
// The worst cost of each move, measured on the chip by Timer1 around 31,962 single steps of random programs (tools/opcost), plus a
// stated quarter for what the bench did not reach, landed upward. A step's weight is what it moves from the field into its place's
// history, and what the latch's supply is counted in: so the supply bounds the latch's time, which a count of steps did not.
#ifdef __AVR__
#define K_ROM PROGMEM
#define K_ROM8(a) pgm_read_byte(a)
#else
#define K_ROM
#define K_ROM8(a) (*(a))
#endif
static const uint8_t OPW[32] K_ROM = { 2, 2, 2, 5, 3, 17, 13, 17, 36, 17, 4, 2, 5, 2, 31, 4, 4, 4, 5, 2, 2, 18, 10, 9, 61, 32, 38, 3, 3, 3, 3, 2 };
// ---------------------------------------------------------------- the flux: who steps next, and what each step carries
static void k_rules_first(Rules *r) { r->C = 64; r->THETA = 4000; r->G0 = 16000; r->S = 1200; r->KAPPA = 4; r->VENT = 6; r->DRAG = 4; r->spare = 0; }   // the first hand (see the README for how each was chosen)
static void k_vent(uint8_t s, uint16_t share16) {   // a share of the place's history goes back to the field (the mouth lands it)
  uint16_t L = flow[s].L, v = (uint16_t)k_mouth32((int32_t)L * share16, 16); if (v > L) v = L;
  flow[s].L = (uint16_t)(L - v); field = (uint16_t)(field + v); }
static void k_release(uint8_t s) { k_vent(s, 16); flow[s].when = 0; flow[s].mask = flow[s].last_mask = 0; }   // all of it: an ended or killed place keeps nothing
static uint8_t k_books(void) { uint32_t h = field; for (uint8_t s = 0; s < NPL; s++) h += flow[s].L; return (uint8_t)(h == rules.G0); }
static void k_begin(void) { k_rules_first(&rules); field = rules.G0; for (uint8_t s = 0; s < NPL; s++) { flow[s].L = 0; flow[s].when = 0; flow[s].mask = flow[s].last_mask = 0; } }
static uint8_t k_set_g0(uint16_t g) {                // a new total: the difference is taken from (or given to) the field only; theta if it is not there
  if (g < rules.G0 && (uint16_t)(rules.G0 - g) > field) return 0;
  field = (uint16_t)(field + g - rules.G0); rules.G0 = g; return 1; }
static uint16_t k_drag(uint8_t s) {                  // the L of every other place sharing a cell with s, this latch or the last
  uint8_t m = (uint8_t)(flow[s].mask | flow[s].last_mask); if (!m) return 0;
  uint32_t d = 0; for (uint8_t j = 0; j < NPL; j++) if (j != s && ((flow[j].mask | flow[j].last_mask) & m)) d += flow[j].L;
  d >>= rules.DRAG; return (uint16_t)(d > 60000u ? 60000u : d); }
static uint8_t k_weight(Proc *p) { uint8_t op = p->pc < p->n ? (uint8_t)(k_code(p, (uint8_t)(1 + 2 * p->pc)) >> 3) : 0; return K_ROM8(&OPW[op]); }
static void k_latch(void) {
  k_latches++; k_steps = 0; k_beats = 0;
  for (uint8_t s = 0; s < NPL; s++) { Flow *f = &flow[s]; Proc *p = &proc[s]; f->took = 0; f->last_mask = f->mask; f->mask = 0;
    if (p->state != ST_RUN) { f->out = 1; continue; }
    if (p->sleep) { p->sleep--; f->out = 1; continue; }
    f->out = 0; }
  uint8_t cur = 255;
  while (k_beats < rules.S) {
    uint8_t s = 255; uint32_t best = 0xFFFFFFFFu;
    for (uint8_t j = 0; j < NPL; j++) if (!flow[j].out && flow[j].when < best) { best = flow[j].when; s = j; }   // OR = min
    if (s == 255) break;
    Flow *f = &flow[s]; Proc *p = &proc[s]; uint8_t w = k_weight(p);
    if (field < w) {                                 // its demand is past what the field holds: it reconfigures (clause 4), it does not wait
      k_spent++; if (!f->L) break;                   // nothing to vent and nothing to take: the latch ends
      uint16_t keep = (uint16_t)k_mouth32((int32_t)f->L * rules.KAPPA, 16); if (keep == f->L) keep--; field = (uint16_t)(field + f->L - keep); f->L = keep; k_recon++; continue; }
    if (s != cur) { k_enter(s); cur = s; }
    field = (uint16_t)(field - w); f->L = (uint16_t)(f->L + w);   // a step carries its weight of the field into the place's history
    k_touch = 0; uint8_t over = vm_step(p); f->mask |= k_touch;
    k_steps++; k_beats = (uint16_t)(k_beats + w); f->took = (uint16_t)(f->took + w); k_steps_all++;
    f->when += (uint32_t)w * ((uint32_t)rules.C + f->L + k_drag(s));   // AND = +, scaled by what the step weighed
    if (p->state != ST_RUN) { f->out = 1; k_release(s); }   // it ended, or was refused: it cannot go on, so its whole history goes back
    else if (over) { f->out = 1; k_vent(s, rules.VENT); }   // it gave the machine back: it vents
    if (f->L > rules.THETA) {                        // past theta: reconfigure, keep a core, vent the rest, back to the front
      uint16_t keep = (uint16_t)k_mouth32((int32_t)f->L * rules.KAPPA, 16); field = (uint16_t)(field + f->L - keep); f->L = keep; k_recon++; f->when = best; } }
  uint32_t m = 0xFFFFFFFFu; for (uint8_t j = 0; j < NPL; j++) if (proc[j].state == ST_RUN && flow[j].when < m) m = flow[j].when;
  if (m == 0xFFFFFFFFu) m = 0;
  for (uint8_t j = 0; j < NPL; j++) flow[j].when = flow[j].when > m ? flow[j].when - m : 0;   // a place that sat out comes back level with the front, never with banked credit
  if (!k_books()) k_leaks++; }
// ================================================================ end of KERNEL

// ======================= the sketch: RULE OS 4 (Uno) =======================
// Stick: the pointer. Press: click. DESK shows four windows, one for each place; click a window to make it big, click again for four.
// FILES: pick a file, RUN it, EDIT it, put its factory program back (DEMO), spoil its spare copy (ZAP) and watch the keeper mend it.
// TASKS: each place's loaded history L and the steps it took this latch; STOP/GO, KILL, VENT (it gives back all its history now),
//        and RULES. RULES: the kernel's own readings, each with - and +, SAVE (kept in lasting memory, two copies) and FIRST.
// The files are OS 2's, byte for byte, in the same places in lasting memory: an Uno that ran OS 2 keeps its programs.
// The top page inverts if the kernel's books ever open (field + every L must equal G0 on every latch).
#define SCR_DESK 0
#define SCR_FILES 1
#define SCR_TASKS 2
#define SCR_EDIT 3
#define SCR_RULES 4
#define MAX_STEPS 48
static const uint8_t DEMO[5][98] PROGMEM = {
  { 33, 8, 3, 24, 2, 9, 5, 25, 1, 10, 1, 11, 58, 12, 23, 15, 3, 168, 0, 96, 0, 120, 7, 104, 0, 123, 0, 104, 0, 97, 0, 121, 7, 105, 0, 124, 1, 105, 0, 37, 0, 53, 2, 38, 0, 46, 2, 189, 1, 198, 1, 37, 1, 53, 2, 38, 1, 46, 2, 184, 5, 192, 6, 160, 0, 152, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 25, 8, 1, 9, 2, 15, 2, 200, 0, 34, 1, 66, 0, 40, 2, 64, 7, 200, 4, 34, 1, 66, 0, 40, 2, 64, 7, 200, 16, 34, 1, 66, 0, 40, 2, 64, 7, 200, 32, 34, 1, 66, 0, 40, 2, 64, 7, 200, 48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 39, 13, 7, 14, 100, 61, 6, 15, 9, 63, 6, 14, 13, 16, 226, 33, 0, 57, 0, 57, 0, 34, 0, 58, 5, 49, 2, 65, 7, 34, 1, 74, 0, 89, 1, 11, 2, 58, 3, 91, 0, 12, 31, 43, 4, 36, 6, 52, 1, 9, 2, 51, 1, 44, 2, 187, 4, 43, 1, 43, 1, 52, 2, 52, 2, 195, 4, 9, 4, 40, 1, 9, 31, 120, 1, 152, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 31, 8, 248, 24, 1, 13, 8, 14, 248, 15, 12, 168, 0, 89, 0, 34, 5, 209, 2, 201, 48, 35, 1, 59, 1, 36, 2, 60, 2, 43, 4, 203, 0, 57, 7, 58, 7, 11, 31, 12, 13, 187, 4, 43, 1, 52, 2, 195, 4, 96, 0, 128, 5, 104, 0, 128, 6, 104, 0, 248, 1, 152, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 31, 8, 1, 24, 2, 9, 61, 25, 253, 14, 1, 15, 61, 13, 10, 12, 16, 168, 0, 34, 1, 50, 0, 35, 2, 83, 2, 202, 0, 203, 8, 184, 5, 192, 4, 185, 5, 193, 4, 96, 0, 97, 0, 128, 1, 104, 0, 128, 1, 105, 0, 128, 6, 104, 0, 129, 7, 105, 0, 248, 2, 152, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };   // BOUNCE, ROOT, SLOPE, TURN, MEET
static const char OP_NAME[32][6] PROGMEM = { "END", "SET", "ISO", "SEED", "MOV", "ADD", "SUB", "MIX", "DIV", "RATE", "WHEN", "VAL", "LATCH", "FLIP", "LAND", "IFLT", "IFEQ", "IFBTN", "LOOP", "JMP", "SHOW", "CLS", "DOT", "PEN", "LINE", "NUM", "HEAD", "STICK", "RAND", "PUT", "GET", "WAIT" };
static const uint8_t OP_KIND[32] PROGMEM = { 0, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 3, 2, 2, 0, 5, 4, 0, 0, 2, 2, 2, 3, 2, 2, 3, 3, 3, 6 };
static uint8_t ed[RK_FILE_BYTES];                    // the program being edited: ed[0] steps, then two bytes a step
static uint8_t screen = SCR_DESK, zoom = 0, repaint = 1, frame_no = 0, file_sel = 0, task_sel = 0, ed_file = 0, ed_sel = 0, ed_top = 0, rule_sel = 0;
static uint8_t win_on = 0, win_big = 0, win_x = 0, win_y = 0, ptr_on = 0, ptr_x = 0, ptr_y = 0, books_open = 0;

// ---------------------------------------------------------------- a window: where a place's dots land
static void rect_clear(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  uint8_t y1 = (uint8_t)(y + h - 1);
  for (uint8_t page = y >> 3; page <= (y1 >> 3); page++) {
    uint8_t m = 0xFF;
    if (page == (y >> 3)) m &= (uint8_t)(0xFF << (y & 7));
    if (page == (y1 >> 3)) m &= (uint8_t)(0xFF >> (7 - (y1 & 7)));
    uint8_t *q = &pc_frame[(uint16_t)page * 128 + x]; m = (uint8_t)~m;
    for (uint8_t i = 0; i < w; i++) *q++ &= m;
  }
}
static void win_set(uint8_t s) {                     // whose window the next dots go to
  win_big = 0; win_on = 0;
  if (screen != SCR_DESK) return;
  if (zoom) { if (zoom - 1 == s) { win_on = 1; win_big = 1; win_x = 2; win_y = 9; } return; }
  win_on = 1; win_x = (uint8_t)((s & 1) * 64 + 1); win_y = (uint8_t)((s >> 1) * 29 + 8);
}
static void win_dot(int16_t x, int16_t y) {
  if (!win_on || x < 0 || x > 61 || y < 0 || y > 25) return;
  if (win_big) { uint8_t X = (uint8_t)(win_x + 2 * x), Y = (uint8_t)(win_y + 2 * y); PC_PLOT(X, Y); PC_PLOT(X + 1, Y); PC_PLOT(X, Y + 1); PC_PLOT(X + 1, Y + 1); }
  else PC_PLOT(win_x + x, win_y + y);
}
static int16_t win_put(int16_t x, int16_t y, char ch) {
  uint16_t g = RD16(&GLYPH[glyph_of(ch)]);
  for (uint8_t r = 0; r < 5; r++) for (uint8_t c = 0; c < 3; c++) if (g & (0x4000 >> (r * 3 + c))) win_dot(x + c, y + r);
  return x + 4;
}
static int16_t win_count(int16_t x, int16_t y, int32_t v) {
  char b[11]; uint8_t n = 0; uint32_t u = (uint32_t)(v < 0 ? -v : v);
  if (v < 0) x = win_put(x, y, '-');
  do { b[n++] = (char)('0' + u % 10); u /= 10; } while (u);
  while (n) x = win_put(x, y, b[--n]);
  return x;
}

#ifdef STACKCHECK                                    // bench only: paint free RAM at reset, count what the stack never reached

static uint16_t stack_free = 0xFFFF;
static void stack_paint(void) __attribute__((naked, used, section(".init3")));
static void stack_paint(void) { uint8_t *p = (uint8_t *)&__bss_end; while (p < (uint8_t *)SP - 4) *p++ = 0xC5; }
static void stack_note(void) { uint8_t *p = (uint8_t *)&__bss_end; uint16_t n = 0; while (p < (uint8_t *)0x8FF && *p == 0xC5) { p++; n++; } stack_free = n; num(100, 57, n); }
#endif
#ifdef GHOST                                         // bench only: a scripted hand that visits every screen (it never saves, spoils or installs)
typedef struct { uint16_t at; uint8_t x, y, hold; } GHand;
static const GHand GH[] PROGMEM = { {40, 30, 20, 1}, {70, 30, 20, 1}, {100, 114, 3, 1}, {130, 20, 42, 1}, {150, 107, 40, 1}, {170, 107, 54, 1},
  {200, 10, 34, 1}, {215, 73, 14, 40}, {270, 10, 26, 1}, {285, 109, 14, 30}, {330, 4, 3, 1}, {350, 4, 3, 1}, {380, 85, 3, 1}, {400, 98, 27, 1},
  {430, 111, 14, 20}, {460, 111, 58, 25}, {500, 79, 14, 20}, {535, 79, 47, 1}, {550, 111, 47, 1}, {565, 4, 3, 1}, {580, 4, 3, 1}, {600, 85, 3, 1}, {620, 4, 3, 1},
  {650, 114, 3, 1}, {680, 107, 54, 1}, {700, 90, 27, 1}, {720, 90, 40, 1}, {740, 4, 3, 1}, {760, 4, 3, 1} };
static uint16_t g_frame; static uint8_t g_was;
static void ghost_input(void) {
  g_frame++; uint8_t down = 0;
  for (uint8_t i = 0; i < sizeof GH / sizeof GH[0]; i++) { uint16_t at = RD16(&GH[i].at); uint8_t h = RD8(&GH[i].hold);
    if (g_frame >= at - 8 && g_frame < at + h) { rk_px = (int16_t)(RD8(&GH[i].x) * 4); rk_py = (int16_t)(RD8(&GH[i].y) * 4); if (g_frame >= at) down = 1; } }
  rk_clicked = down && !g_was; rk_held = down; g_was = down; }
#endif
// ---------------------------------------------------------------- the hooks: how a step reaches this board
#ifdef OPCOST
static uint8_t BCODE[98];
static uint8_t k_code(Proc *p, uint8_t i) { (void)p; return BCODE[i]; }
#else
static uint8_t k_code(Proc *p, uint8_t i) { return RK_EE_RD(p->base + 1 + i); }       // programs run straight out of lasting memory
#endif
static void k_enter(uint8_t s) { win_set(s); }
static void k_cls(void) { if (!win_on) return; if (win_big) rect_clear(2, 9, 124, 52); else rect_clear(win_x, win_y, 62, 26); }
static void k_dot(int32_t x, int32_t y) { if (x < 0 || x > 61 || y < 0 || y > 25) return; win_dot((int16_t)x, (int16_t)y); }
static void k_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {     // the carry rule, as in pc_line, landing its dots in the window
  if (!win_on || x0 < -99 || x0 > 160 || x1 < -99 || x1 > 160 || y0 < -99 || y0 > 125 || y1 < -99 || y1 > 125) return;
  int16_t X = (int16_t)x0, Y = (int16_t)y0, dx = (int16_t)(x1 - x0), dy = (int16_t)(y1 - y0); int8_t sx = pc_way(dx), sy = pc_way(dy);
  uint16_t ax = (uint16_t)(dx < 0 ? -dx : dx), ay = (uint16_t)(dy < 0 ? -dy : dy);
  uint8_t steep = ay > ax; uint16_t lng = steep ? ay : ax, sht = steep ? ax : ay, acc = lng >> 1;
  for (uint16_t k = 0; k <= lng; k++) {
    win_dot(X, Y);
    if (steep) Y += sy; else X += sx;
    acc += sht; if (acc >= lng) { acc -= lng; if (steep) X += sx; else Y += sy; }
  }
}
static void k_num(const Rd *a, uint8_t at) { int16_t y = (int16_t)((at >> 4) * 6); int16_t x = win_count((int16_t)((at & 15) * 4), y, a->v); if (a->d != 1) { x = win_put(x, y, '/'); win_count(x, y, a->d); } }
static void k_stick(Rd *a, Rd *b) { a->v = rk_sx; a->r = 0; a->d = 1; b->v = rk_sy; b->r = 0; b->d = 1; }
static uint8_t k_btn(void) { return rk_held; }
static int32_t k_rand(int32_t n) { return pc_scatter(0, (int16_t)n); }

#ifdef OPCOST                                        // bench only: every op's cost on this chip, measured by Timer1 around single steps of random programs
static volatile uint8_t bench_last_op; static volatile uint16_t bench_last_n, bench_last_k; static uint16_t OPMAX[32], OPN[32]; static uint32_t brs = 12345;
static uint32_t brnd(void) { brs ^= brs << 13; brs ^= brs >> 17; brs ^= brs << 5; return brs; }
static void bench_run(void) {
  TCCR1A = 0; TCCR1B = 1;                            // Timer1 at the chip's own clock
  screen = SCR_DESK; zoom = 0;
  for (uint16_t n = 0; n < BENCH_N; n++) {
    uint8_t ns = (uint8_t)(4 + brnd() % 30); BCODE[0] = ns;
    for (uint8_t i = 0; i < ns; i++) { uint8_t op = (uint8_t)(brnd() % 32); if (op == 17 || op == 27) op = 7;
      BCODE[1 + 2 * i] = (uint8_t)(op << 3 | (brnd() & 7)); uint8_t b1 = (uint8_t)brnd(); if (op == 18 || op == 19) b1 = (uint8_t)(brnd() % ns); if (op == 1 || op == 2 || op == 3) b1 = (uint8_t)((int8_t)(brnd() % 13) - 6); BCODE[2 + 2 * i] = b1; }
    Proc *p = &proc[0]; for (uint8_t i = 0; i < sizeof(Proc); i++) ((uint8_t *)p)[i] = 0; p->n = ns; for (uint8_t i = 0; i < 8; i++) p->reg[i].d = 1; p->state = ST_RUN;
    win_set((uint8_t)(n & 3));
    for (uint16_t k = 0; k < 600 && p->state == ST_RUN; k++) { uint8_t op = (uint8_t)(BCODE[1 + 2 * p->pc] >> 3); if (p->pc >= p->n) op = 0;
      bench_last_op = op; bench_last_n = n; bench_last_k = k; uint16_t t0 = TCNT1; vm_step(p); uint16_t dt = (uint16_t)(TCNT1 - t0); p->sleep = 0;
      if (dt > OPMAX[op]) OPMAX[op] = dt; if (OPN[op] < 65535) OPN[op]++; }
    if ((n & 63) == 0) oled_clear(); }
}
#endif
// ---------------------------------------------------------------- places and files
static void proc_start(uint8_t s, uint8_t f) {       // load file f into place s and set it going from its first step
  Proc *p = &proc[s]; uint8_t st = rk_file_state(f);
  k_release(s);
  for (uint8_t i = 0; i < sizeof(Proc); i++) ((uint8_t *)p)[i] = 0;
  p->file = f;
  if (st == RK_EMPTY || st == RK_LOST) return;       // nothing there to run: the place stays free
  p->base = rk_copy_at(f, st == RK_BAD_A ? 1 : 0); p->n = RK_EE_RD(p->base + 1); if (p->n > MAX_STEPS) p->n = MAX_STEPS;
  for (uint8_t i = 0; i < 8; i++) p->reg[i].d = 1;
  p->state = ST_RUN;
}
static void spawn(uint8_t f) {                       // run file f: in the place already running it, else a free place, else a finished one
  uint8_t s = NPL;
  for (uint8_t i = 0; i < NPL; i++) if (proc[i].state != ST_FREE && proc[i].file == f) { s = i; break; }
  if (s == NPL) for (uint8_t i = 0; i < NPL; i++) if (proc[i].state == ST_FREE) { s = i; break; }
  if (s == NPL) for (uint8_t i = 0; i < NPL; i++) if (proc[i].state >= ST_END) { s = i; break; }
  if (s < NPL) { proc_start(s, f); task_sel = s; }
}
static void restart_file(uint8_t f) { for (uint8_t i = 0; i < NPL; i++) if (proc[i].state != ST_FREE && proc[i].file == f) proc_start(i, f); }
static void install(uint8_t f) { for (uint8_t i = 0; i < RK_FILE_BYTES; i++) ed[i] = RD8(&DEMO[f][i]); rk_file_write(f, ed); }

// ---------------------------------------------------------------- the kernel's rules, kept in the 24 bytes OS 2 left unused (1000 to 1023)
#define RULES_AT 1002                                // two copies of: mark 0xAA, 9 bytes, a sum (1002 to 1023)
#define RULES_N 9
static void rules_pack(uint8_t *d) { d[0] = (uint8_t)rules.C; d[1] = (uint8_t)rules.THETA; d[2] = (uint8_t)(rules.THETA >> 8); d[3] = (uint8_t)rules.G0; d[4] = (uint8_t)(rules.G0 >> 8);
  d[5] = (uint8_t)rules.S; d[6] = (uint8_t)(rules.S >> 8); d[7] = (uint8_t)(rules.KAPPA | (rules.VENT << 4)); d[8] = rules.DRAG; }
static void rules_save(void) { uint8_t d[RULES_N]; rules_pack(d);
  for (uint8_t c = 0; c < 2; c++) { uint16_t at = (uint16_t)(RULES_AT + (RULES_N + 2) * c); uint8_t s = 0x5A; RK_EE_WR(at, 0xAA); for (uint8_t i = 0; i < RULES_N; i++) { RK_EE_WR(at + 1 + i, d[i]); s = rk_sum_step(s, d[i]); } RK_EE_WR(at + 1 + RULES_N, s); } }
static uint8_t rules_load(void) {                    // 1 if a sound copy was found; the field is then grown from the loaded G0
  for (uint8_t c = 0; c < 2; c++) { uint16_t at = (uint16_t)(RULES_AT + (RULES_N + 2) * c); if (RK_EE_RD(at) != 0xAA) continue;
    uint8_t d[RULES_N], s = 0x5A; for (uint8_t i = 0; i < RULES_N; i++) { d[i] = RK_EE_RD(at + 1 + i); s = rk_sum_step(s, d[i]); } if (s != RK_EE_RD(at + 1 + RULES_N)) continue;
    rules.C = d[0]; rules.THETA = (uint16_t)(d[1] | (d[2] << 8)); rules.G0 = (uint16_t)(d[3] | (d[4] << 8)); rules.S = (uint16_t)(d[5] | (d[6] << 8)); rules.KAPPA = (uint8_t)(d[7] & 15); rules.VENT = (uint8_t)(d[7] >> 4); rules.DRAG = d[8];
    field = rules.G0; return 1; }
  return 0; }

// ---------------------------------------------------------------- the pointer, and small helpers for the screens
static void pointer_flip(void) {                     // drawn by turning dots over, so drawing it twice puts the picture back
  for (uint8_t r = 0; r < 7; r++) { uint8_t bits = RD8(&RK_ARROW[r]);
    for (uint8_t c = 0; c < 5; c++) if ((bits >> c) & 1) { uint8_t x = (uint8_t)(ptr_x + c), y = (uint8_t)(ptr_y + r); if (x < 128 && y < 64) pc_frame[x + (uint16_t)(y >> 3) * 128] ^= (uint8_t)(1 << (y & 7)); } }
}
static uint8_t held_for = 0, last_screen = 0;
static uint8_t held_button(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char *label) {   // a button that repeats once it has been held a while
  uint8_t hit = rk_button(x, y, w, h, label);
  return hit || (held_for > 12 && (frame_no % 3 == 0) && rk_over(x, y, w, h));
}
static uint8_t signed_num(uint8_t x, uint8_t y, int16_t v) { if (v < 0) { x = put(x, y, '-'); v = (int16_t)-v; } return num(x, y, (uint32_t)v); }
static uint8_t back_button(void) { if (rk_button(0, 0, 9, 7, P("X"))) { screen = SCR_DESK; repaint = 1; return 1; } return 0; }
static uint8_t step_text(uint8_t x, uint8_t y, uint8_t i, uint8_t b0, uint8_t b1) {
  if (i < 10) x = put(x, y, '0'); x = num(x, y, i); x += 2;
  uint8_t op = b0 >> 3, kind = RD8(&OP_KIND[op]); char ch;
  for (uint8_t k = 0; k < 6 && (ch = (char)RD8(&OP_NAME[op][k])); k++) x = put(x, y, ch);
  x += 3;
  if (kind == 1 || kind == 2 || kind == 3 || kind == 5) x = (uint8_t)(put(x, y, (char)('A' + (b0 & 7))) + 3);
  if (kind == 2) x = put(x, y, (char)('A' + (b1 & 7)));
  if (kind == 3) x = signed_num(x, y, (int8_t)b1);
  if (kind == 4 || kind == 5 || kind == 6) x = num(x, y, b1);
  return x;
}
static const char *state_word(uint8_t st) { return st == ST_RUN ? P("RUN") : st == ST_STOP ? P("STOP") : st == ST_END ? P("END") : st == ST_THETA ? P("THETA") : st == ST_WIDE ? P("WIDE") : st == ST_LOST ? P("LOST") : P("FREE"); }

// ---------------------------------------------------------------- one latch of every place, then what a refusal says
static uint8_t said[NPL];                            // a refusal is written into its window once, not every latch
static void turns(void) {
  k_latch();
  for (uint8_t s = 0; s < NPL; s++) { Proc *p = &proc[s];
    if (p->state >= ST_THETA && !said[s]) { win_set(s); int16_t x = 0; const char *w = state_word(p->state); char ch; while ((ch = (char)RD8(w++))) x = win_put(x, 20, ch); win_count(x + 4, 20, p->pc); said[s] = screen == SCR_DESK; }
    if (p->state < ST_THETA) said[s] = 0; }
  books_open = !k_books();
}

// ---------------------------------------------------------------- the screens
static void desk(void) {
  if (repaint) {                                     // the whole desk again: frames, and every finished program run again to remake its picture
    oled_clear(); repaint = 0;
    if (zoom) rk_box(1, 8, 126, 54); else for (uint8_t s = 0; s < 4; s++) rk_box((uint8_t)((s & 1) * 64), (uint8_t)((s >> 1) * 29 + 7), 64, 28);
    for (uint8_t s = 0; s < NPL; s++) { said[s] = 0; if (proc[s].state >= ST_END) proc_start(s, proc[s].file); }
  }
  for (uint8_t i = 0; i < 128; i++) pc_frame[i] &= 0x80;                 // the bar is drawn afresh every frame; the windows are not
  turns();
  uint8_t x = say(1, 1, P("OS4")); x = num((uint8_t)(x + 3), 1, k_beats); x = say((uint8_t)(x + 3), 1, P("F")); num((uint8_t)(x + 1), 1, field);
  if (rk_button(72, 0, 27, 7, P("FILES"))) { screen = SCR_FILES; return; }
  if (rk_button(100, 0, 28, 7, P("TASKS"))) { screen = SCR_TASKS; return; }
  if (rk_clicked && (rk_py >> 2) >= 8) {
    if (zoom) { zoom = 0; repaint = 1; }
    else { uint8_t s = (uint8_t)(((rk_px >> 2) >= 64) + 2 * ((rk_py >> 2) >= 36)); if (proc[s].state != ST_FREE) { zoom = (uint8_t)(s + 1); repaint = 1; } }
  }
}
static void files(void) {
  turns(); oled_clear();
  if (back_button()) return;
  uint8_t x = say(14, 1, P("FILES   MENDED ")); num(x, 1, rk_mended);
  for (uint8_t f = 0; f < RK_FILES; f++) {
    uint8_t y = (uint8_t)(10 + 10 * f), st = rk_file_state(f);
    x = put(3, y, (char)('A' + f)); x += 4;
    x = say(x, y, st == RK_EMPTY ? P("EMPTY") : st == RK_SOUND ? P("SOUND") : st == RK_BAD_A ? P("BAD A") : st == RK_BAD_B ? P("BAD B") : P("LOST")); x += 4;
    if (st != RK_EMPTY && st != RK_LOST) num(x, y, RK_EE_RD(rk_copy_at(f, st == RK_BAD_A ? 1 : 0) + 1));
    if (f == file_sel) rk_box(0, (uint8_t)(y - 2), 62, 9);
    if (rk_clicked && rk_over(0, (uint8_t)(y - 2), 62, 9)) file_sel = f;
  }
  if (rk_button(70, 9, 56, 11, P("RUN"))) { spawn(file_sel); screen = SCR_DESK; repaint = 1; zoom = 0; return; }
  if (rk_button(70, 22, 56, 11, P("EDIT"))) { ed_file = file_sel; if (!rk_file_read(ed_file, ed)) ed[0] = 0; if (ed[0] > MAX_STEPS) ed[0] = MAX_STEPS; ed_sel = 0; ed_top = 0; screen = SCR_EDIT; return; }
  if (rk_button(70, 35, 56, 11, P("DEMO"))) { install(file_sel); restart_file(file_sel); }
  if (rk_button(70, 48, 56, 11, P("ZAP"))) { uint16_t at = (uint16_t)(rk_copy_at(file_sel, 1) + 5); RK_EE_WR(at, (uint8_t)(RK_EE_RD(at) ^ 0xFF)); }
}
static void tasks(void) {
  turns(); oled_clear();
  if (back_button()) return;
  uint8_t x = say(12, 1, P("TASKS F")); x = num(x, 1, field); x = put(x, 1, '/'); num(x, 1, rules.G0);
  for (uint8_t s = 0; s < NPL; s++) {
    Proc *p = &proc[s]; uint8_t y = (uint8_t)(10 + 10 * s);
    x = num(3, y, (uint32_t)(s + 1)); x += 3;
    if (p->state == ST_FREE) say(x, y, P("FREE"));
    else { x = put(x, y, (char)('A' + p->file)); x += 3; x = say(x, y, state_word(p->state)); x = (uint8_t)(x < 46 ? 46 : x + 2);
      x = say(x, y, P("L")); x = num(x, y, flow[s].L); x += 3; num(x, y, flow[s].took); }
    if (s == task_sel) rk_box(0, (uint8_t)(y - 2), 84, 9);
    if (rk_clicked && rk_over(0, (uint8_t)(y - 2), 84, 9)) task_sel = s;
  }
  Proc *p = &proc[task_sel];
  if (rk_button(87, 9, 40, 11, p->state == ST_STOP ? P("GO") : P("STOP"))) { if (p->state == ST_RUN) p->state = ST_STOP; else if (p->state == ST_STOP) p->state = ST_RUN; }
  if (rk_button(87, 22, 40, 11, P("KILL"))) { p->state = ST_FREE; k_release(task_sel); }
  if (rk_button(87, 35, 40, 11, P("VENT"))) k_vent(task_sel, 16);
  if (rk_button(87, 48, 40, 11, P("RULES"))) { screen = SCR_RULES; return; }
  x = say(3, 52, P("RECON ")); num(x, 52, k_recon);
}
static const char RULE_NAME[7][6] PROGMEM = { "C", "THETA", "G0", "S", "KAPPA", "VENT", "DRAG" };
static void rules_screen(void) {                     // the kernel's alphabet, by hand: each reading with - and +
  turns(); oled_clear();
  if (rk_button(0, 0, 9, 7, P("X"))) { screen = SCR_TASKS; return; }
  uint8_t x = say(12, 1, P("RULES RECON ")); num(x, 1, k_recon);
  for (uint8_t i = 0; i < 7; i++) { uint8_t y = (uint8_t)(9 + 8 * i); char ch; x = 3;
    for (uint8_t k = 0; k < 6 && (ch = (char)RD8(&RULE_NAME[i][k])); k++) x = put(x, y, ch);
    uint16_t v = i == 0 ? rules.C : i == 1 ? rules.THETA : i == 2 ? rules.G0 : i == 3 ? rules.S : i == 4 ? rules.KAPPA : i == 5 ? rules.VENT : rules.DRAG;
    num(28, y, v); if (i == rule_sel) rk_box(0, (uint8_t)(y - 1), 54, 7);
    if (rk_clicked && rk_over(0, (uint8_t)(y - 1), 54, 7)) rule_sel = i; }
  int8_t d = (int8_t)(held_button(92, 9, 35, 11, P("+")) - held_button(56, 9, 35, 11, P("-")));
  if (d) { uint8_t i = rule_sel;                      // each reading has its own step and its own floor and ceiling; the field takes any change to G0
    if (i == 0) { int16_t v = (int16_t)(rules.C + 4 * d); if (v >= 4 && v <= 252) rules.C = (uint16_t)v; }
    if (i == 1) { int32_t v = (int32_t)rules.THETA + 100 * d; if (v >= 100 && v <= 30000) rules.THETA = (uint16_t)v; }
    if (i == 2) { int32_t v = (int32_t)rules.G0 + 500 * d; if (v >= 500 && v <= 60000) k_set_g0((uint16_t)v); }
    if (i == 3) { int16_t v = (int16_t)(rules.S + 50 * d); if (v >= 100 && v <= 3000) rules.S = (uint16_t)v; }
    if (i == 4) { int8_t v = (int8_t)(rules.KAPPA + d); if (v >= 0 && v <= 15) rules.KAPPA = (uint8_t)v; }
    if (i == 5) { int8_t v = (int8_t)(rules.VENT + d); if (v >= 0 && v <= 15) rules.VENT = (uint8_t)v; }
    if (i == 6) { int8_t v = (int8_t)(rules.DRAG + d); if (v >= 0 && v <= 15) rules.DRAG = (uint8_t)v; } }
  if (rk_button(56, 22, 71, 11, P("SAVE"))) rules_save();
  if (rk_button(56, 35, 71, 11, P("FIRST"))) { Rules f; k_rules_first(&f); uint16_t g = f.G0; f.G0 = rules.G0; rules = f; k_set_g0(g); }
  x = say(56, 52, P("BOOKS ")); say(x, 52, books_open ? P("OPEN") : P("SHUT"));
}
static void ed_shift(uint8_t from, int8_t by) {
  for (uint8_t i = 0; i < ed[0]; i++) { uint8_t kind = RD8(&OP_KIND[ed[1 + 2 * i] >> 3]); if ((kind == 4 || kind == 5) && ed[2 + 2 * i] > from) ed[2 + 2 * i] = (uint8_t)(ed[2 + 2 * i] + by); }
}
static void editor(void) {
  turns(); oled_clear();
  if (back_button()) return;
  uint8_t x = say(12, 1, P("EDIT ")); x = put(x, 1, (char)('A' + ed_file)); x += 4; x = num(x, 1, ed[0]); x = say(x, 1, P("/48"));
  if (rk_button(74, 0, 25, 7, P("SAVE"))) { rk_file_write(ed_file, ed); restart_file(ed_file); }
  if (rk_button(102, 0, 26, 7, P("RUN"))) { rk_file_write(ed_file, ed); spawn(ed_file); screen = SCR_DESK; repaint = 1; zoom = 0; return; }
  if (ed_sel < ed_top) ed_top = ed_sel; if (ed_sel > ed_top + 8) ed_top = (uint8_t)(ed_sel - 8);
  for (uint8_t row = 0; row < 9; row++) {
    uint8_t i = (uint8_t)(ed_top + row), y = (uint8_t)(9 + 6 * row); if (i >= ed[0]) break;
    step_text(3, y, i, ed[1 + 2 * i], ed[2 + 2 * i]);
    if (i == ed_sel) { PC_PLOT(0, y + 1); PC_PLOT(0, y + 2); PC_PLOT(0, y + 3); PC_PLOT(1, y + 2); }
    if (rk_clicked && rk_over(0, y, 62, 6)) ed_sel = i;
  }
  uint8_t *b0 = &ed[1 + 2 * ed_sel], *b1 = &ed[2 + 2 * ed_sel], has = ed[0] > 0, kind = has ? RD8(&OP_KIND[*b0 >> 3]) : 0;
  int8_t d;
  d = (int8_t)(held_button(96, 9, 31, 10, P("OP+")) - held_button(64, 9, 31, 10, P("OP-")));
  if (d && has) *b0 = (uint8_t)((((*b0 >> 3) + d) & 31) << 3 | (*b0 & 7));
  d = (int8_t)(held_button(96, 20, 31, 10, P("A+")) - held_button(64, 20, 31, 10, P("A-")));
  if (d && has) *b0 = (uint8_t)((*b0 & 0xF8) | ((*b0 + d) & 7));
  d = (int8_t)(held_button(96, 31, 31, 10, P("B+")) - held_button(64, 31, 31, 10, P("B-")));
  if (d && has) { if (kind == 2) *b1 = (uint8_t)((*b1 + d) & 7); else if (kind == 4 || kind == 5) { if (!(d < 0 && *b1 == 0) && !(d > 0 && *b1 >= MAX_STEPS - 1)) *b1 = (uint8_t)(*b1 + d); } else *b1 = (uint8_t)(*b1 + d); }
  if (rk_button(64, 42, 31, 10, P("INS")) && ed[0] < MAX_STEPS) {
    uint8_t at = has ? (uint8_t)(ed_sel + 1) : 0;
    ed_shift(has ? ed_sel : 0, 1);
    for (uint8_t i = (uint8_t)(2 * ed[0]); i >= 2 * at + 1; i--) ed[i + 2] = ed[i];
    ed[1 + 2 * at] = (uint8_t)(OP_SHOW << 3); ed[2 + 2 * at] = 0; ed[0]++; ed_sel = at;
  }
  if (rk_button(96, 42, 31, 10, P("DEL")) && has) {
    for (uint8_t i = (uint8_t)(1 + 2 * ed_sel); i + 2 < RK_FILE_BYTES; i++) ed[i] = ed[i + 2];
    ed[0]--; ed_shift(ed_sel, -1); if (ed_sel && ed_sel >= ed[0]) ed_sel--;
  }
  if (held_button(64, 53, 31, 10, P("UP")) && ed_sel) ed_sel--;
  if (held_button(96, 53, 31, 10, P("DN")) && ed_sel + 1 < ed[0]) ed_sel++;
}
void setup() {
#ifdef OPCOST
  bench_run(); for (;;) oled_flush();
#endif
  oled_begin(); pinMode(RK_PIN_SW, INPUT_PULLUP);
  k_begin(); rules_load();
  uint8_t any = 0; for (uint8_t f = 0; f < RK_FILES; f++) if (rk_file_state(f) != RK_EMPTY) any = 1;
  if (!any) for (uint8_t f = 0; f < RK_FILES; f++) install(f);            // a new chip: the five factory programs go into the files
  for (uint8_t s = 0; s < NPL; s++) { cell[s].d = 1; proc_start(s, s); }  // and the first four are set going
}
void loop() {
  frame_no++;
  if (ptr_on) { pointer_flip(); ptr_on = 0; }        // take the pointer off the picture before anything else touches it
  rk_input();
#ifdef GHOST
  ghost_input();
#endif
  if (screen != last_screen) { last_screen = screen; held_for = 0; }
  if (rk_clicked) held_for = 1; else if (!rk_held) held_for = 0; else if (held_for && held_for < 255) held_for++;
  if (frame_no % 12 == 0) rk_keeper_turn();          // the keeper looks at one file and mends it from its good copy
  if (screen == SCR_DESK) desk(); else if (screen == SCR_FILES) files(); else if (screen == SCR_TASKS) tasks(); else if (screen == SCR_RULES) rules_screen(); else editor();
  if (books_open) for (uint8_t c = 0; c < 128; c++) pc_frame[c] ^= 0x7F;   // a leak is shown at once: the bar inverts (the windows below it are left alone)
  ptr_x = (uint8_t)(rk_px >> 2); ptr_y = (uint8_t)(rk_py >> 2); pointer_flip(); ptr_on = 1;
#ifdef STACKCHECK
  stack_note();
#endif
  rk_show(40);
}
