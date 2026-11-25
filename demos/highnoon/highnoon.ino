// LESSON-BEGIN 1 of 24: LESSON MAP: HIGH NOON, choosing the construction in which the correction never arises
// SKILL      : the carry rule as a ray walker; a ray built so its length is already the square-on depth (no fisheye, no angle);
//              distance and sun as palette reads, not per-dot arithmetic; art grown from rules; a ghost that plays the game so
//              desk and chip can be compared dot for dot.
// BENCH      : gcc -O2 -w -DSKETCH='"../demos/highnoon/highnoon.ino"' -o /tmp/hn ../../tools/desk_panel.c && /tmp/hn 600
//              (prints a panel hash each 50 frames; SHOTS=/tmp/dir writes PPM frames to look at)
// READ FIRST : demos/highnoon/README.md; docs/METHOD.md section 5 step 6 ("choose the construction in which the correction never arises").
// SECTIONS   : 2 shapes | 3 mouths | 4 panel | 5 tools | 6 face | 7 ring | 8 materials and palettes | 9 noise | 10 walls and floors
//              11 sky | 12 THE PICTURES | 13 THE ARENA and the ray walk | 14 WHAT MOVES | 15 a foe's frame | 16 shots, motes
//              17 waves | 18 the player | 19 THE GHOST | 20 hands | 21 WHAT THE EYE SEES | 22 bands | 23 overlay | 24 THE FRAME
// LESSON-END
// HIGH NOON: a first-person shooter for the ESP32-C3 + 2.0" ST7789 (320 x 240), in bright midday. Robots come in waves across a walled
// arena of brick, steel, crates, a tech bunker and vine-hung ruins. Four guns. Barrels that go up in chains. Everything is whole numbers.
// The world is drawn a band at a time (16 rows): 320 rays a frame walk the grid by the carry rule (take the nearer of the next x-line and
// the next y-line, add its step), floors are laid per dot from a per-row start and step, the sky has its own clouds and mountains, and every
// wall, floor, robot, flame and gun is grown at boot from rules into 8-bit pictures read through shaded palettes (near and far, sun side and
// shade side, struck white), so distance and sun cost nothing per dot.
// Hands: stick forward/back moves, left/right turns. Hold C: left/right strafes. A fires (hold). B changes gun. Press the stick: dash.
// Nothing touched on the title: the ghost plays. Bench builds (FRAMES=n) let the ghost play.
// Wiring: SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3. Stick VRx 0, VRy 1, press 2. Buttons A 3, B 20, C 21 (to ground).
// Arduino IDE: "ESP32C3 Dev Module", core 3.x, USB CDC On Boot: Enabled. Serial Monitor 115200: frame times.
// The arena is made and checked by tools/make_map.py (the block between the MAP markers).
// ---- fixes (uncomment):
//#define RGB_ORDER
//#define NO_INVERT
//#define ROT180
//#define SAFE_SPI
//#define JOY_FLIP_X
//#define JOY_FLIP_Y
//#define JOY_SWAP
//#define GHOST_ONLY
// Bench builds only (set by the tools): DESK, MOUTH_FIFO, NULL_WIRE, FRAMES=n.
#define NO_FB
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifndef DESK
#include <Arduino.h>
#include "esp_cpu.h"
#include "esp_heap_caps.h"
#ifdef MOUTH_FIFO
#include <SPI.h>
#else
#include "driver/spi_master.h"
#include "driver/gpio.h"
#endif
#endif
#pragma GCC optimize("O2")
#ifndef FRAMES
#define FRAMES 0
#endif
#if !defined(DESK) && !defined(MOUTH_FIFO) && FRAMES == 0
#define ON_BOARD 1
#endif
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
#ifdef SAFE_SPI
#define WIRE_HZ 20000000
#else
#define WIRE_HZ 40000000
#endif
#define SW 320
#define SH 240
#define TH 16
#define NTY (SH / TH)
#define STRIP (SW * TH)

// LESSON-BEGIN 2 of 24: Types first (the IDE writes prototypes above the first function; CI 63)
// LESSON-END
// ---------------------------------------------------------------- the shapes of things (first, for the IDE)
typedef struct { int32_t x, y, vx, vy; int32_t a; int hp, gun, fire_cd, kick, dash_t, hurt_t, hurt_from, bob, moving, alive, inv; int ammo[4], has[4]; uint32_t score; int combo, combo_t, kills, shots, hits; } Player;
typedef struct { int32_t x, y, z, vx, vy; uint8_t kind, alive, state, id; int16_t hp, t, cd, flash, stag, side; } Foe;
typedef struct { int32_t x, y, z, vx, vy, vz; uint8_t kind, mine, life, pad; } Shot;
typedef struct { int32_t x, y, z, vx, vy, vz; uint8_t col, life, size, kind; } Mote;
typedef struct { int32_t x, y, z; uint8_t kind, t, life, big; } Fx;
typedef struct { int32_t x, y; uint8_t kind, alive, home; int16_t wait; } Pick;
typedef struct { int32_t depth, x0, x1, y0, y1; int32_t top16, left16, ustep, vstep; const uint8_t *tex; uint8_t tw, th, pal, pad; } Bill;
typedef struct { int32_t depth; int16_t sx, sy, sz; uint8_t col, pal; } Dot;
typedef struct { int32_t x16, y16; int32_t life; uint16_t col; char s[16]; } Floater;
#if defined(DESK)
#define STAGE(...) do {} while (0)
#elif FRAMES > 0
#define STAGE(...) Serial0.printf(__VA_ARGS__)
#else
#define STAGE(...) Serial.printf(__VA_ARGS__)
#endif

// LESSON-BEGIN 3 of 24: Three mouths, one stream
// WHAT    : DESK stream (decoded by tools/desk_panel.c), FIFO for the emulator, FAREY's DMA queue on the board. One source.
// LESSON-END
// ---------------------------------------------------------------- the mouths (DESK stream, FIFO for the emulator, FAREY's queue for the board)
#ifdef DESK
static void m_cmd(uint8_t c, const uint8_t *p, int n) { desk_cmd(c, p, n); }
static void m_data(const void *p, int nb) { desk_data((const uint8_t *)p, nb); }
static void m_fence(int s) { (void)s; }
static void m_wait(int s) { (void)s; }
static void m_wait_all(void) {}
static void m_begin(void) {}
static void hold_ms(int ms) { (void)ms; }
static uint32_t cyc(void) { return 0; }
#elif defined(MOUTH_FIFO)
static void m_cmd(uint8_t c, const uint8_t *p, int n) {
#ifndef NULL_WIRE
  digitalWrite(PIN_DC, LOW); SPI.write(c); digitalWrite(PIN_DC, HIGH); if (n) SPI.writeBytes(p, n);
#else
  (void)c; (void)p; (void)n;
#endif
}
static void m_data(const void *p, int nb) {
#ifndef NULL_WIRE
  SPI.writeBytes((const uint8_t *)p, nb);
#else
  (void)p; (void)nb;
#endif
}
static void m_fence(int s) { (void)s; }
static void m_wait(int s) { (void)s; }
static void m_wait_all(void) {}
static void m_begin(void) {
#ifndef NULL_WIRE
  pinMode(PIN_DC, OUTPUT); pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  SPI.begin(PIN_SCK, -1, PIN_MOSI, -1); SPI.beginTransaction(SPISettings(WIRE_HZ, MSBFIRST, SPI_MODE0)); digitalWrite(PIN_CS, LOW);
#endif
}
static void hold_ms(int ms) { delay(ms); }
static uint32_t cyc(void) { return esp_cpu_get_cycle_count(); }
#else
#define QN 64
static spi_device_handle_t dev;
static int m_e1, m_e2;
static spi_transaction_t TQ[QN];
static uint8_t PRM[QN][8];
static uint32_t q_in, q_done, q_mark[2];
static void IRAM_ATTR dc_cb(spi_transaction_t *t) { gpio_set_level((gpio_num_t)PIN_DC, (int)(intptr_t)t->user); }
static void q_reap(void) { spi_transaction_t *r; spi_device_get_trans_result(dev, &r, portMAX_DELAY); q_done++; }
static spi_transaction_t *q_slot(int *k) { if (q_in - q_done >= QN) q_reap(); *k = (int)(q_in % QN); spi_transaction_t *t = &TQ[*k]; memset(t, 0, sizeof *t); return t; }
static void q_go(spi_transaction_t *t) { spi_device_queue_trans(dev, t, portMAX_DELAY); q_in++; }
static void m_cmd(uint8_t c, const uint8_t *p, int n) {
  int k; spi_transaction_t *t = q_slot(&k); t->length = 8; t->flags = SPI_TRANS_USE_TXDATA; t->tx_data[0] = c; t->user = (void *)0; q_go(t);
  if (n) { t = q_slot(&k); t->length = 8 * n; t->user = (void *)1;
    if (n <= 4) { t->flags = SPI_TRANS_USE_TXDATA; memcpy(t->tx_data, p, n); } else { memcpy(PRM[k], p, n); t->tx_buffer = PRM[k]; }
    q_go(t); }
}
static void m_data(const void *p, int nb) { int k; spi_transaction_t *t = q_slot(&k); t->length = 8 * nb; t->tx_buffer = p; t->user = (void *)1; q_go(t); }
static void m_fence(int s) { q_mark[s] = q_in; }
static void m_wait(int s) { while ((int32_t)(q_done - q_mark[s]) < 0) q_reap(); }
static void m_wait_all(void) { while (q_done != q_in) q_reap(); }
static void m_begin(void) {
  gpio_set_direction((gpio_num_t)PIN_DC, GPIO_MODE_OUTPUT);
  spi_bus_config_t bus; memset(&bus, 0, sizeof bus);
  bus.mosi_io_num = PIN_MOSI; bus.miso_io_num = -1; bus.sclk_io_num = PIN_SCK; bus.quadwp_io_num = -1; bus.quadhd_io_num = -1;
  bus.max_transfer_sz = STRIP * 2 + 64;
  m_e1 = (int)spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
  spi_device_interface_config_t d; memset(&d, 0, sizeof d);
  d.clock_speed_hz = WIRE_HZ; d.mode = 0; d.spics_io_num = PIN_CS; d.queue_size = QN; d.pre_cb = dc_cb;
  m_e2 = (int)spi_bus_add_device(SPI2_HOST, &d, &dev);
}
static void hold_ms(int ms) { m_wait_all(); delay(ms); }
static uint32_t cyc(void) { return esp_cpu_get_cycle_count(); }
#endif

// LESSON-BEGIN 4 of 24: The panel
// WHAT    : ST7789 window and write. Flags at the top (RGB_ORDER, NO_INVERT, ROT180, SAFE_SPI, JOY_*) fix a panel or a stick
//           without touching a rule.
// LESSON-END
// ---------------------------------------------------------------- the panel
static uint32_t f_bytes;
static int32_t wx0 = -1, wx1 = -1, wy0 = -1, wy1 = -1;      // the window the panel holds: not said again
static void cmd(uint8_t c, const uint8_t *p, int n) { f_bytes += 1 + n; m_cmd(c, p, n); }
static void pix(const void *p, int nb) { f_bytes += nb; m_data(p, nb); }
static void window(int x0, int x1, int y0, int y1) {
  if (x0 != wx0 || x1 != wx1) { uint8_t a[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1}; cmd(0x2A, a, 4); wx0 = x0; wx1 = x1; }
  if (y0 != wy0 || y1 != wy1) { uint8_t b[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1}; cmd(0x2B, b, 4); wy0 = y0; wy1 = y1; }
  cmd(0x2C, 0, 0); }
static void panel_begin(void) {
#ifdef ON_BOARD
  pinMode(PIN_RST, OUTPUT); digitalWrite(PIN_RST, HIGH); delay(5); digitalWrite(PIN_RST, LOW); delay(20); digitalWrite(PIN_RST, HIGH); delay(150);
#endif
  m_begin();
  cmd(0x01, 0, 0); hold_ms(150);                             // SWRESET
  cmd(0x11, 0, 0); hold_ms(120);                             // SLPOUT
  { uint8_t v = 0x55; cmd(0x3A, &v, 1); }                    // 16 bits a dot
  { uint8_t m = 0x60;                                        // landscape, 320 across
#ifdef ROT180
    m = 0xA0;
#endif
#ifndef RGB_ORDER
    m |= 0x08;                                               // BGR, as this panel is
#endif
    cmd(0x36, &m, 1); }
#ifdef NO_INVERT
  cmd(0x20, 0, 0);
#else
  cmd(0x21, 0, 0);
#endif
  cmd(0x13, 0, 0); cmd(0x29, 0, 0);
  wx0 = wx1 = wy0 = wy1 = -1; }
static uint16_t glass(int32_t r, int32_t g, int32_t b) {    // the mouth for colour: 8-bit channels to 565, in wire order
  r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
  uint16_t v = (uint16_t)(((r * 249 + 1014) >> 11) << 11 | ((g * 253 + 505) >> 10) << 5 | ((b * 249 + 1014) >> 11));
  return (uint16_t)((v >> 8) | (v << 8)); }

// LESSON-BEGIN 5 of 24: Whole-number tools: mouth, mouth64, scatter
// TRAP    : the README records 75 of 2,000 frames drawn differently on chip and desk: two scatter() draws were arguments of
//           one call, and x86 and RISC-V evaluate arguments in different orders. Draw into named values first.
// LESSON-END
// ---------------------------------------------------------------- small whole-number tools
static int32_t mouth(int32_t top, int32_t den) {          // the only landing: nearest whole, half to even, symmetric about zero; den > 0
  int32_t q = top / den, r = top - q * den, a = r < 0 ? -r : r;
  if (2 * a > den || (2 * a == den && (q & 1))) q += top < 0 ? -1 : 1;
  return q; }
static int64_t mouth64(int64_t top, int64_t den) {
  int64_t q = top / den, r = top - q * den, a = r < 0 ? -r : r;
  if (2 * a > den || (2 * a == den && (q & 1))) q += top < 0 ? -1 : 1;
  return q; }
static uint32_t xs = 0x5EED;
static uint32_t scatter(void) { uint32_t x = xs; x ^= x << 7; x ^= x >> 9; x ^= x << 8; xs = x; return x; }
static uint32_t name_of(const char *s) { uint32_t h = 0x811c9dc5u; while (*s) { h = (h ^ (uint8_t)*s++) * 0x01000193u; } return h; }
static int32_t clampi(int32_t v, int32_t a, int32_t b) { return v < a ? a : v > b ? b : v; }




static int32_t mixc(int32_t a, int32_t b, int32_t k, int32_t n) { return a + mouth((b - a) * k, n); }

// LESSON-BEGIN 6 of 24: The face: 5 x 7 letters grown at boot
// LESSON-END
// ---------------------------------------------------------------- the face: 5x7 letters (ACE's)
static const char FACE_CH[] = "0123456789ABCDEFGHIKLMNOPRSTUVWXY:-!.,/%=+()'?JQZ><*";
static const uint8_t FACE[52][7] = {
  {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31}, {31,2,4,2,1,17,14}, {2,6,10,18,31,2,2},
  {31,16,30,1,1,17,14}, {6,8,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14}, {14,17,17,15,1,2,12},
  {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31},
  {31,16,16,30,16,16,16}, {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14}, {17,18,20,24,20,18,17},
  {16,16,16,16,16,16,31}, {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
  {30,17,17,30,20,18,17}, {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14}, {17,17,17,17,10,10,4},
  {17,17,17,21,21,27,17}, {17,17,10,4,10,17,17}, {17,17,10,4,4,4,4}, {0,12,12,0,12,12,0}, {0,0,0,31,0,0,0}, {4,4,4,4,4,0,4},
  {0,0,0,0,0,12,12}, {0,0,0,0,12,4,8}, {1,1,2,4,8,16,16}, {24,25,2,4,8,19,3}, {0,0,31,0,31,0,0}, {0,4,4,31,4,4,0}, {2,4,8,8,8,4,2},
  {8,4,2,2,2,4,8}, {4,4,8,0,0,0,0}, {14,17,1,2,4,0,4}, {7,2,2,2,2,18,12}, {14,17,17,17,21,18,13}, {31,1,2,4,8,16,31}, {8,4,2,1,2,4,8}, {2,4,8,16,8,4,2}, {0,4,21,14,21,4,0}};
static uint8_t GLYPH[128];
static void grow_face(void) { for (int i = 0; FACE_CH[i]; i++) GLYPH[(uint8_t)FACE_CH[i]] = (uint8_t)(i + 1); }

// LESSON-BEGIN 7 of 24: The ring: 256 headings from one Pythagorean turn, on 16384
// WHAT    : every turn in the game (look, bob, spreads, pulsing lights) reads this table. No sine is computed anywhere.
// LESSON-END
// ---------------------------------------------------------------- the ring: 256 headings grown by one Pythagorean turn (TUFT's), on 16384
static int32_t RC[257], RS[257];
static void grow_ring(void) {
  int32_t c = 16384, s = 0;
  for (int k = 0; k <= 64; k++) { RC[k] = c; RS[k] = s; int32_t c2 = mouth(c * 26565 - s * 652, 26573), s2 = mouth(s * 26565 + c * 652, 26573); c = c2; s = s2; }
  RC[64] = 0; RS[64] = 16384;
  for (int k = 0; k < 64; k++) { RC[64 + k] = -RS[k]; RS[64 + k] = RC[k]; RC[128 + k] = -RC[k]; RS[128 + k] = -RS[k]; RC[192 + k] = RS[k]; RS[192 + k] = -RC[k]; }
  RC[256] = RC[0]; RS[256] = RS[0]; }
static void unit16(int32_t h16, int32_t *c, int32_t *s) { uint32_t h = (uint32_t)h16 & 4095, i = h >> 4, f = h & 15;
  *c = RC[i] + (((RC[i + 1] - RC[i]) * (int32_t)f + 8) >> 4); *s = RS[i] + (((RS[i + 1] - RS[i]) * (int32_t)f + 8) >> 4); }



#ifdef DESK
#define HEAP_DMA(n) malloc(n)
#define FAST
#else
#define HEAP_DMA(n) heap_caps_malloc((n), MALLOC_CAP_DMA)
#define FAST IRAM_ATTR
#endif
static int32_t isqrt32(int32_t v) { if (v <= 0) return 0; int32_t r = 0, b = 1 << 30; while (b > v) b >>= 2;   // the root by digits, whole
  while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; } return r; }
