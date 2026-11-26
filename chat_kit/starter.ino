// STARTER: the template every chat build begins from. ESP32-C3 SuperMini + 2.0" ST7789 (320 x 240).
// It already does the hard parts that are not the idea: the panel, the wire, the letters, the frame loop, the books line.
// YOUR WORK GOES ONLY BETWEEN THE TWO "RULES" MARKERS BELOW. Everything else is board-proven (BIGRACE's mouths and face).
// What it shows as it ships: a dot that steps 7/3 of a cell a latch by the carry (whole pulses, the remainder kept),
// along the exact heading (3 : 4), unit (7, 24) / 25, reflecting at the walls, and the books line: 3 * pulses + carry == 7 * latches, every latch.
// Wiring: SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3; button A to GPIO 3 (to ground). Arduino IDE: "ESP32C3 Dev Module",
// core 3.x, USB CDC On Boot: Enabled. Serial 115200 prints one line every 100 latches.
// ---- fixes (uncomment):
//#define BGR_ORDER    // red and blue swapped
//#define NO_INVERT    // colours look like a negative
//#define ROT180       // upside down
//#define SAFE_SPI     // speckles or streaks: 40 MHz wire (default 80 MHz)
//#define WIRE_20      // still bad: 20 MHz
//#define NO_PACE      // run unpaced (default: a latch every 40 ms at most)
#include <stdint.h>
#include <string.h>
#ifndef DESK
#include <Arduino.h>
#ifdef MOUTH_FIFO
#include <SPI.h>
#else
#include "driver/spi_master.h"
#include "driver/gpio.h"
#endif
#else
#include <stdio.h>
#endif
#define PIN_SCK 4
#define PIN_MOSI 6
#define PIN_RST 10
#define PIN_DC 5
#define PIN_CS 7
#define PIN_A 3
#if defined(WIRE_20)
#define WIRE_HZ 20000000L
#elif defined(SAFE_SPI)
#define WIRE_HZ 40000000L
#else
#define WIRE_HZ 80000000L
#endif
#ifndef FRAMES
#define FRAMES 0
#endif
#ifndef START_F
#define START_F 0
#endif
#if defined(DESK) || defined(MOUTH_FIFO) || FRAMES > 0
#define BENCH 1
#else
#define BENCH 0
#endif
// ---------------------------------------------------------------- every type and size first (the IDE puts its prototypes above the first function)
#define SW 320
#define SH 240
#define BANDH 16
#define NBAND (SH / BANDH)
#define NBUF 2
typedef struct { int16_t x, y; uint16_t col; uint8_t sc; char s[54]; } Item;
typedef struct { int32_t n, d, acc; int64_t count; } Carry;     // a rate n/d held as whole pulses and a remainder
// ---------------------------------------------------------------- the mouths (DESK twin, FIFO for the emulator, the DMA queue on the board)
#ifdef DESK
void desk_cmd(uint8_t c, const uint8_t *p, int n);
void desk_data(const uint8_t *p, int n);
static void m_cmd(uint8_t c, const uint8_t *p, int n) { desk_cmd(c, p, n); }
static void m_data(const void *p, int nb) { desk_data((const uint8_t *)p, nb); }
static void m_wait_all(void) {}
static uint32_t m_mark(void) { return 0; }
static void m_wait_mark(uint32_t m) { (void)m; }
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
static void m_wait_all(void) {}
static uint32_t m_mark(void) { return 0; }
static void m_wait_mark(uint32_t m) { (void)m; }
static void m_begin(void) {
  pinMode(PIN_DC, OUTPUT); pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  SPI.begin(PIN_SCK, -1, PIN_MOSI, -1); SPI.beginTransaction(SPISettings(WIRE_HZ, MSBFIRST, SPI_MODE0)); digitalWrite(PIN_CS, LOW); }
