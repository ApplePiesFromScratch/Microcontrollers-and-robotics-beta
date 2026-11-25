// TIDE: a computer that runs backward. ESP32-C3 + 2.0" ST7789 (320 x 240).
// TIDE is a small machine with its own instruction set, in which every instruction has an exact inverse. A checker reads each program
// before it runs and refuses any instruction that could lose a bit (ADD r3,r3 would double r3 and forget its top bit: refused).
// So the whole machine (registers, memory, the picture, up to 256 balls) can be run backward to the last bit, with nothing kept to rewind it
// except what came in from outside: the hands. The screen keeps the ledger: bits destroyed by the machine, bytes taken from the hands,
// and, at step 0 again, whether the whole state equals the start.
// Programs: MARBLE (a picture stirred to noise, then unstirred), SHATTER (a word of 232 balls smashed by a cannonball, then made whole),
// FORGET (SHATTER's same rule in ordinary floating point, run forward and back: it does not find its way home).
// Hands: hold B to run time backward. A: fire (SHATTER) / stir (MARBLE, with the stick). C: next program. Nothing touched: the ghost plays.
// Wiring: SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3. Stick VRx 0, VRy 1, press 2. Buttons A 3, B 20, C 21 (to ground).
// Arduino IDE: "ESP32C3 Dev Module", core 3.x, USB CDC On Boot: Enabled. Serial Monitor 115200: frame times.
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

// ---------------------------------------------------------------- the shapes of things (first, for the IDE)
typedef struct { uint8_t op, a, b, c, p; int32_t k; } Ins;      // p: run only if register p is not 0 (255: always)
typedef struct { const char *name; const Ins *code; int n; int world; } Prog;
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




static int32_t mixc(int32_t a, int32_t b, int32_t k, int32_t n) { return a + mouth((b - a) * k, n); }

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

// ================================================================ THE MACHINE
// Every instruction either adds to a register something read from OTHER registers (a += f(b, c)), or swaps, or turns a pair
// exactly, or permutes a picture or a world. Each has an exact inverse, and the checker below refuses any instruction that has not.
enum { O_NOP, O_ADD, O_SUB, O_XOR, O_ADDI, O_XORI, O_ROL, O_NEG, O_SWAP, O_MULO, O_ADDM, O_SUBM, O_TURN, O_EXCH, O_IN,
       O_STIRX, O_STIRY, O_LEAP, O_BLAST, O_LEAPF, O_BLASTF, O_NOPS };
static const char *const ONAME[O_NOPS] = {"NOP", "ADD", "SUB", "XOR", "ADDI", "XORI", "ROL", "NEG", "SWAP", "MULO", "ADDM", "SUBM", "TURN", "EXCH", "IN",
  "STIRX", "STIRY", "LEAP", "BLAST", "LEAPF", "BLASTF"};
static const char *const OBACK[O_NOPS] = {"NOP", "SUB", "ADD", "XOR", "ADDI", "XORI", "ROR", "NEG", "SWAP", "MULO", "SUBM", "ADDM", "TURN", "EXCH", "IN",
  "UNSTIRX", "UNSTIRY", "UNLEAP", "UNBLAST", "UNLEAPF", "UNBLASTF"};
#define NREG 16
#define MEMW 256
#define NOP_ 255
static int32_t R[NREG], MEM[MEMW], S;              // S: the step label, a count of latches; it counts down when the machine runs back
static uint32_t MINV[64];                           // MULO's inverse multipliers, found when a program is checked
static uint32_t wadd(int32_t a, int32_t b) { return (uint32_t)a + (uint32_t)b; }   // whole-number wrap (mod 2^32): adding is undone by subtracting

// ---------------------------------------------------------------- exact turns of a pair: three shears, each undone by its own subtraction
// A heading (P : Q) turns by the angle whose half has slope P/Q; its unit has s = 2PQ / (P^2 + Q^2). The heading's own P/Q is the shear.
static const int16_t HP[8] = {1, 1, 1, 1, 1, 2, 1, 1}, HQ[8] = {80, 40, 20, 12, 8, 7, 4, 2};
static void turn(int32_t *x, int32_t *y, int h, int dir) {
  int32_t P = HP[h], Q = HQ[h], D = P * P + Q * Q;
  #define SHEAR(t, src, num, den, sg) t = (int32_t)((uint32_t)t + (uint32_t)(sg) * (uint32_t)(int32_t)mouth64((int64_t)(src) * (num), den))   // whole-number wrap, like ADD
  if (dir > 0) { SHEAR(*x, *y, P, Q, -1); SHEAR(*y, *x, 2 * P * Q, D, 1); SHEAR(*x, *y, P, Q, -1); }
  else { SHEAR(*x, *y, P, Q, 1); SHEAR(*y, *x, 2 * P * Q, D, -1); SHEAR(*x, *y, P, Q, 1); } }

// ---------------------------------------------------------------- the picture: 76,800 dots, each a colour number; stirring only moves dots
static uint8_t CANVAS[SW * SH]; static uint8_t TMPL[SW > SH ? SW : SH];
static int32_t wrapn(int32_t v, int32_t n) { v %= n; return v < 0 ? v + n : v; }
static void stir_rows(int32_t amp, int32_t ph, int dir) {         // each row slides by its own amount: a permutation of the dots
  for (int y = 0; y < SH; y++) { int32_t c, s; unit16(ph + y * 22, &c, &s); int32_t sh = wrapn(mouth(amp * s, 16384) * dir, SW); if (!sh) continue;
    uint8_t *row = CANVAS + y * SW; memcpy(TMPL, row, SW); memcpy(row + sh, TMPL, SW - sh); memcpy(row, TMPL + SW - sh, sh); } }
static void stir_cols(int32_t amp, int32_t ph, int dir) {
  for (int x = 0; x < SW; x++) { int32_t c, s; unit16(ph + x * 16, &c, &s); int32_t sh = wrapn(mouth(amp * s, 16384) * dir, SH); if (!sh) continue;
    for (int y = 0; y < SH; y++) TMPL[y] = CANVAS[y * SW + x];
    for (int y = 0; y < SH; y++) CANVAS[((y + sh) % SH) * SW + x] = TMPL[y]; } }

