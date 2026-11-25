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
#include "irori.h"
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