static void hold_ms(int ms) { delay(ms); }
static uint32_t cyc(void) { return ESP.getCycleCount(); }
#else
#define QN 64
static spi_device_handle_t dev; static int m_e1 = -1, m_e2 = -1;
static spi_transaction_t TQ[QN]; static uint8_t PRM[QN][8]; static uint32_t q_in, q_done;
static void IRAM_ATTR dc_cb(spi_transaction_t *t) { gpio_set_level((gpio_num_t)PIN_DC, (int)(intptr_t)t->user); }
static void q_reap(void) { spi_transaction_t *r; spi_device_get_trans_result(dev, &r, portMAX_DELAY); q_done++; }
static spi_transaction_t *q_slot(int *k) { if (q_in - q_done >= QN) q_reap(); *k = (int)(q_in % QN); spi_transaction_t *t = &TQ[*k]; memset(t, 0, sizeof *t); return t; }
static void q_go(spi_transaction_t *t) { spi_device_queue_trans(dev, t, portMAX_DELAY); q_in++; }
static void m_cmd(uint8_t c, const uint8_t *p, int n) {
  int k; spi_transaction_t *t = q_slot(&k); t->length = 8; t->flags = SPI_TRANS_USE_TXDATA; t->tx_data[0] = c; t->user = (void *)0; q_go(t);
  if (n) { t = q_slot(&k); t->length = 8 * n; t->user = (void *)1;
    if (n <= 4) { t->flags = SPI_TRANS_USE_TXDATA; memcpy(t->tx_data, p, n); } else { memcpy(PRM[k], p, n); t->tx_buffer = PRM[k]; }
    q_go(t); } }
static void m_data(const void *p, int nb) { int k; spi_transaction_t *t = q_slot(&k); t->length = 8 * nb; t->tx_buffer = p; t->user = (void *)1; q_go(t); }
static void m_wait_all(void) { while (q_done != q_in) q_reap(); }
static uint32_t m_mark(void) { return q_in; }
static void m_wait_mark(uint32_t m) { while ((int32_t)(q_done - m) < 0) q_reap(); }
static void m_begin(void) {
  gpio_set_direction((gpio_num_t)PIN_DC, GPIO_MODE_OUTPUT);
  spi_bus_config_t bus; memset(&bus, 0, sizeof bus);
  bus.mosi_io_num = PIN_MOSI; bus.miso_io_num = -1; bus.sclk_io_num = PIN_SCK; bus.quadwp_io_num = -1; bus.quadhd_io_num = -1; bus.max_transfer_sz = SW * BANDH * 2 + 64;
  m_e1 = (int)spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
  spi_device_interface_config_t d; memset(&d, 0, sizeof d);
  d.clock_speed_hz = WIRE_HZ; d.mode = 0; d.spics_io_num = PIN_CS; d.queue_size = QN; d.pre_cb = dc_cb;
  m_e2 = (int)spi_bus_add_device(SPI2_HOST, &d, &dev); }
