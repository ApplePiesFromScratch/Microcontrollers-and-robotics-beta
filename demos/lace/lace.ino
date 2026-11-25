// LESSON-BEGIN 1 of 10: LESSON MAP: LACE, theta as a named refusal that does work
// SKILL      : a refusal (theta) is a working part, shown by ablation; books checked every frame; the glass is the only store.
// BOARD      : Arduino Uno R3 (ATmega328P, 2 KB RAM), SH1106 OLED on SPI, KY-023 stick. Upload as one file.
// READ FIRST : demos/lace/README.md (the Uno build) and project doc claude/LACE_README.md (the graded C3 LACE, not copied here); docs/METHOD.md sections 1, 6, 8.
// SECTIONS   : 2 the engine (pc_: mouth, carry, mix, headings) | 3 screen and words | 4 the kit (ring, stick, tasks)
//              5 the sketch: lace's four rules | 6 the turn constants | 7 latch | 8 books | 9 the frame loop
// THE ONE EQUATION THIS SKETCH CHECKS EVERY FRAME: lit == 8192 + laid + painted - forgot - cut (mod 65536). A frame that
//              does not close inverts the top page.
// WHAT IS NOT HERE: graded memory. One bit a dot means a path is lit or dark; the C3 LACE keeps a graded rule. Stated, not hidden.
// LESSON-END
// lace.ino  ONE FILE. Paste all of it into an empty Arduino IDE sketch, pick board "Arduino Uno", upload.
// This is RuleKit.h and the sketch joined, with every type first (the IDE needs that).
#include <Arduino.h>
#include <stdint.h>

// ---- every type, before any function ----
typedef struct { int8_t way; uint16_t quot, rem, den; } PcRate;
typedef struct { int16_t v, r; } PcPair;
typedef struct { int16_t p, q; } PcHeading;
typedef struct { int16_t c, s, d; } PcUnit;
typedef struct { void (*turn)(void); uint8_t every, wait, running; uint16_t turns; } RkTask;

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
// LESSON-BEGIN 2 of 10: The engine: the book's moves on 16-bit whole numbers
// WHAT    : pc_mouth32 lands a fraction once (nearest, half to even, mirror-true); pc_carry turns a rate n/d into pulses
//           and keeps the remainder; pc_mix is the book's mix on (v; r); pc_unit and pc_turn hold a direction as (P : Q).
// HOLDING : world places are tops over PC_SUB = 16 (sixteenths of a pixel). PC_THETA = -1 marks a refusal, never a size.
// TRAP    : an Uno int is 16 bits. 256*181 is -19,200 there. Every product here is cast to int32_t before it can overflow.
// TRAP    : C's / truncates toward zero and >> floors. Neither is the mouth. Read pc_mouth32 and say which case each line handles.
// TRY     : feed pc_mouth32(-5, 2) and pc_mouth32(5, 2) by hand. Predict -2 and 2. Then do practice/p02_carry_mouth.
// LESSON-END
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
// LESSON-BEGIN 3 of 10: The screen: one buffer of 1,024 bytes is the picture and the state
// WHAT    : oled_* send pc_frame (128 x 64 dots, one bit each) down SPI. pc_line walks a line by the carry rule.
// BUDGET  : the frame buffer is 1,024 of the Uno's 2,048 bytes. Static RAM must stay near 1,750 or the stack meets it.
// TRAP    : at 2,004 static bytes one Uno produced nothing (CI 139). Paint free RAM at boot and read the untouched bytes; do not reason about the stack.
// LESSON-END
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

// LESSON-BEGIN 4 of 10: Words: a 3 x 5 face held in flash
// WHAT    : glyph_of maps a letter to 15 bits in PROGMEM; say and num draw them. Flash costs nothing in RAM.
// TRAP    : a PROGMEM int16 read without a cast comes back unsigned (CI 141).
// LESSON-END
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
// LESSON-BEGIN 5 of 10: The kit: ring, stick, buttons, tasks, files
// WHAT    : rk_ring_begin lands 64 exact headings once on a grid of 256; after that every turn is adds and shifts.
// WHAT    : rk_stick reads -4..4 with a dead middle, so a resting stick reads 0 and starts nothing (a latch with a gap, not a gain).
// TRAP    : the first button read after reset can be wrong. The kit ignores the first 20 frames (CI 118).
// LESSON-END
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

