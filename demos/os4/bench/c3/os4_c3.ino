// RULE OS 4 for the ESP32-C3 + 2.0" ST7789 (320 x 240 across). Made from RULE OS 3 by tools/make_c3.py; what is new:
//  - THE FLUX. No budgets. Each place carries a loaded history L; every step moves its weight from the kernel's field into L; the place
//    whose next step is due soonest runs (min), and a step pushes its due mark on by weight x (C + L + drag) (+). A place that gives the
//    machine back vents a share of L; one that ends or is refused vents all of it; one past THETA reconfigures (keeps KAPPA/16, vents
//    the rest, goes to the front). Drag is the L of every other place sharing a cell with it. Books every latch: field + every L = G0.
//  - THE SAME KERNEL AS THE UNO'S OS 4 (one kernel.h, joined into both sketches): the same files give the same readings, slot for slot,
//    and the same flux, latch for latch (checked: the kernel on this chip's instruction set in Espressif's emulator == the desk).
//  - RULES, an app: the kernel's own readings by hand, kept in flash with the files. MAKE IT YOURS starts there.
// What follows is OS 3's own description: still true, except that "a budget of steps a latch" is now the flux above.
// RULE OS 3: an operating system for the ESP32-C3 + 2.0" ST7789 (320 x 240 across) whose memory says "when", not "no".
// What it is (book words throughout):
//  - PROGRAMS ARE FILES OF THE BOOK'S MOVES (OS 2's machine, the same two-byte steps, the same 32 ops, OS 2's five files run
//    unchanged). A slot is a reading (value; rate) over one shared bottom, now three 32-bit tops in lowest terms after every
//    step; a top that does not fit is refused WIDE and the slot is left alone. So ROOT now reaches the book's own fourth step,
//    665857/470832, where OS 2 had to say WIDE. Four places run at once, each a budget of steps a latch, then cut off.
//  - EVERY WINDOW'S PICTURE LIVES IN ONE ARENA KEPT BY WHENHEAP'S RULE. A window that does not fit is not refused: it is held
//    as a reading (room needed; rate of moving) and answered with when = room / rate latches, said up front on its title bar.
//    The room is made in the cheapest stretch (the least live bytes over it: a min over every stretch, the tropical rule), and
//    the blocks in the way are moved out whole at a carried rate (acc += rate; a block moves when acc covers it), so no latch
//    moves more than the rate or one block. Windows hold handles, not addresses, so a moved picture is never lost. Theta stays
//    only when no stretch can be cleared: a refusal, counted and shown.
//  - FILES MEND THEMSELVES: each is kept twice with a mark and a sum; a keeper checks one file a turn and copies a good copy over
//    a bad one. On the board they are saved to flash (EEPROM), so programs you edit survive power-off.
//  - THE GLASS IS SENT AS ITS CHANGE: the picture is one byte a dot (76,800 bytes) through a grown palette; each 32 x 16 tile has a
//    print, and only tiles whose print changed go down the wire, in runs, through FAREY's queue (the CPU never waits for the wire).
//  - IMMEDIATE BUTTONS: a button exists because its line ran this latch. The ghost hand reads the same buttons the eye sees.
//  - THE BOOKS: EST, DEL and KEEP for heap blocks (KEEP checked against the table every latch); every moved block's print is taken
//    before and after the move (BAD counts any difference); promises kept late or early are counted.
// No t, no float, no sin, no sqrt. The frame pace (30 a second) is pace only: no reading depends on it.
// Wiring: SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3. Stick VRx 0, VRy 1, press 2. Buttons A 3, B 20, C 21 (to ground).
// Hands: the stick moves the pointer; press or A clicks (hold on a title bar to drag); B closes the window on top;
//   C brings the next window to the top. Touch nothing for a while and the ghost hand takes over and plays.
// Arduino IDE: "ESP32C3 Dev Module", core 3.x, USB CDC On Boot: Enabled (Serial Monitor 115200: stages, then books every 300).
// ---- fixes (uncomment):
//#define RGB_ORDER    // red and blue swapped
//#define NO_INVERT    // colours look like a negative
//#define ROT180       // upside down
//#define SAFE_SPI     // speckles or streaks: 20 MHz wire (default 40 MHz)
//#define JOY_FLIP_X   // stick left/right backwards
//#define JOY_FLIP_Y   // stick up/down backwards
//#define JOY_SWAP     // stick axes swapped
//#define GHOST_ONLY   // the ghost always plays (no hands read)
// Bench builds only (set by the tools): DESK, MOUTH_FIFO, NULL_WIRE, FRAMES=n.
#include <stdint.h>
#include <string.h>
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
#include <EEPROM.h>
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
#define TW 32
#define TH 16
#define NTX (SW / TW)
#define NTY (SH / TH)
#define STRIP (SW * TH)

// ---------------------------------------------------------------- the shapes of things (first, so the IDE's own prototypes can name them)
#define KTOP32
#define NPL 4
#define PROC_EXTRA uint8_t win; uint32_t steps; uint8_t code[98];
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
typedef struct { int32_t o, l, id; } Blk;                                 // a block of the arena: place, length (units), handle
typedef struct { uint8_t used, kind, place, held; int16_t x, y, w, h; int32_t id; uint32_t promise, asked; } Win;
typedef struct { int jx, jy, a, b, c, sw; } Pad;
typedef struct { uint8_t win; uint32_t lab; int16_t x, y, w, h; } Seen;   // a button that was drawn last latch, where the eye (and the ghost) saw it
typedef struct { uint8_t act, kind; const char *label; int16_t a, b; } GStep;

#if defined(DESK)
#define STAGE(...) do {} while (0)
#elif FRAMES > 0
#define STAGE(...) Serial0.printf(__VA_ARGS__)
#else
#define STAGE(...) Serial.printf(__VA_ARGS__)
#endif

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

// ---------------------------------------------------------------- the glass: one byte a dot, a grown palette
static uint8_t FB[SW * SH];
static uint16_t PAL[256];
#define C_BG 0        // 0..15: the desk, top to bottom
#define C_BLACK 16
#define C_WHITE 17
#define C_TITLE 18
#define C_TITLE_DIM 19
#define C_EDGE 20
#define C_WIN 21
#define C_TEXT 22
#define C_DIM 23
#define C_BTN 24
#define C_HOT 25
#define C_RED 26
#define C_GREEN 27
#define C_GOLD 28
#define C_TEAL 29
#define C_BAR 30
#define C_SHADOW 31
#define C_INK 32      // 32..35: each place's ink
#define C_OWN 40      // 40..47: each window's colour on the heap map
static const uint8_t B4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
static void grow_palette(void) {
  for (int k = 0; k < 16; k++) PAL[C_BG + k] = glass(18 + k * 3, 28 + k * 4, 52 + k * 6);
  PAL[C_BLACK] = glass(0, 0, 0); PAL[C_WHITE] = glass(255, 255, 255); PAL[C_TITLE] = glass(40, 150, 170); PAL[C_TITLE_DIM] = glass(60, 70, 90);
  PAL[C_EDGE] = glass(10, 12, 20); PAL[C_WIN] = glass(16, 20, 30); PAL[C_TEXT] = glass(225, 235, 240); PAL[C_DIM] = glass(120, 135, 150);
  PAL[C_BTN] = glass(45, 55, 75); PAL[C_HOT] = glass(80, 110, 150); PAL[C_RED] = glass(235, 70, 60); PAL[C_GREEN] = glass(90, 220, 120);
  PAL[C_GOLD] = glass(255, 200, 80); PAL[C_TEAL] = glass(40, 220, 200); PAL[C_BAR] = glass(8, 10, 16); PAL[C_SHADOW] = glass(6, 8, 14);
  PAL[C_INK + 0] = glass(255, 210, 90); PAL[C_INK + 1] = glass(110, 230, 255); PAL[C_INK + 2] = glass(255, 130, 200); PAL[C_INK + 3] = glass(150, 255, 140);
  for (int k = 0; k < 8; k++) { uint32_t h = (uint32_t)(k + 3) * 2654435761u; PAL[C_OWN + k] = glass(90 + (int32_t)(h >> 25), 90 + (int32_t)((h >> 17) & 127), 110 + (int32_t)((h >> 9) & 127)); } }
// a surface: a byte picture of its own size (the glass itself, or a window's picture in the arena)
static uint8_t *s_p; static int s_w, s_h;
static void on(uint8_t *p, int w, int h) { s_p = p; s_w = w; s_h = h; }
static void dot(int x, int y, uint8_t c) { if ((unsigned)x < (unsigned)s_w && (unsigned)y < (unsigned)s_h) s_p[y * s_w + x] = c; }
static void fill(int x, int y, int w, int h, uint8_t c) {
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > s_w ? s_w : x + w, y1 = y + h > s_h ? s_h : y + h;
  for (int yy = y0; yy < y1; yy++) memset(s_p + yy * s_w + x0, c, x1 > x0 ? (size_t)(x1 - x0) : 0); }
static void frame_box(int x, int y, int w, int h, uint8_t c) { fill(x, y, w, 1, c); fill(x, y + h - 1, w, 1, c); fill(x, y, 1, h, c); fill(x + w - 1, y, 1, h, c); }
static void line(int x0, int y0, int x1, int y1, uint8_t c, int fat) {   // the carry in two directions
  if (x0 < -400 || x0 > 800 || x1 < -400 || x1 > 800 || y0 < -400 || y0 > 800 || y1 < -400 || y1 > 800) return;
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1, sx = x1 > x0 ? 1 : -1, sy = y1 > y0 ? 1 : -1, e = dx - dy;
  for (;;) { if (fat) { dot(x0, y0, c); dot(x0 + 1, y0, c); dot(x0, y0 + 1, c); dot(x0 + 1, y0 + 1, c); } else dot(x0, y0, c);
    if (x0 == x1 && y0 == y1) break; int e2 = 2 * e; if (e2 > -dy) { e -= dy; x0 += sx; } if (e2 < dx) { e += dx; y0 += sy; } } }

