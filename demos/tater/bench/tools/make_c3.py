#!/usr/bin/env python3
"""make_c3.py  one file for the Arduino IDE: tater_c3.ino = HIGH NOON's board mouth + tater.h (every type before any function)"""
import os
D = os.path.dirname(os.path.abspath(__file__)) + '/..'
def strip_guard(s, g):
    s = s.replace(f'#ifndef {g}\n#define {g}\n', '', 1); return s[:s.rstrip().rfind('#endif')].rstrip() + '\n'
M = '/*@@ CODE'
def halves(s):
    i = s.index(M); j = s.index('\n', i) + 1; return s[:i], s[j:]
src = strip_guard(open(f'{D}/tater.h').read(), 'TATER_H').replace('#include "ring_q.inc"', open(f'{D}/ring_q.inc').read())
PRE = r'''// TATER-CHAN ASCENDS, for the ESP32-C3 SuperMini + 2.0" ST7789 (320 x 240 across). About 70 seconds that loop, each loop worse:
// a potato with anime eyes -> speed lines -> a magical-girl transformation in a light tunnel -> a kaleidoscope -> a Julia set ->
// the sun over potato hills -> a ringed planet -> everything at once -> just a potato again.
// Whole numbers only: no floats, no libraries, no GPU. Every dot of every frame is worked out on the chip as it goes down the wire.
// Wiring (as HIGH NOON and THE FISH AFFECT): SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3. No controls needed.
// Arduino IDE: "ESP32C3 Dev Module", core 3.x, USB CDC On Boot: Enabled. Serial Monitor 115200: real frame times.
// ---- fixes (uncomment):
//#define BGR_ORDER   // only if red and blue come out swapped (HIGH NOON's old default; your panel showed it swapped)
//#define NO_INVERT
//#define ROT180
//#define SAFE_SPI
#pragma GCC optimize("O2")
#include <stdint.h>
#include <string.h>
#include <Arduino.h>
#include "esp_cpu.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#define FASTF IRAM_ATTR
#define PIN_SCK 4
#define PIN_MOSI 6
#define PIN_RST 10
#define PIN_DC 5
#define PIN_CS 7
#ifdef SAFE_SPI
#define WIRE_HZ 20000000
#else
#define WIRE_HZ 40000000
#endif
'''
MOUTH = r'''// ---------------------------------------------------------------- the mouth: FAREY's queue, as in HIGH NOON (it runs on this board)
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
static void wire_mark(int s) { q_mark[s] = q_in; }
static void wire_hold(int s) { while ((int32_t)(q_done - q_mark[s]) < 0) q_reap(); }
static void wire_wait_all(void) { while (q_done != q_in) q_reap(); }
static void say(const char *s) { Serial.print(s); }
static uint32_t clock_now(void) { return esp_cpu_get_cycle_count(); }
static void panel_begin(void) {
  pinMode(PIN_RST, OUTPUT); digitalWrite(PIN_RST, HIGH); delay(5); digitalWrite(PIN_RST, LOW); delay(20); digitalWrite(PIN_RST, HIGH); delay(150);
  gpio_set_direction((gpio_num_t)PIN_DC, GPIO_MODE_OUTPUT);
  spi_bus_config_t bus; memset(&bus, 0, sizeof bus);
  bus.mosi_io_num = PIN_MOSI; bus.miso_io_num = -1; bus.sclk_io_num = PIN_SCK; bus.quadwp_io_num = -1; bus.quadhd_io_num = -1; bus.max_transfer_sz = 320 * 16 * 2 + 64;
  m_e1 = (int)spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
  spi_device_interface_config_t d; memset(&d, 0, sizeof d);
  d.clock_speed_hz = WIRE_HZ; d.mode = 0; d.spics_io_num = PIN_CS; d.queue_size = QN; d.pre_cb = dc_cb;
  m_e2 = (int)spi_bus_add_device(SPI2_HOST, &d, &dev);
  wire_cmd(0x01, 0, 0); wire_wait_all(); delay(150); wire_cmd(0x11, 0, 0); wire_wait_all(); delay(120);
  { uint8_t v = 0x55; wire_cmd(0x3A, &v, 1); }
  { uint8_t m = 0x60;
#ifdef ROT180
    m = 0xA0;
#endif
#ifdef BGR_ORDER
    m |= 0x08;
#endif
    wire_cmd(0x36, &m, 1); }
#ifdef NO_INVERT
  wire_cmd(0x20, 0, 0);
#else
  wire_cmd(0x21, 0, 0);
#endif
  wire_cmd(0x13, 0, 0); wire_cmd(0x29, 0, 0); wire_wait_all(); }
'''
TAIL = r'''
// ---------------------------------------------------------------- run
void setup(void) {
  Serial.begin(115200); delay(300); Serial.print("TATER-CHAN: panel\n");
  panel_begin(); Serial.printf("TATER-CHAN: wire bus %d device %d (0 is good)\n", m_e1, m_e2);
  tater_begin(); Serial.print("TATER-CHAN ASCENDS\n"); }
static uint32_t rep_n, rep_sum, rep_max, last_us, fps_n, fps_t0;
void loop(void) {
  uint32_t now = micros(), us = last_us ? now - last_us : 0; last_us = now;
  if (++fps_n >= 30) { uint32_t dt = now - fps_t0; if (fps_t0 && dt) hud_fps = (uint32_t)(30000000ull / dt); fps_t0 = now; fps_n = 0; }   /* the corner's FPS: measured here, on the board */
  frame();
  if (us) { rep_sum += us; if (us > rep_max) rep_max = us; }
  if (++rep_n >= 200) { Serial.printf("TATER REAL frame avg %u us (%u fps) worst %u us | stage %u loop %u | story %u, prep %u, bands %u cycles\n", (unsigned)(rep_sum / rep_n), (unsigned)(rep_n * 1000000ull / (rep_sum ? rep_sum : 1)), (unsigned)rep_max,
      (unsigned)stage, (unsigned)loopn, (unsigned)cyc_story, (unsigned)cyc_prep, (unsigned)cyc_draw); rep_n = 0; rep_sum = 0; rep_max = 0; } }
'''
ft, fc = halves(src)
out = (PRE + '// ---------------------------------------------------------------- every type first (tater.h)\n' + ft + MOUTH
       + '// ---------------------------------------------------------------- TATER-CHAN (tater.h)\n' + fc + TAIL)
open(f'{D}/tater_c3.ino', 'w').write(out); print('tater_c3.ino', len(out.splitlines()), 'lines')