static int32_t absi(int32_t v) { return v < 0 ? -v : v; }

// LESSON-BEGIN 8 of 24: Materials: a dot is a ramp and a shade, read through palettes
// WHAT    : every picture is 8-bit: ramp * 16 + shade, 0 = nothing. 17 palettes: sun side and shade side at eight distances
//           fading toward haze, plus struck white. Distance and sun cost one table read per dot.
// RELATION: how a dot looks is a relation (its ramp, the distance band, the side the sun is on), held as an index, landed
//           once into colour at the palette. The picture never stores colour.
// LESSON-END
// ================================================================ WHAT THINGS ARE MADE OF: sixteen ramps of sixteen shades, read through shaded palettes
// A dot is a ramp and a shade (ramp * 16 + shade); 0 is "nothing here". The palettes say how it looks: 0..7 sun side near to far (the far ones
// drift toward the haze of a clear day), 8..15 the shade side, 16 struck (white-hot for a frame).
enum { R_STONE, R_BRICK, R_SAND, R_WOOD, R_GRASS, R_STEEL, R_TEAL, R_GOLD, R_FIRE, R_RED, R_PURPLE, R_CYAN, R_OLIVE, R_DARK, R_WHITE, R_SKY };
static const uint8_t RAMP[16][3] = {{158, 156, 150}, {184, 78, 54}, {226, 200, 142}, {156, 102, 54}, {98, 176, 62}, {112, 144, 188}, {36, 172, 168}, {246, 200, 40},
  {255, 132, 20}, {214, 38, 38}, {152, 72, 206}, {70, 224, 255}, {150, 148, 82}, {72, 74, 88}, {244, 244, 240}, {150, 200, 255}};
static uint8_t BR[256], BGc[256], BB[256];
#define NPAL 17
#define PAL_STRUCK 16
static uint16_t PAL[NPAL][256];
static uint8_t ix(int r, int s) { return (uint8_t)(r * 16 + (s < 1 ? 1 : s > 15 ? 15 : s)); }
static void grow_colours(void) {
  for (int r = 0; r < 16; r++) for (int s = 0; s < 16; s++) { int k = r * 16 + s; int32_t c[3];
    for (int j = 0; j < 3; j++) { int32_t b = RAMP[r][j];
      c[j] = s <= 10 ? mouth(b * (22 + 78 * s / 10), 100) : b + mouth((255 - b) * (s - 10) * 15, 100); }
    BR[k] = (uint8_t)clampi(c[0], 0, 255); BGc[k] = (uint8_t)clampi(c[1], 0, 255); BB[k] = (uint8_t)clampi(c[2], 0, 255); }
  for (int p = 0; p < NPAL; p++) for (int k = 0; k < 256; k++) {
    int32_t r = BR[k], g = BGc[k], b = BB[k];
    if (p < 16) { if (p >= 8) { r = r * 196 >> 8; g = g * 196 >> 8; b = b * 204 >> 8; }
      int32_t h = (p & 7) * 22; r += mouth((198 - r) * h, 256); g += mouth((222 - g) * h, 256); b += mouth((246 - b) * h, 256); }
    else { r += (255 - r) * 170 >> 8; g += (255 - g) * 170 >> 8; b += (255 - b) * 150 >> 8; }
    PAL[p][k] = glass(r, g, b); } }
static void breathe_cyan(uint32_t n) {                                 // the tech lights and plasma pulse: the cyan ramp's top shades turned with a heading
  int32_t k = 200 + (RS[(n * 5) & 255] >> 8);                         // 136..264
  for (int s = 11; s < 16; s++) { int idx = R_CYAN * 16 + s; for (int p = 0; p < NPAL; p++) {
      int32_t r = BR[idx] * k >> 8, g = BGc[idx] * k >> 8, b = BB[idx] * k >> 8;
      if (p < 16) { if (p >= 8) { r = r * 196 >> 8; g = g * 196 >> 8; b = b * 204 >> 8; } int32_t h = (p & 7) * 22; r += mouth((198 - r) * h, 256); g += mouth((222 - g) * h, 256); b += mouth((246 - b) * h, 256); }
      else { r += (255 - r) * 170 >> 8; g += (255 - g) * 170 >> 8; b += (255 - b) * 150 >> 8; }
      PAL[p][idx] = glass(r, g, b); } } }
static int haze_of(int32_t d16) { return (int)clampi((d16 - 5 * 65536) / (3 * 65536), 0, 7); }   // a clear day: nothing hazes nearer than 5 squares

// LESSON-BEGIN 9 of 24: Noise: a hash on the lattice, read between its points
// WHAT    : hsh, vnoise, fbm64: whole-number value noise. A texture is a rule of place, the same every boot.
// LESSON-END
// ---------------------------------------------------------------- noise: a hash on the lattice, and the lattice read between its points
static uint32_t hsh(int32_t x, int32_t y, uint32_t seed) { uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }
static int32_t vnoise(int32_t x, int32_t y, int cell, int px, int py, uint32_t seed) {   // 0..255, wrapping every px, py lattice points
  int32_t ix0 = x / cell, iy0 = y / cell, fx = x % cell, fy = y % cell;
  int32_t a = hsh(ix0 % px, iy0 % py, seed) & 255, b = hsh((ix0 + 1) % px, iy0 % py, seed) & 255, c = hsh(ix0 % px, (iy0 + 1) % py, seed) & 255, d = hsh((ix0 + 1) % px, (iy0 + 1) % py, seed) & 255;
  int32_t top = a * (cell - fx) + b * fx, bot = c * (cell - fx) + d * fx; return (top * (cell - fy) + bot * fy) / (cell * cell); }
static int32_t fbm64(int32_t x, int32_t y, uint32_t seed) {          // three octaves that tile a 64 x 64 picture
  return (vnoise(x, y, 16, 4, 4, seed) * 4 + vnoise(x, y, 8, 8, 8, seed + 1) * 2 + vnoise(x, y, 4, 16, 16, seed + 2)) / 7; }

// LESSON-BEGIN 10 of 24: Walls and floors: 64 x 64, grown by rules
// WHAT    : stone, brick, steel, crates, tech, vines: each a short rule (blocks staggered, bevels, mortar). Stored a column at a
//           time because the ray reads a wall a column at a time: the storage follows the reader.
// LESSON-END
// ---------------------------------------------------------------- the walls and floors: 64 x 64, grown by rules (walls stored a column at a time)
#define NWALL 7
static uint8_t TEX[NWALL][64 * 64];      // [id][u * 64 + v]
static uint8_t FTX[3][64 * 64];          // [id][v * 64 + u]
static void wt(int id, int u, int v, uint8_t c) { TEX[id][u * 64 + v] = c; }
static void grow_walls(void) {
  for (int u = 0; u < 64; u++) for (int v = 0; v < 64; v++) {
    int n = fbm64(u, v, 11), n2 = (int)(hsh(u, v, 5) & 15);
    // 1 stone: big blocks, staggered, bevelled
    { int row = v / 16, off = (row & 1) * 16, bu = (u + off) % 32, bv = v % 16, id = (u + off) / 32 + row * 7;
      int s = 9 + (int)(hsh(id, 0, 21) % 3) - 1 + (n - 128) / 36 + (n2 > 12);
      if (bv < 2 || bu < 2) s = 4 + (n2 & 1); else if (bv == 2 || bu == 2) s += 3; else if (bv == 15 || bu == 31) s -= 2;
      wt(1, u, v, ix(R_STONE, s)); }
    // 2 brick: small bricks in sand mortar
    { int row = v / 8, off = (row & 1) * 8, bu = (u + off) % 16, bv = v % 8, id = (u + off) / 16 + row * 5;
      if (bv == 0 || bu == 0) wt(2, u, v, ix(R_SAND, 7 + (n2 > 10)));
      else { int s = 8 + (int)(hsh(id, 1, 31) % 3) - 1 + (n - 128) / 40 + (bv == 1) * 2 - (bv == 7); wt(2, u, v, ix(R_BRICK, s)); } }
    // 3 steel: riveted panels with a hazard band
    { int pu = u % 32, pv = v % 32, s = 8 + (int)(hsh(u, v / 6, 41) % 3) - 1 + (pv < 16 ? 1 : 0); uint8_t c = ix(R_STEEL, s);
      if (pu == 0 || pv == 0) c = ix(R_DARK, 4); else if (pu == 1 || pv == 1) c = ix(R_STEEL, 12);
      for (int k = 0; k < 4; k++) { int rx = (k & 1) ? 27 : 4, ry = (k & 2) ? 27 : 4, dx = pu - rx, dy = pv - ry; if (dx * dx + dy * dy <= 3) c = ix(R_STEEL, dx + dy < 0 ? 14 : 6); }
      if (v >= 52) c = ((u + v) / 6) & 1 ? ix(R_GOLD, 10) : ix(R_DARK, 3);
      if (v == 51) c = ix(R_DARK, 2);
      wt(3, u, v, c); }
    // 4 crate: planks in a frame, braced
    { int s = 9 + (int)(hsh(u / 9, 2, 51) % 3) - 1 + (vnoise(u * 8, v, 16, 64, 4, 52) - 128) / 32; uint8_t c = ix(R_WOOD, s);
      if (u % 9 == 0) c = ix(R_WOOD, 4);
      int d1 = u - v, d2 = u + v - 63; if ((d1 > -5 && d1 < 5) || (d2 > -5 && d2 < 5)) c = ix(R_WOOD, (d1 == -4 || d1 == 4 || d2 == -4 || d2 == 4) ? 5 : 11);
      if (u < 6 || u > 57 || v < 6 || v > 57) { c = ix(R_WOOD, 7 + (u < 2 || v < 2) * 3 - (u > 61 || v > 61) * 2); if ((u == 3 || u == 60) && (v == 3 || v == 60)) c = ix(R_STEEL, 13); }
      wt(4, u, v, c); }
    // 5 tech: teal panels, a dark frame, two light strips, vents
    { uint8_t c = ix(R_TEAL, 7 + (n - 128) / 50);
      if (u < 3 || u > 60 || v < 3 || v > 60 || (v > 30 && v < 34)) c = ix(R_DARK, 5 + (u == 1 || v == 1));
      if (v >= 13 && v <= 16) c = (v == 13 || v == 16) ? ix(R_CYAN, 11) : ix(R_CYAN, 15);
      if (v >= 45 && v <= 47 && u > 8 && u < 56) c = v == 46 ? ix(R_CYAN, 15) : ix(R_CYAN, 12);
      if (v >= 37 && v <= 43 && u >= 18 && u <= 46 && (v & 1)) c = ix(R_DARK, 3);
      if (u >= 6 && u <= 9 && v >= 20 && v <= 27) c = ix(R_RED, 13);
      wt(5, u, v, c); }
    // 6 ruins: sandstone hung with vines
    { int row = v / 16, off = (row & 1) * 16, bu = (u + off) % 32, bv = v % 16; int s = 9 + (n - 128) / 30;
      uint8_t c = (bv < 2 || bu < 2) ? ix(R_SAND, 5) : ix(R_SAND, s + (bv == 2) * 2);
      static const int ST[4] = {8, 23, 40, 54}, LN[4] = {44, 60, 30, 52};
      for (int k = 0; k < 4; k++) { int w = ST[k] + (vnoise(v * 4, k * 16, 16, 16, 4, 61) - 128) / 22; int dx = u - w;
        if (v < LN[k] && dx >= -1 && dx <= 1) c = ix(R_GRASS, 6 + dx * 2 + (int)(hsh(u, v, 62) & 1));
        if (v < LN[k] - 2 && ((v + k * 5) % 7) < 3 && dx >= -4 && dx <= 4 && (hsh(u, v, 63) & 3)) c = ix(R_GRASS, 9 + (dx < 0 ? 2 : -1)); }
      wt(6, u, v, c); } }
  // floors
  for (int v = 0; v < 64; v++) for (int u = 0; u < 64; u++) {
    int n = fbm64(u, v, 71); uint32_t h = hsh(u, v, 72);
    { int s = 9 + (n - 128) / 32; if (((v * 2 + n / 12) & 15) < 2) s -= 1; if ((h & 63) == 0) s -= 2; FTX[0][v * 64 + u] = ix(R_SAND, s); }
    { int s = 7 + (n - 128) / 28 + (int)(h & 3) - 1; uint8_t c = ix(R_GRASS, s); if (h % 157 == 0) c = ix(R_WHITE, 14); if (h % 211 == 0) c = ix(R_GOLD, 13); FTX[1][v * 64 + u] = c; }
    { int tu = u % 32, tv = v % 32, id = u / 32 + (v / 32) * 2; int s = 10 + (int)(hsh(id, 3, 73) % 3) - 1 + (n - 128) / 48;
      uint8_t c = ix(R_STONE, s); if (tu < 1 || tv < 1) c = ix(R_STONE, 5); else if (tu == 1 || tv == 1) c = ix(R_STONE, 12); FTX[2][v * 64 + u] = c; } } }

// LESSON-BEGIN 11 of 24: The sky: a gradient, drifting clouds, two ridges
// WHAT    : a 256 x 64 density map read with a drift; mountains at the rim keyed to the look. The sky is not a ray cast.
// LESSON-END
// ---------------------------------------------------------------- the sky: a gradient, clouds that drift, two rows of mountains at the rim
#define SKY_E 140
static uint16_t SKYROW[SKY_E][16];
static uint8_t CLOUD[64][256];
static uint8_t MNT1[1024], MNT2[1024];
static uint16_t MCOL[40], HCOL[24], SNOW;
static void grow_sky(void) {
  for (int e = 0; e < SKY_E; e++) { int t = e > 130 ? 130 : e;
    int32_t r = 206 + (66 - 206) * t / 130, g = 230 + (132 - 230) * t / 130, b = 252 + (236 - 252) * t / 130;
    for (int d = 0; d < 16; d++) { int32_t cw = 250 - (15 - d) * 3, k = d * 236 / 15; SKYROW[e][d] = glass(r + (cw - r) * k / 256, g + (cw - g) * k / 256, b + (cw + 2 - b) * k / 256); } }
  for (int v = 0; v < 64; v++) for (int u = 0; u < 256; u++) {
    int32_t n = (vnoise(u * 2, v * 4, 32, 16, 64, 81) * 4 + vnoise(u * 2, v * 4, 16, 32, 64, 82) * 2 + vnoise(u * 2, v * 4, 8, 64, 64, 83)) / 7;
    int32_t fade = v < 8 ? v * 32 : 256; CLOUD[v][u] = (uint8_t)clampi(((n - 132) * fade / 256) / 5, 0, 15); }
  for (int i = 0; i < 1024; i++) {
    int32_t n1 = (vnoise(i, 0, 64, 16, 1, 91) * 3 + vnoise(i, 0, 16, 64, 1, 92)) / 4, n2 = (vnoise(i, 0, 32, 32, 1, 93) * 3 + vnoise(i, 0, 8, 128, 1, 94)) / 4;
    MNT1[i] = (uint8_t)clampi(6 + (n1 - 60) * 30 / 160, 3, 34); MNT2[i] = (uint8_t)clampi(1 + (n2 - 90) * 14 / 120, 0, 14); }
  for (int e = 0; e < 40; e++) MCOL[e] = glass(140 + e, 164 + e, 200 + e / 2);
  for (int e = 0; e < 24; e++) HCOL[e] = glass(96 + e * 2, 140 + e * 2, 118 + e);
  SNOW = glass(236, 242, 250); }

// LESSON-BEGIN 12 of 24: The pictures: robots, flames, pickups, guns, drawn by rules into one pool
// WHAT    : a lit dome per shape: the slope of an ellipse at each dot against a sun up and to the left (a dot product and one
//           root, whole numbers). Outlined dark so they read at a glance.
// BUDGET  : one pool of 112,000 bytes; the README records 104,972 used. The uploaded spr_new returned -1 past the pool without
//           counting the slot; callers ignored -1, so every later id shifted and ospr read past the pool. Fixed here (FIXES.md F1):
//           a refused picture is an empty one, ids stay in place, sp_refused is printed at boot.
// TRY     : set SPPOOL 60000 and build the desk twin with -fsanitize=address, once from original/ and once from this file.
//           Predict: original overflows in ospr; this file runs and the boot line counts the refusals.
// LESSON-END
// ================================================================ THE PICTURES: robots, flames, pickups, shots and guns, drawn by rules into one pool
typedef struct { uint8_t w, h; uint32_t off; } Spr;
#define SPPOOL 112000
static uint8_t SPX[SPPOOL];
static Spr SP[48]; static int sp_n; static uint32_t sp_used;
static uint8_t *cv; static int cw, ch;                                // the canvas being drawn
static int sp_refused;                                               /* pictures refused for want of pool: drawn as nothing, counted at boot */
static int spr_new(int w, int h) { Spr *s = &SP[sp_n]; s->w = (uint8_t)w; s->h = (uint8_t)h; s->off = sp_used; sp_used += (uint32_t)(w * h);
  if (sp_used > SPPOOL) { sp_used -= (uint32_t)(w * h); s->w = 0; s->h = 0; s->off = 0; cw = 0; ch = 0; sp_refused++; return sp_n++; }   /* an empty picture keeps every later id in place */ cv = SPX + s->off; cw = w; ch = h; memset(cv, 0, (size_t)(w * h)); return sp_n++; }
static void cput(int x, int y, uint8_t c) { if (x >= 0 && y >= 0 && x < cw && y < ch) cv[y * cw + x] = c; }
static uint8_t cget(int x, int y) { return (x >= 0 && y >= 0 && x < cw && y < ch) ? cv[y * cw + x] : 0; }
// a lit dome: the slope of an ellipse at each dot, against a sun up and to the left (dot product, whole numbers, one root)
static void blob(int cx, int cy, int rx, int ry, int ramp, int lift) {
  for (int y = cy - ry; y <= cy + ry; y++) for (int x = cx - rx; x <= cx + rx; x++) {
    int32_t nx = (x - cx) * 256 / rx, ny = (y - cy) * 256 / ry, q = nx * nx + ny * ny; if (q > 65536) continue;
    int32_t nz = isqrt32(65536 - q), lam = (-110 * nx - 130 * ny + 180 * nz) / 256;   // the sun's share, about -256..256
    cput(x, y, ix(ramp, 3 + (int)clampi((lam + 60) * 12 / 300, 0, 12) + lift)); } }