// ---------------------------------------------------------------- the world: balls as (now, before); the next place is 2 now - before + push(now)
// Going forward the new place is written over "before" and the two are swapped. Going back: swap, then the SAME formula. The push may be
// any rule of where things are now (here: soft contact, gravity, walls), because it is added and the same push is subtracted back.
#define NB 256
#define BR 768                                       // ball radius, 1/256 dot (3 dots)
#define BD (2 * BR)
#define BD2 ((int64_t)BD * BD)
#define WW (SW * 256)
#define WH ((SH - 36) * 256)                 // the floor sits above the ledger strip
#define GCW (SW * 256 / BD + 2)
#define GCH (SH * 256 / BD + 2)
static int32_t WX[2][NB], WY[2][NB], SX0[NB], SY0[NB]; static int wcur, nb;
static float FXf[2][NB], FYf[2][NB]; static int fcur;
static uint8_t WCOL[NB];
static int32_t PUSHX[NB], PUSHY[NB]; static int16_t GHEAD[GCW * GCH], GNEXT[NB];
static int cell_of(int32_t x, int32_t y) { int cx = clampi(x / BD + 1, 0, GCW - 1), cy = clampi(y / BD + 1, 0, GCH - 1); return cy * GCW + cx; }
static void pushes(const int32_t *X, const int32_t *Y, int32_t g) {
  for (int k = 0; k < GCW * GCH; k++) GHEAD[k] = -1;
  for (int i = 0; i < nb; i++) { int c = cell_of(X[i], Y[i]); GNEXT[i] = GHEAD[c]; GHEAD[c] = (int16_t)i; }
  for (int i = 0; i < nb; i++) { int32_t px = 0, py = g, x = X[i], y = Y[i];
    if (x < BR) px += (BR - x) / 4; if (x > WW - BR) px -= (x - (WW - BR)) / 4; if (y > WH - BR) py -= (y - (WH - BR)) / 4; if (y < BR) py += (BR - y) / 4;   // soft walls
    PUSHX[i] = px; PUSHY[i] = py; }
  for (int i = 0; i < nb; i++) { int c = cell_of(X[i], Y[i]), cx = c % GCW, cy = c / GCW;
    for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) { int nx = cx + ox, ny = cy + oy; if (nx < 0 || ny < 0 || nx >= GCW || ny >= GCH) continue;
      for (int j = GHEAD[ny * GCW + nx]; j >= 0; j = GNEXT[j]) { if (j <= i) continue;
        int32_t dx = X[j] - X[i], dy = Y[j] - Y[i]; int64_t d2 = (int64_t)dx * dx + (int64_t)dy * dy; if (d2 >= BD2) continue;
        int64_t f = BD2 - d2; int32_t fx = (int32_t)(f * dx * 5 / (64 * BD2)), fy = (int32_t)(f * dy * 5 / (64 * BD2));   // soft contact, no root taken; soft enough that six neighbours stay inside the step
        PUSHX[i] -= fx; PUSHY[i] -= fy; PUSHX[j] += fx; PUSHY[j] += fy; } } } }
static void leap(int32_t g, int dir) {
  if (dir < 0) wcur ^= 1;
  int n = wcur, p = wcur ^ 1; pushes(WX[n], WY[n], g);
  for (int i = 0; i < nb; i++) { WX[p][i] = (int32_t)(wadd(wadd(WX[n][i], WX[n][i]), PUSHX[i]) - (uint32_t)WX[p][i]); WY[p][i] = (int32_t)(wadd(wadd(WY[n][i], WY[n][i]), PUSHY[i]) - (uint32_t)WY[p][i]); }
  if (dir > 0) wcur ^= 1; }
static void blast(int32_t cx, int32_t cy, int32_t str, int dir) {   // a kick out from a point, from where each ball is now: written into "before"
  const int32_t RB2 = 90 * 256; int64_t rr = (int64_t)RB2 * RB2; int n = wcur, p = wcur ^ 1;
  for (int i = 0; i < nb; i++) { int32_t dx = WX[n][i] - cx, dy = WY[n][i] - cy; int64_t d2 = (int64_t)dx * dx + (int64_t)dy * dy; if (d2 >= rr) continue;
    int64_t w = rr - d2; int32_t kx = (int32_t)(w * dx / rr * str / 64), ky = (int32_t)(w * dy / rr * str / 64) - (int32_t)(w * 96 * str / rr);
    WX[p][i] -= kx * dir; WY[p][i] -= ky * dir; } }
// the same world in floating point (FORGET): the same formulas, the same order; only the kind of number differs
static float PFX[NB], PFY[NB];
static void pushes_f(const float *X, const float *Y, float g) {
  for (int k = 0; k < GCW * GCH; k++) GHEAD[k] = -1;
  for (int i = 0; i < nb; i++) { int c = cell_of((int32_t)X[i], (int32_t)Y[i]); GNEXT[i] = GHEAD[c]; GHEAD[c] = (int16_t)i; }
  for (int i = 0; i < nb; i++) { float px = 0, py = g, x = X[i], y = Y[i];
    if (x < BR) px += (BR - x) / 4; if (x > WW - BR) px -= (x - (WW - BR)) / 4; if (y > WH - BR) py -= (y - (WH - BR)) / 4; if (y < BR) py += (BR - y) / 4;
    PFX[i] = px; PFY[i] = py; }
  for (int i = 0; i < nb; i++) { int c = cell_of((int32_t)X[i], (int32_t)Y[i]), cx = c % GCW, cy = c / GCW;
    for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) { int nx = cx + ox, ny = cy + oy; if (nx < 0 || ny < 0 || nx >= GCW || ny >= GCH) continue;
      for (int j = GHEAD[ny * GCW + nx]; j >= 0; j = GNEXT[j]) { if (j <= i) continue;
        float dx = X[j] - X[i], dy = Y[j] - Y[i], d2 = dx * dx + dy * dy; if (d2 >= (float)BD2) continue;
        float f = ((float)BD2 - d2) * 0.078125f / (float)BD2, fx = f * dx, fy = f * dy;
        PFX[i] -= fx; PFY[i] -= fy; PFX[j] += fx; PFY[j] += fy; } } } }
static void leap_f(float g, int dir) {
  if (dir < 0) fcur ^= 1;
  int n = fcur, p = fcur ^ 1; pushes_f(FXf[n], FYf[n], g);
  for (int i = 0; i < nb; i++) { FXf[p][i] = FXf[n][i] + FXf[n][i] + PFX[i] - FXf[p][i]; FYf[p][i] = FYf[n][i] + FYf[n][i] + PFY[i] - FYf[p][i]; }
  if (dir > 0) fcur ^= 1; }
static void blast_f(float cx, float cy, float str, int dir) {
  const float RB2 = 90 * 256, rr = RB2 * RB2; int n = fcur, p = fcur ^ 1;
  for (int i = 0; i < nb; i++) { float dx = FXf[n][i] - cx, dy = FYf[n][i] - cy, d2 = dx * dx + dy * dy; if (d2 >= rr) continue;
    float w = rr - d2, kx = w * dx / rr * str / 64.0f, ky = w * dy / rr * str / 64.0f - w * 96.0f * str / rr;
    FXf[p][i] -= kx * (float)dir; FYf[p][i] -= ky * (float)dir; } }

// ---------------------------------------------------------------- the tape: the only thing kept to run back is what came in from outside
#define MAXT 1500
static uint8_t TAPE[MAXT][2]; static int32_t tape_end; static uint32_t futures_dropped;

