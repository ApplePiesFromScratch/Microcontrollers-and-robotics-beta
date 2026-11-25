// LESSON-BEGIN 1 of 18: LESSON MAP: ACE 4, a rate law where everyone else uses a position law, and a whole-number 3D rasteriser
// SKILL      : a missile that reads how fast the line of sight TURNS and turns by a multiple of that rate; parts held as a
//              ledger (load past a threshold reconfigures: tear, keep 2/5); flight read from the parts; a 3D rasteriser in
//              whole numbers woven a band at a time; detail landed coarser when the last frame read heavy.
// BENCH      : gcc -O2 -w -DSKETCH='"../demos/ace4/ace4.ino"' -o /tmp/ace ../../tools/desk_panel.c && /tmp/ace 600
//              compare: add -DPURE_PURSUIT (ACE 3's law: chase the bearing) and read the MISSILES line of an emulator run.
// READ FIRST : demos/ace4/README.md; docs/USEFUL_PATTERN.md (the part ledger is clause 4: past the boundary, reconfigure).
// SECTIONS   : 2 the ring | 3 the mouths | 4 the engine (tile prints) | 5 the panel | 6 the face | 7 GLIDE rasteriser
//              8 the air | 9 the eye | 10 airframes from a seed | 11 THE JETS: parts as a ledger | 12 flight | 13 ground war
//              14 pilots | 15 THE MISSILE AND THE SEEKER | 16 growing the picture | 17 hands and the test card | 18 the frame
// LESSON-END
// ACE 4: a dogfight over a dusk canyon. ESP32-C3 SuperMini + 2.0" ST7789 240x320 (GMT020-02-8P), landscape 320 x 240.
// Every airframe is grown from one seed and regrown every frame from its parts; the parts are a ledger (a hit adds load;
// past its threshold a part keeps 2/5 and tears, the rest falls burning and becomes ground) and flight reads the parts.
// ACE 4's step: a missile does not chase where the target IS; it reads how fast the line of sight TURNS and turns its own
// heading by a multiple of that rate (the bearing's rate, not the bearing). A line that does not turn is a collision.
// Its seeker is a reading of heat: each frame it takes the hottest thing in its cone, so a flare (hot, cooling at a rate)
// can steal it. Bandits flare when a missile reads them; so can you (hold C). The eye banks with your jet; the sky and sun
// follow the true horizon; the far land is laid in 2 x 2 blocks; a frame that read heavy lays the next a grain coarser.
// Whole numbers only: no float, no sin, no sqrt, no math library.
// Controls (buttons to GND, INPUT_PULLUP): stick VRx GPIO0 = roll, VRy GPIO1 = pitch (power the stick from 3V3);
// A GPIO3 = guns (hold); B GPIO20 = missile; C GPIO21 tap = camera: chase / orbit / padlock, C held = flares;
// stick press GPIO2 = afterburner. Idle 20 s and it flies itself.
// Hold C while powering up: the top line shows the stick's raw reading. Hold B: a still test card until B is pressed again. GPIO20/21 are UART0's pins: the board build never opens UART0 (USB CDC is the serial port).
// Arduino IDE: board "ESP32C3 Dev Module", core 3.x, USB CDC On Boot: Enabled.
// ---- fixes (uncomment):
//#define ROT180       // upside down
//#define RGB_ORDER    // red and blue swapped
//#define NO_INVERT    // colours look like a negative
//#define SAFE_SPI     // speckles or streaks: 20 MHz wire (default 40 MHz)
//#define WIRE_80      // try 80 MHz
//#define NO_OVERLAP   // wait for every queued byte before drawing the next band (slower)
//#define NO_HEAL      // turn off the rolling repaint
//#define JOY_FLIP_X   // stick left/right backwards
//#define JOY_FLIP_Y   // stick up/down backwards
//#define JOY_SWAP     // stick axes swapped
//#define DEMO         // always flies itself
//#define NO_ROLL      // keep the horizon level
//#define PURE_PURSUIT // missiles chase the bearing (ACE 3's law), for comparing
// Bench builds only (set by the tools): DESK, MOUTH_FIFO, NULL_WIRE, TRUTH (every tile sent, the reference glass), FRAMES=n. Bench builds always fly the script.

#ifndef DESK
#include <Arduino.h>
#include "esp_cpu.h"
#ifdef MOUTH_FIFO
#include <SPI.h>
#else
#include "driver/spi_master.h"
#include "driver/gpio.h"
#endif
#endif
#include <stdint.h>
#include <string.h>
#pragma GCC optimize("O2")
#define PIN_SCK 4
#define PIN_MOSI 6
#define PIN_RST 10
#define PIN_DC 5
#define PIN_CS 7
#if defined(WIRE_80)
#define WIRE_HZ 80000000
#elif defined(WIRE_20) || defined(SAFE_SPI)
#define WIRE_HZ 20000000
#else
#define WIRE_HZ 40000000
#endif

#ifdef PANEL_240
#error "LOOM STATION is laid out for the 2.0\" 240x320 panel only"
#define SW 240            // screen columns (the square glass)
#else
#define SW 320            // screen columns
#endif
#define SH 240            // screen rows
#define GM 320            // the panel's memory is always 320 lines long along x: the scroll wraps over all of it
#define TS 16             // tile side
#define TX (SW / TS)      // tiles across
#define TY 15             // tiles down (= bands)
#define BANDPX (TS * SW)
#define ALLT ((1u << TX) - 1)


// the shapes of things (declared first, so the IDE's own prototypes can name them)
typedef struct { int jx, jy, a, b, c, sw; } Pad;
typedef struct { int32_t x, y, z, u, v, l; } CV;                       // a corner in the eye's space, with texture and light
typedef struct { int32_t x[3], y[3], s02, s01, s12; int16_t ry0, ry1; int32_t ax, ay, W, Wx, Wy, U, Ux, Uy, V, Vx, Vy, L, Lx, Ly; uint16_t fc; uint8_t mat, fog; } Tri;
#ifndef MSL_DMG
#define MSL_DMG 80                // ACE 4: 130 on every part, just under the tears; now 80: a hit maims, the guns finish
#endif
#ifndef MSL_LIFE
#define MSL_LIFE 120              // frames of motor (ACE 4: 170): a missile let go too far out falls short
#endif
#ifndef MSL_FIN
#define MSL_FIN 45                // ACE 4: 240. On the desk range a jinking bandit now slips 1 missile in 6 (none slipped at 240)
#endif
#define NPART 7
typedef struct { int16_t span, chord, tipc, sweep, flen, frad, fin, tspan, twin, dihed, skin, skin2; } Shape;
typedef struct {
  int32_t x, y, z, v, gam, yaw, bank, dgam, dyaw, lift, load;            // 1/16 units; angles in 1/16 ring steps
  int32_t S[NPART], L[NPART];
  int32_t alive, dead_t, seed, cool, heat, msl, lockt, tgt, hitf, skill, brk, side, mcool, flr, fcool, ext, raid;
  int jx, jy, fire, ab, mfire, rud, trim;
  Shape sh;
} Jet;
typedef struct { int32_t x, y, z, vx, vy, vz, mass; int16_t kind, life, age, owner, tgt, spin, mat, size, lx, ly, lz; } Pt;   // lx..lz: a missile's last line of sight (grain 1024)

// LESSON-BEGIN 2 of 18: The ring: one heading (1 : 1304), composed
// WHAT    : grow_ring composes the heading (1:1304) with itself round the circle; every turn reads this table. root is the
//           digit-pair root: two bits of the radicand a step, no division.
// RELATION: (1 : 1304) turns by an angle whose half has slope 1/1304; its unit is exact before any landing.
// LESSON-END
// ---------------------------------------------------------------- the ring (heading 1:1304)
#define RN 4096
static int16_t RC[RN], RS[RN];   // grain 16384
static void grow_ring(void) {
  const int64_t c = 1304LL * 1304 - 1, s = 2LL * 1304, d = 1304LL * 1304 + 1;   // c^2 + s^2 = d^2 exactly
  int64_t x = (int64_t)1 << 40, y = 0;
  for (int k = 0; k < RN; k++) {
    RC[k] = (int16_t)((x + ((int64_t)1 << 25)) >> 26); RS[k] = (int16_t)((y + ((int64_t)1 << 25)) >> 26);
    int64_t nx = c * x - s * y, ny = s * x + c * y;
    x = nx >= 0 ? (nx + d / 2) / d : -((-nx + d / 2) / d);
    y = ny >= 0 ? (ny + d / 2) / d : -((-ny + d / 2) / d);
  }
}
#define SIN(a) RS[(a) & (RN - 1)]
#define COS(a) RC[(a) & (RN - 1)]
// the digit-pair root: two bits of the radicand per step, no division
static uint32_t root(uint32_t v) {
  uint32_t r = 0, b = 1u << 30;
  while (b > v) b >>= 2;
  while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
  return r;
}
// the angle of (dx, dy) in the first quarter, in ring steps 0..1024, by halving on the ring and cross-multiplying
static int quarter_angle(int32_t dx, int32_t dy) {
  int lo = 0, hi = 1024;
  while (hi - lo > 1) { int m = (lo + hi) >> 1; if ((int64_t)RS[m] * dx <= (int64_t)RC[m] * dy) lo = m; else hi = m; }
  return lo;
}
static inline uint32_t mixh(uint32_t x, uint32_t y, uint32_t k) {
  uint32_t h = x * 0x8da6b343u ^ y * 0xd8163841u ^ k * 0xcb1ab31fu;
  h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16; return h;
}
static uint16_t EASE[257];   // t^2 (3 - 2t) on 256, the cubic ease (value-noise practice, named as such)
static void grow_ease(void) { for (int t = 0; t <= 256; t++) EASE[t] = (uint16_t)((t * t * (768 - 2 * t) + 32768) >> 16); }
static int32_t vnoise(uint32_t wx, uint32_t wy, int g, uint32_t k) {   // 0..65280
  uint32_t ix = wx >> g, iy = wy >> g, fx = wx & ((1u << g) - 1), fy = wy & ((1u << g) - 1);
  if (g >= 8) { fx >>= (g - 8); fy >>= (g - 8); } else { fx <<= (8 - g); fy <<= (8 - g); }
  int32_t ex = EASE[fx], ey = EASE[fy];
  int32_t v00 = mixh(ix, iy, k) & 255, v10 = mixh(ix + 1, iy, k) & 255, v01 = mixh(ix, iy + 1, k) & 255, v11 = mixh(ix + 1, iy + 1, k) & 255;
  int32_t a = v00 * 256 + (v10 - v00) * ex, b = v01 * 256 + (v11 - v01) * ex;
  return a + (((b - a) * ey) >> 8);
}
// the mouth: 8-bit channels to RGB565, already in wire order (high byte first in memory), rounded once
static inline uint16_t glass(int32_t r, int32_t g, int32_t b) {
  if (r < 0) r = 0; if (r > 255) r = 255; if (g < 0) g = 0; if (g > 255) g = 255; if (b < 0) b = 0; if (b > 255) b = 255;
  uint16_t v = (uint16_t)(((r * 31 + 127) / 255) << 11 | ((g * 63 + 127) / 255) << 5 | ((b * 31 + 127) / 255));
  return (uint16_t)((v >> 8) | (v << 8));
}

// LESSON-BEGIN 3 of 18: The mouths: every command, parameter and pixel run is a queued SPI transaction
// WHAT    : the CPU never waits for the wire. DESK, FIFO (emulator) and DMA (board) take the same calls.
// LESSON-END
// ---------------------------------------------------------------- the mouths (DESK, FIFO for the emulator, DMA queue)
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
  pinMode(PIN_DC, OUTPUT); pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  SPI.begin(PIN_SCK, -1, PIN_MOSI, -1); SPI.beginTransaction(SPISettings(WIRE_HZ, MSBFIRST, SPI_MODE0)); digitalWrite(PIN_CS, LOW);
}
static void hold_ms(int ms) { delay(ms); }
static uint32_t cyc(void) { return esp_cpu_get_cycle_count(); }
#else
// the queue: every command, parameter and pixel run is a queued SPI transaction; the CPU never waits for the wire
// except to reuse a strip whose bytes are still going out. DC rides on each transaction (pre-callback).
#define QN 64
static spi_device_handle_t dev;
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
  bus.max_transfer_sz = BANDPX * 2 + 64;
  spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
  spi_device_interface_config_t d; memset(&d, 0, sizeof d);
  d.clock_speed_hz = WIRE_HZ; d.mode = 0; d.spics_io_num = PIN_CS; d.queue_size = QN; d.pre_cb = dc_cb;
  spi_bus_add_device(SPI2_HOST, &d, &dev);
}
static void hold_ms(int ms) { m_wait_all(); delay(ms); }
static uint32_t cyc(void) { return esp_cpu_get_cycle_count(); }
#endif

// LESSON-BEGIN 4 of 18: The engine: send only what changed
// WHAT    : tile_print hashes each tile; a tile whose print equals what the panel holds is not sent. forget_glass marks every
//           tile unknown after a scene change.
// RELATION: what the panel holds is a reading the sketch keeps of the glass. It is a second description of one thing, so
//           every scene change must forget it, or the two descriptions disagree (WAKE's pattern).
// LESSON-END
// ---------------------------------------------------------------- the engine
static uint16_t RB[2][BANDPX] __attribute__((aligned(4)));   // the strip being woven (stride 320)
static uint16_t PB[2][BANDPX] __attribute__((aligned(4)));   // the same strip's changed runs, packed for the wire
static uint32_t PRINT[TY][TX];                              // what each tile of the glass shows (its print)
static uint32_t HELD[TY];                                    // which prints are known (bit per tile)
static uint32_t SUS[TY];                                     // tiles this frame may have touched
static uint32_t L_est, L_del;                                // EST a picture placed on a tile, DEL one replaced
static uint32_t f_bytes, f_trans, f_sent, f_kept, f_drawn;   // this frame
static int32_t wx0 = -1, wx1 = -1, wy0 = -1, wy1 = -1;       // the window the panel holds (KEEP: not re-sent)
static uint32_t slot_n;

static void cmd(uint8_t c, const uint8_t *p, int n) { f_bytes += 1 + n; f_trans += n ? 2 : 1; m_cmd(c, p, n); }
static void pix(const void *p, int nb) { f_bytes += nb; f_trans++; m_data(p, nb); }
static void window(int x0, int x1, int y0, int y1) {        // inclusive; only what changed is said
  if (x0 != wx0 || x1 != wx1) { uint8_t a[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1}; cmd(0x2A, a, 4); wx0 = x0; wx1 = x1; }
  if (y0 != wy0 || y1 != wy1) { uint8_t a[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1}; cmd(0x2B, a, 4); wy0 = y0; wy1 = y1; }
  cmd(0x2C, 0, 0);
}
static int32_t XO;                                          // where the glass's first column sits in panel memory
static void forget_glass(void) {                            // the glass moved under us: every print is let go
  for (int b = 0; b < TY; b++) for (int t = 0; t < TX; t++) if (HELD[b] >> t & 1) L_del++;
  memset(HELD, 0, sizeof HELD);
}
static uint32_t tile_print(const uint16_t *p) {             // 128 words, one multiply each
  uint32_t h = 0x811c9dc5u;
  for (int r = 0; r < TS; r++) { const uint32_t *q = (const uint32_t *)(p + r * SW);
    for (int w = 0; w < 8; w++) { h = (h ^ q[w]) * 0x01000193u; h ^= h >> 15; } }   // the fold after every word (CARRYWING: a fold only at the end let 4 in 2,400 frames through)   // a multiply only carries upward: without this fold, the top bits of two words (odd pixels) could cancel
  return h;
}


// LESSON-BEGIN 5 of 18: The panel: clean slate on a scene change
// WHAT    : drain the queue, say the panel's state again, paint all 320 x 240 black, forget every print.
// LESSON-END
// ---------------------------------------------------------------- the panel
static void panel_state(void);
static void panel_begin(void) {
#ifndef DESK
  pinMode(PIN_RST, OUTPUT); digitalWrite(PIN_RST, HIGH); delay(5); digitalWrite(PIN_RST, LOW); delay(20); digitalWrite(PIN_RST, HIGH); delay(150);
#endif
  m_begin();
  cmd(0x01, 0, 0); hold_ms(150);                             // SWRESET
  cmd(0x11, 0, 0); hold_ms(120);                             // SLPOUT
  panel_state();
}
static void panel_state(void) {                             // everything the glass must hold; said again at every scene change
  { uint8_t v = 0x55; cmd(0x3A, &v, 1); }                    // COLMOD 16 bits
  { uint8_t m = 0x60;                                        // MX | MV: landscape, 320 across
#ifdef ROT180
    m = 0xA0;                                                // MY | MV: the same, half a turn
#endif
#if defined(PANEL_240) && defined(ROT180)
    XO = GM - SW;                                            // the square glass shows the far 240 lines when turned
#else
    XO = 0;
#endif
#ifndef RGB_ORDER
    m |= 0x08;                                               // BGR (this 2.0" IPS panel)
#endif
    cmd(0x36, &m, 1); }
#ifdef NO_INVERT
  cmd(0x20, 0, 0);
#else
  cmd(0x21, 0, 0);                                           // INVON: true colour on this IPS panel
#endif
  cmd(0x13, 0, 0);                                           // NORON (also leaves scroll mode)
  { uint8_t a[2] = {0, 0}; cmd(0x37, a, 2); }                // scroll offset 0
  cmd(0x29, 0, 0);                                           // DISPON
  wx0 = wx1 = wy0 = wy1 = -1;                                // the window must be said again
}
// a scene change: drain the queue, say the panel's state again, paint all 320 x 240 memory black, forget every print
static void clean_slate(void) {
  m_wait_all();
  panel_state();
  memset(RB[0], 0, sizeof RB[0]);
  for (int b = 0; b < TY; b++) for (int h = 0; h < GM; h += GM / 2) { window(h, h + GM / 2 - 1, b * TS, b * TS + TS - 1); pix(RB[0], GM / 2 * TS * 2); }
  m_wait_all();
  forget_glass();
}