static void capsule(int x0, int y0, int x1, int y1, int r, int ramp, int lift) {   // a limb: a lit tube between two points
  int32_t vx = x1 - x0, vy = y1 - y0, vv = vx * vx + vy * vy; if (vv == 0) vv = 1;
  int xa = (x0 < x1 ? x0 : x1) - r, xb = (x0 > x1 ? x0 : x1) + r, ya = (y0 < y1 ? y0 : y1) - r, yb = (y0 > y1 ? y0 : y1) + r;
  for (int y = ya; y <= yb; y++) for (int x = xa; x <= xb; x++) {
    int32_t wx = x - x0, wy = y - y0, t = clampi((wx * vx + wy * vy) * 256 / vv, 0, 256), qx = x0 * 256 + vx * t - x * 256, qy = y0 * 256 + vy * t - y * 256;
    int32_t d2 = (qx * qx + qy * qy) / 256; if (d2 > r * r * 256) continue;
    int32_t across = qx / (r > 0 ? r : 1);                       // -256..256 across the tube (sun from the left: the right side is in shade)
    int s = 10 + (int)(across / 64) - (int)(d2 * 4 / (r * r * 256 + 1)); cput(x, y, ix(ramp, s + lift)); } }
static void boxs(int x0, int y0, int x1, int y1, int ramp, int s) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) cput(x, y, ix(ramp, s + (x == x0) * 2 - (x == x1) * 2 + (y == y0) * 2 - (y == y1))); }
static void disc(int cx, int cy, int r, int ramp, int s) { for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) if (x * x + y * y <= r * r) cput(cx + x, cy + y, ix(ramp, s)); }
static void glow(int cx, int cy, int r, int ramp) { for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) { int d2 = x * x + y * y; if (d2 <= r * r) cput(cx + x, cy + y, ix(d2 * 4 < r * r ? R_WHITE : ramp, 15 - d2 * 5 / (r * r + 1))); } }
static void outline(void) {                                          // a dark rim where a picture meets nothing: things read at a glance
  static uint8_t tmp[144 * 100]; memcpy(tmp, cv, (size_t)(cw * ch));
  for (int y = 0; y < ch; y++) for (int x = 0; x < cw; x++) if (!tmp[y * cw + x]) {
    int n = (x > 0 && tmp[y * cw + x - 1]) || (x < cw - 1 && tmp[y * cw + x + 1]) || (y > 0 && tmp[(y - 1) * cw + x]) || (y < ch - 1 && tmp[(y + 1) * cw + x]);
    if (n) cv[y * cw + x] = ix(R_DARK, 2); } }
// the robots
enum { SPR_GRUNT = 0, SPR_DRONE = 3, SPR_BRUTE = 5, SPR_TITAN = 8, SPR_BARREL = 11, SPR_BOOM = 12, SPR_SCRAP = 18, SPR_MED = 19, SPR_AMMO = 20, SPR_GUN1 = 21,
       SPR_BOLT = 24, SPR_FIREB = 25, SPR_ROCKET = 26, SPR_PLASMA = 27, SPR_VIEW = 28 };
static void draw_grunt(int f) {                                       // 0, 1 walking; 2 firing
  spr_new(48, 48); int sw = f == 0 ? 4 : f == 1 ? -4 : 0;
  capsule(20, 30, 18 + sw, 45, 3, R_DARK, 1); capsule(28, 30, 30 - sw, 45, 3, R_DARK, 1); boxs(14 + sw, 44, 22 + sw, 47, R_DARK, 6); boxs(26 - sw, 44, 34 - sw, 47, R_DARK, 6);
  blob(24, 31, 8, 4, R_DARK, 0);
  capsule(15, 19, 11, 31, 3, R_STEEL, -1);
  blob(24, 23, 11, 9, R_RED, 0); boxs(19, 19, 29, 21, R_GOLD, 10);
  blob(24, 10, 7, 7, R_STEEL, 1); boxs(18, 8, 30, 11, R_CYAN, 13); for (int x = 19; x < 30; x++) cput(x, 9, ix(R_WHITE, 15));
  if (f < 2) { capsule(33, 19, 38, 30, 3, R_STEEL, -1); boxs(35, 27, 46, 31, R_DARK, 5); cput(46, 29, ix(R_FIRE, 14)); }
  else { capsule(33, 19, 27, 27, 3, R_STEEL, 0); disc(25, 28, 5, R_DARK, 4); glow(25, 28, 3, R_FIRE); }
  outline(); }
static void draw_drone(int f) {
  spr_new(40, 40); int w = f ? 7 : 4;
  boxs(3, 9, 37, 11, R_DARK, 5); for (int k = 0; k < 2; k++) { int cx = k ? 34 : 6; for (int x = -w; x <= w; x++) cput(cx + x, 7, ix(R_WHITE, 11 - (x * x) / 8)); }
  blob(20, 20, 13, 12, R_PURPLE, 0); blob(20, 13, 8, 4, R_PURPLE, 2);
  disc(20, 21, 6, R_DARK, 3); glow(20, 21, 4, R_RED); glow(20, 35, 3 + f, R_FIRE); outline(); }
static void draw_brute(int f) {
  spr_new(56, 56); int sw = f == 0 ? 3 : f == 1 ? -3 : 0;
  capsule(21, 36, 18 + sw, 53, 5, R_DARK, 1); capsule(35, 36, 38 - sw, 53, 5, R_DARK, 1); boxs(11 + sw, 51, 25 + sw, 55, R_DARK, 6); boxs(31 - sw, 51, 45 - sw, 55, R_DARK, 6);
  blob(28, 29, 18, 14, R_OLIVE, 0); boxs(17, 33, 39, 36, R_DARK, 6); blob(28, 25, 9, 6, R_OLIVE, 2);
  blob(9, 18, 8, 7, R_DARK, 2); blob(47, 18, 8, 7, R_DARK, 2);
  boxs(4, 12, 9, 22, R_STEEL, 7); boxs(47, 12, 52, 22, R_STEEL, 7);
  if (f == 2) { glow(6, 14, 4, R_FIRE); glow(50, 14, 4, R_FIRE); }
  blob(28, 12, 7, 6, R_DARK, 1); boxs(23, 11, 33, 12, R_RED, 15); outline(); }
static void draw_titan(int f) {
  spr_new(64, 64); int sw = f == 0 ? 3 : f == 1 ? -3 : 0;
  capsule(24, 42, 20 + sw, 61, 6, R_DARK, 1); capsule(40, 42, 44 - sw, 61, 6, R_DARK, 1); boxs(12 + sw, 59, 28 + sw, 63, R_GOLD, 7); boxs(36 - sw, 59, 52 - sw, 63, R_GOLD, 7);
  blob(32, 33, 20, 15, R_GOLD, 0); blob(32, 31, 7, 7, R_DARK, 2); glow(32, 31, 5, R_CYAN);
  capsule(12, 22, 6, 40, 5, R_DARK, 2); capsule(52, 22, 58, 40, 5, R_DARK, 2); boxs(1, 38, 11, 46, R_GOLD, 8); boxs(53, 38, 63, 46, R_GOLD, 8);
  blob(32, 13, 9, 8, R_GOLD, 1); boxs(25, 12, 39, 14, R_RED, 15);
  for (int k = 0; k < 6; k++) { cput(24 - k / 2, 6 - k, ix(R_GOLD, 13)); cput(40 + k / 2, 6 - k, ix(R_GOLD, 13)); }
  if (f == 2) { glow(6, 46, 5, R_FIRE); glow(58, 46, 5, R_FIRE); }
  outline(); }
static void draw_barrel(void) {
  spr_new(32, 40);
  for (int y = 4; y < 40; y++) for (int x = 4; x < 28; x++) { int32_t nx = (x - 16) * 256 / 12; int s = 10 - (int)(nx / 48) - (nx * nx) / 20000; cput(x, y, ix(R_RED, s)); }
  for (int k = 0; k < 3; k++) { int y = 6 + k * 14; boxs(4, y, 27, y + 2, R_DARK, 6); }
  for (int y = 0; y < 10; y++) for (int x = -y / 2; x <= y / 2; x++) cput(16 + x, 15 + y, ix(R_GOLD, 13 - (y == 9) * 3));
  cput(16, 18, ix(R_DARK, 2)); cput(16, 19, ix(R_DARK, 2)); cput(16, 22, ix(R_DARK, 2));
  blob(16, 4, 12, 3, R_RED, 1); outline(); }
static void draw_boom(int f) {                                        // flame that grows, cools and turns to smoke
  spr_new(48, 48); int32_t r = 9 + f * 3;
  for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) { int32_t dx = x - 24, dy = y - 24, d = isqrt32(dx * dx + dy * dy); if (d > r) continue;
    int32_t heat = (r - d) * 256 / r + (int32_t)(vnoise(x * 4, y * 4, 16, 16, 16, 100 + f) - 128) - f * 34 - (dy < 0 ? 0 : dy * 2);
    if (heat > 210) cput(x, y, ix(R_WHITE, 15)); else if (heat > 160) cput(x, y, ix(R_GOLD, 14)); else if (heat > 110) cput(x, y, ix(R_FIRE, 12));
    else if (heat > 60) cput(x, y, ix(R_RED, 10)); else if (heat > 10) cput(x, y, ix(R_DARK, 4 + (int)(heat / 20))); } }
static void draw_scrap(void) { spr_new(40, 24);
  for (int k = 0; k < 14; k++) { uint32_t a = hsh(k, 0, 120), b = hsh(k, 1, 120); int x = 4 + (int)(a % 30), y = 10 + (int)(b % 12), w = 3 + (int)(a >> 8) % 6, h = 2 + (int)(b >> 8) % 4;
    static const int RR[4] = {R_DARK, R_STEEL, R_RED, R_DARK}; boxs(x, y, x + w, y + h > 23 ? 23 : y + h, RR[(a >> 16) & 3], 7); }
  glow(20, 18, 2, R_FIRE); outline(); }
static void draw_pick(int k) {
  if (k == 0) { spr_new(28, 24); boxs(2, 6, 25, 23, R_WHITE, 11); boxs(11, 9, 16, 20, R_RED, 11); boxs(7, 12, 20, 17, R_RED, 11); boxs(10, 3, 17, 6, R_STEEL, 9); }
  else if (k == 1) { spr_new(28, 24); boxs(2, 8, 25, 23, R_OLIVE, 8); for (int i = 0; i < 4; i++) { boxs(4 + i * 6, 2, 7 + i * 6, 8, R_GOLD, 11); cput(5 + i * 6, 2, ix(R_WHITE, 15)); } boxs(2, 13, 25, 15, R_DARK, 5); }
  else { spr_new(40, 20); int g = k - 2;
    if (g == 0) { boxs(2, 8, 30, 11, R_DARK, 6); boxs(2, 12, 30, 14, R_DARK, 5); boxs(26, 9, 38, 17, R_WOOD, 9); boxs(16, 13, 24, 16, R_WOOD, 10); }
    else if (g == 1) { capsule(4, 10, 34, 10, 5, R_GRASS, -2); boxs(30, 4, 34, 16, R_GOLD, 10); disc(4, 10, 4, R_DARK, 2); boxs(18, 14, 22, 19, R_DARK, 5); }
    else { blob(20, 10, 16, 6, R_TEAL, 0); for (int i = 0; i < 4; i++) glow(8 + i * 7, 10, 2, R_CYAN); boxs(30, 12, 36, 18, R_DARK, 5); } }
  outline(); }
static void draw_shot(int k) {
  if (k == 0) { spr_new(12, 12); glow(6, 6, 5, R_RED); }
  else if (k == 1) { spr_new(20, 20); for (int y = 0; y < 20; y++) for (int x = 0; x < 20; x++) { int dx = x - 10, dy = y - 10, d2 = dx * dx + dy * dy; if (d2 > 81) continue;
      int heat = (81 - d2) * 3 + (int)(hsh(x, y, 130) & 63); cput(x, y, heat > 220 ? ix(R_WHITE, 15) : heat > 140 ? ix(R_GOLD, 14) : ix(R_FIRE, 11)); } }
  else if (k == 2) { spr_new(16, 16); disc(8, 8, 4, R_STEEL, 9); disc(8, 8, 2, R_DARK, 3); glow(8, 8, 1, R_FIRE); }
  else { spr_new(14, 14); glow(7, 7, 6, R_CYAN); } }
// the guns, as you hold them: long shapes running up from the bottom edge toward the middle of the screen
typedef struct { int w, h, mx, my; const char *name; } GunLook;
static const GunLook GL_[4] = {{140, 92, 62, 8, "PULSE RIFLE"}, {140, 92, 66, 10, "SCATTERGUN"}, {140, 98, 70, 18, "ROCKETS"}, {140, 92, 62, 8, "PLASMA"}};
static void draw_view(int g) {                                       // the gun as you hold it: from behind and to the right, its muzzle under the crosshair
  spr_new(GL_[g].w, GL_[g].h); int H = GL_[g].h;
  if (g == 0) { capsule(116, H + 4, 96, H - 22, 13, R_OLIVE, -1);                       // the glove
    blob(84, H - 26, 30, 20, R_STEEL, 0); boxs(70, H - 52, 90, H - 46, R_DARK, 6);       // the body, the rail
    capsule(76, H - 34, 64, 14, 10, R_DARK, 2); capsule(64, 22, 62, 6, 5, R_DARK, 3);    // the shroud, the barrel
    for (int k = 0; k < 4; k++) boxs(66 - k * 1, 30 + k * 9, 76 - k * 1, 32 + k * 9, R_CYAN, 14);   // the cells
    blob(76, H - 56, 9, 6, R_DARK, 3); disc(76, H - 57, 3, R_CYAN, 15); boxs(92, H - 14, 104, H, R_DARK, 5); }
  else if (g == 1) { capsule(118, H + 4, 98, H - 20, 13, R_OLIVE, -1);
    capsule(72, H + 2, 64, 12, 8, R_DARK, 3); capsule(88, H + 2, 78, 12, 8, R_DARK, 2);  // two barrels
    disc(64, 11, 6, R_DARK, 1); disc(78, 11, 6, R_DARK, 1);
    capsule(84, H - 18, 72, 40, 14, R_WOOD, 0); for (int k = 0; k < 5; k++) boxs(62 + k, 44 + k * 7, 92 - k, 45 + k * 7, R_WOOD, 6); }
  else if (g == 2) { capsule(120, H + 4, 102, H - 18, 13, R_OLIVE, -1);
    capsule(84, H + 6, 70, 22, 14, R_GRASS, -3); boxs(58, 30, 84, 36, R_GOLD, 11); boxs(62, 62, 90, 66, R_GOLD, 9);
    disc(70, 19, 10, R_DARK, 3); disc(70, 19, 6, R_DARK, 1); glow(70, 19, 3, R_FIRE); boxs(96, H - 30, 106, H - 4, R_DARK, 5); }
  else { capsule(118, H + 4, 98, H - 20, 13, R_OLIVE, -1);
    blob(80, H - 24, 28, 24, R_TEAL, 0); capsule(74, H - 40, 63, 12, 8, R_TEAL, 1); capsule(64, 18, 62, 6, 5, R_DARK, 2);
    for (int k = 0; k < 4; k++) glow(70 - k * 2, H - 36 - k * 13, 5, R_CYAN); boxs(56, H - 30, 60, H - 10, R_DARK, 4); }
  outline(); }
static void grow_pictures(void) {
  for (int f = 0; f < 3; f++) draw_grunt(f);
  for (int f = 0; f < 2; f++) draw_drone(f);
  for (int f = 0; f < 3; f++) draw_brute(f);
  for (int f = 0; f < 3; f++) draw_titan(f);
  draw_barrel(); for (int f = 0; f < 6; f++) draw_boom(f); draw_scrap();
  for (int k = 0; k < 5; k++) draw_pick(k);
  for (int k = 0; k < 4; k++) draw_shot(k);
  for (int g = 0; g < 4; g++) draw_view(g); }