// ---------------------------------------------------------------- one instruction, either way
static void exec1(const Ins *in, int idx, int dir) {
  if (in->p != NOP_ && R[in->p] == 0) return;                      // predicated: the predicate register is never written by its own instruction
  int a = in->a, b = in->b, c = in->c; int32_t k = in->k;
  switch (in->op) {
    case O_ADD: R[a] = (int32_t)(dir > 0 ? wadd(R[a], R[b]) : (uint32_t)R[a] - (uint32_t)R[b]); break;
    case O_SUB: R[a] = (int32_t)(dir > 0 ? (uint32_t)R[a] - (uint32_t)R[b] : wadd(R[a], R[b])); break;
    case O_XOR: R[a] ^= R[b]; break;
    case O_ADDI: R[a] = (int32_t)(dir > 0 ? wadd(R[a], k) : (uint32_t)R[a] - (uint32_t)k); break;
    case O_XORI: R[a] ^= k; break;
    case O_ROL: { uint32_t v = (uint32_t)R[a]; int s = (int)(k & 31); if (s) R[a] = (int32_t)(dir > 0 ? (v << s) | (v >> (32 - s)) : (v >> s) | (v << (32 - s))); } break;
    case O_NEG: R[a] = (int32_t)(0u - (uint32_t)R[a]); break;
    case O_SWAP: { int32_t t = R[a]; R[a] = R[b]; R[b] = t; } break;
    case O_MULO: R[a] = (int32_t)((uint32_t)R[a] * (dir > 0 ? (uint32_t)k : MINV[idx & 63])); break;
    case O_ADDM: case O_SUBM: { int32_t m = (int32_t)(((int64_t)R[b] * R[c]) >> 14); int sgn = (in->op == O_ADDM ? 1 : -1) * dir;
      R[a] = (int32_t)(sgn > 0 ? wadd(R[a], m) : (uint32_t)R[a] - (uint32_t)m); } break;
    case O_TURN: turn(&R[a], &R[b], (int)k, dir); break;
    case O_EXCH: { int32_t *m = &MEM[R[b] & (MEMW - 1)]; int32_t t = R[a]; R[a] = *m; *m = t; } break;
    case O_IN: R[a] ^= TAPE[S][b]; break;
    case O_STIRX: stir_rows(R[a], R[b], dir); break;
    case O_STIRY: stir_cols(R[a], R[b], dir); break;
    case O_LEAP: leap(R[a], dir); break;
    case O_BLAST: blast(R[a], R[b], R[c], dir); break;
    case O_LEAPF: leap_f((float)R[a], dir); break;
    case O_BLASTF: blast_f((float)R[a], (float)R[b], (float)R[c], dir); break;
    default: break; } }
// one step of a program: forward runs the list down and then counts S up; back counts S down and runs the list up, each one undone
static void tick(const Prog *pg, int dir) {
  if (dir > 0) { for (int i = 0; i < pg->n; i++) exec1(&pg->code[i], i, 1); S++; }
  else { S--; for (int i = pg->n - 1; i >= 0; i--) exec1(&pg->code[i], i, -1); } }

// ---------------------------------------------------------------- the checker: a program is run only if every instruction can be undone
static const char *refusal; static char refusal_b[48];
static int writes(const Ins *in, int r) {
  switch (in->op) { case O_ADD: case O_SUB: case O_XOR: case O_ADDI: case O_XORI: case O_ROL: case O_NEG: case O_MULO: case O_ADDM: case O_SUBM: case O_EXCH: case O_IN: return in->a == r;
    case O_SWAP: case O_TURN: return in->a == r || in->b == r; default: return 0; } }
static int reads(const Ins *in, int r) {
  switch (in->op) { case O_ADD: case O_SUB: case O_XOR: return in->b == r; case O_ADDM: case O_SUBM: return in->b == r || in->c == r; case O_EXCH: return in->b == r;
    case O_STIRX: case O_STIRY: return in->a == r || in->b == r; case O_LEAP: case O_LEAPF: return in->a == r;
    case O_BLAST: case O_BLASTF: return in->a == r || in->b == r || in->c == r; default: return 0; } }
static int check_prog(const Prog *pg) {
  refusal = 0;
  for (int i = 0; i < pg->n; i++) { const Ins *in = &pg->code[i];
    if (in->op >= O_NOPS) { refusal = "NOT AN INSTRUCTION"; return 0; }
    for (int r = 0; r < NREG; r++) if (writes(in, r) && reads(in, r)) { snprintf(refusal_b, sizeof refusal_b, "%s R%d,R%d: R%d READ AND WRITTEN", ONAME[in->op], in->a, in->b, r); refusal = refusal_b; return 0; }
    if (in->p != NOP_ && writes(in, in->p)) { snprintf(refusal_b, sizeof refusal_b, "%s: WRITES ITS OWN CONDITION R%d", ONAME[in->op], in->p); refusal = refusal_b; return 0; }
    if (in->op == O_TURN && (in->a == in->b || in->k < 0 || in->k > 7)) { refusal = "TURN NEEDS TWO REGISTERS"; return 0; }
    if (in->op == O_EXCH && in->a == in->b) { refusal = "EXCH OF ITS OWN ADDRESS"; return 0; }
    if (in->op == O_MULO) { if (!(in->k & 1)) { snprintf(refusal_b, sizeof refusal_b, "MULO R%d,%d: EVEN, THE TOP BIT IS LOST", in->a, (int)in->k); refusal = refusal_b; return 0; }
      uint32_t k = (uint32_t)in->k, v = k; for (int it = 0; it < 5; it++) v *= 2u - k * v; MINV[i & 63] = v; } }   // k * v = 1 (mod 2^32)
  return 1; }

// ================================================================ THE PROGRAMS
#define I_(o, a, b, c, k) {(uint8_t)(o), (uint8_t)(a), (uint8_t)(b), (uint8_t)(c), NOP_, (int32_t)(k)}
#define IP(o, a, b, c, k, p) {(uint8_t)(o), (uint8_t)(a), (uint8_t)(b), (uint8_t)(c), (uint8_t)(p), (int32_t)(k)}
static const Ins MARBLE_C[] = {                     // a spoon goes round; the hands push the stir; the picture is stirred both ways
  I_(O_IN, 7, 0, 0, 0), I_(O_IN, 8, 1, 0, 0), I_(O_ADDI, 7, 0, 0, -128), I_(O_ADDI, 8, 0, 0, -128),
  I_(O_TURN, 1, 2, 0, 1), I_(O_ADD, 4, 1, 0, 0), I_(O_ADD, 5, 2, 0, 0), I_(O_ADD, 4, 7, 0, 0), I_(O_ADD, 5, 8, 0, 0),
  I_(O_STIRX, 3, 4, 0, 0), I_(O_STIRY, 6, 5, 0, 0),
  I_(O_ADDI, 8, 0, 0, 128), I_(O_ADDI, 7, 0, 0, 128), I_(O_IN, 8, 1, 0, 0), I_(O_IN, 7, 0, 0, 0)};
static const Ins SHATTER_C[] = {                    // the hands strike the word; the strike turns gravity on; every ball latches its rate
  I_(O_IN, 7, 0, 0, 0), I_(O_IN, 8, 1, 0, 0), I_(O_ADDM, 10, 8, 14, 0),
  IP(O_BLAST, 10, 11, 12, 0, 7), IP(O_XOR, 13, 15, 0, 0, 7),
  I_(O_SUBM, 10, 8, 14, 0), I_(O_LEAP, 13, 0, 0, 0), I_(O_IN, 8, 1, 0, 0), I_(O_IN, 7, 0, 0, 0)};
static const Ins FORGET_C[] = {
  I_(O_IN, 7, 0, 0, 0), I_(O_IN, 8, 1, 0, 0), I_(O_ADDM, 10, 8, 14, 0),
  IP(O_BLASTF, 10, 11, 12, 0, 7), IP(O_XOR, 13, 15, 0, 0, 7),
  I_(O_SUBM, 10, 8, 14, 0), I_(O_LEAPF, 13, 0, 0, 0), I_(O_IN, 8, 1, 0, 0), I_(O_IN, 7, 0, 0, 0)};
