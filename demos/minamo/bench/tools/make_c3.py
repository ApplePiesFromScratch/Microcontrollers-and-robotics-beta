#!/usr/bin/env python3
"""make_c3.py  one file for the Arduino IDE: minamo_c3.ino = the board mouth (FAREY's queue, as HIGH NOON) + minamo.h (every type before any function)"""
import os
D = os.path.dirname(os.path.abspath(__file__)) + '/..'
def strip_guard(s, g):
    s = s.replace(f'#ifndef {g}\n#define {g}\n', '', 1); return s[:s.rstrip().rfind('#endif')].rstrip() + '\n'
M = '/*@@ CODE'
def halves(s):
    i = s.index(M); j = s.index('\n', i) + 1; return s[:i], s[j:]
src = strip_guard(open(f'{D}/minamo.h').read(), 'MINAMO_H')
PRE = r'''// MINAMO (水面, the water's face), for the ESP32-C3 SuperMini + 2.0" ST7789 (320 x 240 across). A film of 100 seconds, then it loops.
// A pond at dusk: a maple leaf falls, five koi come in, night comes, rain, a storm. Then the pond runs backward, exactly: the rain
// leaves the water upward, the rings close into their drops, the koi swim back out, the leaf leaps into the air, and dawn finds the
// water flat. No video is stored. The surface is two readings of height in whole numbers, stepped by the leapfrog; the same line
// read the other way is the step back, so the way back is exact (the board checks every latch of it against the way out, and the
// last latch against flat water). The floor is seen through the surface (bent by its slope), the sky in it, the floor's light
// gathered by its curve, the koi where the bent ray lands. Whole numbers only: no floats, no libraries, no clock variable.
// Wiring (as HIGH NOON, KAGEROU, JUMON): SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3. Button C (21, to GND, optional): the film runs the other way.
// Arduino IDE: "ESP32C3 Dev Module", core 3.x, USB CDC On Boot: Enabled. Serial Monitor 115200: real latch times and the witnesses.
// A red square top right means a witness broke (the way back did not match the way out). It should never show.
// ---- fixes (uncomment):
//#define BGR_ORDER   // red and blue swapped (the default is RGB order: James's panel, 20 Nov 2026)
//#define NO_INVERT   // colours look like a negative
//#define ROT180      // upside down
//#define SAFE_SPI    // speckles or streaks: 40 MHz wire (default 80 MHz)
//#define WIRE_20     // still bad: 20 MHz
//#define NO_PACE     // run unpaced (default: a latch at most every 40 ms, the film's own tempo)
#pragma GCC optimize("O3")
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
#define PIN_C 21
#if defined(WIRE_20)
#define WIRE_HZ 20000000
#elif defined(SAFE_SPI)
#define WIRE_HZ 40000000
#else
#define WIRE_HZ 80000000
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
  Serial.begin(115200); delay(300); Serial.print("MINAMO: panel\n");
  panel_begin(); Serial.printf("MINAMO: wire bus %d device %d (0 is good)\n", m_e1, m_e2);
  pinMode(PIN_C, INPUT_PULLUP);
  uint32_t t0 = micros(); minamo_begin(); Serial.printf("MINAMO: floor painted in %u ms; film %d latches\n", (unsigned)((micros() - t0) / 1000), FILM); }
static uint32_t rep_n, rep_sum, rep_max, last_us;
void loop(void) {
#ifndef NO_PACE
  static uint32_t next_ms; while ((int32_t)(millis() - next_ms) < 0) { } next_ms += 40; if ((int32_t)(millis() - next_ms) > 200) next_ms = millis();   /* STIPULATED: 25 latches a second, the film's tempo; a slow latch borrows from the next, up to 200 ms */
#endif
  { static int was; int now = !digitalRead(PIN_C); if (now && !was && frame_n > 20) play = -play; was = now; }   /* button C: the film the other way */
  uint32_t now = micros(), us = last_us ? now - last_us : 0; last_us = now;
  frame();
  if (us) { rep_sum += us; if (us > rep_max) rep_max = us; }
  if (++rep_n >= 125) { Serial.printf("MINAMO latch %d step %d | avg %u us (%u fps) worst %u us | pond %u draw %u cycles | way back matched %u, broke %u | flat at the start %u, not %u\n",
      (int)film_p, (int)sim_s, (unsigned)(rep_sum / rep_n), (unsigned)(rep_n * 1000000ull / (rep_sum ? rep_sum : 1)), (unsigned)rep_max, (unsigned)cyc_sim, (unsigned)cyc_draw,
      (unsigned)mirror_ok, (unsigned)mirror_bad, (unsigned)homes, (unsigned)home_bad); rep_n = 0; rep_sum = 0; rep_max = 0; } }
'''
ft, fc = halves(src)
out = (PRE + '// ---------------------------------------------------------------- every type first (minamo.h)\n' + ft + MOUTH
       + '// ---------------------------------------------------------------- MINAMO (minamo.h)\n' + fc + TAIL)
open(f'{D}/minamo_c3.ino', 'w').write(out); print('minamo_c3.ino', len(out.splitlines()), 'lines')