static int full_angle(int32_t dx, int32_t dy) {   // ring steps 0..4095 of the direction (dx, dy), y down
  if (!dx && !dy) return 0;
  int q = quarter_angle(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
  if (dx >= 0 && dy >= 0) return q; if (dx < 0 && dy >= 0) return 2048 - q; if (dx < 0) return 2048 + q; return (4096 - q) & 4095;
}
static uint32_t frame_n;
// LESSON-BEGIN 6 of 18: The face: 5 x 7 letters at any whole scale
// LESSON-END
// ================================================================ the face: 5x7 letters drawn for this sketch
static const char FACE_CH[] = "0123456789ABCDEFGHIKLMNOPRSTUVWXY:-!";
static const uint8_t FACE[36][7] = {
  {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31}, {31,2,4,2,1,17,14}, {2,6,10,18,31,2,2},
  {31,16,30,1,1,17,14}, {6,8,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14}, {14,17,17,15,1,2,12},
  {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31},
  {31,16,16,30,16,16,16}, {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14}, {17,18,20,24,20,18,17},
  {16,16,16,16,16,16,31}, {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
  {30,17,17,30,20,18,17}, {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14}, {17,17,17,17,10,10,4},
  {17,17,17,21,21,27,17}, {17,17,10,4,10,17,17}, {17,17,10,4,4,4,4}, {0,12,12,0,12,12,0}, {0,0,0,31,0,0,0}, {4,4,4,4,4,0,4}};
static uint8_t GLYPH[128];   // char -> face row index + 1 (0 = blank)
static void grow_face(void) { for (int i = 0; FACE_CH[i]; i++) GLYPH[(uint8_t)FACE_CH[i]] = (uint8_t)(i + 1); }
static inline int face_bit(char ch, int gx, int gy) { int g = GLYPH[ch & 127]; return g ? (FACE[g - 1][gy] >> (4 - gx)) & 1 : 0; }

// ---- text: the face at any whole scale, into a row
static void text_row(int y, int x0, int x1, uint16_t *d, const char *s, int tx, int ty, int sc, uint16_t col) {
  int gy = y - ty; if (gy < 0 || gy >= 7 * sc) return; gy /= sc;
  int n = (int)strlen(s), a = tx > x0 ? tx : x0, e = tx + n * 6 * sc; if (e > x1) e = x1;
  for (int x = a; x < e; x++) { int xx = x - tx, ci = xx / (6 * sc), gx = (xx % (6 * sc)) / sc; if (gx < 5 && face_bit(s[ci], gx, gy)) d[x - x0] = col; }
}
static void put_num(char *p, uint32_t v, int n) { for (int k = n - 1; k >= 0; k--) { p[k] = (char)('0' + v % 10); v /= 10; } }
// LESSON-BEGIN 7 of 18: GLIDE: a whole-number 3D rasteriser, woven a band at a time
// WHAT    : each triangle is set up once a frame: screen edges, and planes for 1/z, u/z, v/z and light. Each 16-row band clears
//           its own 16 x 320 depth strip and fills only the triangles binned to it.
// HOLDING : 1/z, u/z, v/z are planes across the screen, so they step by adds along a row. Texture is perspective-true every
//           16th dot (two divisions) and straight between. Light per corner, smooth across. Fog by distance.
// WHY     : a band-sized depth strip is the budget rule: 16 x 320 depths fit in fast RAM; a full-screen depth buffer would not
//           leave room for the pictures.
// TRAP    : mulhi64 exists because a 32 x 32 product needs 64 bits; read where it is used and why a plain int32 product
//           would wrap.
// LESSON-END
// ================================================================ GLIDE: a whole-number 3D rasteriser, woven a band at a time
// Every triangle is set up once a frame (screen edges, and planes for 1/z, u/z, v/z and light). Each 16-row band then
// clears its own 16 x 320 depth strip and fills only the triangles binned to it. Texture is perspective-true at every
// 16th dot (two divisions), straight in between. Light is smooth across each triangle (per corner). Fog by distance.
enum { M_GRASS, M_ROCK, M_SNOW, M_SAND, M_WATER, M_SCORCH, M_EMBER, M_FOREST, M_WHEAT, M_PLOW, M_ROOF, M_WALL, M_SKIN0, M_SKIN1, M_SKIN2, M_SKIN3, M_DARK, M_GLASS, M_RED, M_GLOW, M_TRACER, M_FIRE, M_SMOKE, M_CLOUD, NMAT };
enum { X_GRASS, X_ROCK, X_SNOW, X_SAND, X_WATER, X_SCORCH, X_SKIN, X_FLAT, X_PUFF, X_STREAK, X_CANOPY, X_ROWS, X_TILES, X_PLASTER, NTEX };
static const uint8_t TEXOF[NMAT] = {X_GRASS, X_ROCK, X_SNOW, X_SAND, X_WATER, X_SCORCH, X_SCORCH, X_CANOPY, X_ROWS, X_ROWS, X_TILES, X_PLASTER, X_SKIN, X_SKIN, X_SKIN, X_SKIN, X_SKIN, X_STREAK, X_SKIN, X_FLAT, X_STREAK, X_PUFF, X_PUFF, X_PUFF};
#define CUTM(m) ((m) >= M_FIRE)            // texel 0 of a puff is no dot at all: round fire, smoke and cloud from two triangles
#define LITM(m) ((m) == M_EMBER || (m) == M_GLOW || (m) == M_TRACER || (m) == M_FIRE)   // their own light
#define TN 128                    // the terrain wraps every 128 cells; a cell is 256 units
static uint8_t TH[TN * TN], TL[TN * TN];   // height; light (low 4 bits) and material (high 4 bits)
static uint8_t TEX[NTEX][32 * 32];
static uint16_t LUT[NMAT][8][16][8];   // material, fog, light, texel
#define MAXTRI 560
#define NBIN 2000                 // ACE 5: band entries (was 3800; desk peak 866): 3.6 KB back for the link margin
static Tri TR[MAXTRI]; static int ntri;
static uint16_t TBIN[NBIN], BINAT[TY + 1], BCNT[TY];   // BCNT was 8 bits in ACE 3 (AERIE K10: a band of 256+ triangles wrapped to none)
static uint16_t ZB[TS * 320];
#define NEAR 24
#define FOC 160
#define HORIZ 104                   // the horizon row when level
#define RV 14                    // cells seen each way
#define R0 8                     // cells each way laid one by one; beyond, in 2 x 2 blocks
static int16_t VCX[(2 * RV + 1) * (2 * RV + 1)], VCY[(2 * RV + 1) * (2 * RV + 1)], VCZ[(2 * RV + 1) * (2 * RV + 1)];   // ACE 5: 16 bits (desk max |6,042|; the ring's bound is about 6,500): 5 KB back for the link margin
#define HMAX 256
static uint16_t SKYH[HMAX], SUNC, HUDC, HUDW, HUDR, HUDG;
static int sun_x, sun_y, sun_on, hz;                          // the sun on the glass; the true horizon's row before the roll
static int32_t cr = 16384, sr = 0, camroll;                  // the eye's roll (grain 16384)
static uint32_t tris_set;

static int path_x(int j) { return 64 + (SIN(j * 32) * 22 >> 14) + (SIN(j * 96 + 500) * 6 >> 14); }   // the canyon's line, in cells
static int32_t tnoise(int i, int j, int g, uint32_t k) {   // value noise that wraps at 128 cells
  int n = TN >> g, ix = i >> g, iy = j >> g, sh = 8 - g;
  int32_t ex = EASE[(i & ((1 << g) - 1)) << sh], ey = EASE[(j & ((1 << g) - 1)) << sh];
  int32_t v00 = mixh((uint32_t)(ix % n), (uint32_t)(iy % n), k) & 255, v10 = mixh((uint32_t)((ix + 1) % n), (uint32_t)(iy % n), k) & 255;
  int32_t v01 = mixh((uint32_t)(ix % n), (uint32_t)((iy + 1) % n), k) & 255, v11 = mixh((uint32_t)((ix + 1) % n), (uint32_t)((iy + 1) % n), k) & 255;
  int32_t a = v00 * 256 + (v10 - v00) * ex, b = v01 * 256 + (v11 - v01) * ex;
  return a + (((b - a) * ey) >> 8);
}
#define WL 34                     // the river's level (height units of 8)
// ACE 5: stone bridges over the river. A bridge is a box from the water to its deck (arched plaster sides), so the height
// field the jets strike is exactly what is drawn. Burnt, it slumps to rubble a few units over the water (drawn and struck).
#define NBR 4
#define BR_W 56                   // half width (z); the half length is BR_L
#define BR_L 896
typedef struct { int32_t x, z, top; } Bridge;
static Bridge BRG[NBR]; static int nbr; static uint8_t br_up[NBR] = {1, 1, 1, 1};
static int32_t br_top(int k) { return br_up[k] ? BRG[k].top : WL * 8 + 10; }
static int32_t bridge_at(int32_t wx, int32_t wz) {        // the deck under (wx, wz), or -1
  for (int k = 0; k < nbr; k++) { int32_t dx = ((wx - BRG[k].x + 16384) & 32767) - 16384, dz = ((wz - BRG[k].z + 16384) & 32767) - 16384;
    if (dx > -BR_L && dx < BR_L && dz > -BR_W && dz < BR_W) return br_top(k); }
  return -1;
}
// ---- the world: mostly farmland, forest, lakes, a river and a coast, with villages; mountains are rare
// Heights are in units of 8. Every rule below reads only the noise and the cells already laid, so it is grown the same each boot.
#define NVIL 12
#define OB_TREE 1
#define OB_HOUSE 2
typedef struct { int16_t ci, cj; char name[12]; } Village;
static Village VIL[NVIL]; static int nvil;
// names are mixed from syllables of the HUD's own alphabet; the boot check counts any letter that escapes it
static const char *const SYL_A[16] = {"KEL", "MAR", "BRO", "ASH", "DUN", "FEN", "HOL", "OAK", "RIV", "STO", "WIL", "CAR", "TOR", "ELM", "PEN", "LOW"};
static const char *const SYL_B[16] = {"WICK", "BY", "TON", "FORD", "HAM", "STEAD", "MERE", "DALE", "BURY", "LEY", "COMBE", "WELL", "HOLT", "MOOR", "BROOK", "GATE"};
static uint32_t name_escapes;
// what stands on a cell is not stored (16 KB the chip lacks): it is a rule, read the same by the renderer and by solid()
static int house_at(int i, int j) {                      // 0, or OB_HOUSE with bit 4 for a turned (flat-roofed) barn
  for (int v = 0; v < nvil; v++) { int di = ((i - VIL[v].ci + 64) & 127) - 64, dj = ((j - VIL[v].cj + 64) & 127) - 64;
    if (di < -3 || di > 3 || dj < -3 || dj > 3) continue;
    if ((di == 0 && (dj == 0 || dj == 3)) || ((di == 2 || di == -2) && dj < 0 && dj >= -2) || di * di + dj * dj > 10) return 0;   // the square, and the guns' and tanks' ground
    uint32_t q = mixh((uint32_t)(VIL[v].ci + di), (uint32_t)(VIL[v].cj + dj), 62); return (q & 3) ? 0 : OB_HOUSE | (int)((q >> 4) & 1) << 4; }
  return 0;
}
static int obj_at(int i, int j) {
  i &= 127; j &= 127;
  if ((TL[j * TN + i] >> 4) == M_FOREST) return (mixh((uint32_t)i, (uint32_t)j, 18) & 3) ? OB_TREE : 0;
  return house_at(i, j);
}
static void grow_terrain(void) {
  for (int j = 0; j < TN; j++) for (int i = 0; i < TN; i++) {
    int d = ((i - path_x(j) + 64) & 127) - 64, ad = d < 0 ? -d : d;
    int c = tnoise(i, j, 5, 11) >> 8, r = tnoise(i, j, 3, 12) >> 8, m = tnoise(i, j, 4, 13) >> 8, lk = tnoise(i, j, 3, 14) >> 8;
    int h = WL + 3 + (c - 100) / 5 + (r >> 5);                // rolling land; below WL is the sea
    if (m > 196 && c > 120) { int e = m - 196; h += e * e / 7; } // mountains: only where two rare readings agree
    if (lk > 206 && m < 170) h = h < WL + 2 ? h : WL - 1;         // lakes in the hollows
    if (ad <= 1) h = WL - 1; else if (ad <= 3 && h > WL + ad) h = WL + ad;   // the river and its banks
    if (h > 250) h = 250 - ((h - 250) >> 3); if (h < WL) h = WL;
    TH[j * TN + i] = (uint8_t)h; }
  // villages: flat dry land near water, apart from each other; the ground under them is levelled
  nvil = 0; name_escapes = 0;
  for (uint32_t t = 0; t < 4000 && nvil < NVIL; t++) { uint32_t hh = mixh(t, 3, 61); int ci = (int)(hh & 127), cj = (int)((hh >> 8) & 127), h0 = TH[cj * TN + ci];
    if (h0 < WL + 2 || h0 > WL + 24) continue;
    int ok = 1, wet = 0; for (int dj = -3; dj <= 3 && ok; dj++) for (int di = -3; di <= 3; di++) { int h1 = TH[((cj + dj) & 127) * TN + ((ci + di) & 127)]; if (h1 <= WL) ok = 0; if (h1 - h0 > 6 || h0 - h1 > 6) ok = 0; }
    for (int dj = -8; dj <= 8 && ok; dj += 2) for (int di = -8; di <= 8; di += 2) wet |= TH[((cj + dj) & 127) * TN + ((ci + di) & 127)] <= WL;
    for (int v = 0; v < nvil && ok; v++) { int dx = ((VIL[v].ci - ci + 64) & 127) - 64, dz = ((VIL[v].cj - cj + 64) & 127) - 64; if (dx * dx + dz * dz < 18 * 18) ok = 0; }
    if (!ok || !wet) continue;
    Village *V = &VIL[nvil++]; V->ci = (int16_t)ci; V->cj = (int16_t)cj;
    { const char *a = SYL_A[hh >> 16 & 15], *b = SYL_B[hh >> 20 & 15]; int n = 0; while (*a && n < 11) V->name[n++] = *a++; while (*b && n < 11) V->name[n++] = *b++; V->name[n] = 0;
      for (int k = 0; k < n; k++) if (!GLYPH[(uint8_t)V->name[k] & 127]) name_escapes++; }
    for (int dj = -3; dj <= 4; dj++) for (int di = -3; di <= 4; di++) TH[((cj + dj) & 127) * TN + ((ci + di) & 127)] = (uint8_t)h0;
  }
  for (int j = 0; j < TN; j++) for (int i = 0; i < TN; i++) {
    int h = TH[j * TN + i], hx = (TH[j * TN + ((i - 1) & 127)] - TH[j * TN + ((i + 1) & 127)]) * 8, hz = (TH[((j - 1) & 127) * TN + i] - TH[((j + 1) & 127) * TN + i]) * 8;
    int slope = (hx < 0 ? -hx : hx) + (hz < 0 ? -hz : hz);
    int hm = h; { int h1 = TH[j * TN + ((i + 1) & 127)], h2 = TH[((j + 1) & 127) * TN + i], h3 = TH[((j + 1) & 127) * TN + ((i + 1) & 127)]; if (h1 > hm) hm = h1; if (h2 > hm) hm = h2; if (h3 > hm) hm = h3; }
    int lo = h; { int h1 = TH[j * TN + ((i + 1) & 127)], h2 = TH[((j + 1) & 127) * TN + i], h3 = TH[((j + 1) & 127) * TN + ((i + 1) & 127)]; if (h1 < lo) lo = h1; if (h2 < lo) lo = h2; if (h3 < lo) lo = h3; }
    int fo = tnoise(i, j, 3, 15) >> 8, fa = tnoise(i, j, 4, 16) >> 8; uint32_t pq = mixh((uint32_t)(i / 3), (uint32_t)(j / 3), 17);
    int mt = hm <= WL ? M_WATER : lo <= WL || h < WL + 3 ? M_SAND : h > 205 ? M_SNOW : slope > 200 || h > 150 ? M_ROCK
           : house_at(i, j) ? M_GRASS : fo > 150 ? M_FOREST : (fa > 110 && slope < 60) ? ((pq & 3) == 0 ? M_GRASS : (pq & 3) == 3 ? M_PLOW : M_WHEAT) : M_GRASS;
    int32_t dot = -48 * hx + 64 * 512 + 40 * hz, nl = (int32_t)root((uint32_t)(hx * hx + 512 * 512 + hz * hz));
    int li = dot > 0 ? (int)(dot * 13 / (nl * 89)) : 0; li += 2; if (li > 15) li = 15;
    TL[j * TN + i] = (uint8_t)((hm <= WL ? 11 : li) | mt << 4); }
  nbr = 0;
  for (int j = 6; j < TN && nbr < NBR; j += 2) { int px = path_x(j), hl = TH[j * TN + ((px - 4) & 127)], hr = TH[j * TN + ((px + 4) & 127)], hm = hl > hr ? hl : hr;
    if (hm > WL + 18 || (nbr && j - (BRG[nbr - 1].z >> 8) < 22)) continue;
    int ok = 1; for (int v = 0; v < nvil; v++) { int dx = ((VIL[v].ci - px + 64) & 127) - 64, dz = ((VIL[v].cj - j + 64) & 127) - 64; if (dx * dx + dz * dz < 25) ok = 0; }
    if (!ok) continue;
    BRG[nbr].x = px * 256 + 128; BRG[nbr].z = j * 256 + 128; BRG[nbr].top = (hm + 4) * 8; nbr++; }
}
static void grow_looks2(void);
static void grow_looks(void) {
  for (int v = 0; v < 32; v++) for (int u = 0; u < 32; u++) { int k = v * 32 + u; uint32_t h = mixh((uint32_t)u, (uint32_t)v, 77);
    TEX[X_GRASS][k] = (uint8_t)(2 + (h & 3) + ((((u + (int)(h >> 8 & 1)) & 3) == 0) ? 2 : 0));
    TEX[X_ROCK][k] = (uint8_t)(2 + ((v / 3 + (int)(mixh((uint32_t)u / 8, (uint32_t)v / 3, 5) & 1)) & 1) * 3 + (h & 1) - ((((u * 3 + v) & 15) == 0) ? 2 : 0));
    TEX[X_SNOW][k] = (uint8_t)(5 + (h & 1) + ((h & 63) == 0 ? 1 : 0));
    TEX[X_SAND][k] = (uint8_t)(3 + (((u + v / 2 + (int)(h & 1)) >> 2) & 1) * 2 + (h >> 4 & 1));
    TEX[X_WATER][k] = (uint8_t)(3 + ((SIN(u * 128) + SIN(v * 256 + u * 64)) >> 14) + ((h & 31) == 0 ? 3 : 0));
    TEX[X_SCORCH][k] = (uint8_t)(1 + (h & 1) + ((h & 15) == 0 ? 5 : 0) + ((h & 63) == 1 ? 3 : 0));
    { int pl = (u & 15) == 0 || (v & 15) == 0, rv = (u & 7) == 4 && (v & 15) == 8;                      // panels, rivets, weather
      TEX[X_SKIN][k] = (uint8_t)(pl ? 3 : rv ? 6 : 4 + (int)((vnoise((uint32_t)u * 8, (uint32_t)v * 8, 6, 31) >> 15) & 1)); }
    TEX[X_FLAT][k] = 6;
    { int du = u * 2 - 31, dv = v * 2 - 31, r2 = du * du + dv * dv, rr = 26 + (int)(mixh((uint32_t)(u / 4), (uint32_t)(v / 4), 33) % 7);   // a puff with a ragged rim
      TEX[X_PUFF][k] = (uint8_t)(r2 >= rr * rr ? 0 : 7 - r2 * 5 / (rr * rr) - (int)(h & 1)); if (r2 < rr * rr && !TEX[X_PUFF][k]) TEX[X_PUFF][k] = 1; }
    { int au = u < 16 ? 16 - u : u - 16; TEX[X_STREAK][k] = (uint8_t)(au < 2 ? 7 : au < 6 ? 5 : ((u + v) & 15) < 3 ? 6 : 3); }
    TEX[X_CANOPY][k] = (uint8_t)(1 + (int)((vnoise((uint32_t)u * 16, (uint32_t)v * 16, 6, 51) >> 14) & 3) + (int)(h & 1) * 2);           // clumped leaves
    TEX[X_ROWS][k] = (uint8_t)((((u + (v >> 3)) & 3) == 0 ? 1 : 4) + (int)(h & 1) + ((h & 31) == 0 ? 2 : 0));                            // furrows
    TEX[X_TILES][k] = (uint8_t)((v & 3) == 0 ? 1 : 4 + (int)(((u + (v >> 2) * 2) >> 2) & 1) + (int)(h & 1));                            // roof tiles in courses
    { int wn = (u & 15) >= 5 && (u & 15) <= 10 && (v & 15) >= 4 && (v & 15) <= 10; TEX[X_PLASTER][k] = (uint8_t)(wn ? 0 : 5 + (int)(h & 1)); } }   // windows
  for (int k = 0; k < 32 * 32; k++) for (int m = 0; m < NTEX; m++) if (TEX[m][k] > 7) TEX[m][k] = 7;
  grow_looks2();
}
static const uint8_t DK[NMAT][3] = {{36, 70, 30}, {96, 52, 40}, {150, 160, 190}, {150, 110, 70}, {20, 50, 100}, {14, 10, 10}, {30, 4, 0}, {10, 30, 14}, {120, 96, 30}, {60, 36, 20}, {80, 20, 14}, {110, 100, 84},
    {60, 66, 74}, {90, 70, 40}, {40, 50, 25}, {20, 28, 50}, {15, 15, 18}, {20, 30, 40}, {90, 10, 10}, {255, 90, 20}, {255, 160, 40}, {200, 40, 0}, {20, 20, 22}, {150, 140, 160}};
static const uint8_t BR[NMAT][3] = {{110, 160, 60}, {210, 130, 80}, {255, 255, 255}, {240, 200, 130}, {110, 180, 230}, {90, 60, 40}, {255, 200, 60}, {60, 120, 50}, {250, 220, 110}, {150, 100, 60}, {220, 90, 60}, {250, 240, 215},
    {190, 200, 210}, {230, 200, 140}, {140, 160, 90}, {90, 110, 160}, {70, 70, 80}, {160, 220, 255}, {240, 60, 40}, {255, 240, 180}, {255, 255, 200}, {255, 250, 170}, {150, 145, 140}, {255, 250, 250}};
static void look_mat(int m, const uint8_t *dk, const uint8_t *br); static void grow_sky(void);
static void grow_looks2(void) {
  for (int m = 0; m < NMAT; m++) look_mat(m, DK[m], BR[m]);
  grow_sky();
  SUNC = glass(255, 240, 200); HUDC = glass(255, 255, 255); HUDW = glass(255, 230, 150); HUDR = glass(255, 80, 60); HUDG = glass(80, 255, 140);
}
static int tod_now;
// ACE 5 K9: the day turns. tod 0 is ACE 4's dusk (the first 60 s are the same picture), 256 is high day. The fog IS the horizon,
// so both move together: one material's 1,024 colours are regrown a frame (a sweep of 24 frames), the sky with material 0.
static void day_mix(int *fr, int *fg, int *fb, int t) { *fr = 246 + (172 - 246) * t / 256; *fg = 166 + (204 - 166) * t / 256; *fb = 120 + (236 - 120) * t / 256; }
static void look_mat(int m, const uint8_t *dk, const uint8_t *br) {
  int FR, FG, FB; day_mix(&FR, &FG, &FB, tod_now); int day = tod_now * 3 / 256;
  for (int f = 0; f < 8; f++) for (int l = 0; l < 16; l++) for (int t = 0; t < 8; t++) {
    int ll = LITM(m) ? 15 : l;
    int r = dk[0] + (br[0] - dk[0]) * t / 7, g = dk[1] + (br[1] - dk[1]) * t / 7, b = dk[2] + (br[2] - dk[2]) * t / 7;
    r = r * (ll + 5 + day) / 20; g = g * (ll + 5 + day) / 20; b = b * (ll + 5 + day) / 20; if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    if (ll == 15 && m != M_WATER) { r += 8; g += 8; b += 8; }
    r += (FR - r) * f / 8; g += (FG - g) * f / 8; b += (FB - b) * f / 8;
    LUT[m][f][l][t] = glass(r, g, b); }
}
static void grow_sky(void) {
  int FR, FG, FB; day_mix(&FR, &FG, &FB, tod_now); int zr = 24 + (40 - 24) * tod_now / 256, zg = 30 + (100 - 30) * tod_now / 256, zb = 90 + (200 - 90) * tod_now / 256;
  for (int h = 0; h < HMAX; h++) { int t = h < HORIZ ? 256 - h * 256 / HORIZ : 0;   // the sky, top to horizon
    SKYH[h] = glass(zr + (FR - zr) * t / 256, zg + (FG - zg) * t * t / 65536, zb + (FB - zb) * t / 256); }
}

// ---- the setup of one triangle (corners already in front of the near plane)
static int zq;                    // 4 while the flyer is set up: its corners are in 1/16 units (the same picture, finer)
static int32_t clamp32(int64_t v) { return v > 2000000000LL ? 2000000000 : v < -2000000000LL ? -2000000000 : (int32_t)v; }
static inline uint64_t mulhi64(uint64_t a, uint64_t b) {   // the top 64 bits of a 128-bit product, from 32-bit halves
  uint64_t al = (uint32_t)a, ah = a >> 32, bl = (uint32_t)b, bh = b >> 32;
  uint64_t ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
  uint64_t mid = (ll >> 32) + (uint32_t)lh + (uint32_t)hl;
  return hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
}
static inline int32_t per_area(int64_t n, uint64_t ra, int neg) {   // n / area, toward zero (the last unit may fall one short)
  int ng = (n < 0) != neg; uint64_t q = mulhi64((uint64_t)(n < 0 ? -n : n), ra);
  int64_t v = q > 4000000000ULL ? 4000000000LL : (int64_t)q; return clamp32(ng ? -v : v);
}
static inline int32_t slope16(int32_t dx, int32_t dy) {     // (dx << 16) / dy, in 32 bits when it fits
  if (dx > -32768 && dx < 32768) return (dx * 65536) / dy;
  return clamp32(((int64_t)dx << 16) / dy);
}
static void tri_setup(const CV *a, const CV *b, const CV *c, int mat, int fog, int cull) {
  if (ntri >= MAXTRI) return;
  int32_t X[3], Y[3], W[3], U[3], V[3], L[3]; const CV *p[3] = {a, b, c};
  for (int k = 0; k < 3; k++) { int32_t z = p[k]->z;
    int32_t ax_ = p[k]->x < 0 ? -p[k]->x : p[k]->x, ay_ = p[k]->y < 0 ? -p[k]->y : p[k]->y;
    X[k] = SW * 8 + (ax_ < 800000 ? p[k]->x * (FOC * 16) / z : (int32_t)((int64_t)p[k]->x * FOC * 16 / z));
    Y[k] = HORIZ * 16 - (ay_ < 800000 ? p[k]->y * (FOC * 16) / z : (int32_t)((int64_t)p[k]->y * FOC * 16 / z));
#ifndef NO_PS1
    X[k] = (X[k] + 8) & ~15; Y[k] = (Y[k] + 8) & ~15;       // PS1: corners land on whole dots, so edges step and shimmer as the eye moves
#endif
    W[k] = (int32_t)(((uint32_t)1 << (24 + zq)) / (uint32_t)z); U[k] = p[k]->u * W[k]; V[k] = p[k]->v * W[k]; L[k] = p[k]->l; W[k] <<= 8; }
  int64_t dx1 = X[1] - X[0], dy1 = Y[1] - Y[0], dx2 = X[2] - X[0], dy2 = Y[2] - Y[0], area = dx1 * dy2 - dx2 * dy1;
  if (area == 0 || (cull && area < 0)) return;
  if ((X[0] < 0 && X[1] < 0 && X[2] < 0) || (X[0] >= SW * 16 && X[1] >= SW * 16 && X[2] >= SW * 16)) return;
  if ((Y[0] < 0 && Y[1] < 0 && Y[2] < 0) || (Y[0] >= SH * 16 && Y[1] >= SH * 16 && Y[2] >= SH * 16)) return;
  Tri *t = &TR[ntri];
  int o[3] = {0, 1, 2};                                   // sort the corners top to bottom
  if (Y[o[1]] < Y[o[0]]) { int q = o[0]; o[0] = o[1]; o[1] = q; } if (Y[o[2]] < Y[o[1]]) { int q = o[1]; o[1] = o[2]; o[2] = q; } if (Y[o[1]] < Y[o[0]]) { int q = o[0]; o[0] = o[1]; o[1] = q; }
  for (int k = 0; k < 3; k++) { t->x[k] = X[o[k]]; t->y[k] = Y[o[k]]; }
  int ry0 = (t->y[0] + 7) >> 4, ry1 = (t->y[2] - 9) >> 4;   // rows whose middles lie inside
  if (ry0 < 0) ry0 = 0; if (ry1 > SH - 1) ry1 = SH - 1; if (ry0 > ry1) return;
  t->ry0 = (int16_t)ry0; t->ry1 = (int16_t)ry1;
  t->s02 = t->y[2] > t->y[0] ? slope16(t->x[2] - t->x[0], t->y[2] - t->y[0]) : 0;
  t->s01 = t->y[1] > t->y[0] ? slope16(t->x[1] - t->x[0], t->y[1] - t->y[0]) : 0;
  t->s12 = t->y[2] > t->y[1] ? slope16(t->x[2] - t->x[1], t->y[2] - t->y[1]) : 0;
  t->ax = X[0]; t->ay = Y[0];
  // planes: the change of each value per dot across and down (per 1/16 dot x 16)
  // one division a triangle: the area's reciprocal once, then each gradient is a high-half product (AERIE K10)
  const int neg = area < 0; const uint64_t aa = (uint64_t)(neg ? -area : area), RA = ~(uint64_t)0 / aa;
  #define PLANE(A, a0, ax_, ay_) { int64_t d1 = (int64_t)A[1] - A[0], d2 = (int64_t)A[2] - A[0]; a0 = A[0]; ax_ = per_area((d1 * dy2 - d2 * dy1) * 16, RA, neg); ay_ = per_area((d2 * dx1 - d1 * dx2) * 16, RA, neg); }
  PLANE(W, t->W, t->Wx, t->Wy); PLANE(U, t->U, t->Ux, t->Uy); PLANE(V, t->V, t->Vx, t->Vy); PLANE(L, t->L, t->Lx, t->Ly);
  t->mat = (uint8_t)mat; t->fog = (uint8_t)fog;
  { int li = (int)((L[0] + L[1] + L[2]) / 3) >> 8; li = li < 0 ? 0 : li > 15 ? 15 : li; t->fc = LUT[mat][fog][li][4]; }   // its one colour, for slivers
  ntri++;
}
static void lerp_cv(const CV *a, const CV *b, CV *o) {    // where the edge a-b meets the near plane
  int32_t num = (NEAR << zq) - a->z, den = b->z - a->z;
  o->x = a->x + (int32_t)((int64_t)(b->x - a->x) * num / den); o->y = a->y + (int32_t)((int64_t)(b->y - a->y) * num / den); o->z = NEAR << zq;
  o->u = a->u + (b->u - a->u) * num / den; o->v = a->v + (b->v - a->v) * num / den; o->l = a->l + (b->l - a->l) * num / den;
}
static void tri(const CV *a, const CV *b, const CV *c, int mat, int fog, int cull) {
  const CV *p[3] = {a, b, c}; const int32_t nz = NEAR << zq; int in = (a->z >= nz) + (b->z >= nz) + (c->z >= nz);
  if (in == 3) { tri_setup(a, b, c, mat, fog, cull); return; }
  if (in == 0) return;
  CV q[4]; int n = 0;                                    // clip against the near plane: 3 or 4 corners remain
  for (int k = 0; k < 3; k++) { const CV *s = p[k], *e = p[(k + 1) % 3];
    if (s->z >= nz) q[n++] = *s;
    if ((s->z >= nz) != (e->z >= nz)) lerp_cv(s, e, &q[n++]); }
  tri_setup(&q[0], &q[1], &q[2], mat, fog, cull); if (n == 4) tri_setup(&q[0], &q[2], &q[3], mat, fog, cull);
}
// ---- one band: the sky first, then each binned triangle's spans, nearest wins
#ifndef THIN
#define THIN 4                    // land spans this short are drawn in one colour
#endif
#ifndef THIN_FLYER
#define THIN_FLYER 10             // the jets' skins: a short span shows one colour
#endif
#ifndef HAZE
#define HAZE 4                    // fog step from which a triangle is drawn in one colour
#endif
#ifndef GRAIN_AT
#define GRAIN_AT 1600000u         // work units over which the next frame is laid a grain coarser
#endif
static uint32_t g_it, g_sp;       // this frame's dots visited and spans begun: the frame's own work ledger
static int grain;                 // 1 when the last frame's ledger read heavy
static void span(const Tri *t, int py, int pa, int pe, uint16_t *d, uint16_t *zb) {
  g_it += (uint32_t)(pe - pa); g_sp++;
  const int cut = CUTM(t->mat);
  const int32_t dxs = pa * 16 + 8 - t->ax, dys = py * 16 + 8 - t->ay;   // both fit 32 bits: each product is one widening multiply
  const int land = !cut && t->mat < M_SKIN0, coarse = grain && land;
  if (!cut && (pe - pa <= (t->mat >= M_SKIN0 ? THIN_FLYER : THIN) + grain * 3 || t->fog >= HAZE - coarse * 2)) {   // a sliver, or deep haze: depth and one colour
    int32_t W = t->W + (int32_t)(((int64_t)t->Wx * dxs + (int64_t)t->Wy * dys) >> 4), Wx = t->Wx; const uint16_t c = t->fc;
    for (int x = pa; x < pe; x++) { int32_t z = W >> 12; if (z > zb[x]) { zb[x] = (uint16_t)z; d[x] = c; } W += Wx; }
    return; }
  int32_t W = t->W + (int32_t)(((int64_t)t->Wx * dxs + (int64_t)t->Wy * dys) >> 4), U = t->U + (int32_t)(((int64_t)t->Ux * dxs + (int64_t)t->Uy * dys) >> 4);
  int32_t V = t->V + (int32_t)(((int64_t)t->Vx * dxs + (int64_t)t->Vy * dys) >> 4), L = t->L + (int32_t)(((int64_t)t->Lx * dxs + (int64_t)t->Ly * dys) >> 4);
  const uint8_t *tx = TEX[TEXOF[t->mat]]; const uint16_t *lut = &LUT[t->mat][t->fog][0][0];
  const int32_t Wx = t->Wx, Ux = t->Ux, Vx = t->Vx, Lx = t->Lx;
  for (int x = pa; x < pe;) {
    const int sl = coarse ? 32 : 16; int n = pe - x > sl ? sl : pe - x;   // K6: a heavy frame's land is perspective-true every 32nd dot
    int32_t w0 = W >> 8; if (w0 < 1) w0 = 1; int32_t u0 = U / w0, v0 = V / w0;
    int32_t U2 = U + Ux * n, V2 = V + Vx * n, w1 = (W + Wx * n) >> 8; if (w1 < 1) w1 = 1;
    int32_t du = (U2 / w1 - u0), dv = (V2 / w1 - v0); if (n == 16) { du >>= 4; dv >>= 4; } else if (n == 32) { du >>= 5; dv >>= 5; } else { du /= n; dv /= n; }
    int32_t lm = L + Lx * (n >> 1); int li = lm >> 8; li = li < 0 ? 0 : li > 15 ? 15 : li; const uint16_t *lc = lut + li * 8;   // light once per 16 dots
#ifndef NO_PS1
    // PS1: the dots of a checkerboard take the next light step up at two thresholds, so 16 steps read as 31 (a dither, no extra table)
    int fr = lm & 255, l1 = li + (fr >= 64) , l2 = li + (fr >= 192); l1 = l1 > 15 ? 15 : l1; l2 = l2 > 15 ? 15 : l2;
    const uint16_t *lA = lut + (((x ^ py) & 1) ? l2 : l1) * 8, *lB = lut + (((x ^ py) & 1) ? l1 : l2) * 8;
#else
    const uint16_t *lA = lc, *lB = lc;
#endif
    int e = x + n; const int xs = x; (void)lc;
    if (coarse) {                // K6, a heavy frame: the land reads one texel per four dots (each dot keeps its own depth)
      const int32_t du4 = du * 4, dv4 = dv * 4;
      for (; x + 3 < e; x += 4) {
        int tv = tx[((v0 >> 4) & 31) * 32 + ((u0 >> 4) & 31)]; const uint16_t ca = lA[tv], cb = lB[tv];
        int32_t z = W >> 12; if (z > zb[x]) { zb[x] = (uint16_t)z; d[x] = ca; } W += Wx;
        z = W >> 12; if (z > zb[x + 1]) { zb[x + 1] = (uint16_t)z; d[x + 1] = cb; } W += Wx;
        z = W >> 12; if (z > zb[x + 2]) { zb[x + 2] = (uint16_t)z; d[x + 2] = ca; } W += Wx;
        z = W >> 12; if (z > zb[x + 3]) { zb[x + 3] = (uint16_t)z; d[x + 3] = cb; } W += Wx;
        u0 += du4; v0 += dv4; } }
    else if ((!cut && t->fog >= 2) || (du > -16 && du < 16 && dv > -16 && dv < 16)) {   // K5: a magnified puff pairs too (texel 0 is no dot for both)   // a texel spans two dots (or the haze hides it): one read per pair
      const int32_t W2 = Wx * 2, du2 = du * 2, dv2 = dv * 2;
      for (; x + 1 < e; x += 2) {
        int tv = tx[((v0 >> 4) & 31) * 32 + ((u0 >> 4) & 31)];
        int32_t z = W >> 12, z1 = (W + Wx) >> 12;
        if (tv || !cut) { if (z > zb[x]) { zb[x] = (uint16_t)z; d[x] = lA[tv]; }
          if (z1 > zb[x + 1]) { zb[x + 1] = (uint16_t)z1; d[x + 1] = lB[tv]; } }
        W += W2; u0 += du2; v0 += dv2; } }
    for (; x < e; x++) {
      int32_t z = W >> 12;                                 // never above 43,690: 1/z is largest at the near plane
      if (z > zb[x]) { int tv = tx[((v0 >> 4) & 31) * 32 + ((u0 >> 4) & 31)]; if (tv || !cut) { zb[x] = (uint16_t)z; d[x] = ((x - xs) & 1) ? lB[tv] : lA[tv]; } }
      W += Wx; u0 += du; v0 += dv; }
    L += Lx * n; U = U2; V = V2; }
}
static char GHUD[54], GPLACE[20], GMIS[40]; static int cloud_in;
#if !defined(DESK) && !defined(MOUTH_FIFO) && !defined(DEMO) && FRAMES == 0   // bench builds read no pins
static int stick_test, raw_x, raw_y, raw_b; static void stick_line(void);
#endif
static inline uint16_t sky_at(int32_t h) { return SKYH[h < 0 ? 0 : h >= HMAX ? HMAX - 1 : h]; }
static void glide_band(int b, uint16_t *rb) {
  memset(ZB, 0, sizeof ZB);
  const int y0 = b * TS;
  // the sky is laid first, two dots a word, as a reading of the height above the TRUE horizon measured across the roll
  for (int r = 0; r < TS; r++) { int y = y0 + r; uint32_t *d2 = (uint32_t *)(rb + r * SW); int32_t bb = HORIZ - y;
    if (!sr) { uint32_t c = (uint32_t)sky_at(hz - HORIZ + (bb * cr >> 14)) * 0x10001u; for (int x = 0; x < SW / 2; x++) d2[x] = c; }
    else { int32_t acc = (hz - HORIZ) * 16384 + bb * cr + (2 - SW / 2) * sr, st = 4 * sr;   // a reading every 4 dots
      for (int x = 0; x < SW / 2; x += 2) { uint32_t c = (uint32_t)sky_at(acc >> 14) * 0x10001u; d2[x] = c; d2[x + 1] = c; acc += st; } }
    int dy = y - sun_y;
    if (sun_on && dy > -22 && dy < 22) { uint16_t *d = rb + r * SW; int xa = sun_x - 22 < 0 ? 0 : sun_x - 22, xe = sun_x + 22 > SW ? SW : sun_x + 22;
      for (int x = xa; x < xe; x++) { int dx = x - sun_x, q = dx * dx + dy * dy; if (q < 100) d[x] = SUNC; else if (q < 480) d[x] = LUT[M_FIRE][0][15][7 - (q - 100) / 64]; } } }
  for (int q = BINAT[b]; q < BINAT[b + 1]; q++) { const Tri *t = &TR[TBIN[q]];
    int ya = t->ry0 > y0 ? t->ry0 : y0, ye = t->ry1 < y0 + TS - 1 ? t->ry1 : y0 + TS - 1;
    for (int py = ya; py <= ye; py++) { int32_t yc = py * 16 + 8;
      int32_t xl = t->x[0] + (int32_t)(((int64_t)(yc - t->y[0]) * t->s02) >> 16);
      int32_t xr = yc < t->y[1] ? t->x[0] + (int32_t)(((int64_t)(yc - t->y[0]) * t->s01) >> 16) : t->x[1] + (int32_t)(((int64_t)(yc - t->y[1]) * t->s12) >> 16);
      if (xl > xr) { int32_t q2 = xl; xl = xr; xr = q2; }
      int pa = (xl + 7) >> 4, pe = (xr + 7) >> 4; if (pa < 0) pa = 0; if (pe > SW) pe = SW; if (pa >= pe) continue;
      span(t, py, pa, pe, rb + (py - y0) * SW, ZB + (py - y0) * SW); } }
  if (cloud_in) { uint16_t cw = LUT[M_CLOUD][0][13][7];                                // inside a cloud: a dither of white, deeper inside more (the PS1's way)
    for (int r = 0; r < TS; r++) { uint16_t *d = rb + r * SW; int y = y0 + r; for (int x = (y & 1) * 2 + (y & 2 ? 1 : 0); x < SW; x += 4) d[x] = cw;
      if (cloud_in > 1) for (int x = ((y & 1) * 2 + 2) & 3; x < SW; x += 4) d[x] = cw;
      if (cloud_in > 2) for (int x = ((y & 1) * 2 + 1) & 3; x < SW; x += 4) d[x] = cw; } }
  if (b == 0) { for (int y = 3; y < 10; y++) text_row(y, 0, SW, rb + y * SW, GHUD, 4, 3, 1, HUDC); if (GPLACE[0]) for (int y = 13; y < 16; y++) text_row(y, 0, SW, rb + y * SW, GPLACE, 4, 13, 1, HUDW); }
  if (b == 1 && GPLACE[0]) for (int y = 16; y < 20; y++) text_row(y, 0, SW, rb + (y - 16) * SW, GPLACE, 4, 13, 1, HUDW);
  if (b == TY - 1 && GMIS[0]) for (int y = SH - 11; y < SH - 4; y++) text_row(y, 0, SW, rb + (y - (TY - 1) * TS) * SW, GMIS, 4, SH - 11, 1, HUDW);
}
// LESSON-BEGIN 8 of 18: The air: lift, drag, gusts, thermals, ridge lift
// HOLDING : places and speeds in 1/16 units (a cell is 256 units; a span about 126). Angles in 1/16 ring steps.
// WHAT    : air_up is a rule of place (thermals over sunlit ground, lift where wind meets a ridge), read by every jet alike.
// LESSON-END
// ================================================================ the air: lift, drag, gusts, thermals, ridge lift
// Positions and speeds are in 1/16 units (a cell is 256 units; the flyer's wings span about 126). Angles in 1/16 ring steps.
#define G16 12                    // gravity, 1/16 units per frame per frame
static int32_t ground(int32_t wx, int32_t wz) {          // the height of the very triangles the renderer lays (units)
  int i = (wx >> 8) & 127, j = (wz >> 8) & 127, fx = wx & 255, fz = wz & 255;
  int a = TH[j * TN + i], b2 = TH[j * TN + ((i + 1) & 127)], c = TH[((j + 1) & 127) * TN + i], d = TH[((j + 1) & 127) * TN + ((i + 1) & 127)];
  // ACE 4 K2: this was a smooth (bilinear) blend of the four corners, but the glass shows two flat triangles split on the
  // a-d diagonal. On a ridge the two differ by up to 160 units: the jet hit air over a drawn valley, or passed through a drawn ridge.
  return fx >= fz ? (a * 2048 + (b2 - a) * fx * 8 + (d - b2) * fz * 8) >> 8 : (a * 2048 + (c - a) * fz * 8 + (d - c) * fx * 8) >> 8;
}
#define HOUSE_W 70                // half sizes and heights of what stands on a cell: drawn and struck by the same numbers
#define HOUSE_D 50
#define HOUSE_H 56
#define HOUSE_R 92
#define TREE_R 40
#define TREE_H 120
static int32_t solid(int32_t wx, int32_t wz) {          // the ground and what stands on it: the height a jet strikes
  int32_t g = ground(wx, wz); int i = (wx >> 8) & 127, j = (wz >> 8) & 127, o = obj_at(i, j);
  { int32_t b = bridge_at(wx, wz); if (b > g) g = b; }
  if (!o) return g;
  int32_t dx = (wx & 255) - 128, dz = (wz & 255) - 128, base = TH[j * TN + i] * 8;
  if ((o & 15) == OB_TREE) { int32_t r2 = dx * dx + dz * dz; if (r2 < TREE_R * TREE_R) { int32_t t = base + TREE_H * (TREE_R * TREE_R - r2) / (TREE_R * TREE_R); return t > g ? t : g; } return g; }
  if (o & 16) { int32_t q = dx; dx = dz; dz = q; }
  if (dx > -HOUSE_W && dx < HOUSE_W && dz > -HOUSE_D && dz < HOUSE_D) { int32_t adz = dz < 0 ? -dz : dz, t = base + HOUSE_H + (HOUSE_R - HOUSE_H) * (HOUSE_D - adz) / HOUSE_D; return t > g ? t : g; }
  return g;
}
// the rising air at a point (1/16 units per frame): broad thermals that drift, the wind thrown up a slope, and fine gusts
static int32_t air_up(int32_t x, int32_t y, int32_t z, uint32_t t) {
  int32_t th = ((vnoise((uint32_t)(x + 400000 + (int32_t)t * 3), (uint32_t)(z + 400000), 10, 41) >> 8) - 150) / 6;
  int32_t g = ground(x, z), slope = g - ground(x - 160, z);       // the wind blows toward +x
  int32_t above = y - g, fade = above < 0 ? 256 : above > 700 ? 0 : 256 - above * 256 / 700;
  int32_t ridge = slope * fade / 160; if (ridge > 40) ridge = 40; if (ridge < -40) ridge = -40;   // K1: unbounded, a cliff threw up to 75 units a frame
  int32_t gust = ((vnoise((uint32_t)(x + 400000), (uint32_t)(z + 400000 + (int32_t)t * 24), 7, 43) >> 8) - 128) / 10;
  return th + ridge + gust;
}
// LESSON-BEGIN 9 of 18: The eye, and the flyer grown into triangles every frame
// WHAT    : to_eye takes a world place into the eye's frame by the ring's headings. The eye banks with your jet.
// LESSON-END
// ================================================================ the eye, and the flyer grown into triangles every frame
static int32_t cxw, cyw, czw, cc, cs, cpc, cps;              // the eye (units), and its turn and tilt
static void to_eye(int32_t wx, int32_t wy, int32_t wz, CV *o) {
  int32_t dx = wx - cxw, dy = wy - cyw, dz = wz - czw;
  int32_t x1 = (dx * cc - dz * cs) >> 14, z1 = (dx * cs + dz * cc) >> 14;
  int32_t y1 = (dy * cpc + z1 * cps) >> 14; o->z = (z1 * cpc - dy * cps) >> 14;
  o->x = (int32_t)(((int64_t)x1 * cr + (int64_t)y1 * sr) >> 14); o->y = (int32_t)(((int64_t)y1 * cr - (int64_t)x1 * sr) >> 14);   // the roll
}
static void eye16(const int32_t *w, CV *o) {               // the same, in 1/16 units
  int64_t dx = w[0] - cxw * 16, dy = w[1] - cyw * 16, dz = w[2] - czw * 16;
  int64_t x1 = (dx * cc - dz * cs) >> 14, z1 = (dx * cs + dz * cc) >> 14;
  int64_t y1 = (dy * cpc + z1 * cps) >> 14; o->z = (int32_t)((z1 * cpc - dy * cps) >> 14);
  o->x = (int32_t)((x1 * cr + y1 * sr) >> 14); o->y = (int32_t)((y1 * cr - x1 * sr) >> 14);
}
static int fog_of(int32_t z) { int f = (int)((z - 1600) / 380); return f < 0 ? 0 : f > 7 ? 7 : f; }
static void glide_cell(int i, int j, int ci, int cj, int sp) {   // one terrain cell (sp = 1) or a 2 x 2 block (sp = 2)
  const int Wd = 2 * RV + 1;
    int k00 = j * Wd + i, k10 = k00 + sp, k01 = k00 + sp * Wd, k11 = k01 + sp; int ks[4] = {k00, k10, k01, k11};
    int front = 0, right = 0, left = 0, far = 0;
    for (int q = 0; q < 4; q++) { int32_t z = VCZ[ks[q]], x = VCX[ks[q]]; if (z >= NEAR) front++; if (z > RV * 256) far++; if (x * 10 > z * 11 + 300) right++; if (-x * 10 > z * 11 + 300) left++; }
    if (!front || far == 4 || right == 4 || left == 4) return;
    int gi = ci - RV + i, gj = cj - RV + j, cell = (gj & 127) * TN + (gi & 127);
    CV a, b, c, d;
    a.x = VCX[k00]; a.y = VCY[k00]; a.z = VCZ[k00]; a.u = 0; a.v = 0; a.l = (TL[cell] & 15) << 8;
    b.x = VCX[k10]; b.y = VCY[k10]; b.z = VCZ[k10]; b.u = 512 * sp; b.v = 0; b.l = (TL[(gj & 127) * TN + ((gi + sp) & 127)] & 15) << 8;
    c.x = VCX[k01]; c.y = VCY[k01]; c.z = VCZ[k01]; c.u = 0; c.v = 512 * sp; c.l = (TL[((gj + sp) & 127) * TN + (gi & 127)] & 15) << 8;
    d.x = VCX[k11]; d.y = VCY[k11]; d.z = VCZ[k11]; d.u = 512 * sp; d.v = 512 * sp; d.l = (TL[((gj + sp) & 127) * TN + ((gi + sp) & 127)] & 15) << 8;
    int fog = fog_of((a.z + d.z) / 2), m = TL[cell] >> 4;
    tri(&a, &d, &b, m, fog, 1); tri(&a, &c, &d, m, fog, 1); }
static int lit_n(int64_t nx, int64_t ny, int64_t nz, int two) {   // light from the low sun, of 15 (two-sided: a lit skin glows through)
  int64_t n2 = nx * nx + ny * ny + nz * nz; while (n2 > 0xffffffffLL) { n2 >>= 2; nx >>= 1; ny >>= 1; nz >>= 1; }
  int64_t nl = (int64_t)root((uint32_t)n2) + 1, dt = (-48 * nx + 64 * ny + 40 * nz) / nl;
  if (dt < 0) dt = two ? -dt * 2 / 3 : 0;
  int li = (int)(3 + dt * 12 / 89); return li > 15 ? 15 : li;
}
static int lit3(const int32_t *a, const int32_t *b, const int32_t *c) {
  int64_t ax = b[0] - a[0], ay = b[1] - a[1], az = b[2] - a[2], bx = c[0] - a[0], by = c[1] - a[1], bz = c[2] - a[2];
  return lit_n(ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx, 1);
}
static int fog16(int32_t z16) { return fog_of(z16 >> 4); }
static void quad16(const int32_t *a, const int32_t *b, const int32_t *c, const int32_t *d, int mat, int li, int32_t v0, int32_t v1) {   // a-b leading, d-c trailing
  CV p[4]; eye16(a, &p[0]); eye16(b, &p[1]); eye16(c, &p[2]); eye16(d, &p[3]);
  p[0].u = 0; p[1].u = 511; p[2].u = 511; p[3].u = 0; p[0].v = p[1].v = v0; p[2].v = p[3].v = v1;
  for (int q = 0; q < 4; q++) p[q].l = li << 8;
  int fog = fog16(p[0].z); tri(&p[0], &p[1], &p[2], mat, fog, 0); tri(&p[0], &p[2], &p[3], mat, fog, 0);
}
// LESSON-BEGIN 10 of 18: Airframes from one seed each
// WHAT    : a seed decides span, sweep, chords, fuselage, fins, tailplanes, canopy and the camouflage. Nothing is a stored model.
// LESSON-END
// ================================================================ airframes grown from one number each
// A seed decides span, sweep, chords, fuselage, fins (one or two), tailplanes, canopy and the two skins of its camouflage.
// No airframe is stored: every frame each one is grown again from its seed, the state of its parts, and its motion.
static void grow_shape(uint32_t seed, Shape *s) {
  uint32_t h = mixh(seed, 1, 21), g = mixh(seed, 2, 21);
  s->span = (int16_t)(30 + h % 13); s->chord = (int16_t)(16 + (h >> 4) % 9); s->tipc = (int16_t)(4 + (h >> 8) % 7);
  s->sweep = (int16_t)(60 + (h >> 12) % 380); s->flen = (int16_t)(54 + (h >> 20) % 13); s->frad = (int16_t)(4 + (h >> 24) % 3);
  s->fin = (int16_t)(10 + g % 7); s->tspan = (int16_t)(10 + (g >> 4) % 7); s->twin = (int16_t)((g >> 8) & 1); s->dihed = (int16_t)((int)((g >> 10) % 9) - 4);
  s->skin = (int16_t)(M_SKIN0 + (g >> 14) % 4); s->skin2 = (int16_t)(M_SKIN0 + (s->skin - M_SKIN0 + 1 + (g >> 18) % 3) % 4);
}

// LESSON-BEGIN 11 of 18: THE JETS: parts as a ledger
// LEDGER  : each part holds a size (of 256) and a load (damage carried). A hit adds load. Past its threshold (PTH: nose 300,
//           engine 260, wings 210, tails and fin 150) the part RECONFIGURES: keeps 2/5 of its load, loses a third of its size;
//           the rest of the load leaves as a torn piece that falls and becomes ground.
// WHY     : the useful pattern's clause, run as code: when demand exceeds what the structure can carry, it reconfigures to a
//           simpler one and vents the rest. No hit points: a jet is what its parts still carry.
// READS   : flight reads the parts: lift from each wing's size, so a torn wing rolls the jet; thrust from the engine; pitch
//           from the tailplanes; the fin holds the nose straight.
// BOOKS   : DAMAGE line on the emulator: in, ground, tears, kills, deaths, unbalanced. unbalanced should read 0.
// LESSON-END
// ================================================================ the jets: one body of flight for the player and the bandits
// Parts are a ledger (the useful pattern). Each part holds a size (of 256) and a load (damage carried). A hit adds load.
// Past its threshold a part RECONFIGURES: it keeps 2/5 of its load and loses a third of its size; the rest of the load
// leaves as a torn piece (debris) that falls and becomes ground. Flight reads the parts: lift from each wing's size, so
// a torn wing rolls the jet; thrust from the engine; pitch from the tailplanes; the fin holds the nose straight.
#define NJ 5                      // you and four bandits (ACE 3 flew three)
enum { P_NOSE, P_ENG, P_WL, P_WR, P_TL, P_TR, P_FIN };
static const int16_t PTH[NPART] = {300, 260, 210, 210, 150, 150, 150};   // each part's threshold
static Jet J[NJ];
static uint32_t dmg_in, dmg_ground, dmg_bad, kills, deaths, wave = 1, tears;
static int cam, demo = 1, killcam, killj;
static int32_t camyaw;
static uint32_t ftris;

// ---- the particles: rounds, missiles, smoke, fire, sparks, torn pieces
enum { K_NONE, K_ROUND, K_MSL, K_SMOKE, K_FIRE, K_SPARK, K_DEBRIS, K_FLARE };
static uint32_t m_fired, m_hit, m_fooled, m_spent, flares_out;   // what every missile came to: a jet, a flare, or nothing
#define NPT 110
static Pt PT[NPT];
static int pt_slot(int ledgered) {                         // a free slot; else the oldest smoke, spark or fire; a torn piece is never dropped
  int best = -1, ba = -1;
  for (int k = 0; k < NPT; k++) { if (!PT[k].kind) return k; int kd = PT[k].kind; if ((kd == K_SMOKE || kd == K_SPARK || kd == K_FIRE) && PT[k].age > ba) { ba = PT[k].age; best = k; } }
  (void)ledgered; return best;
}
static Pt *spawn(int kind, int32_t x, int32_t y, int32_t z, int32_t vx, int32_t vy, int32_t vz, int life) {
  int k = pt_slot(kind == K_DEBRIS); if (k < 0) return 0;
  Pt *p = &PT[k]; memset(p, 0, sizeof *p); p->kind = (int16_t)kind; p->x = x; p->y = y; p->z = z; p->vx = vx; p->vy = vy; p->vz = vz; p->life = (int16_t)life; return p;
}

static int32_t RM[3][3]; static const Jet *CJ;             // the jet being grown: body to world (grain 16384)
static void jet_turn(const Jet *j) {
  CJ = j; int32_t cb = COS(j->bank >> 4), sb = SIN(j->bank >> 4), cp = COS(j->gam >> 4), sp = SIN(j->gam >> 4), cy = COS(j->yaw >> 4), sy = SIN(j->yaw >> 4);
  for (int c = 0; c < 3; c++) { int32_t x = c == 0 ? 16384 : 0, y = c == 1 ? 16384 : 0, z = c == 2 ? 16384 : 0;
    int32_t x1 = (x * cb + y * sb) >> 14, y1 = (y * cb - x * sb) >> 14;
    int32_t y2 = (y1 * cp + z * sp) >> 14, z2 = (z * cp - y1 * sp) >> 14;
    RM[0][c] = (x1 * cy + z2 * sy) >> 14; RM[1][c] = y2; RM[2][c] = (z2 * cy - x1 * sy) >> 14; }
}
static void b2w(int32_t bx, int32_t by, int32_t bz, int32_t *w) {
  w[0] = CJ->x + ((RM[0][0] * bx + RM[0][1] * by + RM[0][2] * bz) >> 14);
  w[1] = CJ->y + ((RM[1][0] * bx + RM[1][1] * by + RM[1][2] * bz) >> 14);
  w[2] = CJ->z + ((RM[2][0] * bx + RM[2][1] * by + RM[2][2] * bz) >> 14);
}
static void part_at(const Jet *j, int p, int32_t *w) {     // where a part sits in the world (1/16 units)
  const Shape *s = &j->sh; int32_t bx = 0, by = 0, bz = 0, L = s->flen * 16;
  switch (p) { case P_NOSE: bz = L * 3 / 10; break; case P_ENG: bz = -L * 3 / 10; break;
    case P_WL: bx = -s->span * 8 * j->S[P_WL] / 256 - s->frad * 16; break; case P_WR: bx = s->span * 8 * j->S[P_WR] / 256 + s->frad * 16; break;
    case P_TL: bx = -s->tspan * 8; bz = -L * 9 / 20; break; case P_TR: bx = s->tspan * 8; bz = -L * 9 / 20; break;
    default: by = s->fin * 8; bz = -L * 2 / 5; }
  jet_turn(j); b2w(bx, by, bz, w);
}
static int32_t part_r(const Jet *j, int p) { const Shape *s = &j->sh; return (p == P_WL || p == P_WR ? s->span * 9 : p <= P_ENG ? s->flen * 5 : s->tspan * 8) + 120; }
static void jet_fwd(const Jet *j, int32_t *f) { int32_t cg = COS(j->gam >> 4); f[0] = cg * SIN(j->yaw >> 4) >> 14; f[1] = SIN(j->gam >> 4); f[2] = cg * COS(j->yaw >> 4) >> 14; }

static void jet_begin(int i, uint32_t seed) {
  Jet *j = &J[i]; memset(j, 0, sizeof *j); j->seed = (int32_t)seed; grow_shape(seed, &j->sh);
  for (int p = 0; p < NPART; p++) j->S[p] = 256;
  j->alive = 1; j->v = 900; j->msl = 2; j->flr = i ? 3 : 6; j->skill = (int32_t)wave; j->tgt = i ? 0 : 1; j->side = (int32_t)(mixh(seed, 4, 4) & 1) * 2 - 1;
  if (i == 0) { j->x = (path_x(4) * 256 + 128) * 16; j->z = 4 * 256 * 16; j->yaw = 0;
#ifdef START_VIL
    if (nvil > START_VIL) { j->x = (VIL[START_VIL].ci * 256 + 128) * 16; j->z = (VIL[START_VIL].cj * 256 + 128 - 2400) * 16; }   // bench: the heaviest view, low over a village and its woods
#endif
  }
  else { const Jet *p = &J[0]; int a = (int)(mixh(seed, 5, 5) & 4095), d = 1500 + (int)(mixh(seed, 6, 5) % 1200);
    j->x = p->x + SIN(a) * d / 1024; j->z = p->z + COS(a) * d / 1024 + 1500 * 16; j->yaw = (int32_t)(((a + 2048) & 4095) * 16); }
  j->y = (ground(j->x >> 4, j->z >> 4) + 700 + (int32_t)(mixh(seed, 7, 5) % 500)) * 16;
}
static void deposit(Jet *j) { for (int p = 0; p < NPART; p++) { dmg_ground += (uint32_t)j->L[p]; j->L[p] = 0; } }
static void tear(Jet *j, int p) {                          // the part reconfigures: KEEP 2/5 of its load, lose a third of its size, vent the rest as a torn piece
  int32_t keep = j->L[p] * 2 / 5, vent = j->L[p] - keep; j->L[p] = keep; j->S[p] = j->S[p] * 2 / 3; if (j->S[p] < 80) j->S[p] = 0; tears++;
  int32_t w[3]; part_at(j, p, w); int32_t f[3]; jet_fwd(j, f);
  Pt *d = spawn(K_DEBRIS, w[0], w[1], w[2], (f[0] * j->v >> 14) / 2 + (int32_t)(mixh((uint32_t)p, frame_n, 9) % 61) - 30, 20, (f[2] * j->v >> 14) / 2, 400);
  if (d) { d->mass = vent; d->mat = (int16_t)j->sh.skin; d->size = (int16_t)(p == P_WL || p == P_WR ? 14 : 8); d->spin = (int16_t)(mixh((uint32_t)p, frame_n, 3) & 4095); }
  else dmg_ground += (uint32_t)vent;
  for (int n = 0; n < 3; n++) { Pt *q = spawn(K_FIRE, w[0], w[1], w[2], (f[0] * j->v >> 14) / 3, 8, (f[2] * j->v >> 14) / 3, 18 + n * 4); if (q) q->size = (int16_t)(10 + n * 6); }
}
static void hit(Jet *j, int p, int32_t dmg) {
  j->L[p] += dmg; dmg_in += (uint32_t)dmg; j->hitf = 4;
  while (j->S[p] && j->L[p] > PTH[p]) tear(j, p);
  if (!j->S[P_NOSE] || !j->S[P_ENG] || (!j->S[P_WL] && !j->S[P_WR])) { if (j->alive) { j->alive = 0; j->dead_t = 0; } }
}

// ---- the scorched earth: a crash heats the cells under it; they glow, then cool to scorch, then the grass returns
#define NSC 24
static int16_t SCC[NSC]; static uint8_t SCO[NSC]; static int16_t SCT[NSC]; static int nsc;
static void scorch(int32_t wx, int32_t wz) {
  int ci = (wx >> 8) & 127, cj = (wz >> 8) & 127;
  for (int dj = -1; dj <= 1; dj++) for (int di = -1; di <= 1; di++) { int c = ((cj + dj) & 127) * TN + ((ci + di) & 127), k = -1;
    for (int n = 0; n < nsc; n++) if (SCC[n] == c) k = n;
    if (k < 0) { if (nsc < NSC) k = nsc++; else { k = (int)(frame_n % NSC); TL[SCC[k]] = SCO[k]; } SCC[k] = (int16_t)c; SCO[k] = TL[c]; }
    SCT[k] = (int16_t)(di == 0 && dj == 0 ? 0 : 60); }
}
static void scorch_step(void) {
  for (int n = 0; n < nsc; n++) { int c = SCC[n], t = ++SCT[n];
    if (t > 1400) { TL[c] = SCO[n]; nsc--; SCC[n] = SCC[nsc]; SCO[n] = SCO[nsc]; SCT[n] = SCT[nsc]; n--; continue; }
    int m = t < 240 ? M_EMBER : M_SCORCH; TL[c] = (uint8_t)((SCO[n] & 15) | m << 4); }
}

// LESSON-BEGIN 12 of 18: Flight: the same lift, drag and stall for every jet, read from its parts
// WHAT    : one jet_step for you and the bandits. Guns: two streams from the wing roots, a round every other frame, heat that
//           locks them out (heat carried as a reading, venting at a rate).
// LESSON-END
// ================================================================ flight: the same lift, drag and stall for every jet, read from its parts
static void explode(int32_t x, int32_t y, int32_t z, int big) {
  for (int n = 0; n < (big ? 7 : 3); n++) { uint32_t h = mixh((uint32_t)n, frame_n, 77);
    Pt *q = spawn(K_FIRE, x, y, z, (int32_t)(h % 41) - 20, (int32_t)((h >> 8) % 21), (int32_t)((h >> 16) % 41) - 20, 20 + n * 3); if (q) q->size = (int16_t)((big ? 40 : 16) + n * 8);
    q = spawn(K_SMOKE, x, y, z, (int32_t)(h % 21) - 10, 4, (int32_t)((h >> 12) % 21) - 10, 70 + n * 8); if (q) q->size = (int16_t)(big ? 50 : 20); }
}
static void jet_step(Jet *j, int me) {
  if (!j->alive) {                                         // falling: no hands, the engine dead, burning
    j->jx = 90 * j->side; j->jy = 30; j->ab = 0; j->fire = 0; j->mfire = 0; j->dead_t++; }
  int32_t vq = (j->v * j->v) >> 8;
  int32_t tail = (j->S[P_TL] + j->S[P_TR]) / 2, wl = j->S[P_WL], wr = j->S[P_WR], aw = (wl + wr) / 2;
  int32_t cl = 40 - j->jy * (me ? 14 : 18) * tail / (10 * 256); if (cl < -60) cl = -60; if (cl > (me ? 220 : 270)) cl = me ? 220 : 270;
  if (j->v < 420) cl = cl * j->v / 420;
  int32_t w = air_up(j->x >> 4, j->y >> 4, j->z >> 4, frame_n) / 4;               // K1: the air moves a jet at most 2 units a frame
  if (w > 32) w = 32; if (w < -32) w = -32;
  { int32_t room = (j->y >> 4) - solid(j->x >> 4, j->z >> 4) - 18, wmin = room > 0 ? -(room * 16 / 8) : 0; if (w < wmin) w = wmin; }   // air cannot blow into the ground (AERIE K10)
  int32_t aL = (vq * cl / 15820) * aw / 256;
  int32_t drag = 3 + cl * cl / 2400 + (7 * 256 - j->S[0] - j->S[1] - j->S[2] - j->S[3] - j->S[4] - j->S[5] - j->S[6]) / 96;
  int32_t aD = vq * drag / 8000;
  int32_t thr = j->alive ? (j->ab ? 7 : me ? 2 : 3) * j->S[P_ENG] / 256 : 0;
  int32_t sg = SIN(j->gam >> 4), cg = COS(j->gam >> 4);
  j->v += thr - aD - (G16 * sg >> 14); if (j->v < 100) j->v = 100; if (j->v > 1800) j->v = 1800;
  if (j->v < 380 && j->gam > 0) j->gam -= (380 - j->v) * 3;   // K3: below flying speed the nose falls (a hammerhead); the old speed floor hung a jet nose-up, climbing forever
  int32_t cb = COS(j->bank >> 4), sb = SIN(j->bank >> 4);
  j->lift = aL; j->load = aL * 256 / G16;
  j->dgam = ((aL * cb >> 14) - (G16 * cg >> 14)) * 10430 / j->v; if (j->dgam > 6000) j->dgam = 6000; if (j->dgam < -6000) j->dgam = -6000;
  j->gam += j->dgam; if (j->gam > 1000 * 16) j->gam = 1000 * 16; if (j->gam < -1000 * 16) j->gam = -1000 * 16;
  j->dyaw = (int32_t)((int64_t)(aL * sb >> 14) * 10430 / j->v * 16384 / (cg > 4000 ? cg : 4000));
  if (j->S[P_FIN] < 256) j->dyaw += (SIN((int)((frame_n * 300u + (uint32_t)me * 1000u) & 4095)) * (256 - j->S[P_FIN]) >> 14);   // no fin: the nose wanders
  j->yaw = (j->yaw + j->dyaw + j->rud * 16) & (4096 * 16 - 1); j->gam += j->trim * 16;
  j->bank += j->jx * 14 + (wr - wl) * aL / 2;              // the stick rolls; a short wing rolls it the other way
  j->bank = ((j->bank + 32768) & 65535) - 32768;
  int32_t hv = j->v * cg >> 14;
  j->x += hv * SIN(j->yaw >> 4) >> 14; j->z += hv * COS(j->yaw >> 4) >> 14; j->y += (j->v * sg >> 14) + w;
  if (j->y > 3200 * 16) j->y = 3200 * 16;
  // guns: two streams from the wing roots, a round every other frame, heat that locks them out
  if (j->cool) j->cool--; if (j->heat) j->heat--;
  if (j->fire && j->alive && !j->cool && j->heat < 200) {
    int32_t f[3]; jet_fwd(j, f); j->cool = 2; j->heat += 4; j->side = -j->side;
    int32_t g[3]; jet_turn(j); b2w(j->side * j->sh.frad * 24, -16, j->sh.flen * 4, g);
    uint32_t h = mixh(frame_n, (uint32_t)me, 15); int32_t sp = 7 - j->skill / 2; if (sp < 2) sp = 2; if (me == 0) sp = 2;
    int32_t mv = j->v + 1700;
    Pt *r = spawn(K_ROUND, g[0], g[1], g[2], (f[0] * mv >> 14) + (int32_t)(h % (2 * sp + 1)) - sp, (f[1] * mv >> 14) + (int32_t)((h >> 8) % (2 * sp + 1)) - sp, (f[2] * mv >> 14), 34);
    if (r) r->owner = (int16_t)me; }
  if (j->mcool) j->mcool--;
  if (j->mfire && j->alive && j->msl > 0 && !j->mcool) {
    int32_t f[3]; jet_fwd(j, f); j->msl--; j->mcool = 60; int32_t g[3]; jet_turn(j); b2w((j->msl & 1 ? 1 : -1) * j->sh.span * 8, -48, 0, g);
    Pt *m = spawn(K_MSL, g[0], g[1], g[2], f[0] * j->v >> 14, (f[1] * j->v >> 14) - 8, f[2] * j->v >> 14, MSL_LIFE);
    if (m) { m->owner = (int16_t)me; m->tgt = (int16_t)j->tgt; m_fired++; } }
  // smoke from a hurt jet, fire from a dying one
  { int32_t hurt = 0; for (int p = 0; p < NPART; p++) hurt += j->L[p];
    if ((!j->alive || hurt > 160) && (frame_n & (j->alive ? 3 : 1)) == 0) { int32_t e[3]; part_at(j, P_ENG, e);
      Pt *s = spawn(K_SMOKE, e[0], e[1], e[2], 0, 2, 0, j->alive ? 50 : 80); if (s) s->size = (int16_t)(j->alive ? 10 : 18);
      if (!j->alive) { s = spawn(K_FIRE, e[0], e[1], e[2], 0, 0, 0, 10); if (s) s->size = 14; } }
    for (int p = P_WL; p <= P_WR; p++) if (j->alive && j->S[p] < 256 && (frame_n & 3) == (uint32_t)(p - P_WL) * 2) { int32_t e[3]; part_at(j, p, e);   // the stump of a lost wingtip trails smoke
      Pt *s = spawn(K_SMOKE, e[0], e[1], e[2], 0, 1, 0, 36); if (s) s->size = (int16_t)(4 + (256 - j->S[p]) / 32); } }
  // the ground
  int32_t gy = (solid(j->x >> 4, j->z >> 4) + 18) * 16;
  if (j->y < gy) { explode(j->x, gy, j->z, 1); scorch(j->x >> 4, j->z >> 4); deposit(j);
    if (j->alive) { j->alive = 0; }
    j->dead_t = 1000; }
  if (j->hitf) j->hitf--;
}

// LESSON-BEGIN 13 of 18: The ground war: flak guns and fuel tanks in every village
// LEDGER  : a target's hull is 120 (gun) or 60 (tank); each round takes 20, held as its LOAD; at the hull it burns.
// LESSON-END
// ================================================================ the ground war: flak guns and fuel tanks in every village
// Ledger: a target's hull is 120 (gun) or 60 (tank); every round that strikes takes 20, held as its LOAD; at the hull it burns.
#define NGT (NVIL * 3 + NBR)
enum { GT_GUN, GT_TANK, GT_BRIDGE };
typedef struct { int32_t x, z, y; int16_t kind, load, alive, cool, burst, down_t, by; } GTarget;
static GTarget GT[NGT]; static int ngt;
static int mis, mis_v, mis_g, mis_n, mis_res; static uint32_t mis_t, mis_ok, mis_fail;

static uint32_t gkills, g_in, g_out, g_me, flak_fired, flak_hits;
static void ground_begin(void) {
  ngt = 0;
  for (int v = 0; v < nvil; v++) for (int k = 0; k < 3; k++) { GTarget *g = &GT[ngt++]; uint32_t h = mixh((uint32_t)v, (uint32_t)k, 71);
    int ci = VIL[v].ci + (k == 0 ? 0 : k == 1 ? 2 : -2), cj = VIL[v].cj + (k == 0 ? 3 : -2 + (int)(h & 1));
    g->x = ci * 256 + 128; g->z = cj * 256 + 128; g->y = ground(g->x, g->z); g->kind = (int16_t)(k == 0 ? GT_GUN : GT_TANK);
    g->load = 0; g->alive = 1; g->cool = (int16_t)(h % 60); g->burst = 0; g->down_t = 0; }
  for (int k = 0; k < nbr; k++) { GTarget *g = &GT[ngt++]; memset(g, 0, sizeof *g); g->x = BRG[k].x; g->z = BRG[k].z; g->y = BRG[k].top; g->kind = GT_BRIDGE; g->alive = 1; }
}
static int32_t wrapw(int32_t d) { return ((d + 16384) & 32767) - 16384; }   // the world repeats every 128 cells: the nearest copy
#define GT_HULL(g) ((g)->kind == GT_GUN ? 120 : (g)->kind == GT_BRIDGE ? 300 : 60)   // a macro: the Arduino builder hoists prototypes above the typedef
static void gt_strike(int32_t x, int32_t y, int32_t z, int dmg, int who) {   // a round (1/16 units) reached the ground here
  for (int k = 0; k < ngt; k++) { GTarget *g = &GT[k]; if (!g->alive) continue;
    int32_t dx = wrapw((x >> 4) - g->x), dz = wrapw((z >> 4) - g->z), dy = (y >> 4) - g->y;
    if (g->kind == GT_BRIDGE) { if (dx > BR_L + 40 || dx < -BR_L - 40 || dz > BR_W + 60 || dz < -BR_W - 60 || dy > 120) continue; }
    else if (dx > 90 || dx < -90 || dz > 90 || dz < -90 || dx * dx + dz * dz > 90 * 90 || dy > 120) continue;   // the box and its splash
    g->load = (int16_t)(g->load + dmg); g_in += (uint32_t)dmg; if (who == 0) g_me += (uint32_t)dmg;
    if (g->load >= GT_HULL(g)) { g->alive = 0; g->down_t = 0; g->by = (int16_t)who; if (who == 0) gkills++; if (g->kind == GT_BRIDGE) br_up[k - (ngt - nbr)] = 0; explode(x - dx * 16, (g->y + 30) * 16, z - dz * 16, 1); scorch(g->x, g->z); }
    return; }
}
static void ground_step(void) {
  const Jet *me = &J[0];
  for (int k = 0; k < ngt; k++) { GTarget *g = &GT[k];
    int32_t dx = wrapw((me->x >> 4) - g->x), dz = wrapw((me->z >> 4) - g->z), gx = (me->x >> 4) - dx, gz = (me->z >> 4) - dz;   // this target's nearest copy
    if (!g->alive) { if ((frame_n & 7) == 0 && dx < 4000 && dx > -4000 && dz < 4000 && dz > -4000) { Pt *s = spawn(K_SMOKE, gx * 16, (g->y + 20) * 16, gz * 16, 0, 6, 0, 90); if (s) s->size = 22; }
      if (++g->down_t > 1800) { g->alive = 1; g_out += (uint32_t)g->load; g->load = 0; if (g->kind == GT_BRIDGE) br_up[k - (ngt - nbr)] = 1; } continue; }
    if (g->kind != GT_GUN || !me->alive || me->dead_t >= 1000) continue;
    if (g->load * 2 >= GT_HULL(g) && (frame_n & 15) == 0) { Pt *s = spawn(K_SMOKE, gx * 16, (g->y + 24) * 16, gz * 16, 0, 5, 0, 60); if (s) s->size = 14; }   // a hurt gun smokes
    int32_t dy = (me->y >> 4) - g->y - 20;
    if (dx > 2800 || dx < -2800 || dz > 2800 || dz < -2800) continue;
    int32_t d = (int32_t)root((uint32_t)(dx * dx + dy * dy + dz * dz)) + 1; if (d > 2800 || dy < 80) continue;
    if (g->cool) { g->cool--; continue; }
    // a burst of four, then a pause; it leads the jet by the shell's time of flight, and misses by a spread that shrinks with wave
    int32_t f[3]; jet_fwd(me, f); int32_t tf = d * 16 / 1400, sp = 160 - (int32_t)(wave > 6 ? 6 : wave) * 15;
    int32_t ax = dx + (f[0] * me->v >> 14) * tf / 16, ay = dy + (f[1] * me->v >> 14) * tf / 16 + tf * tf / 40, az = dz + (f[2] * me->v >> 14) * tf / 16;
    int32_t al = (int32_t)root((uint32_t)(ax * ax + ay * ay + az * az)) + 1; uint32_t h = mixh(frame_n, (uint32_t)k, 73);
    Pt *r = spawn(K_ROUND, gx * 16, (g->y + 30) * 16, gz * 16, ax * 1400 / al + (int32_t)(h % (2 * sp + 1)) - sp, ay * 1400 / al + (int32_t)((h >> 8) % (2 * sp + 1)) - sp, az * 1400 / al + (int32_t)((h >> 16) % (2 * sp + 1)) - sp, 60);
    if (r) { r->owner = 60; r->spin = (int16_t)(tf - 2 + (int32_t)((h >> 24) % 5)); flak_fired++; }
    if (++g->burst >= 4) { g->burst = 0; g->cool = (int16_t)(40 + (int32_t)(h % 30)); } else g->cool = 3; }
}

// LESSON-BEGIN 14 of 18: The pilots: chase a lead point, pull out of the ground, break when close
// WHAT    : a bandit hunts in bursts and cruises the canyon between them, so it can be caught. Nobody hunts the fallen.
// LESSON-END
// ================================================================ the pilots: chase a lead point, pull out of the ground, break when close
static int32_t dbg_ye, dbg_ge, dbg_rng;
static int32_t wrap4k(int32_t a) { return ((a + 2048) & 4095) - 2048; }
static int threat_on(int i);
static void pilot(int me, Jet *j) {
  j->jx = j->jy = 0; j->fire = j->mfire = 0; j->ab = 0; j->rud = j->trim = 0; if (!j->alive) return;
  const Jet *t = &J[j->tgt];
  if (me && !j->raid && j->ext && j->brk) j->ext = 0;
  // a bandit hunts in bursts and cruises the canyon between them (weaving), so it can be caught; nobody hunts the fallen
  int hunt = t->alive && (me == 0 || ((frame_n + (uint32_t)j->seed) % 600) < (uint32_t)(220 + j->skill * 30));
  int32_t dx, dy, dz;
  int strafe = -1;                                         // the demo pilot spends every other 20 s on the ground war: the nearest gun or tank
  if (me && j->raid) { int32_t bd = 1 << 30; for (int k = mis_v * 3; k < mis_v * 3 + 3; k++) if (GT[k].alive) { int32_t ex = wrapw(GT[k].x - (j->x >> 4)), ez = wrapw(GT[k].z - (j->z >> 4)), m = (ex < 0 ? -ex : ex) + (ez < 0 ? -ez : ez); if (m < bd) { bd = m; strafe = k; } } }
  else if (me == 0 && mis == 1) strafe = mis_g;                    // the demo pilot flies the mission: the bridge
  else if (me == 0 && mis == 2) { for (int i = 1; i <= 2; i++) if (J[i].raid && J[i].alive) { j->tgt = i; t = &J[i]; hunt = 1; } }
  else if (me == 0 && ((frame_n / 1200) & 1)) { int32_t bd = 5000; for (int k = 0; k < ngt; k++) if (GT[k].alive) { int32_t ex = wrapw(GT[k].x - (j->x >> 4)), ez = wrapw(GT[k].z - (j->z >> 4)), m = (ex < 0 ? -ex : ex) + (ez < 0 ? -ez : ez); if (m < bd) { bd = m; strafe = k; } } }
  int32_t ext = j->ext;                                    // a strafing run: dive in, fire, pass over, then extend before turning back
  if (strafe >= 0) { hunt = 0; dx = wrapw(GT[strafe].x - (j->x >> 4)); dz = wrapw(GT[strafe].z - (j->z >> 4)); dy = GT[strafe].y + 30 - (j->y >> 4);
    int32_t m = (dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz), a = wrap4k(full_angle(dz, dx) - (j->yaw >> 4)); if (a < 0) a = -a;
    if (!ext && (m < 300 || (m < 1100 && a > 420))) ext = 60;        // over it, or inside the turn that would bring the nose on: run out and come back
    if (GT[strafe].kind == GT_BRIDGE && !ext) { int32_t ax = dx < 0 ? -dx : dx; if (ax < BR_L) dx = 0; else dx += dx > 0 ? -BR_L + 200 : BR_L - 200; m = (dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz); }   // a bridge: aim at its nearest stretch
    if (ext) { ext--; int32_t f0[3]; jet_fwd(j, f0); dx = f0[0] * 3000 >> 14; dz = f0[2] * 3000 >> 14; dy = solid((j->x >> 4) + dx, (j->z >> 4) + dz) + 700 - (j->y >> 4); }
    j->ext = ext; }
  else if (hunt) { dx = (t->x - j->x) >> 4; dy = (t->y - j->y) >> 4; dz = (t->z - j->z) >> 4; }
  else { int32_t az = (j->z >> 4) + 3000, ax = path_x((az >> 8) & 127) * 256 + 128 + (SIN((int)((frame_n * 12u + (uint32_t)j->seed) & 4095)) * 500 >> 14);
    dx = ax - (j->x >> 4); dz = 3000; dy = ground(ax, az) + 650 - (j->y >> 4); }
  uint32_t r2 = (uint32_t)(dx * dx + dz * dz); int32_t hd = (int32_t)root(r2), rng = (int32_t)root(r2 + (uint32_t)(dy * dy)) ;
  int32_t tf[3]; jet_fwd(t, tf); int32_t lt = hunt ? rng * 16 / (j->v + 1700) : 0;   // frames for a round to get there
  int32_t lx = dx + (tf[0] * t->v >> 14) * lt / 16, ly = dy + (tf[1] * t->v >> 14) * lt / 16, lz = dz + (tf[2] * t->v >> 14) * lt / 16;
  int32_t lh = (int32_t)root((uint32_t)(lx * lx + lz * lz));
  int32_t ye = wrap4k(full_angle(lz, lx) - (j->yaw >> 4)), ge = wrap4k(full_angle(lh, ly) - (j->gam >> 4)); (void)hd;
  int32_t want = ye * 3; if (want > 900) want = 900; if (want < -900) want = -900;
  if (me && !j->raid) {                                    // ACE 5 K11: the bandits fight for energy
    const Jet *P = &J[0]; int32_t pf[3]; jet_fwd(P, pf); int32_t ex = (j->x - P->x) >> 4, ey = (j->y - P->y) >> 4, ez = (j->z - P->z) >> 4;
    int32_t pr = (int32_t)root((uint32_t)(ex * ex + ey * ey + ez * ez)) + 1, cosa = (int32_t)(((int64_t)pf[0] * ex + (int64_t)pf[1] * ey + (int64_t)pf[2] * ez) / pr);   // grain 16384
    if (!j->brk && threat_on(me) && j->skill >= 2) { j->brk = 36; j->side = -j->side; }   // a missile on me (from the second wave): break across it
    if (P->alive && !j->brk && !j->ext && pr < 1900 && cosa > 15400 && (frame_n + (uint32_t)me * 13) % 16 < (uint32_t)(4 + j->skill)) {   // a gun on me: break INTO him, hard
      int32_t f0[3]; jet_fwd(j, f0); int64_t cr = (int64_t)f0[2] * ex - (int64_t)f0[0] * ez; j->side = cr > 0 ? -1 : 1; j->brk = 30 + (j->skill > 6 ? 6 : j->skill) * 4; }
    if (!j->brk && !j->ext && hunt && rng < 450 && (ye > 1300 || ye < -1300)) j->ext = 70;   // overshot: extend, then come back with speed
    if (j->ext) { j->ext--; want = 0; ye = 0; ge = wrap4k(full_angle(1000, -40) - (j->gam >> 4)); } }
  if (j->brk) { j->brk--; want = j->side * 900; }
  else if (rng < 220) j->brk = 40;
  int32_t f[3]; jet_fwd(j, f); int32_t ah = 1 << 20;            // look along the path a second ahead, at three points: where will the ground be?
  for (int n = 1; n <= (strafe >= 0 ? 1 : 3); n++) { int32_t dd = j->v * 10 * n >> 4, py = (j->y >> 4) + (f[1] * dd >> 14);
    int32_t a = py - 18 - solid((j->x >> 4) + (f[0] * dd >> 14), (j->z >> 4) + (f[2] * dd >> 14)); if (a < ah) ah = a; }   // read where the crash test reads (18 under the centre)
  { int32_t lo = (j->y >> 4) - 18 - solid(j->x >> 4, j->z >> 4); if (lo < ah) ah = lo; }
  int pull = ah < (strafe >= 0 ? 170 : 300 + (me ? 0 : 100));   // a strafing run looks half a second ahead and pulls late
  if (pull) want = 0;
  int32_t be = wrap4k(want - (j->bank >> 4)); int jx = be / 3; jx = jx > 127 ? 127 : jx < -127 ? -127 : jx; j->jx = jx;
  int32_t ay = ye < 0 ? -ye : ye; int jy = -(int)(ge * 2 + ay / 2); if (jy < -127) jy = -127; if (jy > 60) jy = 60;
  if (j->brk) jy = -127; if (pull) jy = -127;
  if (me) { jy = jy * (6 + (j->skill > 6 ? 6 : j->skill)) / 12; }                // the bandits pull less hard in the early waves
  if (me && !pull && !j->brk && j->v < 620 && jy < -40) jy = -40;                    // slow: unload, buy back speed before pulling again
  j->jy = jy;
  j->ab = strafe >= 0 ? (j->v < 700 || pull) : (hunt && rng > 1200) || j->v < 750 || pull || j->ext || j->brk || (me == 0 && (j->bank >> 4 > 500 || j->bank >> 4 < -500));
  if ((hunt || strafe >= 0) && ye < 260 && ye > -260 && ge < 260 && ge > -260) { int lim = me ? 2 + (j->skill > 6 ? 6 : j->skill) / 2 : 8;   // the last degrees by rudder and trim
    j->rud = ye / 3; j->trim = ge / 3; if (j->rud > lim) j->rud = lim; if (j->rud < -lim) j->rud = -lim; if (j->trim > lim) j->trim = lim; if (j->trim < -lim) j->trim = -lim; }
  if (!me) { dbg_ye = ye; dbg_ge = ge; dbg_rng = rng; }
  int cone = me ? 50 : 90, on = ye < cone && ye > -cone && ge < cone && ge > -cone;
  int tight = ye < 24 && ye > -24 && ge < 24 && ge > -24;
  j->fire = (strafe >= 0 && on && rng < 1600) || ((hunt && (me ? on : tight) && rng < (me ? 2400 : 1700)) && (me == 0 || (frame_n + (uint32_t)me * 37) % 90 < (uint32_t)(20 + j->skill * 6)));
  if (hunt && on && rng < 4000) j->lockt++; else j->lockt = 0;
  j->mfire = j->lockt > 24 && rng > 700 && (me == 0 || j->skill >= 2) && (frame_n % 240) < 3;
}

// LESSON-BEGIN 15 of 18: THE MISSILE: the bearing's rate, not the bearing
// LAW     : each frame the line of sight to the target is read at grain 1024 (lx, ly, lz = d_xyz * 1024 / d). Its turn since
//           last frame is (lx - p->lx, ...). The missile's rate is pushed by PN_N = 4 times that turn times its speed, limited
//           by the fins (MSL_FIN). A line of sight that does not turn means the two paths meet: no correction is needed.
// RELATION: the law reads a rate of a relation (the bearing between two moving things), never where the target is.
//           PURE_PURSUIT (ACE 3) reads the bearing itself and lags behind a crossing target.
// CUT     : the turn is a difference of two frames' readings at grain 1024. A noisier sensor would need the turn taken over
//           several readings (METHOD 8 step 9: an approach rate as a reading over ten readings).
// KNOWN   : this law is proportional navigation, in guidance since the 1950s. What this sketch adds is the law in whole
//           numbers, beside its pursuit control, on a $2 chip.
// SEEKER  : each frame it takes the hottest reading in its 37-degree cone: heat x 2048 / (2048 + d). A jet glows 700 (1500
//           on afterburner); a flare starts at 2400 and cools by a sixteenth a frame, so for about 12 frames a fresh flare
//           outshines the jet behind it. The seeker is a reading, so it can be stolen.
// TRY     : build the emulator image twice, with and without -DPURE_PURSUIT, same FRAMES. Predict the MISSILES line:
//           more hits under the rate law. Run both and log the two lines (practice/p10).
// LESSON-END
// ================================================================ the particles: rounds hit parts, missiles steer, pieces fall and become ground
static int seg_hit(const Pt *p, const int32_t *a, const int32_t *w, int32_t r) {   // does the step a -> p pass within r (1/16 units) of w?
  int64_t sx = p->x - a[0], sy = p->y - a[1], sz = p->z - a[2], wx = w[0] - a[0], wy = w[1] - a[1], wz = w[2] - a[2];
  int64_t ss = sx * sx + sy * sy + sz * sz, t = ss ? (wx * sx + wy * sy + wz * sz) * 1024 / ss : 0; if (t < 0) t = 0; if (t > 1024) t = 1024;
  int64_t cx = wx - sx * t / 1024, cy = wy - sy * t / 1024, cz = wz - sz * t / 1024;
  return cx * cx + cy * cy + cz * cz < (int64_t)r * r;
}
// ---- the seeker and the flares
#ifndef PN_N
#define PN_N 4                    // the heading turns this many times the line of sight's turn
#endif
static int track_at(int id, int32_t *w) {                  // where the tracked thing is (1/16 units); 0 if it is gone
  if (id < NJ) { const Jet *j = &J[id]; if (j->dead_t >= 1000) return 0; w[0] = j->x; w[1] = j->y; w[2] = j->z; return 1; }
  if (id - 100 >= NPT) return 0; const Pt *f = &PT[id - 100]; if (f->kind != K_FLARE) return 0; w[0] = f->x; w[1] = f->y; w[2] = f->z; return 1;
}
// each frame the seeker takes the hottest reading in its cone (37 degrees): a jet glows 700 (1500 on afterburner), a
// flare starts at 2400 and cools by a sixteenth a frame, so for about 12 frames a fresh flare outshines the jet behind it.
// The reading falls with distance: heat x 2048 / (2048 + d).
static void seek(Pt *p, int self) {
  (void)self;
  int32_t vl = (int32_t)root((uint32_t)((p->vx >> 2) * (p->vx >> 2) + (p->vy >> 2) * (p->vy >> 2) + (p->vz >> 2) * (p->vz >> 2))) * 4 + 1;
  int32_t best = 0, id = -1;
  for (int n = 0; n < NJ + NPT; n++) { int32_t heat, w[3];
    if (n < NJ) { const Jet *j = &J[n]; if (n == p->owner || !j->alive) continue; heat = j->ab ? 1500 : 700; w[0] = j->x; w[1] = j->y; w[2] = j->z; }
    else { const Pt *f = &PT[n - NJ]; if (f->kind != K_FLARE) continue; heat = f->mass; w[0] = f->x; w[1] = f->y; w[2] = f->z; }
    int32_t dx = (w[0] - p->x) >> 4, dy = (w[1] - p->y) >> 4, dz = (w[2] - p->z) >> 4;
    if (dx > 6000 || dx < -6000 || dy > 6000 || dy < -6000 || dz > 6000 || dz < -6000) continue;
    int32_t d = (int32_t)root((uint32_t)(dx * dx + dy * dy + dz * dz)) + 1;
    int64_t dot = (int64_t)dx * p->vx + (int64_t)dy * p->vy + (int64_t)dz * p->vz;
    if (dot * 16384 <= (int64_t)d * vl * 13100) continue;  // outside the cone
    int32_t rd = heat * 2048 / (2048 + d); if (rd > best) { best = rd; id = n < NJ ? n : 100 + (n - NJ); } }
  if (id >= 0 && id != p->tgt) { p->tgt = (int16_t)id; p->lx = p->ly = p->lz = 0; }   // a new reading: the rate starts again
}
static void drop_flare(Jet *j) {
  if (!j->alive || j->flr <= 0 || j->fcool) return;
  int32_t f[3]; jet_fwd(j, f); j->flr--; j->fcool = 12; flares_out++;
  for (int s2 = -1; s2 <= 1; s2 += 2) { Pt *q = spawn(K_FLARE, j->x - (f[0] * 30 * 16 >> 14), j->y - 32, j->z - (f[2] * 30 * 16 >> 14),
      (f[0] * j->v >> 14) / 2 + s2 * (f[2] * 60 >> 14), 10, (f[2] * j->v >> 14) / 2 - s2 * (f[0] * 60 >> 14), 70);
    if (q) { q->mass = 2400; q->owner = (int16_t)(j - J); q->size = 6; } }
}
static int threat_on(int i) {                              // a missile reads this jet and is within 2,400 units
  for (int k = 0; k < NPT; k++) { const Pt *p = &PT[k]; if (p->kind != K_MSL || p->tgt != i) continue;
    int32_t dx = (J[i].x - p->x) >> 4, dz = (J[i].z - p->z) >> 4; if (dx < 2400 && dx > -2400 && dz < 2400 && dz > -2400) return 1; }
  return 0;
}
static void particles_step(void) {
  for (int k = 0; k < NPT; k++) { Pt *p = &PT[k]; if (!p->kind) continue; p->age++;
    int32_t a[3] = {p->x, p->y, p->z};
    switch (p->kind) {
    case K_ROUND: p->x += p->vx; p->y += p->vy; p->z += p->vz; p->vy -= 2;
      if (p->owner == 60 && p->age >= p->spin) {             // flak: the fuse runs out; a black burst; within 150 units the splinters strike
        Pt *s = spawn(K_SMOKE, p->x, p->y, p->z, 0, 0, 0, 50); if (s) { s->size = 30; s->mat = 1; }
        const Jet *m0 = &J[0]; int32_t dx = (m0->x - p->x) >> 4, dy = (m0->y - p->y) >> 4, dz = (m0->z - p->z) >> 4;
        if (m0->alive && dx * dx + dy * dy + dz * dz < 150 * 150) { uint32_t h = mixh(frame_n, (uint32_t)k, 74); int q = (int)(h % NPART); if (J[0].S[q]) { hit(&J[0], q, 9); flak_hits++; } }
        p->kind = K_NONE; break; }
      { int hit3 = 0; for (int q = 1; q <= 3 && !hit3; q++) { int32_t qx = a[0] + (p->x - a[0]) * q / 3, qy = a[1] + (p->y - a[1]) * q / 3, qz = a[2] + (p->z - a[2]) * q / 3;   // K12: a round steps 180 units a frame: test
          if (qy < solid(qx >> 4, qz >> 4) * 16) { p->x = qx; p->y = qy; p->z = qz; hit3 = 1; } }                                                            // thirds of the step, or it passes through a deck or a roof
        if (hit3) { p->y = solid(p->x >> 4, p->z >> 4) * 16 - 16; } }
      if (p->y < solid(p->x >> 4, p->z >> 4) * 16) { if (p->owner != 60) gt_strike(p->x, p->y, p->z, 20, p->owner); Pt *s = spawn(K_SPARK, p->x, p->y + 64, p->z, 0, 3, 0, 5); if (s) s->size = 8; p->kind = K_NONE; break; }
      for (int i = 0; i < NJ; i++) { Jet *j = &J[i]; if (i == p->owner || j->dead_t >= 1000) continue;
        int32_t dx = (j->x - p->x) >> 4, dz = (j->z - p->z) >> 4; if (dx > 200 || dx < -200 || dz > 200 || dz < -200) continue;
        for (int q = 0; q < NPART; q++) { if (!j->S[q]) continue; int32_t w[3]; part_at(j, q, w);
          if (seg_hit(p, a, w, part_r(j, q) + (p->owner == 0 ? 160 : 0))) { hit(j, q, 26); Pt *s = spawn(K_SPARK, p->x, p->y, p->z, 0, 4, 0, 6); if (s) s->size = 9;
            s = spawn(K_FIRE, p->x, p->y, p->z, 0, 0, 0, 3); if (s) s->size = 7; p = &PT[k]; p->kind = K_NONE; break; } }
        if (!p->kind) break; }
      break;
    case K_MSL: { int32_t sp = 1500 + (p->age < 30 ? p->age * 10 : 300);
      seek(p, k);
      int32_t T[3], have = track_at(p->tgt, T);
      if (have) { int32_t dx = (T[0] - p->x) >> 4, dy = (T[1] - p->y) >> 4, dz = (T[2] - p->z) >> 4;
        int32_t d = (int32_t)root((uint32_t)(dx * dx + dy * dy + dz * dz)) + 1;
        if (d < 70) { if (p->tgt < NJ) { for (int q = 0; q < NPART; q++) if (J[p->tgt].S[q]) hit(&J[p->tgt], q, MSL_DMG); m_hit++; }
          else { PT[p->tgt - 100].kind = K_NONE; m_fooled++; }
          explode(p->x, p->y, p->z, 0); p->kind = K_NONE; break; }
#ifdef PURE_PURSUIT
        p->vx += (dx * sp / d - p->vx) / 7; p->vy += (dy * sp / d - p->vy) / 7; p->vz += (dz * sp / d - p->vz) / 7;   // ACE 3: lean toward the bearing
#else
        // the line of sight, grain 1024, and how far it turned since the last frame: the heading turns N times that rate
        int32_t lx = dx * 1024 / d, ly = dy * 1024 / d, lz = dz * 1024 / d;
        if (!p->lx && !p->ly && !p->lz) { p->lx = (int16_t)lx; p->ly = (int16_t)ly; p->lz = (int16_t)lz; }
        int32_t ax = PN_N * sp * (lx - p->lx) / 1024, ay = PN_N * sp * (ly - p->ly) / 1024, az = PN_N * sp * (lz - p->lz) / 1024;
        p->lx = (int16_t)lx; p->ly = (int16_t)ly; p->lz = (int16_t)lz;
        if (ax > MSL_FIN) ax = MSL_FIN; if (ax < -MSL_FIN) ax = -MSL_FIN; if (ay > MSL_FIN) ay = MSL_FIN; if (ay < -MSL_FIN) ay = -MSL_FIN; if (az > MSL_FIN) az = MSL_FIN; if (az < -MSL_FIN) az = -MSL_FIN;   // the fins' limit
        p->vx += ax + (lx * sp / 1024 - p->vx) / 40; p->vy += ay + (ly * sp / 1024 - p->vy) / 40; p->vz += az + (lz * sp / 1024 - p->vz) / 40;
        { int32_t vl = (int32_t)root((uint32_t)((p->vx >> 2) * (p->vx >> 2) + (p->vy >> 2) * (p->vy >> 2) + (p->vz >> 2) * (p->vz >> 2))) * 4 + 1;   // hold the speed
          p->vx = p->vx * sp / vl; p->vy = p->vy * sp / vl; p->vz = p->vz * sp / vl; }
#endif
      }
      p->x += p->vx; p->y += p->vy; p->z += p->vz;
      if ((p->age & 1) == 0) { Pt *s = spawn(K_SMOKE, p->x, p->y, p->z, 0, 1, 0, 60); if (s) s->size = 7; p = &PT[k]; }
      if (p->y < solid(p->x >> 4, p->z >> 4) * 16) { gt_strike(p->x, p->y, p->z, 120, p->owner); explode(p->x, p->y, p->z, 0); p->kind = K_NONE; m_spent++; }
      break; }
    case K_FLARE: p->mass = p->mass * 15 / 16; p->vy -= 3; p->vx -= p->vx / 16; p->vz -= p->vz / 16; p->x += p->vx; p->y += p->vy; p->z += p->vz;   // it cools at a sixteenth a frame
      if ((p->age & 3) == 0) { Pt *s = spawn(K_SMOKE, p->x, p->y, p->z, 0, 1, 0, 30); if (s) s->size = 5; p = &PT[k]; }
      break;
    case K_SMOKE: p->x += p->vx + 6; p->y += p->vy; p->z += p->vz; p->vx -= p->vx / 8; p->vz -= p->vz / 8; break;   // the wind takes it
    case K_FIRE: case K_SPARK: p->x += p->vx; p->y += p->vy; p->z += p->vz; p->vx -= p->vx / 6; p->vz -= p->vz / 6; break;
    case K_DEBRIS: p->vy -= 9; p->vx -= p->vx / 32; p->vz -= p->vz / 32; p->x += p->vx; p->y += p->vy; p->z += p->vz; p->spin = (int16_t)(p->spin + 140);
      if ((p->age & 3) == 0) { Pt *s = spawn(K_SMOKE, p->x, p->y, p->z, 0, 2, 0, 30); if (s) s->size = 6; p = &PT[k]; }
      if (p->y < ground(p->x >> 4, p->z >> 4) * 16) { dmg_ground += (uint32_t)p->mass; p->kind = K_NONE; Pt *s = spawn(K_FIRE, p->x, p->y + 160, p->z, 0, 2, 0, 14); if (s) s->size = 12; }
      break; }
    if (p->kind && p->age >= p->life) { if (p->kind == K_DEBRIS) dmg_ground += (uint32_t)p->mass; if (p->kind == K_MSL) { explode(p->x, p->y, p->z, 0); m_spent++; } p->kind = K_NONE; } }
  uint32_t sum = dmg_ground; for (int i = 0; i < NJ; i++) for (int q = 0; q < NPART; q++) sum += (uint32_t)J[i].L[q];
  for (int k = 0; k < NPT; k++) if (PT[k].kind == K_DEBRIS) sum += (uint32_t)PT[k].mass;
  if (sum != dmg_in) dmg_bad++;
}

// LESSON-BEGIN 16 of 18: Growing the picture: jets, particles, clouds, then the land
// WHAT    : the fuselage as rings along a spindle; camouflage from the seed; billboards for puffs. The far land is laid in
//           2 x 2 blocks, and a frame that read heavy lays the next a grain coarser (lod_of).
// RELATION: detail is read against the last frame's cost: a relation, not a fixed setting. The cut moves with the load.
// LESSON-END
// ================================================================ growing the picture: jets, particles, clouds, then the land

static void tri16(const int32_t *a, const int32_t *b, const int32_t *c, int mat, int li) {
  CV p[3]; eye16(a, &p[0]); eye16(b, &p[1]); eye16(c, &p[2]);
  p[0].u = 0; p[1].u = 511; p[2].u = 511; p[0].v = p[1].v = 0; p[2].v = 511; p[0].l = p[1].l = p[2].l = li << 8;
  tri(&p[0], &p[1], &p[2], mat, fog16(p[0].z), 0);
}
static void q16(const int32_t *a, const int32_t *b, const int32_t *c, const int32_t *d, int mat) { quad16(a, b, c, d, mat, LITM(mat) ? 15 : lit3(a, b, d), 0, 511); }
static int lod_of(const Jet *j) { CV c; int32_t w[3] = {j->x, j->y, j->z}; eye16(w, &c); if (c.z < 16 * 40) return 2; int32_t px = (int32_t)((int64_t)FOC * j->sh.span * 16 / c.z); return px > 34 ? 2 : px > 10 ? 1 : 0; }
static void grow_jet(int i) {
  Jet *j = &J[i]; if (j->dead_t >= 1000) return; const Shape *s = &j->sh;
  int lod = lod_of(j); jet_turn(j);
  int32_t L = s->flen * 16, R = s->frad * 16;
  // the fuselage: rings along a spindle; camouflage from the seed, a red band on a bandit's nose
  { static const int16_t RZ[7] = {50, 36, 20, 0, -20, -38, -50}, RRr[7] = {0, 55, 90, 100, 105, 85, 60};
    static const uint8_t PICK[3][7] = {{0, 2, 6, 0, 0, 0, 0}, {0, 1, 3, 5, 6, 0, 0}, {0, 1, 2, 3, 4, 5, 6}};
    const int NR = lod == 0 ? 3 : lod == 1 ? 5 : 7, NS = lod == 0 ? 3 : lod == 1 ? 5 : 8;
    int32_t P[7][8][3];
    for (int r = 0; r < NR; r++) { int rr = PICK[lod][r]; int32_t z = L * RZ[rr] / 100, rad = R * RRr[rr] / 100;
      for (int k = 0; k < NS; k++) { int a = k * 4096 / NS + 1024; int32_t xs = rr == 4 ? 13 : 10;
        b2w((COS(a) * rad >> 14) * xs / 10, SIN(a) * rad >> 14, z, P[r][k]); } }
    for (int r = 0; r + 1 < NR; r++) for (int k = 0; k < NS; k++) { int k1 = (k + 1) % NS;
      int m = (mixh((uint32_t)j->seed, (uint32_t)(r * 16 + k), 3) & 1) ? s->skin : s->skin2;
      if (i && PICK[lod][r] == 1) m = M_RED;
      if (r == 0) tri16(P[0][k], P[1][k], P[1][k1], m, lit3(P[0][k], P[1][k], P[1][k1]));
      else q16(P[r][k], P[r][k1], P[r + 1][k1], P[r + 1][k], m); }
    // the nozzle: dark, or white-hot on afterburner, with a flame that flickers
    { int r = NR - 1; int32_t c[3]; b2w(0, 0, -L / 2 - 16, c);
      for (int k = 0; k < NS; k++) tri16(P[r][k], P[r][(k + 1) % NS], c, j->ab && j->alive ? M_GLOW : M_DARK, 6);
      if (j->ab && j->alive && lod) { int32_t f[3], e0[3], e1[3]; int32_t fl = 10 + (int32_t)(mixh(frame_n, (uint32_t)i, 8) % 8);
        b2w(0, 0, -L / 2 - fl * 16, f); b2w(-R * 2 / 3, 0, -L / 2, e0); b2w(R * 2 / 3, 0, -L / 2, e1); tri16(e0, e1, f, M_GLOW, 15);
        b2w(0, -R * 2 / 3, -L / 2, e0); b2w(0, R * 2 / 3, -L / 2, e1); tri16(e0, e1, f, M_GLOW, 15); } } }
  // the canopy
  if (lod) { int32_t f[3], b[3], t[3], l[3], r_[3]; b2w(0, R * 6 / 10, L * 30 / 100, f); b2w(0, R * 7 / 10, L * 2 / 100, b); b2w(0, R * 17 / 10, L * 15 / 100, t);
    b2w(-R * 6 / 10, R * 7 / 10, L * 15 / 100, l); b2w(R * 6 / 10, R * 7 / 10, L * 15 / 100, r_);
    tri16(f, l, t, M_GLASS, lit3(f, l, t)); tri16(f, t, r_, M_GLASS, lit3(f, t, r_)); tri16(b, t, l, M_GLASS, 9); tri16(b, r_, t, M_GLASS, 9); }
  // the wings: root to tip in two sections that bend with the load; ailerons follow the stick; a torn wing ends ragged
  for (int sd = -1; sd <= 1; sd += 2) { int p = sd < 0 ? P_WL : P_WR; if (!j->S[p]) continue;
    int32_t span = s->span * 16 * j->S[p] / 256, sw = SIN(s->sweep) * span / COS(s->sweep);   // the leading edge's run back to the tip
    int32_t le0 = L / 10, tipc = s->tipc * 16 + (s->chord * 16 - s->tipc * 16) * (256 - j->S[p]) / 256;
    int32_t ld = j->load - 256; if (ld > 768) ld = 768; if (ld < -512) ld = -512;
    int32_t bend = ld * span / 6000 + s->dihed * span / 60;
    int32_t rag = j->S[p] < 256 ? (int32_t)(mixh((uint32_t)j->seed, (uint32_t)p, (uint32_t)j->S[p]) % 64) - 32 : 0;
    int32_t A[3][3], B[3][3];
    for (int q = 0; q <= 2; q += (lod == 2 ? 1 : 2)) { int32_t x = R + span * q / 2, le = le0 - sw * q / 2, ch = s->chord * 16 + (tipc - s->chord * 16) * q / 2;
      int32_t y = bend * q * q / 4;
      b2w(sd * x, y + (q == 2 ? rag : 0), le, A[q]); b2w(sd * x, y, le - ch + (q == 2 ? rag : 0), B[q]); }
    int m = (mixh((uint32_t)j->seed, (uint32_t)p, 5) & 1) ? s->skin : s->skin2;
    if (lod == 2) { q16(A[0], A[1], B[1], B[0], m);
      // the outer section: main wing and the aileron behind it, turned by the stick
      int32_t c1[3], c2[3], a1[3], a2[3]; int32_t x1 = R + span / 2, x2 = R + span, le1 = le0 - sw / 2, le2 = le0 - sw;
      int32_t ch1 = s->chord * 16 + (tipc - s->chord * 16) / 2, y1 = bend / 4, y2 = bend, dfl = -sd * j->jx / 3;
      b2w(sd * x1, y1, le1 - ch1 * 3 / 4, c1); b2w(sd * x2, y2 + rag, le2 - tipc * 3 / 4, c2);
      b2w(sd * x1, y1 + dfl, le1 - ch1, a1); b2w(sd * x2, y2 + dfl, le2 - tipc + rag, a2);
      q16(A[1], A[2], c2, c1, m); q16(c1, c2, a2, a1, M_DARK);
      if (i == 0 && j->msl > (sd < 0 ? 0 : 1)) { int32_t m0[3], m1[3], m2[3];   // a missile under the wing, while it is still there
        b2w(sd * (R + span / 3), -40, le0 + 40, m0); b2w(sd * (R + span / 3) - 12, -56, le0 - 260, m1); b2w(sd * (R + span / 3) + 12, -56, le0 - 260, m2); tri16(m0, m1, m2, M_SKIN0, 12); } }
    else q16(A[0], A[2], B[2], B[0], m); }
  // the tailplanes (elevators follow the stick) and one fin or two
  for (int sd = -1; sd <= 1; sd += 2) { int p = sd < 0 ? P_TL : P_TR; if (!j->S[p]) continue;
    int32_t ts = s->tspan * 16 * j->S[p] / 256, el = j->jy / 3, a[3], b[3], c[3], d[3];
    b2w(sd * R, 0, -L * 34 / 100, a); b2w(sd * (R + ts), 0, -L * 46 / 100, b); b2w(sd * (R + ts), el, -L * 52 / 100, c); b2w(sd * R, el, -L * 50 / 100, d);
    q16(a, b, c, d, s->skin); }
  if (j->S[P_FIN]) for (int f = 0; f <= s->twin; f++) { int32_t fx = s->twin ? (f ? R * 7 / 10 : -R * 7 / 10) : 0, ct = s->twin ? (f ? 40 : -40) : 0, h = s->fin * 16 * j->S[P_FIN] / 256;
    int32_t a[3], b[3], c[3], d[3]; b2w(fx, R / 2, -L * 28 / 100, a); b2w(fx + ct * h / 256, R / 2 + h, -L * 44 / 100, b); b2w(fx + ct * h / 256, R / 2 + h, -L * 50 / 100, c); b2w(fx, R / 2, -L * 50 / 100, d);
    q16(a, b, c, d, i ? (int)M_RED : (int)s->skin2); }
}
static int32_t puff_area;
static void billboard(int32_t x, int32_t y, int32_t z, int32_t size, int mat, int li) {   // a puff that always faces the eye (world 1/16 units; size in units)
  CV c; int32_t w[3] = {x, y, z}; eye16(w, &c); if (c.z < 16 * NEAR + size * 16) return;
  CV p[4]; int32_t s16 = size * 16;
  // K5: a puff is cut-out (no thin, haze or paired fast path), so its cost is its area. Up close (a crash under the eye) a few
  // puffs filled the glass five times over: 13.6 M cycles of drawing. A puff is now at most 32 dots across its half, and the
  // frame's puffs share a budget of 28,000 dots (a third of the glass); past it they are left out.
  { int32_t px = (int32_t)((int64_t)s16 * FOC / c.z); if (px > 32) { s16 = 32 * c.z / FOC; px = 32; } if (puff_area + px * px * 4 > 28000) return; puff_area += px * px * 4; }
  for (int q = 0; q < 4; q++) { p[q] = c; p[q].x += (q == 1 || q == 2) ? s16 : -s16; p[q].y += q >= 2 ? -s16 : s16; p[q].u = (q == 1 || q == 2) ? 511 : 0; p[q].v = q >= 2 ? 511 : 0; p[q].l = li << 8; }
  int fog = fog16(c.z); tri(&p[0], &p[1], &p[2], mat, fog, 0); tri(&p[0], &p[2], &p[3], mat, fog, 0);
}
static void grow_particles(void) {
  puff_area = 0;
  for (int k = 0; k < NPT; k++) { const Pt *p = &PT[k]; if (!p->kind) continue;
    switch (p->kind) {
    case K_ROUND: case K_MSL: { int32_t h[3] = {p->x, p->y, p->z}, t[3] = {p->x - p->vx * (p->kind == K_ROUND ? 2 : 1) / 2, p->y - p->vy, p->z - p->vz * (p->kind == K_ROUND ? 2 : 1) / 2};
      CV a, b; eye16(h, &a); eye16(t, &b); if (a.z < 16 * NEAR || b.z < 16 * NEAR) break;
      int32_t dx = a.x * 64 / (a.z / 16 + 1) - b.x * 64 / (b.z / 16 + 1), dy = a.y * 64 / (a.z / 16 + 1) - b.y * 64 / (b.z / 16 + 1);   // the streak's way across the glass
      int32_t n = (int32_t)root((uint32_t)(dx * dx + dy * dy)) + 1, wd = p->kind == K_ROUND ? 22 : 30;
      CV q[4] = {a, a, b, b}; q[0].x -= dy * wd / n; q[0].y += dx * wd / n; q[1].x += dy * wd / n; q[1].y -= dx * wd / n;
      q[2].x += dy * wd / n; q[2].y -= dx * wd / n; q[3].x -= dy * wd / n; q[3].y += dx * wd / n;
      for (int c = 0; c < 4; c++) { q[c].u = c == 1 || c == 2 ? 511 : 0; q[c].v = c >= 2 ? 511 : 0; q[c].l = 15 << 8; }
      int m = p->kind == K_ROUND ? M_TRACER : M_SKIN0; tri(&q[0], &q[1], &q[2], m, 0, 0); tri(&q[0], &q[2], &q[3], m, 0, 0);
      if (p->kind == K_MSL) billboard(p->x - p->vx / 2, p->y - p->vy / 2, p->z - p->vz / 2, 5, M_FIRE, 15);
      break; }
    case K_SMOKE: billboard(p->x, p->y, p->z, p->size + p->age / 3, M_SMOKE, p->mat == 1 ? 1 : 12 - p->age * 8 / (p->life + 1)); break;
    case K_FIRE: billboard(p->x, p->y, p->z, p->size * (8 + p->age) / 12, M_FIRE, 15); break;
    case K_SPARK: billboard(p->x, p->y, p->z, p->size, M_TRACER, 15); break;
    case K_FLARE: billboard(p->x, p->y, p->z, p->size + (int32_t)(p->mass >> 8), M_FIRE, 15); break;
    case K_DEBRIS: { int32_t c = COS(p->spin), sn = SIN(p->spin), r = p->size * 16, a[3], b[3], d[3];
      a[0] = p->x + (c * r >> 14); a[1] = p->y + (sn * r >> 14); a[2] = p->z;
      b[0] = p->x - (c * r >> 15); b[1] = p->y + (c * r >> 15); b[2] = p->z + (sn * r >> 14);
      d[0] = p->x; d[1] = p->y - (sn * r >> 14); d[2] = p->z - (c * r >> 14);
      tri16(a, b, d, p->mat, lit3(a, b, d)); break; } } }
}
// ---- what stands on the ground, from the same numbers the jets strike (solid): trees, houses, guns and tanks
static void face4(const int32_t *a, const int32_t *b, const int32_t *c, const int32_t *d, int mat, int li) {   // a quad, world 1/16 units
  CV p[4]; eye16(a, &p[0]); eye16(b, &p[1]); eye16(c, &p[2]); eye16(d, &p[3]);
  p[0].u = 0; p[0].v = 0; p[1].u = 511; p[1].v = 0; p[2].u = 511; p[2].v = 511; p[3].u = 0; p[3].v = 511;
  for (int q = 0; q < 4; q++) p[q].l = li << 8;
  int fog = fog16(p[0].z); tri(&p[0], &p[1], &p[2], mat, fog, 0); tri(&p[0], &p[2], &p[3], mat, fog, 0);
}
// a box (units): only the faces turned toward the eye are laid. ridge > 0 puts a gabled roof along x.
static void box(int32_t x, int32_t z, int32_t y0, int32_t hw, int32_t hd, int32_t h, int32_t ridge, int ms, int mt) {
  const int32_t X0 = (x - hw) * 16, X1 = (x + hw) * 16, Z0 = (z - hd) * 16, Z1 = (z + hd) * 16, Y0 = y0 * 16, Y1 = (y0 + h) * 16, YR = (y0 + h + ridge) * 16, ZM = z * 16;
  const int32_t ex = cxw - x, ez = czw - z, ey = cyw - y0 - h;
  if (ex > hw) { int32_t a[3] = {X1, Y1, Z0}, b[3] = {X1, Y1, Z1}, c[3] = {X1, Y0, Z1}, d[3] = {X1, Y0, Z0}; face4(a, b, c, d, ms, 6);
    if (ridge > 0) { int32_t r[3] = {X1, YR, ZM}; tri16(a, r, b, ms, 6); } }
  if (ex < -hw) { int32_t a[3] = {X0, Y1, Z1}, b[3] = {X0, Y1, Z0}, c[3] = {X0, Y0, Z0}, d[3] = {X0, Y0, Z1}; face4(a, b, c, d, ms, 11);
    if (ridge > 0) { int32_t r[3] = {X0, YR, ZM}; tri16(a, r, b, ms, 11); } }
  if (ez > hd) { int32_t a[3] = {X0, Y1, Z1}, b[3] = {X1, Y1, Z1}, c[3] = {X1, Y0, Z1}, d[3] = {X0, Y0, Z1}; face4(a, b, c, d, ms, 4); }
  if (ez < -hd) { int32_t a[3] = {X1, Y1, Z0}, b[3] = {X0, Y1, Z0}, c[3] = {X0, Y0, Z0}, d[3] = {X1, Y0, Z0}; face4(a, b, c, d, ms, 13); }
  if (ridge > 0) {                                           // the two roof slopes, each seen from its own side or from above the ridge
    if (ez > 0 || ey > ridge) { int32_t a[3] = {X0, YR, ZM}, b[3] = {X1, YR, ZM}, c[3] = {X1, Y1, Z1}, d[3] = {X0, Y1, Z1}; face4(a, b, c, d, mt, 7); }
    if (ez < 0 || ey > ridge) { int32_t a[3] = {X1, YR, ZM}, b[3] = {X0, YR, ZM}, c[3] = {X0, Y1, Z0}, d[3] = {X1, Y1, Z0}; face4(a, b, c, d, mt, 13); } }
  else if (ey > 0) { int32_t a[3] = {X0, Y1, Z0}, b[3] = {X1, Y1, Z0}, c[3] = {X1, Y1, Z1}, d[3] = {X0, Y1, Z1}; face4(a, b, c, d, mt, 14); }
}
static uint32_t w_trees, w_houses;
static void grow_world(int ci, int cj) {
  w_trees = w_houses = 0;
  for (int r = 0; r <= 7; r++) for (int dj = -r; dj <= r; dj++) { int st = (dj == -r || dj == r) ? 1 : 2 * r; if (r == 0) st = 1;   // nearest first
    for (int di = -r; di <= r; di += st) { int gi = ci + di, gj = cj + dj, o = obj_at(gi, gj); if (!o) continue;
      int32_t x = gi * 256 + 128, z = gj * 256 + 128, base = TH[(gj & 127) * TN + (gi & 127)] * 8;
      { CV c; to_eye(x, base, z, &c); if (c.z < -300 || c.x * 10 > c.z * 12 + 900 || -c.x * 10 > c.z * 12 + 900) continue; }   // behind or beside the eye
      if ((o & 15) == OB_TREE) { if (r > 3 && (r > 5 || w_trees >= 44)) continue; w_trees++;   // within 3 cells of the eye every tree the jet can strike is drawn
        int32_t top[3] = {x * 16, (base + TREE_H) * 16, z * 16}, a[3] = {(x - TREE_R) * 16, base * 16, z * 16}, b[3] = {(x + TREE_R) * 16, base * 16, z * 16},
                c[3] = {x * 16, base * 16, (z - TREE_R) * 16}, d[3] = {x * 16, base * 16, (z + TREE_R) * 16};
        tri16(top, a, b, M_FOREST, 15); tri16(top, c, d, M_FOREST, 11); }
      else { if (r > 3 && w_houses >= 24) continue; w_houses++;
        if (o & 16) box(x, z, base, HOUSE_D, HOUSE_W, HOUSE_H, 0, M_WALL, M_ROOF);            // turned: a flat-roofed barn
        else box(x, z, base, HOUSE_W, HOUSE_D, HOUSE_H, HOUSE_R - HOUSE_H, M_WALL, M_ROOF); } } }
  for (int k = 0; k < ngt; k++) { const GTarget *g = &GT[k];
    int32_t dx = ((g->x - cxw + 16384) & 32767) - 16384, dz = ((g->z - czw + 16384) & 32767) - 16384; if (dx > 2600 || dx < -2600 || dz > 2600 || dz < -2600) continue;
    int32_t x = cxw + dx, z = czw + dz;
    if (g->kind == GT_GUN) { box(x, z, g->y, 34, 34, g->alive ? 14 : 6, 0, M_PLOW, M_DARK);
      if (g->alive) { const Jet *m = &J[0]; int32_t bx = wrapw((m->x >> 4) - g->x), bz = wrapw((m->z >> 4) - g->z), bl = (int32_t)root((uint32_t)(bx * bx + bz * bz)) + 1;
        int32_t b0[3] = {x * 16, (g->y + 16) * 16, z * 16}, b1[3] = {(x + bx * 60 / bl) * 16, (g->y + 60) * 16, (z + bz * 60 / bl) * 16}, b2[3] = {(x - bz * 8 / bl) * 16, (g->y + 16) * 16, (z + bx * 8 / bl) * 16};
        tri16(b0, b1, b2, M_DARK, 9); } }
    else if (g->kind == GT_TANK) box(x, z, g->y, 30, 30, g->alive ? 44 : 10, 0, g->alive ? M_WALL : M_SCORCH, g->alive ? M_RED : M_SCORCH); }
  for (int k = 0; k < nbr; k++) { int32_t dx = wrapw(BRG[k].x - cxw), dz = wrapw(BRG[k].z - czw); if (dx > 4200 || dx < -4200 || dz > 4200 || dz < -4200) continue;
    int32_t y0 = WL * 8 - 8, t = br_top(k); box(cxw + dx, czw + dz, y0, BR_L, BR_W, t - y0, 0, br_up[k] ? M_WALL : M_SCORCH, br_up[k] ? M_PLOW : M_SCORCH); }
}
// clouds: seven heaps of puffs, placed by seeds over the wrapping world, drawn as the nearest copy
#define NCLOUD 7
static void grow_clouds(void) {
  cloud_in = 0;
  for (int c = 0; c < NCLOUD; c++) { uint32_t h = mixh((uint32_t)c, 11, 61);
    int32_t cx = (int32_t)(h % 32768), cz = (int32_t)((h >> 15) % 32768), cy = 2350 + (int32_t)((h >> 7) % 700);
    int32_t dx = ((cx - cxw + 16384) & 32767) - 16384, dz = ((cz - czw + 16384) & 32767) - 16384;
    if (dx > 7000 || dx < -7000 || dz > 7000 || dz < -7000) continue;
    { int32_t ax = dx < 0 ? -dx : dx, az = dz < 0 ? -dz : dz, m = ax > az ? ax : az, dy = cyw - cy;   // K10: flying through: the eye inside the heap sees a dither, not the puffs
      if (m < 420 && dy > -160 && dy < 320) { int lv = 1 + (420 - m) / 140; if (lv > 3) lv = 3; if (lv > cloud_in) cloud_in = lv; continue; } }
    for (int k = 0; k < 5; k++) { uint32_t q = mixh((uint32_t)c, (uint32_t)k, 62);
      billboard((cxw + dx + (int32_t)(q % 500) - 250) * 16, (cy + (int32_t)((q >> 9) % 140)) * 16, (czw + dz + (int32_t)((q >> 17) % 500) - 250) * 16, 230 + (int32_t)((q >> 25) % 120), M_CLOUD, 13 - k); } }
}
static int brk_x, brk_y, brk_s, brk_on, brk_lock, mk_x, mk_y, mk_on;
// ---- the missions: strike a bridge, or defend a village from two raiders. They begin after the first 20 s (the opening is ACE 4's).
static int near_vil(int32_t x, int32_t z) { int bv = 0; int32_t best = 0x7fffffff; for (int v = 0; v < nvil; v++) { int32_t dx = wrapw(x - (VIL[v].ci * 256 + 128)) >> 4, dz = wrapw(z - (VIL[v].cj * 256 + 128)) >> 4, d = dx * dx + dz * dz; if (d < best) { best = d; bv = v; } } return bv; }
static void mission_end(int ok) { if (ok) mis_ok++; else mis_fail++; mis_res = ok ? 1 : 2; mis = 0; mis_t = 0; for (int i = 1; i < NJ; i++) J[i].raid = 0; }
static void mission_step(void) {
  const Jet *me = &J[0]; mis_t++;
  if (frame_n < 600 || !nvil) return;
  if (!mis) { if (mis_t < 300) return; mis_res = 0; mis_t = 0; mis_n++;
    if (mis_n & 1) { int bk = 0; int32_t best = 0x7fffffff; for (int k = 0; k < nbr; k++) { int32_t dx = wrapw(BRG[k].x - (me->x >> 4)) >> 4, dz = wrapw(BRG[k].z - (me->z >> 4)) >> 4, d = dx * dx + dz * dz; if (br_up[k] && d < best && d > 100) { best = d; bk = k; } }
      if (!nbr) return; mis = 1; mis_g = ngt - nbr + bk; mis_v = near_vil(BRG[bk].x, BRG[bk].z); }
    else { int v = near_vil((me->x >> 4) + 4000, me->z >> 4); if (!GT[v * 3].alive || !GT[v * 3 + 1].alive || !GT[v * 3 + 2].alive) return;
      mis = 2; mis_v = v; mis_g = v * 3; J[1].raid = J[2].raid = 1; }
    return; }
  if (mis == 1) { const GTarget *g = &GT[mis_g]; if (!g->alive) mission_end(g->by == 0); else if (mis_t > 5400) mission_end(0); }
  else { int left = 0; for (int k = 0; k < 3; k++) left += GT[mis_v * 3 + k].alive;
    int raiders = 0; for (int i = 1; i <= 2; i++) { if (J[i].raid && !J[i].alive) J[i].raid = 0; raiders += J[i].raid; }
    if (!raiders) mission_end(1); else if (!left || mis_t > 3600) mission_end(0); }
}
static void mission_text(void) {
  memset(GMIS, 0, sizeof GMIS); const Jet *me = &J[0];
  if (mis) { const GTarget *g = &GT[mis_g]; int32_t dx = wrapw(g->x - (me->x >> 4)), dz = wrapw(g->z - (me->z >> 4)); uint32_t d = root((uint32_t)((dx >> 4) * (dx >> 4) + (dz >> 4) * (dz >> 4))) * 16 / 100;
    const char *a = mis == 1 ? "STRIKE BRIDGE " : "DEFEND "; int n = (int)strlen(a); memcpy(GMIS, a, (size_t)n); memcpy(GMIS + n, VIL[mis_v].name, strlen(VIL[mis_v].name)); n += (int)strlen(VIL[mis_v].name);
    GMIS[n++] = ' '; put_num(GMIS + n, d > 99 ? 99 : d, 2); n += 2; GMIS[n++] = 'H'; }
  else if (mis_res && mis_t < 240) memcpy(GMIS, mis_res == 1 ? "MISSION DONE" : "MISSION FAILED", mis_res == 1 ? 12 : 14);
  if (GMIS[0]) { int n = (int)strlen(GMIS); memcpy(GMIS + n, "  M", 3); put_num(GMIS + n + 3, mis_ok % 100, 2); GMIS[n + 5] = '-'; put_num(GMIS + n + 6, mis_fail % 100, 2); }
}
static void ace_build(void) {
  ntri = 0; Jet *me = &J[0];
  // the eye
  const Jet *fj = me; int32_t dist = 118, up = 26, pitch = 90;
  if (killcam) { fj = &J[killj]; dist = 280; up = 170; if (fj->dead_t >= 1000) { dist = 800; up = 420; } }   // a wreck is watched from further and higher
  int32_t fx = fj->x >> 4, fy = fj->y >> 4, fz = fj->z >> 4;
  if (cam == 0 || killcam) { int32_t df = wrap4k((fj->yaw >> 4) - camyaw); camyaw = (camyaw + df / 3) & 4095; pitch = 90 - (fj->gam >> 4) / 3; }
  else if (cam == 1) { camyaw = (camyaw + 10) & 4095; dist = 120; up = 14; pitch = 40; }
  else if (cam == 3) { camyaw = (fj->yaw >> 4) & 4095; dist = -30; up = 10; pitch = -(fj->gam >> 4); }   // the cockpit: the eye is the jet's
  else { const Jet *t = &J[me->tgt]; camyaw = full_angle((t->z - me->z) >> 4, (t->x - me->x) >> 4); dist = 170; up = 50;
    int32_t tx_ = (t->x - me->x) >> 4, tz_ = (t->z - me->z) >> 4, dy = (t->y - me->y) >> 4, hd = (int32_t)root((uint32_t)(tx_ * tx_ + tz_ * tz_));
    pitch = -wrap4k(full_angle(hd + dist, dy - up)) + 40; }
  if (me->hitf && !killcam) { uint32_t q = mixh(frame_n, 3, 91); camyaw = (camyaw + (int32_t)(q % 9) - 4) & 4095; pitch += (int32_t)((q >> 8) % 13) - 6; }   // impacts are felt: the eye jolts
  if (pitch > 600) pitch = 600; if (pitch < -600) pitch = -600;
  cxw = fx - (SIN(camyaw) * dist >> 14); czw = fz - (COS(camyaw) * dist >> 14); cyw = fy + up;
  { int32_t g = ground(cxw, czw), g2 = ground((cxw + fx) / 2, (czw + fz) / 2); if (g2 > g) g = g2; g += 70; if (cyw < g) { if (killcam) pitch += (g - cyw) / 2; cyw = g; } }   // the eye stays out of the hills
  if (pitch > 700) pitch = 700;
  if (killcam && pitch > (fj->dead_t >= 1000 ? 150 : 300)) pitch = fj->dead_t >= 1000 ? 150 : 300;                    // K6: the eye on a wreck looked straight down into the fire: 121,000 dots of near ground and smoke
  cc = COS(camyaw); cs = SIN(camyaw); cpc = COS(pitch); cps = SIN(pitch);
#ifdef NO_ROLL
  camroll = 0;
#else
  { int32_t bk = ((fj->bank >> 4) + 2048) & 4095; bk -= 2048; if (bk > 1024) bk = 2048 - bk; if (bk < -1024) bk = -2048 - bk;   // inverted: lean back toward level
    int32_t want = (cam == 3 && !killcam) ? bk : (cam == 0 || killcam) ? bk * 2 / 5 : cam == 1 ? bk / 5 : 0; camroll += (want - camroll) / 5; }
#endif
  cr = COS(camroll); sr = SIN(camroll); if (!camroll) { cr = 16384; sr = 0; }
  hz = HORIZ - FOC * cps / cpc;                                  // where level ground at infinity meets the glass (before the roll)
  { int32_t ca = COS(900 - camyaw), sa = SIN(900 - camyaw);       // the sun only in front of the eye (it showed behind in ACE 3)
    sun_on = ca > 3000; int32_t a = sun_on ? (int32_t)((int64_t)sa * FOC / ca) : 0, bb = HORIZ - (hz - 40);
    if (a > 2000) a = 2000; if (a < -2000) a = -2000;
    sun_x = SW / 2 + (int)((a * cr + bb * sr) >> 14); sun_y = HORIZ - (int)((bb * cr - a * sr) >> 14); }
  zq = 4; uint32_t t0 = (uint32_t)ntri;
  if (cam != 3 || killcam) grow_jet(0); for (int i = 1; i < NJ; i++) grow_jet(i);
  ftris = (uint32_t)ntri - t0;
  grow_particles(); grow_clouds();
  // the bracket on the target
  { const Jet *t = &J[me->tgt]; CV c; int32_t w[3] = {t->x, t->y, t->z}; eye16(w, &c); brk_on = 0;
    if (t->alive && c.z > 16 * 60) { brk_x = SW / 2 + (int)((int64_t)c.x * FOC / c.z); brk_y = HORIZ - (int)((int64_t)c.y * FOC / c.z);
      brk_s = (int)((int64_t)FOC * 16 * 40 / c.z); if (brk_s < 7) brk_s = 7; if (brk_s > 40) brk_s = 40; brk_on = brk_x > -40 && brk_x < SW + 40 && brk_y > 0 && brk_y < SH + 40; brk_lock = me->lockt > 24; } }
  mk_on = 0; mission_text();
  if (mis) { const GTarget *g = &GT[mis_g]; CV c; to_eye(cxw + wrapw(g->x - cxw), g->y + 40, czw + wrapw(g->z - czw), &c);
    if (c.z > 60) { mk_x = SW / 2 + (int)((int64_t)c.x * FOC / c.z); mk_y = HORIZ - (int)((int64_t)c.y * FOC / c.z); }
    else { mk_x = c.x > 0 ? SW - 8 : 8; mk_y = SH / 2; }                              // behind: the marker sits on the side to turn to
    if (mk_x < 8) mk_x = 8; if (mk_x > SW - 8) mk_x = SW - 8; if (mk_y < 24) mk_y = 24; if (mk_y > SH - 20) mk_y = SH - 20; mk_on = 1; }
  zq = 0;
  int ci = cxw >> 8, cj = czw >> 8, Wd = 2 * RV + 1;
  for (int j = 0; j < Wd; j++) for (int i = 0; i < Wd; i++) { int gi = ci - RV + i, gj = cj - RV + j; CV v;
    int hh = TH[(gj & 127) * TN + (gi & 127)];
    if (i < RV - R0 || i > RV + R0 || j < RV - R0 || j > RV + R0) for (int q = 0; q < 4; q++) { int h2 = TH[((gj + (q >> 1)) & 127) * TN + ((gi + (q & 1)) & 127)]; if (h2 > hh) hh = h2; }   // K4: a far block's corner keeps the peak it hides
    to_eye(gi * 256, hh * 8, gj * 256, &v); VCX[j * Wd + i] = (int16_t)v.x; VCY[j * Wd + i] = (int16_t)v.y; VCZ[j * Wd + i] = (int16_t)v.z; }
  for (int e = -R0 + 1; e < R0; e += 2) { const int lo = RV - R0, hi = RV + R0, ii = RV + e;   // the fine ring's mid-edge corners sit on the blocks' edges
    const int ks[4][3] = {{lo * Wd + ii, lo * Wd + ii - 1, lo * Wd + ii + 1}, {hi * Wd + ii, hi * Wd + ii - 1, hi * Wd + ii + 1},
                          {ii * Wd + lo, (ii - 1) * Wd + lo, (ii + 1) * Wd + lo}, {ii * Wd + hi, (ii - 1) * Wd + hi, (ii + 1) * Wd + hi}};
    for (int q = 0; q < 4; q++) { VCX[ks[q][0]] = (int16_t)((VCX[ks[q][1]] + VCX[ks[q][2]]) / 2); VCY[ks[q][0]] = (int16_t)((VCY[ks[q][1]] + VCY[ks[q][2]]) / 2); VCZ[ks[q][0]] = (int16_t)((VCZ[ks[q][1]] + VCZ[ks[q][2]]) / 2); } }
  for (int r = 0; r <= RV; r++) for (int dj = -r; dj <= r; dj++) { int st = (dj == -r || dj == r) ? 1 : 2 * r; if (r == 0) st = 1;
    for (int di = -r; di <= r; di += st) { int i = RV + di, j = RV + dj; if (i < 0 || j < 0 || i >= 2 * RV || j >= 2 * RV) continue;
      if (di >= -R0 && di < R0 && dj >= -R0 && dj < R0) glide_cell(i, j, ci, cj, 1);
      else if (!(di & 1) && !(dj & 1) && (di >= R0 || di <= -R0 - 2 || dj >= R0 || dj <= -R0 - 2)) glide_cell(i, j, ci, cj, 2); } }
  zq = 4; grow_world(ci, cj); zq = 0;                         // laid in 1/16 units, like the flyer
  memset(BCNT, 0, sizeof BCNT);
  for (int k = 0; k < ntri; k++) for (int b = TR[k].ry0 >> 4; b <= TR[k].ry1 >> 4; b++) BCNT[b]++;
  BINAT[0] = 0; for (int b = 0; b < TY; b++) { int n = BCNT[b]; if (BINAT[b] + n > NBIN) n = NBIN - BINAT[b]; BINAT[b + 1] = (uint16_t)(BINAT[b] + n); BCNT[b] = 0; }
  for (int k = 0; k < ntri; k++) for (int b = TR[k].ry0 >> 4; b <= TR[k].ry1 >> 4; b++) { int at = BINAT[b] + BCNT[b]; if (at < BINAT[b + 1]) { TBIN[at] = (uint16_t)k; BCNT[b]++; } }
  tris_set = (uint32_t)ntri;
  memset(GHUD, ' ', sizeof GHUD - 1); GHUD[sizeof GHUD - 1] = 0;
  { int32_t hull = 0; for (int p = 0; p < NPART; p++) hull += me->S[p]; hull = hull * 100 / (NPART * 256);
    memcpy(GHUD, "ACE4", 4); memcpy(GHUD + 5, "W", 1); put_num(GHUD + 6, wave % 100, 2); GHUD[9] = 'K'; put_num(GHUD + 10, kills % 1000, 3); GHUD[14] = 'G'; put_num(GHUD + 15, gkills % 100, 2);
    memcpy(GHUD + 18, "HULL", 4); put_num(GHUD + 23, (uint32_t)hull, 3); memcpy(GHUD + 27, "MSL", 3); put_num(GHUD + 31, (uint32_t)me->msl, 1);
    memcpy(GHUD + 33, "FLR", 3); put_num(GHUD + 37, (uint32_t)(me->flr < 0 ? 0 : me->flr), 1);
    if (me->heat > 150) memcpy(GHUD + 40, "HOT", 3);
    if (brk_lock && brk_on) memcpy(GHUD + 46, "LOCK", 4);
    for (int k = 0; k < NPT; k++) if (PT[k].kind == K_MSL && PT[k].tgt == 0) memcpy(GHUD + 46, "MSL!", 4);
    if (!me->alive) memcpy(GHUD + 46, "DOWN", 4); }
  { int best = 1 << 30, bv = -1; for (int v = 0; v < nvil; v++) { int32_t dx = wrapw((me->x >> 4) - (VIL[v].ci * 256 + 128)), dz = wrapw((me->z >> 4) - (VIL[v].cj * 256 + 128));   // a sense of place
      int32_t d = (dx >> 4) * (dx >> 4) + (dz >> 4) * (dz >> 4); if (d < best) { best = d; bv = v; } }
    memset(GPLACE, 0, sizeof GPLACE);
    if (bv >= 0 && best < 200 * 200) { const char *w = best < 64 * 64 ? "OVER " : "NEAR "; memcpy(GPLACE, w, 5); memcpy(GPLACE + 5, VIL[bv].name, strlen(VIL[bv].name)); } }
#if !defined(DESK) && !defined(MOUTH_FIFO) && !defined(DEMO) && FRAMES == 0   // bench builds read no pins
  if (stick_test) stick_line();
#endif
}
// the bracket and the hit flash, laid over a woven band
static void overlay_band(int b, uint16_t *rb) {
  const int y0 = b * TS; Jet *me = &J[0];
  if (brk_on) { uint16_t col = brk_lock ? HUDR : HUDG; int x0 = brk_x - brk_s, x1 = brk_x + brk_s, ya = brk_y - brk_s, yb = brk_y + brk_s, arm = brk_s / 2;
    for (int r = 0; r < TS; r++) { int y = y0 + r; uint16_t *d = rb + r * SW;
      if (y == ya || y == yb) for (int x = x0; x <= x1; x++) { if ((x < x0 + arm || x > x1 - arm) && x >= 0 && x < SW) d[x] = col; }
      if ((y > ya && y < ya + arm) || (y < yb && y > yb - arm)) { if (x0 >= 0 && x0 < SW) d[x0] = col; if (x1 >= 0 && x1 < SW) d[x1] = col; } } }
  if (me->hitf) for (int r = 0; r < TS; r++) { uint16_t *d = rb + r * SW; for (int x = 0; x < 3 + me->hitf; x++) { d[x] = HUDR; d[SW - 1 - x] = HUDR; } }
  if (mk_on) for (int r = 0; r < TS; r++) { int y = y0 + r, dy = y - mk_y; if (dy < -6 || dy > 6) continue; int w = 6 - (dy < 0 ? -dy : dy); uint16_t *d = rb + r * SW;   // the mission's diamond
    if (mk_x - w >= 0) d[mk_x - w] = HUDW; if (mk_x + w < SW) d[mk_x + w] = HUDW; }
  if (cam == 3 && !killcam) { uint16_t fr = LUT[M_DARK][0][4][2];                     // the cockpit: canopy bows, the sill, a gunsight ring
    for (int r = 0; r < TS; r++) { int y = y0 + r; uint16_t *d = rb + r * SW;
      if (y < 3) for (int x = 0; x < SW; x++) d[x] = fr;
      if (y >= SH - 26 && y < SH - 13) { for (int x = 0; x < SW; x++) d[x] = fr; continue; }
      { int xa = (SH - y) * 70 / SH; for (int x = xa; x < xa + 5 && x < SW; x++) { d[x] = fr; d[SW - 1 - x] = fr; } }
      int dy = y - HORIZ; for (int x = SW / 2 - 24; x <= SW / 2 + 24; x++) { int dx = x - SW / 2, q = dx * dx + dy * dy; if ((q > 380 && q < 430) || (q < 3)) d[x] = HUDG; } } }
}
// LESSON-BEGIN 17 of 18: The hands, the test card, and the pilot that flies when no one does
// WHAT    : B held at power-up shows a still test card (eight bars, a one-dot frame, a centre cross) to set RGB_ORDER and
//           NO_INVERT by eye before anything moves. Nothing touched: the demo pilot flies, so the bench can play.
// LESSON-END
// ================================================================ the hands: the stick, three buttons, and the pilot that flies when no one does
#define PIN_VRX 0
#define PIN_VRY 1
#define PIN_JSW 2
#define PIN_A 3
#define PIN_B 20
#define PIN_C 21
#if !defined(DESK) && !defined(MOUTH_FIFO) && !defined(DEMO) && FRAMES == 0   // bench builds read no pins
static int cx0 = 2048, cy0 = 2048; static uint32_t idle_t;
static void pad_begin(void) {
  pinMode(PIN_JSW, INPUT_PULLUP); pinMode(PIN_A, INPUT_PULLUP); pinMode(PIN_B, INPUT_PULLUP); pinMode(PIN_C, INPUT_PULLUP);
  analogReadResolution(12); int32_t sx = 0, sy = 0;
  for (int k = 0; k < 32; k++) { sx += analogRead(PIN_VRX); sy += analogRead(PIN_VRY); }
  cx0 = (int)(sx >> 5); cy0 = (int)(sy >> 5);             // where the stick rests is its middle
  stick_test = !digitalRead(PIN_C);                        // C held at power-up: show the stick's raw reading
}
static void pad_read(Pad *p) {
  int rx = ((analogRead(PIN_VRX) + analogRead(PIN_VRX)) >> 1) - cx0, ry = ((analogRead(PIN_VRY) + analogRead(PIN_VRY)) >> 1) - cy0;   // two looks: half the noise
  raw_x = rx; raw_y = ry;
#ifdef JOY_SWAP
  { int t = rx; rx = ry; ry = t; }
#endif
#ifdef JOY_FLIP_X
  rx = -rx;
#endif
#ifdef JOY_FLIP_Y
  ry = -ry;
#endif
  rx /= 14; ry /= 14; if (rx > 127) rx = 127; if (rx < -127) rx = -127; if (ry > 127) ry = 127; if (ry < -127) ry = -127;
  if (rx > -14 && rx < 14) rx = 0; if (ry > -14 && ry < 14) ry = 0;   // a dead band per axis (LANTERN's lesson: a round one leaks on the diagonal)
  rx = (rx + rx * (rx < 0 ? -rx : rx) / 127) / 2; ry = (ry + ry * (ry < 0 ? -ry : ry) / 127) / 2;   // finer near the middle, full at the stop
  p->jx = rx; p->jy = ry; p->a = !digitalRead(PIN_A); p->b = !digitalRead(PIN_B); p->c = !digitalRead(PIN_C); p->sw = !digitalRead(PIN_JSW);
  raw_b = p->a | p->b << 1 | p->c << 2 | p->sw << 3;
}
// B held at power-up: a still test card (eight bars, a one-dot frame, a centre cross) until B is pressed again. Nothing on
// it is 3D or timed: a line or speckle seen here is the wire or the panel, not the game.
static void test_card(void) {
  static const uint8_t BAR[8][3] = {{255, 255, 255}, {255, 255, 0}, {0, 255, 255}, {0, 255, 0}, {255, 0, 255}, {255, 0, 0}, {0, 0, 255}, {0, 0, 0}};
  for (int b = 0; b < TY; b++) { uint16_t *rb = RB[0];
    for (int r = 0; r < TS; r++) for (int x = 0; x < SW; x++) { int y = b * TS + r; const uint8_t *c = BAR[x * 8 / SW];
      int edge = x == 0 || x == SW - 1 || y == 0 || y == SH - 1 || x == SW / 2 || y == SH / 2;
      rb[r * SW + x] = edge ? glass(255, 255, 255) : glass(c[0], c[1], c[2]); }
    window(XO, XO + SW - 1, b * TS, b * TS + TS - 1); pix(rb, BANDPX * 2); m_wait_all(); }
  while (!digitalRead(PIN_B)) delay(10);
  while (digitalRead(PIN_B)) delay(10);
  while (!digitalRead(PIN_B)) delay(10);
  forget_glass();
}
static void stick_line(void) {                             // STICK X+0123 Y-0004 ABCP MID: what the pins read, before any rule
  memset(GHUD, ' ', sizeof GHUD - 1); memcpy(GHUD, "STICK X", 7); GHUD[7] = raw_x < 0 ? '-' : '+'; put_num(GHUD + 8, (uint32_t)(raw_x < 0 ? -raw_x : raw_x), 4);
  GHUD[13] = 'Y'; GHUD[14] = raw_y < 0 ? '-' : '+'; put_num(GHUD + 15, (uint32_t)(raw_y < 0 ? -raw_y : raw_y), 4);
  for (int k = 0; k < 4; k++) GHUD[21 + k] = raw_b >> k & 1 ? "ABCP"[k] : '-';
  memcpy(GHUD + 27, "MID", 3); put_num(GHUD + 31, (uint32_t)cx0, 4); GHUD[35] = ' '; put_num(GHUD + 36, (uint32_t)cy0, 4);
}
#endif
static Pad IN, LAST;
static uint32_t gtime;
static void step_all(void) {
  LAST = IN;
#if defined(DESK) || defined(MOUTH_FIFO) || defined(DEMO) || FRAMES > 0
  demo = 1;
#else
  { Pad p; pad_read(&p); if (p.jx || p.jy || p.a || p.b || p.c || p.sw) { idle_t = 0; demo = 0; } else if (++idle_t > 600) demo = 1; if (!demo) IN = p; }
#endif
  Jet *me = &J[0];
  // the target: the nearest bandit still flying
  { int32_t best = 0x7fffffff; for (int i = 1; i < NJ; i++) if (J[i].alive) { int32_t dx = (J[i].x - me->x) >> 8, dy = (J[i].y - me->y) >> 8, dz = (J[i].z - me->z) >> 8, d = dx * dx + dy * dy + dz * dz;
      if (d < best) { best = d; me->tgt = i; } } }
  if (demo) { pilot(0, me); IN.jx = me->jx; IN.jy = me->jy; IN.a = me->fire; IN.b = me->mfire; IN.sw = me->ab;
    uint32_t ph = gtime % 1800; int want = ph < 900 ? 0 : ph < 1150 ? 2 : ph < 1350 ? 0 : ph < 1500 ? 1 : ph < 1650 ? 0 : (gtime >= 1800 ? 3 : 0); IN.c = want != cam && !LAST.c; }
  else { me->jx = IN.jx; me->jy = IN.jy; me->fire = IN.a; me->mfire = IN.b && !LAST.b; me->ab = IN.sw; }
#if defined(DESK) && defined(DESK_FUZZ)
  { uint32_t h = mixh(frame_n / 23, 7, 99); me->jx = (int)(h % 255) - 127; me->jy = (int)((h >> 8) % 255) - 127; me->fire = (h >> 16 & 3) == 0; me->ab = (h >> 21 & 15) == 0; }   // a careless hand
#endif
#if defined(DESK) && defined(DESK_PULL)
  me->jx = 0; me->jy = -127; me->ab = (frame_n / 300) & 1; me->fire = 0; me->mfire = 0;   // the stuck-climb hand: stick held fully back
#endif
#if defined(DESK) && defined(DESK_RIDGE)
  { int32_t sy = SIN(me->yaw >> 4), cy = COS(me->yaw >> 4), px = me->x >> 4, pz = me->z >> 4;   // the ridge runner: turn toward the higher side, hold 60-200 units
    int32_t l = ground(px - (cy * 500 >> 14), pz + (sy * 500 >> 14)), r = ground(px + (cy * 500 >> 14), pz - (sy * 500 >> 14)), cl = (me->y >> 4) - ground(px, pz);
    int32_t wb = (l > r ? -500 : 500) * 16 - me->bank; me->jx = wb > 900 ? 90 : wb < -900 ? -90 : wb / 10; me->jy = cl > 200 ? 60 : cl < 60 ? -127 : 0; me->ab = (frame_n / 90) & 1; me->fire = 0; me->mfire = 0; }
#endif
  { static int c_held, c_lock; if (c_lock) c_lock--;           // C: a tap turns the camera (once, on release); held, it drops flares
    if (IN.c) { c_held++; if (c_held >= 10 && (c_held - 10) % 12 == 0) drop_flare(me); }
    else { if (c_held > 0 && c_held < 10 && !c_lock) { cam = (cam + 1) % 4; c_lock = 8; } c_held = 0; } }
  if (demo && threat_on(0)) drop_flare(me);
  for (int i = 1; i < NJ; i++) pilot(i, &J[i]);
  for (int i = 1; i < NJ; i++) if (J[i].skill >= 2 && threat_on(i)) drop_flare(&J[i]);   // from the second wave the bandits flare
  for (int i = 0; i < NJ; i++) if (J[i].fcool) J[i].fcool--;
  for (int i = 0; i < NJ; i++) { Jet *j = &J[i]; int was = j->alive;
    if (j->dead_t < 1000) jet_step(j, i);
    if (was && !j->alive) { if (i) { kills++; if (kills % 3 == 0) wave++; } else deaths++; if (!killcam) { killcam = 75; killj = i; } }
    if (j->dead_t >= 1000 && ++j->dead_t > 1100) { jet_begin(i, i ? mixh((uint32_t)j->seed, kills, 17) : (uint32_t)j->seed); if (!i) for (int k = 1; k < NJ; k++) J[k].tgt = 0; } }
  if (killcam) killcam--;
#if defined(DESK) && defined(TRACE)
  { static uint32_t fires, ons; fires += (uint32_t)me->fire; ons += me->lockt > 0; if (frame_n % 300 == 299) printf("  player firing frames %u, on-target frames %u\n", (unsigned)fires, (unsigned)ons); }
#endif
  ground_step(); particles_step(); scorch_step(); mission_step(); gtime++;
  { static int tod_next; int m = (int)(frame_n % NMAT);                  // K9: the day turns, one material a frame
    if (m == 0) { uint32_t t = frame_n < 1800 ? 0 : (frame_n - 1800) % 14400; tod_next = t < 7200 ? (int)(t * 256 / 7200) : (int)((14400 - t) * 256 / 7200); tod_now = tod_next; grow_sky(); }
    if (frame_n >= 1800) look_mat(m, DK[m], BR[m]); }
#if defined(DESK) && defined(TRACE)
  if (frame_n % 15 == 0) printf("f %u me v %d gam %d bank %d alt %d tgt %d lock %d | b1 %d alive %d v %d | kills %u deaths %u tears %u dmg %u/%u bad %u tris %d\n", (unsigned)frame_n, (int)me->v, (int)(me->gam >> 4), (int)(me->bank >> 4),
    (int)((me->y >> 4) - ground(me->x >> 4, me->z >> 4)), (int)me->tgt, (int)me->lockt, (int)((J[1].y >> 4) - ground(J[1].x >> 4, J[1].z >> 4)), (int)J[1].alive, (int)J[1].v, (unsigned)kills, (unsigned)deaths, (unsigned)tears, (unsigned)dmg_in, (unsigned)dmg_ground, (unsigned)dmg_bad, ntri);
#endif
}
// LESSON-BEGIN 18 of 18: The frame: grow, weave each band, print, send only what changed
// WHAT    : step_all, then the picture grown, each band woven and its tiles printed; only changed tiles go down the wire.
// BENCH   : FRAMES=n emulator builds print F lines and, at the end, LEDGER, MISSILES, DAMAGE, STACK and HEAP lines.
// LESSON-END
// ================================================================ the frame: grow, weave each band, print, send only what changed
static int full_next = 1;
static uint32_t c_prep, c_draw, c_print, c_send;
void frame(void) {
  uint32_t c0 = cyc();
  f_bytes = f_trans = f_sent = f_kept = f_drawn = 0;
  step_all();
  grain = (int)(g_it * 16 + g_sp * 240 > GRAIN_AT);
  if (killcam == 75 || killcam == 74) grain = 1;               // K6: a cut to a new eye has no ledger yet: its first frames are laid coarse   // the frame's own ledger: the last frame's work against the budget's drawing share
  g_it = g_sp = 0;
  ace_build();
  for (int b = 0; b < TY; b++) SUS[b] = ALLT;
  if (full_next) full_next = 0;
#ifndef NO_HEAL
  if ((frame_n & 3) == 0) { int hb = (int)((frame_n >> 2) % TY);
    for (int t = 0; t < TX; t++) if (HELD[hb] >> t & 1) L_del++;
    HELD[hb] = 0; }
#endif
  uint32_t c1 = cyc(); c_draw = c_print = c_send = 0;
  for (int b = 0; b < TY; b++) {
    int s = (int)(slot_n++ & 1); uint16_t *rb = RB[s], *pb = PB[s];
#ifdef NO_OVERLAP
    m_wait_all(); m_wait(s);
#else
    m_wait(s);
#endif
    uint32_t ca = cyc();
    glide_band(b, rb); overlay_band(b, rb); f_drawn += TX;
    uint32_t cb = cyc();
    uint32_t chg = 0;
    for (int t = 0; t < TX; t++) {
      uint32_t p = tile_print(rb + t * TS);
#ifndef TRUTH
      if ((HELD[b] >> t & 1) && PRINT[b][t] == p) { f_kept++; continue; }   // KEEP: the glass already shows this print
#endif
      if (HELD[b] >> t & 1) L_del++;
      L_est++; PRINT[b][t] = p; HELD[b] |= 1u << t; chg |= 1u << t; f_sent++; }
    uint32_t cc_ = cyc();
    if (chg == ALLT) { window(XO, XO + SW - 1, b * TS, b * TS + TS - 1); pix(rb, BANDPX * 2); }
    else { uint16_t *o = pb;
      for (int t = 0; t < TX;) { if (!(chg >> t & 1)) { t++; continue; }
        int t1 = t; while (t1 < TX && (chg >> t1 & 1)) t1++;
        int w = (t1 - t) * TS;
        for (int r = 0; r < TS; r++) memcpy(o + r * w, rb + r * SW + t * TS, (size_t)w * 2);
        window(XO + t * TS, XO + t1 * TS - 1, b * TS, b * TS + TS - 1); pix(o, w * TS * 2); o += w * TS; t = t1; } }
    m_fence(s);
    uint32_t cd = cyc(); c_draw += cb - ca; c_print += cc_ - cb; c_send += cd - cc_;
  }
  cmd(0x00, 0, 0);
  c_prep = c1 - c0;
  frame_n++;
}

#ifndef FRAMES
#define FRAMES 0
#endif
#if defined(DESK) || FRAMES > 0
static uint32_t held_count(void) { uint32_t n = 0; for (int b = 0; b < TY; b++) for (int t = 0; t < TX; t++) n += HELD[b] >> t & 1; return n; }
#endif
void setup(void) {
#if !defined(DESK) && FRAMES > 0
  Serial0.begin(921600);                                  // emulator bench builds only; the board build never opens UART0 (GPIO20/21 are buttons)
#endif
  grow_ring(); grow_ease(); grow_face(); grow_looks(); grow_terrain(); ground_begin();
#if !defined(DESK) && !defined(MOUTH_FIFO) && !defined(DEMO) && FRAMES == 0   // bench builds read no pins
  pad_begin();
#endif
  panel_begin();
  clean_slate();
#if !defined(DESK) && !defined(MOUTH_FIFO) && !defined(DEMO) && FRAMES == 0   // bench builds read no pins
  if (!digitalRead(PIN_B)) test_card();
#endif
  for (int i = 0; i < NJ; i++) jet_begin(i, 0xACE3u + (uint32_t)i * 977u);
#ifdef SKIP_TO
  for (int f = 0; f < SKIP_TO; f++) { step_all(); if ((int)(frame_n % NMAT) == 0) grow_sky(); frame_n++; }   // bench: fly the world on, unseen, to a later hour
#endif
#ifdef START_CLOUD
  { Jet *m = &J[0]; uint32_t h = mixh(0, 11, 61); m->x = (int32_t)(h % 32768) * 16 + 150 * 16; m->z = (int32_t)((h >> 15) % 32768) * 16; m->y = (2410 + (int32_t)((h >> 7) % 700)) * 16; m->gam = 0; m->yaw = 0; }   // bench: inside cloud 0
#endif
}
#ifndef DESK
void loop(void) {
#if !defined(MOUTH_FIFO) && FRAMES == 0
  static uint32_t t0; while ((uint32_t)(micros() - t0) < 33000) delayMicroseconds(250);   // at most 30 frames a second
  t0 = micros();
#endif
  frame();
#if FRAMES > 0
  Serial0.printf("F %u pre %u draw %u print %u send %u bytes %u trans %u drawn %u sent %u kept %u tris %u fly %u it %u sp %u\n", (unsigned)frame_n, (unsigned)c_prep,
    (unsigned)c_draw, (unsigned)c_print, (unsigned)c_send, (unsigned)f_bytes, (unsigned)f_trans, (unsigned)f_drawn, (unsigned)f_sent, (unsigned)f_kept, (unsigned)tris_set, (unsigned)ftris, (unsigned)g_it, (unsigned)g_sp);
#ifndef SKIP_TO
#define SKIP_TO 0
#endif
  if (frame_n >= FRAMES + SKIP_TO) {
    m_wait_all();
    Serial0.printf("LEDGER est %u del %u keep %u held %u\n", (unsigned)L_est, (unsigned)L_del, (unsigned)(L_est - L_del), (unsigned)held_count());
    Serial0.printf("MISSILES fired %u hit %u fooled %u spent %u flares %u\n", (unsigned)m_fired, (unsigned)m_hit, (unsigned)m_fooled, (unsigned)m_spent, (unsigned)flares_out);
    Serial0.printf("DAMAGE in %u ground %u tears %u kills %u deaths %u unbalanced %u\n", (unsigned)dmg_in, (unsigned)dmg_ground, (unsigned)tears, (unsigned)kills, (unsigned)deaths, (unsigned)dmg_bad);
    Serial0.printf("STACK free %u words\n", (unsigned)uxTaskGetStackHighWaterMark(NULL));
    Serial0.printf("HEAP free %u\n", (unsigned)ESP.getFreeHeap());
    Serial0.printf("DONE\n");
    while (1) delay(1000);
  }
#endif
}
#endif