static const Ins BAD1_C[] = {I_(O_ADDI, 3, 0, 0, 5), I_(O_ADD, 3, 3, 0, 0)};                     // doubling R3 forgets its top bit
static const Ins BAD2_C[] = {I_(O_MULO, 2, 0, 0, 6)};                                            // so does any even multiplier
static const Ins BAD3_C[] = {IP(O_ADDI, 7, 0, 0, 1, 7)};                                         // a step that changes its own condition
enum { W_PICTURE, W_BALLS, W_FLOATS };
static const Prog PROGS[] = {{"MARBLE", MARBLE_C, (int)(sizeof MARBLE_C / sizeof(Ins)), W_PICTURE}, {"SHATTER", SHATTER_C, (int)(sizeof SHATTER_C / sizeof(Ins)), W_BALLS},
  {"FORGET", FORGET_C, (int)(sizeof FORGET_C / sizeof(Ins)), W_FLOATS}};
static const Prog BADS[] = {{"BAD1", BAD1_C, 2, W_PICTURE}, {"BAD2", BAD2_C, 1, W_PICTURE}, {"BAD3", BAD3_C, 1, W_PICTURE}};
#define NPROG 3

// ---------------------------------------------------------------- the starting states, grown by rules
static const uint8_t MPAL[16][3] = {{14, 26, 62}, {246, 236, 214}, {196, 40, 52}, {232, 168, 46}, {28, 140, 140}, {255, 255, 255}, {8, 8, 16}, {120, 60, 140},
  {66, 120, 200}, {250, 210, 150}, {150, 200, 90}, {230, 110, 140}, {40, 60, 110}, {255, 236, 160}, {90, 30, 40}, {170, 220, 230}};
static uint16_t PALC[16];
static void glyph_put(int x0, int y0, char ch, int sc, uint8_t col, uint8_t edge) {
  int g = GLYPH[(uint8_t)ch]; if (!g) return;
  for (int pass = 0; pass < 2; pass++) for (int r = 0; r < 7; r++) for (int k = 0; k < 5; k++) if (FACE[g - 1][r] >> (4 - k) & 1)
    for (int yy = -pass * 0; yy < sc; yy++) for (int xx = 0; xx < sc; xx++) {
      if (pass == 0) { for (int oy = -2; oy <= 2; oy++) for (int ox = -2; ox <= 2; ox++) { int x = x0 + k * sc + xx + ox, y = y0 + r * sc + yy + oy; if (x >= 0 && x < SW && y >= 0 && y < SH) CANVAS[y * SW + x] = edge; } }
      else { int x = x0 + k * sc + xx, y = y0 + r * sc + yy; if (x >= 0 && x < SW && y >= 0 && y < SH) CANVAS[y * SW + x] = col; } } }
static void text_put(int x0, int y0, const char *s, int sc, uint8_t col, uint8_t edge) { for (; *s; s++, x0 += 6 * sc) if (*s != ' ') glyph_put(x0, y0, *s, sc, col, edge); }
static void grow_picture(void) {                     // marbling drops: rings of colour from seven centres, the name on top
  static const int16_t DC[7][3] = {{70, 60, 0}, {250, 70, 2}, {160, 120, 4}, {60, 190, 1}, {270, 190, 3}, {160, 20, 5}, {160, 225, 2}};
  static const uint8_t RING[8] = {1, 2, 3, 4, 9, 8, 11, 10};
  for (int i = 0; i < SW * SH; i++) CANVAS[i] = 0;
  for (int y = 0; y < SH; y++) for (int x = 0; x < SW; x++) if (((x / 20) + (y / 20)) & 1) CANVAS[y * SW + x] = 12;
  for (int d = 0; d < 7; d++) for (int y = 0; y < SH; y++) for (int x = 0; x < SW; x++) {
    int32_t dx = x - DC[d][0], dy = y - DC[d][1], d2 = dx * dx + dy * dy; if (d2 >= 62 * 62) continue;
    int ring = isqrt32(d2) / 8; CANVAS[y * SW + x] = RING[(ring + DC[d][2]) & 7]; }
  text_put(160 - 3 * 7 * 4 + 4, 88, "TIDE", 7, 5, 6);
  text_put(160 - 3 * 2 * 22 + 2, 208, "EVERY NUMBER IS A RULE", 2, 13, 6); }
static void grow_word(void) {                        // the word as balls: each dot of the face is four balls, just touching (no push)
  static const char W4[] = "TIDE"; static const uint8_t WC[4] = {0, 1, 2, 3}; nb = 0;
  int x0 = (WW - (4 * 12 - 2) * BD) / 2 + BR, y0 = WH - BR - 13 * BD - 2 * 256;   // standing on the floor
  for (int l = 0; l < 4; l++) { int g = GLYPH[(uint8_t)W4[l]];
    for (int r = 0; r < 7; r++) for (int k = 0; k < 5; k++) if (FACE[g - 1][r] >> (4 - k) & 1)
      for (int sy = 0; sy < 2; sy++) for (int sx = 0; sx < 2; sx++) if (nb < NB) {
        SX0[nb] = x0 + (l * 12 + k * 2 + sx) * BD; SY0[nb] = y0 + (r * 2 + sy) * BD; WCOL[nb] = WC[l]; nb++; } } }
// a program's registers and world as they are at step 0
static void load_prog(int p) {
  memset(R, 0, sizeof R); memset(MEM, 0, sizeof MEM); S = 0; wcur = 0; fcur = 0;
  if (PROGS[p].world == W_PICTURE) { grow_picture(); R[1] = 70; R[2] = 0; R[3] = 2; R[6] = 2; }
  else { grow_word(); for (int i = 0; i < nb; i++) { WX[0][i] = WX[1][i] = SX0[i]; WY[0][i] = WY[1][i] = SY0[i]; FXf[0][i] = FXf[1][i] = (float)SX0[i]; FYf[0][i] = FYf[1][i] = (float)SY0[i]; }
    R[11] = (SH - 36 - 44) * 256; R[12] = 2; R[14] = WW * 64; R[15] = 6; } }
static uint32_t state_print(int world) {             // the whole machine state, as one print
  uint32_t h = 0x811c9dc5u; const uint8_t *q = (const uint8_t *)R; for (unsigned k = 0; k < sizeof R; k++) h = (h ^ q[k]) * 0x01000193u;
  q = (const uint8_t *)MEM; for (unsigned k = 0; k < sizeof MEM; k++) h = (h ^ q[k]) * 0x01000193u;
  h = (h ^ (uint32_t)S) * 0x01000193u;
  if (world == W_PICTURE) { const uint32_t *c = (const uint32_t *)CANVAS; for (int k = 0; k < SW * SH / 4; k++) { h = (h ^ c[k]) * 0x01000193u; h ^= h >> 15; } }
  else if (world == W_BALLS) { for (int i = 0; i < nb; i++) { h = (h ^ (uint32_t)WX[wcur][i]) * 0x01000193u; h = (h ^ (uint32_t)WY[wcur][i]) * 0x01000193u; h = (h ^ (uint32_t)WX[wcur ^ 1][i]) * 0x01000193u; h = (h ^ (uint32_t)WY[wcur ^ 1][i]) * 0x01000193u; } }
  else { for (int i = 0; i < nb; i++) { uint32_t a, b; memcpy(&a, &FXf[fcur][i], 4); memcpy(&b, &FYf[fcur][i], 4); h = (h ^ a) * 0x01000193u; h = (h ^ b) * 0x01000193u; } }
  return h; }