static void hold_ms(int ms) { m_wait_all(); delay(ms); }
static uint32_t cyc(void) { return ESP.getCycleCount(); }
#endif
#define HOT __attribute__((noinline, optimize("O2")))
// ---------------------------------------------------------------- the font (SHINEN's 5 x 7)
static const char FONT_CH[] = " !#%'()*+,-./0123456789:<=>?ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_fx|";
static const uint8_t FONT[61][7] = {
  { 0, 0, 0, 0, 0, 0, 0 }, { 4, 4, 4, 4, 4, 0, 4 }, { 10, 10, 31, 10, 31, 10, 10 }, { 24, 25, 2, 4, 8, 19, 3 }, { 4, 4, 8, 0, 0, 0, 0 },
  { 2, 4, 8, 8, 8, 4, 2 }, { 8, 4, 2, 2, 2, 4, 8 }, { 0, 4, 21, 14, 21, 4, 0 }, { 0, 4, 4, 31, 4, 4, 0 }, { 0, 0, 0, 0, 12, 4, 8 },
  { 0, 0, 0, 31, 0, 0, 0 }, { 0, 0, 0, 0, 0, 12, 12 }, { 1, 1, 2, 4, 8, 16, 16 }, { 14, 17, 19, 21, 25, 17, 14 }, { 4, 12, 4, 4, 4, 4, 14 },
  { 14, 17, 1, 2, 4, 8, 31 }, { 31, 2, 4, 2, 1, 17, 14 }, { 2, 6, 10, 18, 31, 2, 2 }, { 31, 16, 30, 1, 1, 17, 14 }, { 6, 8, 16, 30, 17, 17, 14 },
  { 31, 1, 2, 4, 8, 8, 8 }, { 14, 17, 17, 14, 17, 17, 14 }, { 14, 17, 17, 15, 1, 2, 12 }, { 0, 12, 12, 0, 12, 12, 0 }, { 2, 4, 8, 16, 8, 4, 2 },
  { 0, 0, 31, 0, 31, 0, 0 }, { 8, 4, 2, 1, 2, 4, 8 }, { 14, 17, 1, 2, 4, 0, 4 }, { 14, 17, 17, 31, 17, 17, 17 }, { 30, 17, 17, 30, 17, 17, 30 },
  { 14, 17, 16, 16, 16, 17, 14 }, { 30, 17, 17, 17, 17, 17, 30 }, { 31, 16, 16, 30, 16, 16, 31 }, { 31, 16, 16, 30, 16, 16, 16 }, { 14, 17, 16, 23, 17, 17, 15 },
  { 17, 17, 17, 31, 17, 17, 17 }, { 14, 4, 4, 4, 4, 4, 14 }, { 7, 2, 2, 2, 2, 18, 12 }, { 17, 18, 20, 24, 20, 18, 17 }, { 16, 16, 16, 16, 16, 16, 31 },
  { 17, 27, 21, 21, 17, 17, 17 }, { 17, 25, 21, 19, 17, 17, 17 }, { 14, 17, 17, 17, 17, 17, 14 }, { 30, 17, 17, 30, 16, 16, 16 }, { 14, 17, 17, 17, 21, 18, 13 },
  { 30, 17, 17, 30, 20, 18, 17 }, { 15, 16, 16, 14, 1, 1, 30 }, { 31, 4, 4, 4, 4, 4, 4 }, { 17, 17, 17, 17, 17, 17, 14 }, { 17, 17, 17, 17, 10, 10, 4 },
  { 17, 17, 17, 21, 21, 27, 17 }, { 17, 17, 10, 4, 10, 17, 17 }, { 17, 17, 10, 4, 4, 4, 4 }, { 31, 1, 2, 4, 8, 16, 31 }, { 14, 8, 8, 8, 8, 8, 14 },
  { 14, 2, 2, 2, 2, 2, 14 }, { 4, 10, 17, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 31 }, { 6, 9, 8, 28, 8, 8, 8 }, { 0, 0, 17, 10, 4, 10, 17 },
  { 4, 4, 4, 4, 4, 4, 4 } };
static uint8_t GLY[128];
static uint16_t rgb(int r, int g, int b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }
static int band_y0;   // the band being painted
static void fill(uint16_t *o, int x, int y, int w, int h, uint16_t c) {
  int y0 = y < band_y0 ? band_y0 : y, y1 = y + h > band_y0 + BANDH ? band_y0 + BANDH : y + h; if (x < 0) { w += x; x = 0; } if (x + w > SW) w = SW - x;
  for (int yy = y0; yy < y1; yy++) { uint16_t *row = o + (yy - band_y0) * SW + x; for (int i = 0; i < w; i++) row[i] = c; } }
static void dot(uint16_t *o, int x, int y, uint16_t c) { if (y >= band_y0 && y < band_y0 + BANDH && x >= 0 && x < SW) o[(y - band_y0) * SW + x] = c; }
static Item IT[40]; static int n_it;
static Item *say(int x, int y, int sc, uint16_t col) { Item *t = &IT[n_it < 39 ? n_it++ : 39]; t->x = (int16_t)x; t->y = (int16_t)y; t->sc = (uint8_t)sc; t->col = col; t->s[0] = 0; return t; }
static char *put_s(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }
static char *put_u64(char *p, uint64_t v) { char b[24]; int n = 0; do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *p++ = b[--n]; *p = 0; return p; }
static char *put_u(char *p, uint32_t v) { return put_u64(p, v); }
static char *put_hex(char *p, uint32_t v, int nd) { for (int i = nd - 1; i >= 0; i--) *p++ = "0123456789ABCDEF"[(v >> (4 * i)) & 15]; *p = 0; return p; }
static int slen(const char *s) { int k = 0; while (s[k]) k++; return k; }
static void words_band(uint16_t *o) {
  for (int k = 0; k < n_it; k++) { Item *t = &IT[k]; int sc = t->sc; if (t->y + 7 * sc <= band_y0 || t->y >= band_y0 + BANDH) continue;
    for (int c = 0; t->s[c]; c++) { int g = GLY[(uint8_t)t->s[c]]; if (!g) continue; const uint8_t *f = FONT[g - 1];
      for (int ry = 0; ry < 7 * sc; ry++) { int y = t->y + ry - band_y0; if (y < 0 || y >= BANDH) continue; uint8_t bits = f[ry / sc];
        for (int rx = 0; rx < 5 * sc; rx++) if (bits & (16 >> (rx / sc))) { int x = t->x + c * 6 * sc + rx; if (x >= 0 && x < SW) o[y * SW + x] = t->col; } } } } }
