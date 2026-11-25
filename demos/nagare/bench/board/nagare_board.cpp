// NAGARE (流れ, flowing): the same warm box computed two ways, side by side. ESP32-C3 SuperMini + 2.0" ST7789 (320 x 240 across).
// LEFT, THE TEXTBOOK: continuity, Navier-Stokes and the energy equation, written on the screen as taught, discretised with a time step
//   DT = 1/4 and solved by the stable-fluids method (Stam 1999): lift as a body force, viscous diffusion, pressure projection (20 Jacobi
//   sweeps) for continuity, semi-Lagrangian advection, heat advected and conducted. Fixed point (a double-precision run on the desk
//   agrees with it to about a percent). Its books, measured live: how far its velocity is from divergence-free, and how much heat it
//   holds that nothing put in (or has lost that nothing took out).
// RIGHT, OURS: no equation imported. Each place holds stuff, a carried rate (as momentum) and warmth. Every change is a hand-on:
//   MOVE (each place's packet goes where its rate takes it, split over where it lands), PUSH (crowding on both sides of a face becomes
//   rate), DRAG (neighbours trade rate), EXCHANGE (crowding handed down its own difference), LIFT (a place warmer than the box as a
//   whole is handed upward rate, cooler ones downward, the ground's hand counted), FEED and VENT (counted). Stuff, warmth and both
//   components of momentum are kept by construction and shown EXACT, every step. No DT: a step is a trade.
// Hands: A a puff of warmth at the marker. B the heater on/off at the marker. C the view: warmth, speed, turning. Stick: move the
//   marker. Stick press: start again. Nothing touched for 30 s: the demo hand plays.
// Wiring: SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3. Stick VRx 0, VRy 1, press 2. Buttons A 3, B 20, C 21 (to ground).
// Arduino IDE: "ESP32C3 Dev Module", USB CDC On Boot: Enabled. Serial Monitor 115200: the time each side takes.
// Fixes (uncomment): colours red/blue swapped, colours inverted, upside down, stick left/right.
//#define RGB_ORDER
//#define NO_INVERT
//#define ROT180
//#define JOY_FLIP_X
#pragma GCC optimize("O2")
#include <stdint.h>
#include <string.h>
#include <Arduino.h>
#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
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
#include "nagare.h"
// ---------------------------------------------------------------- the panel's wire (HITOMI's queue) and the change-only glass (OS 4, IRORI)
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
  { uint8_t v = 0x55; wire_cmd(0x3A, &v, 1); }
  { uint8_t m = 0x60;                                                  /* landscape, 320 across */
#ifdef ROT180
    m = 0xA0;
#endif
#ifndef RGB_ORDER
    m |= 0x08;                                                         /* BGR, as TIDE found this panel */
#endif
    wire_cmd(0x36, &m, 1); }
#ifdef NO_INVERT
  wire_cmd(0x20, 0, 0);
#else
  wire_cmd(0x21, 0, 0);
#endif
  wire_cmd(0x13, 0, 0); wire_cmd(0x29, 0, 0); wire_wait_all(); }
// ---------------------------------------------------------------- the readings NAGARE asks for
static void hal_buttons(uint8_t down[3]) { down[0] = !digitalRead(PIN_A); down[1] = !digitalRead(PIN_B); down[2] = !digitalRead(PIN_C); }
static int hal_stick(int *x, int *y, int *press) {
  int rx = analogRead(PIN_VRX), ry = analogRead(PIN_VRY);
#ifdef JOY_FLIP_X
  rx = 4095 - rx;
#endif
  *x = rx; *y = ry; *press = !digitalRead(PIN_JSW); return 1; }
static uint32_t hal_cycles(void) { return (uint32_t)esp_cpu_get_cycle_count(); }
void setup(void) {
  Serial.begin(115200); delay(300);
  pinMode(PIN_A, INPUT_PULLUP); pinMode(PIN_B, INPUT_PULLUP); pinMode(PIN_C, INPUT_PULLUP); pinMode(PIN_JSW, INPUT_PULLUP); analogReadResolution(12);
  RB[0] = (uint16_t *)heap_caps_malloc(SW * TH * 2, MALLOC_CAP_DMA | MALLOC_CAP_8BIT); RB[1] = (uint16_t *)heap_caps_malloc(SW * TH * 2, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  nagare_begin();
  for (int k = 0; k < NCOLOUR; k++) { uint32_t r = PALRGB[k][0], g = PALRGB[k][1], b = PALRGB[k][2]; uint16_t v = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); PAL565[k] = (uint16_t)((v >> 8) | (v << 8)); }
  panel_begin(); Serial.printf("NAGARE: wire bus %d device %d (0 is good), strips %s, %u bytes free\n", m_e1, m_e2, RB[0] && RB[1] ? "ok" : "REFUSED", (unsigned)ESP.getFreeHeap()); }
static uint32_t rep_n, rep_sum, rep_max, sum_t, sum_o, sum_d;
void loop(void) {
  uint32_t t0 = micros(); nagare_frame(); show(); uint32_t us = micros() - t0; rep_sum += us; if (us > rep_max) rep_max = us; sum_t += cyc_t / 160; sum_o += cyc_o / 160; sum_d += cyc_d / 160;
  if (++rep_n >= 100) { Serial.printf("NAGARE step %u | frame avg %u us worst %u us | textbook %u us, ours %u us, drawing %u us | ours' books %s\n", (unsigned)ours_latch,
      (unsigned)(rep_sum / rep_n), (unsigned)rep_max, (unsigned)(sum_t / rep_n), (unsigned)(sum_o / rep_n), (unsigned)(sum_d / rep_n), ours_books_shut() ? "EXACT" : "OPEN");
    rep_n = 0; rep_sum = rep_max = sum_t = sum_o = sum_d = 0; } }