// ================================================================ DRAWING TOOLS THAT CLIP TO THE BAND
static uint16_t *RB[2]; static uint16_t *OB; static int band_y0;
static uint16_t addc(uint16_t w, int r, int g, int b) {             // light added to a dot already drawn (wire order in, wire order out)
  uint16_t v = (uint16_t)((w >> 8) | (w << 8)); int R = ((v >> 11) & 31) + (r >> 3), G = ((v >> 5) & 63) + (g >> 2), B = (v & 31) + (b >> 3);
  if (R > 31) R = 31; if (G > 63) G = 63; if (B > 31) B = 31; v = (uint16_t)(R << 11 | G << 5 | B); return (uint16_t)((v >> 8) | (v << 8)); }
static uint16_t dimc(uint16_t w) { uint16_t v = (uint16_t)((w >> 8) | (w << 8)); v = (uint16_t)((v >> 1) & 0x7BEF); return (uint16_t)((v >> 8) | (v << 8)); }
static uint16_t c565(int r, int g, int b) { return glass(r, g, b); }
static uint16_t halfc(uint16_t a, uint16_t b) { uint16_t u = (uint16_t)((a >> 8) | (a << 8)), v = (uint16_t)((b >> 8) | (b << 8)); uint16_t w = (uint16_t)(((u >> 1) & 0x7BEF) + ((v >> 1) & 0x7BEF)); return (uint16_t)((w >> 8) | (w << 8)); }
static void opix(int x, int y, uint16_t c) { if (x >= 0 && x < SW && y >= band_y0 && y < band_y0 + TH) OB[(y - band_y0) * SW + x] = c; }
static void orect(int x, int y, int w, int h, uint16_t c) { int a = y > band_y0 ? y : band_y0, b = y + h < band_y0 + TH ? y + h : band_y0 + TH;
  for (int yy = a; yy < b; yy++) for (int xx = x < 0 ? 0 : x; xx < x + w && xx < SW; xx++) OB[(yy - band_y0) * SW + xx] = c; }
static void odim(int x, int y, int w, int h) { int a = y > band_y0 ? y : band_y0, b = y + h < band_y0 + TH ? y + h : band_y0 + TH;
  for (int yy = a; yy < b; yy++) for (int xx = x < 0 ? 0 : x; xx < x + w && xx < SW; xx++) OB[(yy - band_y0) * SW + xx] = dimc(OB[(yy - band_y0) * SW + xx]); }
static int otext_s(int x, int y, const char *s, uint16_t c, int sc) {
  int h = 7 * sc; if (y > band_y0 + TH || y + h + sc < band_y0) return x + 6 * sc * (int)strlen(s);
  uint16_t sh = c565(10, 18, 40);
  for (; *s; s++) { int chh = *s; if (chh >= 'a' && chh <= 'z') chh -= 32; int g = GLYPH[chh & 127];
    if (g) for (int pass = 0; pass < 2; pass++) for (int r = 0; r < 7; r++) { uint8_t bits = FACE[g - 1][r]; for (int k = 0; k < 5; k++) if (bits >> (4 - k) & 1)
      for (int yy = 0; yy < sc; yy++) for (int xx = 0; xx < sc; xx++) opix(x + k * sc + xx + (pass ? 0 : sc > 1 ? 2 : 1), y + r * sc + yy + (pass ? 0 : sc > 1 ? 2 : 1), pass ? c : sh); }
    x += 6 * sc; }
  return x; }
static int text_w(const char *s, int sc) { return 6 * sc * (int)strlen(s) - sc; }

// ================================================================ THE DIRECTOR: which program, which way, and the ledger
enum { P_BOOT, P_FWD, P_HOLD, P_BACK, P_RESULT, P_LEDGER };
static int prog, phase, ph_t, dirv, hands_on;
static uint32_t start_print, HS[MAXT]; static int32_t matched, checked, s_max, home_n;
static uint32_t frame_n, steps_run, steps_back; static int res_ok[NPROG], res_home[NPROG]; static uint32_t res_tape[NPROG], res_snap[NPROG];
static int checks_ok[NPROG]; static const char *bad_why[3]; static char bad_b[3][48];
static int32_t tape_bytes(void) { int32_t n = 0; for (int s = 0; s < tape_end; s++) for (int k = 0; k < 2; k++) if (s == 0 ? TAPE[s][k] != TAPE[0][k] || TAPE[s][k] : TAPE[s][k] != TAPE[s - 1][k]) n += 3; return n; }   // a change: where (2 bytes) and what (1)
static uint32_t state_bytes(int w) { return w == W_PICTURE ? SW * SH + sizeof R + sizeof MEM : (uint32_t)nb * 16 + sizeof R + sizeof MEM; }
static void ghost_tape(int p) {                      // what the ghost's hands do, written before the program runs: it is input like any other
  memset(TAPE, 0, sizeof TAPE); tape_end = MAXT;
  if (PROGS[p].world == W_PICTURE) { static const uint8_t ST[10][2] = {{128, 128}, {160, 128}, {128, 104}, {100, 150}, {128, 128}, {156, 156}, {104, 110}, {128, 128}, {170, 100}, {128, 128}};
    for (int s = 0; s < MAXT; s++) { TAPE[s][0] = ST[(s / 36) % 10][0]; TAPE[s][1] = ST[(s / 36) % 10][1]; } }
  else { for (int s = 0; s < MAXT; s++) { TAPE[s][0] = 0; TAPE[s][1] = 128; } TAPE[60][0] = 1; TAPE[60][1] = 118; } }
static void start_prog(int p) {
  prog = p; load_prog(p); start_print = state_print(PROGS[p].world); HS[0] = start_print; matched = checked = 0; s_max = 0; home_n = -1;
  if (!hands_on) ghost_tape(p); else { memset(TAPE, 0, sizeof TAPE); tape_end = 0; } futures_dropped = 0; }
static int run_len(int p) { return PROGS[p].world == W_PICTURE ? 360 : 1040; }
static int per_frame(int p) { return PROGS[p].world == W_PICTURE ? 1 : 4; }
static void step_fwd(void) {
  if (S >= MAXT - 1) return; tick(&PROGS[prog], 1); steps_run++; HS[S] = state_print(PROGS[prog].world); if (S > s_max) s_max = S; dirv = 1; }
static void step_back(void) {
  if (S <= 0) return; tick(&PROGS[prog], -1); steps_back++; checked++; if (state_print(PROGS[prog].world) == HS[S]) matched++; dirv = -1; }
static int count_home(void) {                        // balls within one dot of where they began
  int n = 0; for (int i = 0; i < nb; i++) { int32_t dx, dy;
    if (PROGS[prog].world == W_FLOATS) { dx = (int32_t)FXf[fcur][i] - SX0[i]; dy = (int32_t)FYf[fcur][i] - SY0[i]; } else { dx = WX[wcur][i] - SX0[i]; dy = WY[wcur][i] - SY0[i]; }
    if (absi(dx) <= 256 && absi(dy) <= 256) n++; }
  return n; }
static void finish_prog(void) {
  res_ok[prog] = state_print(PROGS[prog].world) == start_print; res_home[prog] = PROGS[prog].world == W_PICTURE ? -1 : count_home(); home_n = res_home[prog];
  res_tape[prog] = (uint32_t)tape_bytes(); res_snap[prog] = state_bytes(PROGS[prog].world) * (uint32_t)s_max; }