// LESSON-BEGIN 13 of 24: The arena and the ray walk
// MAP     : the 32 x 32 map between the MAP markers was made and checked by tools/make_map.py (walled, all 798 open squares
//           reachable). The map is data with its check named.
// CAST    : the carry rule in two directions: from (x, y) along (rx, ry), take the nearer of the next x-line and the next
//           y-line, add its step. No angle, no tangent. ddx = 2^30 / |rx| is the step along the ray per x-line.
// WHY     : the ray is built as look + lean x camera (section 21), so the length to the wall along it is already the depth
//           along the look. The fisheye correction (multiply by cos of the angle) never arises, because no angle was used.
// TRY     : normalise each ray to unit length before cast (divide by its length). Predict: walls bow outward at the screen
//           edges (the fisheye), by the factor sqrt(1 + 0.66^2) = 1.20 at the edge columns.
// LESSON-END
// ================================================================ THE ARENA
// MAP BEGIN (tools/make_map.py)
static const char *const MAPROWS[32] = {
  "SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS",
  "S,,,,,,,,,,,,,,..______________S",
  "S,e,,,,,BBB,,,,e.____________e_S",
  "S,,BBB,,BBB,,,S..____TTTTTTTT__S",
  "S,,BB,,,BBB,,,S..____T______T__S",
  "S,,B,B,,,,,,,,S..____T______T__S",
  "S,,,,,h,,,,a,,S.._b__T__MM__T__S",
  "S,,,,,,,,,,,,,,..________h__T__S",
  "S,,BB,,,BB,,,,,..___________T__S",
  "S,,BB,,,BB,,,,,.b______TTTTTT__S",
  "S,,BB,,,,,_____________________S",
  "S,,,,,,,,,_____________________S",
  "S,,,,,,,,,__M______M__b____a___S",
  "S,,,,,,,,,_____w_______________S",
  "S,,BBBB,,,_____________________S",
  "S.e.......____________w......e.S",
  "S........._b___P______.........S",
  "S.........____________,,,TTTT,,S",
  "S..CC.....____________,,,,,,V,,S",
  "S..C......__M______M__,VV,,,V,,S",
  "S.....b...____________,,,,,,V,,S",
  "S......CC.____________,h,,,,,,,S",
  "S.a..........w...,,,V,,,,,V,,,,S",
  "S........b.......,,,V,,,,,V,,,,S",
  "S.......h...C....,,,,,,,b,V,,a,S",
  "S..CC............S,,V,,,,,V,,,,S",
  "S...C....CC......S,,V,,,,,,,,,,S",
  "S....b....C......S,,VVV,VVV,,,,S",
  "S.....C..........S,,,,,,,,,,,,,S",
  "S.e............e.,,,,,,,,,,,,e,S",
  "S................,,,,,,,,,,,,,,S",
  "SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS",
};
// MAP END
#define MW 32
#define ONE 65536
static uint8_t WM[MW][MW], FM[MW][MW];                 // wall id (0 open) and floor id per square
static int32_t start_x, start_y; static int n_spawn, n_bhome, n_phome;
static int16_t SPAWN[12][2], BHOME[12][2], PHOME[16][3];
static void read_world(void) {
  n_spawn = n_bhome = n_phome = 0;
  for (int y = 0; y < MW; y++) for (int x = 0; x < MW; x++) { char c = MAPROWS[y][x]; uint8_t w = 0, f = 2;
    switch (c) { case 'S': w = 1; break; case 'B': w = 2; break; case 'M': w = 3; break; case 'C': w = 4; break; case 'T': w = 5; break; case 'V': w = 6; break;
      case '.': f = 0; break; case ',': f = 1; break; case '_': f = 2; break;
      case 'P': start_x = x * ONE + ONE / 2; start_y = y * ONE + ONE / 2; break;
      case 'e': SPAWN[n_spawn][0] = (int16_t)x; SPAWN[n_spawn++][1] = (int16_t)y; break;
      case 'b': f = 0; BHOME[n_bhome][0] = (int16_t)x; BHOME[n_bhome++][1] = (int16_t)y; break;
      case 'h': case 'a': case 'w': PHOME[n_phome][0] = (int16_t)x; PHOME[n_phome][1] = (int16_t)y; PHOME[n_phome++][2] = (int16_t)(c == 'h' ? 0 : c == 'a' ? 1 : 2); break; }
    WM[y][x] = w; FM[y][x] = f; } }
static int wall_at(int32_t x, int32_t y) { int cx = x >> 16, cy = y >> 16; if (cx < 0 || cy < 0 || cx >= MW || cy >= MW) return 1; return WM[cy][cx] != 0; }
static int blocked(int32_t x, int32_t y, int32_t r) { return wall_at(x - r, y - r) || wall_at(x + r, y - r) || wall_at(x - r, y + r) || wall_at(x + r, y + r); }
// the carry rule in two directions: from (x, y) along (rx, ry) (on 16384), take the nearer of the next x-line and the next y-line, add its step.
// Returns the square-on length (16.16 squares) of the ray built as dir + plane * k (for a unit ray, the true length); side 0 = an x-line.
static int32_t cast(int32_t x, int32_t y, int32_t rx, int32_t ry, int *side, int *wid, int32_t *hitx, int32_t *hity) {
  int mx = x >> 16, my = y >> 16, sx = rx < 0 ? -1 : 1, sy = ry < 0 ? -1 : 1;
  int32_t ax = rx < 0 ? -rx : rx, ay = ry < 0 ? -ry : ry;
  int32_t ddx = ax ? (int32_t)((((int64_t)1) << 30) / ax) : 0x3FFFFFFF, ddy = ay ? (int32_t)((((int64_t)1) << 30) / ay) : 0x3FFFFFFF;
  int32_t fx = x & 0xFFFF, fy = y & 0xFFFF;
  int64_t dsx = (int64_t)(rx < 0 ? fx : ONE - fx) * ddx >> 16, dsy = (int64_t)(ry < 0 ? fy : ONE - fy) * ddy >> 16;
  int s = 0, w = 1;
  for (int k = 0; k < 80; k++) {
    if (dsx < dsy) { dsx += ddx; mx += sx; s = 0; } else { dsy += ddy; my += sy; s = 1; }
    if (mx < 0 || my < 0 || mx >= MW || my >= MW) { w = 1; break; }
    if (WM[my][mx]) { w = WM[my][mx]; break; } }
  int64_t d = s == 0 ? dsx - ddx : dsy - ddy; if (d < 3000) d = 3000;
  *side = s; *wid = w; *hitx = x + (int32_t)(d * rx >> 14); *hity = y + (int32_t)(d * ry >> 14); return (int32_t)d; }