// ---------------------------------------------------------------- the face: 5x7 letters (ACE's), at any whole scale
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
static int text(int x, int y, const char *s, uint8_t c) {   // returns where the next letter would go
  for (; *s; s++) { int ch = *s; if (ch >= 'a' && ch <= 'z') ch -= 32; int g = GLYPH[ch & 127];
    if (g) for (int r = 0; r < 7; r++) { uint8_t bits = FACE[g - 1][r]; for (int k = 0; k < 5; k++) if (bits >> (4 - k) & 1) dot(x + k, y + r, c); }
    x += 6; }
  return x; }
static int number(int x, int y, int64_t v, uint8_t c) { char b[24]; int n = 0; uint64_t u = (uint64_t)(v < 0 ? -v : v); if (v < 0) x = text(x, y, "-", c);
  do { b[n++] = (char)('0' + u % 10); u /= 10; } while (u); char o[24]; for (int k = 0; k < n; k++) o[k] = b[n - 1 - k]; o[n] = 0; return text(x, y, o, c); }

// ---------------------------------------------------------------- the hands: the stick and buttons, or the ghost
static int32_t ptr_x = 160 * 16, ptr_y = 120 * 16;         // the pointer, in sixteenths of a dot
static int down, was_down, clicked, b_tap, c_tap, ghost = 1;
static uint32_t latch_n;
static Pad pad;
#if defined(ON_BOARD) && !defined(GHOST_ONLY)
#define LIVE_HANDS 1
static int cx0 = 2048, cy0 = 2048, idle_n, hands_seen, lb = 1, lc = 1;
static void pad_begin(void) {
  pinMode(PIN_JSW, INPUT_PULLUP); pinMode(PIN_A, INPUT_PULLUP); pinMode(PIN_B, INPUT_PULLUP); pinMode(PIN_C, INPUT_PULLUP);
  analogReadResolution(12); int32_t sx = 0, sy = 0;
  for (int k = 0; k < 32; k++) { sx += analogRead(PIN_VRX); sy += analogRead(PIN_VRY); }
  cx0 = (int)(sx >> 5); cy0 = (int)(sy >> 5); }
static void pad_read(Pad *p) {
  int rx = ((analogRead(PIN_VRX) + analogRead(PIN_VRX)) >> 1) - cx0, ry = ((analogRead(PIN_VRY) + analogRead(PIN_VRY)) >> 1) - cy0;
#ifdef JOY_SWAP
  { int t = rx; rx = ry; ry = t; }
#endif
#ifdef JOY_FLIP_X
  rx = -rx;
#endif
#ifdef JOY_FLIP_Y
  ry = -ry;
#endif
  rx /= 14; ry /= 14; rx = clampi(rx, -127, 127); ry = clampi(ry, -127, 127);
  if (rx > -14 && rx < 14) rx = 0; if (ry > -14 && ry < 14) ry = 0;
  p->jx = rx; p->jy = ry; p->a = !digitalRead(PIN_A); p->b = !digitalRead(PIN_B); p->c = !digitalRead(PIN_C); p->sw = !digitalRead(PIN_JSW); }
#endif

// ---------------------------------------------------------------- the arena: WHENHEAP's rule, for window pictures
#define UB 256                       // a unit: 256 bytes
#define HU 300                       // 300 units: 76,800 bytes for every window's picture (less than all of them want at once)
#define HCAP 24
#define RATE 2                       // units moved a latch, at most (a block bigger than this waits for the carry to cover it)
static uint8_t *ARENA;
static Blk HB[HCAP]; static int hn;
static int32_t h_acc, h_need, h_w0, h_w1, h_id, h_cost;   // the held request: room needed, its stretch, whose it is
static uint32_t h_since, h_promise;
static uint32_t L_est, L_del, keep_bad, bad_moves, refused, held_n, late_n, early_n, moved_all, worst_move, moved_now, next_id = 1;
static void h_insert(int32_t o, int32_t l, int32_t id) { int k = hn; while (k > 0 && HB[k - 1].o > o) { HB[k] = HB[k - 1]; k--; } HB[k].o = o; HB[k].l = l; HB[k].id = id; hn++; }
static void h_remove(int k) { for (; k < hn - 1; k++) HB[k] = HB[k + 1]; hn--; }
static uint8_t *h_at(int32_t id) { for (int k = 0; k < hn; k++) if (HB[k].id == id) return ARENA + HB[k].o * UB; return 0; }   // a handle names a block, wherever it sits now
static int32_t h_free(void) { int32_t f = HU; for (int k = 0; k < hn; k++) f -= HB[k].l; return f; }
static int32_t gap_in(const Blk *T, int tn, int32_t l, int32_t w0, int32_t w1) {   // first fit in a table, keeping clear of [w0, w1)
  int32_t p = 0;
  for (int k = 0; k <= tn; k++) { int32_t e = k < tn ? T[k].o : HU, s = p;
    if (s < w1 && s + l > w0) s = w1;
    if (e - s >= l && !(s < w1 && s + l > w0)) return s;
    if (k < tn) p = T[k].o + T[k].l; }
  return -1; }
static int32_t find_gap(int32_t l, int32_t w0, int32_t w1) { return gap_in(HB, hn, l, w0, w1); }
static void t_insert(Blk *T, int *tn, Blk b) { int k = *tn; while (k > 0 && T[k - 1].o > b.o) { T[k] = T[k - 1]; k--; } T[k] = b; (*tn)++; }
static void t_remove(Blk *T, int *tn, int k) { for (; k < *tn - 1; k++) T[k] = T[k + 1]; (*tn)--; }
static int can_clear(int32_t x, int32_t n) {                // a dry run on a copy of the table: can every block in [x, x+n) be put somewhere else?
  Blk T[HCAP]; int tn = hn; memcpy(T, HB, sizeof(Blk) * (size_t)hn);
  for (;;) { int k = -1; for (int j = 0; j < tn; j++) if (T[j].o < x + n && T[j].o + T[j].l > x) { k = j; break; }
    if (k < 0) return 1;
    int32_t to = gap_in(T, tn, T[k].l, x, x + n); if (to < 0) return 0;
    Blk b = T[k]; t_remove(T, &tn, k); b.o = to; t_insert(T, &tn, b); } }
static uint32_t print_of(const uint8_t *p, int32_t n) { uint32_t h = 0x811c9dc5u; for (int32_t k = 0; k < n; k++) h = (h ^ p[k]) * 0x01000193u; return h; }
static void move_block(int k, int32_t to) {                 // a block moved whole; its print before and after must agree
  Blk b = HB[k]; uint32_t before = print_of(ARENA + b.o * UB, b.l * UB);
  memmove(ARENA + to * UB, ARENA + b.o * UB, (size_t)b.l * UB);
  if (print_of(ARENA + to * UB, b.l * UB) != before) bad_moves++;
  moved_now += (uint32_t)b.l; moved_all += (uint32_t)b.l; h_remove(k); h_insert(to, b.l, b.id); }
static int32_t plan(int32_t n, int32_t *cost) {              // the cheapest stretch that can be cleared: least live units over [x, x+n), for x = 0 and every block's end
  int32_t best = -1, bc = 0x7fffffff;
  for (int k = -1; k < hn; k++) { int32_t x = k < 0 ? 0 : HB[k].o + HB[k].l; if (x + n > HU) continue;
    int32_t c = 0; for (int j = 0; j < hn; j++) if (HB[j].o < x + n && HB[j].o + HB[j].l > x) c += HB[j].l;
    if (c < bc && can_clear(x, n)) { bc = c; best = x; } }
  *cost = bc; return best; }
// ask for n units for handle id: 1 = placed now, 0 = held (a promise is said), -1 = refused (theta)
static int h_ask(int32_t n, int32_t id) {
#ifdef DESK_TRACE
  printf("ASK latch %u units %d id %d free %d held %d\n", (unsigned)latch_n, (int)n, (int)id, (int)h_free(), (int)(h_need != 0));
#endif
  int32_t o = find_gap(n, h_need ? h_w0 : 0, h_need ? h_w1 : 0);
  if (o >= 0) { h_insert(o, n, id); L_est++; return 1; }
  if (h_need || n > h_free()) { refused++; return -1; }  // one held at a time, and the room must exist in all: else theta
  int32_t c, x = plan(n, &c); if (x < 0) { refused++; return -1; }
  h_need = n; h_id = id; h_w0 = x; h_w1 = x + n; h_cost = c; h_acc = 0; h_since = latch_n; h_promise = (uint32_t)((c + RATE - 1) / RATE); held_n++;
  return 0; }
static void h_drop(int32_t id) {
  if (h_need && h_id == id) { h_need = 0; return; }        // a held request let go before it was served
  for (int k = 0; k < hn; k++) if (HB[k].id == id) { h_remove(k); L_del++; return; } }
static int32_t h_turn(void) {                               // the arena's turn: move blocks out of the stretch at the carried rate; returns a served handle or 0
  moved_now = 0; if (!h_need) { h_acc = 0; return 0; }
  h_acc += RATE;
  for (;;) {
    int k = -1; for (int j = 0; j < hn; j++) if (HB[j].o < h_w1 && HB[j].o + HB[j].l > h_w0) { k = j; break; }
    if (k < 0) {                                            // the stretch is clear: the promise is kept
      uint32_t waited = latch_n - h_since; if (waited > h_promise) late_n++; else if (waited < h_promise) early_n++;
      int32_t id = h_id; h_insert(h_w0, h_need, id); L_est++; h_need = 0; h_acc = 0;
      if (moved_now > worst_move) worst_move = moved_now; return id; }
    if (h_acc < HB[k].l) break;
    int32_t to = find_gap(HB[k].l, h_w0, h_w1);
    if (to < 0) { refused++; h_need = 0; h_acc = 0; if (moved_now > worst_move) worst_move = moved_now; return -h_id; }   // nowhere to put it after all: theta, said
    h_acc -= HB[k].l; move_block(k, to); }
  if (moved_now > worst_move) worst_move = moved_now;
  return 0; }