static void director(void) {
  ph_t++; dirv = 0;
  switch (phase) {
    case P_BOOT: if (ph_t > 150) { phase = P_FWD; ph_t = 0; } break;
    case P_FWD: for (int k = 0; k < per_frame(prog); k++) if (S < run_len(prog)) step_fwd(); if (S >= run_len(prog)) { phase = P_HOLD; ph_t = 0; } break;
    case P_HOLD: if (ph_t > 40) { phase = P_BACK; ph_t = 0; } break;
    case P_BACK: for (int k = 0; k < per_frame(prog); k++) step_back(); if (S == 0) { finish_prog(); phase = P_RESULT; ph_t = 0; } break;
    case P_RESULT: if (ph_t > 90) { if (prog + 1 < NPROG) { start_prog(prog + 1); phase = P_FWD; } else phase = P_LEDGER; ph_t = 0; } break;
    case P_LEDGER: if (ph_t > 170) { start_prog(0); phase = P_FWD; ph_t = 0; } break; } }

// ================================================================ THE PICTURE
static uint16_t BCOL[5][8]; static uint8_t BALLM[7][7];
static const uint8_t BHUE[5][3] = {{240, 90, 70}, {250, 200, 60}, {60, 200, 150}, {80, 140, 250}, {235, 235, 245}};
static void grow_colours(void) {
  for (int i = 0; i < 16; i++) PALC[i] = glass(MPAL[i][0], MPAL[i][1], MPAL[i][2]);
  for (int h = 0; h < 5; h++) for (int l = 0; l < 8; l++) { int k = 60 + l * 34; BCOL[h][l] = glass(clampi(BHUE[h][0] * k / 256 + (l == 7 ? 60 : 0), 0, 255), clampi(BHUE[h][1] * k / 256 + (l == 7 ? 60 : 0), 0, 255), clampi(BHUE[h][2] * k / 256 + (l == 7 ? 60 : 0), 0, 255)); }
  for (int y = 0; y < 7; y++) for (int x = 0; x < 7; x++) { int dx = x - 3, dy = y - 3, d2 = dx * dx + dy * dy;   // a dome lit from up and left
    if (d2 > 11) { BALLM[y][x] = 255; continue; } int l = 5 - (dx + dy) / 2 - d2 / 4; BALLM[y][x] = (uint8_t)clampi(l, 0, 7); } }
static uint16_t SKYR[SH];
static void grow_room(void) { for (int y = 0; y < SH; y++) { int k = y * 256 / SH; SKYR[y] = glass(150 + (40 - 150) * k / 256, 200 + (70 - 200) * k / 256, 250 + (120 - 250) * k / 256); } }
static void FAST picture_band(void) {
  int y0 = band_y0; const Prog *pg = &PROGS[prog];
  if (pg->world == W_PICTURE) { for (int y = y0; y < y0 + TH; y++) { const uint8_t *c = CANVAS + y * SW; uint16_t *o = OB + (y - y0) * SW; for (int x = 0; x < SW; x++) o[x] = PALC[c[x]]; } return; }
  for (int y = y0; y < y0 + TH; y++) { uint16_t *o = OB + (y - y0) * SW, c = SKYR[y]; for (int x = 0; x < SW; x++) o[x] = c; }
  if (pg->world == W_FLOATS && (phase == P_RESULT || phase == P_LEDGER)) for (int i = 0; i < nb; i++) { int cx = SX0[i] >> 8, cy = SY0[i] >> 8;   // where they should be
    if (cy >= y0 && cy < y0 + TH) { uint16_t *o = OB + (cy - y0) * SW; for (int k = -1; k <= 1; k++) if (cx + k >= 0 && cx + k < SW) o[cx + k] = glass(255, 255, 255); } }
  for (int i = 0; i < nb; i++) { int32_t bx, by;
    if (pg->world == W_FLOATS) { bx = (int32_t)FXf[fcur][i]; by = (int32_t)FYf[fcur][i]; } else { bx = WX[wcur][i]; by = WY[wcur][i]; }
    int cx = bx >> 8, cy = by >> 8; if (cy + 3 < y0 || cy - 3 >= y0 + TH) continue; const uint16_t *col = BCOL[WCOL[i]];
    for (int r = 0; r < 7; r++) { int y = cy - 3 + r; if (y < y0 || y >= y0 + TH || y >= SH) continue; uint16_t *o = OB + (y - y0) * SW;
      for (int k = 0; k < 7; k++) { int x = cx - 3 + k; if (x < 0 || x >= SW) continue; uint8_t m = BALLM[r][k]; if (m != 255) o[x] = col[m]; } } } }

// ================================================================ THE OVERLAY: the ledger, the program as it is running, the verdicts
static void ins_text(const Ins *in, int back, char *b, int n) {
  const char *nm = back ? OBACK[in->op] : ONAME[in->op]; int32_t k = in->k;
  switch (in->op) {
    case O_ADDI: snprintf(b, n, "%s R%d,%d", nm, in->a, (int)(back ? -k : k)); break;
    case O_XORI: case O_ROL: snprintf(b, n, "%s R%d,%d", nm, in->a, (int)k); break;
    case O_NEG: case O_LEAP: case O_LEAPF: snprintf(b, n, "%s R%d", nm, in->a); break;
    case O_IN: snprintf(b, n, "%s R%d,HAND%d", nm, in->a, in->b); break;
    case O_TURN: snprintf(b, n, "%s R%d,R%d,%s(%d:%d)", nm, in->a, in->b, back ? "-" : "", HP[k], HQ[k]); break;
    case O_ADDM: case O_SUBM: case O_BLAST: case O_BLASTF: snprintf(b, n, "%s R%d,R%d,R%d", nm, in->a, in->b, in->c); break;
    default: snprintf(b, n, "%s R%d,R%d", nm, in->a, in->b); break; }
  if (in->p != NOP_) { int l = (int)strlen(b); snprintf(b + l, n - l, " IF R%d", in->p); } }
static void listing(int x0, int y0, int back) {        // the program, as it runs this way: back is the same list, upward, each step undone
  const Prog *pg = &PROGS[prog]; int h = pg->n * 9 + 16; if (y0 > band_y0 + TH || y0 + h < band_y0) return;
  odim(x0 - 4, y0 - 4, SW - x0 + 4, h); odim(x0 - 4, y0 - 4, SW - x0 + 4, h);
  otext_s(x0, y0, back ? "RUNNING UP:" : "RUNNING DOWN:", back ? c565(255, 150, 120) : c565(140, 230, 255), 1);
  for (int i = 0; i < pg->n; i++) { int j = back ? pg->n - 1 - i : i; char b[40]; ins_text(&pg->code[j], back, b, sizeof b);
    otext_s(x0, y0 + 12 + i * 9, b, back ? c565(255, 220, 200) : c565(220, 245, 255), 1); } }