// LESSON-BEGIN 6 of 10: Lace: four rules on one store
// RULES   : 1 a heading steps straight. 2 where it stands it lays a lit dot. 3 a carry walks the glass and gives up lit dots
//           as it passes (the forgetting). 4 a dark dot ahead reads as a wall: theta, nothing to land on, so the heading turns.
// RELATION: the network is the configuration of laying against forgetting under the wall rule. No dot stores "network".
// BOOKS   : start + laid + painted = held + forgot + cut. lit is counted by popcount of the whole buffer, never kept as a variable.
// WITNESS : ablation on the C3 version: theta removed, the network turns to haze at every setting (project doc claude/LACE_README.md).
// TRY     : add #define NO_WALL and make reads() return 0 always. Predict: no lanes form; the glass fills, then the carry thins it evenly.
// LESSON-END
// ======================= the sketch =======================
// Lace: the glass is the ground. 8,192 dots, each one reading: lit (a path laid there) or dark (below theta, so a wall).
// Four rules and nothing else: a heading steps straight; where it stands it lays a lit dot; a carry gives up lit dots as it passes
// (the forgetting); a dark dot ahead reads as a wall, so the heading turns toward the open side. No second copy of anything:
// the picture is the readings. The headings are shown by turning their dot over while the frame is sent, then turned back.
// Hands: the stick moves the hand and lays path; hold the press to cut a hole; a short press steps the forgetting bar (8 stops).
// Left alone, a ghost hand wanders, lays, cuts and walks the bar. The row of ticks along the top shows the stop.
// Books, kept to the unit every frame: lit = start + laid + painted - forgot - cut. A frame that does not close inverts the top page.
#ifndef pgm_read_byte
#define pgm_read_byte(a) (*(a))
#endif
#ifndef NK
#define NK 96                                   // heading readings
#endif
#ifndef LPF
#define LPF 6                                   // latches a frame
#endif
#ifndef STRIDE
#define STRIDE 4099                             // the forgetting carry visits dots by an odd step on 8,192: every dot once a lap, no bands
#endif
#ifndef FD
#define FD 1                                    // forgetting carry: FR added a latch, one visit each time it passes FD
#endif
// LESSON-BEGIN 7 of 10: Constants landed once, from a table generator
// WHAT    : RA = 34/256 of a turn landed on 64 (8.5, half to even: 8). SA = 16/256 on 64 = 4. The step and sense tables were
//           generated by tools/lace_tables.cpp and checked exact on all 64 headings before they were pasted.
// CUT     : 64 headings and a 9-dot reach are the cut this build holds. Change either and say which readings moved.
// TRAP    : a constant typed by hand is a number without its steps. Every table here names the program that made it.
// LESSON-END
#define RA 8                                    // turn: 34 of 256 landed on 64 (8.5, half to even: 8)
#define SA 4                                    // sensing spread: 16 of 256 on 64
// generated by tools/lace_tables.cpp: ring exact on all 64; step 8 sixteenths... (1/8 dot units), sense reach 72 (9 dots)
static const int8_t STX[64] PC_ROM = { 8,8,8,8,7,7,7,6,6,5,4,4,3,2,2,1,0,-1,-2,-2,-3,-4,-4,-5,-6,-6,-7,-7,-7,-8,-8,-8,-8,-8,-8,-8,-7,-7,-7,-6,-6,-5,-4,-4,-3,-2,-2,-1,0,1,2,2,3,4,4,5,6,6,7,7,7,8,8,8 };
static const int8_t STY[64] PC_ROM = { 0,1,2,2,3,4,4,5,6,6,7,7,7,8,8,8,8,8,8,8,7,7,7,6,6,5,4,4,3,2,2,1,0,-1,-2,-2,-3,-4,-4,-5,-6,-6,-7,-7,-7,-8,-8,-8,-8,-8,-8,-8,-7,-7,-7,-6,-6,-5,-4,-4,-3,-2,-2,-1 };
static const int8_t SNX[64] PC_ROM = { 72,72,71,69,67,64,60,56,51,46,40,34,27,21,14,7,0,-7,-14,-21,-27,-34,-40,-46,-51,-56,-60,-64,-67,-69,-71,-72,-72,-72,-71,-69,-67,-64,-60,-56,-51,-46,-40,-34,-27,-21,-14,-7,0,7,14,21,27,34,40,46,51,56,60,64,67,69,71,72 };
static const int8_t SNY[64] PC_ROM = { 0,7,14,21,27,34,40,46,51,56,60,64,67,69,71,72,72,72,71,69,67,64,60,56,51,46,40,34,27,21,14,7,0,-7,-14,-21,-27,-34,-40,-46,-51,-56,-60,-64,-67,-69,-71,-72,-72,-72,-71,-69,-67,-64,-60,-56,-51,-46,-40,-34,-27,-21,-14,-7 };
static const uint8_t BARFR[8] PC_ROM = { 4, 8, 12, 16, 24, 32, 48, 64 };   // visits a latch, over FD: from 1 visit in 2 latches to 8 a latch
static const uint8_t BM[8] PC_ROM = { 1, 2, 4, 8, 16, 32, 64, 128 };
static uint16_t kx[NK], ky[NK]; static uint8_t kh[NK];   // place in 1/8 dot (wraps by mask), heading of 64
static uint16_t laid, painted, forgot, cut, cursor, facc;   // (counts: they wrap, the book is taken mod 65,536 beside the lit count)
static uint16_t hx, hy; static uint8_t hh, bar = 4, hold_n, idle_n, ghost, shut = 1;
static uint16_t frames; static uint8_t over;                       // over: the headings and hand are turned over in the buffer (what was last sent)
static uint8_t *dot_at(uint8_t x, uint8_t y) { return &pc_frame[((uint16_t)(y >> 3) << 7) + x]; }
static uint8_t lit(uint8_t x, uint8_t y) { return (uint8_t)((*dot_at(x, y) & pgm_read_byte(&BM[y & 7])) != 0); }
static uint8_t reads(uint16_t x, uint16_t y, uint8_t h) {          // 1: a wall ahead (dark). 0: open.
  h &= 63; uint16_t px = (uint16_t)((x + (int16_t)PC_ROM8(&SNX[h])) & 1023), py = (uint16_t)((y + (int16_t)PC_ROM8(&SNY[h])) & 511);
  return (uint8_t)!lit((uint8_t)(px >> 3), (uint8_t)(py >> 3)); }