static int see(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {     // a clear line: sampled every quarter square
  int32_t dx = x1 - x0, dy = y1 - y0, m = absi(dx) > absi(dy) ? absi(dx) : absi(dy); int n = (int)(m / (ONE / 4)) + 1;
  for (int i = 1; i < n; i++) if (wall_at(x0 + (int32_t)((int64_t)dx * i / n), y0 + (int32_t)((int64_t)dy * i / n))) return 0;
  return 1; }
static int32_t dist256(int32_t dx, int32_t dy) { dx >>= 8; dy >>= 8; return isqrt32(dx * dx + dy * dy); }   // in 1/256 squares
static uint8_t DIST[MW][MW], GDIST[MW][MW];
static void flood(uint8_t D[MW][MW], int gx, int gy) {               // steps from a square to every open square (255: none)
  static uint16_t Q[MW * MW]; memset(D, 255, MW * MW); if (gx < 0 || gy < 0 || gx >= MW || gy >= MW || WM[gy][gx]) return;
  int h = 0, t = 0; D[gy][gx] = 0; Q[t++] = (uint16_t)(gy * MW + gx);
  while (h < t) { int q = Q[h++], x = q % MW, y = q / MW, d = D[y][x];
    static const int8_t NX[4] = {1, -1, 0, 0}, NY[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) { int a = x + NX[k], b = y + NY[k]; if (!WM[b][a] && D[b][a] == 255) { D[b][a] = (uint8_t)(d < 254 ? d + 1 : 254); Q[t++] = (uint16_t)(b * MW + a); } } } }

// LESSON-BEGIN 14 of 24: What moves: motes, bursts, shots, foes, kills
// WHAT    : every moving thing holds a place and a carried rate in 16.16 squares; each frame the rate is latched into the place.
// RULE    : a step is tried one axis at a time; blocked() tests four corners of the thing's radius; a refused axis
//           reverses its rate at half (f->vx = -f->vx / 2). C's / truncates toward zero, so a rate of 1 stops dead.
// LESSON-END
// ================================================================ WHAT MOVES
enum { F_GRUNT, F_DRONE, F_BRUTE, F_TITAN, F_BARREL };
typedef struct { int hp; int32_t speed, rad, hgt; int spr, nfr, cool, score, shot, drop; int32_t pref; } FoeKind;
static const FoeKind FK[5] = {
  {40, 2300, 19660, 51118, SPR_GRUNT, 3, 58, 100, 0, 30, 4 * ONE}, {24, 3900, 16384, 27525, SPR_DRONE, 2, 46, 150, 0, 22, 5 * ONE / 2},
  {170, 1600, 27525, 68813, SPR_BRUTE, 3, 96, 400, 1, 65, 5 * ONE}, {1400, 1300, 39321, 104857, SPR_TITAN, 3, 58, 3000, 1, 100, 6 * ONE},
  {20, 0, 18350, 40632, SPR_BARREL, 1, 0, 0, 0, 0, 0}};
enum { S_BOLT, S_FIRE, S_ROCKET, S_PLASMA, S_EROCKET };
typedef struct { int32_t speed; int dmg; int32_t splash, size; int spr; } ShotKind;
static const ShotKind SK[5] = {{9000, 7, 0, 11796, SPR_BOLT}, {7200, 13, 0, 19660, SPR_FIREB}, {18000, 110, 111411, 14418, SPR_ROCKET}, {26000, 16, 0, 13107, SPR_PLASMA}, {10000, 30, 91750, 16384, SPR_ROCKET}};
typedef struct { int cd, pellets, dmg, spread, max, start; } GunKind;
static const GunKind GK[4] = {{5, 1, 11, 10, 240, 140}, {18, 8, 10, 110, 40, 12}, {24, 0, 0, 0, 20, 6}, {3, 0, 0, 0, 260, 90}};
#define NFOE 24
#define NSHOT 48
#define NMOTE 200
#define NFX 32
#define NPICK 20
#define NFLOAT 8
static Player P; static Foe FO[NFOE]; static Shot SO[NSHOT]; static Mote MO[NMOTE]; static Fx FX[NFX]; static Pick PK[NPICK]; static Floater FLT[NFLOAT];
enum { M_TITLE, M_PLAY, M_OVER };
typedef struct { int16_t x, y, vx, vy; uint8_t life, kind; } Brass;
#define NBRASS 16
static Brass BS[NBRASS]; static int32_t tr_x, tr_y, tr_z; static int tracer_t;
static int muzzle_t, gmode, gm_t, wave, wave_q[4], spawn_t, break_t, banner_t, combo_banner_t, hitmark, shake, flash_t, aim_hit;
static const char *banner_s, *combo_s; static char banner_buf[24];
static uint32_t frame_n, book_kills, book_shots, book_hurt, book_deaths, book_waves, book_barrels, book_best;
static int32_t cam_x, cam_y, cam_a, horizon;
static int ghost = 1;
static void add_mote(int32_t x, int32_t y, int32_t z, int32_t vx, int32_t vy, int32_t vz, uint8_t col, int life, int size, int kind) {
  static int next; for (int k = 0; k < NMOTE; k++) { int i = (next + k) % NMOTE; if (!MO[i].life) { Mote *m = &MO[i];
      m->x = x; m->y = y; m->z = z; m->vx = vx; m->vy = vy; m->vz = vz; m->col = col; m->life = (uint8_t)life; m->size = (uint8_t)size; m->kind = (uint8_t)kind; next = i + 1; return; } } }
static int32_t rnd_s(int32_t amp) { uint32_t r = scatter(); return (int32_t)(r % (uint32_t)(2 * amp + 1)) - amp; }
static void burst(int32_t x, int32_t y, int32_t z, int n, int ramp, int kind, int32_t speed) {   // sparks or debris flung out
  for (int k = 0; k < n; k++) { int32_t vx = rnd_s(speed), vy = rnd_s(speed), vz = rnd_s(speed) + speed / 2; uint32_t r = scatter();
    add_mote(x, y, z, vx, vy, vz, ix(ramp, 10 + (int)(r % 6)), 18 + (int)((r >> 8) % 24), kind == 1 ? 3 : 2, kind); } }
static void add_fx(int32_t x, int32_t y, int32_t z, int kind, int big) {
  for (int i = 0; i < NFX; i++) if (!FX[i].life) { Fx *f = &FX[i]; f->x = x; f->y = y; f->z = z; f->kind = (uint8_t)kind; f->t = 0; f->big = (uint8_t)big;
      f->life = (uint8_t)(kind == 1 ? 255 : kind == 0 ? 18 : 9); return; }
  for (int i = 0; i < NFX; i++) if (FX[i].kind == 1) { FX[i].life = 0; add_fx(x, y, z, kind, big); return; } }
static void floater(int32_t x, int32_t y, const char *s, uint16_t col) {
  for (int i = 0; i < NFLOAT; i++) if (FLT[i].life <= 0) { FLT[i].x16 = x; FLT[i].y16 = y; FLT[i].life = 50; FLT[i].col = col; snprintf(FLT[i].s, sizeof FLT[i].s, "%s", s); return; } }
static void drop_pick(int32_t x, int32_t y, int kind) {
  for (int i = 0; i < NPICK; i++) if (!PK[i].alive && !PK[i].home) { Pick *p = &PK[i]; p->x = x; p->y = y; p->kind = (uint8_t)kind; p->alive = 1; p->wait = 700; return; } }
static void hurt_player(int dmg, int32_t fromx, int32_t fromy) {
  if (P.inv > 0 || !P.alive) return;
  P.hp -= dmg; P.hurt_t = 14; book_hurt += (uint32_t)dmg;
  int32_t c, s; unit16(P.a, &c, &s); int32_t rx = fromx - P.x, ry = fromy - P.y;
  int32_t fwd = (int32_t)(((int64_t)rx * c + (int64_t)ry * s) >> 14), side = (int32_t)(((int64_t)-rx * s + (int64_t)ry * c) >> 14);
  P.hurt_from = absi(fwd) > absi(side) ? (fwd > 0 ? 0 : 2) : (side > 0 ? 1 : 3);       // ahead, right, behind, left
  if (dmg >= 12) shake = shake > 8 ? shake : 8;
  if (P.hp <= 0) { P.hp = 0; P.alive = 0; gmode = M_OVER; gm_t = 0; book_deaths++; if (P.score > book_best) book_best = P.score; } }
static void kill_foe(int i);
static void damage_foe(int i, int dmg, int32_t kx, int32_t ky, int mine) {
  Foe *f = &FO[i]; if (!f->alive || f->state == 9) return;
  f->hp = (int16_t)(f->hp - dmg); f->flash = 4; if (dmg >= 25 && f->kind != F_TITAN) f->stag = 8;
  if (f->kind != F_BARREL) { int32_t k = f->kind == F_TITAN ? 1 : f->kind == F_BRUTE ? 4 : 10; f->vx += (kx * k * dmg) >> 12; f->vy += (ky * k * dmg) >> 12; }
  if (mine) { hitmark = 7; P.hits++; }
  if (f->hp <= 0) { if (f->kind == F_BARREL) { f->state = 9; f->t = 7; } else kill_foe(i); } }
static void splash(int32_t x, int32_t y, int32_t R, int dmg, int mine, int self) {
  int32_t r256 = R >> 8;
  for (int i = 0; i < NFOE; i++) { Foe *f = &FO[i]; if (!f->alive || i == self) continue; int32_t d = dist256(f->x - x, f->y - y);
    if (d < r256) { int32_t k = (r256 - d) * 256 / r256, kx = d ? (f->x - x) / d : 0, ky = d ? (f->y - y) / d : 0; damage_foe(i, (int)(dmg * k / 256) + 1, kx * 64, ky * 64, mine); } }
  int32_t dp = dist256(P.x - x, P.y - y);
  if (dp < r256) { int32_t k = (r256 - dp) * 256 / r256; hurt_player((int)((mine ? dmg / 4 : dmg) * k / 256) / (mine ? 2 : 1) + 1, x, y); }
  int32_t sh = 26 - dp / 128; if (sh > shake) shake = (int)clampi(sh, 0, 26); flash_t = 4; }
static void boom_at(int32_t x, int32_t y, int32_t z, int big) {
  add_fx(x, y, z, 0, big); burst(x, y, z + ONE / 4, big ? 26 : 14, R_FIRE, 0, big ? 2600 : 1700); burst(x, y, z + ONE / 4, big ? 12 : 6, R_DARK, 1, 2000);
  for (int k = 0; k < (big ? 6 : 3); k++) { int32_t a = rnd_s(ONE / 3), b = rnd_s(ONE / 3); add_mote(x + a, y + b, z + ONE / 3, 0, 0, 260, ix(R_STONE, 9 + k % 3), 60, 6 + big * 2, 2); } }
static void combo_kill(int score, int32_t x, int32_t y) {
  P.combo = P.combo_t > 0 ? P.combo + 1 : 1; P.combo_t = 160; int mult = P.combo > 8 ? 8 : P.combo; P.score += (uint32_t)(score * mult); P.kills++; book_kills++;
  char b[16]; snprintf(b, sizeof b, mult > 1 ? "+%d X%d" : "+%d", score, mult); floater(x, y, b, glass(255, 230, 90));
  static const char *const NAMES[9] = {0, 0, "DOUBLE KILL", "TRIPLE KILL", "MULTI KILL", "RAMPAGE", "RAMPAGE", "RAMPAGE", "UNSTOPPABLE"};
  if (P.combo >= 2) { combo_s = NAMES[P.combo > 8 ? 8 : P.combo]; combo_banner_t = 70; } }
static void kill_foe(int i) {
  Foe *f = &FO[i]; int k = f->kind; f->alive = 0;
  int big = k == F_BRUTE || k == F_TITAN; boom_at(f->x, f->y, f->z, big);
  if (k == F_TITAN) { for (int j = 0; j < 4; j++) { int32_t a = rnd_s(ONE / 2), b = rnd_s(ONE / 2); boom_at(f->x + a, f->y + b, ONE / 3, 1); } shake = 30; }
  burst(f->x, f->y, f->z + FK[k].hgt / 2, k == F_TITAN ? 50 : 22, k == F_DRONE ? R_PURPLE : k == F_GRUNT ? R_RED : k == F_BRUTE ? R_OLIVE : R_GOLD, 1, 2200);
  if (k != F_DRONE) add_fx(f->x, f->y, 0, 1, big);
  combo_kill(FK[k].score, f->x, f->y);
  uint32_t r = scatter() % 100;
  if ((int)r < FK[k].drop) { uint32_t r2 = scatter(); int kind = (r2 & 1) ? 0 : 1; if (k >= F_BRUTE) { int g = 1 + (int)(r2 % 3); if (!P.has[g]) kind = 1 + g; } drop_pick(f->x, f->y, kind); } }
static int foe_new(int kind, int32_t x, int32_t y) {
  for (int i = 0; i < NFOE; i++) if (!FO[i].alive) { Foe *f = &FO[i]; memset(f, 0, sizeof *f); f->kind = (uint8_t)kind; f->alive = 1; f->x = x; f->y = y; f->hp = (int16_t)FK[kind].hp;
      f->id = (uint8_t)i; uint32_t r = scatter(); f->cd = (int16_t)(40 + (int)(r % 60)); f->side = (r >> 8) & 1 ? 1 : -1; f->z = kind == F_DRONE ? ONE / 3 : 0; return i; }
  return -1; }
static void fire_shot(int kind, int mine, int32_t x, int32_t y, int32_t z, int32_t c, int32_t s) {
  for (int i = 0; i < NSHOT; i++) if (!SO[i].life) { Shot *o = &SO[i]; o->kind = (uint8_t)kind; o->mine = (uint8_t)mine; o->x = x + (c * 20000 >> 14); o->y = y + (s * 20000 >> 14); o->z = z;
      o->vx = c * SK[kind].speed >> 14; o->vy = s * SK[kind].speed >> 14; o->vz = 0; o->life = 200; return; } }
static void aim_at(int32_t fx, int32_t fy, int32_t tx, int32_t ty, int32_t *c, int32_t *s) {
  int32_t d = dist256(tx - fx, ty - fy); if (d < 1) d = 1; *c = (int32_t)(((int64_t)(tx - fx) >> 8) * 16384 / d); *s = (int32_t)(((int64_t)(ty - fy) >> 8) * 16384 / d); }
static void turn_vec(int32_t c, int32_t s, int32_t da, int32_t *oc, int32_t *os) { int32_t uc, us; unit16(da, &uc, &us); *oc = (c * uc - s * us) >> 14; *os = (c * us + s * uc) >> 14; }

// LESSON-BEGIN 15 of 24: A foe's frame
// WHAT    : a foe sees the player by the same cast as the eye, walks the flood downhill, and fires. One rule of sight for all.
// LESSON-END
// ---------------------------------------------------------------- a foe's frame
static void foe_step(Foe *f, int i) {
  const FoeKind *K = &FK[f->kind];
  if (f->flash) f->flash--;
  if (f->kind == F_BARREL) { if (f->state == 9 && --f->t <= 0) { f->alive = 0; book_barrels++; boom_at(f->x, f->y, 0, 1); splash(f->x, f->y, 124518, 95, 1, i); } return; }
  f->t++; if (f->cd > 0) f->cd--;
  int32_t dx = P.x - f->x, dy = P.y - f->y, d = dist256(dx, dy);
  if (((frame_n + (uint32_t)i) & 3) == 0) f->state = (uint8_t)((f->state & ~1) | (see(f->x, f->y, P.x, P.y) ? 1 : 0));
  int los = f->state & 1;
  int32_t tx = 0, ty = 0;                                          // where it wants to go (a direction, any length)
  if (los && d > (K->pref >> 8)) { tx = dx; ty = dy; }
  else if (los) { if ((f->t % 110) == 0) f->side = (int16_t)-f->side; tx = -dy * f->side; ty = dx * f->side; if (d < (K->pref >> 9)) { tx -= dx; ty -= dy; } }
  else { int cx = f->x >> 16, cy = f->y >> 16, best = DIST[cy][cx], bx = cx, by = cy;
    static const int8_t NX[4] = {1, -1, 0, 0}, NY[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) { int a = cx + NX[k], b = cy + NY[k]; if (DIST[b][a] < best) { best = DIST[b][a]; bx = a; by = b; } }
    tx = bx * ONE + ONE / 2 - f->x; ty = by * ONE + ONE / 2 - f->y; }
  for (int j = 0; j < NFOE; j++) { Foe *g = &FO[j]; if (j == i || !g->alive) continue; int32_t ex = f->x - g->x, ey = f->y - g->y;   // keep apart
    if (absi(ex) < 40000 && absi(ey) < 40000) { tx += ex * 2; ty += ey * 2; } }
  int32_t tl = dist256(tx, ty); int32_t sp = f->stag > 0 ? 0 : K->speed;
  if (f->stag > 0) f->stag--;
  if (tl > 0) { f->vx += (int32_t)(((int64_t)(tx >> 8) * sp / tl - f->vx) / 4); f->vy += (int32_t)(((int64_t)(ty >> 8) * sp / tl - f->vy) / 4); }
  int32_t r = K->rad > 19660 ? 19660 : K->rad;
  int32_t nx = f->x + f->vx; if (!blocked(nx, f->y, r)) f->x = nx; else f->vx = -f->vx / 2;
  int32_t ny = f->y + f->vy; if (!blocked(f->x, ny, r)) f->y = ny; else f->vy = -f->vy / 2;
  if (f->kind == F_DRONE) { int32_t c, s; unit16((int32_t)(f->t * 37 + i * 400), &c, &s); f->z = ONE / 3 + (s * 6000 >> 14); }
  if (los && f->cd == 0 && d < 11 * 256 && P.alive) {               // fire
    int32_t c, s; aim_at(f->x, f->y, P.x, P.y, &c, &s); int32_t z = f->z + K->hgt / 2;
    if (f->kind == F_GRUNT || f->kind == F_DRONE) fire_shot(S_BOLT, 0, f->x, f->y, z, c, s);
    else if (f->kind == F_BRUTE) for (int k = -1; k <= 1; k++) { int32_t c2, s2; turn_vec(c, s, k * 90, &c2, &s2); fire_shot(S_FIRE, 0, f->x, f->y, z, c2, s2); }
    else { if ((f->t / K->cool) & 1) fire_shot(S_EROCKET, 0, f->x, f->y, z, c, s); else for (int k = -2; k <= 2; k++) { int32_t c2, s2; turn_vec(c, s, k * 70, &c2, &s2); fire_shot(S_FIRE, 0, f->x, f->y, z, c2, s2); } }
    uint32_t rr = scatter(); f->cd = (int16_t)(K->cool + (int)(rr % 30)); f->state = (uint8_t)(f->state | 2); f->t = 0; }
  if (f->t > 12) f->state &= (uint8_t)~2; }

// LESSON-BEGIN 16 of 24: Shots, motes, flames, pickups: each latched once a frame
// LESSON-END
// ---------------------------------------------------------------- shots, motes, flames, pickups
static void shots_step(void) {
  for (int i = 0; i < NSHOT; i++) { Shot *o = &SO[i]; if (!o->life) continue; o->life--;
    const ShotKind *K = &SK[o->kind];
    if ((o->kind == S_ROCKET || o->kind == S_EROCKET) && (frame_n & 1)) { int32_t ta = rnd_s(300), tb = rnd_s(300); add_mote(o->x, o->y, o->z, ta, tb, 200, ix(R_STONE, 11), 40, 4, 2); }   // draws into named values first: argument order differs between chips
    if (o->kind == S_FIRE && (frame_n & 1)) add_mote(o->x, o->y, o->z, 0, 0, 100, ix(R_FIRE, 12), 12, 3, 0);
    for (int sub = 0; sub < 2 && o->life; sub++) {
      o->x += o->vx / 2; o->y += o->vy / 2;
      int hit = wall_at(o->x, o->y), who = -1;
      if (o->mine) { for (int j = 0; j < NFOE; j++) { Foe *f = &FO[j]; if (!f->alive) continue; int32_t r = FK[f->kind].rad;
          if (absi(f->x - o->x) < r && absi(f->y - o->y) < r) { who = j; break; } } }
      else if (P.alive && absi(P.x - o->x) < 19660 && absi(P.y - o->y) < 19660) who = 99;
      if (!hit && who < 0) continue;
      if (K->splash) { int32_t bx = o->x - (hit ? o->vx / 2 : 0), by = o->y - (hit ? o->vy / 2 : 0); boom_at(bx, by, o->z - ONE / 4 > 0 ? o->z - ONE / 4 : 0, 1); splash(bx, by, K->splash, K->dmg, o->mine, -1); }
      else if (who == 99) hurt_player(K->dmg, o->x - o->vx * 4, o->y - o->vy * 4);
      else if (who >= 0) { int32_t c, s; aim_at(0, 0, o->vx, o->vy, &c, &s); damage_foe(who, K->dmg, c, s, 1); burst(o->x, o->y, o->z, 6, o->kind == S_PLASMA ? R_CYAN : R_GOLD, 0, 1400); add_fx(o->x, o->y, o->z - ONE / 8, 2, 0); }
      else { burst(o->x - o->vx / 2, o->y - o->vy / 2, o->z, 5, o->kind == S_PLASMA ? R_CYAN : o->kind == S_BOLT ? R_RED : R_FIRE, 0, 1200); add_fx(o->x - o->vx / 2, o->y - o->vy / 2, o->z - ONE / 8, 2, 0); }
      o->life = 0; } } }
static void motes_step(void) {
  for (int i = 0; i < NMOTE; i++) { Mote *m = &MO[i]; if (!m->life) continue; m->life--;
    if (m->kind == 2) { m->vz = m->vz * 15 / 16 + 20; m->vx = m->vx * 15 / 16; m->vy = m->vy * 15 / 16; if ((m->life & 7) == 0 && m->size < 14) m->size++; }
    else { m->vz -= m->kind == 1 ? 260 : 180; }
    int32_t nx = m->x + m->vx, ny = m->y + m->vy; if (wall_at(nx, ny)) { m->vx = -m->vx / 2; m->vy = -m->vy / 2; } else { m->x = nx; m->y = ny; }
    m->z += m->vz; if (m->z < 0) { m->z = 0; if (m->kind == 1) { m->vz = -m->vz / 3; m->vx = m->vx * 3 / 4; m->vy = m->vy * 3 / 4; } else if (m->kind == 0) m->life = m->life > 3 ? 3 : m->life; } } }
static void pickups_step(void) {
  for (int i = 0; i < NPICK; i++) { Pick *p = &PK[i];
    if (!p->alive) { if (p->home && p->wait > 0 && --p->wait == 0) p->alive = 1; continue; }
    if (!p->home && --p->wait <= 0) { p->alive = 0; continue; }
    if (!P.alive || dist256(P.x - p->x, P.y - p->y) > 140) continue;
    int took = 0;
    if (p->kind == 0 && P.hp < 100) { P.hp = P.hp + 35 > 100 ? 100 : P.hp + 35; took = 1; floater(p->x, p->y, "+35 HEALTH", glass(120, 255, 140)); }
    if (p->kind == 1) { for (int g = 0; g < 4; g++) if (P.has[g] && P.ammo[g] < GK[g].max) { P.ammo[g] = clampi(P.ammo[g] + GK[g].max / 3, 0, GK[g].max); took = 1; } if (took) floater(p->x, p->y, "+AMMO", glass(255, 220, 120)); }
    if (p->kind >= 2) { int g = p->kind - 1; P.has[g] = 1; P.ammo[g] = clampi(P.ammo[g] + GK[g].max / 2, 0, GK[g].max); took = 1; if (P.gun < g) P.gun = g; floater(p->x, p->y, GL_[g].name, glass(140, 220, 255)); }
    if (took) { p->alive = 0; p->wait = 600; } } }

// LESSON-BEGIN 17 of 24: Waves: counted, with the Titan every fifth
// LESSON-END
// ---------------------------------------------------------------- the waves
static int foes_left(void) { int n = 0; for (int i = 0; i < NFOE; i++) if (FO[i].alive && FO[i].kind != F_BARREL) n++; return n + wave_q[0] + wave_q[1] + wave_q[2] + wave_q[3]; }
static void barrels_home(void) { for (int b = 0; b < n_bhome; b++) { int32_t x = BHOME[b][0] * ONE + ONE / 2, y = BHOME[b][1] * ONE + ONE / 2; int there = 0;
    for (int i = 0; i < NFOE; i++) if (FO[i].alive && FO[i].kind == F_BARREL && FO[i].x == x && FO[i].y == y) there = 1;
    if (!there) foe_new(F_BARREL, x, y); } }
static void start_wave(int w) {
  wave = w; wave_q[F_GRUNT] = 3 + w * 2; wave_q[F_DRONE] = w >= 2 ? w + 1 : 0; wave_q[F_BRUTE] = w >= 3 ? (w - 1) / 2 : 0; wave_q[F_TITAN] = (w % 5) == 0 ? 1 : 0;
  spawn_t = 60; snprintf(banner_buf, sizeof banner_buf, w % 5 == 0 ? "WAVE %d  TITAN" : "WAVE %d", w); banner_s = banner_buf; banner_t = 110; book_waves = (uint32_t)w; }
static void waves_step(void) {
  if (break_t > 0) { if (--break_t == 0) start_wave(wave + 1); return; }
  if (spawn_t > 0) spawn_t--;
  int alive = 0; for (int i = 0; i < NFOE; i++) if (FO[i].alive && FO[i].kind != F_BARREL) alive++;
  int cap = 8 + wave > 14 ? 14 : 8 + wave, queued = wave_q[0] + wave_q[1] + wave_q[2] + wave_q[3];
  if (queued && spawn_t == 0 && alive < cap) {
    int best = -1; int32_t bd = 0;
    for (int k = 0; k < n_spawn; k++) { uint32_t r = scatter(); int32_t x = SPAWN[k][0] * ONE + ONE / 2, y = SPAWN[k][1] * ONE + ONE / 2, d = dist256(x - P.x, y - P.y) + (int32_t)(r % 512);
      if (d > bd) { bd = d; best = k; } }
    uint32_t r = scatter() % (uint32_t)queued; int kind = 0; for (int k = 3; k >= 0; k--) { if ((int)r < wave_q[k]) { kind = k; break; } r -= (uint32_t)wave_q[k]; }
    if (wave_q[F_TITAN]) kind = F_TITAN;
    if (best >= 0 && foe_new(kind, SPAWN[best][0] * ONE + ONE / 2, SPAWN[best][1] * ONE + ONE / 2) >= 0) { wave_q[kind]--; add_fx(SPAWN[best][0] * ONE + ONE / 2, SPAWN[best][1] * ONE + ONE / 2, 0, 2, 1); }
    spawn_t = 46 - (wave * 3 > 26 ? 26 : wave * 3); }
  if (!queued && !alive) { break_t = 150; P.score += (uint32_t)(500 * wave); snprintf(banner_buf, sizeof banner_buf, "WAVE %d CLEAR", wave); banner_s = banner_buf; banner_t = 120; barrels_home();
    for (int i = 0; i < NPICK; i++) if (PK[i].home && !PK[i].alive) PK[i].wait = 1; } }

// LESSON-BEGIN 18 of 24: The player
// WHAT    : the hands become rates; the rates are latched into the place, refused at walls.
// LESSON-END
// ---------------------------------------------------------------- the player
typedef struct { int mv, st, tn, fire, sel, cycle, dash; } Hands;
static Hands IN;
static void fire_gun(void) {
  int g = P.gun; if (P.fire_cd > 0 || !P.alive) return;
  if (!P.ammo[g]) { for (int k = 3; k >= 0; k--) if (P.has[k] && P.ammo[k]) { P.gun = k; break; } return; }
  P.ammo[g]--; P.fire_cd = GK[g].cd; P.kick = g == 2 ? 18 : g == 1 ? 14 : 6; muzzle_t = 3; P.shots++; book_shots++;
  if (g <= 1) for (int i = 0; i < NBRASS; i++) if (!BS[i].life) { uint32_t r = scatter(); Brass *q = &BS[i]; q->x = (int16_t)((SW / 2 + 58) * 16); q->y = (int16_t)((SH - 58) * 16);
      q->vx = (int16_t)(40 + (int)(r % 30)); q->vy = (int16_t)(-70 - (int)((r >> 8) % 30)); q->life = 26; q->kind = (uint8_t)g; break; }
  int32_t c, s; unit16(P.a, &c, &s);
  if (g == 2) { fire_shot(S_ROCKET, 1, P.x, P.y, ONE * 2 / 5, c, s); shake = shake > 4 ? shake : 4; return; }
  if (g == 3) { int32_t c2, s2; turn_vec(c, s, rnd_s(12), &c2, &s2); fire_shot(S_PLASMA, 1, P.x, P.y, ONE * 2 / 5, c2, s2); return; }
  for (int k = 0; k < GK[g].pellets; k++) {                         // hitscan: the first wall along the line, and the nearest foe whose width the line passes through
    int32_t c2, s2; turn_vec(c, s, rnd_s(GK[g].spread), &c2, &s2);
    int sd, wid; int32_t hx, hy, wd = cast(P.x, P.y, c2, s2, &sd, &wid, &hx, &hy);
    int best = -1; int32_t ba = wd;
    for (int i = 0; i < NFOE; i++) { Foe *f = &FO[i]; if (!f->alive) continue; int32_t rx = f->x - P.x, ry = f->y - P.y;
      int32_t along = (int32_t)(((int64_t)rx * c2 + (int64_t)ry * s2) >> 14), side = (int32_t)(((int64_t)-rx * s2 + (int64_t)ry * c2) >> 14);
      if (along > 0 && along < ba && absi(side) < FK[f->kind].rad) { ba = along; best = i; } }
    if (k == 0) { tr_x = P.x + (int32_t)((int64_t)ba * c2 >> 14); tr_y = P.y + (int32_t)((int64_t)ba * s2 >> 14); tr_z = ONE / 2; tracer_t = g == 0 ? 2 : 0; }
    if (best >= 0) { Foe *f = &FO[best]; damage_foe(best, GK[g].dmg, c2, s2, 1); burst(P.x + (int32_t)((int64_t)ba * c2 >> 14), P.y + (int32_t)((int64_t)ba * s2 >> 14), f->z + FK[f->kind].hgt / 2, 4, R_GOLD, 0, 1300); }
    else { int32_t bx = hx - (c2 * 1200 >> 14), by = hy - (s2 * 1200 >> 14); burst(bx, by, ONE / 2 + rnd_s(ONE / 8), 3, R_GOLD, 0, 1000);
      add_mote(bx, by, ONE / 2, 0, 0, 120, ix(wid == 4 ? R_WOOD : wid == 2 ? R_BRICK : R_STONE, 11), 26, 5, 2); } } }
static void player_step(void) {
  if (P.fire_cd > 0) P.fire_cd--; if (P.kick > 0) P.kick -= (P.kick + 3) / 4; if (P.hurt_t > 0) P.hurt_t--; if (P.inv > 0) P.inv--; if (P.dash_t > 0) P.dash_t--;
  if (P.combo_t > 0 && --P.combo_t == 0) P.combo = 0;
  if (!P.alive) return;
  if (IN.sel >= 0 && IN.sel < 4 && P.has[IN.sel] && P.ammo[IN.sel]) P.gun = IN.sel;
  if (IN.cycle) { for (int k = 1; k <= 4; k++) { int g = (P.gun + k) % 4; if (P.has[g] && P.ammo[g]) { P.gun = g; break; } } }
  P.a = (P.a + IN.tn * absi(IN.tn) * 72 / (127 * 127)) & 4095;
  int32_t c, s; unit16(P.a, &c, &s);
  P.vx += (int32_t)(((int64_t)IN.mv * c - (int64_t)IN.st * s) * 1150 / (127 * 16384)); P.vy += (int32_t)(((int64_t)IN.mv * s + (int64_t)IN.st * c) * 1150 / (127 * 16384));
  if (IN.dash && P.dash_t == 0) { int32_t dx = IN.mv * c - IN.st * s, dy = IN.mv * s + IN.st * c; if (!IN.mv && !IN.st) { dx = c * 127; dy = s * 127; }
    int32_t l = dist256(dx, dy); if (l > 0) { P.vx += (int32_t)((int64_t)(dx >> 8) * 24000 / l); P.vy += (int32_t)((int64_t)(dy >> 8) * 24000 / l); } P.dash_t = 40; P.inv = 10; }
  P.vx -= mouth(P.vx, 5); P.vy -= mouth(P.vy, 5);
  int32_t nx = P.x + P.vx; if (!blocked(nx, P.y, 14418)) P.x = nx; else P.vx = 0;
  int32_t ny = P.y + P.vy; if (!blocked(P.x, ny, 14418)) P.y = ny; else P.vy = 0;
  int32_t sp = dist256(P.vx, P.vy); P.moving = sp > 6; if (P.moving) P.bob = (P.bob + sp * 3) & 4095;
  if (IN.fire) fire_gun(); }
static void new_game(void) {
  memset(&P, 0, sizeof P); memset(BS, 0, sizeof BS); memset(FO, 0, sizeof FO); memset(SO, 0, sizeof SO); memset(MO, 0, sizeof MO); memset(FX, 0, sizeof FX); memset(PK, 0, sizeof PK); memset(FLT, 0, sizeof FLT);
  P.x = start_x; P.y = start_y; P.a = 3072; P.hp = 100; P.alive = 1; P.has[0] = 1; P.ammo[0] = GK[0].start; P.inv = 60;
  for (int k = 0; k < n_phome && k < NPICK; k++) { Pick *p = &PK[k]; p->x = PHOME[k][0] * ONE + ONE / 2; p->y = PHOME[k][1] * ONE + ONE / 2; p->home = 1; p->alive = 1;
    p->kind = (uint8_t)(PHOME[k][2] == 2 ? 2 + (k % 3) : PHOME[k][2]); }
  barrels_home(); break_t = 0; start_wave(1); gmode = M_PLAY; gm_t = 0; }

// LESSON-BEGIN 19 of 24: THE GHOST: a player made of the game's own readings
// WHAT    : looks for a foe it can see, turns until the centre line passes through it, fires. With nothing in sight it walks
//           the flood toward a goal and string-pulls: steers for the farthest square of its path it can see.
// WHY     : the ghost lets the bench play 9,000 frames on the desk and 2,000 on the emulated chip and compare every dot. A game
//           nobody can play on the bench has no second witness for its play.
// FOUND   : the ghost first hugged walls (steering for the next square's centre) and never picked up a gun (README).
// LESSON-END
// ================================================================ THE GHOST: it looks for a foe it can see, turns until the line passes through it, and fires
static int g_strafe = 90, g_t, g_dash_cd;
static void ghost_hands(void) {
  memset(&IN, 0, sizeof IN); IN.sel = -1; g_t++; if (g_dash_cd > 0) g_dash_cd--;
  if (!P.alive) return;
  int32_t c, s; unit16(P.a, &c, &s);
  int best = -1; int32_t bd = 0x7fffffff;
  for (int i = 0; i < NFOE; i++) { Foe *f = &FO[i]; if (!f->alive || f->kind == F_BARREL) continue; int32_t d = dist256(f->x - P.x, f->y - P.y); if (d < bd && see(P.x, P.y, f->x, f->y)) { bd = d; best = i; } }
  if (best >= 0) for (int i = 0; i < NFOE; i++) { Foe *b = &FO[i]; if (!b->alive || b->kind != F_BARREL || b->state == 9) continue;   // a barrel by the target: shoot that instead
    if (dist256(b->x - FO[best].x, b->y - FO[best].y) < 300 && dist256(b->x - P.x, b->y - P.y) > 640 && see(P.x, P.y, b->x, b->y)) { best = i; break; } }
  if (P.hp < 45 || P.ammo[P.gun] == 0) {                          // hurt: go for a health pack the ghost can reach and see
    for (int i = 0; i < NPICK; i++) { Pick *p = &PK[i]; if (p->alive && p->kind == 0 && P.hp < 45 && dist256(p->x - P.x, p->y - P.y) < 1400) { best = -2 - i; break; } } }
  int32_t tx, ty;
  if (best >= 0) { Foe *f = &FO[best]; tx = f->x; ty = f->y;
    int32_t rx = tx - P.x, ry = ty - P.y, fwd = (int32_t)(((int64_t)rx * c + (int64_t)ry * s) >> 14), side = (int32_t)(((int64_t)-rx * s + (int64_t)ry * c) >> 14);
    IN.tn = fwd <= 0 ? (side >= 0 ? 127 : -127) : (int)clampi((int64_t)side * 127 * 3 / (absi(fwd) + 1), -127, 127);
    int32_t d = bd; int k = f->kind;
    int want = 0; if (P.has[2] && P.ammo[2] && (k == F_BRUTE || k == F_TITAN || k == F_BARREL) && d > 600) want = 2; else if (P.has[1] && P.ammo[1] && d < 900) want = 1; else if (P.has[3] && P.ammo[3]) want = 3;
    IN.sel = want;
    if (fwd > 0 && absi(side) < FK[k].rad * 3 / 4 && !(P.gun == 2 && d < 520)) IN.fire = 1;
    int32_t pref = P.gun == 1 ? 600 : P.gun == 2 ? 1000 : 850;
    IN.mv = d > pref + 256 ? 110 : d < pref - 256 ? -100 : 0;
    if ((g_t % 55) == 0) g_strafe = -g_strafe; IN.st = g_strafe;
    if (P.hurt_t == 13) g_strafe = -g_strafe;
    for (int i = 0; i < NSHOT; i++) { Shot *o = &SO[i]; if (!o->life || o->mine) continue; if (dist256(o->x - P.x, o->y - P.y) < 420 && g_dash_cd == 0) { IN.dash = 1; g_dash_cd = 90; } } }
  else {                                                            // nobody in sight: a goal (health when hurt, a gun it lacks, ammo, the nearest foe)
    int gx = MW / 2, gy = MW / 2; int32_t nd = 0x7fffffff;
    for (int i = 0; i < NPICK; i++) { Pick *p = &PK[i]; if (!p->alive) continue;
      int want = (p->kind == 0 && P.hp < 60) || (p->kind >= 2 && !P.has[p->kind - 1]) || (p->kind == 1 && P.ammo[P.gun] < GK[P.gun].max / 5);
      int32_t d = dist256(p->x - P.x, p->y - P.y) - (p->kind >= 2 ? 1200 : 0); if (want && d < nd) { nd = d; gx = p->x >> 16; gy = p->y >> 16; } }
    if (nd == 0x7fffffff) for (int i = 0; i < NFOE; i++) { Foe *f = &FO[i]; if (!f->alive || f->kind == F_BARREL) continue; int32_t d = dist256(f->x - P.x, f->y - P.y); if (d < nd) { nd = d; gx = f->x >> 16; gy = f->y >> 16; } }
    if ((g_t & 7) == 0 || GDIST[gy][gx] != 0) flood(GDIST, gx, gy);
    // string-pull: walk the flood downhill up to eight squares and steer for the farthest one in plain sight
    int cx = P.x >> 16, cy = P.y >> 16; tx = gx * ONE + ONE / 2; ty = gy * ONE + ONE / 2;
    static const int8_t NX[4] = {1, -1, 0, 0}, NY[4] = {0, 0, 1, -1};
    int wx = cx, wy = cy; int found = 0;
    for (int step = 0; step < 8 && GDIST[wy][wx] > 0; step++) { int bx = wx, by = wy, bb = GDIST[wy][wx];
      for (int k = 0; k < 4; k++) { int a2 = wx + NX[k], b2 = wy + NY[k]; if (GDIST[b2][a2] < bb) { bb = GDIST[b2][a2]; bx = a2; by = b2; } }
      if (bx == wx && by == wy) break; wx = bx; wy = by;
      int32_t qx = wx * ONE + ONE / 2, qy = wy * ONE + ONE / 2;
      if (see(P.x + 14000, P.y + 14000, qx, qy) && see(P.x - 14000, P.y - 14000, qx, qy) && see(P.x + 14000, P.y - 14000, qx, qy) && see(P.x - 14000, P.y + 14000, qx, qy)) { tx = qx; ty = qy; found = 1; } else if (found) break; }
    if (!found && GDIST[cy][cx] > 0) { tx = wx * ONE + ONE / 2; ty = wy * ONE + ONE / 2; }
    int32_t rx = tx - P.x, ry = ty - P.y, fwd = (int32_t)(((int64_t)rx * c + (int64_t)ry * s) >> 14), side = (int32_t)(((int64_t)-rx * s + (int64_t)ry * c) >> 14);
    IN.tn = fwd <= 0 ? (side >= 0 ? 127 : -127) : (int)clampi((int64_t)side * 127 * 2 / (absi(fwd) + 1), -127, 127);
    IN.mv = fwd > absi(side) ? 127 : 40; } }

// LESSON-BEGIN 20 of 24: The hands: stick and buttons, read once a frame
// LESSON-END
// ================================================================ THE HANDS: stick and buttons
static int a_down, b_tap, c_down, p_tap, hands_seen; static int32_t stick_x, stick_y;
#if defined(ON_BOARD) && !defined(GHOST_ONLY)
#define LIVE_HANDS 1
static int jx0 = 2048, jy0 = 2048, lb, lsw;
static void pad_begin(void) {
  pinMode(PIN_JSW, INPUT_PULLUP); pinMode(PIN_A, INPUT_PULLUP); pinMode(PIN_B, INPUT_PULLUP); pinMode(PIN_C, INPUT_PULLUP);
  analogReadResolution(12); int32_t sx = 0, sy = 0; for (int k = 0; k < 32; k++) { sx += analogRead(PIN_VRX); sy += analogRead(PIN_VRY); }
  jx0 = (int)(sx >> 5); jy0 = (int)(sy >> 5); }
static int pad_read(void) {
  int rx = ((analogRead(PIN_VRX) + analogRead(PIN_VRX)) >> 1) - jx0, ry = ((analogRead(PIN_VRY) + analogRead(PIN_VRY)) >> 1) - jy0;
#ifdef JOY_SWAP
  { int t = rx; rx = ry; ry = t; }
#endif
#ifdef JOY_FLIP_X
  rx = -rx;
#endif
#ifdef JOY_FLIP_Y
  ry = -ry;
#endif
  rx /= 14; ry /= 14; rx = clampi(rx, -127, 127); ry = clampi(ry, -127, 127); if (rx > -14 && rx < 14) rx = 0; if (ry > -14 && ry < 14) ry = 0;
  int a = !digitalRead(PIN_A), b = !digitalRead(PIN_B), c = !digitalRead(PIN_C), sw = !digitalRead(PIN_JSW);
  stick_x = rx; stick_y = ry; a_down = a; c_down = c; b_tap = b && !lb; p_tap = sw && !lsw; lb = b; lsw = sw;
  return rx || ry || a || b || c || sw; }
#endif

// LESSON-BEGIN 21 of 24: What the eye sees: the one view rule
// WHAT    : grow_view lands the camera's lean per column once: PLX[x] = mouth((2x - 319) * 16384 * 66, 320 * 100), so the edge
//           columns lean 0.66. Each column's ray is look + PLX[x] * the look turned a quarter.
// RULE    : a thing at (wx, wy) goes to the glass by depth along the look and side across it, scaled by PPU / depth with
//           PPU = 242. Walls, robots, shadows all use this one rule, so they agree on where things are.
// LESSON-END
// ================================================================ WHAT THE EYE SEES THIS FRAME
#define PPU 242                                     // dots per square at a distance of one square (160 / 0.66): squares look square
#define EYE (ONE / 2)
static int32_t PLX[SW], XOFF[SW];                   // the camera's lean per column (0.66 at the edges, on 16384); the panorama's offset per column
static int32_t CZ[SW], CTOP16[SW], CVS[SW], VACC[SW]; static int16_t CTOP[SW], CBOT[SW]; static const uint8_t *CTEX[SW]; static uint8_t CPAL[SW];
static uint8_t CMH1[SW], CMH2[SW], CCU[SW];
static int32_t rdx0, rdy0, rdx1, rdy1, cdir_c, cdir_s;
#define NBILL 64
#define NDOT 200
static Bill BL[NBILL]; static int bl_n; static Dot DT[NDOT]; static int dt_n;
static int project(int32_t wx, int32_t wy, int32_t *depth, int32_t *sx16);
typedef struct { int32_t depth; int16_t cx, hw, yf, yn; } Shade;
#define NSHADE 40
static Shade SHD[NSHADE]; static int sh_n;
static void add_shade(int32_t wx, int32_t wy, int32_t r) {           // a soft dark oval on the floor under a thing: its near and far rows by the one view rule
  if (sh_n >= NSHADE) return; int32_t d, sx16; if (!project(wx, wy, &d, &sx16) || d < r + 12000) return; Shade *h = &SHD[sh_n];
  h->depth = d; h->cx = (int16_t)clampi(sx16 >> 16, -200, SW + 200); h->hw = (int16_t)clampi((int32_t)((int64_t)r * PPU / d), 1, 200);
  h->yf = (int16_t)clampi(horizon + (int32_t)((int64_t)EYE * PPU / (d + r)), -1, SH); h->yn = (int16_t)clampi(horizon + (int32_t)((int64_t)EYE * PPU / (d - r)), -1, SH); sh_n++; }
static void grow_view(void) { for (int x = 0; x < SW; x++) { PLX[x] = mouth((2 * x - (SW - 1)) * 16384 * 66, SW * 100); XOFF[x] = mouth((x - SW / 2) * 188, SW); } }
static void cast_columns(void) {
  int32_t c = cdir_c, s = cdir_s;
  rdx0 = c + (int32_t)(((int64_t)-s * PLX[0]) >> 14); rdy0 = s + (int32_t)(((int64_t)c * PLX[0]) >> 14);
  rdx1 = c + (int32_t)(((int64_t)-s * PLX[SW - 1]) >> 14); rdy1 = s + (int32_t)(((int64_t)c * PLX[SW - 1]) >> 14);
  int32_t pano0 = (cam_a * 1024) >> 12, drift = (int32_t)(frame_n >> 2);
  for (int x = 0; x < SW; x++) {
    int32_t rx = c + (int32_t)(((int64_t)-s * PLX[x]) >> 14), ry = s + (int32_t)(((int64_t)c * PLX[x]) >> 14);
    int side, wid; int32_t hx, hy; int32_t d = cast(cam_x, cam_y, rx, ry, &side, &wid, &hx, &hy);
    int32_t wx = side == 0 ? hy : hx; int u = (wx >> 10) & 63; if ((side == 0 && rx > 0) || (side == 1 && ry < 0)) u = 63 - u;
    int64_t lh16 = ((int64_t)PPU << 32) / d;                       // the wall's height on the glass, 16.16 dots
    int64_t top = ((int64_t)horizon << 16) - lh16 / 2; if (top < -(1LL << 30)) top = -(1LL << 30);
    CZ[x] = d; CTOP16[x] = (int32_t)top; CVS[x] = (int32_t)(((int64_t)64 << 32) / (lh16 > 64 ? lh16 : 64));
    int64_t t = (top + 65535) >> 16, b = (top + lh16) >> 16; CTOP[x] = (int16_t)clampi((int32_t)(t > 32767 ? 32767 : t < -32767 ? -32767 : t), -1, SH);
    CBOT[x] = (int16_t)clampi((int32_t)(b > 32767 ? 32767 : b < -32767 ? -32767 : b), -1, SH);
    CTEX[x] = TEX[wid] + u * 64; CPAL[x] = (uint8_t)(haze_of(d) + side * 8);
    int32_t pano = (pano0 + XOFF[x]) & 1023; CMH1[x] = MNT1[pano]; CMH2[x] = MNT2[pano]; CCU[x] = (uint8_t)(((pano >> 2) + drift) & 255); } }
// a thing standing in the world, to the glass: depth along the look, side across it, then scaled by PPU / depth
static int project(int32_t wx, int32_t wy, int32_t *depth, int32_t *sx16) {
  int32_t rx = wx - cam_x, ry = wy - cam_y;
  int32_t d = (int32_t)(((int64_t)rx * cdir_c + (int64_t)ry * cdir_s) >> 14), side = (int32_t)(((int64_t)-rx * cdir_s + (int64_t)ry * cdir_c) >> 14);
  if (d < 9000) return 0;
  *depth = d; *sx16 = (SW / 2 << 16) + (int32_t)(((int64_t)side * PPU * 65536) / d); return 1; }
static void add_bill(int32_t wx, int32_t wy, int32_t z, int32_t hgt, int spr, int pal) {
  if (bl_n >= NBILL) return; int32_t d, sx16; if (!project(wx, wy, &d, &sx16)) return;
  const Spr *S = &SP[spr]; Bill *b = &BL[bl_n];
  int64_t h16 = ((int64_t)hgt * PPU << 16) / d, w16 = h16 * S->w / S->h;
  int64_t bot16 = (int64_t)horizon * 65536 + ((int64_t)(EYE - z) * PPU * 65536) / d, top16 = bot16 - h16, left16 = sx16 - w16 / 2;
  if (w16 < 65536 || left16 > (int64_t)SW << 16 || left16 + w16 < 0 || top16 > (int64_t)SH << 16 || bot16 < 0) return;
  b->depth = d; b->top16 = (int32_t)clampi((int32_t)(top16 >> 4), -(1 << 27), 1 << 27) * 16; b->left16 = (int32_t)left16;
  b->x0 = (int32_t)clampi((int32_t)((left16 + 65535) >> 16), 0, SW - 1); b->x1 = (int32_t)clampi((int32_t)((left16 + w16) >> 16), 0, SW - 1);
  b->y0 = (int32_t)clampi((int32_t)((top16 + 65535) >> 16), 0, SH - 1); b->y1 = (int32_t)clampi((int32_t)(bot16 >> 16), 0, SH - 1);
  b->ustep = (int32_t)(((int64_t)S->w << 32) / w16); b->vstep = (int32_t)(((int64_t)S->h << 32) / h16);
  b->tex = SPX + S->off; b->tw = S->w; b->th = S->h; b->pal = (uint8_t)pal; bl_n++; }
static void build_scene(void) {
  bl_n = 0; dt_n = 0; sh_n = 0;
  for (int i = 0; i < NFOE; i++) { Foe *f = &FO[i]; if (!f->alive) continue; const FoeKind *K = &FK[f->kind];
    int fr = K->nfr == 1 ? 0 : (f->state & 2) && K->nfr == 3 ? 2 : (int)((f->t / 9) & 1);
    if (f->kind == F_DRONE) fr = (int)((frame_n / 3) & 1);
    int32_t d, sx; int pal = f->flash ? PAL_STRUCK : (project(f->x, f->y, &d, &sx) ? haze_of(d) : 0);
    if (f->kind == F_BARREL && f->state == 9) pal = (f->t & 1) ? PAL_STRUCK : pal;
    add_bill(f->x, f->y, f->z, K->hgt, K->spr + fr, pal); add_shade(f->x, f->y, K->rad * (f->kind == F_DRONE ? 3 : 4) / 4); }
  for (int i = 0; i < NSHOT; i++) { Shot *o = &SO[i]; if (!o->life) continue; add_bill(o->x, o->y, o->z - SK[o->kind].size / 2, SK[o->kind].size, SK[o->kind].spr, 0); }
  for (int i = 0; i < NFX; i++) { Fx *f = &FX[i]; if (!f->life) continue;
    if (f->kind == 1) add_bill(f->x, f->y, 0, f->big ? ONE * 3 / 8 : ONE / 4, SPR_SCRAP, 0);
    else { int fr = f->kind == 0 ? f->t / 3 : f->t / 3; if (fr > 5) fr = 5; int32_t h = f->kind == 0 ? (f->big ? ONE * 3 / 2 : ONE * 9 / 10) : (f->big ? ONE * 3 / 5 : ONE * 7 / 20);
      add_bill(f->x, f->y, f->z + (f->kind == 0 ? 0 : 0) - (f->kind == 0 ? ONE / 8 : 0), h, SPR_BOOM + fr, 0); } }
  for (int i = 0; i < NPICK; i++) { Pick *p = &PK[i]; if (!p->alive) continue; int32_t c, s; unit16((int32_t)(frame_n * 48 + i * 300), &c, &s);
    add_bill(p->x, p->y, ONE / 14 + (s * 2400 >> 14), p->kind >= 2 ? ONE / 5 : ONE * 3 / 10, p->kind == 0 ? SPR_MED : p->kind == 1 ? SPR_AMMO : SPR_GUN1 + p->kind - 2, 0); }
  for (int i = 1; i < bl_n; i++) { Bill t = BL[i]; int j = i - 1; while (j >= 0 && BL[j].depth < t.depth) { BL[j + 1] = BL[j]; j--; } BL[j + 1] = t; }   // far first
  for (int i = 0; i < NMOTE && dt_n < NDOT; i++) { Mote *m = &MO[i]; if (!m->life) continue; int32_t d, sx16; if (!project(m->x, m->y, &d, &sx16)) continue;
    Dot *t = &DT[dt_n]; int32_t sy = horizon + (int32_t)(((int64_t)(EYE - m->z) * PPU) / d), sz = (int32_t)((int64_t)m->size * PPU * 1024 / d) >> 10;
    sz = clampi(sz, 1, 24); t->depth = d; t->sx = (int16_t)clampi(sx16 >> 16, -40, SW + 40); t->sy = (int16_t)clampi(sy, -40, SH + 40); t->sz = (int16_t)sz; t->col = m->col; t->pal = (uint8_t)(m->kind == 2 ? haze_of(d) : 0); dt_n++; } }

// LESSON-BEGIN 22 of 24: The bands: background, things far to near, dots, then the gun and HUD
// WHAT    : 15 bands of 16 rows. Floors: each row's distance is one divide (242 / (2 row + 1)), then two adds a dot.
// BUDGET  : the README records 21 ms of the 29.4 ms average in these bands. The wire takes about 31 ms a frame at 40 MHz and
//           runs beside them on DMA, so the wire sets the pace.
// LESSON-END
// ================================================================ THE BANDS: the background, then things far to near, then dots, then the gun and the HUD
static uint16_t *RB[2]; static int band_y0; static uint16_t *OB;
static void FAST background_band(void) {
  int y0 = band_y0;
  for (int x = 0; x < SW; x++) VACC[x] = (int32_t)((((int64_t)y0 << 16) + 32768 - CTOP16[x]) * CVS[x] >> 16);
  for (int y = y0; y < y0 + TH; y++) { uint16_t *o = OB + (y - y0) * SW;
    if (y < horizon) { int e = horizon - y; const uint16_t *row = SKYROW[e < SKY_E ? e : SKY_E - 1]; int cv = e >= 18 && e < 128 ? (e - 18) * 64 / 110 : -1; const uint8_t *cl = cv >= 0 ? CLOUD[cv] : 0;
      uint16_t mc = MCOL[e < 40 ? e : 39], hc = HCOL[e < 24 ? e : 23];
      for (int x = 0; x < SW; x++) {
        if (y >= CTOP[x]) { o[x] = PAL[CPAL[x]][CTEX[x][(VACC[x] >> 16) & 63]]; }
        else if (e <= CMH2[x]) o[x] = hc;
        else if (e <= CMH1[x]) o[x] = (CMH1[x] > 27 && e > CMH1[x] - 3) ? SNOW : mc;
        else o[x] = row[cl ? cl[CCU[x]] : 0];
        VACC[x] += CVS[x]; } }
    else { int32_t rd = (int32_t)(((int64_t)PPU << 16) / (2 * (y - horizon) + 1));   // the floor's distance on this row (squares, 16.16)
      int32_t fx = cam_x + (int32_t)(((int64_t)rd * rdx0) >> 14), fy = cam_y + (int32_t)(((int64_t)rd * rdy0) >> 14);
      int32_t dfx = (int32_t)((((int64_t)rd * (rdx1 - rdx0)) >> 14) / (SW - 1)), dfy = (int32_t)((((int64_t)rd * (rdy1 - rdy0)) >> 14) / (SW - 1));
      const uint16_t *pal = PAL[haze_of(rd)];
      for (int x = 0; x < SW; x++) {
        if (y <= CBOT[x]) o[x] = PAL[CPAL[x]][CTEX[x][(VACC[x] >> 16) & 63]];
        else { const uint8_t *ft = FTX[FM[(fy >> 16) & 31][(fx >> 16) & 31]]; o[x] = pal[ft[((fy >> 4) & 0xFC0) | ((fx >> 10) & 63)]]; }
        VACC[x] += CVS[x]; fx += dfx; fy += dfy; } } } }
static uint16_t shadec(uint16_t w) { uint16_t v = (uint16_t)((w >> 8) | (w << 8)); v = (uint16_t)(v - ((v >> 2) & 0x39E7) - ((v >> 3) & 0x18E3)); return (uint16_t)((v >> 8) | (v << 8)); }
static void FAST shades_band(void) {
  int y0 = band_y0, y1 = band_y0 + TH - 1;
  for (int i = 0; i < sh_n; i++) { const Shade *h = &SHD[i]; if (h->yn < y0 || h->yf > y1 || h->yn <= h->yf) continue;
    int ya = h->yf > y0 ? h->yf : y0, yb = h->yn < y1 ? h->yn : y1, half = (h->yn - h->yf + 1) / 2, mid = (h->yn + h->yf) / 2;
    for (int y = ya; y <= yb; y++) { int dy = y - mid; int32_t q = half > 0 ? 256 - dy * dy * 256 / (half * half + 1) : 256; if (q <= 0) continue;
      int w = (int)(h->hw * isqrt32(q * 256) / 256); uint16_t *o = OB + (y - y0) * SW;
      for (int x = h->cx - w; x <= h->cx + w; x++) { if (x < 0 || x >= SW || CZ[x] <= h->depth || y <= CBOT[x]) continue; o[x] = shadec(o[x]); } } } }
static void FAST bills_band(void) {
  int y0 = band_y0, y1 = band_y0 + TH - 1;
  for (int i = 0; i < bl_n; i++) { const Bill *b = &BL[i]; if (b->y1 < y0 || b->y0 > y1) continue;
    int ya = b->y0 > y0 ? b->y0 : y0, yb = b->y1 < y1 ? b->y1 : y1; const uint16_t *pal = PAL[b->pal];
    int32_t u0 = (int32_t)((((int64_t)b->x0 << 16) + 32768 - b->left16) * b->ustep >> 16); if (u0 < 0) u0 = 0;
    for (int y = ya; y <= yb; y++) { int32_t v = (int32_t)(((((int64_t)y << 16) + 32768 - b->top16) * b->vstep) >> 32); if (v < 0) v = 0; if (v >= b->th) v = b->th - 1;
      const uint8_t *row = b->tex + v * b->tw; uint16_t *o = OB + (y - y0) * SW; int32_t u = u0;
      for (int x = b->x0; x <= b->x1; x++, u += b->ustep) { if (CZ[x] <= b->depth) continue; int uu = u >> 16; if (uu >= b->tw) break; uint8_t t = row[uu]; if (t) o[x] = pal[t]; } } } }
static void FAST dots_band(void) {
  int y0 = band_y0, y1 = band_y0 + TH - 1;
  for (int i = 0; i < dt_n; i++) { const Dot *t = &DT[i]; int h = t->sz / 2, ya = t->sy - h, yb = t->sy - h + t->sz - 1; if (yb < y0 || ya > y1) continue;
    if (ya < y0) ya = y0; if (yb > y1) yb = y1; int xa = t->sx - h, xb = xa + t->sz - 1; if (xa < 0) xa = 0; if (xb > SW - 1) xb = SW - 1; uint16_t c = PAL[t->pal][t->col];
    for (int y = ya; y <= yb; y++) { uint16_t *o = OB + (y - y0) * SW; for (int x = xa; x <= xb; x++) if (CZ[x] > t->depth) o[x] = c; } } }
// LESSON-BEGIN 23 of 24: The overlay: light added, never a lit scene
// WHAT    : muzzle flashes, tracers, explosions, the hurt edge: light added to a dot already drawn (addc), clipped to the band.
// LESSON-END
// ---------------------------------------------------------------- the overlay: drawing tools that clip to the band
static uint16_t c565(int r, int g, int b) { return glass(r, g, b); }
static void opix(int x, int y, uint16_t c) { if (x >= 0 && x < SW && y >= band_y0 && y < band_y0 + TH) OB[(y - band_y0) * SW + x] = c; }
static void orect(int x, int y, int w, int h, uint16_t c) { int a = y > band_y0 ? y : band_y0, b = y + h < band_y0 + TH ? y + h : band_y0 + TH;
  for (int yy = a; yy < b; yy++) for (int xx = x < 0 ? 0 : x; xx < x + w && xx < SW; xx++) OB[(yy - band_y0) * SW + xx] = c; }
static uint16_t addc(uint16_t w, int r, int g, int b) {             // light added to a dot already drawn (wire order in, wire order out)
  uint16_t v = (uint16_t)((w >> 8) | (w << 8)); int R = ((v >> 11) & 31) + (r >> 3), G = ((v >> 5) & 63) + (g >> 2), B = (v & 31) + (b >> 3);
  if (R > 31) R = 31; if (G > 63) G = 63; if (B > 31) B = 31; v = (uint16_t)(R << 11 | G << 5 | B); return (uint16_t)((v >> 8) | (v << 8)); }
static uint16_t dimc(uint16_t w) { uint16_t v = (uint16_t)((w >> 8) | (w << 8)); v = (uint16_t)((v >> 1) & 0x7BEF); return (uint16_t)((v >> 8) | (v << 8)); }
static void odim(int x, int y, int w, int h) { int a = y > band_y0 ? y : band_y0, b = y + h < band_y0 + TH ? y + h : band_y0 + TH;
  for (int yy = a; yy < b; yy++) for (int xx = x < 0 ? 0 : x; xx < x + w && xx < SW; xx++) OB[(yy - band_y0) * SW + xx] = dimc(OB[(yy - band_y0) * SW + xx]); }
static int otext_s(int x, int y, const char *s, uint16_t c, int sc) {
  int h = 7 * sc; if (y > band_y0 + TH || y + h + sc < band_y0) return x + 6 * sc * (int)strlen(s);
  uint16_t sh = c565(10, 8, 20);
  for (; *s; s++) { int chh = *s; if (chh >= 'a' && chh <= 'z') chh -= 32; int g = GLYPH[chh & 127];
    if (g) for (int pass = 0; pass < 2; pass++) for (int r = 0; r < 7; r++) { uint8_t bits = FACE[g - 1][r]; for (int k = 0; k < 5; k++) if (bits >> (4 - k) & 1)
      for (int yy = 0; yy < sc; yy++) for (int xx = 0; xx < sc; xx++) opix(x + k * sc + xx + (pass ? 0 : sc > 1 ? 2 : 1), y + r * sc + yy + (pass ? 0 : sc > 1 ? 2 : 1), pass ? c : sh); }
    x += 6 * sc; }
  return x; }
static int text_w(const char *s, int sc) { return 6 * sc * (int)strlen(s) - sc; }
static void ospr(int id, int x0, int y0, int pal) { const Spr *S = &SP[id]; if (y0 > band_y0 + TH || y0 + S->h < band_y0) return;
  int ya = y0 > band_y0 ? y0 : band_y0, yb = y0 + S->h < band_y0 + TH ? y0 + S->h : band_y0 + TH;
  for (int y = ya; y < yb; y++) { const uint8_t *row = SPX + S->off + (y - y0) * S->w; uint16_t *o = OB + (y - band_y0) * SW;
    for (int c = 0; c < S->w; c++) { int x = x0 + c; if (x < 0 || x >= SW) continue; uint8_t t = row[c]; if (t) o[x] = PAL[pal][t]; } } }
static void oglow(int cx, int cy, int r, int cr, int cg, int cb) {   // an additive flare: brightest at the middle, falling off by squares
  int ya = cy - r > band_y0 ? cy - r : band_y0, yb = cy + r < band_y0 + TH - 1 ? cy + r : band_y0 + TH - 1; int rr = r * r;
  for (int y = ya; y <= yb; y++) for (int x = cx - r; x <= cx + r; x++) { if (x < 0 || x >= SW) continue; int d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy); if (d2 >= rr) continue;
    int k = (rr - d2) * 256 / rr; uint16_t *o = OB + (y - band_y0) * SW + x; *o = addc(*o, cr * k >> 8, cg * k >> 8, cb * k >> 8); } }