static int32_t tb_frame;
static void fmt_bytes(char *b, int n, uint32_t v) { if (v >= 1000000) snprintf(b, n, "%u.%u MB", (unsigned)(v / 1000000), (unsigned)(v % 1000000 / 100000)); else if (v >= 1000) snprintf(b, n, "%u.%u KB", (unsigned)(v / 1000), (unsigned)(v % 1000 / 100)); else snprintf(b, n, "%u B", (unsigned)v); }
static void overlay_band(void) {
  int y0 = band_y0, y1 = band_y0 + TH; char b[64], c[24];
  if (phase == P_BOOT) {
    if (y0 < 200) odim(0, 0, SW, 200);
    otext_s(SW / 2 - text_w("TIDE", 6) / 2, 14, "TIDE", c565(120, 220, 255), 6);
    otext_s(SW / 2 - text_w("A COMPUTER THAT RUNS BACKWARD", 1) / 2, 64, "A COMPUTER THAT RUNS BACKWARD", c565(255, 255, 255), 1);
    otext_s(14, 84, "THE CHECKER READ EACH PROGRAM:", c565(255, 236, 160), 1);
    for (int p = 0; p < NPROG; p++) { snprintf(b, sizeof b, "%-8s %s", PROGS[p].name, checks_ok[p] ? "EVERY STEP CAN BE UNDONE" : "REFUSED"); otext_s(20, 98 + p * 11, b, c565(140, 255, 160), 1); }
    for (int k = 0; k < 3; k++) { snprintf(b, sizeof b, "REFUSED: %s", bad_why[k] ? bad_why[k] : "?"); otext_s(20, 136 + k * 11, b, c565(255, 120, 110), 1); }
    otext_s(14, 178, "ONLY WHAT COMES IN FROM THE HANDS IS KEPT.", c565(255, 255, 255), 1);
    return; }
  const Prog *pg = &PROGS[prog];
  if (phase == P_LEDGER) {
    if (y0 < 230) odim(0, 0, SW, 232); if (y0 < 230) odim(0, 0, SW, 232);
    otext_s(SW / 2 - text_w("THE LEDGER", 3) / 2, 10, "THE LEDGER", c565(120, 220, 255), 3);
    snprintf(b, sizeof b, "%u STEPS FORWARD, %u BACK", (unsigned)steps_run, (unsigned)steps_back); otext_s(SW / 2 - text_w(b, 1) / 2, 42, b, c565(255, 255, 255), 1);
    for (int p = 0; p < NPROG; p++) { int y = 64 + p * 40; uint16_t ok = c565(140, 255, 160), no = c565(255, 110, 100);
      otext_s(14, y, PROGS[p].name, c565(255, 236, 160), 2);
      if (PROGS[p].world == W_PICTURE) snprintf(b, sizeof b, "%s", res_ok[p] ? "EVERY DOT CAME BACK" : "NOT BACK");
      else snprintf(b, sizeof b, "%d OF %d BALLS HOME%s", res_home[p], nb, res_ok[p] ? ", EVERY BIT" : "");
      otext_s(110, y + 2, b, res_ok[p] ? ok : no, 1);
      fmt_bytes(c, sizeof c, res_tape[p]); snprintf(b, sizeof b, "KEPT: HANDS %s", c); otext_s(110, y + 14, b, c565(220, 230, 255), 1);
      fmt_bytes(c, sizeof c, res_snap[p]); snprintf(b, sizeof b, "SNAPSHOTS WOULD BE %s", c); otext_s(110, y + 24, b, c565(170, 180, 210), 1); }
    otext_s(SW / 2 - text_w("A FLOAT ROUNDS, AND A ROUNDING IS A LOST BIT.", 1) / 2, 192, "A FLOAT ROUNDS, AND A ROUNDING IS A LOST BIT.", c565(255, 200, 160), 1);
    otext_s(SW / 2 - text_w("TIDE NEVER ROUNDS AWAY WHAT IT CANNOT GIVE BACK.", 1) / 2, 206, "TIDE NEVER ROUNDS AWAY WHAT IT CANNOT GIVE BACK.", c565(255, 255, 255), 1);
    return; }
  if (y0 < 34) { odim(0, 0, 170, 32);
    otext_s(6, 4, pg->name, c565(255, 255, 255), 2);
    snprintf(b, sizeof b, "STEP %d", (int)S); otext_s(6, 22, b, c565(220, 230, 255), 1);
    uint16_t dc = dirv > 0 ? c565(140, 230, 255) : dirv < 0 ? c565(255, 150, 120) : c565(200, 200, 200);
    otext_s(66, 22, dirv > 0 ? ">> FORWARD" : dirv < 0 ? "<< BACKWARD" : "|| HELD", dc, 1); }
  if (y1 > SH - 40) { odim(0, SH - 34, SW, 34);
    int32_t tb = tb_frame; fmt_bytes(c, sizeof c, (uint32_t)tb); snprintf(b, sizeof b, "KEPT: THE HANDS, %s", c); otext_s(6, SH - 30, b, c565(255, 236, 160), 1);
    fmt_bytes(c, sizeof c, state_bytes(pg->world) * (uint32_t)(s_max > 0 ? s_max : 0)); snprintf(b, sizeof b, "SNAPSHOTS: %s", c); otext_s(6, SH - 20, b, c565(190, 200, 220), 1);
    if (checked) { snprintf(b, sizeof b, "BACK MATCHED %d/%d", (int)matched, (int)checked); otext_s(SW - 6 - text_w(b, 1), SH - 30, b, matched == checked ? c565(140, 255, 160) : c565(255, 110, 100), 1); }
    if (futures_dropped) { snprintf(b, sizeof b, "FUTURES DROPPED %u B", (unsigned)futures_dropped); otext_s(SW - 6 - text_w(b, 1), SH - 20, b, c565(255, 180, 120), 1); }
    int w = s_max > 0 ? (int)S * (SW - 12) / (s_max > run_len(prog) ? s_max : run_len(prog)) : 0; orect(6, SH - 8, SW - 12, 3, c565(40, 50, 80)); orect(6, SH - 8, w, 3, dirv < 0 ? c565(255, 150, 120) : c565(140, 230, 255)); }
  if ((phase == P_FWD && ph_t < 110) || (phase == P_BACK && ph_t < 110)) listing(SW - 150, 40, phase == P_BACK);
  if (phase == P_RESULT) { if (y0 < 150 && y1 > 86) odim(0, 86, SW, 64);
    if (res_ok[prog]) { otext_s(SW / 2 - text_w("STATE = START", 3) / 2, 92, "STATE = START", c565(140, 255, 160), 3);
      otext_s(SW / 2 - text_w("EVERY BIT CAME BACK", 1) / 2, 120, "EVERY BIT CAME BACK", c565(255, 255, 255), 1); }
    else { snprintf(b, sizeof b, "%d OF %d HOME", home_n, nb); otext_s(SW / 2 - text_w(b, 3) / 2, 92, b, c565(255, 110, 100), 3);
      otext_s(SW / 2 - text_w("THE SAME RULE IN FLOATS: IT CANNOT FIND THE WAY BACK", 1) / 2, 120, "THE SAME RULE IN FLOATS: IT CANNOT FIND THE WAY BACK", c565(255, 255, 255), 1); }
    snprintf(b, sizeof b, "START %08X  NOW %08X", (unsigned)start_print, (unsigned)state_print(pg->world)); otext_s(SW / 2 - text_w(b, 1) / 2, 136, b, c565(200, 210, 230), 1); } }

// ================================================================ THE HANDS
static int a_down, a_tap, b_down, c_tap, hands_seen; static int32_t stick_x, stick_y;
#if defined(ON_BOARD) && !defined(GHOST_ONLY)
#define LIVE_HANDS 1
static int jx0 = 2048, jy0 = 2048, la, lc;
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
  int a = !digitalRead(PIN_A), b = !digitalRead(PIN_B), c = !digitalRead(PIN_C) || !digitalRead(PIN_JSW);
  stick_x = rx; stick_y = ry; a_down = a; b_down = b; a_tap = a && !la; c_tap = c && !lc; la = a; lc = c;
  return rx || ry || a || b || c; }