// ---------------------------------------------------------------- files: kept twice, each with a mark and a sum; a keeper mends
#define NF 8
#define FB_BYTES 98                  // OS 2's file: byte 0 the steps, then two bytes a step (48 steps)
#define COPY (FB_BYTES + 3)          // mark, bytes, sum (two bytes)
#define FS_EMPTY 0
#define FS_OK 1
#define FS_BAD_A 2
#define FS_BAD_B 3
#define FS_LOST 4
static uint8_t DISK[NF][2][COPY];
static uint32_t mends, saves;
static const char *FNAME[NF] = {"BOUNCE", "ROOT", "SLOPE", "TURN", "MEET", "E", "CUBE", "NEW"};
static const uint8_t DEMO[NF][FB_BYTES] = {
// ---- os3_asm.py block (OS 2's five files, unchanged, then three new) ----
  {33, 8, 3, 24, 2, 9, 5, 25, 1, 10, 1, 11, 58, 12, 23, 15, 3, 168, 0, 96, 0, 120, 7, 104, 0, 123, 0, 104, 0, 97, 0, 121, 7, 105, 0, 124, 1, 105, 0, 37, 0, 53, 2, 38, 0, 46, 2, 189, 1, 198, 1, 37, 1, 53, 2, 38, 1, 46, 2, 184, 5, 192, 6, 160, 0, 152, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {25, 8, 1, 9, 2, 15, 2, 200, 0, 34, 1, 66, 0, 40, 2, 64, 7, 200, 4, 34, 1, 66, 0, 40, 2, 64, 7, 200, 16, 34, 1, 66, 0, 40, 2, 64, 7, 200, 32, 34, 1, 66, 0, 40, 2, 64, 7, 200, 48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {39, 13, 7, 14, 100, 61, 6, 15, 9, 63, 6, 14, 13, 16, 226, 33, 0, 57, 0, 57, 0, 34, 0, 58, 5, 49, 2, 65, 7, 34, 1, 74, 0, 89, 1, 11, 2, 58, 3, 91, 0, 12, 31, 43, 4, 36, 6, 52, 1, 9, 2, 51, 1, 44, 2, 187, 4, 43, 1, 43, 1, 52, 2, 52, 2, 195, 4, 9, 4, 40, 1, 9, 31, 120, 1, 152, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {31, 8, 248, 24, 1, 13, 8, 14, 248, 15, 12, 168, 0, 89, 0, 34, 5, 209, 2, 201, 48, 35, 1, 59, 1, 36, 2, 60, 2, 43, 4, 203, 0, 57, 7, 58, 7, 11, 31, 12, 13, 187, 4, 43, 1, 52, 2, 195, 4, 96, 0, 128, 5, 104, 0, 128, 6, 104, 0, 248, 1, 152, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {31, 8, 1, 24, 2, 9, 61, 25, 253, 14, 1, 15, 61, 13, 10, 12, 16, 168, 0, 34, 1, 50, 0, 35, 2, 83, 2, 202, 0, 203, 8, 184, 5, 192, 4, 185, 5, 193, 4, 96, 0, 97, 0, 128, 1, 104, 0, 128, 1, 105, 0, 128, 6, 104, 0, 129, 7, 105, 0, 248, 2, 152, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {14, 8, 1, 9, 1, 10, 1, 11, 1, 14, 7, 65, 2, 40, 1, 42, 3, 201, 16, 200, 32, 130, 6, 200, 0, 160, 0, 152, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {18, 16, 2, 33, 0, 57, 0, 57, 0, 201, 0, 34, 1, 74, 0, 202, 4, 35, 0, 43, 0, 43, 1, 12, 1, 43, 4, 203, 16, 36, 3, 76, 0, 204, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {3, 8, 1, 200, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
// ---- end of os3_asm.py block ----
};
static uint16_t f_sum(const uint8_t *b) { uint16_t s = 0x1D0F; for (int k = 0; k < FB_BYTES; k++) s = (uint16_t)((s << 1 | s >> 15) ^ b[k]); return s; }
static int copy_ok(int f, int c) { const uint8_t *p = DISK[f][c]; return p[0] == 'R' && f_sum(p + 1) == (uint16_t)(p[FB_BYTES + 1] | p[FB_BYTES + 2] << 8); }
static int file_state(int f) { int a = copy_ok(f, 0), b = copy_ok(f, 1); if (a && b) return FS_OK; if (a) return FS_BAD_B; if (b) return FS_BAD_A;
  for (int k = 0; k < COPY; k++) if (DISK[f][0][k] || DISK[f][1][k]) return FS_LOST; return FS_EMPTY; }
#define RULES_AT (NF * 2 * COPY)               // after the files: two copies of mark 0xAA, the 12 bytes of the rules, a sum
static uint8_t RULEDISK[2][14];
static void disk_save(void) {
#ifdef ON_BOARD
  for (int f = 0; f < NF; f++) for (int c = 0; c < 2; c++) for (int k = 0; k < COPY; k++) EEPROM.write((f * 2 + c) * COPY + k, DISK[f][c][k]);
  for (int c = 0; c < 2; c++) for (int k = 0; k < 14; k++) EEPROM.write(RULES_AT + 14 * c + k, RULEDISK[c][k]);
  EEPROM.commit();
#endif
  saves++; }
static void file_write(int f, const uint8_t *b) {          // copy A whole, then copy B: one copy is always whole
  for (int c = 0; c < 2; c++) { uint8_t *p = DISK[f][c]; p[0] = 'R'; memcpy(p + 1, b, FB_BYTES); uint16_t s = f_sum(b); p[FB_BYTES + 1] = (uint8_t)s; p[FB_BYTES + 2] = (uint8_t)(s >> 8); }
  disk_save(); }
static const uint8_t *file_bytes(int f) { if (copy_ok(f, 0)) return DISK[f][0] + 1; if (copy_ok(f, 1)) return DISK[f][1] + 1; return 0; }
static void file_spoil(int f) { DISK[f][0][1 + (int)(scatter() % FB_BYTES)] ^= 0x5A; disk_save(); }   // one byte of copy A turned over
static int keeper_on = 1, keep_f;
static void keeper_turn(void) {                            // one file a turn: a good copy over a bad one
  if (!keeper_on) return; int f = keep_f; keep_f = (keep_f + 1) % NF; int st = file_state(f);
  if (st == FS_BAD_A) { memcpy(DISK[f][0], DISK[f][1], COPY); mends++; disk_save(); }
  if (st == FS_BAD_B) { memcpy(DISK[f][1], DISK[f][0], COPY); mends++; disk_save(); } }
static void disk_begin(void) {
#ifdef ON_BOARD
  EEPROM.begin(NF * 2 * COPY + 28);
  for (int f = 0; f < NF; f++) for (int c = 0; c < 2; c++) for (int k = 0; k < COPY; k++) DISK[f][c][k] = EEPROM.read((f * 2 + c) * COPY + k);
  for (int c = 0; c < 2; c++) for (int k = 0; k < 14; k++) RULEDISK[c][k] = EEPROM.read(RULES_AT + 14 * c + k);
#endif
  for (int f = 0; f < NF; f++) { int st = file_state(f); if (st == FS_EMPTY || st == FS_LOST) file_write(f, DEMO[f]); } }


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

static const char OP_NAME[32][6] = { "END", "SET", "ISO", "SEED", "MOV", "ADD", "SUB", "MIX", "DIV", "RATE", "WHEN", "VAL", "LATCH", "FLIP", "LAND", "IFLT", "IFEQ", "IFBTN", "LOOP", "JMP", "SHOW", "CLS", "DOT", "PEN", "LINE", "NUM", "HEAD", "STICK", "RAND", "PUT", "GET", "WAIT" };
static const uint8_t OP_KIND[32] = { 0, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 3, 2, 2, 0, 5, 4, 0, 0, 2, 2, 2, 3, 2, 2, 3, 3, 3, 6 };
#define NP NPL
#define PW 124                       // a place's picture: OS 2's 62 x 26, at two dots a dot
#define PH 60
static uint32_t steps_all;
static Win WN[8]; static uint8_t ZO[8]; static int nz;     // windows, and their order bottom to top
static uint8_t *pic_of(int w) { return WN[w].id && !WN[w].held ? h_at(WN[w].id) : 0; }
static uint8_t *pw_p;                                      // the place's picture this step
static void pw_dot(int32_t x, int32_t y, uint8_t c) { if (!pw_p || x < 0 || x > 61 || y < 0 || y > 25) return; uint8_t *q = pw_p + (y * 2) * PW + x * 2; q[0] = c; q[1] = c; q[PW] = c; q[PW + 1] = c; }
static void pw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c) {   // the carry, landing dots two by two
  if (x0 < -99 || x0 > 160 || x1 < -99 || x1 > 160 || y0 < -99 || y0 > 125 || y1 < -99 || y1 > 125) return;
  int32_t dx = x1 - x0, dy = y1 - y0, sx = dx > 0 ? 1 : dx < 0 ? -1 : 0, sy = dy > 0 ? 1 : dy < 0 ? -1 : 0, ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
  int steep = ay > ax; int32_t lng = steep ? ay : ax, sht = steep ? ax : ay, acc = lng >> 1;
  for (int32_t k = 0; k <= lng; k++) { pw_dot(x0, y0, c); if (steep) y0 += sy; else x0 += sx; acc += sht; if (acc >= lng) { acc -= lng; if (steep) x0 += sx; else y0 += sy; } } }
static void pw_num(int x, int y, const Rd *a, uint8_t c) {   // a reading's value as a fraction, in the face, at the picture's own dots
  if (!pw_p) return; on(pw_p, PW, PH); fill(x, y, PW - x, 8, C_WIN); x = number(x, y, a->v, c); if (a->d != 1) { x = text(x, y, "/", c); number(x, y, a->d, c); } }
static uint8_t rsum(const uint8_t *b) { uint8_t s = 0x5A; for (int k = 0; k < 12; k++) s = (uint8_t)(((s << 1) | (s >> 7)) ^ b[k]); return s; }
static void rules_save(void) { for (int c = 0; c < 2; c++) { RULEDISK[c][0] = 0xAA; memcpy(RULEDISK[c] + 1, &rules, 12); RULEDISK[c][13] = rsum(RULEDISK[c] + 1); } disk_save(); }
static int rules_load(void) { for (int c = 0; c < 2; c++) if (RULEDISK[c][0] == 0xAA && RULEDISK[c][13] == rsum(RULEDISK[c] + 1)) { memcpy(&rules, RULEDISK[c] + 1, 12); field = rules.G0; return 1; } return 0; }
static uint8_t pw_ink, said[NP];
// ---- the hooks: how a step reaches this board
static uint8_t k_code(Proc *p, uint8_t i) { return p->code[i]; }
static void k_enter(uint8_t s) { pw_p = pic_of(proc[s].win); pw_ink = (uint8_t)(C_INK + s); }
static void k_cls(void) { if (pw_p) memset(pw_p, C_WIN, PW * PH); }
static void k_dot(int32_t x, int32_t y) { pw_dot(x, y, pw_ink); }
static void k_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1) { pw_line(x0, y0, x1, y1, pw_ink); }
static void k_num(const Rd *a, uint8_t at) { pw_num((at & 15) * 8, (at >> 4) * 12, a, pw_ink); }
static void k_stick(Rd *a, Rd *b) { a->v = pad.jx / 32; a->r = 0; a->d = 1; b->v = pad.jy / 32; b->r = 0; b->d = 1; }
static uint8_t k_btn(void) { return (uint8_t)down; }
static int32_t k_rand(int32_t n) { return (int32_t)(scatter() % (uint32_t)n); }
static void run_places(void) {                             // one latch of the flux for every place, then what a refusal says
  for (int s = 0; s < NP; s++) { Proc *p = &proc[s]; if (p->state == ST_WAIT && pic_of(p->win)) { p->state = ST_RUN; memset(pic_of(p->win), C_WIN, PW * PH); } }   // the promise came in
  k_latch(); pw_p = 0; steps_all = k_steps;
  for (int s = 0; s < NP; s++) { Proc *p = &proc[s]; p->steps += flow[s].took;
    if (p->state >= ST_THETA && !said[s] && pic_of(p->win)) { uint8_t *q = pic_of(p->win); on(q, PW, PH); fill(0, PH - 9, PW, 9, C_WIN);
      int x = text(2, PH - 8, p->state == ST_THETA ? "THETA" : p->state == ST_WIDE ? "WIDE" : "LOST", C_RED); number(x + 6, PH - 8, p->pc, C_RED); said[s] = 1; }
    if (p->state < ST_THETA) said[s] = 0; } }

// ---------------------------------------------------------------- windows
#define K_FILES 1
#define K_TASKS 2
#define K_HEAP 3
#define K_EDIT 4
#define K_PLACE 5
#define K_RULES 6
#define TITLE_H 11
static const int16_t KW[7] = {0, 150, 164, 196, 182, PW, 150}, KH[7] = {0, 118, 92, 92, 110, PH, 86};
static const char *KNAME[7] = {"", "FILES", "TASKS", "HEAP", "EDIT", "", "RULES"};
static int cascade;
static int theta_kind, theta_left, want_rules;
static int win_units(int w) { int32_t b = (int32_t)WN[w].w * WN[w].h; return (int)((b + UB - 1) / UB); }
static void z_top(int w) { int k = 0; while (k < nz && ZO[k] != w) k++; if (k == nz) return; for (; k < nz - 1; k++) ZO[k] = ZO[k + 1]; ZO[nz - 1] = (uint8_t)w; }
static int win_find(int kind, int place) { for (int w = 0; w < 8; w++) if (WN[w].used && WN[w].kind == kind && (kind != K_PLACE || WN[w].place == place)) return w; return -1; }
static int win_open(int kind, int place) {
  int w = win_find(kind, place); if (w >= 0) { z_top(w); return w; }
  for (w = 0; w < 8 && WN[w].used; w++) {}
  if (w == 8) { refused++; return -1; }
  Win *n = &WN[w]; memset(n, 0, sizeof *n); n->used = 1; n->kind = (uint8_t)kind; n->place = (uint8_t)place; n->w = KW[kind]; n->h = KH[kind];
  static const int16_t PX[NP] = {194, 194, 4, 130}, PY[NP] = {14, 88, 150, 150};   // each place has its own spot; the apps fall in a cascade
  if (kind == K_PLACE) { n->x = PX[place]; n->y = PY[place]; } else if (kind == K_FILES) { n->x = 4; n->y = 14; }
  else { n->x = (int16_t)(40 + (cascade % 4) * 24); n->y = (int16_t)(26 + (cascade % 4) * 12); cascade++; }
  if (n->x + n->w + 2 > SW) n->x = (int16_t)(SW - n->w - 2); if (n->y + n->h + TITLE_H + 2 > SH - 18) n->y = (int16_t)(SH - 18 - n->h - TITLE_H - 2);
  n->id = (int32_t)next_id++; n->asked = latch_n;
  int got = h_ask(win_units(w), n->id);
  if (got < 0) { n->used = 0; theta_kind = kind; theta_left = 90; return -1; }   // refused: no stretch can be cleared (counted, and said on the bar)
  n->held = got == 0; n->promise = got == 0 ? h_promise : 0;
  if (!n->held) memset(h_at(n->id), C_WIN, (size_t)n->w * n->h);
  ZO[nz++] = (uint8_t)w; return w; }
static void place_kill(int s) { Proc *p = &proc[s]; k_release((uint8_t)s); memset(p, 0, sizeof *p); }
static void win_close(int w) {
  if (w < 0 || !WN[w].used) return;
  if (WN[w].kind == K_PLACE) place_kill(WN[w].place);
  h_drop(WN[w].id); WN[w].used = 0;
  int k = 0; while (k < nz && ZO[k] != w) k++; for (; k < nz - 1; k++) ZO[k] = ZO[k + 1]; if (k < nz) nz--; }
static void served(int32_t id) { for (int w = 0; w < 8; w++) if (WN[w].used && WN[w].id == id && WN[w].held) { WN[w].held = 0; memset(h_at(id), C_WIN, (size_t)WN[w].w * WN[w].h); } }
static void place_start(int s, int f) {                    // load file f into place s; its window asks the arena for its picture
  const uint8_t *b = file_bytes(f); place_kill(s); if (!b) return;
  Proc *p = &proc[s]; memcpy(p->code, b, FB_BYTES); p->n = p->code[0] > 48 ? 48 : p->code[0]; p->file = (uint8_t)f;
  for (int i = 0; i < 8; i++) p->reg[i].d = 1;
  int w = win_open(K_PLACE, s); if (w < 0) { p->state = ST_FREE; return; }
  p->win = (uint8_t)w; p->state = WN[w].held ? ST_WAIT : ST_RUN; }
static int spawn(int f) {                                  // run file f: where it already runs, else a free place, else a finished one
  int s = -1;
  for (int i = 0; i < NP; i++) if (proc[i].state != ST_FREE && proc[i].file == f) { s = i; break; }
  if (s < 0) for (int i = 0; i < NP; i++) if (proc[i].state == ST_FREE) { s = i; break; }
  if (s < 0) for (int i = 0; i < NP; i++) if (proc[i].state >= ST_END) { s = i; break; }
  if (s < 0) return -1;
  int w = win_find(K_PLACE, s); if (w >= 0) { proc[s].win = (uint8_t)w; const uint8_t *b = file_bytes(f); if (!b) return -1;
    Proc *p = &proc[s]; k_release((uint8_t)s); memset(p, 0, sizeof *p); memcpy(p->code, b, FB_BYTES); p->n = p->code[0] > 48 ? 48 : p->code[0]; p->file = (uint8_t)f; p->win = (uint8_t)w;
    for (int i = 0; i < 8; i++) p->reg[i].d = 1; p->state = WN[w].held ? ST_WAIT : ST_RUN; if (!WN[w].held) memset(h_at(WN[w].id), C_WIN, PW * PH); z_top(w); }
  else place_start(s, f);
  return s; }

// ---------------------------------------------------------------- immediate buttons, and what the eye saw last latch
static Seen SEEN[2][96]; static int nseen[2], seen_now;
static int cur_win;                                        // whose picture is being drawn (0..7), 8 = the desk
static int lpx, lpy, l_click;                              // the pointer in this surface's dots, and whether a click is for it
static int button(int x, int y, int w, int h, const char *label, int lit) {
  int over = lpx >= x && lpx < x + w && lpy >= y && lpy < y + h;
  fill(x, y, w, h, lit ? C_TITLE : over ? C_HOT : C_BTN); frame_box(x, y, w, h, C_EDGE);
  int tw = (int)strlen(label) * 6 - 1; text(x + (w - tw) / 2, y + (h - 7) / 2, label, C_TEXT);
  if (nseen[seen_now] < 96) { Seen *e = &SEEN[seen_now][nseen[seen_now]++]; e->win = (uint8_t)cur_win; e->lab = name_of(label); e->x = (int16_t)x; e->y = (int16_t)y; e->w = (int16_t)w; e->h = (int16_t)h;
    if (cur_win < 8) { e->x = (int16_t)(e->x + WN[cur_win].x + 1); e->y = (int16_t)(e->y + WN[cur_win].y + TITLE_H); } }
  return over && l_click; }

// ---------------------------------------------------------------- the apps: each draws its own picture and answers its own clicks
static int file_sel = 1, task_sel = 0, ed_file = 1, ed_sel = 0, ed_top = 0, ed_loaded = -1;
static uint8_t ed[FB_BYTES];
static const char *SNAME[8] = {"FREE", "RUN", "STOP", "WAIT", "END", "THETA", "WIDE", "LOST"};
static void app_files(void) {
  fill(0, 0, s_w, s_h, C_WIN);
  for (int f = 0; f < NF; f++) { int y = 2 + f * 11, st = file_state(f);
    if (button(2, y, 60, 10, FNAME[f], f == file_sel)) file_sel = f;
    const char *w = st == FS_OK ? "OK" : st == FS_BAD_A ? "A BAD" : st == FS_BAD_B ? "B BAD" : st == FS_LOST ? "LOST" : "EMPTY";
    text(66, y + 2, w, st == FS_OK ? C_GREEN : st == FS_EMPTY ? C_DIM : C_RED); const uint8_t *b = file_bytes(f); if (b) { int x = number(108, y + 2, b[0], C_DIM); text(x, y + 2, "ST", C_DIM); } }
  int y = 92;
  if (button(2, y, 34, 11, "RUN", 0)) spawn(file_sel);
  if (button(38, y, 34, 11, "EDIT", 0)) { ed_file = file_sel; ed_loaded = -1; int w = win_open(K_EDIT, 0); (void)w; }
  if (button(74, y, 36, 11, "SPOIL", 0)) file_spoil(file_sel);
  if (button(112, y, 36, 11, "DEMO", 0)) file_write(file_sel, DEMO[file_sel]);
  if (button(2, y + 13, 70, 11, keeper_on ? "KEEPER ON" : "KEEPER OFF", keeper_on)) keeper_on = !keeper_on;
  int x = text(76, y + 15, "MENDS ", C_DIM); number(x, y + 15, mends, C_TEXT); }
static void app_tasks(void) {
  fill(0, 0, s_w, s_h, C_WIN);
  for (int s = 0; s < NP; s++) { Proc *p = &proc[s]; int y = 2 + s * 13; char lab[3] = {(char)('1' + s), 0, 0};
    if (button(2, y, 14, 11, lab, s == task_sel)) task_sel = s;
    text(20, y + 2, p->state == ST_FREE ? "-" : FNAME[p->file], C_INK + s);
    text(64, y + 2, SNAME[p->state], p->state >= ST_THETA ? C_RED : p->state == ST_RUN ? C_GREEN : C_DIM);
    int x = text(106, y + 2, "L", C_DIM); x = number(x, y + 2, flow[s].L, C_TEXT); number(x + 6, y + 2, flow[s].took, C_DIM); }
  int y = 56; Proc *p = &proc[task_sel];
  if (button(2, y, 38, 11, p->state == ST_STOP ? "GO" : "STOP", 0)) { if (p->state == ST_RUN) p->state = ST_STOP; else if (p->state == ST_STOP) p->state = ST_RUN; }
  if (button(42, y, 38, 11, "KILL", 0)) { int w = win_find(K_PLACE, task_sel); if (w >= 0) win_close(w); else place_kill(task_sel); }
  if (button(82, y, 38, 11, "VENT", 0)) k_vent((uint8_t)task_sel, 16);
  if (button(122, y, 38, 11, "RULES", 0)) want_rules = 1;   // TASKS gives its room to RULES, between frames
  int x = text(2, 74, "FIELD ", C_DIM); x = number(x, 74, field, C_TEXT); x = text(x, 74, "/", C_DIM); x = number(x, 74, rules.G0, C_DIM); x = text(x + 6, 74, "RECON ", C_DIM); number(x, 74, k_recon, C_TEXT);
  x = text(2, 83, "CELL A ", C_DIM); x = number(x, 83, cell[0].v, C_TEXT); if (cell[0].d != 1) { x = text(x, 83, "/", C_TEXT); number(x, 83, cell[0].d, C_TEXT); } }
static int rule_sel = 3;
static void rules_save(void);
static void app_rules(void) {                              // the kernel's own readings, by hand: each with - and +; SAVE keeps them with the files
  fill(0, 0, s_w, s_h, C_WIN);
  static const char *RN[7] = {"C", "THETA", "G0", "S", "KAPPA", "VENT", "DRAG"};
  for (int i = 0; i < 7; i++) { int y = 2 + i * 11; uint32_t v = i == 0 ? rules.C : i == 1 ? rules.THETA : i == 2 ? rules.G0 : i == 3 ? rules.S : i == 4 ? rules.KAPPA : i == 5 ? rules.VENT : rules.DRAG;
    if (button(2, y, 34, 10, RN[i], i == rule_sel)) rule_sel = i; number(39, y + 2, v, C_TEXT); }
  int d = button(104, 2, 20, 12, "-", 0) ? -1 : button(126, 2, 20, 12, "+", 0) ? 1 : 0, i = rule_sel;
  if (d) { if (i == 0) { int v = rules.C + 4 * d; if (v >= 4 && v <= 252) rules.C = (uint16_t)v; }
    if (i == 1) { int v = rules.THETA + 100 * d; if (v >= 100 && v <= 30000) rules.THETA = (uint16_t)v; }
    if (i == 2) { int v = rules.G0 + 500 * d; if (v >= 500 && v <= 60000) k_set_g0((uint16_t)v); }
    if (i == 3) { int v = rules.S + 250 * d; if (v >= 250 && v <= 12000) rules.S = (uint16_t)v; }
    if (i == 4) { int v = rules.KAPPA + d; if (v >= 0 && v <= 15) rules.KAPPA = (uint8_t)v; }
    if (i == 5) { int v = rules.VENT + d; if (v >= 0 && v <= 15) rules.VENT = (uint8_t)v; }
    if (i == 6) { int v = rules.DRAG + d; if (v >= 0 && v <= 15) rules.DRAG = (uint8_t)v; } }
  if (button(74, 18, 72, 12, "SAVE", 0)) rules_save();
  if (button(74, 32, 72, 12, "FIRST", 0)) { Rules f; k_rules_first(&f); uint16_t g = f.G0; f.G0 = rules.G0; f.S = 3000; rules = f; k_set_g0(g); }
  int x = text(74, 50, "BOOKS ", C_DIM); text(x, 50, k_books() ? "SHUT" : "OPEN", k_books() ? C_GREEN : C_RED);
  x = text(74, 62, "RECON ", C_DIM); number(x, 62, k_recon, C_TEXT);
  x = text(74, 74, "FIELD ", C_DIM); number(x, 74, field, C_TEXT); }
static void app_heap(void) {                               // the arena as it is: whose each unit is, the held stretch, the books
  fill(0, 0, s_w, s_h, C_WIN);
  for (int u = 0; u < HU; u++) { int x = 2 + (u % 48) * 4, y = 2 + (u / 48) * 6; uint8_t c = C_SHADOW;
    for (int k = 0; k < hn; k++) if (u >= HB[k].o && u < HB[k].o + HB[k].l) { int w = 0; for (int j = 0; j < 8; j++) if (WN[j].used && WN[j].id == HB[k].id) w = j; c = (uint8_t)(C_OWN + w); }
    fill(x, y, 3, 5, c);
    if (h_need && u >= h_w0 && u < h_w1) { dot(x, y, C_TEAL); dot(x + 2, y, C_TEAL); dot(x + 1, y + 4, C_TEAL); } }
  int y = 50, x = text(2, y, "FREE ", C_DIM); x = number(x, y, h_free(), C_TEXT); x = text(x, y, "/", C_DIM); x = number(x, y, HU, C_DIM);
  x = text(x + 8, y, "BLOCKS ", C_DIM); number(x, y, hn, C_TEXT);
  y = 60;
  if (h_need) { x = text(2, y, "HELD ", C_TEAL); x = number(x, y, h_need, C_TEXT); x = text(x, y, "U WHEN ", C_TEAL); x = number(x, y, h_promise, C_TEXT);
    x = text(x, y, " NOW ", C_DIM); number(x, y, latch_n - h_since, C_TEXT); }
  else text(2, y, "NOTHING HELD", C_DIM);
  y = 70; x = text(2, y, "HELD ", C_DIM); x = number(x, y, held_n, C_TEXT); x = text(x + 4, y, "LATE ", C_DIM); x = number(x, y, late_n, late_n ? C_RED : C_TEXT);
  x = text(x + 4, y, "THETA ", C_DIM); number(x, y, refused, refused ? C_RED : C_TEXT);
  y = 80; x = text(2, y, "MOVED ", C_DIM); x = number(x, y, moved_all, C_TEXT); x = text(x + 4, y, "WORST ", C_DIM); x = number(x, y, worst_move, C_TEXT);
  x = text(x + 4, y, "BAD ", C_DIM); number(x, y, bad_moves + keep_bad, bad_moves + keep_bad ? C_RED : C_GREEN); }
static void ed_shift(int from, int by) {                   // steps moved: every jump that pointed past the change is moved with them
  for (int i = 0; i < ed[0]; i++) { int k = OP_KIND[ed[1 + 2 * i] >> 3]; if ((k == 4 || k == 5) && ed[2 + 2 * i] > from) ed[2 + 2 * i] = (uint8_t)(ed[2 + 2 * i] + by); } }
static void app_edit(void) {
  fill(0, 0, s_w, s_h, C_WIN);
  if (ed_loaded != ed_file) { const uint8_t *b = file_bytes(ed_file); if (b) memcpy(ed, b, FB_BYTES); else memset(ed, 0, FB_BYTES); ed_loaded = ed_file; ed_sel = 0; ed_top = 0; }
  if (ed_sel < ed_top) ed_top = ed_sel; if (ed_sel > ed_top + 7) ed_top = ed_sel - 7;
  for (int r = 0; r < 8; r++) { int i = ed_top + r, y = 2 + r * 10; if (i >= ed[0]) break;
    uint8_t b0 = ed[1 + 2 * i], b1 = ed[2 + 2 * i]; int k = OP_KIND[b0 >> 3];
    if (i == ed_sel) fill(0, y - 1, 120, 10, C_BTN);
    int x = number(2, y, i, C_DIM); x = text(20, y, OP_NAME[b0 >> 3], C_TEXT); x = 60; char sl[2] = {(char)('A' + (b0 & 7)), 0};
    if (k == 1 || k == 2 || k == 3 || k == 5) x = text(x, y, sl, C_INK);
    if (k == 2) { char sb[2] = {(char)('A' + (b1 & 7)), 0}; text(x + 6, y, sb, C_INK); }
    if (k == 3 || k == 6) number(x + 6, y, k == 3 ? (int8_t)b1 : b1, C_GOLD);
    if (k == 4 || k == 5) { int xx = text(x + 6, y, ">", C_DIM); number(xx, y, b1, C_GOLD); } }
  uint8_t *b0 = &ed[1 + 2 * ed_sel], *b1 = &ed[2 + 2 * ed_sel]; int has = ed[0] > 0;
  int bx = 124, y = 2;
  if (button(bx, y, 28, 10, "UP", 0) && ed_sel > 0) ed_sel--; if (button(bx + 30, y, 28, 10, "DN", 0) && ed_sel + 1 < ed[0]) ed_sel++;
  y += 12; if (button(bx, y, 28, 10, "OP-", 0) && has) *b0 = (uint8_t)((*b0 + 248) & 255); if (button(bx + 30, y, 28, 10, "OP+", 0) && has) *b0 = (uint8_t)((*b0 + 8) & 255);
  y += 12; if (button(bx, y, 28, 10, "A-", 0) && has) *b0 = (uint8_t)((*b0 & 0xF8) | ((*b0 + 7) & 7)); if (button(bx + 30, y, 28, 10, "A+", 0) && has) *b0 = (uint8_t)((*b0 & 0xF8) | ((*b0 + 1) & 7));
  y += 12; if (button(bx, y, 28, 10, "B-", 0) && has) (*b1)--; if (button(bx + 30, y, 28, 10, "B+", 0) && has) (*b1)++;
  y += 12; if (button(bx, y, 28, 10, "INS", 0) && ed[0] < 48) { int at = has ? ed_sel + 1 : 0; memmove(&ed[1 + 2 * (at + 1)], &ed[1 + 2 * at], (size_t)(2 * (ed[0] - at))); ed[1 + 2 * at] = 0; ed[2 + 2 * at] = 0; ed[0]++; ed_shift(at - 1, 1); ed_sel = at; }
  if (button(bx + 30, y, 28, 10, "DEL", 0) && has) { memmove(&ed[1 + 2 * ed_sel], &ed[1 + 2 * (ed_sel + 1)], (size_t)(2 * (ed[0] - ed_sel - 1))); ed[0]--; ed[1 + 2 * ed[0]] = 0; ed[2 + 2 * ed[0]] = 0; ed_shift(ed_sel, -1); if (ed_sel >= ed[0] && ed_sel) ed_sel--; }
  y += 12; if (button(bx, y, 58, 11, "SAVE", 0)) { file_write(ed_file, ed); for (int s = 0; s < NP; s++) if (proc[s].state != ST_FREE && proc[s].file == ed_file) spawn(ed_file); }
  y += 13; if (button(bx, y, 58, 11, "RUN", 0)) { file_write(ed_file, ed); spawn(ed_file); }
  int x = text(2, s_h - 9, "FILE ", C_DIM); text(x, s_h - 9, FNAME[ed_file], C_GOLD); }

// ---------------------------------------------------------------- the ghost hand: aims at the buttons the eye saw, and plays
#define G_DOCK 1
#define G_BTN 2
#define G_DRAG 3
#define G_CLOSE 4
#define G_WAIT 5
static const GStep GHOST[] = {
  {G_DOCK, 0, "FILES", 0, 0}, {G_BTN, K_FILES, "ROOT", 0, 0}, {G_BTN, K_FILES, "RUN", 0, 0}, {G_WAIT, 0, 0, 20, 0},
  {G_BTN, K_FILES, "E", 0, 0}, {G_BTN, K_FILES, "RUN", 0, 0},
  {G_BTN, K_FILES, "BOUNCE", 0, 0}, {G_BTN, K_FILES, "RUN", 0, 0},
  {G_DOCK, 0, "HEAP", 0, 0}, {G_DRAG, K_HEAP, 0, 96, 104},
  {G_BTN, K_FILES, "TURN", 0, 0}, {G_BTN, K_FILES, "RUN", 0, 0}, {G_WAIT, 0, 0, 60, 0},
  {G_CLOSE, K_PLACE, 0, 1, 0},                                            // a hole in the middle of the arena
  {G_DOCK, 0, "TASKS", 0, 0}, {G_WAIT, 0, 0, 80, 0},                      // too big for any hole, small enough for all: held, with a when
  {G_CLOSE, K_HEAP, 0, 0, 0}, {G_CLOSE, K_PLACE, 0, 2, 0}, {G_DOCK, 0, "EDIT", 0, 0}, {G_WAIT, 0, 0, 80, 0},
  {G_BTN, K_EDIT, "B+", 0, 0}, {G_BTN, K_EDIT, "B+", 0, 0}, {G_BTN, K_EDIT, "SAVE", 0, 0}, {G_WAIT, 0, 0, 60, 0},
  {G_CLOSE, K_EDIT, 0, 0, 0}, {G_BTN, K_FILES, "SLOPE", 0, 0}, {G_BTN, K_FILES, "SPOIL", 0, 0}, {G_WAIT, 0, 0, 40, 0},
  {G_BTN, K_FILES, "SLOPE", 0, 0}, {G_BTN, K_FILES, "RUN", 0, 0}, {G_WAIT, 0, 0, 100, 0},
  {G_BTN, K_FILES, "CUBE", 0, 0}, {G_BTN, K_FILES, "RUN", 0, 0}, {G_WAIT, 0, 0, 60, 0},
  {G_BTN, K_TASKS, "2", 0, 0}, {G_BTN, K_TASKS, "VENT", 0, 0}, {G_BTN, K_TASKS, "RULES", 0, 0}, {G_WAIT, 0, 0, 40, 0},
  {G_BTN, K_RULES, "S", 0, 0}, {G_BTN, K_RULES, "-", 0, 0}, {G_BTN, K_RULES, "+", 0, 0}, {G_CLOSE, K_RULES, 0, 0, 0},
  {G_BTN, K_FILES, "MEET", 0, 0}, {G_BTN, K_FILES, "RUN", 0, 0}, {G_WAIT, 0, 0, 120, 0},
  {G_CLOSE, K_PLACE, 0, 0, 0}, {G_DOCK, 0, "HEAP", 0, 0}, {G_WAIT, 0, 0, 90, 0}, {G_CLOSE, K_PLACE, 0, 1, 0}, {G_CLOSE, K_PLACE, 0, 2, 0}, {G_CLOSE, K_PLACE, 0, 3, 0},
  {G_CLOSE, K_HEAP, 0, 0, 0}, {G_CLOSE, K_TASKS, 0, 0, 0}, {G_CLOSE, K_FILES, 0, 0, 0}, {G_WAIT, 0, 0, 40, 0},
};
#define NGHOST ((int)(sizeof GHOST / sizeof GHOST[0]))
static int g_at, g_phase, g_left, g_hold, g_raise; static int32_t g_tx, g_ty, g_dx, g_dy;
static int seen_find(int win_kind_or_dock, const char *label, int place, int32_t *x, int32_t *y) {   // the button as last drawn
  int prev = seen_now ^ 1; uint32_t lab = name_of(label);
  for (int k = 0; k < nseen[prev]; k++) { Seen *e = &SEEN[prev][k];
    int wk = e->win < 8 ? WN[e->win].kind : 0; (void)place;
    if (e->lab == lab && ((win_kind_or_dock == 0 && e->win == 8) || (win_kind_or_dock && e->win < 8 && wk == win_kind_or_dock))) {
      *x = (e->x + e->w / 2) * 16; *y = (e->y + e->h / 2) * 16; return 1; } }
  return 0; }
static int hit_window(int x, int y);
static int visible_spot(int w, int32_t *x, int32_t *y) {    // a dot of window w that no window above it covers: its title bar first, then its body
  for (int yy = WN[w].y + 5; yy < WN[w].y + WN[w].h + TITLE_H; yy += (yy < WN[w].y + TITLE_H ? TITLE_H : 6))
    for (int xx = WN[w].x + 6; xx < WN[w].x + WN[w].w - 12; xx += 8) if (xx >= 0 && xx < SW && yy >= 12 && yy < SH - 18 && hit_window(xx, yy) == w) { *x = xx * 16; *y = yy * 16; return 1; }
  return 0; }
static void ghost_latch(void) {                            // move a share of what is left over the latches left; click on arrival
  const GStep *g = &GHOST[g_at];
  if (g_phase == 0) {                                      // choose the aim
    int ok = 0; g_hold = 0; g_raise = 0;
    if (g->act == G_DOCK) ok = seen_find(0, g->label, 0, &g_tx, &g_ty);
    else if (g->act == G_BTN) { ok = seen_find(g->kind, g->label, 0, &g_tx, &g_ty);
      int w = win_find(g->kind, 0); if (ok && w >= 0 && nz && ZO[nz - 1] != w) { int32_t vx, vy; if (visible_spot(w, &vx, &vy)) { g_tx = vx; g_ty = vy; g_raise = 1; } else ok = 0; } }   // covered: raise it first, where it shows, as a hand would
    else if (g->act == G_DRAG || g->act == G_CLOSE) { int w = win_find(g->kind, g->act == G_CLOSE ? g->a : 0); if (w >= 0) { ok = 1;
        if (g->act == G_DRAG) { g_tx = (WN[w].x + 24) * 16; g_ty = (WN[w].y + 5) * 16; g_dx = g->a * 16; g_dy = g->b * 16; }
        else { g_tx = (WN[w].x + WN[w].w - 5) * 16; g_ty = (WN[w].y + 5) * 16; } } }
    else if (g->act == G_WAIT) { g_left = g->a; g_phase = 3; return; }
    if (!ok) { if (++g_left > 30) { g_left = 0; g_at = (g_at + 1) % NGHOST; } return; }
    g_left = 12 + (int)(scatter() % 10); g_phase = 1; }
  if (g_phase == 1) {                                      // travel
    ptr_x += (g_tx - ptr_x) / g_left; ptr_y += (g_ty - ptr_y) / g_left; g_left--;
    if (g_left <= 0) { ptr_x = g_tx; ptr_y = g_ty; g_phase = 2; g_left = g->act == G_DRAG ? 22 : 2; if (g->act == G_DRAG) { g_tx = g->a * 16; g_ty = g->b * 16; } } return; }
  if (g_phase == 2) {                                      // press (and for a drag, carry the window to its place while held)
    down = 1; if (g->act == G_DRAG && g_left > 1) { ptr_x += (g_tx - ptr_x) / (g_left - 1); ptr_y += (g_ty - ptr_y) / (g_left - 1); }
    if (--g_left <= 0) { down = 0; g_phase = 0; g_left = 0; if (g_raise) g_raise = 0; else g_at = (g_at + 1) % NGHOST; } return; }
  if (g_phase == 3) { if (--g_left <= 0) { g_phase = 0; g_left = 0; g_at = (g_at + 1) % NGHOST; } } }

// ---------------------------------------------------------------- the frame: hands, one latch of every task, one picture, its change sent
static uint32_t TILEP[NTY][NTX]; static uint32_t tiles_sent, f_tiles, frame_n;
static uint16_t *RB[2]; static uint32_t slot_n;
static int drag_w = -1; static int32_t drag_ox, drag_oy;
static uint32_t c_pre, c_draw, c_send;
static void show(void) {                                   // a tile whose print is unchanged is not said again; changed tiles go out in runs
  for (int ty = 0; ty < NTY; ty++) {
    uint32_t chg = 0;
    for (int tx = 0; tx < NTX; tx++) { uint32_t h = 0x811c9dc5u; const uint8_t *p = FB + ty * TH * SW + tx * TW;
      for (int r = 0; r < TH; r++) { const uint32_t *q = (const uint32_t *)(p + r * SW); for (int k = 0; k < TW / 4; k++) { h = (h ^ q[k]) * 0x01000193u; h ^= h >> 15; } }
      if (h != TILEP[ty][tx]) { TILEP[ty][tx] = h; chg |= 1u << tx; } }
    for (int t = 0; t < NTX;) { if (!(chg >> t & 1)) { t++; continue; }
      int t1 = t; while (t1 < NTX && (chg >> t1 & 1)) t1++;
      int s = (int)(slot_n++ & 1); uint16_t *rb = RB[s]; m_wait(s); int w = (t1 - t) * TW;
      for (int r = 0; r < TH; r++) { const uint8_t *src = FB + (ty * TH + r) * SW + t * TW; uint16_t *o = rb + r * w; for (int k = 0; k < w; k++) o[k] = PAL[src[k]]; }
      window(t * TW, t1 * TW - 1, ty * TH, ty * TH + TH - 1); pix(rb, w * TH * 2); m_fence(s); tiles_sent += (uint32_t)(t1 - t); f_tiles += (uint32_t)(t1 - t); t = t1; } } }
static const uint8_t ARROW[11] = {0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xF8, 0xD8, 0x8C, 0x0C};
static void draw_pointer(void) {
  int x0 = ptr_x >> 4, y0 = ptr_y >> 4; on(FB, SW, SH);
  for (int r = 0; r < 11; r++) for (int c = 0; c < 8; c++) if (ARROW[r] >> (7 - c) & 1) {
      int edge = !(r + 1 < 11 && (ARROW[r + 1] >> (7 - c) & 1)) || !(c + 1 < 8 && (ARROW[r] >> (6 - c) & 1)) || c == 0;
      dot(x0 + c, y0 + r, edge ? C_EDGE : (down ? C_GOLD : C_WHITE)); } }
static void hands(void) {
  was_down = down; b_tap = c_tap = 0;
#ifdef LIVE_HANDS
  pad_read(&pad);
  int any = pad.jx > 36 || pad.jx < -36 || pad.jy > 36 || pad.jy < -36 || pad.a || pad.sw || pad.b || pad.c;   // a floating stick reads noise: only a real push counts
  if (any) { ghost = 0; idle_n = 0; hands_seen = 1; } else if (++idle_n > 900) ghost = 1;   // half a minute untouched: the ghost plays
  if (!ghost) { int32_t vx = pad.jx + pad.jx * (pad.jx < 0 ? -pad.jx : pad.jx) / 127, vy = pad.jy + pad.jy * (pad.jy < 0 ? -pad.jy : pad.jy) / 127;
    ptr_x = clampi(ptr_x + vx / 2, 0, (SW - 1) * 16); ptr_y = clampi(ptr_y + vy / 2, 0, (SH - 1) * 16); down = pad.a || pad.sw;
    b_tap = pad.b && lb == 0; c_tap = pad.c && lc == 0; lb = pad.b; lc = pad.c; }
#endif
  if (ghost) ghost_latch();
  clicked = down && !was_down; }
static int hit_window(int x, int y) { for (int k = nz - 1; k >= 0; k--) { Win *w = &WN[ZO[k]]; if (x >= w->x && x < w->x + w->w + 2 && y >= w->y && y < w->y + w->h + TITLE_H + 1) return ZO[k]; } return -1; }
static void draw_window(int w, int top, int hw) {
  Win *n = &WN[w]; on(FB, SW, SH); int x = n->x, y = n->y, ww = n->w + 2, hh = n->h + TITLE_H + 1;
  fill(x + 3, y + 3, ww, hh, C_SHADOW); fill(x, y, ww, hh, C_EDGE); fill(x + 1, y + 1, ww - 2, TITLE_H - 1, top ? C_TITLE : C_TITLE_DIM);
  char t[24]; if (n->kind == K_PLACE) { t[0] = (char)('1' + n->place); t[1] = ' '; strcpy(t + 2, proc[n->place].state == ST_FREE ? "" : FNAME[proc[n->place].file]); } else strcpy(t, KNAME[n->kind]);
  if (n->kind == K_EDIT) { strcat(t, " "); strcat(t, FNAME[ed_file]); }
  int tx = text(x + 4, y + 2, t, C_TEXT);
  if (n->held) { tx = text(tx + 8, y + 2, "WHEN ", C_GOLD); number(tx, y + 2, (int64_t)n->promise - (int64_t)(latch_n - n->asked) > 0 ? (int64_t)n->promise - (int64_t)(latch_n - n->asked) : 0, C_GOLD); }
  text(x + ww - 9, y + 2, "X", C_TEXT);
  uint8_t *pic = pic_of(w);
  if (!pic) { fill(x + 1, y + TITLE_H, n->w, n->h, C_WIN); on(FB, SW, SH);
    int cx = x + 1 + n->w / 2 - 42, cy = y + TITLE_H + n->h / 2 - 8; text(cx, cy, "HELD BY THE HEAP", C_TEAL); int xx = text(cx, cy + 10, "PROMISED IN ", C_DIM); number(xx, cy + 10, n->promise, C_GOLD); return; }
  if (n->kind != K_PLACE) {                                // an app draws its own picture now, and answers clicks meant for it
    cur_win = w; on(pic, n->w, n->h); lpx = (ptr_x >> 4) - x - 1; lpy = (ptr_y >> 4) - y - TITLE_H; l_click = clicked && hw == w && lpy >= 0;
    if (n->kind == K_FILES) app_files(); else if (n->kind == K_TASKS) app_tasks(); else if (n->kind == K_HEAP) app_heap(); else if (n->kind == K_EDIT) app_edit(); else if (n->kind == K_RULES) app_rules();
    pic = pic_of(w); if (!pic) return; }
  for (int r = 0; r < n->h; r++) { int yy = y + TITLE_H + r; if ((unsigned)yy >= SH) continue; int x0 = x + 1, x1 = x + 1 + n->w; const uint8_t *src = pic + r * n->w;
    int a = x0 < 0 ? -x0 : 0, b = x1 > SW ? n->w - (x1 - SW) : n->w; if (b > a) memcpy(FB + yy * SW + x0 + a, src + a, (size_t)(b - a)); } }
static void desk_and_dock(int hw) {
  on(FB, SW, SH);
  for (int y = 0; y < SH; y++) { uint8_t *o = FB + y * SW; for (int x = 0; x < SW; x++) { int v = (y * 15 * 16) / SH + B4[((y & 3) << 2) | (x & 3)]; o[x] = (uint8_t)(C_BG + (v >> 4 > 15 ? 15 : v >> 4)); } }
  fill(0, 0, SW, 12, C_BAR); int x = text(4, 3, "RULE OS 4", C_GOLD);
  x = text(x + 10, 3, "LATCH ", C_DIM); x = number(x, 3, latch_n, C_TEXT);
  x = text(x + 10, 3, "HEAP ", C_DIM); x = number(x, 3, h_free(), C_TEXT); x = text(x, 3, "/", C_DIM); x = number(x, 3, HU, C_DIM);
  if (!theta_left) { x = text(x + 10, 3, "FIELD ", k_books() ? C_DIM : C_RED); x = number(x, 3, field, k_books() ? C_TEXT : C_RED); }
  if (h_need) { x = text(x + 8, 3, "WHEN ", C_TEAL); number(x, 3, h_promise, C_TEAL); }
  if (theta_left) { theta_left--; x = text(x + 8, 3, theta_kind == K_PLACE ? "PLACE" : KNAME[theta_kind], C_RED); text(x + 6, 3, "THETA", C_RED); }
  text(SW - 40, 3, ghost ? "GHOST" : "HANDS", ghost ? C_TEAL : C_GREEN);
  fill(0, SH - 18, SW, 18, C_BAR);
  cur_win = 8; lpx = ptr_x >> 4; lpy = (ptr_y >> 4) - (SH - 18); l_click = clicked && hw < 0;
  on(FB + (SH - 18) * SW, SW, 18);
  const char *dock[4] = {"FILES", "TASKS", "HEAP", "EDIT"}; const int kinds[4] = {K_FILES, K_TASKS, K_HEAP, K_EDIT};
  for (int k = 0; k < 4; k++) { int lit = win_find(kinds[k], 0) >= 0;
    if (button(4 + k * 46, 3, 44, 12, dock[k], lit)) { int w = win_find(kinds[k], 0); if (w >= 0) z_top(w); else { if (kinds[k] == K_EDIT) { ed_file = file_sel; ed_loaded = -1; } win_open(kinds[k], 0); } } }
  for (int s = 0; s < NP; s++) { Proc *p = &proc[s]; int bx = 194 + s * 31; char lab[3] = {(char)('1' + s), 0, 0};
    uint8_t c = p->state == ST_RUN ? C_INK + s : p->state >= ST_THETA ? C_RED : p->state == ST_WAIT ? C_TEAL : C_BTN;
    fill(bx, 3, 28, 12, c); frame_box(bx, 3, 28, 12, C_EDGE); text(bx + 11, 5, lab, C_EDGE);
    if (lpx >= bx && lpx < bx + 28 && lpy >= 3 && lpy < 15 && l_click) { int w = win_find(K_PLACE, s); if (w >= 0) z_top(w); } }
  for (int k = 0; k < nseen[seen_now]; k++) if (SEEN[seen_now][k].win == 8) SEEN[seen_now][k].y = (int16_t)(SEEN[seen_now][k].y + SH - 18); }
void frame(void) {
  f_bytes = 0; f_tiles = 0; uint32_t c0 = cyc();
  hands();
  latch_n++;
  if ((latch_n % 12) == 0) keeper_turn();
  int32_t got = h_turn(); if (got > 0) served(got);
  if (got < 0) { for (int w = 0; w < 8; w++) if (WN[w].used && WN[w].id == -got) { theta_kind = WN[w].kind; theta_left = 90; win_close(w); } }   // the promise could not be kept: the window is told, and closes
  { uint32_t blocks = (uint32_t)hn; if (blocks != L_est - L_del) keep_bad++; }   // KEEP, checked against the table every latch
  if (want_rules) { want_rules = 0; int w = win_find(K_TASKS, 0); if (w >= 0) win_close(w); win_open(K_RULES, 0); }
  run_places();
  uint32_t c1 = cyc();
  int hw = hit_window(ptr_x >> 4, ptr_y >> 4);              // whose the pointer is over: only that one hears a click
  if (clicked && hw >= 0) { Win *n = &WN[hw]; int lx = (ptr_x >> 4) - n->x, ly = (ptr_y >> 4) - n->y; z_top(hw);
    if (ly < TITLE_H) { if (lx >= n->w + 2 - 11) { win_close(hw); hw = -2; clicked = 0; } else { drag_w = hw; drag_ox = ptr_x - n->x * 16; drag_oy = ptr_y - n->y * 16; } } }
  if (drag_w >= 0) { if (!down || !WN[drag_w].used) drag_w = -1; else { WN[drag_w].x = (int16_t)clampi((ptr_x - drag_ox) >> 4, -WN[drag_w].w + 30, SW - 30); WN[drag_w].y = (int16_t)clampi((ptr_y - drag_oy) >> 4, 12, SH - 30); } }
  if (b_tap && nz) win_close(ZO[nz - 1]);
  if (c_tap && nz > 1) z_top(ZO[0]);
  seen_now ^= 1; nseen[seen_now] = 0;
  desk_and_dock(hw);
  for (int k = 0; k < nz; k++) draw_window(ZO[k], k == nz - 1, hw);
  draw_pointer();
  uint32_t c2 = cyc();
  show(); cmd(0x00, 0, 0);
  c_pre = c1 - c0; c_draw = c2 - c1; c_send = cyc() - c2; frame_n++; }
static uint32_t glass_print(void) { uint32_t h = 0x811c9dc5u; for (int k = 0; k < SW * SH; k++) h = (h ^ FB[k]) * 0x01000193u; return h; }

#ifdef DESK
#define HEAP_GET(n) malloc(n)
#define HEAP_DMA(n) malloc(n)
#else
#define HEAP_GET(n) heap_caps_malloc((n), MALLOC_CAP_8BIT)
#define HEAP_DMA(n) heap_caps_malloc((n), MALLOC_CAP_DMA | MALLOC_CAP_8BIT)
#endif
void setup(void) {
#if !defined(DESK) && FRAMES > 0
  Serial0.begin(921600);                                   // emulator bench builds only; the board never opens UART0 (GPIO 20/21 are buttons)
#elif defined(ON_BOARD)
  Serial.begin(115200); delay(300);
#endif
  STAGE("OS4 1 boot\n");
  for (int i = 0; i < 2; i++) RB[i] = (uint16_t *)HEAP_DMA(STRIP * 2);
  ARENA = (uint8_t *)HEAP_GET(HU * UB);
  STAGE("OS4 2 strips %s, arena %s (%d bytes)\n", RB[0] && RB[1] ? "ok" : "REFUSED", ARENA ? "ok" : "REFUSED", HU * UB);
  if (!(RB[0] && RB[1] && ARENA)) { while (1) { STAGE("OS4 memory refused (theta)\n"); hold_ms(1000); } }
  grow_face(); grow_palette();
#ifdef LIVE_HANDS
  pad_begin();
#endif
  panel_begin();
#if defined(ON_BOARD)
  STAGE("OS4 3 spi bus %d device %d (0 = ok)\n", m_e1, m_e2);
#endif
  { uint16_t g = glass(255, 200, 80); for (int k = 0; k < STRIP; k++) RB[0][k] = g;   // the boot mark: a gold band at once
    window(0, SW - 1, 112, 127); pix(RB[0], STRIP * 2); m_wait_all(); }
  STAGE("OS4 4 panel up: a gold band across the middle\n");
  disk_begin();
  k_begin(); rules.S = 3000; rules_load();   // S on the C3: 3,000 beats (see README)
  STAGE("OS4 5 files ready: %d, keeper on\n", NF);
}
#ifndef DESK
static uint32_t rep_n, rep_sum, rep_max, last_us;
void loop(void) {
#if FRAMES == 0 && !defined(MOUTH_FIFO)
  static uint32_t t0; while ((uint32_t)(micros() - t0) < 33000) delayMicroseconds(200);   // pace only: at most 30 latches a second
  uint32_t now = micros(), us = last_us ? now - last_us : 0; last_us = now; t0 = now;
#endif
  frame();
#if FRAMES > 0
  Serial0.printf("F %u glass %08x pre %u draw %u send %u bytes %u tiles %u free %d held %d field %u recon %u books %d\n", (unsigned)frame_n, (unsigned)glass_print(), (unsigned)c_pre, (unsigned)c_draw, (unsigned)c_send,
    (unsigned)f_bytes, (unsigned)f_tiles, (int)h_free(), (int)(h_need != 0), (unsigned)field, (unsigned)k_recon, (int)k_books());
  if (frame_n >= FRAMES) {
    m_wait_all();
    Serial0.printf("BOOKS est %u del %u keep %u blocks %d keep_bad %u bad_moves %u held %u late %u early %u refused %u moved %u worst %u mends %u steps %u\n",
      (unsigned)L_est, (unsigned)L_del, (unsigned)(L_est - L_del), hn, (unsigned)keep_bad, (unsigned)bad_moves, (unsigned)held_n, (unsigned)late_n, (unsigned)early_n,
      (unsigned)refused, (unsigned)moved_all, (unsigned)worst_move, (unsigned)mends, (unsigned)steps_all);
    Serial0.printf("STACK free %u words\n", (unsigned)uxTaskGetStackHighWaterMark(NULL));
    Serial0.printf("HEAP free %u\n", (unsigned)ESP.getFreeHeap());
    Serial0.printf("DONE\n");
    while (1) delay(1000);
  }
#elif defined(ON_BOARD)
  if (us) { rep_sum += us; if (us > rep_max) rep_max = us; }
  if (++rep_n >= 300) { Serial.printf("OS4 REAL field %u/%u recon %u books %s | latch", (unsigned)field, (unsigned)rules.G0, (unsigned)k_recon, k_books() ? "shut" : "OPEN"); Serial.printf("  %u frame avg %u us worst %u us | heap free %d held %u late %u theta %u bad %u | mends %u | %s\n", (unsigned)latch_n,
      (unsigned)(rep_sum / rep_n), (unsigned)rep_max, (int)h_free(), (unsigned)held_n, (unsigned)late_n, (unsigned)refused, (unsigned)(bad_moves + keep_bad), (unsigned)mends, ghost ? "ghost" : "hands");
    rep_n = 0; rep_sum = 0; rep_max = 0; }
#endif
}
#endif