static int view_bob_x, view_bob_y;
static void overlay_band(void) {
  int y0 = band_y0, y1 = band_y0 + TH;
  // the gun: carried low and right, bobbing as you walk, kicked by each shot
  if (P.alive || gmode == M_TITLE) { const GunLook *g = &GL_[P.gun]; int gx = SW / 2 - g->w / 2 + 44 + view_bob_x, gy = SH - g->h + 14 + view_bob_y + P.kick;
    if (gy < y1 && gy + g->h > y0) ospr(SPR_VIEW + P.gun, gx, gy, 0);
    if (muzzle_t > 0) { int mx = gx + g->mx, my = gy + g->my; int r = P.gun == 2 ? 30 : P.gun == 1 ? 26 : 18;
      if (P.gun == 3) oglow(mx, my, r, 80, 220, 255); else oglow(mx, my, r, 255, 210, 120); } }
  if (tracer_t > 0 && P.alive) { int32_t d, sx16; if (project(tr_x, tr_y, &d, &sx16)) { const GunLook *g = &GL_[P.gun];
      int x0 = SW / 2 - g->w / 2 + 44 + view_bob_x + g->mx, ya = SH - g->h + 14 + view_bob_y + P.kick + g->my;
      int x1 = sx16 >> 16, yb = horizon + (int)(((int64_t)(EYE - tr_z) * PPU) / d);
      int n = absi(x1 - x0) > absi(yb - ya) ? absi(x1 - x0) : absi(yb - ya); if (n < 1) n = 1;
      for (int k = n / 6; k <= n; k++) { int x = x0 + (x1 - x0) * k / n, y = ya + (yb - ya) * k / n; if (y >= y0 && y < y1 && x >= 0 && x < SW) { uint16_t *o = OB + (y - y0) * SW + x; *o = addc(*o, 255, 220, 120); if (x + 1 < SW) o[1] = addc(o[1], 120, 90, 40); } } } }
  for (int i = 0; i < NBRASS; i++) if (BS[i].life) { int x = BS[i].x >> 4, y = BS[i].y >> 4; uint16_t c = BS[i].kind ? c565(220, 60, 40) : c565(255, 210, 90); orect(x, y, 3, 2, c); orect(x + 1, y + 2, 1, 1, c565(120, 80, 30)); }
  if (P.alive && P.hp <= 30) { int k = 40 + (int)(RS[(frame_n * 6) & 255] > 0 ? RS[(frame_n * 6) & 255] : -RS[(frame_n * 6) & 255]) / 200;
    for (int y = y0; y < y1; y++) { uint16_t *o = OB + (y - y0) * SW; int ey = y < 10 ? 10 - y : y > SH - 11 ? y - (SH - 11) : 0;
      for (int x = 0; x < SW; x++) { int ex = x < 10 ? 10 - x : x > SW - 11 ? x - (SW - 11) : 0; int e = ex > ey ? ex : ey; if (e) o[x] = addc(o[x], e * k / 10, 0, 0); } } }
  // the edges redden when hit, from the side the hit came from
  if (P.hurt_t > 0) { int k = P.hurt_t * 18;
    for (int y = y0; y < y1; y++) { uint16_t *o = OB + (y - y0) * SW; int ey = y < 14 ? 14 - y : y > SH - 15 ? y - (SH - 15) : 0;
      for (int x = 0; x < SW; x++) { int ex = x < 14 ? 14 - x : x > SW - 15 ? x - (SW - 15) : 0; int e = ex > ey ? ex : ey;
        if (P.hurt_from == 3 && x < 40) e += (40 - x) / 3; if (P.hurt_from == 1 && x > SW - 41) e += (x - (SW - 41)) / 3; if (P.hurt_from == 0 && y < 30) e += (30 - y) / 3; if (P.hurt_from == 2 && y > SH - 31) e += (y - (SH - 31)) / 3;
        if (e) o[x] = addc(o[x], clampi(e * k / 14, 0, 255), 0, 0); } } }
  if (flash_t > 0) for (int y = y0; y < y1; y++) { uint16_t *o = OB + (y - y0) * SW; for (int x = 0; x < SW; x += 1) o[x] = addc(o[x], flash_t * 10, flash_t * 8, flash_t * 4); }
  // crosshair (red over a foe), and the hit mark
  { int g = 4 + P.kick / 3; uint16_t c = aim_hit ? c565(255, 70, 60) : c565(255, 255, 255);
    for (int k = 0; k < 5; k++) { opix(SW / 2 - g - k, SH / 2, c); opix(SW / 2 + g + k, SH / 2, c); opix(SW / 2, SH / 2 - g - k, c); opix(SW / 2, SH / 2 + g + k, c); }
    if (hitmark) for (int k = 3; k < 8; k++) { uint16_t hc = c565(255, 230, 120); opix(SW / 2 - k, SH / 2 - k, hc); opix(SW / 2 + k, SH / 2 - k, hc); opix(SW / 2 - k, SH / 2 + k, hc); opix(SW / 2 + k, SH / 2 + k, hc); } }
  // floating scores where things died
  for (int i = 0; i < NFLOAT; i++) { Floater *f = &FLT[i]; if (f->life <= 0) continue; int32_t d, sx16; if (!project(f->x16, f->y16, &d, &sx16)) continue;
    int sy = horizon + (int)(((int64_t)(EYE - ONE * 7 / 10 - (50 - f->life) * 600) * PPU) / d); int sx = (sx16 >> 16) - text_w(f->s, 1) / 2; if (sy > y0 - 10 && sy < y1 + 2) otext_s(sx, sy, f->s, f->col, 1); }
  if (y0 < 24) {                                                    // top: score and combo, wave, radar
    char b[32]; snprintf(b, sizeof b, "%06u", (unsigned)P.score); otext_s(6, 5, b, c565(255, 255, 255), 2);
    if (P.combo > 1) { snprintf(b, sizeof b, "X%d", P.combo > 8 ? 8 : P.combo); otext_s(84, 5, b, c565(255, 200, 40), 2); }
    snprintf(b, sizeof b, "WAVE %d", wave); otext_s(SW / 2 - text_w(b, 1) / 2, 4, b, c565(255, 240, 200), 1);
    snprintf(b, sizeof b, "FOES %d", foes_left()); otext_s(SW / 2 - text_w(b, 1) / 2, 14, b, c565(255, 160, 140), 1); }
  { int rcx = SW - 30, rcy = 30, R = 24;                           // the radar: turned with you, foes red, pickups green
    if (rcy - R < y1 && rcy + R >= y0) {
      for (int y = (rcy - R > y0 ? rcy - R : y0); y < (rcy + R < y1 ? rcy + R : y1); y++) { int dy = y - rcy; uint16_t *o = OB + (y - y0) * SW;
        for (int x = rcx - R; x <= rcx + R; x++) { int dx = x - rcx, d2 = dx * dx + dy * dy; if (d2 > R * R) continue; o[x] = d2 > (R - 2) * (R - 2) ? c565(250, 250, 240) : dimc(dimc(o[x])); } }
      int32_t c = cdir_c, s = cdir_s;
      for (int i = 0; i < NFOE + NPICK; i++) { int32_t wx, wy; uint16_t col; if (i < NFOE) { Foe *f = &FO[i]; if (!f->alive) continue; wx = f->x; wy = f->y; col = f->kind == F_BARREL ? c565(255, 150, 40) : c565(255, 60, 50); }
        else { Pick *p = &PK[i - NFOE]; if (!p->alive) continue; wx = p->x; wy = p->y; col = c565(90, 255, 120); }
        int32_t rx = (wx - P.x) >> 8, ry = (wy - P.y) >> 8, fw = (rx * c + ry * s) >> 14, sd = (-rx * s + ry * c) >> 14;   // 1/256 squares
        int px = rcx + sd * 2 / 256, py = rcy - fw * 2 / 256; if ((px - rcx) * (px - rcx) + (py - rcy) * (py - rcy) > (R - 3) * (R - 3)) continue;
        for (int k = -1; k <= 1; k++) { opix(px + k, py, col); opix(px, py + k, col); } }
      opix(rcx, rcy - 2, c565(255, 255, 255)); opix(rcx - 1, rcy, c565(255, 255, 255)); opix(rcx + 1, rcy, c565(255, 255, 255)); opix(rcx, rcy, c565(255, 255, 255)); } }
  if (y1 > SH - 30) {                                               // bottom: health, ammo, gun
    char b[24]; int hp = P.hp; uint16_t hc = hp > 60 ? c565(110, 255, 130) : hp > 30 ? c565(255, 220, 80) : c565(255, 80, 70);
    orect(8, SH - 23, 5, 15, hc); orect(3, SH - 18, 15, 5, hc);
    snprintf(b, sizeof b, "%d", hp); otext_s(22, SH - 24, b, hc, 2);
    orect(22, SH - 7, 60, 3, c565(30, 20, 30)); orect(22, SH - 7, hp * 60 / 100, 3, hc);
    snprintf(b, sizeof b, "%d", P.ammo[P.gun]); int w = text_w(b, 2); otext_s(SW - 8 - w, SH - 24, b, c565(255, 230, 150), 2);
    otext_s(SW - 8 - text_w(GL_[P.gun].name, 1), SH - 9, GL_[P.gun].name, c565(220, 240, 255), 1);
    for (int g = 0; g < 4; g++) { uint16_t c = !P.has[g] ? c565(60, 60, 70) : g == P.gun ? c565(255, 220, 90) : c565(200, 200, 210); orect(SW - 80 + g * 9, SH - 28, 6, 3, c); } }
  // banners
  if (banner_t > 0 && banner_s) { int sc = 3, w = text_w(banner_s, sc), y = 62; if (y < y1 && y + 7 * sc + 3 > y0) otext_s(SW / 2 - w / 2, y, banner_s, c565(255, 236, 160), sc); }
  if (combo_banner_t > 0 && combo_s) { int sc = 2, w = text_w(combo_s, sc), y = 96; if (y < y1 && y + 7 * sc + 3 > y0) otext_s(SW / 2 - w / 2, y, combo_s, (combo_banner_t & 4) ? c565(255, 120, 60) : c565(255, 220, 60), sc); }
  if (gmode == M_TITLE) { const char *t = "HIGH NOON"; int w = text_w(t, 5), y = 36; if (y < y1 && y + 40 > y0) { otext_s(SW / 2 - w / 2, y, t, c565(255, 190, 40), 5); }
    if (172 < y1 && 182 > y0 && (frame_n & 32)) { const char *p = "PRESS A"; otext_s(SW / 2 - text_w(p, 2) / 2, 172, p, c565(255, 255, 255), 2); } }
  if (gmode == M_OVER) { if (y0 < 150 && y1 > 70) odim(0, 70, SW, 80);
    const char *t = "YOU FELL"; int w = text_w(t, 4); otext_s(SW / 2 - w / 2, 82, t, c565(255, 90, 70), 4);
    char b[32]; snprintf(b, sizeof b, "SCORE %u  WAVE %d", (unsigned)P.score, wave); otext_s(SW / 2 - text_w(b, 1) / 2, 124, b, c565(255, 240, 200), 1); } }