// hands playing: hold B and time runs back; let go and it runs on, replaying what was kept unless the hands now do something new
static void hands_step(void) {
  if (phase == P_BOOT || phase == P_LEDGER || phase == P_RESULT) { phase = P_FWD; ph_t = 0; }
  if (c_tap) { start_prog((prog + 1) % NPROG); phase = P_FWD; ph_t = 0; return; }
  int k = per_frame(prog);
  if (b_down) { phase = P_BACK; for (int i = 0; i < k; i++) step_back(); return; }
  phase = P_FWD;
  for (int i = 0; i < k; i++) { if (S >= MAXT - 1) break;
    uint8_t h0, h1; if (PROGS[prog].world == W_PICTURE) { h0 = (uint8_t)(128 + stick_x); h1 = (uint8_t)(128 + stick_y); } else { h0 = (uint8_t)(a_tap && i == 0); h1 = (uint8_t)(128 + stick_x); }
    int fresh = PROGS[prog].world == W_PICTURE ? (stick_x || stick_y) : h0;
    if (S < tape_end && fresh && (TAPE[S][0] != h0 || TAPE[S][1] != h1)) { futures_dropped += (uint32_t)(tape_end - S) * 2; tape_end = S; }   // a new future: the old one is let go
    if (S >= tape_end) { TAPE[S][0] = h0; TAPE[S][1] = h1; tape_end = S + 1; }
    step_fwd(); } }
#endif

// ================================================================ THE FRAME
static uint32_t wire_print, c_world, c_draw, f_tiles;
#ifdef DESK
static uint16_t SHOWN[NTY][SW * TH];
#endif
static void FAST send_band(int b, uint16_t *rb) {
#ifndef ON_BOARD
  uint32_t h = 0x811c9dc5u; const uint32_t *q = (const uint32_t *)rb; for (int k = 0; k < SW * TH / 2; k++) { h = (h ^ q[k]) * 0x01000193u; h ^= h >> 15; }
  wire_print = (wire_print ^ h) * 0x01000193u;
#endif
#ifdef DESK
  memcpy(SHOWN[b], rb, sizeof SHOWN[b]);
#endif
  window(0, SW - 1, b * TH, b * TH + TH - 1); pix(rb, SW * TH * 2); f_tiles++; }
static uint32_t slot_n;
void frame(void) {
  uint32_t c0 = cyc(); f_bytes = 0; f_tiles = 0;
#ifdef LIVE_HANDS
  { int hand = pad_read(); if (hand) hands_seen = 1;
    if (hand && !hands_on) { hands_on = 1; start_prog(prog); phase = P_FWD; ph_t = 0; }
    if (hands_on) hands_step(); else director(); }
#else
  director();
#endif
  tb_frame = tape_bytes(); uint32_t c1 = cyc(); wire_print = 0x811c9dc5u;
  for (int b = 0; b < NTY; b++) { band_y0 = b * TH; int s = (int)(slot_n++ & 1); OB = RB[s]; m_wait(s); picture_band(); overlay_band(); send_band(b, OB); m_fence(s); }
  c_world = c1 - c0; c_draw = cyc() - c1; frame_n++; }
#ifdef DESK
static uint32_t desk_expected_print(void) { uint32_t h = 0x811c9dc5u;
  for (int b = 0; b < NTY; b++) for (int k = 0; k < SW * TH; k++) { uint16_t v = SHOWN[b][k]; v = (uint16_t)((v >> 8) | (v << 8)); h = (h ^ (v & 255)) * 0x01000193u; h = (h ^ (v >> 8)) * 0x01000193u; } return h; }
#endif
#define BOOK_LINE "BOOKS forward %u back %u marble %d shatter %d/%d %d forget %d/%d %d\n", (unsigned)steps_run, (unsigned)steps_back, res_ok[0], res_home[1], nb, res_ok[1], res_home[2], nb, res_ok[2]
#define STATE_LINE "prog %d phase %d S %d matched %d/%d print %08x", prog, phase, (int)S, (int)matched, (int)checked, (unsigned)HS[S > 0 ? S : 0]
#ifdef DESK
static void desk_line(void) { printf(STATE_LINE); }
static void desk_books(void) { printf(BOOK_LINE); }
#endif
static uint32_t glass_print(void) { return wire_print; }
void setup(void) {
#if !defined(DESK) && FRAMES > 0
  Serial0.begin(921600);
#elif defined(ON_BOARD)
  Serial.begin(115200); delay(300);
#endif
  STAGE("TIDE 1 boot\n");
  for (int i = 0; i < 2; i++) RB[i] = (uint16_t *)HEAP_DMA(SW * TH * 2);
  STAGE("TIDE 2 strips %s\n", RB[0] && RB[1] ? "ok" : "REFUSED");
  grow_face(); grow_ring(); grow_colours(); grow_room();
  for (int p = 0; p < NPROG; p++) checks_ok[p] = check_prog(&PROGS[p]);
  for (int k = 0; k < 3; k++) { int ok = check_prog(&BADS[k]); snprintf(bad_b[k], sizeof bad_b[k], "%s", ok ? "(ACCEPTED?)" : refusal); bad_why[k] = bad_b[k]; }
  for (int p = 0; p < NPROG; p++) check_prog(&PROGS[p]);                 // the inverse multipliers, for the programs that run
#ifdef LIVE_HANDS
  pad_begin();
#endif
  panel_begin();
  STAGE("TIDE 3 checked: %d %d %d; refused: %s | %s | %s\n", checks_ok[0], checks_ok[1], checks_ok[2], bad_why[0], bad_why[1], bad_why[2]);
  start_prog(0); phase = P_BOOT; ph_t = 0; }
#ifndef DESK
static uint32_t rep_n, rep_sum, rep_max, last_us;
void loop(void) {
#if FRAMES == 0 && !defined(MOUTH_FIFO)
  uint32_t now = micros(), us = last_us ? now - last_us : 0; last_us = now;
#endif
  frame();
#if FRAMES > 0
  Serial0.printf("F %u glass %08x world %u draw %u bytes %u ", (unsigned)frame_n, (unsigned)glass_print(), (unsigned)c_world, (unsigned)c_draw, (unsigned)f_bytes);
  Serial0.printf(STATE_LINE); Serial0.printf("\n");
  if (frame_n >= FRAMES) { m_wait_all(); Serial0.printf(BOOK_LINE);
    Serial0.printf("STACK free %u words\n", (unsigned)uxTaskGetStackHighWaterMark(NULL)); Serial0.printf("HEAP free %u\n", (unsigned)ESP.getFreeHeap()); Serial0.printf("DONE\n");
    while (1) delay(1000); }
#elif defined(ON_BOARD)
  if (us) { rep_sum += us; if (us > rep_max) rep_max = us; }
  if (++rep_n >= 200) { Serial.printf("TIDE REAL frame avg %u us worst %u us | %s S %d\n", (unsigned)(rep_sum / rep_n), (unsigned)rep_max, PROGS[prog].name, (int)S); rep_n = 0; rep_sum = 0; rep_max = 0; }
#endif
}
#endif
