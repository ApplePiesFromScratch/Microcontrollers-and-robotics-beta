// IRORI 2 (囲炉裏, the hearth): an operating system for everyday ESP32-C3 projects and small cyberdecks.
// ESP32-C3 SuperMini + 2.0" ST7789 (320 x 240 across). One flash, twelve applications, several running at once:
//   PINS        every free GPIO: choose input, output, PWM or analog; switch outputs; see readings live
//   PLOTTER     draw a reading as it goes, a second one against it, and rate(A, B) (theta when B did not move)
//   SERVO       a servo by angle (with sweep), an LED dimmer by duty, or a blink
//   RULES       WHEN something THEN something: eight rules kept in flash, running in the background
//   TERMINAL    the USB serial line, read and written
//   I2C SCAN    who answers on the bus, and what they usually are
//   CALCULATOR  exact fractions (1/3 + 1/6 = 1/2); division by nothing is named THETA, not guessed
//   NOTES       four notes kept in flash
//   TIMER       a stopwatch and a countdown that can switch a pin when done
//   PROFILES    which apps start together and which rules are on: BENCH, PLANT CARE, ROBOT, DECK, EVERYTHING, SENSOR LOG
//   TASKS       start or stop any app (or all twelve), the work each did, latch time, memory, flash copies
//   SETTINGS    controls, a button test, the look, the home screen, the system
// Controls (Settings > CONTROLS: a preset, or what each button does on a tap, a hold and a longer hold; tried for 15 s before kept):
//   STICK + A B C   stick moves, A or stick press selects, B back (hold B: home), C next running app
//   BUTTONS A B C   A next (hold to repeat), B select (hold B: home), C back (hold C: next app)
//   ONE BUTTON (C)  C next, hold C select, hold longer back
//   KEYBOARD ONLY   the Serial Monitor (arrows or WASD, Enter, Esc, Tab) or an M5 CardKB on I2C
//   The strip at the bottom always says what each button does, from the roles chosen.
//   Settings > TEST THE BUTTONS AND STICK shows every press live, and tells it on the Serial Monitor.
//   Hold A or B while powering on: a safe start (factory controls, no apps, rules paused). Three crashes in a row do the same.
// Look: five themes (night, day, high contrast, amber, green), large text in lists, apps hidden from home if unwanted.
// Changes save themselves 3 to 6 s after they stop changing (Settings > SYSTEM can turn that off).
// Left alone for about 30 s, a demo hand tours the apps (Settings > SYSTEM > DEMO turns it off); touching anything stops it.
// Settings, rules, notes, profiles and pin setup are kept in flash twice, each copy with a sum; a keeper mends a bad copy.
// IRORI 1's settings are carried over on the first start. Each pin has one owner (hands, I2C, servo, timer, rules, then PINS).
// Whole numbers only. No independent variable: every changing quantity is a reading; a rate is rate(A, B).
// Checked off the board: desk == the C3's instruction set (QEMU), the demo hand driving both; 46 self-checks with planted faults,
// also under the address and undefined-behaviour sanitizers. Not run on a board yet.
// Wiring (as HITOMI, KEA, HIGH NOON): SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3. Stick VRx 0, VRy 1, press 2. Buttons A 3,
// B 20, C 21 (to ground). Optional I2C: SDA 8, SCL 9 (turn on in Settings; GPIO 8 is also the board's LED). Other wiring: Settings > CONTROLS.
// Arduino IDE: "ESP32C3 Dev Module" (any C3 board works: pins are GPIO numbers), core 3.x, USB CDC On Boot: Enabled.
#pragma GCC optimize("O2")
#include <stdint.h>
#include <string.h>
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_system.h"
#define PIN_SCK 4
#define PIN_MOSI 6
#define PIN_RST 10
#define PIN_DC 5
#define PIN_CS 7
#define PIN_VRX 0
#define PIN_VRY 1
#define PIN_JSW 2
#define PIN_A 3
#define PIN_B 20
#define PIN_C 21
#define WIRE_HZ 40000000
// ---------------------------------------------------------------- IRORI itself (core/irori.h, pasted by tools/make_ino.py)
/* irori.h  IRORI (囲炉裏, the hearth): an operating system for the ESP32-C3 SuperMini + 2.0" ST7789 (320 x 240), for everyday
   hobby projects and small cyberdecks. One flash, many applications: apps run layered (several at once, in the background) and
   PROFILES say which ones start together. Everything on screen is labelled; focus moves with the stick or buttons; A opens,
   B goes back, C switches between running apps. Typing comes from the USB Serial Monitor or an M5 CardKB on I2C.
   Whole numbers only. No independent variable: every changing quantity is a reading, and a rate is always rate(A, B), the
   change of one reading against the change of another, theta when B did not change. The clock is read as a count it changed.
   This file is the whole OS: shared by desk/desk.c (the desk twin), bare/emu.c (the C3's instruction set in QEMU) and the sketch.
   The hardware is reached only through the hal_* functions, which each of those three provides. */
/* ---------------------------------------------------------------- every type first (the Arduino IDE writes its prototypes above the first function) */
typedef struct { uint8_t type; uint8_t ch; } ievent_t;
typedef struct { int64_t n, d; uint8_t bad; } exact_t;                 /* an exact fraction; bad: 0 ok, 1 theta, 2 wide */
typedef struct {
  const char *name; const char *icon[16]; uint8_t colour, background;
  void (*draw)(void); void (*tick)(void); uint16_t weight;            /* weight: work units a background tick declares */
} app_t;
typedef struct { uint8_t on, when, in_pin, then, out_pin; int16_t value, out_value; uint8_t last, spare; } rule_t;   /* last, spare: unused since IRORI 2 (kept for the layout) */
typedef struct { char name[12]; uint16_t apps; uint8_t rules; uint8_t spare; } profile_t;
typedef struct {                                                       /* IRORI 1's keeping, read once to carry settings over */
  uint32_t magic; uint16_t version, size;
  uint8_t input_mode, i2c_on, demo_on, invert, rot180, bgr, boot_profile, spare0;
  uint8_t pin_mode[8]; uint8_t plot_a, plot_b, plot_every, servo_pin, servo_mode, spare1; int16_t servo_value;
  rule_t rules[8]; profile_t prof[4]; char notes[4][160]; char sendbox[40];
  uint32_t sum;
} store1_t;
typedef struct {                                                       /* the hands: what each button does on a tap, a hold and a long hold, and where it is wired */
  uint8_t preset, stick_on, vx, vy, inv_x, inv_y, swap_xy, spare;
  uint8_t tap[4], hold[4], lng[4], pin[4];                             /* buttons A, B, C and the stick's PRESS; pin 255: not fitted */
  int16_t cx, cy, dead, hold_ms, rep_delay, rep_ms, debounce, spare2;  /* stick centre and dead zone (raw counts); times in ms of clock change */
} controls_t;
#define NPROF 6
typedef struct {                                                       /* everything kept across power-off: two copies, each with a sum */
  uint32_t magic; uint16_t version, size;
  controls_t ctl;
  uint8_t i2c_on, demo_on, invert, rot180, bgr, boot_profile, theme, big_text, autosave, spare0; uint16_t home_hide;
  uint8_t pin_mode[8]; uint8_t plot_a, plot_b, plot_every, servo_pin, servo_mode, spare1; int16_t servo_value;
  rule_t rules[8]; profile_t prof[NPROF]; char notes[4][160]; char sendbox[40];
  uint32_t sum;
} store_t;
typedef struct { uint8_t kind, a, b, wait; } gstep_t;

/* ---------------------------------------------------------------- the hardware, provided by each build */
static int  hal_pin_read(int pin);
static void hal_pin_write(int pin, int v);
static void hal_pin_mode(int pin, int mode);                           /* 0 input with pull-up, 1 output, 2 PWM, 3 analog */
static int  hal_adc(int pin);                                          /* 0..4095 */
static void hal_pwm(int pin, int permille);
static void hal_servo(int pin, int degrees);
static int  hal_i2c_probe(int addr);                                   /* 1 if a device answered */
static int  hal_serial_read(void);                                     /* a byte, or -1 */
static void hal_serial_write(const char *s);
static uint32_t hal_clock_ms(void);                                    /* a reading of the clock; only its change is used */
static uint8_t hal_store_get(int addr);
static void hal_store_put(int addr, uint8_t v);
static void hal_store_commit(void);
static int  hal_cardkb(void);                                          /* a key from an M5 CardKB at 0x5F, or -1 */
static void hal_panel_mode(int invert, int rot180, int bgr);
static uint32_t hal_free_memory(void);
static void hal_buttons(uint8_t down[4], uint8_t presses[4]);         /* each button now (1 pressed), and presses counted since the last call (a tap between latches) */
static int  hal_stick(int *x, int *y);                                 /* the stick's two readings 0..4095; 0 if no stick answers */
static void hal_inputs(const controls_t *c);                           /* the buttons' or stick's wiring changed: set those pins up */
static void hal_palette(void);                                         /* the colours changed: the panel side re-reads PAL */
static int  hal_boot_trouble(void);                                    /* starts in a row that followed a crash, watchdog or brown-out; 100 more if A or B was held at power-on */

/* ---------------------------------------------------------------- screen: one byte a dot through a palette */
#define SW 320
#define SH 240
static uint8_t FB[SW * SH];
enum { K_BG, K_BG2, K_BAR, K_PANEL, K_LINE, K_TEXT, K_DIM, K_ACCENT, K_FOCUS, K_GOOD, K_WARN, K_BAD, K_TITLE, K_KEY, K_PLOT, K_PLOT2,
       K_SHADOW, K_WHITE, K_APP0, NCOLOUR = K_APP0 + 12 };
static uint8_t PALRGB[NCOLOUR][3] = {                                  /* the colours now; the first 18 come from the theme */
  { 14, 20, 34 }, { 20, 28, 46 }, { 8, 12, 22 }, { 26, 36, 58 }, { 52, 66, 96 }, { 232, 238, 246 }, { 140, 156, 180 }, { 70, 200, 210 },
  { 255, 196, 70 }, { 90, 220, 120 }, { 245, 170, 50 }, { 240, 80, 70 }, { 36, 120, 150 }, { 44, 56, 84 }, { 90, 230, 255 }, { 255, 130, 200 },
  { 6, 9, 16 }, { 255, 255, 255 },
  { 90, 200, 255 }, { 120, 230, 140 }, { 255, 170, 90 }, { 200, 140, 255 }, { 150, 230, 230 }, { 255, 210, 90 }, { 255, 140, 160 },
  { 230, 230, 140 }, { 140, 200, 255 }, { 190, 160, 255 }, { 160, 240, 190 }, { 210, 210, 220 } };
#define NTHEME 5
static const char *THEME_NAME[NTHEME] = { "NIGHT", "DAY", "HIGH CONTRAST", "AMBER DECK", "GREEN TERMINAL" };
static const uint8_t THEME[NTHEME][K_APP0][3] = {
  { { 14, 20, 34 }, { 20, 28, 46 }, { 8, 12, 22 }, { 26, 36, 58 }, { 52, 66, 96 }, { 232, 238, 246 }, { 140, 156, 180 }, { 70, 200, 210 },
    { 255, 196, 70 }, { 90, 220, 120 }, { 245, 170, 50 }, { 240, 80, 70 }, { 36, 120, 150 }, { 44, 56, 84 }, { 90, 230, 255 }, { 255, 130, 200 }, { 6, 9, 16 }, { 255, 255, 255 } },
  { { 236, 240, 246 }, { 226, 232, 240 }, { 196, 206, 222 }, { 212, 220, 232 }, { 150, 165, 190 }, { 20, 28, 44 }, { 80, 94, 118 }, { 0, 120, 140 },
    { 214, 100, 0 }, { 20, 140, 60 }, { 190, 100, 0 }, { 200, 40, 40 }, { 36, 96, 168 }, { 176, 190, 212 }, { 0, 100, 200 }, { 200, 30, 130 }, { 250, 252, 255 }, { 255, 255, 255 } },
  { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 24, 24, 24 }, { 255, 255, 255 }, { 255, 255, 255 }, { 210, 210, 210 }, { 0, 255, 255 },
    { 255, 255, 0 }, { 0, 255, 0 }, { 255, 200, 0 }, { 255, 70, 70 }, { 0, 0, 170 }, { 48, 48, 48 }, { 0, 255, 255 }, { 255, 0, 255 }, { 0, 0, 0 }, { 255, 255, 255 } },
  { { 16, 10, 4 }, { 22, 14, 6 }, { 8, 5, 2 }, { 36, 24, 10 }, { 90, 60, 20 }, { 255, 190, 90 }, { 170, 120, 60 }, { 255, 150, 40 },
    { 255, 232, 150 }, { 200, 255, 120 }, { 255, 120, 40 }, { 255, 70, 50 }, { 120, 70, 10 }, { 54, 36, 14 }, { 255, 180, 60 }, { 255, 110, 60 }, { 6, 4, 2 }, { 255, 236, 200 } },
  { { 4, 14, 8 }, { 6, 20, 10 }, { 2, 8, 4 }, { 10, 34, 18 }, { 30, 90, 50 }, { 120, 255, 150 }, { 70, 170, 100 }, { 80, 255, 200 },
    { 220, 255, 120 }, { 120, 255, 120 }, { 255, 220, 80 }, { 255, 90, 80 }, { 20, 90, 50 }, { 14, 44, 24 }, { 100, 255, 160 }, { 200, 255, 100 }, { 2, 6, 3 }, { 220, 255, 230 } } };
static uint8_t APPRGB[12][3];
static void theme_apply(int t) { t = t < 0 || t >= NTHEME ? 0 : t; static int kept; if (!kept) { for (int k = 0; k < 12; k++) for (int j = 0; j < 3; j++) APPRGB[k][j] = PALRGB[K_APP0 + k][j]; kept = 1; }
  for (int k = 0; k < K_APP0; k++) for (int j = 0; j < 3; j++) PALRGB[k][j] = THEME[t][k][j];
  for (int k = 0; k < 12; k++) for (int j = 0; j < 3; j++) PALRGB[K_APP0 + k][j] = (uint8_t)(t == 1 ? APPRGB[k][j] * 3 / 5 : APPRGB[k][j]); }   /* the day theme darkens the icons to stand on a light tile */
static int clipx0 = 0, clipy0 = 0, clipx1 = SW, clipy1 = SH;
static void px(int x, int y, uint8_t c) { if (x >= clipx0 && x < clipx1 && y >= clipy0 && y < clipy1) FB[y * SW + x] = c; }
static void fill(int x, int y, int w, int h, uint8_t c) {
  int x0 = x < clipx0 ? clipx0 : x, y0 = y < clipy0 ? clipy0 : y, x1 = x + w > clipx1 ? clipx1 : x + w, y1 = y + h > clipy1 ? clipy1 : y + h;
  for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) FB[yy * SW + xx] = c; }
static void box(int x, int y, int w, int h, uint8_t c) { fill(x, y, w, 1, c); fill(x, y + h - 1, w, 1, c); fill(x, y, 1, h, c); fill(x + w - 1, y, 1, h, c); }
static void hline(int x0, int x1, int y, uint8_t c) { if (x1 < x0) { int t = x0; x0 = x1; x1 = t; } fill(x0, y, x1 - x0 + 1, 1, c); }
static void vline(int x, int y0, int y1, uint8_t c) { if (y1 < y0) { int t = y0; y0 = y1; y1 = t; } fill(x, y0, 1, y1 - y0 + 1, c); }