// LESSON-BEGIN 24 of 24: The frame
// WHAT    : world_step, camera, then each band: background, shades, bills, dots, overlay, send. DESK builds print a line
//           (desk_line) and the books (waves, kills, shots, hurt, deaths, barrels).
// BENCH   : the emulator's F lines and the desk's lines carry the same print; the README records 2,000 of 2,000 identical.
// LESSON-END
// ================================================================ THE FRAME
static uint32_t wire_print, f_bytes_dummy, c_world, c_draw, c_bg, c_things, c_over, c_send, f_tiles;
#ifdef DESK
static uint16_t SHOWN[NTY][SW * TH];
#endif
static void FAST send_band(int b, uint16_t *rb) {
#ifndef ON_BOARD
  uint32_t h = 0x811c9dc5u; const uint32_t *q = (const uint32_t *)rb; for (int k = 0; k < SW * TH / 2; k++) { h = (h ^ q[k]) * 0x01000193u; h ^= h >> 15; }   // the print (bench builds only)
  wire_print = (wire_print ^ h) * 0x01000193u;
#endif
#ifdef DESK
  memcpy(SHOWN[b], rb, sizeof SHOWN[b]);
#endif
  window(0, SW - 1, b * TH, b * TH + TH - 1); pix(rb, SW * TH * 2); f_tiles++; }