static void lay(uint8_t x, uint8_t y, uint16_t *book) { uint8_t *p = dot_at(x, y), m = pgm_read_byte(&BM[y & 7]); if (!(*p & m)) { *p |= m; (*book)++; } }
static void cutdot(uint8_t x, uint8_t y) { uint8_t *p = dot_at(x, y), m = pgm_read_byte(&BM[y & 7]); if (*p & m) { *p &= (uint8_t)~m; cut++; } }
static void ground_begin(void) {
  for (uint16_t i = 0; i < 1024; i++) pc_frame[i] = 0xFF;
  laid = painted = forgot = cut = cursor = facc = 0;
  for (uint8_t n = 0; n < NK; n++) { kx[n] = (uint16_t)pc_scatter(0, 1024); ky[n] = (uint16_t)pc_scatter(0, 512); kh[n] = (uint8_t)pc_scatter(0, 64); } }
// LESSON-BEGIN 8 of 10: One latch: forget, then sense, turn, step, lay
// FORGET  : facc += BARFR[bar]; while facc >= FD, clear the dot at cursor and step cursor by STRIDE = 4099. 4099 is odd on
//           8,192, so every dot is visited once a lap and no band forms.
// SENSE   : reads() looks 9 dots ahead at h, h - SA, h + SA. 1 = dark (wall), 0 = open.
// TURN    : open ahead: keep going. Walls both sides and ahead: pick a side by scatter. Otherwise turn toward the open side by RA.
// TRAP    : two random draws as arguments of one call evaluate in a different order on x86 and RISC-V (CI 165). Here each
//           pc_scatter call is its own statement.
// TRY     : set STRIDE 4096. The cursor then visits 8192 / gcd(8192, 4096) = 2 dots. Predict: forgot stops near 2, the glass
//           fills to lit everywhere the headings pass, and the walls vanish. Try STRIDE 4098 (visits 4,096) and say what changes.
// LESSON-END
static void latch(void) {
  facc = (uint16_t)(facc + pgm_read_byte(&BARFR[bar]));               // the forgetting carry
  while (facc >= FD) { facc = (uint16_t)(facc - FD); uint8_t x = (uint8_t)(cursor & 127), y = (uint8_t)(cursor >> 7);
    uint8_t *p = dot_at(x, y), m = pgm_read_byte(&BM[y & 7]); if (*p & m) { *p &= (uint8_t)~m; forgot++; } cursor = (uint16_t)((cursor + STRIDE) & 8191); }
  for (uint8_t n = 0; n < NK; n++) {
    uint16_t x = kx[n], y = ky[n]; uint8_t h = kh[n];
    uint8_t f = reads(x, y, h), l = reads(x, y, (uint8_t)(h - SA)), r = reads(x, y, (uint8_t)(h + SA));
    if (f <= l && f <= r) { }
    else if (f > l && f > r) { h = (uint8_t)(h + (pc_scatter(0, 2) ? RA : -RA)); }
    else if (l < r) h = (uint8_t)(h - RA); else h = (uint8_t)(h + RA);
    h &= 63; x = (uint16_t)((x + (int16_t)PC_ROM8(&STX[h])) & 1023); y = (uint16_t)((y + (int16_t)PC_ROM8(&STY[h])) & 511);
    kx[n] = x; ky[n] = y; kh[n] = h; lay((uint8_t)(x >> 3), (uint8_t)(y >> 3), &laid); } }