#ifdef DESK
#define SAY(...) fprintf(stderr, __VA_ARGS__)
#elif BENCH
#define SAY(...) Serial0.printf(__VA_ARGS__)
#else
#define SAY(...) Serial.printf(__VA_ARGS__)
#endif
// ---------------------------------------------------------------- the book's tools in whole numbers (do not change; use them)
static int32_t mouth(int32_t top, int32_t den) {   // land top/den once: nearest whole, an exact half to the even one, mouth(-x) = -mouth(x). den > 0
  int32_t a = top < 0 ? -top : top, q = a / den, r = a % den;
  if (2 * (int64_t)r > den || (2 * (int64_t)r == den && (q & 1))) q++;
  return top < 0 ? -q : q; }
static int carry_tick(Carry *c) {   // add n; emit a pulse while the remainder reaches d. Books: d * count + acc == n * ticks
  int e = 0; c->acc += c->n; while (c->acc >= c->d) { c->acc -= c->d; e++; c->count++; } return e; }
static int pop_carry_ok(const Carry *c, int64_t ticks) { return (int64_t)c->d * c->count + c->acc == (int64_t)c->n * ticks && c->acc >= 0 && c->acc < c->d; }
// ---------------------------------------------------------------- the picture's tools
#define INK rgb(9, 12, 26)
#define DIM rgb(150, 160, 185)
#define TXT rgb(230, 222, 205)
#define GOLD rgb(255, 190, 90)
#define OKC rgb(120, 230, 140)
#define BAD rgb(255, 90, 80)
static uint16_t STRIP[NBUF][SW * BANDH]; static uint32_t BUFM[NBUF]; static uint32_t frame_n;
// ================================================================ RULES BEGIN: write your sketch's rules here
// State: whole numbers with a named bottom. Change: a rate latched into a value, or an amount moved. No float, no clock.
#define CELL 16                         // the dot's place is held in 1/16 of a pixel (the bottom)
static int32_t px = 40 * CELL, py = 60 * CELL;     // place, in 1/16 px
// the heading (3 : 4): a direction as two whole numbers P : Q. Its unit is (Q*Q - P*P, 2*P*Q) / (P*P + Q*Q) = (7, 24) / 25
static int32_t dx = 7, dy = 24, dd = 25;           // the heading's unit, exact: 7^2 + 24^2 = 25^2
static Carry speed = { 7 * CELL, 3, 0, 0 };        // 7/3 px a latch, as pulses of 1/16 px
static uint32_t ticks, bounces; static int books_ok = 1;
static void rules_begin(void) { }
static void rules_step(void) {                     // one latch: carry the speed into pulses, move by the heading, turn at walls
  int pulses = carry_tick(&speed); ticks++;
  px += mouth(pulses * dx, dd); py += mouth(pulses * dy, dd);       // a pulse along (dx, dy)/dd, landed once
  if (px < 4 * CELL || px > (SW - 4) * CELL) { dx = -dx; bounces++; px = px < 4 * CELL ? 4 * CELL : (SW - 4) * CELL; }
  if (py < 4 * CELL || py > (200 - 4) * CELL) { dy = -dy; bounces++; py = py < 4 * CELL ? 4 * CELL : (200 - 4) * CELL; }
  if (!pop_carry_ok(&speed, ticks)) books_ok = 0; }
static void rules_band(uint16_t *o) {              // draw your state into one 16-row band (band_y0 = its first row)
  int x = mouth(px, CELL), y = mouth(py, CELL); fill(o, x - 3, y - 3, 7, 7, GOLD); fill(o, 0, 200, SW, 1, DIM); }