static uint32_t slot_n;
static void world_step(void) {
  if (gmode != M_OVER || gm_t < 150) {
    if (ghost) ghost_hands();
    player_step();
    if ((frame_n % 6) == 0) flood(DIST, P.x >> 16, P.y >> 16);
    for (int i = 0; i < NFOE; i++) if (FO[i].alive) foe_step(&FO[i], i);
    shots_step(); waves_step(); }
  motes_step(); pickups_step();
  for (int i = 0; i < NFX; i++) if (FX[i].life) { FX[i].life--; FX[i].t++; }
  for (int i = 0; i < NFLOAT; i++) if (FLT[i].life > 0) FLT[i].life--;
  if (banner_t) banner_t--; if (combo_banner_t) combo_banner_t--; if (hitmark) hitmark--; if (flash_t) flash_t--; if (muzzle_t) muzzle_t--; if (tracer_t) tracer_t--;
  for (int i = 0; i < NBRASS; i++) if (BS[i].life) { Brass *q = &BS[i]; q->life--; q->x = (int16_t)(q->x + q->vx); q->y = (int16_t)(q->y + q->vy); q->vy = (int16_t)(q->vy + 9); } }
static void camera(void) {
  int sx = 0, sy = 0; if (shake) { shake--; uint32_t r0 = scatter(), r1 = scatter(); sx = (int)(r0 % 9) - 4; sy = (int)(r1 % 7) - 3; sx = sx * shake / 10; sy = sy * shake / 10; }
  int32_t bc, bs; unit16(P.bob, &bc, &bs);
  cam_x = P.x; cam_y = P.y; cam_a = (P.a + sx * 3) & 4095; horizon = SH / 2 + (P.moving ? (bs * 3 >> 14) : 0) + sy;
  view_bob_x = P.moving ? (bc * 6 >> 14) : 0; view_bob_y = P.moving ? (absi(bs) * 5 >> 14) : 0;
  unit16(cam_a, &cdir_c, &cdir_s);
  // what is under the crosshair (for its colour): the nearest foe whose width the centre line passes through, before the wall
  aim_hit = 0; int sd, wid; int32_t hx, hy, wd = cast(cam_x, cam_y, cdir_c, cdir_s, &sd, &wid, &hx, &hy);
  for (int i = 0; i < NFOE; i++) { Foe *f = &FO[i]; if (!f->alive || f->kind == F_BARREL) continue; int32_t rx = f->x - cam_x, ry = f->y - cam_y;
    int32_t along = (int32_t)(((int64_t)rx * cdir_c + (int64_t)ry * cdir_s) >> 14), side = (int32_t)(((int64_t)-rx * cdir_s + (int64_t)ry * cdir_c) >> 14);
    if (along > 0 && along < wd && absi(side) < FK[f->kind].rad) aim_hit = 1; } }
void frame(void) {
  uint32_t c0 = cyc(); f_bytes = 0; f_tiles = 0; gm_t++;
  memset(&IN, 0, sizeof IN); IN.sel = -1;
#ifdef LIVE_HANDS
  { int hand = pad_read();
    if (hand && ghost) { ghost = 0; hands_seen = 1; new_game(); }
    if (!ghost) { IN.mv = -stick_y; if (c_down) IN.st = stick_x; else IN.tn = stick_x; IN.fire = a_down; IN.cycle = b_tap; IN.dash = p_tap; }
    if (gmode == M_OVER && gm_t > 200) { if (!hands_seen || a_down) { new_game(); if (!hands_seen) gmode = M_TITLE; } }
    if (!hands_seen && gmode == M_PLAY && gm_t == 1) gmode = M_TITLE; }
#else
  if (gmode == M_OVER && gm_t > 200) new_game();
#endif
  world_step(); breathe_cyan(frame_n); camera(); cast_columns(); build_scene();
  uint32_t c1 = cyc(); wire_print = 0x811c9dc5u; c_bg = c_things = c_over = c_send = 0;
  for (int b = 0; b < NTY; b++) { band_y0 = b * TH; int s = (int)(slot_n++ & 1); OB = RB[s]; m_wait(s);
    uint32_t t0 = cyc(); background_band(); uint32_t t1 = cyc(); shades_band(); bills_band(); dots_band(); uint32_t t2 = cyc(); overlay_band(); uint32_t t3 = cyc(); send_band(b, OB); m_fence(s);
    c_bg += t1 - t0; c_things += t2 - t1; c_over += t3 - t2; c_send += cyc() - t3; }
  uint32_t c2 = cyc(); c_world = c1 - c0; c_draw = c2 - c1; frame_n++; }
static int alive_foes(void) { int n = 0; for (int i = 0; i < NFOE; i++) if (FO[i].alive && FO[i].kind != F_BARREL) n++; return n; }
static int live_motes(void) { int n = 0; for (int i = 0; i < NMOTE; i++) if (MO[i].life) n++; return n; }
#ifdef DESK
static uint32_t desk_expected_print(void) { uint32_t h = 0x811c9dc5u;
  for (int b = 0; b < NTY; b++) for (int k = 0; k < SW * TH; k++) { uint16_t v = SHOWN[b][k]; v = (uint16_t)((v >> 8) | (v << 8)); h = (h ^ (v & 255)) * 0x01000193u; h = (h ^ (v >> 8)) * 0x01000193u; } return h; }
static void desk_line(void) { printf("mode %d wave %d score %u hp %d gun %d kills %d foes %d bills %d dots %d at %d,%d", gmode, wave, (unsigned)P.score, P.hp, P.gun, P.kills, alive_foes(), bl_n, dt_n, (int)(P.x >> 16), (int)(P.y >> 16)); }
static void desk_books(void) { printf("BOOKS waves %u kills %u shots %u hurt %u deaths %u barrels %u best %u\n", (unsigned)book_waves, (unsigned)book_kills, (unsigned)book_shots, (unsigned)book_hurt, (unsigned)book_deaths, (unsigned)book_barrels, (unsigned)book_best); }
#endif
static uint32_t glass_print(void) { return wire_print; }
void setup(void) {
#if !defined(DESK) && FRAMES > 0
  Serial0.begin(921600);
#elif defined(ON_BOARD)
  Serial.begin(115200); delay(300);
#endif
  STAGE("HIGH NOON 1 boot\n");
  for (int i = 0; i < 2; i++) RB[i] = (uint16_t *)HEAP_DMA(SW * TH * 2);
  STAGE("HIGH NOON 2 strips %s\n", RB[0] && RB[1] ? "ok" : "REFUSED");
  grow_face(); grow_ring(); grow_colours(); grow_walls(); grow_sky(); grow_pictures(); grow_view(); read_world();
#ifdef LIVE_HANDS
  pad_begin();
#endif
  panel_begin();
  STAGE("HIGH NOON 3 grown: %d pictures, %u of %d picture bytes, %d refused\n", sp_n, (unsigned)sp_used, SPPOOL, sp_refused);
  new_game();
#ifdef LIVE_HANDS
  gmode = M_TITLE;
#endif
}
#ifndef DESK
static uint32_t rep_n, rep_sum, rep_max, last_us, rep_world, rep_draw;
void loop(void) {
#if FRAMES == 0 && !defined(MOUTH_FIFO)
  uint32_t now = micros(), us = last_us ? now - last_us : 0; last_us = now;
#endif
  frame();
#if FRAMES > 0
  Serial0.printf("F %u glass %08x world %u draw %u bytes %u bg %u things %u over %u send %u ", (unsigned)frame_n, (unsigned)glass_print(), (unsigned)c_world, (unsigned)c_draw, (unsigned)f_bytes,
    (unsigned)c_bg, (unsigned)c_things, (unsigned)c_over, (unsigned)c_send);
  Serial0.printf("mode %d wave %d score %u hp %d gun %d kills %d foes %d bills %d dots %d at %d,%d\n", gmode, wave, (unsigned)P.score, P.hp, P.gun, P.kills, alive_foes(), bl_n, dt_n, (int)(P.x >> 16), (int)(P.y >> 16));
  if (frame_n >= FRAMES) { m_wait_all();
    Serial0.printf("BOOKS waves %u kills %u shots %u hurt %u deaths %u barrels %u best %u\n", (unsigned)book_waves, (unsigned)book_kills, (unsigned)book_shots, (unsigned)book_hurt, (unsigned)book_deaths, (unsigned)book_barrels, (unsigned)book_best);
    Serial0.printf("STACK free %u words\n", (unsigned)uxTaskGetStackHighWaterMark(NULL)); Serial0.printf("HEAP free %u\n", (unsigned)ESP.getFreeHeap()); Serial0.printf("DONE\n");
    while (1) delay(1000); }
#elif defined(ON_BOARD)
  if (us) { rep_sum += us; if (us > rep_max) rep_max = us; rep_world += c_world; rep_draw += c_draw; }
  if (++rep_n >= 200) { Serial.printf("HIGH NOON REAL frame avg %u us worst %u us | world %u cycles, picture %u cycles | bills %d dots %d | %s\n",
      (unsigned)(rep_sum / rep_n), (unsigned)rep_max, (unsigned)(rep_world / rep_n), (unsigned)(rep_draw / rep_n), bl_n, dt_n, ghost ? "ghost" : "hands");
    rep_n = 0; rep_sum = 0; rep_max = 0; rep_world = 0; rep_draw = 0; }
#endif
}
#endif