// LESSON-BEGIN 9 of 10: Books: counted, compared, shown
// WHAT    : popcount of all 1,024 bytes against 8192 + laid + painted - forgot - cut, in 16-bit arithmetic on both sides.
// WHY     : the counters wrap; the book is taken mod 65,536 on both sides, so a wrap is not a break.
// SHOWN   : shut = 0 inverts the top page on the glass. A refusal nobody can read is a silent refusal (OS 4 E-list).
// TRAP    : the books guard conservation, not correctness. A rule that lays nothing and forgets nothing also balances.
// LESSON-END
static void books(void) { uint16_t s = 0; for (uint16_t i = 0; i < 1024; i++) { uint8_t b = pc_frame[i]; while (b) { b &= (uint8_t)(b - 1); s++; } }
  shut = (uint8_t)(s == (uint16_t)(8192u + laid + painted - forgot - cut)); }
static void hand_cut(void) { for (int8_t dy = -3; dy <= 3; dy++) for (int8_t dx = -3; dx <= 3; dx++) if (dx * dx + dy * dy <= 10) cutdot((uint8_t)((hx >> 3) + dx), (uint8_t)((hy >> 3) + dy)); }
static void overlay(void) {                                         // headings and the hand turned over; the same call turns them back
  for (uint8_t n = 0; n < NK; n++) { uint8_t x = (uint8_t)(kx[n] >> 3), y = (uint8_t)(ky[n] >> 3); *dot_at(x, y) ^= pgm_read_byte(&BM[y & 7]); }
  uint8_t x = (uint8_t)(hx >> 3), y = (uint8_t)(hy >> 3);
  for (int8_t d = -2; d <= 2; d++) { int16_t a = x + d, b = y + d; if (a >= 0 && a < 128) *dot_at((uint8_t)a, y) ^= pgm_read_byte(&BM[y & 7]); if (d && b >= 0 && b < 64) *dot_at(x, (uint8_t)b) ^= pgm_read_byte(&BM[b & 7]); }
  for (uint8_t k = 0; k < 8; k++) pc_frame[k * 3] ^= (uint8_t)((k == bar) ? 3 : 1); }
void setup() { oled_begin(); pinMode(RK_PIN_SW, INPUT_PULLUP); ground_begin(); hx = 512; hy = 256; }
// LESSON-BEGIN 10 of 10: The frame: turn back, read hands, latch LPF times, check, turn over, send
// ORDER   : overlay() turns the headings' dots over for sending and the same call turns them back first thing next frame.
//           The glass holds no second copy, so the overlay must be undone before any rule reads a dot.
// GHOST   : no hand for 120 frames and a ghost wanders, lays, cuts and steps the bar: the bench plays the sketch through.
// TRY     : move books() after overlay(). Predict: shut fails every frame by the count of turned-over dots that were dark.
// LESSON-END
void loop() {
  if (over) { overlay(); over = 0; }                                // turn them back before anything moves
  rk_input();
  uint8_t moved = (rk_sx != 0 || rk_sy != 0), down = rk_held;
  if (moved || down) { idle_n = 0; ghost = 0; } else if (idle_n < 255 && ++idle_n >= 120) ghost = 1;
  if (ghost) { hh = (uint8_t)(hh + ((frames & 1) ? 0 : 1)); hx = (uint16_t)((hx + 4 * (int16_t)PC_ROM8(&STX[hh & 63])) & 1023); hy = (uint16_t)((hy + 4 * (int16_t)PC_ROM8(&STY[hh & 63])) & 511);
    lay((uint8_t)(hx >> 3), (uint8_t)(hy >> 3), &painted); if ((frames & 255) == 255) hand_cut(); if ((frames & 511) == 511) bar = (uint8_t)((bar + 1) & 7); }
  else { hx = (uint16_t)((hx + rk_sx * 8) & 1023); hy = (uint16_t)((hy + rk_sy * 8) & 511); if (moved) lay((uint8_t)(hx >> 3), (uint8_t)(hy >> 3), &painted); }
  if (down) { hold_n++; if (hold_n >= 12) hand_cut(); } else { if (hold_n > 0 && hold_n < 12) bar = (uint8_t)((bar + 1) & 7); hold_n = 0; }
  for (uint8_t k = 0; k < LPF; k++) latch();
  books(); overlay(); over = 1; rk_show(40); frames++;
}