static void rules_words(void) {                    // the text: what the state is, and its books
  Item *t; char *p;
  t = say(6, 206, 1, TXT); p = put_s(t->s, "STARTER  LATCH "); p = put_u(p, ticks); p = put_s(p, "  BOUNCES "); put_u(p, bounces);
  t = say(6, 216, 1, DIM); p = put_s(t->s, "SPEED 7/3 PX: PULSES "); p = put_u64(p, (uint64_t)speed.count); p = put_s(p, " CARRY "); put_u(p, (uint32_t)speed.acc);
  t = say(6, 226, 1, books_ok ? OKC : BAD); put_s(t->s, books_ok ? "BOOKS 3*PULSES+CARRY == 7*16*LATCHES: HELD" : "BOOKS OPEN"); }
// ================================================================ RULES END
static void band(int b, uint16_t *o) { band_y0 = b * BANDH; uint16_t ink = INK; for (int i = 0; i < SW * BANDH; i++) o[i] = ink; rules_band(o); words_band(o); }
static void frame(void) {
  rules_step(); n_it = 0; rules_words();
  { uint8_t a[4] = { 0, 0, (uint8_t)((SW - 1) >> 8), (uint8_t)(SW - 1) }; m_cmd(0x2A, a, 4); }
  { uint8_t a[4] = { 0, 0, (uint8_t)((SH - 1) >> 8), (uint8_t)(SH - 1) }; m_cmd(0x2B, a, 4); }
  m_cmd(0x2C, 0, 0);
  static int nb_i;
  for (int b = 0; b < NBAND; b++) { uint16_t *buf = STRIP[nb_i]; m_wait_mark(BUFM[nb_i]); band(b, buf);
    { uint32_t *w = (uint32_t *)(void *)buf; for (int i = 0; i < SW * BANDH / 2; i++) { uint32_t v = w[i]; w[i] = ((v >> 8) & 0x00FF00FFu) | ((v << 8) & 0xFF00FF00u); } }
    m_data(buf, SW * BANDH * 2); BUFM[nb_i] = m_mark(); nb_i = (nb_i + 1) % NBUF; }
  frame_n++; }
static void panel_begin(void) {
  m_begin();
#ifndef DESK
  pinMode(PIN_RST, OUTPUT); digitalWrite(PIN_RST, HIGH); delay(5); digitalWrite(PIN_RST, LOW); delay(20); digitalWrite(PIN_RST, HIGH); delay(150);
#endif
  m_cmd(0x01, 0, 0); hold_ms(150); m_cmd(0x11, 0, 0); hold_ms(120);
  { uint8_t v = 0x55; m_cmd(0x3A, &v, 1); }
  { uint8_t m = 0x60;
#ifdef ROT180
    m = 0xA0;
#endif
#ifdef BGR_ORDER
    m |= 0x08;
#endif
    m_cmd(0x36, &m, 1); }
#ifdef NO_INVERT
  m_cmd(0x20, 0, 0);
#else
  m_cmd(0x21, 0, 0);
#endif
  m_cmd(0x13, 0, 0); m_cmd(0x29, 0, 0); m_wait_all(); }
void setup(void) {
#ifndef DESK
#if BENCH
  Serial0.begin(115200L);
#else
  Serial.begin(115200L); delay(300); pinMode(PIN_A, INPUT_PULLUP);
#endif
#endif
  for (int i = 0; FONT_CH[i]; i++) GLY[(uint8_t)FONT_CH[i]] = (uint8_t)(i + 1);
  rules_begin(); panel_begin(); SAY("STARTER boot\n"); }
void loop(void) {
#if !BENCH
  static uint32_t next_ms;
#ifndef NO_PACE
  while ((int32_t)(millis() - next_ms) < 0) { } next_ms += 40; if ((int32_t)(millis() - next_ms) > 200) next_ms = millis();   // pacing only: no rule reads the clock
#endif
#endif
  frame();
  if (frame_n % 100 == 0) SAY("STARTER latch %u pulses %lld carry %d bounces %u books %s\n", (unsigned)ticks, (long long)speed.count, (int)speed.acc, (unsigned)bounces, books_ok ? "HELD" : "OPEN");
#if !defined(DESK) && BENCH
  if (FRAMES > 0 && frame_n >= (uint32_t)FRAMES) { m_wait_all(); Serial0.printf("DONE\n"); while (1) delay(1000); }
#endif
}