/* ---------------------------------------------------------------- letters: 5x7 (OS 4's face, with a few more) */
static const char FACE_CH[] = "0123456789ABCDEFGHIKLMNOPRSTUVWXY:-!.,/%=+()'?JQZ><*_#;\"&@[]|";
static const uint8_t FACE[61][7] = {
  {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31}, {31,2,4,2,1,17,14}, {2,6,10,18,31,2,2},
  {31,16,30,1,1,17,14}, {6,8,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14}, {14,17,17,15,1,2,12},
  {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31},
  {31,16,16,30,16,16,16}, {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14}, {17,18,20,24,20,18,17},
  {16,16,16,16,16,16,31}, {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
  {30,17,17,30,20,18,17}, {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14}, {17,17,17,17,10,10,4},
  {17,17,17,21,21,27,17}, {17,17,10,4,10,17,17}, {17,17,10,4,4,4,4}, {0,12,12,0,12,12,0}, {0,0,0,31,0,0,0}, {4,4,4,4,4,0,4},
  {0,0,0,0,0,12,12}, {0,0,0,0,12,4,8}, {1,1,2,4,8,16,16}, {24,25,2,4,8,19,3}, {0,0,31,0,31,0,0}, {0,4,4,31,4,4,0}, {2,4,8,8,8,4,2},
  {8,4,2,2,2,4,8}, {4,4,8,0,0,0,0}, {14,17,1,2,4,0,4}, {7,2,2,2,2,18,12}, {14,17,17,17,21,18,13}, {31,1,2,4,8,16,31}, {8,4,2,1,2,4,8},
  {2,4,8,16,8,4,2}, {0,4,21,14,21,4,0}, {0,0,0,0,0,0,31}, {10,10,31,10,31,10,10}, {0,12,12,0,12,4,8}, {10,10,0,0,0,0,0},
  {12,18,20,8,21,18,13}, {14,17,23,21,23,16,14}, {14,8,8,8,8,8,14}, {14,2,2,2,2,2,14}, {4,4,4,4,4,4,4} };
static uint8_t GLYPH[128];
static void face_begin(void) { for (int i = 0; FACE_CH[i]; i++) GLYPH[(uint8_t)FACE_CH[i]] = (uint8_t)(i + 1); }
static int text(int x, int y, const char *s, uint8_t c, int sc) {      /* returns where the next letter goes */
  for (; *s; s++) { int ch = (uint8_t)*s; if (ch >= 'a' && ch <= 'z') ch -= 32; int g = ch < 128 ? GLYPH[ch] : 0;
    if (g) for (int r = 0; r < 7; r++) { uint8_t b = FACE[g - 1][r]; for (int k = 0; k < 5; k++) if (b >> (4 - k) & 1) { if (sc == 1) px(x + k, y + r, c); else fill(x + k * sc, y + r * sc, sc, sc, c); } }
    x += 6 * sc; }
  return x; }
static int text_w(const char *s, int sc) { int n = 0; while (s[n]) n++; return n * 6 * sc - sc; }
static void text_c(int cx, int y, const char *s, uint8_t c, int sc) { text(cx - text_w(s, sc) / 2, y, s, c, sc); }
static int text_fit(int x, int y, const char *s, uint8_t c, int sc, int maxw) {   /* as many letters as fit in maxw; a cut is marked with two dots */
  int n = 0; while (s[n]) n++; int fit = (maxw + sc) / (6 * sc); if (fit < 0) fit = 0; if (n <= fit) return text(x, y, s, c, sc);
  char b[64]; int k = fit - 1; if (k < 0) k = 0; if (k > 62) k = 62; for (int i = 0; i < k; i++) b[i] = s[i]; b[k] = 0; int e = text(x, y, b, c, sc);
  if (fit >= 1) { fill(e, y + 6 * sc, sc, sc, c); fill(e + 2 * sc, y + 6 * sc, sc, sc, c); } return e + 4 * sc; }

/* ---------------------------------------------------------------- small whole-number words (no libc: the bare build has none) */
static int s_len(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void s_cpy(char *d, const char *s, int cap) { int i = 0; while (s[i] && i < cap - 1) { d[i] = s[i]; i++; } d[i] = 0; }
static void s_cat(char *d, const char *s, int cap) { int n = s_len(d); s_cpy(d + n, s, cap - n); }
static void s_u(char *b, uint32_t v) { char t[11]; int n = 0; do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v); for (int i = 0; i < n; i++) b[i] = t[n - 1 - i]; b[n] = 0; }
static void s_i(char *b, int64_t v) { if (v < 0) { b[0] = '-'; v = -v; b++; } char t[21]; int n = 0; uint64_t u = (uint64_t)v; do { t[n++] = (char)('0' + u % 10); u /= 10; } while (u); for (int i = 0; i < n; i++) b[i] = t[n - 1 - i]; b[n] = 0; }
static void s_cat_i(char *d, int64_t v, int cap) { char b[24]; s_i(b, v); s_cat(d, b, cap); }
static void s_two(char *d, uint32_t v, int cap) { char b[3] = { (char)('0' + v / 10 % 10), (char)('0' + v % 10), 0 }; s_cat(d, b, cap); }
static void s_hex2(char *d, int v, int cap) { const char *H = "0123456789ABCDEF"; char b[3] = { H[(v >> 4) & 15], H[v & 15], 0 }; s_cat(d, b, cap); }
static int32_t i_min(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t i_max(int32_t a, int32_t b) { return a > b ? a : b; }
static int32_t i_clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static int64_t i_gcd(int64_t a, int64_t b) { if (a < 0) a = -a; if (b < 0) b = -b; while (b) { int64_t t = a % b; a = b; b = t; } return a ? a : 1; }
static int64_t mouth(int64_t top, int64_t den) {                        /* the only landing: nearest whole, half to even, symmetric about zero */
  int64_t q = top / den, r = top - q * den, a = r < 0 ? -r : r; if (2 * a > den || (2 * a == den && (q & 1))) q += top < 0 ? -1 : 1; return q; }

/* ---------------------------------------------------------------- exact fractions (the calculator, and rate(A, B)) */
#define EXACT_MAX ((int64_t)1 << 52)
static exact_t ex_make(int64_t n, int64_t d) { exact_t e; e.bad = 0; if (d == 0) { e.n = 0; e.d = 1; e.bad = 1; return e; } if (d < 0) { n = -n; d = -d; }
  int64_t g = i_gcd(n, d); e.n = n / g; e.d = d / g; if (e.n > EXACT_MAX || e.n < -EXACT_MAX || e.d > EXACT_MAX) e.bad = 2; return e; }
static int ex_fits(int64_t a, int64_t b) { int64_t aa = a < 0 ? -a : a, bb = b < 0 ? -b : b; return aa == 0 || bb == 0 || aa <= ((int64_t)1 << 62) / bb; }
static exact_t ex_op(exact_t a, exact_t b, char op) {
  exact_t r; r.n = 0; r.d = 1; r.bad = a.bad ? a.bad : b.bad; if (r.bad) return r;
  if (op == '/' && b.n == 0) { r.bad = 1; return r; }
  int64_t n1 = a.n, d1 = a.d, n2 = b.n, d2 = b.d;
  if (op == '/') { n2 = b.d; d2 = b.n; op = 'x'; if (d2 < 0) { d2 = -d2; n2 = -n2; } }
  if (op == 'x') { if (!ex_fits(n1, n2) || !ex_fits(d1, d2)) { r.bad = 2; return r; } return ex_make(n1 * n2, d1 * d2); }
  if (!ex_fits(n1, d2) || !ex_fits(n2, d1) || !ex_fits(d1, d2)) { r.bad = 2; return r; }
  return ex_make(op == '+' ? n1 * d2 + n2 * d1 : n1 * d2 - n2 * d1, d1 * d2); }
static void ex_text(char *b, exact_t e, int cap) {                     /* "7/3 = 2.333" ; theta and wide named */
  b[0] = 0; if (e.bad == 1) { s_cpy(b, "THETA: NOTHING TO COMPARE AGAINST", cap); return; } if (e.bad == 2) { s_cpy(b, "WIDE: TOO BIG TO KEEP EXACT", cap); return; }
  s_cat_i(b, e.n, cap); if (e.d != 1) { s_cat(b, "/", cap); s_cat_i(b, e.d, cap);
    if (ex_fits(e.n, 1000)) { int64_t m = mouth(e.n * 1000, e.d); s_cat(b, " = ", cap); if (m < 0) { s_cat(b, "-", cap); m = -m; } s_cat_i(b, m / 1000, cap); s_cat(b, ".", cap);
      char f[4] = { (char)('0' + m / 100 % 10), (char)('0' + m / 10 % 10), (char)('0' + m % 10), 0 }; s_cat(b, f, cap); } } }

/* ---------------------------------------------------------------- input: events, one a latch */
enum { IE_NONE, IE_UP, IE_DOWN, IE_LEFT, IE_RIGHT, IE_OK, IE_BACK, IE_SWITCH, IE_CHAR, IE_NEXT, IE_PREV, IE_HOME };   /* NEXT/PREV: one step in reading order, whatever the screen's shape */
#define EQN 16
static ievent_t EQ[EQN]; static int eq_in, eq_out;
static void os_push(uint8_t type, uint8_t ch) { if (eq_in - eq_out >= EQN) return; EQ[eq_in % EQN].type = type; EQ[eq_in % EQN].ch = ch; eq_in++; }
static ievent_t ev;                                                    /* the event this latch; a part that uses it sets ev.type = IE_NONE */
static void os_key(int c, int text_mode) {                             /* a byte from the Serial Monitor or a CardKB, as an event */
  static int esc = 0;
  if (esc == 1) { esc = c == '[' ? 2 : 0; if (esc) return; }
  else if (esc == 2) { esc = 0; if (c == 'A') os_push(IE_UP, 0); else if (c == 'B') os_push(IE_DOWN, 0); else if (c == 'C') os_push(IE_RIGHT, 0); else if (c == 'D') os_push(IE_LEFT, 0); return; }
  if (c == 27) { esc = 1; os_push(IE_BACK, 0); return; }
  if (c == 0xB5) { os_push(IE_UP, 0); return; } if (c == 0xB6) { os_push(IE_DOWN, 0); return; } if (c == 0xB4) { os_push(IE_LEFT, 0); return; } if (c == 0xB7) { os_push(IE_RIGHT, 0); return; }
  if (c == '\r' || c == '\n') { os_push(IE_OK, 0); return; } if (c == '\t') { os_push(IE_SWITCH, 0); return; }
  if (c == 8 || c == 127) { os_push(IE_CHAR, 8); return; }
  if (!text_mode) { if (c == 'w' || c == 'W') { os_push(IE_UP, 0); return; } if (c == 's' || c == 'S') { os_push(IE_DOWN, 0); return; }
    if (c == 'a' || c == 'A') { os_push(IE_LEFT, 0); return; } if (c == 'd' || c == 'D') { os_push(IE_RIGHT, 0); return; }
    if (c == ' ') { os_push(IE_OK, 0); return; } if (c == 'q' || c == 'Q') { os_push(IE_BACK, 0); return; } }
  if (c >= 32 && c < 127) os_push(IE_CHAR, (uint8_t)c); }

/* ---------------------------------------------------------------- the hands' roles and presets */
static store_t ST;
static uint32_t latch, idle, dclock, clock_total, clock_last;          /* the clock is kept only as what it changed */
enum { R_NONE, R_SELECT, R_BACK, R_NEXT, R_PREV, R_NEXTAPP, R_HOME, NROLE };
static const char *ROLE_NAME[NROLE] = { "NOTHING", "SELECT", "BACK", "NEXT", "PREVIOUS", "NEXT APP", "HOME" };
static const char *BT_NAME[4] = { "A", "B", "C", "PRESS" };
#define NPRESET 5
static const char *PRESET_NAME[NPRESET] = { "STICK + A B C", "BUTTONS A B C", "ONE BUTTON (C)", "KEYBOARD ONLY", "MY OWN" };
static void controls_preset(controls_t *c, int k) {                    /* the roles of a preset; wiring, stick centre and times are kept */
  if (k >= 4) { c->preset = 4; return; } c->preset = (uint8_t)k;
  for (int b = 0; b < 4; b++) { c->tap[b] = R_NONE; c->hold[b] = R_NONE; c->lng[b] = R_NONE; } c->stick_on = 0;
  if (k == 0) { c->stick_on = 1; c->tap[0] = R_SELECT; c->tap[1] = R_BACK; c->hold[1] = R_HOME; c->tap[2] = R_NEXTAPP; c->tap[3] = R_SELECT; }
  if (k == 1) { c->tap[0] = R_NEXT; c->tap[1] = R_SELECT; c->hold[1] = R_HOME; c->tap[2] = R_BACK; c->hold[2] = R_NEXTAPP; }
  if (k == 2) { c->tap[2] = R_NEXT; c->hold[2] = R_SELECT; c->lng[2] = R_BACK; } }
static void controls_factory(controls_t *c) {
  uint8_t *p = (uint8_t *)c; for (size_t i = 0; i < sizeof *c; i++) p[i] = 0;
  c->pin[0] = 3; c->pin[1] = 20; c->pin[2] = 21; c->pin[3] = 2; c->vx = 0; c->vy = 1; c->cx = 2048; c->cy = 2048; c->dead = 1200;
  c->hold_ms = 700; c->rep_delay = 450; c->rep_ms = 130; c->debounce = 25; controls_preset(c, 0); }
static int keys_mode(void) { return ST.ctl.preset == 3; }

/* ---------------------------------------------------------------- the keeping: two copies, each with a sum; a keeper mends; changes save themselves */
#define STORE_MAGIC 0x49524F52u
static uint32_t fnv(const uint8_t *p, size_t n) { uint32_t h = 2166136261u; for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u; return h; }
static uint32_t st_sum(const store_t *s) { return fnv((const uint8_t *)s, offsetof(store_t, sum)); }
static int copy_ok(int c) { store_t t; uint8_t *p = (uint8_t *)&t; for (size_t i = 0; i < sizeof t; i++) p[i] = hal_store_get((int)(c * sizeof t + i));
  return t.magic == STORE_MAGIC && t.version == 2 && t.size == sizeof t && t.sum == st_sum(&t); }
static void copy_load(int c) { uint8_t *p = (uint8_t *)&ST; for (size_t i = 0; i < sizeof ST; i++) p[i] = hal_store_get((int)(c * sizeof ST + i)); }
static void copy_write(int c) { const uint8_t *p = (const uint8_t *)&ST; for (size_t i = 0; i < sizeof ST; i++) hal_store_put((int)(c * sizeof ST + i), p[i]); }
static uint32_t saves, mends, saved_sum, pending_sum, save_clock; static int carried;
static char toast_text[48]; static uint32_t toast_ms;
static void toast(const char *t) { s_cpy(toast_text, t, 48); toast_ms = 2500; }
static void os_save(void) { ST.sum = st_sum(&ST); copy_write(0); copy_write(1); hal_store_commit(); saves++; saved_sum = ST.sum; pending_sum = 0; }
static void keeper_turn(void) {                                        /* a good copy over a bad one */
  int a = copy_ok(0), b = copy_ok(1); if (a == b) return;
  store_t keep = ST; copy_load(a ? 0 : 1); copy_write(a ? 1 : 0); hal_store_commit(); ST = keep; mends++; }   /* the kept copy is mended; what is on screen stays */
static void autosave_latch(int quiet) {                                /* a change is saved once it has held still for one look (3 s of clock change) */
  save_clock += dclock; if (save_clock < 3000) return; save_clock = 0;
  uint32_t s = st_sum(&ST); if (s == saved_sum || quiet || !ST.autosave) { pending_sum = 0; return; }
  if (s == pending_sum) { os_save(); toast("SAVED"); } else pending_sum = s; }
static const char *PROF_NAME[NPROF] = { "BENCH", "PLANT CARE", "ROBOT", "DECK", "EVERYTHING", "SENSOR LOG" };
static void store_factory(void) {
  uint8_t *p = (uint8_t *)&ST; for (size_t i = 0; i < sizeof ST; i++) p[i] = 0;
  ST.magic = STORE_MAGIC; ST.version = 2; ST.size = (uint16_t)sizeof ST;
  controls_factory(&ST.ctl); ST.i2c_on = 0; ST.demo_on = 1; ST.invert = 1; ST.rot180 = 0; ST.bgr = 0; ST.boot_profile = 0; ST.theme = 0; ST.big_text = 0; ST.autosave = 1; ST.home_hide = 0;
  for (int i = 0; i < 8; i++) ST.pin_mode[i] = 0;
  ST.plot_a = 0; ST.plot_b = 1; ST.plot_every = 0; ST.servo_pin = 9; ST.servo_mode = 0; ST.servo_value = 90;
  /* rules: a plant waterer, a night light, a blink; off until a profile turns them on */
  ST.rules[0].when = 3; ST.rules[0].in_pin = 0; ST.rules[0].value = 1500; ST.rules[0].then = 0; ST.rules[0].out_pin = 8;   /* analog below 1500: pump on */
  ST.rules[1].when = 2; ST.rules[1].in_pin = 0; ST.rules[1].value = 2200; ST.rules[1].then = 1; ST.rules[1].out_pin = 8;   /* analog above 2200: pump off */
  ST.rules[2].when = 4; ST.rules[2].value = 2; ST.rules[2].then = 2; ST.rules[2].out_pin = 8;                              /* every 2 s: toggle */
  ST.rules[3].when = 4; ST.rules[3].value = 60; ST.rules[3].then = 5;                                                      /* every minute: a line on serial (a logger's heartbeat) */
  for (int k = 0; k < NPROF; k++) s_cpy(ST.prof[k].name, PROF_NAME[k], 12);
  ST.prof[0].apps = (1 << 0) | (1 << 1) | (1 << 3);  ST.prof[0].rules = 4;                                                 /* bench: pins, plotter, rules (rule 3 blinks the LED) */
  ST.prof[1].apps = (1 << 3) | (1 << 1);  ST.prof[1].rules = 3;                                                            /* plant: rules 1-2, plotter */
  ST.prof[2].apps = (1 << 2) | (1 << 0);  ST.prof[2].rules = 0;                                                            /* robot: servo, pins */
  ST.prof[3].apps = (1 << 4) | (1 << 7) | (1 << 6);  ST.prof[3].rules = 0;                                                 /* deck: terminal, notes, calculator */
  ST.prof[4].apps = 0x0FFF;  ST.prof[4].rules = 4;                                                                         /* everything: all twelve, the blink */
  ST.prof[5].apps = (1 << 1) | (1 << 4) | (1 << 5) | (1 << 8);  ST.prof[5].rules = 8;                                      /* sensor log: plotter, terminal, I2C, timer, the heartbeat */
  s_cpy(ST.notes[0], "Welcome to IRORI. Notes are kept in flash.", 160);
  s_cpy(ST.notes[1], "Wiring: SCK 4 MOSI 6 DC 5 CS 7 RST 10. Stick 0 1 press 2. Buttons A 3 B 20 C 21. I2C SDA 8 SCL 9 (Settings).", 160);
  s_cpy(ST.notes[2], "GPIO 8 is the board LED (on when LOW). Hold A or B while powering on for a safe start.", 160);
  s_cpy(ST.sendbox, "hello from irori", 40); }
static int carry_v1(void) {                                            /* IRORI 1's settings, notes, rules and profiles, carried into IRORI 2 */
  for (int c = 0; c < 2; c++) { store1_t o; uint8_t *p = (uint8_t *)&o; for (size_t i = 0; i < sizeof o; i++) p[i] = hal_store_get((int)(c * sizeof o + i));
    if (o.magic != STORE_MAGIC || o.version != 1 || o.size != sizeof o || o.sum != fnv(p, offsetof(store1_t, sum))) continue;
    store_factory(); controls_preset(&ST.ctl, o.input_mode == 1 ? 1 : o.input_mode == 2 ? 2 : o.input_mode == 3 ? 3 : 0);
    ST.i2c_on = o.i2c_on; ST.demo_on = o.demo_on; ST.invert = o.invert; ST.rot180 = o.rot180; ST.bgr = o.bgr; ST.boot_profile = o.boot_profile & 3;
    for (int i = 0; i < 8; i++) ST.pin_mode[i] = o.pin_mode[i];
    ST.plot_a = o.plot_a; ST.plot_b = o.plot_b; ST.plot_every = o.plot_every; ST.servo_pin = o.servo_pin; ST.servo_mode = o.servo_mode; ST.servo_value = o.servo_value;
    for (int r = 0; r < 8; r++) ST.rules[r] = o.rules[r]; for (int k = 0; k < 4; k++) ST.prof[k] = o.prof[k];
    for (int k = 0; k < 4; k++) s_cpy(ST.notes[k], o.notes[k], 160); s_cpy(ST.sendbox, o.sendbox, 40); return 1; }
  return 0; }
static void store_begin(void) {
  if (copy_ok(0)) copy_load(0); else if (copy_ok(1)) copy_load(1);
  else if (carry_v1()) { carried = 1; os_save(); } else { store_factory(); os_save(); }
  saved_sum = st_sum(&ST); pending_sum = 0; }

/* ---------------------------------------------------------------- pins: the user's GPIOs, and who else uses them */
static const uint8_t UPIN[8] = { 0, 1, 2, 3, 8, 9, 20, 21 };
static uint16_t running;                                               /* apps running (bit per app) */
static uint8_t cd_alarm_pin_on; static int16_t cd_pin_i = 4;           /* the timer's alarm pin (declared here: it can claim a pin) */
enum { U_PINS, U_RULES, U_TIMER, U_SERVO };
static char own_buf[16];
static const char *pin_user(int pin, int me) {                         /* who else uses a pin, as seen by part me; one owner a pin: hands, I2C, SERVO, TIMER, RULES, then PINS */
  const controls_t *c = &ST.ctl;
  if (c->stick_on && (pin == c->vx || pin == c->vy)) return "STICK";
  for (int b = 0; b < 4; b++) if (c->pin[b] == pin && (c->tap[b] || c->hold[b] || c->lng[b])) { s_cpy(own_buf, b == 3 ? "STICK PRESS" : "BUTTON ", 16); if (b < 3) s_cat(own_buf, BT_NAME[b], 16); return own_buf; }
  if (ST.i2c_on && (pin == 8 || pin == 9)) return "I2C";
  if (me != U_SERVO && (running >> 2 & 1) && ST.servo_pin == pin) return "SERVO";
  if (me != U_SERVO && me != U_TIMER && cd_alarm_pin_on && UPIN[cd_pin_i & 7] == pin) return "TIMER";
  if (me == U_PINS) for (int k = 0; k < 8; k++) if (ST.rules[k].on && ST.rules[k].then < 5 && ST.rules[k].out_pin == pin) return "RULES";
  return 0; }
static int is_analog(int pin) { return pin <= 4; }
static int pin_index(int pin) { for (int i = 0; i < 8; i++) if (UPIN[i] == pin) return i; return -1; }
static void pin_apply(int i) { int pin = UPIN[i]; if (pin_user(pin, U_PINS)) return; int m = ST.pin_mode[i]; if (m == 3 && !is_analog(pin)) m = 0; hal_pin_mode(pin, m); }

/* ---------------------------------------------------------------- the frame: bars, home, windows, forms */
static int fg = -1;                                                    /* the app in front; -1 = home */
static int home_focus;
static uint32_t work_now[12], work_all[12];
static int kb_on; static char *kb_buf; static int kb_cap, kb_row, kb_col, kb_shift; static char kb_title[40];
#define NAPP 12
static const app_t *apps_table(void);
#define APPS (apps_table())
static int ghost_on, safe_mode;
static void open_app(int a) { if (a < 0 || a >= NAPP) return; running |= (uint16_t)(1u << a); fg = a; }
static void stop_app(int a) { running &= (uint16_t)~(1u << a); if (fg == a) fg = -1;
  if (a == 2) { int i = pin_index(ST.servo_pin); if (i >= 0) pin_apply(i); } }                  /* the servo's pin goes back to what PINS says */
static void switch_app(void) {                                         /* the next running app after the one in front; home after the last */
  for (int k = 1; k <= NAPP + 1; k++) { int a = (fg + 1 + k) % (NAPP + 1) - 1; if (a < 0) { fg = -1; return; } if (running >> a & 1) { fg = a; return; } } fg = -1; }
static void icon(int x, int y, const char *const *rows, uint8_t c, int sc) {
  for (int r = 0; r < 16; r++) { const char *s = rows[r]; if (!s) continue; for (int k = 0; k < 16 && s[k]; k++) if (s[k] == '#') fill(x + k * sc, y + r * sc, sc, sc, c); } }
static void clock_text(char *b, uint32_t ms, int cap) { uint32_t s = ms / 1000; b[0] = 0; s_two(b, s / 60 % 100, cap); s_cat(b, ":", cap); s_two(b, s % 60, cap); }

/* ---------------------------------------------------------------- the hands: buttons and stick as readings, turned into events by the roles chosen */
static uint8_t bt_was[4], bt_fired[4], bt_down[4]; static uint32_t bt_held[4], bt_rep[4], bt_count[4];
static int st_dir, st_x = -1, st_y = -1, st_cx = 2048, st_cy = 2048; static uint32_t st_held, st_rep;
static int last_bt = -1, last_how, last_role; static uint32_t last_at;
static int bt_test;                                                    /* the button test is open: presses are told on serial and act on nothing */
static int32_t ctl_trial_ms; static controls_t ctl_old;                /* new controls on trial: kept by a SELECT made with them, else put back */
static void os_input(int user) { if (user) { idle = 0; if (ghost_on) { ghost_on = 0; eq_in = eq_out = 0; } } }
static uint8_t role_event(int r) { static const uint8_t E[NROLE] = { IE_NONE, IE_OK, IE_BACK, IE_NEXT, IE_PREV, IE_SWITCH, IE_HOME }; return E[r % NROLE]; }
static void say_btn(int b, const char *what, int role) {
  if (!bt_test) return; char m[72]; s_cpy(m, "IRORI BUTTON ", 72); s_cat(m, BT_NAME[b], 72); s_cat(m, " (GPIO ", 72); s_cat_i(m, ST.ctl.pin[b], 72); s_cat(m, ") ", 72); s_cat(m, what, 72);
  if (role >= 0) { s_cat(m, " = ", 72); s_cat(m, ROLE_NAME[role % NROLE], 72); } s_cat(m, "\n", 72); hal_serial_write(m); }
static void role_fire(int b, int role, int how) {
  static const char *HOW[4] = { "TAP", "HOLD", "LONG HOLD", "REPEAT" };
  os_input(1); last_bt = b; last_how = how; last_role = role; last_at = latch; say_btn(b, HOW[how & 3], role);
  if (role != R_NONE && !bt_test) os_push(role_event(role), 0); }
static int repeats(int role) { return role == R_NEXT || role == R_PREV; }
static void hands_latch(void) {
  controls_t *c = &ST.ctl; uint8_t down[4] = { 0, 0, 0, 0 }, taps[4] = { 0, 0, 0, 0 }; hal_buttons(down, taps);
  for (int b = 0; b < 4; b++) {
    if (c->pin[b] == 255) { bt_was[b] = 0; continue; }
    int tap = c->tap[b], hold = c->hold[b], lng = c->lng[b], n = taps[b] > 3 ? 3 : taps[b];
    if (down[b] && !bt_was[b]) say_btn(b, "DOWN", -1); if (!down[b] && bt_was[b]) say_btn(b, "UP", -1);
    bt_count[b] += taps[b] ? taps[b] : (down[b] && !bt_was[b]); bt_down[b] = down[b];
    if (!hold && !lng) {                                               /* a plain button: acts on the press; NEXT and PREVIOUS repeat while held */
      if (!n && down[b] && !bt_was[b]) n = 1;
      for (int k = 0; k < n; k++) role_fire(b, tap, 0);
      if (down[b]) { if (bt_was[b]) bt_held[b] += dclock; else { bt_held[b] = 0; bt_rep[b] = 0; }
        if (repeats(tap) && bt_held[b] >= (uint32_t)c->rep_delay) { bt_rep[b] += dclock; for (int k = 0; k < 3 && bt_rep[b] >= (uint32_t)i_max(40, c->rep_ms); k++) { bt_rep[b] -= (uint32_t)i_max(40, c->rep_ms); role_fire(b, tap, 3); } } }
      else { bt_held[b] = 0; bt_rep[b] = 0; } }
    else {                                                             /* a button with a hold: a tap acts on release, a hold when it is reached */
      int starts = down[b] && (!bt_was[b] || taps[b]), whole = n - starts; if (whole < 0) whole = 0;   /* a press counted while it already looked down began again */
      for (int k = 0; k < whole; k++) role_fire(b, tap, 0);            /* whole taps that began and ended between two latches */
      if (down[b]) { if (starts) { bt_held[b] = 0; bt_fired[b] = 0; } else bt_held[b] += dclock;
        uint32_t h = (uint32_t)i_max(150, c->hold_ms);
        if (!lng && hold && !bt_fired[b] && bt_held[b] >= h) { role_fire(b, hold, 1); bt_fired[b] = 1; }
        if (lng && !bt_fired[b] && bt_held[b] >= h * 5 / 2) { role_fire(b, lng, 2); bt_fired[b] = 1; } }
      else { if (bt_was[b] && !bt_fired[b]) { uint32_t h = (uint32_t)i_max(150, c->hold_ms); if (lng && hold && bt_held[b] >= h) role_fire(b, hold, 1); else if (bt_held[b] < h || !hold) role_fire(b, tap, 0); }
        bt_held[b] = 0; } }
    bt_was[b] = down[b]; }
  int x, y; if (c->stick_on && hal_stick(&x, &y)) {                     /* the stick: a push moves once, then repeats while held */
    st_x = x; st_y = y; int dx = x - st_cx, dy = y - st_cy; if (c->swap_xy) { int t = dx; dx = dy; dy = t; } if (c->inv_x) dx = -dx; if (c->inv_y) dy = -dy;
    int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy, dir = 0;
    if (adx > c->dead || ady > c->dead) dir = adx >= ady ? (dx > 0 ? IE_RIGHT : IE_LEFT) : (dy > 0 ? IE_DOWN : IE_UP);
    if (dir && dir != st_dir) { os_input(1); if (!bt_test) os_push((uint8_t)dir, 0); st_held = 0; st_rep = 0; }
    else if (dir) { st_held += dclock; if (st_held >= (uint32_t)c->rep_delay) { st_rep += dclock; for (int k = 0; k < 3 && st_rep >= (uint32_t)i_max(40, c->rep_ms); k++) { st_rep -= (uint32_t)i_max(40, c->rep_ms); os_input(1); if (!bt_test) os_push((uint8_t)dir, 0); } } }
    st_dir = dir; } else { st_x = -1; st_y = -1; st_dir = 0; } }
static void hands_reset(void) { for (int b = 0; b < 4; b++) { bt_was[b] = 1; bt_fired[b] = 1; bt_held[b] = 0; bt_rep[b] = 0; } st_dir = 0; }   /* a button already down when the roles change acts on nothing until it is let go */
static void hint_build(char *h, int cap, int ctx) {                    /* what each button does here, from the roles chosen; ctx 0 home, 1 app, 2 keyboard */
  const controls_t *c = &ST.ctl; h[0] = 0;
  if (keys_mode()) { s_cpy(h, ctx == 2 ? "TYPE ON YOUR KEYBOARD   ESC: DONE" : "ARROWS  ENTER: SELECT  ESC: BACK  TAB: APP", cap); return; }
  for (int b = 0; b < 3; b++) if (c->pin[b] != 255 && c->tap[b]) { if (h[0]) s_cat(h, "  ", cap); s_cat(h, BT_NAME[b], cap); s_cat(h, " ", cap);
      s_cat(h, ctx == 2 && c->tap[b] == R_BACK ? "DONE" : ctx == 0 && c->tap[b] == R_SELECT ? "OPEN" : ROLE_NAME[c->tap[b]], cap); }
  for (int b = 0; b < 3; b++) if (c->pin[b] != 255 && c->hold[b]) { s_cat(h, "  HOLD ", cap); s_cat(h, BT_NAME[b], cap); s_cat(h, " ", cap); s_cat(h, ROLE_NAME[c->hold[b]], cap); }
  for (int b = 0; b < 3; b++) if (c->pin[b] != 255 && c->lng[b]) { s_cat(h, "  LONGER ", cap); s_cat(h, ROLE_NAME[c->lng[b]], cap); } }
static int hold_progress(char *h, int cap) {                           /* a hold under way: what it will do, and how far along (0..100) */
  const controls_t *c = &ST.ctl; uint32_t hm = (uint32_t)i_max(150, c->hold_ms);
  for (int b = 0; b < 4; b++) if (bt_down[b] && !bt_fired[b] && (c->hold[b] || c->lng[b]) && bt_held[b] >= 120) {
      int toward_long = c->lng[b] && (bt_held[b] >= hm || !c->hold[b]); uint32_t goal = toward_long ? hm * 5 / 2 : hm;
      s_cpy(h, "HOLD ", cap); s_cat(h, BT_NAME[b], cap); s_cat(h, ": ", cap); s_cat(h, ROLE_NAME[toward_long ? c->lng[b] : c->hold[b]], cap);
      return (int)i_min(100, bt_held[b] * 100 / goal); }
  return -1; }
static void top_bar(void) {
  fill(0, 0, SW, 14, K_BAR); int x = text(5, 4, "IRORI", K_ACCENT, 1); char b[48];
  if (safe_mode) { fill(x + 4, 2, 30, 10, K_WARN); x = text(x + 6, 4, "SAFE", K_BAR, 1); }
  b[0] = 0; s_cat(b, fg < 0 ? "HOME" : APPS[fg].name, 48); x = text(x + 10, 4, b, K_TEXT, 1);
  int n = 0; for (int a = 0; a < NAPP; a++) n += running >> a & 1; b[0] = 0; s_cat_i(b, n, 48); s_cat(b, " RUNNING", 48); text(170, 4, b, K_DIM, 1);
  clock_text(b, clock_total, 48); text(SW - 5 - text_w(b, 1), 4, b, K_TEXT, 1); }
static void bottom_bar(const char *hint) {                             /* the taskbar: running apps, and what the buttons do here */
  fill(0, SH - 26, SW, 26, K_BAR); int x = 4, more = 0;
  for (int a = 0; a < NAPP; a++) if (running >> a & 1) { int w = text_w(APPS[a].name, 1) + 8; if (x + w > SW - 40) { more++; continue; }
      fill(x, SH - 24, w, 11, a == fg ? K_TITLE : K_KEY); text(x + 4, SH - 22, APPS[a].name, a == fg ? K_WHITE : K_TEXT, 1); x += w + 3; }
  if (more) { char m[12]; s_cpy(m, "+", 12); s_cat_i(m, more, 12); s_cat(m, " MORE", 12); text(x + 2, SH - 22, m, K_DIM, 1); }
  if (x == 4 && !more) text(6, SH - 22, "NO APPS RUNNING", K_DIM, 1);
  char h[56]; int pr = hold_progress(h, 56);
  if (pr >= 0) { int e = text(6, SH - 10, h, K_FOCUS, 1); int w = SW - 12 - e - 8; if (w > 20) { fill(e + 8, SH - 10, w, 7, K_KEY); fill(e + 8, SH - 10, w * pr / 100, 7, K_FOCUS); } }
  else text_fit(6, SH - 10, hint, K_DIM, 1, SW - 12); }

/* immediate-mode form: an app declares its rows every frame; the focused row takes the event */
#define ROWH (ST.big_text ? 24 : 15)
#define RS (ST.big_text ? 2 : 1)
#define RTY (ST.big_text ? 4 : 3)
static int f_n, f_focus[NAPP + 1], f_scroll[NAPP + 1], f_y0, f_y1, f_slot; static uint64_t f_can;   /* f_can: which rows take the focus (an info row does not) */
static void form_begin(int y0, int y1) { f_n = 0; f_y0 = y0; f_y1 = y1; f_slot = fg + 1; f_can = 0; }
static int f_vis(void) { return (f_y1 - f_y0) / ROWH; }
static int row_at(int *y, int *focused, int can) {                     /* index of the next row; 1 if it is on screen */
  int i = f_n++; if (can && i < 64) f_can |= (uint64_t)1 << i; *focused = can && i == f_focus[f_slot]; *y = f_y0 + (i - f_scroll[f_slot]) * ROWH;
  int on = i >= f_scroll[f_slot] && i < f_scroll[f_slot] + f_vis();
  if (on) { fill(4, *y, SW - 8, ROWH - 2, *focused ? K_TITLE : K_PANEL); if (*focused) { box(4, *y, SW - 8, ROWH - 2, K_FOCUS); box(5, *y + 1, SW - 10, ROWH - 4, K_FOCUS); } }
  return on; }
static void row_label(int y, const char *label, uint8_t c, int f, int room) { text_fit(10, y + RTY, label, f ? (uint8_t)K_WHITE : c, RS, room - 10); }
static int row_button(const char *label) {
  int y, f; int on = row_at(&y, &f, 1); if (on) { row_label(y, label, K_TEXT, f, SW - 30); text(SW - 12 - 6 * RS, y + RTY, ">", f ? K_FOCUS : K_DIM, RS); }
  if (f && ev.type == IE_OK) { ev.type = IE_NONE; return 1; } return 0; }
static void row_info(const char *label, const char *value, uint8_t vc) {
  int y, f; int on = row_at(&y, &f, 0);
  if (on) { int w = text_w(value, RS); if (w > (SW - 24) * 3 / 5) w = (SW - 24) * 3 / 5; text_fit(SW - 10 - w, y + RTY, value, vc, RS, w); row_label(y, label, K_DIM, 0, SW - 18 - w); } }
static int row_toggle(const char *label, uint8_t *v) {
  int y, f; int on = row_at(&y, &f, 1); int ch = 0;
  if (f && (ev.type == IE_OK || ev.type == IE_LEFT || ev.type == IE_RIGHT)) { *v = !*v; ev.type = IE_NONE; ch = 1; }
  if (on) { int bw = 22 * RS + 14; row_label(y, label, K_TEXT, f, SW - 16 - bw); fill(SW - 10 - bw, y + 2, bw, ROWH - 6, *v ? K_GOOD : K_LINE); text_c(SW - 10 - bw / 2, y + RTY, *v ? "ON" : "OFF", *v ? K_BAR : K_TEXT, RS); }
  return ch; }
static void row_value_right(int y, int f, const char *b, int *w_out) {
  int w = text_w(b, RS); text(SW - 16 - 6 * RS - w, y + RTY, b, K_FOCUS, RS);
  if (f) { text(SW - 28 - 12 * RS - w, y + RTY, "<", K_DIM, RS); text(SW - 12 - 6 * RS, y + RTY, ">", K_DIM, RS); } *w_out = w + 24 + 18 * RS; }
static int row_number(const char *label, int16_t *v, int lo, int hi, int step, const char *unit) {
  int y, f; int on = row_at(&y, &f, 1); int ch = 0;
  if (f && ev.type == IE_LEFT) { *v = (int16_t)i_clamp(*v - step, lo, hi); ev.type = IE_NONE; ch = 1; }
  if (f && ev.type == IE_RIGHT) { *v = (int16_t)i_clamp(*v + step, lo, hi); ev.type = IE_NONE; ch = 1; }
  if (f && ev.type == IE_OK) { *v = (int16_t)(*v + step > hi ? lo : *v + step); ev.type = IE_NONE; ch = 1; }
  if (on) { char b[24]; b[0] = 0; s_cat_i(b, *v, 24); s_cat(b, unit, 24); int w; row_value_right(y, f, b, &w); row_label(y, label, K_TEXT, f, SW - w); }
  return ch; }
static int row_choice(const char *label, uint8_t *v, const char *const *names, int n) {
  int y, f; int on = row_at(&y, &f, 1); int ch = 0;
  if (f && (ev.type == IE_RIGHT || ev.type == IE_OK)) { *v = (uint8_t)((*v + 1) % n); ev.type = IE_NONE; ch = 1; }
  if (f && ev.type == IE_LEFT) { *v = (uint8_t)((*v + n - 1) % n); ev.type = IE_NONE; ch = 1; }
  if (on) { int w; row_value_right(y, f, names[*v % n], &w); row_label(y, label, K_TEXT, f, SW - w); }
  return ch; }
static void kb_open(char *buf, int cap, const char *title) { kb_on = 1; kb_buf = buf; kb_cap = cap; s_cpy(kb_title, title, 40); kb_row = 1; kb_col = 0; }   /* the title is copied: a caller's words may not outlive the call */
static int row_text(const char *label, char *buf, int cap) {
  int y, f; int on = row_at(&y, &f, 1);
  if (f && ev.type == IE_OK) { ev.type = IE_NONE; kb_open(buf, cap, label); }
  if (on) { int w = text_w(buf, RS); if (w > (SW - 24) / 2) w = (SW - 24) / 2; text_fit(SW - 10 - w, y + RTY, buf, f ? K_WHITE : K_TEXT, RS, w); row_label(y, label, K_DIM, f, SW - 18 - w); }
  return 0; }
static void form_end(void) {                                           /* the focus moves among rows that take it; info rows are passed over and scrolled into view */
  int n = f_n < 64 ? f_n : 64; if (!n || !f_can) return; int *fo = &f_focus[f_slot], *sc = &f_scroll[f_slot];
  int first = 0, last = n - 1; while (!(f_can >> first & 1)) first++; while (!(f_can >> last & 1)) last--;
  int step = 0; if (ev.type == IE_UP) step = -1; if (ev.type == IE_DOWN) step = 1;
  if (step) { ev.type = IE_NONE; int k = *fo; for (int t = 0; t < n; t++) { k = (k + step + n) % n; if (f_can >> k & 1) break; } *fo = k; }
  if (*fo >= n || *fo < 0) *fo = first; if (!(f_can >> *fo & 1)) { int k = *fo; while (k < n && !(f_can >> k & 1)) k++; *fo = k < n ? k : first; }
  int vis = f_vis(); if (vis < 1) vis = 1;
  if (*fo == first) *sc = 0; else if (*fo == last) *sc = n - vis; else { if (*fo < *sc) *sc = *fo; if (*fo >= *sc + vis) *sc = *fo - vis + 1; }
  if (*sc > n - vis) *sc = n - vis; if (*sc < 0) *sc = 0;
  if (n > vis) { int h = f_y1 - f_y0, bh = i_max(8, h * vis / n), by = f_y0 + (h - bh) * *sc / i_max(1, n - vis); fill(SW - 3, f_y0, 2, h, K_PANEL); fill(SW - 3, by, 2, bh, K_ACCENT); } }
static void back_word(char *b, int cap) {                              /* the name of whatever goes back, from the roles chosen */
  const controls_t *c = &ST.ctl; b[0] = 0; if (keys_mode()) { s_cpy(b, "ESC: BACK", cap); return; }
  for (int k = 0; k < 4; k++) if (c->pin[k] != 255 && c->tap[k] == R_BACK) { s_cpy(b, BT_NAME[k], cap); s_cat(b, ": BACK", cap); return; }
  for (int k = 0; k < 4; k++) if (c->pin[k] != 255 && (c->hold[k] == R_BACK || c->lng[k] == R_BACK)) { s_cpy(b, "HOLD ", cap); s_cat(b, BT_NAME[k], cap); s_cat(b, ": BACK", cap); return; } }
static void window_frame(const char *title, const char *note) {        /* the app's window: a title bar with its name, and what it is doing */
  fill(0, 14, SW, SH - 40, K_BG); fill(0, 14, SW, 14, K_TITLE); int x = text(6, 17, title, K_WHITE, 1); char bw[24]; back_word(bw, 24);
  if (note) text_fit(x + 10, 17, note, K_FOCUS, 1, SW - 16 - text_w(bw, 1) - x - 10); text(SW - 6 - text_w(bw, 1), 17, bw, K_WHITE, 1); }

/* ---------------------------------------------------------------- readings: a value and its change, and rate(A, B) */
static int16_t rd_last[8], rd_now[8];
static int read_pin(int pin) { int i = pin_index(pin); if (i < 0) return 0; return (ST.pin_mode[i] == 3 && is_analog(pin)) ? hal_adc(pin) : hal_pin_read(pin); }
static int read_any(int pin) { return is_analog(pin) ? hal_adc(pin) : hal_pin_read(pin); }   /* reading only: safe on a pin another part owns */
static exact_t rate_of(int da, int db) { return ex_make(da, db); }        /* theta when B did not change */

/* ================================================================ the applications */
/* ---- 1. PINS: every free GPIO, its mode and its reading; outputs switch with A */
static const char *PMODE[4] = { "INPUT", "OUTPUT", "PWM", "ANALOG" };
static uint8_t pin_out[8]; static int16_t pin_pwm[8] = { 500, 500, 500, 500, 500, 500, 500, 500 };
static void app_pins(void) {
  window_frame("PINS", "EVERY FREE GPIO"); form_begin(32, SH - 30);
  for (int i = 0; i < 8; i++) { int pin = UPIN[i]; char lab[32]; lab[0] = 0; s_cat(lab, "GPIO ", 32); s_cat_i(lab, pin, 32);
    const char *own = pin_user(pin, U_PINS);
    if (own) { char b[40]; s_cpy(b, own, 40); s_cat(b, "  READS ", 40); s_cat_i(b, read_any(pin), 40); s_cat(lab, " (IN USE)", 32); row_info(lab, b, K_DIM); continue; }
    s_cat(lab, " MODE", 32); uint8_t m = ST.pin_mode[i]; int nm = is_analog(pin) ? 4 : 3; if (m >= nm) m = 0;
    if (row_choice(lab, &m, PMODE, nm)) { ST.pin_mode[i] = m; pin_apply(i); }
    char v[32]; v[0] = 0; char l2[28]; s_cpy(l2, "   READING", 28);
    if (m == 1) { s_cpy(l2, "   OUTPUT, SELECT SWITCHES", 28); if (row_toggle(l2, &pin_out[i])) hal_pin_write(pin, pin_out[i]); }
    else if (m == 2) { if (row_number("   DUTY", &pin_pwm[i], 0, 1000, 50, "/1000")) hal_pwm(pin, pin_pwm[i]); }
    else { int r = read_pin(pin); s_cat_i(v, r, 32); if (m == 3) { s_cat(v, "  CHANGE ", 32); s_cat_i(v, rd_now[i] - rd_last[i], 32); } else s_cpy(v, r ? "HIGH" : "LOW", 32);
      row_info(l2, v, m == 3 ? K_PLOT : (r ? K_GOOD : K_DIM)); } }
  if (row_button("SAVE PIN SETUP")) os_save();
  if (row_button("CLOSE PINS")) stop_app(0);
  form_end(); }
static void tick_pins(void) { for (int i = 0; i < 8; i++) { rd_last[i] = rd_now[i]; rd_now[i] = (int16_t)read_pin(UPIN[i]); } }

/* ---- 2. PLOTTER: a reading drawn as it goes, a second one against it, and rate(A, B) */
#define PLOTN 288
static int16_t plot_a[PLOTN], plot_b[PLOTN]; static int plot_n, plot_count; static uint8_t plot_run = 1;
static const char *PEV[4] = { "EVERY LATCH", "EVERY 4", "EVERY 16", "EVERY 64" };
static const char *PCH[9] = { "GPIO 0", "GPIO 1", "GPIO 2", "GPIO 3", "GPIO 8", "GPIO 9", "GPIO 20", "GPIO 21", "NONE" };
static void tick_plot(void) {
  if (!plot_run) return; static int skip; static const int E[4] = { 1, 4, 16, 64 }; if (++skip < E[ST.plot_every & 3]) return; skip = 0;
  int pa = ST.plot_a < 8 ? read_any(UPIN[ST.plot_a]) : 0, pb = ST.plot_b < 8 ? read_any(UPIN[ST.plot_b]) : 0;
  plot_a[plot_n % PLOTN] = (int16_t)pa; plot_b[plot_n % PLOTN] = (int16_t)pb; plot_n++; if (plot_count < PLOTN) plot_count++; }
static void app_plot(void) {
  window_frame("PLOTTER", plot_run ? "RECORDING" : "PAUSED");
  int gx = 16, gy = 34, gw = PLOTN, gh = 92; fill(gx - 2, gy - 2, gw + 4, gh + 4, K_SHADOW); box(gx - 2, gy - 2, gw + 4, gh + 4, K_LINE);
  int lo = 32767, hi = -32768; for (int i = 0; i < plot_count; i++) { int k = (plot_n - plot_count + i) % PLOTN; lo = i_min(lo, plot_a[k]); hi = i_max(hi, plot_a[k]); if (ST.plot_b < 8) { lo = i_min(lo, plot_b[k]); hi = i_max(hi, plot_b[k]); } }
  if (plot_count && hi == lo) { hi++; lo--; }
  for (int g = 1; g < 4; g++) hline(gx, gx + gw - 1, gy + gh * g / 4, K_PANEL);
  for (int ch = 1; ch >= 0; ch--) { if (ch == 1 && ST.plot_b >= 8) continue; int py = -1; int16_t *src = ch ? plot_b : plot_a;
    for (int i = 0; i < plot_count; i++) { int k = (plot_n - plot_count + i) % PLOTN; int y = gy + gh - 1 - (int)((int32_t)(src[k] - lo) * (gh - 1) / (hi - lo));
      if (py >= 0) vline(gx + i, py, y, ch ? K_PLOT2 : K_PLOT); else px(gx + i, y, ch ? K_PLOT2 : K_PLOT); py = y; } }
  char b[64]; b[0] = 0;
  if (plot_count) { int k = (plot_n - 1) % PLOTN, k0 = (plot_n - 2 + PLOTN) % PLOTN; s_cpy(b, "A NOW ", 64); s_cat_i(b, plot_a[k], 64); s_cat(b, "  LOW ", 64); s_cat_i(b, lo, 64); s_cat(b, "  HIGH ", 64); s_cat_i(b, hi, 64);
    text(gx, gy + gh + 6, b, K_PLOT, 1);
    if (ST.plot_b < 8 && plot_count > 1) { exact_t r = rate_of(plot_a[k] - plot_a[k0], plot_b[k] - plot_b[k0]); s_cpy(b, "RATE(A, B) ", 64); char e[48]; ex_text(e, r, 48); s_cat(b, e, 64); text(gx, gy + gh + 16, b, r.bad ? K_WARN : K_PLOT2, 1); } }
  else text(gx, gy + gh + 6, "WAITING FOR THE FIRST READING", K_DIM, 1);
  form_begin(gy + gh + 28, SH - 30);
  uint8_t a = ST.plot_a, bb = ST.plot_b;
  if (row_choice("A (BLUE)", &a, PCH, 8)) { ST.plot_a = a; plot_count = 0; } if (row_choice("B (PINK)", &bb, PCH, 9)) { ST.plot_b = bb; plot_count = 0; }
  uint8_t e2 = ST.plot_every; if (row_choice("SAMPLE", &e2, PEV, 4)) ST.plot_every = e2;
  row_toggle("RECORDING", &plot_run);
  if (row_button("CLEAR")) plot_count = 0;
  if (row_button("CLOSE PLOTTER")) stop_app(1);
  form_end(); }

/* ---- 3. SERVO & PWM: a servo by angle, a dimmer by duty, or a blink; sweep runs in the background */
static const char *SMODE[3] = { "SERVO", "PWM DIMMER", "BLINK" };
static uint8_t sweep_on, blink_state; static int sweep_dir = 1; static int16_t blink_half = 30, servo_angle = 90; static int blink_n;
static void servo_out(void) { int pin = ST.servo_pin; if (pin_user(pin, U_SERVO)) return;
  if (ST.servo_mode == 0) hal_servo(pin, servo_angle); else if (ST.servo_mode == 1) hal_pwm(pin, ST.servo_value * 10); }
static void tick_servo(void) {
  if (ST.servo_mode == 0 && sweep_on) { servo_angle = (int16_t)(servo_angle + sweep_dir * 2); if (servo_angle >= 180) { servo_angle = 180; sweep_dir = -1; } if (servo_angle <= 0) { servo_angle = 0; sweep_dir = 1; } servo_out(); }
  if (ST.servo_mode == 2 && !pin_user(ST.servo_pin, U_SERVO)) { if (++blink_n >= blink_half) { blink_n = 0; blink_state = !blink_state; hal_pin_mode(ST.servo_pin, 1); hal_pin_write(ST.servo_pin, blink_state); } } }
static void app_servo(void) {
  window_frame("SERVO & PWM", sweep_on ? "SWEEPING" : 0); form_begin(32, SH - 30);
  uint8_t pi = (uint8_t)i_max(0, pin_index(ST.servo_pin)); if (row_choice("OUTPUT PIN", &pi, PCH, 8)) { ST.servo_pin = UPIN[pi]; servo_out(); }
  if (pin_user(ST.servo_pin, U_SERVO)) { char b[32]; s_cpy(b, "USED BY ", 32); s_cat(b, pin_user(ST.servo_pin, U_SERVO), 32); row_info("   THAT PIN IS", b, K_WARN); }
  uint8_t m = ST.servo_mode; if (row_choice("MODE", &m, SMODE, 3)) { ST.servo_mode = m; servo_out(); }
  if (ST.servo_mode == 0) { if (row_number("ANGLE", &servo_angle, 0, 180, 5, " DEG")) servo_out(); row_toggle("SWEEP 0-180", &sweep_on); }
  if (ST.servo_mode == 1) { if (row_number("BRIGHTNESS", &ST.servo_value, 0, 100, 5, " %")) servo_out(); }
  if (ST.servo_mode == 2) { row_number("HALF PERIOD", &blink_half, 1, 200, 1, " LATCHES"); row_info("   LIGHT IS", blink_state ? "ON" : "OFF", blink_state ? K_GOOD : K_DIM); }
  { int y = SH - 64; fill(16, y, SW - 32, 10, K_SHADOW); int w = ST.servo_mode == 0 ? (SW - 32) * servo_angle / 180 : ST.servo_mode == 1 ? (SW - 32) * ST.servo_value / 100 : (blink_state ? SW - 32 : 0); fill(16, y, w, 10, K_ACCENT); }
  if (row_button("SAVE")) os_save();
  if (row_button("CLOSE SERVO & PWM")) { sweep_on = 0; stop_app(2); }
  form_end(); }

/* ---- 4. RULES: WHEN something THEN something, kept in flash, running in the background */
static const char *RWHEN[5] = { "PIN IS HIGH", "PIN IS LOW", "ANALOG ABOVE", "ANALOG BELOW", "EVERY N SECONDS" };
static const char *RTHEN[6] = { "PIN ON", "PIN OFF", "TOGGLE PIN", "PWM DUTY %", "SERVO ANGLE", "SEND A LINE" };
static int rule_edit = -1; static uint32_t rule_fired[8], rule_clock[8]; static uint8_t rule_last[8];   /* rule_last: a reading of the condition, kept out of flash (it changes every latch) */
static void rule_text(char *b, const rule_t *r, int cap) {
  b[0] = 0; if (r->when <= 1) { s_cat(b, "GPIO ", cap); s_cat_i(b, r->in_pin, cap); s_cat(b, r->when ? " LOW" : " HIGH", cap); }
  else if (r->when <= 3) { s_cat(b, "A", cap); s_cat_i(b, r->in_pin, cap); s_cat(b, r->when == 2 ? " > " : " < ", cap); s_cat_i(b, r->value, cap); }
  else { s_cat(b, "EVERY ", cap); s_cat_i(b, r->value, cap); s_cat(b, "S", cap); }
  s_cat(b, " -> ", cap); static const char *T[6] = { "ON ", "OFF ", "FLIP ", "PWM ", "SERVO ", "SEND" }; s_cat(b, T[r->then % 6], cap);
  if (r->then < 5) { s_cat(b, "G", cap); s_cat_i(b, r->out_pin, cap); } if (r->then == 3 || r->then == 4) { s_cat(b, "=", cap); s_cat_i(b, r->out_value, cap); } }
static void tick_rules(void) {
  for (int k = 0; k < 8; k++) { rule_t *r = &ST.rules[k]; if (!r->on) continue; int now = 0;
    if (r->when <= 1) { int v = hal_pin_read(r->in_pin); now = r->when ? !v : v; }
    else if (r->when <= 3) { int v = is_analog(r->in_pin) ? hal_adc(r->in_pin) : 0; now = r->when == 2 ? v > r->value : v < r->value; }
    else { rule_clock[k] += dclock; if (rule_clock[k] >= (uint32_t)i_max(1, r->value) * 1000u) { rule_clock[k] = 0; now = 1; rule_last[k] = 0; } }
    if (now && !rule_last[k]) { int p = r->out_pin; rule_fired[k]++;                                     /* fires when the condition turns true */
      if (r->then == 5) { char b[48]; s_cpy(b, "RULE ", 48); s_cat_i(b, k + 1, 48); s_cat(b, " FIRED\n", 48); hal_serial_write(b); }
      else if (!pin_user(p, U_RULES)) { if (r->then <= 2) { hal_pin_mode(p, 1); int i = pin_index(p); int v = r->then == 0 ? 1 : r->then == 1 ? 0 : (i >= 0 ? !pin_out[i] : 1); if (i >= 0) pin_out[i] = (uint8_t)v; hal_pin_write(p, v); }
        else if (r->then == 3) hal_pwm(p, r->out_value * 10); else hal_servo(p, r->out_value); } }
    rule_last[k] = (uint8_t)now; } }
static void app_rules(void) {
  if (rule_edit >= 0) { rule_t *r = &ST.rules[rule_edit]; char t[24]; s_cpy(t, "RULE ", 24); s_cat_i(t, rule_edit + 1, 24);
    window_frame(t, r->on ? "ON" : "OFF"); form_begin(32, SH - 30);
    row_toggle("RULE IS ON", &r->on); row_choice("WHEN", &r->when, RWHEN, 5);
    if (r->when <= 3) { uint8_t pi = (uint8_t)i_max(0, pin_index(r->in_pin)); if (row_choice("   INPUT PIN", &pi, PCH, 8)) r->in_pin = UPIN[pi]; }
    if (r->when >= 2) row_number(r->when == 4 ? "   EVERY" : "   VALUE (0-4095)", &r->value, r->when == 4 ? 1 : 0, r->when == 4 ? 3600 : 4095, r->when == 4 ? 1 : 50, r->when == 4 ? " S" : "");
    row_choice("THEN", &r->then, RTHEN, 6);
    if (r->then < 5) { uint8_t po = (uint8_t)i_max(0, pin_index(r->out_pin)); if (row_choice("   OUTPUT PIN", &po, PCH, 8)) r->out_pin = UPIN[po]; }
    if (r->then == 3 || r->then == 4) row_number("   TO", &r->out_value, 0, r->then == 3 ? 100 : 180, 5, r->then == 3 ? " %" : " DEG");
    char b[64]; rule_text(b, r, 64); row_info("READS AS", b, K_ACCENT);
    if (r->when <= 3 && pin_user(r->in_pin, U_RULES)) row_info("   INPUT PIN ALSO USED BY", pin_user(r->in_pin, U_RULES), K_DIM);
    if (r->then < 5 && pin_user(r->out_pin, U_RULES)) row_info("   OUTPUT PIN TAKEN BY", pin_user(r->out_pin, U_RULES), K_WARN);
    if (row_button("SAVE RULES")) os_save();
    if (row_button("BACK TO THE LIST")) rule_edit = -1;
    if (ev.type == IE_BACK) { rule_edit = -1; ev.type = IE_NONE; }
    form_end(); return; }
  int on = 0; for (int k = 0; k < 8; k++) on += ST.rules[k].on; char note[24]; note[0] = 0; s_cat_i(note, on, 24); s_cat(note, " ON, IN THE BACKGROUND", 24);
  window_frame("RULES", note); form_begin(32, SH - 30);
  for (int k = 0; k < 8; k++) { char lab[72]; lab[0] = 0; s_cat_i(lab, k + 1, 72); s_cat(lab, ST.rules[k].on ? " ON   " : " off  ", 72); char b[56]; rule_text(b, &ST.rules[k], 56); s_cat(lab, b, 72);
    if (rule_fired[k]) { s_cat(lab, "  x", 72); s_cat_i(lab, rule_fired[k], 72); } if (row_button(lab)) rule_edit = k; }
  if (row_button("SAVE RULES")) os_save();
  if (row_button("CLOSE RULES (THEY STOP)")) stop_app(3);
  form_end(); }

/* ---- 5. TERMINAL: the USB serial line, read and written */
#define TL 11
static char term[TL][48]; static int term_n, term_col;
static void term_put(int c) { if (c == '\r') return; if (c == '\n' || term_col >= 46) { term_n++; term_col = 0; term[term_n % TL][0] = 0; if (c == '\n') return; }
  char *l = term[term_n % TL]; l[term_col++] = (char)c; l[term_col] = 0; }
static void app_terminal(void) {
  window_frame("TERMINAL", "USB SERIAL, 115200"); fill(6, 32, SW - 12, TL * 10 + 6, K_SHADOW);
  for (int i = 0; i < TL; i++) { int k = term_n - TL + 1 + i; if (k < 0) continue; text(10, 35 + i * 10, term[k % TL], i == TL - 1 ? K_GOOD : K_TEXT, 1); }
  form_begin(32 + TL * 10 + 12, SH - 30);
  row_text("MESSAGE (A TO TYPE)", ST.sendbox, 40);
  if (row_button("SEND THE MESSAGE")) { hal_serial_write(ST.sendbox); hal_serial_write("\n"); term_put('>'); for (int i = 0; ST.sendbox[i]; i++) term_put(ST.sendbox[i]); term_put('\n'); }
  if (row_button("CLOSE TERMINAL")) stop_app(4);
  form_end(); }

/* ---- 6. I2C SCAN: who answers, and what they usually are */
static uint8_t i2c_found[16]; static int i2c_at = 128, i2c_nfound;
static const char *i2c_name(int a) {
  switch (a) { case 0x3C: case 0x3D: return "OLED DISPLAY (SSD1306/SH1106)"; case 0x76: case 0x77: return "BME280 / BMP280 (WEATHER)"; case 0x68: return "MPU6050 OR DS3231 CLOCK";
    case 0x69: return "MPU6050 (ALT)"; case 0x48: return "ADS1115 ADC / TMP102"; case 0x40: return "INA219 / HTU21D / PCA9685"; case 0x5F: return "M5 CARDKB KEYBOARD";
    case 0x23: return "BH1750 LIGHT"; case 0x29: return "VL53L0X DISTANCE"; case 0x57: return "EEPROM (DS3231 BOARD)"; case 0x27: return "LCD BACKPACK (PCF8574)";
    case 0x50: return "EEPROM 24C32"; case 0x1E: return "HMC5883 COMPASS"; case 0x70: return "TCA9548 MUX"; default: return "UNKNOWN DEVICE"; } }
static void tick_i2c(void) { for (int k = 0; k < 8 && i2c_at < 127; k++, i2c_at++) if (i2c_at >= 1 && hal_i2c_probe(i2c_at) && i2c_nfound < 16) i2c_found[i2c_nfound++] = (uint8_t)i2c_at; }
static void app_i2c(void) {
  char note[24]; if (!ST.i2c_on) s_cpy(note, "I2C IS OFF (SETTINGS)", 24); else if (i2c_at < 127) { s_cpy(note, "SCANNING ", 24); s_cat_i(note, i2c_at * 100 / 127, 24); s_cat(note, "%", 24); } else s_cpy(note, "SDA 8  SCL 9", 24);
  window_frame("I2C SCAN", note); form_begin(32, SH - 30);
  if (row_button(i2c_at < 127 ? "SCANNING..." : "SCAN THE BUS") && ST.i2c_on && i2c_at >= 127) { i2c_at = 1; i2c_nfound = 0; }
  if (i2c_at >= 127 && i2c_at < 128 && !i2c_nfound) row_info("FOUND", "NOTHING ANSWERED", K_WARN);
  for (int k = 0; k < i2c_nfound; k++) { char lab[16]; s_cpy(lab, "0X", 16); s_hex2(lab, i2c_found[k], 16); row_info(lab, i2c_name(i2c_found[k]), K_GOOD); }
  if (row_button("CLOSE I2C SCAN")) stop_app(5);
  form_end(); }

/* ---- 7. CALCULATOR: exact fractions; division by nothing is named, not guessed */
static const char *CKEYS[5][4] = { { "7", "8", "9", "/" }, { "4", "5", "6", "X" }, { "1", "2", "3", "-" }, { "0", "FR", "=", "+" }, { "C", "DEL", "+/-", "1/X" } };
static char calc_entry[24]; static exact_t calc_acc; static char calc_op; static int calc_r = 3, calc_c = 2, calc_fresh = 1; static char calc_show[64];
static exact_t calc_parse(void) { int64_t n = 0, d = 0; int neg = 0, frac = 0; for (int i = 0; calc_entry[i]; i++) { char c = calc_entry[i]; if (c == '-') neg = 1; else if (c == '/') frac = 1; else if (frac) d = d * 10 + (c - '0'); else n = n * 10 + (c - '0'); }
  if (!frac) d = 1; return ex_make(neg ? -n : n, d); }
static void calc_press(const char *k) {
  if (k[0] >= '0' && k[0] <= '9') { if (calc_fresh) { calc_entry[0] = 0; calc_fresh = 0; } if (s_len(calc_entry) < 16) { char b[2] = { k[0], 0 }; s_cat(calc_entry, b, 24); } }
  else if (k[0] == 'F') { if (calc_fresh) { s_cpy(calc_entry, "0", 24); calc_fresh = 0; } int has = 0; for (int i = 0; calc_entry[i]; i++) has |= calc_entry[i] == '/'; if (!has) s_cat(calc_entry, "/", 24); }
  else if (k[0] == 'C') { calc_entry[0] = 0; calc_acc = ex_make(0, 1); calc_op = 0; calc_show[0] = 0; calc_fresh = 1; }
  else if (k[0] == 'D') { int n = s_len(calc_entry); if (n) calc_entry[n - 1] = 0; }
  else if (k[0] == '+' && k[1] == '/') { if (calc_entry[0] == '-') s_cpy(calc_entry, calc_entry + 1, 24); else { char b[24] = "-"; s_cat(b, calc_entry, 24); s_cpy(calc_entry, b, 24); } }
  else if (k[0] == '1') { exact_t e = ex_op(ex_make(1, 1), calc_entry[0] ? calc_parse() : calc_acc, '/'); calc_acc = e; calc_op = 0; ex_text(calc_show, e, 64); calc_entry[0] = 0; calc_fresh = 1; }
  else { exact_t e = calc_entry[0] ? calc_parse() : calc_acc; if (calc_op && calc_entry[0]) e = ex_op(calc_acc, e, calc_op == 'X' ? 'x' : calc_op);
    calc_acc = e; calc_op = k[0] == '=' ? 0 : k[0]; ex_text(calc_show, e, 64); calc_entry[0] = 0; calc_fresh = 1; } }
static void app_calc(void) {
  window_frame("CALCULATOR", "EXACT FRACTIONS"); fill(8, 32, SW - 16, 40, K_SHADOW);
  char b[40]; s_cpy(b, calc_entry[0] ? calc_entry : "0", 40); if (calc_op) { char o[3] = { calc_op, ' ', 0 }; char t[40]; s_cpy(t, o, 40); s_cat(t, b, 40); s_cpy(b, t, 40); }
  text(SW - 14 - text_w(b, 2), 36, b, K_WHITE, 2); text(14, 58, calc_show[0] ? calc_show : "FR MAKES A FRACTION: 1 FR 3 IS 1/3", calc_show[0] == 'T' || calc_show[0] == 'W' ? K_WARN : K_ACCENT, 1);
  if (ev.type == IE_UP) { calc_r = (calc_r + 4) % 5; ev.type = IE_NONE; } if (ev.type == IE_DOWN) { calc_r = (calc_r + 1) % 5; ev.type = IE_NONE; }
  if (ev.type == IE_LEFT) { calc_c = (calc_c + 3) % 4; ev.type = IE_NONE; } if (ev.type == IE_RIGHT) { calc_c = (calc_c + 1) % 4; ev.type = IE_NONE; }
  if (ev.type == IE_NEXT || ev.type == IE_PREV) { int k = (calc_r * 4 + calc_c + (ev.type == IE_NEXT ? 1 : 19)) % 20; calc_r = k / 4; calc_c = k % 4; ev.type = IE_NONE; }   /* buttons only: every key in reading order */
  if (ev.type == IE_OK) { calc_press(CKEYS[calc_r][calc_c]); ev.type = IE_NONE; }
  if (ev.type == IE_CHAR) { char c = (char)ev.ch; const char *k = 0; static char one[2]; if (c >= '0' && c <= '9') { one[0] = c; one[1] = 0; k = one; }
    else if (c == '+') k = "+"; else if (c == '-') k = "-"; else if (c == '*' || c == 'x') k = "X"; else if (c == '/') k = "/"; else if (c == '=') k = "="; else if (c == 'f') k = "FR"; else if (c == 8) k = "DEL"; else if (c == 'c') k = "C";
    if (k) calc_press(k); ev.type = IE_NONE; }
  for (int r = 0; r < 5; r++) for (int c = 0; c < 4; c++) { int x = 14 + c * 75, y = 80 + r * 26, f = r == calc_r && c == calc_c;
      fill(x, y, 70, 22, f ? K_TITLE : (c == 3 ? K_LINE : K_KEY)); if (f) box(x, y, 70, 22, K_FOCUS); text_c(x + 35, y + 4, CKEYS[r][c], f ? K_WHITE : K_TEXT, 2); } }

/* ---- 8. NOTES: four notes, kept in flash */
static int note_open = -1;
static void app_notes(void) {
  if (note_open >= 0) { char t[16]; s_cpy(t, "NOTE ", 16); s_cat_i(t, note_open + 1, 16); window_frame(t, "SELECT: EDIT"); char *s = ST.notes[note_open];
    int x = 10, y = 34; for (int i = 0; s[i]; i++) { if (s[i] == '\n' || x > SW - 16) { x = 10; y += 11; if (s[i] == '\n') continue; } char c[2] = { s[i], 0 }; text(x, y, c, K_TEXT, 1); x += 6; if (y > SH - 50) break; }
    if (ev.type == IE_OK) { ev.type = IE_NONE; kb_open(s, 160, t); } if (ev.type == IE_BACK) { ev.type = IE_NONE; note_open = -1; os_save(); } return; }
  window_frame("NOTES", "KEPT IN FLASH"); form_begin(32, SH - 30);
  for (int k = 0; k < 4; k++) { char lab[40]; lab[0] = 0; s_cat_i(lab, k + 1, 40); s_cat(lab, "  ", 40); char p[28]; s_cpy(p, ST.notes[k][0] ? ST.notes[k] : "(EMPTY)", 28); s_cat(lab, p, 40); if (row_button(lab)) note_open = k; }
  if (row_button("CLOSE NOTES")) stop_app(7);
  form_end(); }

/* ---- 9. TIMER: a stopwatch and a countdown, read from the clock's own change */
static uint8_t sw_run, cd_run; static uint32_t sw_ms, cd_left; static int16_t cd_set = 60;
static void tick_timer(void) { if (sw_run) sw_ms += dclock;
  if (cd_run) { if (cd_left > dclock) cd_left -= dclock; else { cd_left = 0; cd_run = 0; hal_serial_write("TIMER DONE\n"); int p = UPIN[cd_pin_i & 7]; if (cd_alarm_pin_on && !pin_user(p, U_TIMER)) { hal_pin_mode(p, 1); hal_pin_write(p, 1); } } } }
static void app_timer(void) {
  window_frame("TIMER", sw_run || cd_run ? "COUNTING IN THE BACKGROUND" : 0);
  char b[24]; clock_text(b, sw_ms, 24); s_cat(b, ".", 24); s_two(b, sw_ms / 10 % 100, 24); text(16, 36, "STOPWATCH", K_DIM, 1); text(16, 48, b, K_WHITE, 3);
  clock_text(b, cd_left, 24); text(176, 36, "COUNTDOWN", K_DIM, 1); text(176, 48, b, cd_run ? K_FOCUS : K_WHITE, 3);
  form_begin(80, SH - 30);
  if (row_button(sw_run ? "STOPWATCH: STOP" : "STOPWATCH: START")) sw_run = !sw_run;
  if (row_button("STOPWATCH: RESET")) sw_ms = 0;
  row_number("COUNTDOWN FROM", &cd_set, 1, 5999, 5, " S");
  if (row_button(cd_run ? "COUNTDOWN: STOP" : "COUNTDOWN: START")) { if (!cd_run && !cd_left) cd_left = (uint32_t)cd_set * 1000u; cd_run = !cd_run; }
  if (row_button("COUNTDOWN: RESET")) { cd_run = 0; cd_left = 0; }
  row_toggle("WHEN DONE, TURN A PIN ON", &cd_alarm_pin_on);
  { uint8_t pi = (uint8_t)cd_pin_i; if (row_choice("   THAT PIN", &pi, PCH, 8)) cd_pin_i = pi; }
  if (row_button("CLOSE TIMER")) stop_app(8);
  form_end(); }

/* ---- 10. PROFILES: which apps start together, and which rules are on: one flash, many projects */
static int prof_edit = -1;
static void profile_start(int k) {
  running = 0; fg = -1; profile_t *p = &ST.prof[(k < 0 ? 0 : k) % NPROF];
  for (int r = 0; r < 8; r++) { ST.rules[r].on = (uint8_t)(p->rules >> r & 1); rule_last[r] = 0; rule_clock[r] = 0; }
  for (int a = 0; a < NAPP; a++) if (p->apps >> a & 1) running |= (uint16_t)(1u << a);
  for (int i = 0; i < 8; i++) pin_apply(i); }
static void app_profiles(void) {
  if (prof_edit >= 0) { profile_t *p = &ST.prof[prof_edit]; window_frame(p->name, "WHAT STARTS TOGETHER"); form_begin(32, SH - 30);
    row_text("NAME", p->name, 12);
    for (int a = 0; a < NAPP; a++) { uint8_t v = (uint8_t)(p->apps >> a & 1); char lab[32]; s_cpy(lab, "START ", 32); s_cat(lab, APPS[a].name, 32); if (row_toggle(lab, &v)) p->apps = (uint16_t)(v ? p->apps | (1u << a) : p->apps & ~(1u << a)); }
    for (int r = 0; r < 8; r++) { uint8_t v = (uint8_t)(p->rules >> r & 1); char lab[48]; s_cpy(lab, "RULE ", 48); s_cat_i(lab, r + 1, 48); s_cat(lab, " ON:  ", 48); char b[40]; rule_text(b, &ST.rules[r], 40); s_cat(lab, b, 48);
      if (row_toggle(lab, &v)) p->rules = (uint8_t)(v ? p->rules | (1u << r) : p->rules & ~(1u << r)); }
    if (row_button("START THIS PROFILE NOW")) { profile_start(prof_edit); prof_edit = -1; open_app(9); }
    if (row_button("BACK TO PROFILES")) prof_edit = -1;
    if (ev.type == IE_BACK) { prof_edit = -1; ev.type = IE_NONE; }
    form_end(); return; }
  window_frame("PROFILES", "ONE FLASH, MANY PROJECTS"); form_begin(32, SH - 30);
  for (int k = 0; k < NPROF; k++) { char lab[40]; s_cpy(lab, ST.prof[k].name, 40); if (k == ST.boot_profile) s_cat(lab, "  (AT POWER-ON)", 40); if (row_button(lab)) prof_edit = k; }
  { static const char *PN[NPROF] = { "1", "2", "3", "4", "5", "6" }; uint8_t b = ST.boot_profile; if (row_choice("START AT POWER-ON: PROFILE", &b, PN, NPROF)) ST.boot_profile = b; }
  row_info("CHANGES", ST.autosave ? "SAVE THEMSELVES" : "SAVED WITH SAVE NOW", K_DIM);
  if (row_button("SAVE NOW")) { os_save(); toast("SAVED"); }
  if (row_button("CLOSE PROFILES")) stop_app(9);
  form_end(); }

/* ---- 11. TASKS: every app, started or stopped from here; what each did; how the machine is */
static uint32_t frame_us_avg, frame_us_worst;                          /* told by the board (0 on the desk) */
static void app_tasks(void) {
  int n = 0; for (int a = 0; a < NAPP; a++) n += running >> a & 1; char note[24]; note[0] = 0; s_cat_i(note, n, 24); s_cat(note, " OF 12 RUNNING", 24);
  window_frame("TASKS", note); form_begin(32, SH - 30);
  for (int a = 0; a < NAPP; a++) { uint8_t v = (uint8_t)(running >> a & 1); char lab[48]; s_cpy(lab, APPS[a].name, 48);
    if (v && APPS[a].tick) { s_cat(lab, "  WORK ", 48); s_cat_i(lab, work_now[a], 48); } else if (v) s_cat(lab, "  (WHEN IN FRONT)", 48);
    if (row_toggle(lab, &v)) { if (v) running |= (uint16_t)(1u << a); else stop_app(a); } }
  if (row_button("START EVERY APP")) { running = 0x0FFF; for (int i = 0; i < 8; i++) pin_apply(i); }
  if (row_button("STOP EVERY OTHER APP")) { for (int a = 0; a < NAPP; a++) if (a != 10 && (running >> a & 1)) stop_app(a); running = (uint16_t)(1u << 10); fg = 10; }
  char b[40];
  if (frame_us_avg) { b[0] = 0; s_cat_i(b, frame_us_avg / 1000, 40); s_cat(b, ".", 40); s_cat_i(b, frame_us_avg / 100 % 10, 40); s_cat(b, " MS, WORST ", 40); s_cat_i(b, frame_us_worst / 1000, 40); s_cat(b, " MS", 40); row_info("A LATCH TAKES", b, K_TEXT); }
  b[0] = 0; s_cat_i(b, (int64_t)hal_free_memory(), 40); s_cat(b, " BYTES", 40); row_info("FREE MEMORY", b, K_TEXT);
  b[0] = 0; s_cat_i(b, latch, 40); row_info("LATCHES SINCE POWER-ON", b, K_TEXT);
  b[0] = 0; s_cat_i(b, saves, 40); s_cat(b, " SAVED / ", 40); s_cat_i(b, mends, 40); s_cat(b, " MENDED", 40); row_info("FLASH COPIES", b, K_TEXT);
  row_info("THIS START", safe_mode ? "SAFE START (RULES PAUSED)" : "NORMAL", safe_mode ? K_WARN : K_GOOD);
  if (row_button("CLOSE TASKS")) stop_app(10);
  form_end(); }

/* ---- 12. SETTINGS: controls (tried before kept), a button test, the look, the home screen, the system */
static int set_page, reset_ask; static controls_t ctl_edit;
static const char *GPIO_NAME[9] = { "GPIO 0", "GPIO 1", "GPIO 2", "GPIO 3", "GPIO 8", "GPIO 9", "GPIO 20", "GPIO 21", "NOT FITTED" };
static uint8_t gpio_choice(uint8_t pin) { int i = pin_index(pin); return (uint8_t)(i < 0 ? 8 : i); }
static void controls_use(const controls_t *c) { ST.ctl = *c; hal_inputs(&ST.ctl); for (int i = 0; i < 8; i++) pin_apply(i); hands_reset(); }
static void settings_controls(void) {
  controls_t *e = &ctl_edit; int differs = 0; { const uint8_t *x = (const uint8_t *)e, *y = (const uint8_t *)&ST.ctl; for (size_t i = 0; i < sizeof *e; i++) differs |= x[i] != y[i]; }
  window_frame("CONTROLS", differs ? "CHANGED, NOT IN USE YET" : "IN USE"); form_begin(32, SH - 30);
  uint8_t pr = e->preset; if (row_choice("PRESET", &pr, PRESET_NAME, NPRESET)) controls_preset(e, pr);
  for (int b = 0; b < 4; b++) { char l[24]; s_cpy(l, b == 3 ? "STICK PRESS" : "BUTTON ", 24); if (b < 3) s_cat(l, BT_NAME[b], 24); s_cat(l, ": TAP", 24);
    int ch = row_choice(l, &e->tap[b], ROLE_NAME, NROLE); ch |= row_choice("   HOLD", &e->hold[b], ROLE_NAME, NROLE); ch |= row_choice("   HOLD LONGER", &e->lng[b], ROLE_NAME, NROLE); if (ch) e->preset = 4; }
  row_number("HOLD TIME", &e->hold_ms, 300, 3000, 100, " MS");
  row_number("REPEAT AFTER", &e->rep_delay, 200, 2000, 50, " MS"); row_number("REPEAT EVERY", &e->rep_ms, 50, 1000, 10, " MS");
  row_toggle("STICK MOVES THE FOCUS", &e->stick_on); row_number("   DEAD ZONE (OF 2048)", &e->dead, 300, 1900, 50, "");
  row_toggle("   FLIP LEFT AND RIGHT", &e->inv_x); row_toggle("   FLIP UP AND DOWN", &e->inv_y); row_toggle("   SWAP ITS TWO AXES", &e->swap_xy);
  if (row_button("   CENTRE THE STICK NOW (LET GO OF IT)") && st_x >= 0) { st_cx = st_x; st_cy = st_y; e->cx = ST.ctl.cx = (int16_t)st_x; e->cy = ST.ctl.cy = (int16_t)st_y; toast("STICK CENTRED"); }
  for (int b = 0; b < 4; b++) { char l[32]; s_cpy(l, "WIRING: ", 32); s_cat(l, b == 3 ? "STICK PRESS" : "BUTTON ", 32); if (b < 3) s_cat(l, BT_NAME[b], 32); uint8_t g = gpio_choice(e->pin[b]);
    if (row_choice(l, &g, GPIO_NAME, 9)) e->pin[b] = g == 8 ? 255 : UPIN[g]; }
  { uint8_t g = (uint8_t)(e->vx & 3); if (row_choice("WIRING: STICK X", &g, GPIO_NAME, 4)) e->vx = g; g = (uint8_t)(e->vy & 3); if (row_choice("WIRING: STICK Y", &g, GPIO_NAME, 4)) e->vy = g; }
  if (row_button(differs ? "TRY THESE CONTROLS (15 S TO KEEP)" : "TRY THESE CONTROLS") && differs) { ctl_old = ST.ctl; controls_use(e); ctl_trial_ms = 15000; }
  if (row_button("FORGET THE CHANGES")) *e = ST.ctl;
  if (row_button("BACK TO SETTINGS")) set_page = 0;
  form_end(); }
static void settings_test(void) {                                      /* every input live; nothing acts; hold any button 3 s to leave */
  window_frame("BUTTON TEST", "PRESSES ACT ON NOTHING HERE"); const controls_t *c = &ST.ctl;
  for (int b = 0; b < 4; b++) { int x = 8 + b * 78, y = 34; uint8_t on = bt_down[b]; fill(x, y, 72, 58, on ? K_GOOD : K_PANEL); box(x, y, 72, 58, on ? K_FOCUS : K_LINE);
    text_c(x + 36, y + 4, b == 3 ? "PRESS" : BT_NAME[b], on ? K_BAR : K_WHITE, 2); char t[24];
    if (c->pin[b] == 255) s_cpy(t, "NOT FITTED", 24); else { s_cpy(t, "GPIO ", 24); s_cat_i(t, c->pin[b], 24); } text_c(x + 36, y + 22, t, on ? K_BAR : K_DIM, 1);
    text_c(x + 36, y + 32, on ? "DOWN" : "UP", on ? K_BAR : K_TEXT, 1); s_cpy(t, "PRESSES ", 24); s_cat_i(t, bt_count[b], 24); text_c(x + 36, y + 44, t, on ? K_BAR : K_DIM, 1); }
  int sx0 = 12, sy0 = 100, sz = 80; fill(sx0, sy0, sz, sz, K_SHADOW); box(sx0, sy0, sz, sz, K_LINE);
  { int dz = c->dead * (sz / 2) / 2048; box(sx0 + sz / 2 - dz, sy0 + sz / 2 - dz, 2 * dz, 2 * dz, K_KEY); hline(sx0, sx0 + sz - 1, sy0 + sz / 2, K_KEY); vline(sx0 + sz / 2, sy0, sy0 + sz - 1, K_KEY); }
  char t[48];
  if (c->stick_on && st_x >= 0) { int dx = st_x * (sz - 6) / 4095, dy = st_y * (sz - 6) / 4095; fill(sx0 + 1 + dx, sy0 + 1 + dy, 4, 4, K_FOCUS);
    s_cpy(t, "X ", 48); s_cat_i(t, st_x, 48); s_cat(t, "  Y ", 48); s_cat_i(t, st_y, 48); text(104, 104, t, K_TEXT, 1);
    s_cpy(t, "CENTRE ", 48); s_cat_i(t, st_cx, 48); s_cat(t, ", ", 48); s_cat_i(t, st_cy, 48); text(104, 116, t, K_DIM, 1);
    static const char *DN[5] = { "STILL", "UP", "DOWN", "LEFT", "RIGHT" }; s_cpy(t, "PUSHED: ", 48); s_cat(t, DN[st_dir <= 4 ? st_dir : 0], 48); text(104, 128, t, st_dir ? K_FOCUS : K_DIM, 1); }
  else text(104, 104, c->stick_on ? "NO STICK ANSWERS" : "STICK IS OFF (CONTROLS)", K_DIM, 1);
  if (last_bt >= 0) { static const char *HOW[4] = { "TAP", "HOLD", "LONG HOLD", "REPEAT" }; s_cpy(t, "LAST: ", 48); s_cat(t, BT_NAME[last_bt], 48); s_cat(t, " ", 48); s_cat(t, HOW[last_how & 3], 48);
    s_cat(t, " = ", 48); s_cat(t, ROLE_NAME[last_role % NROLE], 48); text(104, 146, t, K_ACCENT, 1); }
  text(104, 160, "ALSO TOLD ON THE SERIAL MONITOR", K_DIM, 1);
  uint32_t most = 0; for (int b = 0; b < 4; b++) if (bt_down[b] && bt_held[b] > most) most = bt_held[b];
  text(12, 190, "HOLD ANY BUTTON 3 S TO LEAVE", K_TEXT, 1); fill(190, 190, 118, 7, K_KEY); fill(190, 190, (int)i_min(118, most * 118 / 3000), 7, K_FOCUS);
  if (most >= 3000 || (keys_mode() && ev.type == IE_BACK)) { set_page = 0; bt_test = 0; hands_reset(); ev.type = IE_NONE; } }
static void settings_look(void) {
  window_frame("LOOK", THEME_NAME[ST.theme % NTHEME]); form_begin(32, SH - 30);
  uint8_t t = ST.theme; if (row_choice("THEME", &t, THEME_NAME, NTHEME)) { ST.theme = t; theme_apply(t); hal_palette(); }
  row_toggle("LARGE TEXT IN LISTS", &ST.big_text);
  int pm = 0; pm |= row_toggle("PANEL: INVERT COLOURS", &ST.invert); pm |= row_toggle("PANEL: UPSIDE DOWN", &ST.rot180); pm |= row_toggle("PANEL: SWAP RED AND BLUE", &ST.bgr);
  if (pm) hal_panel_mode(ST.invert, ST.rot180, ST.bgr);
  row_info("IF COLOURS LOOK WRONG", "TRY INVERT, THEN SWAP", K_DIM);
  if (row_button("BACK TO SETTINGS")) set_page = 0;
  form_end(); }
static void settings_home(void) {
  int shown = 0; for (int a = 0; a < NAPP; a++) shown += a == 11 || !(ST.home_hide >> a & 1); char note[24]; note[0] = 0; s_cat_i(note, shown, 24); s_cat(note, " SHOWN", 24);
  window_frame("HOME SCREEN", note); form_begin(32, SH - 30);
  for (int a = 0; a < NAPP - 1; a++) { uint8_t v = (uint8_t)!(ST.home_hide >> a & 1); char l[32]; s_cpy(l, "SHOW ", 32); s_cat(l, APPS[a].name, 32);
    if (row_toggle(l, &v)) ST.home_hide = (uint16_t)(v ? ST.home_hide & ~(1u << a) : ST.home_hide | (1u << a)); }
  row_info("SHOW SETTINGS", "ALWAYS", K_DIM);
  if (row_button("SHOW EVERY APP")) ST.home_hide = 0;
  if (row_button("BACK TO SETTINGS")) set_page = 0;
  form_end(); }
static void settings_system(void) {
  window_frame("SYSTEM", safe_mode ? "SAFE START" : 0); form_begin(32, SH - 30);
  if (row_toggle("I2C ON GPIO 8 (SDA) AND 9 (SCL)", &ST.i2c_on)) for (int i = 0; i < 8; i++) pin_apply(i);
  row_toggle("DEMO WHEN LEFT ALONE", &ST.demo_on);
  row_toggle("CHANGES SAVE THEMSELVES", &ST.autosave);
  if (row_button("SAVE NOW")) { os_save(); toast("SAVED"); }
  if (safe_mode && row_button("LEAVE SAFE START: RUN THE PROFILE")) { safe_mode = 0; profile_start(ST.boot_profile); open_app(11); }
  if (row_button(reset_ask ? "SELECT AGAIN: ERASE EVERYTHING" : "FACTORY RESET")) { if (reset_ask) { store_factory(); os_save(); reset_ask = 0; theme_apply(0); hal_palette(); hal_panel_mode(ST.invert, ST.rot180, ST.bgr); controls_use(&ST.ctl); toast("FACTORY SETTINGS"); } else reset_ask = 1; }
  if (f_focus[f_slot] != f_n - 1) reset_ask = 0;                     /* the second press must follow the first on the same row */
  row_info("ABOUT", "IRORI 2  WHOLE NUMBERS, NO CLOCK VARIABLE", K_DIM);
  if (row_button("BACK TO SETTINGS")) set_page = 0;
  form_end(); }
static void app_settings(void) {
  { static int last_page, main_focus; if (set_page != last_page) { if (!last_page) main_focus = f_focus[12]; f_focus[12] = set_page ? 0 : main_focus; f_scroll[12] = 0; last_page = set_page; } }   /* each page its own focus; the main list remembers */
  if (set_page && ev.type == IE_BACK) { set_page = 0; ev.type = IE_NONE; reset_ask = 0; }
  if (set_page == 1) { settings_controls(); return; } if (set_page == 2) { settings_test(); return; } if (set_page == 3) { settings_look(); return; }
  if (set_page == 4) { settings_home(); return; } if (set_page == 5) { settings_system(); return; }
  window_frame("SETTINGS", 0); form_begin(32, SH - 30); char l[48];
  s_cpy(l, "CONTROLS: ", 48); s_cat(l, PRESET_NAME[ST.ctl.preset % NPRESET], 48); if (row_button(l)) { set_page = 1; ctl_edit = ST.ctl; }
  if (row_button("TEST THE BUTTONS AND STICK")) { set_page = 2; bt_test = 1; for (int b = 0; b < 4; b++) bt_count[b] = 0; last_bt = -1; hal_serial_write("IRORI BUTTON TEST: every press is told here\n"); }
  s_cpy(l, "LOOK: ", 48); s_cat(l, THEME_NAME[ST.theme % NTHEME], 48); if (ST.big_text) s_cat(l, ", LARGE TEXT", 48); if (row_button(l)) set_page = 3;
  if (row_button("HOME SCREEN: WHICH APPS SHOW")) set_page = 4;
  if (row_button("SYSTEM: I2C, DEMO, SAVING, RESET")) set_page = 5;
  row_info("SERIAL KEYS", "ARROWS/WASD ENTER ESC TAB", K_DIM);
  row_info("ABOUT", "IRORI 2", K_DIM);
  if (row_button("CLOSE SETTINGS")) stop_app(11);
  form_end(); }

/* ---------------------------------------------------------------- the apps, their icons, and the home screen */
static const app_t APP_TABLE[NAPP] = {
  { "PINS", { "................", "..#..#..#..#....", "..#..#..#..#....", ".############...", ".#..........#...", ".#.##.......#...", ".#.##.......#...", ".#..........#...",
              ".#..........#...", ".#..........#...", ".#..........#...", ".############...", "..#..#..#..#....", "..#..#..#..#....", "................", "................" }, 0, 1, app_pins, tick_pins, 8 },
  { "PLOTTER", { "#...............", "#.......##......", "#......#..#.....", "#.....#....#....", "#.....#....#....", "#....#......#...", "#.##.#......#..#", "##..#........#.#",
              "#............#.#", "#.............#.", "#...............", "#...............", "################", "................", "................", "................" }, 1, 1, app_plot, tick_plot, 4 },
  { "SERVO", { "................", "....########....", "..##........##..", ".#..........#.#.", ".#.........#..#.", "#.........#....#", "#........#.....#", "#.......##.....#",
              "#.......##.....#", "#..............#", ".#............#.", ".#............#.", "..##........##..", "....########....", "................", "................" }, 2, 1, app_servo, tick_servo, 2 },
  { "RULES", { "................", "................", "#####......#####", "#...#......#...#", "#.#.#...#..#.#.#", "#...#....#.#...#", "#...########...#", "#...#....#.#...#",
              "#...#...#..#...#", "#####......#####", "................", "................", "................", "................", "................", "................" }, 3, 1, app_rules, tick_rules, 16 },
  { "TERMINAL", { "################", "#..............#", "#.#............#", "#..#...........#", "#...#..........#", "#..#...........#", "#.#....#####...#", "#..............#",
              "#..............#", "################", "................", "................", "................", "................", "................", "................" }, 4, 1, app_terminal, 0, 0 },
  { "I2C SCAN", { "................", "..##........##..", ".####......####.", ".####......####.", "..##........##..", "...#........#...", "....#......#....", ".....#....#.....",
              "......#..#......", ".......##.......", "......####......", "......####......", ".......##.......", "................", "................", "................" }, 5, 1, app_i2c, tick_i2c, 8 },
  { "CALCULATOR", { "..############..", "..#..........#..", "..#.########.#..", "..#.########.#..", "..#..........#..", "..#.##.##.##.#..", "..#.##.##.##.#..", "..#..........#..",
              "..#.##.##.##.#..", "..#.##.##.##.#..", "..#..........#..", "..#.##.##.##.#..", "..#.##.##.##.#..", "..#..........#..", "..############..", "................" }, 6, 0, app_calc, 0, 0 },
  { "NOTES", { "...#########....", "...#.......##...", "...#.......#.#..", "...#.......####.", "...#..........#.", "...#.######...#.", "...#..........#.", "...#.######...#.",
              "...#..........#.", "...#.######...#.", "...#..........#.", "...#.####.....#.", "...#..........#.", "...############.", "................", "................" }, 7, 0, app_notes, 0, 0 },
  { "TIMER", { "................", ".....######.....", "...##......##...", "..#....#.....#..", ".#.....#......#.", ".#.....#......#.", "#......#.......#", "#......#####...#",
              "#..............#", "#..............#", ".#............#.", ".#............#.", "..#..........#..", "...##......##...", ".....######.....", "................" }, 8, 1, app_timer, tick_timer, 1 },
  { "PROFILES", { "................", "...##########...", "..#..........#..", "...##########...", "................", "...##########...", "..#..........#..", "...##########...",
              "................", "...##########...", "..#..........#..", "...##########...", "................", "................", "................", "................" }, 9, 0, app_profiles, 0, 0 },
  { "TASKS", { "................", "................", "..##............", "..##......##....", "..##......##....", "..##..##..##....", "..##..##..##..##", "..##..##..##..##",
              "..##..##..##..##", "..##..##..##..##", "################", "................", "................", "................", "................", "................" }, 10, 0, app_tasks, 0, 0 },
  { "SETTINGS", { "................", ".......##.......", "...#..####..#...", "..###.####.###..", "...##########...", "....###..###....", "..####....####..", "..####....####..",
              "....###..###....", "...##########...", "..###.####.###..", "...#..####..#...", ".......##.......", "................", "................", "................" }, 11, 0, app_settings, 0, 0 } };
static const app_t *apps_table(void) { return APP_TABLE; }
static int home_list(int *L) { int n = 0; for (int a = 0; a < NAPP; a++) if (a == 11 || !(ST.home_hide >> a & 1)) L[n++] = a; return n; }
static void home(void) {
  fill(0, 14, SW, SH - 40, K_BG);
  for (int y = 14; y < SH - 26; y += 2) hline(0, SW - 1, y, (uint8_t)(y & 2 ? K_BG : K_BG2));
  int L[NAPP], n = home_list(L), p = 0; for (int i = 0; i < n; i++) if (L[i] == home_focus) p = i; home_focus = L[p];
  if (ev.type == IE_LEFT) { p = (p + n - 1) % n; ev.type = IE_NONE; } if (ev.type == IE_RIGHT) { p = (p + 1) % n; ev.type = IE_NONE; }
  if (ev.type == IE_UP) { if (p >= 4) p -= 4; else { int q = p; while (q + 4 < n) q += 4; p = q; } ev.type = IE_NONE; }        /* up from the top row: the bottom of the same column */
  if (ev.type == IE_DOWN) { p = p + 4 < n ? p + 4 : p % 4; ev.type = IE_NONE; }
  home_focus = L[p];
  if (ev.type == IE_OK) { open_app(home_focus); ev.type = IE_NONE; return; }
  for (int i = 0; i < n; i++) { int a = L[i], c = i % 4, r = i / 4, x = 8 + c * 78, y = 20 + r * 64, f = i == p, on = running >> a & 1;
    fill(x, y, 72, 60, f ? K_TITLE : K_PANEL); if (f) { box(x, y, 72, 60, K_FOCUS); box(x + 1, y + 1, 70, 58, K_FOCUS); }
    icon(x + 20, y + 6, APPS[a].icon, (uint8_t)(K_APP0 + APPS[a].colour), 2);
    text_c(x + 36, y + 46, APPS[a].name, f ? K_WHITE : K_TEXT, 1); if (on) fill(x + 64, y + 4, 4, 4, K_GOOD); } }
static const char *KB[4] = { "1234567890", "QWERTYUIOP", "ASDFGHJKL.", "ZXCVBNM,?!" };
static void keyboard(void) {                                           /* on-screen keys; typed keys from serial or a CardKB also land here */
  int x0 = 10, y0 = 70; fill(4, y0 - 34, SW - 8, 178, K_SHADOW); box(4, y0 - 34, SW - 8, 178, K_ACCENT);
  text(10, y0 - 30, kb_title, K_FOCUS, 1); char b[44]; int n = s_len(kb_buf); s_cpy(b, n > 40 ? kb_buf + n - 40 : kb_buf, 44); s_cat(b, "_", 44); fill(8, y0 - 20, SW - 16, 14, K_BG); text(12, y0 - 17, b, K_WHITE, 1);
  if (ev.type == IE_UP) { kb_row = (kb_row + 4) % 5; ev.type = IE_NONE; } if (ev.type == IE_DOWN) { kb_row = (kb_row + 1) % 5; ev.type = IE_NONE; }
  int cols = kb_row == 4 ? 4 : 10;
  if (ev.type == IE_LEFT) { kb_col = (kb_col + cols - 1) % cols; ev.type = IE_NONE; } if (ev.type == IE_RIGHT) { kb_col = (kb_col + 1) % cols; ev.type = IE_NONE; }
  if (kb_col >= cols) kb_col = cols - 1;
  if (ev.type == IE_NEXT || ev.type == IE_PREV) { int k = kb_row * 10 + kb_col; k = (k + (ev.type == IE_NEXT ? 1 : 43)) % 44; kb_row = k / 10; kb_col = k % 10; ev.type = IE_NONE; }   /* buttons only: every key in reading order */
  int put = -1; if (ev.type == IE_CHAR) { put = ev.ch; ev.type = IE_NONE; }
  if (ev.type == IE_OK) { ev.type = IE_NONE; if (kb_row < 4) { char c = KB[kb_row][kb_col]; put = (kb_shift || c < 'A' || c > 'Z') ? c : c + 32; }
    else if (kb_col == 0) kb_shift = !kb_shift; else if (kb_col == 1) put = ' '; else if (kb_col == 2) put = 8; else { kb_on = 0; os_save(); } }
  if (ev.type == IE_BACK) { ev.type = IE_NONE; kb_on = 0; os_save(); }
  if (put == 8) { if (n) kb_buf[n - 1] = 0; } else if (put > 0 && n < kb_cap - 1) { kb_buf[n] = (char)put; kb_buf[n + 1] = 0; }
  for (int r = 0; r < 4; r++) for (int c = 0; c < 10; c++) { int x = x0 + c * 30, y = y0 + r * 22, f = r == kb_row && c == kb_col; char ch[2] = { KB[r][c], 0 };
      if (!kb_shift && ch[0] >= 'A' && ch[0] <= 'Z') ch[0] = (char)(ch[0] + 32);
      fill(x, y, 27, 19, f ? K_TITLE : K_KEY); if (f) box(x, y, 27, 19, K_FOCUS); text_c(x + 13, y + 6, ch, f ? K_WHITE : K_TEXT, 1); }
  static const char *BOT[4] = { "SHIFT", "SPACE", "DELETE", "DONE" };
  for (int c = 0; c < 4; c++) { int x = x0 + c * 75, y = y0 + 4 * 22, f = kb_row == 4 && c == kb_col; fill(x, y, 72, 19, f ? K_TITLE : (c == 3 ? K_GOOD : K_KEY)); if (f) box(x, y, 72, 19, K_FOCUS);
    text_c(x + 36, y + 6, BOT[c], f ? K_WHITE : c == 3 ? K_BAR : K_TEXT, 1); } }

/* ---------------------------------------------------------------- the demo: a ghost that tours the apps when left alone (also the test's hand) */
/* each step: K_EV pushes an event; K_OPEN opens an app (from home, as a hand would) with its focus on a row; K_ROW puts the focus on a row */
enum { G_EV, G_OPEN, G_ROW };
static const gstep_t GHOST[] = {
  { G_OPEN, 0, 5, 30 }, { G_EV, IE_RIGHT, 0, 20 }, { G_ROW, 6, 0, 10 }, { G_EV, IE_OK, 0, 40 }, { G_EV, IE_OK, 0, 30 }, { G_ROW, 5, 0, 6 }, { G_EV, IE_LEFT, 0, 30 },   /* PINS: GPIO 9 out, on, off, back to input (GPIO 8 is rule 3's) */
  { G_OPEN, 1, 0, 120 },                                                                                                                                              /* PLOTTER: the stick's two axes, rate(A, B) */
  { G_OPEN, 2, 3, 20 }, { G_EV, IE_OK, 0, 90 }, { G_EV, IE_OK, 0, 20 },                                                                                             /* SERVO: sweep on, then off */
  { G_OPEN, 3, 2, 60 }, { G_EV, IE_OK, 0, 70 }, { G_EV, IE_BACK, 0, 20 },                                                                                           /* RULES: the list, rule 3 opened, back */
  { G_OPEN, 6, 0, 20 }, { G_EV, IE_CHAR, '1', 8 }, { G_EV, IE_CHAR, 'f', 8 }, { G_EV, IE_CHAR, '3', 8 }, { G_EV, IE_CHAR, '+', 8 }, { G_EV, IE_CHAR, '1', 8 },
  { G_EV, IE_CHAR, 'f', 8 }, { G_EV, IE_CHAR, '6', 8 }, { G_EV, IE_CHAR, '=', 60 }, { G_EV, IE_CHAR, '/', 8 }, { G_EV, IE_CHAR, '0', 8 }, { G_EV, IE_CHAR, '=', 60 },
  { G_EV, IE_CHAR, 'c', 10 },                                                                                                                                         /* CALCULATOR: 1/3 + 1/6 = 1/2, then / 0 = THETA */
  { G_OPEN, 8, 0, 20 }, { G_EV, IE_OK, 0, 80 }, { G_EV, IE_OK, 0, 20 }, { G_ROW, 1, 0, 6 }, { G_EV, IE_OK, 0, 20 },                                                 /* TIMER: stopwatch start, stop, reset */
  { G_OPEN, 5, 0, 30 }, { G_OPEN, 9, 0, 40 }, { G_EV, IE_OK, 0, 50 }, { G_EV, IE_BACK, 0, 20 },                                                                      /* I2C SCAN, PROFILES: bench opened, back */
  { G_OPEN, 10, 0, 60 }, { G_OPEN, 11, 0, 60 },                                                                                                                       /* TASKS, SETTINGS */
  { G_EV, IE_SWITCH, 0, 30 }, { G_EV, IE_SWITCH, 0, 30 }, { G_EV, IE_SWITCH, 0, 30 }, { G_EV, IE_SWITCH, 0, 30 }, { G_EV, IE_BACK, 0, 90 } };                    /* through the running apps, home */
#define NGHOST ((int)(sizeof GHOST / sizeof GHOST[0]))
static int g_at, g_wait;
static void ghost_latch(void) {
  if (g_wait > 0) { g_wait--; return; } const gstep_t *g = &GHOST[g_at];
  if (g->kind == G_EV) os_push(g->a, g->b);
  else if (g->kind == G_OPEN) { kb_on = 0; fg = -1; home_focus = g->a; f_focus[g->a + 1] = g->b; f_scroll[g->a + 1] = 0; if (g->a == 11) set_page = 0; os_push(IE_OK, 0); }
  else if (fg >= 0) f_focus[fg + 1] = g->a;
  g_wait = g->wait; g_at = (g_at + 1) % NGHOST; }

/* ---------------------------------------------------------------- one latch of the OS */
static void os_begin(void) {
  face_begin(); store_begin(); theme_apply(ST.theme); hal_palette(); hal_panel_mode(ST.invert, ST.rot180, ST.bgr);
  hal_inputs(&ST.ctl); if (hal_boot_trouble() >= 3) safe_mode = 1;     /* A or B held at power-on, or three troubled starts in a row */
  if (safe_mode) { controls_factory(&ST.ctl); hal_inputs(&ST.ctl); for (int r = 0; r < 8; r++) ST.rules[r].on = 0; running = 0; for (int i = 0; i < 8; i++) pin_apply(i); toast("SAFE START: RULES PAUSED, CONTROLS RESET"); }
  else profile_start(ST.boot_profile);
  st_cx = ST.ctl.cx; st_cy = ST.ctl.cy;
  { int x, y; if (ST.ctl.stick_on && hal_stick(&x, &y) && x > 1348 && x < 2748 && y > 1348 && y < 2748) { st_cx = x; st_cy = y; } }   /* a stick at rest when powered: its centre now */
  if (carried) toast("SETTINGS CARRIED OVER FROM IRORI 1");
  hands_reset(); fg = -1; clock_last = hal_clock_ms(); ghost_on = ST.demo_on && !safe_mode; }
static void select_word(char *b, int cap) {                            /* the name of whatever selects, from the roles chosen */
  const controls_t *c = &ST.ctl; b[0] = 0; if (keys_mode()) { s_cpy(b, "ENTER", cap); return; }
  for (int k = 0; k < 4; k++) if (c->pin[k] != 255 && c->tap[k] == R_SELECT) { if (b[0]) s_cat(b, " OR ", cap); s_cat(b, BT_NAME[k], cap); } if (b[0]) return;
  for (int k = 0; k < 4; k++) if (c->pin[k] != 255 && (c->hold[k] == R_SELECT || c->lng[k] == R_SELECT)) { s_cpy(b, "HOLD ", cap); s_cat(b, BT_NAME[k], cap); return; }
  s_cpy(b, "(NOTHING SELECTS)", cap); }
static void trial_banner(void) {
  int x = 20, y = 70, w = SW - 40, h = 78; fill(x, y, w, h, K_SHADOW); box(x, y, w, h, K_FOCUS); box(x + 1, y + 1, w - 2, h - 2, K_FOCUS);
  text_c(SW / 2, y + 8, "NEW CONTROLS ON TRIAL", K_FOCUS, 2);
  char b[48], sw[24]; select_word(sw, 24); s_cpy(b, "PRESS ", 48); s_cat(b, sw, 48); s_cat(b, " TO KEEP THEM", 48); text_c(SW / 2, y + 32, b, K_WHITE, 1);
  s_cpy(b, "THE OLD ONES COME BACK IN ", 48); s_cat_i(b, (ctl_trial_ms + 999) / 1000, 48); s_cat(b, " S", 48); text_c(SW / 2, y + 46, b, K_TEXT, 1);
  fill(x + 10, y + 62, w - 20, 6, K_KEY); fill(x + 10, y + 62, (w - 20) * ctl_trial_ms / 15000, 6, K_FOCUS); }
static void toast_draw(void) { int w = text_w(toast_text, 1) + 16, x = (SW - w) / 2, y = SH - 46; fill(x, y, w, 15, K_TITLE); box(x, y, w, 15, K_FOCUS); text(x + 8, y + 4, toast_text, K_WHITE, 1); }
static void os_latch(void) {
  latch++; uint32_t now = hal_clock_ms(); dclock = now - clock_last; clock_last = now; clock_total += dclock;      /* the clock as a reading: only what it changed */
  toast_ms = toast_ms > dclock ? toast_ms - dclock : 0;
  for (int c; (c = hal_serial_read()) >= 0;) {                                     /* data and commands do not share a channel by accident */
    int as_keys = keys_mode() || kb_on || fg == 6;                                  /* keys only in keyboard-only mode, or while typing */
    if (as_keys && !(fg == 4 && !kb_on && c != 27 && c != '\t')) { os_input(1); os_key(c, kb_on || fg == 6); } else term_put(c); }
  if (ST.i2c_on) { int c = hal_cardkb(); if (c >= 0) { os_input(1); os_key(c, kb_on || fg == 6); } }
  hands_latch();
  if (bt_test && !(fg == 11 && set_page == 2)) bt_test = 0;
  if (++idle > 900 && ST.demo_on && !ghost_on && !safe_mode && !bt_test && !ctl_trial_ms) { ghost_on = 1; g_at = 0; g_wait = 0; }
  if (ghost_on) ghost_latch();
  ev.type = IE_NONE; if (eq_out < eq_in) { ev = EQ[eq_out % EQN]; eq_out++; }
  if (ctl_trial_ms > 0) {                                                          /* new controls on trial: only a SELECT made with them keeps them */
    if (ev.type == IE_OK) { ctl_trial_ms = 0; toast("CONTROLS KEPT"); }
    else { ctl_trial_ms -= (int32_t)dclock; if (ctl_trial_ms <= 0) { ctl_trial_ms = 0; controls_use(&ctl_old); toast("OLD CONTROLS PUT BACK"); } }
    ev.type = IE_NONE; }
  if (ev.type == IE_HOME) { if (kb_on) { kb_on = 0; } fg = -1; ev.type = IE_NONE; }
  if ((ev.type == IE_NEXT || ev.type == IE_PREV) && !kb_on && fg != 6) ev.type = (uint8_t)(fg < 0 ? (ev.type == IE_NEXT ? IE_RIGHT : IE_LEFT) : (ev.type == IE_NEXT ? IE_DOWN : IE_UP));
  for (int a = 0; a < NAPP; a++) { work_now[a] = 0; if ((running >> a & 1) && APPS[a].tick) { APPS[a].tick(); work_now[a] = APPS[a].weight; work_all[a] += APPS[a].weight; } }
  if ((latch & 63) == 0) keeper_turn();
  autosave_latch(ctl_trial_ms > 0 || kb_on);
  clipx0 = 0; clipy0 = 0; clipx1 = SW; clipy1 = SH;
  if (!kb_on && ev.type == IE_SWITCH) { switch_app(); ev.type = IE_NONE; }
  if (kb_on) { ievent_t keep = ev; ev.type = IE_NONE; clipy1 = SH - 26; if (fg >= 0) APPS[fg].draw(); else home(); ev = keep; clipy1 = SH; keyboard(); }   /* under the keyboard, nothing takes the key */
  else if (fg < 0) { clipy1 = SH - 26; home(); if (fg >= 0) { ev.type = IE_NONE; APPS[fg].draw(); } }     /* an app opened: show it this latch, not an empty frame */
  else { clipy1 = SH - 26; APPS[fg].draw(); if (ev.type == IE_BACK) { fg = -1; ev.type = IE_NONE; home(); } }
  clipy1 = SH; char hint[72]; hint_build(hint, 72, kb_on ? 2 : fg < 0 ? 0 : 1); top_bar(); bottom_bar(hint);
  if (ctl_trial_ms > 0) trial_banner();
  if (toast_ms) toast_draw(); }
static uint32_t fb_print(void) { uint32_t h = 2166136261u; for (int i = 0; i < SW * SH; i++) h = (h ^ FB[i]) * 16777619u; return h; }

// ---------------------------------------------------------------- the panel's wire (HITOMI's queue) and OS 4's change-only glass
#define QN 64
static spi_device_handle_t dev; static int m_e1, m_e2;
static spi_transaction_t TQ[QN]; static uint8_t PRM[QN][8]; static uint32_t q_in, q_done, q_mark[2];
static void IRAM_ATTR dc_cb(spi_transaction_t *t) { gpio_set_level((gpio_num_t)PIN_DC, (int)(intptr_t)t->user); }
static void q_reap(void) { spi_transaction_t *r; spi_device_get_trans_result(dev, &r, portMAX_DELAY); q_done++; }
static spi_transaction_t *q_slot(int *k) { if (q_in - q_done >= QN) q_reap(); *k = (int)(q_in % QN); spi_transaction_t *t = &TQ[*k]; memset(t, 0, sizeof *t); return t; }
static void q_go(spi_transaction_t *t) { spi_device_queue_trans(dev, t, portMAX_DELAY); q_in++; }
static void wire_cmd(uint8_t c, const uint8_t *p, int n) {
  int k; spi_transaction_t *t = q_slot(&k); t->length = 8; t->flags = SPI_TRANS_USE_TXDATA; t->tx_data[0] = c; t->user = (void *)0; q_go(t);
  if (n) { t = q_slot(&k); t->length = 8 * n; t->user = (void *)1;
    if (n <= 4) { t->flags = SPI_TRANS_USE_TXDATA; memcpy(t->tx_data, p, n); } else { memcpy(PRM[k], p, n); t->tx_buffer = PRM[k]; }
    q_go(t); } }
static void wire_data(const void *p, int nb) { int k; spi_transaction_t *t = q_slot(&k); t->length = 8 * nb; t->tx_buffer = p; t->user = (void *)1; q_go(t); }
static void wire_fence(int s) { q_mark[s] = q_in; }
static void wire_wait(int s) { while ((int32_t)(q_done - q_mark[s]) < 0) q_reap(); }
static void wire_wait_all(void) { while (q_done != q_in) q_reap(); }
#define TW 32
#define TH 16
static uint32_t TILEP[SH / TH][SW / TW]; static uint16_t *RB[2]; static uint32_t slot_n; static uint16_t PAL565[NCOLOUR];
static void window(int x0, int x1, int y0, int y1) {
  uint8_t a[4] = { (uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1 }; wire_cmd(0x2A, a, 4);
  uint8_t b[4] = { (uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1 }; wire_cmd(0x2B, b, 4); wire_cmd(0x2C, 0, 0); }
static void glass_forget(void) { memset(TILEP, 0, sizeof TILEP); }
static void show(void) {                                              /* a tile whose print is unchanged is not sent again */
  for (int ty = 0; ty < SH / TH; ty++) { uint32_t chg = 0;
    for (int tx = 0; tx < SW / TW; tx++) { uint32_t h = 0x811c9dc5u; const uint8_t *p = FB + ty * TH * SW + tx * TW;
      for (int r = 0; r < TH; r++) { const uint32_t *q = (const uint32_t *)(p + r * SW); for (int k = 0; k < TW / 4; k++) { h = (h ^ q[k]) * 0x01000193u; h ^= h >> 15; } }
      if (h != TILEP[ty][tx]) { TILEP[ty][tx] = h; chg |= 1u << tx; } }
    for (int t = 0; t < SW / TW;) { if (!(chg >> t & 1)) { t++; continue; } int t1 = t; while (t1 < SW / TW && (chg >> t1 & 1)) t1++;
      int s = (int)(slot_n++ & 1); uint16_t *rb = RB[s]; wire_wait(s); int w = (t1 - t) * TW;
      for (int r = 0; r < TH; r++) { const uint8_t *src = FB + (ty * TH + r) * SW + t * TW; uint16_t *o = rb + r * w; for (int k = 0; k < w; k++) o[k] = PAL565[src[k]]; }
      window(t * TW, t1 * TW - 1, ty * TH, ty * TH + TH - 1); wire_data(rb, w * TH * 2); wire_fence(s); t = t1; } } }
static void panel_begin(void) {
  pinMode(PIN_RST, OUTPUT); digitalWrite(PIN_RST, HIGH); delay(5); digitalWrite(PIN_RST, LOW); delay(20); digitalWrite(PIN_RST, HIGH); delay(150);
  gpio_set_direction((gpio_num_t)PIN_DC, GPIO_MODE_OUTPUT);
  spi_bus_config_t bus; memset(&bus, 0, sizeof bus);
  bus.mosi_io_num = PIN_MOSI; bus.miso_io_num = -1; bus.sclk_io_num = PIN_SCK; bus.quadwp_io_num = -1; bus.quadhd_io_num = -1; bus.max_transfer_sz = SW * TH * 2 + 64;
  m_e1 = (int)spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
  spi_device_interface_config_t d; memset(&d, 0, sizeof d);
  d.clock_speed_hz = WIRE_HZ; d.mode = 0; d.spics_io_num = PIN_CS; d.queue_size = QN; d.pre_cb = dc_cb;
  m_e2 = (int)spi_bus_add_device(SPI2_HOST, &d, &dev);
  wire_cmd(0x01, 0, 0); wire_wait_all(); delay(150); wire_cmd(0x11, 0, 0); wire_wait_all(); delay(120);
  { uint8_t v = 0x55; wire_cmd(0x3A, &v, 1); } wire_cmd(0x13, 0, 0); wire_cmd(0x29, 0, 0); wire_wait_all(); }
// ---------------------------------------------------------------- the hardware layer IRORI asks for
static uint8_t pwm_on[22];
static int  hal_pin_read(int pin) { return digitalRead(pin); }
static void hal_pin_write(int pin, int v) { if (pwm_on[pin]) { ledcDetach(pin); pwm_on[pin] = 0; pinMode(pin, OUTPUT); } digitalWrite(pin, v ? HIGH : LOW); }
static void hal_pin_mode(int pin, int mode) { if (pwm_on[pin] && mode != 2) { ledcDetach(pin); pwm_on[pin] = 0; }
  if (mode == 0) pinMode(pin, INPUT_PULLUP); else if (mode == 1) pinMode(pin, OUTPUT); else if (mode == 3) pinMode(pin, INPUT); }
static int  hal_adc(int pin) { return analogRead(pin); }
static void hal_pwm(int pin, int permille) { if (pwm_on[pin] != 1) { if (pwm_on[pin]) ledcDetach(pin); ledcAttach(pin, 5000, 10); pwm_on[pin] = 1; } ledcWrite(pin, (uint32_t)(permille * 1023 / 1000)); }
static void hal_servo(int pin, int degrees) { if (pwm_on[pin] != 2) { if (pwm_on[pin]) ledcDetach(pin); ledcAttach(pin, 50, 14); pwm_on[pin] = 2; }
  uint32_t us = 500u + (uint32_t)degrees * 2000u / 180u; ledcWrite(pin, us * 16383u / 20000u); }     /* 0.5 to 2.5 ms of a 20 ms frame */
static int wire_up;
static void i2c_check(void) { if (ST.i2c_on && !wire_up) { Wire.begin(8, 9); Wire.setTimeOut(5); wire_up = 1; } if (!ST.i2c_on && wire_up) { Wire.end(); wire_up = 0; } }
static int  hal_i2c_probe(int addr) { i2c_check(); if (!wire_up) return 0; Wire.beginTransmission((uint8_t)addr); return Wire.endTransmission() == 0; }
static int  hal_serial_read(void) { return Serial.available() ? Serial.read() : -1; }
static void hal_serial_write(const char *s) { Serial.print(s); }
static uint32_t hal_clock_ms(void) { return millis(); }
#define STORE_BYTES (2 * (int)sizeof(store_t) + 16)
static uint8_t hal_store_get(int addr) { return EEPROM.read(addr); }
static void hal_store_put(int addr, uint8_t v) { EEPROM.write(addr, v); }
static void hal_store_commit(void) { EEPROM.commit(); }
static uint32_t cardkb_last;
static int  hal_cardkb(void) { i2c_check(); if (!wire_up || millis() - cardkb_last < 20) return -1; cardkb_last = millis();
  if (Wire.requestFrom((uint8_t)0x5F, (uint8_t)1) != 1) return -1; int c = Wire.read(); return c > 0 ? c : -1; }
static void hal_panel_mode(int invert, int rot180, int bgr) {
  uint8_t m = (uint8_t)((rot180 ? 0xA0 : 0x60) | (bgr ? 0x08 : 0)); wire_cmd(0x36, &m, 1); wire_cmd(invert ? 0x21 : 0x20, 0, 0); wire_wait_all(); glass_forget(); }
static uint32_t hal_free_memory(void) { return ESP.getFreeHeap(); }
// ---------------------------------------------------------------- hands: each button counted by its own interrupt (a tap shorter than a latch is not lost), debounced by time
static portMUX_TYPE bt_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint8_t bt_state[4]; static volatile uint16_t bt_press[4]; static volatile uint32_t bt_t[4]; static uint8_t bt_pin[4] = { 255, 255, 255, 255 }; static uint32_t bt_deb_us = 25000;
static uint8_t st_on, st_vx, st_vy;
static void IRAM_ATTR bt_isr(void *arg) {
  int b = (int)(intptr_t)arg; uint8_t pin = bt_pin[b]; if (pin == 255) return; int lvl = gpio_get_level((gpio_num_t)pin); uint32_t t = micros();
  portENTER_CRITICAL_ISR(&bt_mux);
  if (t - bt_t[b] >= bt_deb_us) { if (!lvl && !bt_state[b]) { bt_state[b] = 1; bt_press[b] = (uint16_t)(bt_press[b] + 1); bt_t[b] = t; } else if (lvl && bt_state[b]) { bt_state[b] = 0; bt_t[b] = t; } }
  portEXIT_CRITICAL_ISR(&bt_mux); }
static void hal_buttons(uint8_t down[4], uint8_t presses[4]) {
  for (int b = 0; b < 4; b++) { if (bt_pin[b] == 255) { down[b] = 0; presses[b] = 0; continue; }
    int lvl = digitalRead(bt_pin[b]); uint32_t t = micros();
    portENTER_CRITICAL(&bt_mux);                                       /* an edge lost inside a bounce is caught up here from the level */
    if (t - bt_t[b] >= bt_deb_us) { if (!lvl && !bt_state[b]) { bt_state[b] = 1; bt_press[b] = (uint16_t)(bt_press[b] + 1); bt_t[b] = t; } else if (lvl && bt_state[b]) { bt_state[b] = 0; bt_t[b] = t; } }
    presses[b] = (uint8_t)(bt_press[b] > 255 ? 255 : bt_press[b]); bt_press[b] = 0; down[b] = bt_state[b];
    portEXIT_CRITICAL(&bt_mux); } }
static int hal_stick(int *x, int *y) { if (!st_on) return 0; *x = analogRead(st_vx); *y = analogRead(st_vy); return 1; }
static void hal_inputs(const controls_t *c) {                          /* a button with no role is not taken: PINS may use its pin */
  for (int b = 0; b < 4; b++) { uint8_t want = (c->pin[b] != 255 && (c->tap[b] || c->hold[b] || c->lng[b])) ? c->pin[b] : 255;
    if (bt_pin[b] != 255 && bt_pin[b] != want) { detachInterrupt(bt_pin[b]); bt_pin[b] = 255; }
    if (want != 255 && bt_pin[b] != want) { pinMode(want, INPUT_PULLUP); bt_state[b] = (uint8_t)!digitalRead(want); bt_press[b] = 0; bt_t[b] = micros(); bt_pin[b] = want; attachInterruptArg(want, bt_isr, (void *)(intptr_t)b, CHANGE); } }
  bt_deb_us = (uint32_t)i_clamp(c->debounce, 5, 100) * 1000u; st_on = c->stick_on; st_vx = c->vx; st_vy = c->vy;
  if (st_on) { pinMode(st_vx, INPUT); pinMode(st_vy, INPUT); } }
static void hal_palette(void) {
  for (int k = 0; k < NCOLOUR; k++) { uint32_t r = PALRGB[k][0], g = PALRGB[k][1], b = PALRGB[k][2]; uint16_t v = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); PAL565[k] = (uint16_t)((v >> 8) | (v << 8)); }
  glass_forget(); }
// ---------------------------------------------------------------- starts that followed trouble (kept in memory that survives a reset, not a power-off)
RTC_NOINIT_ATTR static uint32_t trouble_magic, trouble_n; static int boot_held;
static int hal_boot_trouble(void) { return (int)trouble_n + (boot_held ? 100 : 0); }
static void trouble_begin(void) {
  esp_reset_reason_t r = esp_reset_reason(); int bad = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT || r == ESP_RST_BROWNOUT;
  if (trouble_magic != 0x49524F52u) { trouble_magic = 0x49524F52u; trouble_n = 0; } trouble_n = bad ? trouble_n + 1 : 0;
  pinMode(PIN_A, INPUT_PULLUP); pinMode(PIN_B, INPUT_PULLUP); delay(5); boot_held = !digitalRead(PIN_A) || !digitalRead(PIN_B);   /* the factory wiring, read before anything else */
  Serial.printf("IRORI: start reason %d, troubled starts in a row %u%s\n", (int)r, (unsigned)trouble_n, boot_held ? ", A or B held: safe start" : ""); }
// ---------------------------------------------------------------- run
void setup(void) {
  Serial.begin(115200); delay(300); trouble_begin(); analogReadResolution(12);
  RB[0] = (uint16_t *)heap_caps_malloc(SW * TH * 2, MALLOC_CAP_DMA | MALLOC_CAP_8BIT); RB[1] = (uint16_t *)heap_caps_malloc(SW * TH * 2, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  EEPROM.begin(STORE_BYTES);
  panel_begin(); Serial.printf("IRORI: wire bus %d device %d (0 is good), strips %s\n", m_e1, m_e2, RB[0] && RB[1] ? "ok" : "REFUSED");
  os_begin(); i2c_check(); enableLoopWDT();                            /* a loop stuck for 5 s restarts the board; three such starts in a row give a safe start */
  Serial.printf("IRORI 2: profile %s, %u bytes free%s. Settings > TEST THE BUTTONS AND STICK shows every press here.\n", ST.prof[ST.boot_profile % NPROF].name, (unsigned)ESP.getFreeHeap(), safe_mode ? ", SAFE START" : ""); }
static uint32_t pace, rep_n, rep_sum, rep_max, frames_ok;
void loop(void) {
  while ((uint32_t)(millis() - pace) < 33) delay(1);                   /* pace only: about 30 latches a second; no reading depends on it */
  pace = millis(); uint32_t t0 = micros();
  i2c_check(); os_latch(); show();
  uint32_t us = micros() - t0; rep_sum += us; if (us > rep_max) rep_max = us;
  if (++frames_ok == 1800) trouble_n = 0;                              /* a minute of good running: the trouble count starts again */
  if (++rep_n >= 300) { frame_us_avg = rep_sum / rep_n; frame_us_worst = rep_max;
    Serial.printf("IRORI latch %u | %s in front | %u running | latch avg %u us worst %u us | saved %u mended %u | free %u\n", (unsigned)latch,
      fg < 0 ? "HOME" : APPS[fg].name, (unsigned)__builtin_popcount(running), (unsigned)(rep_sum / rep_n), (unsigned)rep_max, (unsigned)saves, (unsigned)mends, (unsigned)ESP.getFreeHeap()); rep_n = 0; rep_sum = 0; rep_max = 0; } }
