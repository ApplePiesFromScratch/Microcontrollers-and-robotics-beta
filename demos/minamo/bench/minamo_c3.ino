// MINAMO (水面, the water's face), for the ESP32-C3 SuperMini + 2.0" ST7789 (320 x 240 across). A film of 100 seconds, then it loops.
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
// ---------------------------------------------------------------- every type first (minamo.h)
/* MINAMO (水面, the water's face): a pond that remembers.
   The surface is two readings of height, h(s-1) and h(s), whole numbers. One step is the leapfrog:
       h(s+1) = 2 h(s) - h(s-1) + F(h(s)) + S(s)
   F reads only h(s) and S only the step's number, so the same line read the other way is the step back:
       h(s-1) = 2 h(s) - h(s+1) + F(h(s)) + S(s)
   Whole numbers make that exact, bit for bit (in floats each step rounds: on this film the way back comes within a thousandth
   in single floats, 3e-12 in doubles, but not home, so it could not be checked word for word). So the film runs the pond forward
   for fifty seconds (a leaf, koi, rain, a storm) and then back for fifty: the rain leaves the water upward, the rings close
   into their drops, the leaf leaps back into the air, and the last latch is the first: flat water, checked word for word.
   Light is read out of the surface, never stored: the floor through the water (bent by the slope), the sky in it (the slope
   again, twice as far), the floor's light gathered by the curve of the surface (caustics). Koi are tested where the bent ray
   lands, so they ripple with the water. One painting of floor, one of sky; dusk, night, storm and dawn are palettes.
   No float, no clock: a latch is a position on the film, and the step is read from the position. */
#include <stdint.h>
#include <string.h>
#ifndef FASTF
#define FASTF
#endif
#define SW 320
#define SH 240
#define GW 160                      /* the surface: cells across and down; a cell is 2 x 2 dots seen from above */
#define GH 120
#define NCELL (GW * GH)
#ifndef PROBE_SHADOW
#define PROBE_SHADOW
#endif
#ifndef PROBE_SLOW
#define PROBE_SLOW
#endif
#define BAND 8                      /* dots a strip */
#define NBAND (SH / BAND)
#define PEAKP 1240                  /* latches the pond runs forward (two steps a latch) */
#define HOLDP 16                    /* latches held at the storm's height */
#define TAILP 4
#define FILM (2 * PEAKP + HOLDP + TAILP)   /* 2500 latches: 100 s at 25 a second */
#define SMAX (2 * PEAKP)
#define T_LEAF 360                  /* the step the leaf lands */
#define T_KOI0 300                  /* koi start to come in */
#define T_KOI1 900                  /* koi fully in */
#define T_RAIN0 1260                /* rain: starts, full, the step it is heaviest */
#define T_RAIN1 2300
#define NKOI 5
#define HORIZON 92                       /* the horizon's dot row in the low shots */
typedef struct { uint8_t c[8][3]; } Key;     /* a light: sun, ambient, zenith, horizon, glow, disc, water, leaf */
typedef struct { int32_t x, y, hx, hy, x0, y0, x1, y1, ph; int8_t wav[64]; uint8_t type; } Koi;   /* head (1/16 dot), heading (of 1024), box */
typedef struct { int16_t p0, p1; uint8_t kind; } Shot;  /* 0 above, 1 low */
// ---------------------------------------------------------------- the mouth: FAREY's queue, as in HIGH NOON (it runs on this board)
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
// ---------------------------------------------------------------- MINAMO (minamo.h)
/* ---------------------------------------------------------------- the state */
static int16_t WA[NCELL], WB[NCELL];                 /* the two readings of the surface */
static int16_t *wold = WA, *wnew = WB;         /* h(s-1), h(s) */
static int32_t sim_s, film_p, frame_n, play = 1;
static uint8_t FLOOR[SW * SH];                 /* the pond floor: material * 8 + shade (bit 7: in shadow, set only while drawing) */
static uint8_t SKY[NCELL];                        /* the sky, half size: 0..127 height + 32 * glow; 128.. things */
static int sky_kind = -1;
static uint16_t FLT[32][256];                  /* (light level, floor index) -> colour, this latch */
static uint32_t SKT[256];                      /* the sky's colour as the water gives it back from above: a sixth */
static uint32_t SKS[256];                      /* sky index -> colour spread for adding, this latch */
static uint16_t STRIP[2][SW * BAND] __attribute__((aligned(4)));
static int32_t RING[257][2];                   /* 256 headings, length 1024, grown by halving; [256] = [0] */
static uint8_t KX[GW], KY[GH];                 /* wave speed toward the banks (shallows) */
static const int16_t DK[49] = {                /* a drop's kick: a narrow bell down less a wide bell up, 7 x 7; worked out on the desk to sum to -3 (the middle takes the rest) */
  34, 64, 88, 95, 88, 64, 34, 64, 100, 78, 39, 78, 100, 64, 88, 78, -162, -381, -162, 78, 88, 95, 39, -381, -743, -381, 39, 95,
  88, 78, -162, -381, -162, 78, 88, 64, 100, 78, 39, 78, 100, 64, 34, 64, 88, 95, 88, 64, 34 };
static uint32_t PRINT[PEAKP + 1];              /* the pond's print at each latch going forward: the way back must match */
static uint32_t homes, home_bad, mirror_ok, mirror_bad, steps_n, held_n;
static uint32_t cyc_sim, cyc_draw, cyc_band, cyc_over, cyc_tab;
static Koi KO[NKOI]; static int n_koi_on;
static Key LIT;                                /* this latch's light */
static int32_t fade_a;                         /* 0..256: 256 is no fade */
static int32_t flash_a;                        /* lightning: 0..256 */
static int shot_i = -1, cut_now;
/* ---------------------------------------------------------------- whole-number tools */
static uint32_t hsh(uint32_t a, uint32_t b) { uint32_t x = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u; x ^= x >> 15; x *= 0x2C1B3C6Du; x ^= x >> 12; x *= 0x297A2D39u; x ^= x >> 15; return x; }
static uint32_t root(uint32_t v) { uint32_t r = 0, b = 1u << 30; while (b > v) b >>= 2; while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; } return r; }
static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }
static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static int32_t imin(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t imax(int32_t a, int32_t b) { return a > b ? a : b; }
static void grow_ring(void) {                  /* a quarter from (1,0) and (0,1): each mid-heading the sum of its neighbours, set to 1024 by the counted root */
  RING[0][0] = 1024; RING[0][1] = 0; RING[64][0] = 0; RING[64][1] = 1024;
  for (int st = 32; st >= 1; st >>= 1) for (int a = st; a < 64; a += 2 * st) {
    int32_t mx = RING[a - st][0] + RING[a + st][0], my = RING[a - st][1] + RING[a + st][1]; uint32_t L = root((uint32_t)(mx * mx + my * my));
    RING[a][0] = (int32_t)(((int64_t)mx * 1024 + L / 2) / L); RING[a][1] = (int32_t)(((int64_t)my * 1024 + L / 2) / L); }
  for (int a = 64; a < 257; a++) { RING[a][0] = -RING[(a - 64) & 255][1]; RING[a][1] = RING[(a - 64) & 255][0]; } }
static int32_t cosq(uint32_t ph) { uint32_t i = (ph >> 8) & 255, f = ph & 255; return RING[i][0] + (((RING[i + 1][0] - RING[i][0]) * (int32_t)f) >> 8); }   /* ph: 65536 a turn */
static int32_t sinq(uint32_t ph) { uint32_t i = (ph >> 8) & 255, f = ph & 255; return RING[i][1] + (((RING[i + 1][1] - RING[i][1]) * (int32_t)f) >> 8); }
static int32_t ease(int32_t k, int32_t n) { if (k <= 0) return 0; if (k >= n) return 4096; int64_t u = ((int64_t)k << 12) / n; return (int32_t)((u * u * (3 * 4096 - 2 * u)) >> 24); }   /* 3u^2 - 2u^3 */
static int32_t vnoise(int32_t x, int32_t y, int sh, uint32_t k) {   /* smooth value noise 0..255 over 2^sh cells */
  int32_t gx = x >> sh, gy = y >> sh, fx = ((x & ((1 << sh) - 1)) << 8) >> sh, fy = ((y & ((1 << sh) - 1)) << 8) >> sh;
  int32_t a = hsh((uint32_t)gx * 7919u + k, (uint32_t)gy) & 255, b = hsh((uint32_t)(gx + 1) * 7919u + k, (uint32_t)gy) & 255;
  int32_t c = hsh((uint32_t)gx * 7919u + k, (uint32_t)(gy + 1)) & 255, d = hsh((uint32_t)(gx + 1) * 7919u + k, (uint32_t)(gy + 1)) & 255;
  fx = (fx * fx * (768 - 2 * fx)) >> 16; fy = (fy * fy * (768 - 2 * fy)) >> 16;
  int32_t t = a * 256 + (b - a) * fx, u = c * 256 + (d - c) * fx; return (t * 256 + (u - t) * fy) >> 16; }
static int32_t pang(int32_t x, int32_t y) {    /* a heading without a root or a table: the diamond heading 0..4095, same order as the true one */
  int32_t ax = iabs(x), ay = iabs(y), s = ax + ay; if (!s) return 0; int32_t t = ay * 1024 / s;
  return x >= 0 ? (y >= 0 ? t : 4096 - t) : (y >= 0 ? 2048 - t : 2048 + t); }
static uint16_t rgb(int r, int g, int b) { return (uint16_t)(((clampi(r, 0, 255) & 0xF8) << 8) | ((clampi(g, 0, 255) & 0xFC) << 3) | (clampi(b, 0, 255) >> 3)); }
static inline uint32_t spread(uint16_t c) { return (c | ((uint32_t)c << 16)) & 0x07E0F81Fu; }
static inline uint16_t unspread(uint32_t S) { return (uint16_t)((S & 0xF81Fu) | ((S >> 16) & 0x07E0u)); }
static inline uint16_t addS(uint16_t d, uint32_t S) {   /* light added, each channel held at its top */
  uint32_t D = spread(d) + S;
  if (D & 0x08010020u) { if (D & 0x20u) D = (D & ~0x3Fu) | 0x1Fu; if (D & 0x10000u) D = (D & ~0x1F800u) | 0xF800u; if (D & 0x08000000u) D = (D & ~0x0FE00000u) | 0x07E00000u; }
  return unspread(D); }
static inline uint32_t scaleS(uint32_t S, uint32_t a) { return ((S * a) >> 5) & 0x07E0F81Fu; }   /* a of 32 (a <= 32) */
/* ---------------------------------------------------------------- the film: a position, and the step read from it */
static int32_t step_at(int32_t p) {
  if (p < PEAKP) return 2 * p;
  if (p < PEAKP + HOLDP) return SMAX;
  if (p < 2 * PEAKP + HOLDP) return 2 * (2 * PEAKP + HOLDP - p);
  return 0; }
static const Shot SHOTS[] = { { 0, 150, 1 }, { 150, 430, 0 }, { 430, 570, 1 }, { 570, 800, 0 }, { 800, 950, 1 }, { 950, 1100, 0 }, { 1100, 1256, 1 },
                              { 1256, 1420, 1 }, { 1420, 1700, 0 }, { 1700, 1900, 1 }, { 1900, 2350, 0 }, { 2350, FILM, 1 } };
#define NSHOTS ((int)(sizeof SHOTS / sizeof SHOTS[0]))
static int shot_of(int32_t p) { for (int i = 0; i < NSHOTS; i++) if (p < SHOTS[i].p1) return i; return NSHOTS - 1; }
/* ---------------------------------------------------------------- what falls on the water: read from the step's number alone */
static int64_t rain_cum(int32_t t) {           /* drops landed before step t (in 1/256 drop): a rate rising from nothing to 3 a step, then held */
  if (t <= T_RAIN0) return 0;
  int64_t R = 768, L = T_RAIN1 - T_RAIN0;
  if (t <= T_RAIN1) { int64_t d = t - T_RAIN0; return R * d * d / (2 * L); }
  return R * L / 2 + R * (t - T_RAIN1); }
static int32_t drops_from(int32_t t) { return (int32_t)(rain_cum(t) >> 8); }   /* drop i lands at the step t with drops_from(t) <= i < drops_from(t+1) */
static void drop_place(int32_t i, int32_t *x, int32_t *y, int32_t *a) { uint32_t h = hsh(0xD50Fu, (uint32_t)i);
  *x = 6 + (int32_t)(h % (GW - 12)); *y = 6 + (int32_t)((h >> 10) % (GH - 12)); *a = 30 + (int32_t)((h >> 20) % 60); }
static void koi_at(int k, int32_t t, int32_t *x, int32_t *y) {   /* a koi's head in 1/256 cell: two loops summed, and the way in */
  static const int32_t P[NKOI][8] = { { 80, 60, 46, 30, 33, 12, 200, 3 }, { 76, 64, 40, 34, -27, 15, 9000, 11 }, { 84, 58, 50, 26, 23, -10, 31000, 5 },
                                      { 82, 62, 36, 40, -38, 19, 47000, 17 }, { 78, 60, 52, 22, 30, -14, 18000, 7 } };
  const int32_t *q = P[k]; uint32_t a = (uint32_t)(q[6] + t * q[4]), b = (uint32_t)(q[6] * 3 + t * q[5]);
  int32_t px = q[0] * 256 + q[2] * cosq(a) / 4 + q[7] * cosq(b) / 4, py = q[1] * 256 + q[3] * sinq(a) / 4 + q[7] * sinq(b + 9000) / 4;
  int32_t e = 4096 - ease(t - T_KOI0 - k * 70, T_KOI1 - T_KOI0);   /* in from outside the pond, each from its own side */
  uint32_t side = (uint32_t)(k * 13107 + 4000); px += (int32_t)((int64_t)cosq(side) * 150 * e >> 14); py += (int32_t)((int64_t)sinq(side) * 150 * e >> 14);
  *x = px; *y = py; }
#define LEAF_X (104 * 256)
#define LEAF_Y (46 * 256)
static void leaf_at(int32_t t, int32_t *x, int32_t *y, int32_t *rot, int32_t *hgt) {   /* the leaf: falling until T_LEAF, then afloat, drifting */
  int32_t d = t - T_LEAF;
  if (d < 0) { int32_t u = -d; *hgt = u; *x = LEAF_X + u * 90 + (sinq((uint32_t)u * 1400) * u >> 6); *y = LEAF_Y - u * 50; *rot = 1800 + u * 900; return; }
  *hgt = 0; *x = LEAF_X - d * 6 + (sinq((uint32_t)d * 40) >> 2); *y = LEAF_Y + d * 4; *rot = 1800 - d * 9; }
/* ---------------------------------------------------------------- the step: one function, both ways */
static void kick(int16_t *X, int32_t cx, int32_t cy, int32_t a) {   /* a drop: the 7 x 7 kick scaled, its middle set so the whole sums to zero */
  if (cx < 4 || cy < 4 || cx >= GW - 4 || cy >= GH - 4) return;
  int32_t sum = 0;
  for (int j = 0; j < 49; j++) { if (j == 24) continue; int32_t v = DK[j] * a / 256; sum += v; int i = (cy + j / 7 - 3) * GW + cx + j % 7 - 3; X[i] = (int16_t)(X[i] + v); }
  int i = cy * GW + cx; X[i] = (int16_t)(X[i] - sum); }
static void sources(int16_t *X, int32_t t) {   /* S(t): every push on the water at step t, from t alone */
  for (int32_t i = drops_from(t), e = drops_from(t + 1); i < e; i++) { int32_t x, y, a; drop_place(i, &x, &y, &a); kick(X, x, y, a); }
  if (t == T_LEAF) kick(X, LEAF_X >> 8, LEAF_Y >> 8, 220);
  if (t > T_LEAF && t < T_LEAF + 6) { int32_t x, y, r, h; leaf_at(t, &x, &y, &r, &h); kick(X, x >> 8, y >> 8, 30); }
  if (t > T_KOI0) for (int k = 0; k < NKOI; k++) {   /* a koi's wake: a push ahead and a pull behind, summing to zero */
    int32_t x0, y0, x1, y1; koi_at(k, t, &x1, &y1); koi_at(k, t - 8, &x0, &y0); int32_t dx = x1 - x0, dy = y1 - y0, L = (int32_t)root((uint32_t)(dx * dx + dy * dy)) + 1;
    int32_t fx = (x1 + dx * 512 / L) >> 8, fy = (y1 + dy * 512 / L) >> 8, bx = (x1 - dx * 1024 / L) >> 8, by = (y1 - dy * 1024 / L) >> 8;
    if ((t & 3) == (k & 3)) { kick(X, fx, fy, -14); kick(X, bx, by, 14); } } }   /* every fourth step, each koi on its own: a smooth push, never a single cell */
static FASTF void leap(int16_t *X, const int16_t *Y, int32_t t) {   /* X <- 2Y - X + F(Y) + S(t): forward with X = h(s-1), back with X = h(s+1) */
  for (int y = 1; y < GH - 1; y++) { const int16_t *r = Y + y * GW; int16_t *o = X + y * GW; int ky = KY[y];
    int32_t A0 = r[-GW] + r[GW], C0 = r[0], A1 = r[1 - GW] + r[1 + GW], C1 = r[1];   /* columns rolled along: up + down, and the middle */
    for (int x = 1; x < GW - 1; x++) { int32_t A2 = r[x + 1 - GW] + r[x + 1 + GW], C2 = r[x + 1];
      int32_t L = 4 * (A1 + C0 + C2) + A0 + A2 - 20 * C1;   /* the nine-point Laplacian, six times over */
      int32_t k = KX[x] < ky ? KX[x] : ky, q = L * k; q = (q + ((q >> 31) & 2047)) >> 11;   /* toward zero: no drift either way */
      o[x] = (int16_t)(2 * C1 - o[x] + q); A0 = A1; C0 = C1; A1 = A2; C1 = C2; } }
  sources(X, t); steps_n++; }
static void step_fwd(void) { leap(wold, wnew, sim_s); int16_t *t = wold; wold = wnew; wnew = t; sim_s++; }
static void step_back(void) { leap(wnew, wold, sim_s - 1); int16_t *t = wold; wold = wnew; wnew = t; sim_s--; }
static uint32_t pond_print(void) { uint32_t h = 2166136261u; for (int i = 0; i < NCELL; i++) h = (h ^ (uint16_t)wnew[i]) * 16777619u; for (int i = 0; i < NCELL; i += 7) h = (h ^ (uint16_t)wold[i]) * 16777619u; return h; }
static int pond_flat(void) { for (int i = 0; i < NCELL; i++) if (WA[i] | WB[i]) return 0; return 1; }
/* ---------------------------------------------------------------- the light, by position on the film (iro-gae: one painting, five lights) */
static const Key KEYS[5] = {
  /* sun        ambient       zenith        horizon        glow           disc           water         leaf */
  { { { 255, 170, 100 }, { 70, 60, 80 }, { 40, 50, 110 }, { 255, 150, 90 }, { 255, 120, 60 }, { 255, 160, 90 }, { 20, 40, 45 }, { 230, 70, 30 } } },   /* dusk */
  { { { 120, 150, 210 }, { 22, 30, 52 }, { 6, 10, 30 }, { 30, 44, 84 }, { 90, 120, 190 }, { 250, 250, 236 }, { 6, 16, 24 }, { 150, 40, 30 } } },     /* night */
  { { { 90, 100, 150 }, { 16, 20, 36 }, { 10, 12, 22 }, { 40, 44, 66 }, { 70, 80, 130 }, { 200, 205, 220 }, { 6, 12, 18 }, { 120, 34, 30 } } },     /* storm */
  { { { 170, 150, 200 }, { 40, 44, 74 }, { 30, 36, 80 }, { 160, 120, 160 }, { 200, 130, 170 }, { 230, 200, 220 }, { 14, 28, 40 }, { 190, 60, 40 } } },  /* before dawn */
  { { { 210, 180, 140 }, { 62, 66, 78 }, { 80, 120, 190 }, { 235, 190, 150 }, { 240, 180, 110 }, { 255, 230, 180 }, { 24, 46, 50 }, { 240, 90, 40 } } } };   /* dawn */
static void light_at(int32_t p) {
  static const int16_t KP[6] = { 0, 480, 820, 1300, 1800, 2200 };
  static const uint8_t KI[6] = { 0, 0, 1, 2, 3, 4 };
  int i = 0; while (i < 4 && p >= KP[i + 1]) i++;
  const Key *a = &KEYS[KI[i]], *b = &KEYS[KI[i + 1]]; int32_t e = i >= 5 ? 4096 : ease(p - KP[i], KP[i + 1] - KP[i]);
  if (p >= KP[5]) { a = b = &KEYS[4]; e = 0; }
  for (int c = 0; c < 8; c++) for (int k = 0; k < 3; k++) LIT.c[c][k] = (uint8_t)(a->c[c][k] + (((b->c[c][k] - a->c[c][k]) * e) >> 12)); }
static const uint8_t B4[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
static uint8_t TM[1024];                       /* the film's curve: linear light 0..1023 (256 = paper white) to 0..255, a soft shoulder */
static void grow_tm(void) { for (int v = 0; v < 1024; v++) { int32_t o = v <= 180 ? v : 180 + (int32_t)((int64_t)(v - 180) * 75 * 1024 / ((v - 180) * 4 + 75 * 1024 / 75)); TM[v] = (uint8_t)clampi(o, 0, 255); } }
/* floor materials: albedo (of 255) */
static const uint8_t ALB[16][3] = { { 196, 172, 128 }, { 120, 104, 80 }, { 150, 150, 146 }, { 150, 112, 80 }, { 96, 108, 124 }, { 220, 214, 200 }, { 70, 120, 50 }, { 34, 30, 26 },
                                    { 170, 60, 30 }, { 255, 70, 30 }, { 250, 246, 236 }, { 24, 22, 26 }, { 255, 196, 60 }, { 220, 210, 200 }, { 10, 10, 14 }, { 30, 70, 36 } };
static void tables(void) {                     /* this latch's colours: floor by light level, sky for adding */
  int32_t fd = fade_a, fl = flash_a;
  for (int m = 0; m < 16; m++) for (int sh = 0; sh < 8; sh++) { int idx = m * 8 + sh, koi = m >= 9 && m <= 14; int32_t ab = 50 + sh * 30;   /* shade: 50..260 of 256 */
    for (int lv = 0; lv < 32; lv++) { int32_t ch[3];
      for (int k = 0; k < 3; k++) { int32_t light = LIT.c[1][k] * 3 / 2 + LIT.c[0][k] * lv / 20 + fl * 2;   /* ambient + sun by how much the surface gathered it */
        int32_t v = ALB[m][k] * ab / 256 * light / 255;               /* linear, 256 white */
        int32_t fog = koi ? 36 : 96; v = (v * (256 - fog) + LIT.c[6][k] * 3 * fog) >> 8;   /* the water between: deeper for the floor than for a koi */
        ch[k] = TM[clampi(v * fd >> 8, 0, 1023)]; }
      FLT[lv][idx] = rgb(ch[0], ch[1], ch[2]); }
    { int32_t ch[3]; for (int k = 0; k < 3; k++) { int32_t light = LIT.c[1][k] * 3 / 2 + LIT.c[0][k] / 6 + fl; int32_t v = ALB[m][k] * ab / 256 * light / 255; v = (v * 160 + LIT.c[6][k] * 3 * 96) >> 8; ch[k] = TM[clampi(v * fd >> 8, 0, 1023)]; }
      uint16_t c = rgb(ch[0], ch[1], ch[2]); for (int lv = 0; lv < 32; lv++) FLT[lv][idx | 128] = c; } }
  for (int i = 0; i < 256; i++) { int32_t r, g, b;
    if (i < 128) { int h = i & 31, gl = i >> 5;               /* the sky's height, and the glow round the light */
      r = LIT.c[3][0] + ((LIT.c[2][0] - LIT.c[3][0]) * h) / 31 + LIT.c[4][0] * gl * gl / 12; g = LIT.c[3][1] + ((LIT.c[2][1] - LIT.c[3][1]) * h) / 31 + LIT.c[4][1] * gl * gl / 12; b = LIT.c[3][2] + ((LIT.c[2][2] - LIT.c[3][2]) * h) / 31 + LIT.c[4][2] * gl * gl / 12; }
    else { int m = (i - 128) >> 3, sh = i & 7;
      switch (m) {
        case 0: r = LIT.c[5][0] * (5 + sh) / 12; g = LIT.c[5][1] * (5 + sh) / 12; b = LIT.c[5][2] * (5 + sh) / 12; break;      /* the disc */
        case 1: r = (LIT.c[3][0] + LIT.c[4][0]) * (4 + sh) / 24; g = (LIT.c[3][1] + LIT.c[4][1]) * (4 + sh) / 24; b = (LIT.c[3][2] + LIT.c[4][2]) * (4 + sh) / 24; break;   /* cloud */
        case 2: r = LIT.c[2][0] / 3 + LIT.c[1][0] / 3 + sh * 3; g = LIT.c[2][1] / 3 + LIT.c[1][1] / 3 + sh * 3; b = LIT.c[2][2] / 3 + LIT.c[1][2] / 3 + sh * 4; break;   /* far trees */
        case 3: r = LIT.c[1][0] / 3 + sh * 2; g = LIT.c[1][1] / 3 + sh * 3; b = LIT.c[1][2] / 3 + sh * 2; break;               /* near foliage */
        case 4: r = LIT.c[7][0] * (3 + sh) / 12; g = LIT.c[7][1] * (3 + sh) / 12; b = LIT.c[7][2] * (3 + sh) / 12; break;      /* maple leaves */
        case 5: r = LIT.c[1][0] / 4 + sh; g = LIT.c[1][1] / 4 + sh; b = LIT.c[1][2] / 4 + sh; break;                           /* branch */
        case 6: { int32_t n = (256 - LIT.c[2][2] * 2); n = clampi(n, 0, 256); r = LIT.c[2][0] + (200 * sh / 7) * n / 256; g = LIT.c[2][1] + (205 * sh / 7) * n / 256; b = LIT.c[2][2] + (230 * sh / 7) * n / 256; break; }   /* stars: only where the sky is dark */
        case 7: r = 255; g = 250; b = 255; break;                                                                                 /* lightning */
        default: r = g = b = 0; } }
    r = (r + fl) * fd >> 8; g = (g + fl) * fd >> 8; b = (b + fl * 5 / 4) * fd >> 8;
    SKS[i] = spread(rgb(TM[clampi(r, 0, 1023)], TM[clampi(g, 0, 1023)], TM[clampi(b, 0, 1023)])); SKT[i] = scaleS(SKS[i], 6); } }
/* ---------------------------------------------------------------- the paintings: the floor once at boot, the sky at each cut */
static void fput(int x, int y, int m, int sh) { if (x >= 0 && y >= 0 && x < SW && y < SH) FLOOR[y * SW + x] = (uint8_t)(m * 8 + clampi(sh, 0, 7)); }
static int32_t LR[64];                         /* a maple leaf's reach by heading (64 of the diamond turn), of 256 */
static void grow_leaf(void) { for (int a = 0; a < 64; a++) { int32_t ph = a * 5 * 1024 / 64 * 64; int32_t c = cosq((uint32_t)(ph / 2)); c = iabs(c);   /* five lobes, pointed */
    int32_t pt = c; LR[a] = 96 + (160 * pt >> 10); if (a > 26 && a < 38) LR[a] = LR[a] * 3 / 4; } }
static int leaf_in(int32_t dx, int32_t dy, int32_t R, int32_t rot, int32_t *sh) {   /* is (dx, dy) inside a leaf of reach R (dots) turned by rot (of 4096)? */
  int32_t d2 = dx * dx + dy * dy; if (d2 > R * R) return 0; int32_t a = ((pang(dx, dy) - rot) & 4095) >> 6, r = LR[a] * R >> 8; if (d2 > r * r) return 0;
  *sh = 7 - (int32_t)(d2 * 4 / (r * r + 1)) - (((pang(dx, dy) - rot + 32) & 819) < 64 ? 2 : 0); return 1; }   /* veins: darker along five spokes */
static void paint_floor(void) {
  for (int y = 0; y < SH; y++) for (int x = 0; x < SW; x++) { int32_t n = vnoise(x, y, 5, 11), n2 = vnoise(x, y, 3, 12); int m = n > 150 ? 1 : 0; fput(x, y, m, 3 + (n2 >> 6) - (n > 190)); }
  for (int gy = -1; gy <= SH / 12; gy++) for (int gx = -1; gx <= SW / 12; gx++) {   /* pebbles: one a cell of a jittered grid, more where the noise says */
    uint32_t h = hsh((uint32_t)(gx * 131 + 7), (uint32_t)(gy * 71 + 3)); int32_t cx = gx * 12 + 6 + (int32_t)(h % 9) - 4, cy = gy * 12 + 6 + (int32_t)((h >> 4) % 9) - 4;
    int32_t dens = vnoise(cx, cy, 6, 21); if ((int32_t)((h >> 8) & 255) > dens + 40) continue;
    int32_t rx = 3 + (int32_t)((h >> 16) % 5), ry = rx * (6 + (int32_t)((h >> 20) % 5)) / 10, rot = (int32_t)((h >> 24) & 255), m = 2 + (int32_t)((h >> 12) % 4);
    int32_t ca = RING[rot][0], sa = RING[rot][1], R = rx + 2;
    for (int y = cy - R; y <= cy + R; y++) for (int x = cx - R; x <= cx + R; x++) { int32_t dx = x - cx, dy = y - cy, u = (dx * ca + dy * sa) >> 10, v = (dy * ca - dx * sa) >> 10;
      int32_t q = u * u * 256 / (rx * rx) + v * v * 256 / (ry * ry);
      if (q <= 256) { int32_t lit = (-(dx + dy) * 3) / (rx + 1); int32_t sh = 4 + lit - (q > 200 ? 1 : 0); int mm = m;
          if (vnoise(x, y, 4, 33) > 185 && dy < 0) mm = 6;            /* moss on the tops */
          fput(x, y, mm, sh); }
      else if (q <= 400 && FLOOR[clampi(y, 0, SH - 1) * SW + clampi(x, 0, SW - 1)] < 16) fput(x, y, 7, 3 + (q - 256) / 50); } }   /* where it sits in the silt */
  { int32_t cx = 268, cy = 46;                  /* a big stone, its top in moss */
    for (int y = cy - 40; y <= cy + 40; y++) for (int x = cx - 56; x <= cx + 56; x++) { int32_t dx = x - cx, dy = y - cy; int32_t q = dx * dx * 256 / (52 * 52) + dy * dy * 256 / (36 * 36) - (vnoise(x, y, 3, 41) - 128) / 2;
      if (q <= 256) { int32_t sh = 4 - (dx + dy) / 18 + (vnoise(x, y, 2, 42) >> 7); fput(x, y, (vnoise(x, y, 4, 43) > 140 && dy < 6) ? 6 : 4, sh); }
      else if (q < 330) fput(x, y, 7, 2); } }
  for (int k = 0; k < 5; k++) { uint32_t h = hsh(0x1EAFu, (uint32_t)k); int32_t cx = 30 + (int32_t)(h % 260), cy = 30 + (int32_t)((h >> 9) % 180), R = 13 + (int32_t)((h >> 18) % 6), rot = (int32_t)((h >> 22) & 4095);   /* old leaves on the floor */
    for (int y = cy - R; y <= cy + R; y++) for (int x = cx - R; x <= cx + R; x++) { int32_t sh; if (leaf_in(x - cx, y - cy, R, rot, &sh)) fput(x, y, 3, sh / 2 + 1); } }
  }
static void sput(int x, int y, int v) { if (x >= 0 && y >= 0 && x < GW && y < GH) SKY[y * GW + x] = (uint8_t)v; }
static void branch(int32_t x, int32_t y, int32_t hd, int32_t len, int32_t w, int depth, uint32_t seed) {   /* TRAILER's heading tree: a piece, then two turned pieces; leaves at the tips */
  int32_t ex = x + RING[hd & 255][0] * len / 1024, ey = y + RING[hd & 255][1] * len / 1024;
  for (int s = 0; s <= len; s++) { int32_t px = x + (ex - x) * s / (len + 1), py = y + (ey - y) * s / (len + 1); for (int dy = -w / 2; dy <= w / 2; dy++) for (int dx = -w / 2; dx <= w / 2; dx++) sput(px + dx, py + dy, 128 + 5 * 8 + 2 + (dx < 0)); }
  if (depth == 0 || len < 4) { for (int k = 0; k < 7; k++) { uint32_t h = hsh(seed, (uint32_t)k); int32_t lx = ex + (int32_t)(h % 9) - 4, ly = ey + (int32_t)((h >> 4) % 9) - 4, r = 2 + (int32_t)((h >> 8) % 2);
      for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) if (dx * dx + dy * dy <= r * r) sput(lx + dx, ly + dy, 128 + 4 * 8 + clampi(5 - dy - (dx > 0), 0, 7)); } return; }
  uint32_t h = hsh(seed, 99); branch(ex, ey, hd - 14 - (int32_t)(h % 14), len * 7 / 10, imax(1, w - 1), depth - 1, seed * 3 + 1); branch(ex, ey, hd + 12 + (int32_t)((h >> 8) % 16), len * 6 / 10, imax(1, w - 1), depth - 1, seed * 5 + 2); }
static void paint_sky(int kind) {              /* 0: the sky above the pond (seen in it from above); 1: the far bank from low over the water */
  sky_kind = kind;
  if (kind == 0) { int32_t lx = 118, ly = 30;
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) { int32_t d = (int32_t)root((uint32_t)((x - lx) * (x - lx) + (y - ly) * (y - ly))); int32_t nz = B4[y & 3][x & 3]; int h = clampi((d * 16 + nz * 3) / 48, 0, 31), gl = clampi((27 * 16 - d * 16 + nz * 9) / 144, 0, 3);
      SKY[y * GW + x] = (uint8_t)(h + 32 * gl); if (h > 16 && (hsh((uint32_t)x, (uint32_t)y * 31u) & 255) < 3) SKY[y * GW + x] = (uint8_t)(128 + 6 * 8 + (hsh((uint32_t)y, (uint32_t)x) & 7)); }
    for (int y = ly - 6; y <= ly + 6; y++) for (int x = lx - 6; x <= lx + 6; x++) { int32_t d2 = (x - lx) * (x - lx) + (y - ly) * (y - ly); if (d2 <= 30) sput(x, y, 128 + clampi(7 - d2 / 6, 0, 7)); }
    branch(-6, 8, 22, 40, 3, 4, 7); branch(150, -6, 100, 26, 2, 3, 31); }
  else { int hc = HORIZON / 2; int32_t lx = 112, ly = 30;
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) { int32_t dy = y - ly, dx = x - lx, d = (int32_t)root((uint32_t)(dx * dx + dy * dy * 2)); int32_t nz = B4[y & 3][x & 3]; int h = clampi(((hc - y) * 31 * 16 + nz * hc) / (hc * 16), 0, 31), gl = clampi((30 * 16 - d * 16 + nz * 10) / 160, 0, 3);
      if (y > hc) h = 0; SKY[y * GW + x] = (uint8_t)(h + 32 * gl);
      if (h > 12 && (hsh((uint32_t)x * 3u, (uint32_t)y) & 511) < 3) SKY[y * GW + x] = (uint8_t)(128 + 6 * 8 + (hsh((uint32_t)y, (uint32_t)x) & 7)); }
    for (int k = 0; k < 4; k++) { int32_t cy = 8 + k * 6, cx = 30 + k * 34, w = 30 + k * 6;   /* long clouds, lit from below */
      for (int y = cy - 3; y <= cy + 3; y++) for (int x = cx - w; x <= cx + w; x++) { int32_t u = (x - cx) * 256 / w, v = (y - cy) * 256 / 4; int32_t q = u * u / 256 + v * v / 64 + (vnoise(x * 2, y * 5, 3, 51 + k) - 128) * 3;
        if (q > 200 || ((hsh((uint32_t)x, (uint32_t)y + 999u) & 255) < (uint32_t)clampi(q - 120, 0, 255) * 3)) continue; sput(x, y, 128 + 8 + clampi(3 + (y - cy) + (vnoise(x, y, 2, 55) >> 6), 0, 7)); } }
    for (int y = ly - 5; y <= ly + 5; y++) for (int x = lx - 5; x <= lx + 5; x++) { int32_t d2 = (x - lx) * (x - lx) + (y - ly) * (y - ly); if (d2 <= 20) sput(x, y, 128 + clampi(7 - d2 / 4, 0, 7)); }
    for (int x = 0; x < GW; x++) { int32_t top = hc - 4 - (vnoise(x, 0, 4, 61) * 9 >> 8) - (vnoise(x, 0, 2, 62) * 3 >> 8); for (int y = top; y <= hc; y++) sput(x, y, 128 + 2 * 8 + clampi((y - top) * 7 / 10 + (int32_t)(hsh((uint32_t)x, (uint32_t)y) & 1), 0, 7)); }
    for (int y = 0; y <= hc; y++) { int32_t w = 18 + (vnoise(0, y, 3, 71) * 20 >> 8) - (y < 10 ? 10 - y : 0); for (int x = 0; x < w; x++) if (vnoise(x, y, 2, 72) > 70 || x < w - 6) sput(x, y, 128 + 3 * 8 + clampi((vnoise(x, y, 2, 73) >> 5) - 1, 0, 7)); }   /* a near tree, left */
    for (int x = GW - 22; x < GW; x++) { int32_t top = hc - 12 - (vnoise(x, 1, 2, 81) * 10 >> 8); for (int y = top; y <= hc; y++) if ((hsh((uint32_t)x, 77) & 3) || y > top + 3) sput(x, y, 128 + 3 * 8 + 1); } } }   /* reeds, right */
static void lightning_paint(uint32_t seed) {   /* JUMON's midpoint walk, drawn into the sky (so the water shows it too) */
  int32_t BX[33], BY[33]; BX[0] = 70 * 16; BY[0] = 0; BX[32] = 92 * 16; BY[32] = (HORIZON / 2 - 2) * 16;
  for (int st = 32; st > 1; st >>= 1) for (int i = 0; i < 32; i += st) { int m = i + st / 2; int32_t L = iabs(BY[i + st] - BY[i]) + iabs(BX[i + st] - BX[i]); int32_t o = (int32_t)(hsh(seed, (uint32_t)(m * 64 + st)) % 1000) - 500;
      BX[m] = (BX[i] + BX[i + st]) / 2 + (int32_t)((int64_t)L * o / 2200); BY[m] = (BY[i] + BY[i + st]) / 2; }
  for (int i = 0; i < 32; i++) for (int s = 0; s < 8; s++) { int32_t x = (BX[i] + (BX[i + 1] - BX[i]) * s / 8) >> 4, y = (BY[i] + (BY[i + 1] - BY[i]) * s / 8) >> 4; sput(x, y, 128 + 7 * 8); if (i < 20) sput(x + 1, y, 128 + 7 * 8); } }
/* ---------------------------------------------------------------- koi: tested where the bent ray lands */
static int8_t KW[64], KFIN[64], KTAIL[64];     /* half-widths by distance behind the nose (2 dots): body, side fins, tail */
static int32_t KRW[64];                        /* 6 over the body's full half-width in dots, of 65536: the shade across, without a divide */
static uint8_t KPAT[NKOI][16][2];              /* each koi's colours down its two sides */
static void grow_koi(void) {
  for (int u = 0; u < 64; u++) { int32_t w = 0;
    if (u < 8) w = (int32_t)root((uint32_t)(u * (16 - u))) * 7 / 8 + 1; else if (u < 40) w = 7 - (u - 8) * (u - 8) * 5 / 1024 - (u - 8) / 8; else w = 1;
    KW[u] = (int8_t)(u < 42 ? imax(1, w) : 0);
    KFIN[u] = (int8_t)(u >= 9 && u <= 18 ? 7 + (u - 9) - (u - 9) * (u - 9) / 6 : 0);
    KTAIL[u] = (int8_t)(u >= 40 && u < 56 ? 2 + (u - 40) * 9 / 15 : 0); KRW[u] = 6 * 65536 / (2 * KW[u] + 2); }
  static const uint8_t T[NKOI] = { 0, 1, 2, 0, 3 };
  for (int k = 0; k < NKOI; k++) for (int s = 0; s < 16; s++) for (int side = 0; side < 2; side++) { uint32_t h = hsh(0xC01u + (uint32_t)k, (uint32_t)(s * 2 + side)); int m = 10;
      int32_t n = vnoise(s * 40, side * 60 + k * 300, 6, 91 + k);
      switch (T[k]) { case 0: m = (n > 120 && s > 0 && s < 12) ? 9 : 10; break;                       /* kohaku: white, red down the back */
        case 1: m = n > 150 ? 11 : (n > 95 ? 9 : 10); break;                                         /* showa: black, red, white */
        case 2: m = 12; break;                                                                        /* ogon: gold */
        default: m = (h & 3) == 0 ? 11 : (n > 140 ? 9 : 4); break; }                                 /* asagi-ish: slate, red belly */
      KPAT[k][s][side] = (uint8_t)m; } }
static void koi_pose(int32_t t) {
  n_koi_on = 0;
  for (int k = 0; k < NKOI; k++) { Koi *K = &KO[k]; int32_t x1, y1, x0, y0; koi_at(k, t, &x1, &y1); koi_at(k, t - 10, &x0, &y0);
    K->x = x1 * 2 / 16; K->y = y1 * 2 / 16;     /* 1/256 cell to 1/16 dot */
    int32_t dx = x1 - x0, dy = y1 - y0, L = (int32_t)root((uint32_t)(dx * dx + dy * dy)) + 1; K->hx = dx * 1024 / L; K->hy = dy * 1024 / L;
    int32_t mx = (K->x >> 4) - K->hx * 56 / 1024, my = (K->y >> 4) - K->hy * 56 / 1024; K->x0 = mx - 62; K->x1 = mx + 62; K->y0 = my - 62; K->y1 = my + 62;
    K->ph = t * 900 + k * 13000;
    int32_t turn = ((x1 - x0) * 0 + 0); (void)turn;
    for (int u = 0; u < 64; u++) { int32_t amp = 256 + u * u * 2; K->wav[u] = (int8_t)(sinq((uint32_t)(K->ph - u * 1500)) * amp / (1024 * 1024)); }
    K->type = (uint8_t)k;
    if (K->x1 > -12 && K->x0 < SW + 12 && K->y1 > -12 && K->y0 < SH + 12) n_koi_on++; } }
static int32_t fdiv(int32_t a, int32_t b) { int32_t q = a / b; if ((a % b) && ((a < 0) != (b < 0))) q--; return q; }   /* floor */
static int32_t cdiv(int32_t a, int32_t b) { return -fdiv(-a, b); }
static int koi_span(const Koi *K, int32_t y, int32_t sx, int32_t wid, int32_t *xa, int32_t *xb) {   /* the dots x of row y where (x + sx, y) can fall in the koi's box, widened by wid */
  int32_t X0 = K->x >> 4, Y0 = K->y >> 4, dy = y - Y0, lo = -100000, hi = 100000;   /* t = x + sx - X0: u = -(t hx + dy hy) / 1024 in [-6, 118]; v = (dy hx - t hy) / 1024 in [-36, 36] */
  int32_t p = -118 * 1024 - dy * K->hy, q = 6 * 1024 - dy * K->hy, hx = K->hx, hy = K->hy;   /* t hx in [p, q] */
  if (hx > 0) { lo = imax(lo, cdiv(p, hx)); hi = imin(hi, fdiv(q, hx)); } else if (hx < 0) { lo = imax(lo, cdiv(q, hx)); hi = imin(hi, fdiv(p, hx)); } else if (p > 0 || q < 0) return 0;
  p = dy * hx - 36 * 1024; q = dy * hx + 36 * 1024;   /* t hy in [p, q] */
  if (hy > 0) { lo = imax(lo, cdiv(p, hy)); hi = imin(hi, fdiv(q, hy)); } else if (hy < 0) { lo = imax(lo, cdiv(q, hy)); hi = imin(hi, fdiv(p, hy)); } else if (p > 0 || q < 0) return 0;
  *xa = imax(0, lo - sx + X0 - wid); *xb = imin(SW, hi - sx + X0 + wid + 1); return *xa < *xb; }
static inline int koi_hit(const Koi *K, int k, int32_t fx, int32_t fy, int *idx) {   /* -1 miss, else 1 (body/fin) */
  int32_t dx = fx * 16 - K->x, dy = fy * 16 - K->y; int32_t u = -(dx * K->hx + dy * K->hy) >> 15, v = (dy * K->hx - dx * K->hy) >> 14;   /* behind the nose in 2-dot steps, across in dots */
  if (u < 0 || u >= 56) return 0; v -= 2 * K->wav[u]; int32_t av = v < 0 ? -v : v, w = 2 * KW[u] + 1;
  if (av <= w) { int side = v > 0, s = u >> 2; int m = KPAT[k][s > 15 ? 15 : s][side]; int sh = 7 - ((av * KRW[u]) >> 16) - (u < 3 ? 2 : 0) - (av == w);
    if (u >= 4 && u <= 5 && av >= w - 6 && av <= w - 3) { m = 14; sh = 2; }   /* an eye */
    *idx = m * 8 + clampi(sh, 0, 7); return 1; }
  if (av <= 2 * KFIN[u]) return 2;   /* fins: the floor seen through them, lit */
  if (KTAIL[u] && av <= 2 * KTAIL[u] + 1 && !(u > 50 && av < (u - 50) * 4)) return 2;
  return 0; }
/* ---------------------------------------------------------------- seen from above: one band */
static uint8_t BK[NKOI];
static inline uint32_t sky_bi(int32_t xq, int32_t yq) {   /* the sky read between its cells (Q4 dots), in spread colour */
  xq = clampi(xq - 16, 0, (GW - 1) * 32 - 1); yq = clampi(yq - 16, 0, (GH - 1) * 32 - 1);
  int32_t cx = xq >> 5, cy = yq >> 5, fx = (xq >> 3) & 3, fy = (yq >> 3) & 3; const uint8_t *s = SKY + cy * GW + cx;
  uint32_t a = SKS[s[0]], b = SKS[s[1]], c = SKS[s[GW]], d = SKS[s[GW + 1]];
  uint32_t t = (a * (4 - fx) + b * fx) * (4 - fy) + (c * (4 - fx) + d * fx) * fy; return (t >> 4) & 0x07E0F81Fu; }
static uint32_t VG[GW], SC[GW]; static int32_t VC[GW];
static int16_t XCL[SW + 80], YCL[SH + 80], CXL[GW + 80], CYL[GH + 80];     /* a dot index held to the glass, 40 either side */
static uint8_t LVL[128];                       /* a light level held to 0..31, from 48 below */
static int16_t VIGC[GW], VIGR[GH];             /* the lens's falloff by cell, across and down (x 4) */
static uint8_t SHM[16][SW]; static int32_t SHR[16][2];   /* shadows marked ahead: a ring of rows, and each row's span */
static int32_t OFS[GW]; static int8_t OXC[GW], OYC[GW], KXC[GW], KYC[GW];   /* by cell, this cell row: where its floor is read (less the dot's own place); the bend for the floor and for a koi */
static FASTF void band_above(int b, uint16_t *o) {
  int nk = 0; for (int k = 0; k < NKOI; k++) { const Koi *K = &KO[k]; if (K->y1 + 24 > b * BAND && K->y0 - 24 < b * BAND + BAND && K->x1 > -24 && K->x0 < SW + 24) BK[nk++] = (uint8_t)k; }
  for (int cy = b * BAND / 2; cy < (b + 1) * BAND / 2; cy++) {   /* a cell row: read out of the surface once, laid as two dot rows */
    { const int16_t *h = wnew + clampi(cy, 1, GH - 2) * GW; int32_t vr = VIGR[cy] / 4 - 24 * 256, y = cy * 2, y0 = -y * SW; const uint8_t *skr = SKY;
      int32_t u1 = h[-GW] + h[GW], c1 = h[0], v1 = u1 + 2 * c1, d1 = h[GW] - h[-GW], u0 = u1, v0 = v1, d0 = d1;   /* Sobel's slopes, columns rolled along: up + down, the middle */
      for (int c = 0; c < GW; c++) { int n = c < GW - 1 ? c + 1 : c; int32_t a = h[n - GW], bb = h[n + GW], cn = h[n], u2 = a + bb, v2 = u2 + 2 * cn, d2 = bb - a;
        int32_t gx = (v2 - v0) * 3 >> 2, gy = (d0 + 2 * d1 + d2) * 3 >> 2, lap = (v0 + v2 + u0 + 2 * u1 + u2 - 16 * c1) >> 2;   /* slopes, of 4 a cell's; weighed 1 2 1 so the grid's own grain is not read as waves */
        int32_t ox = clampi(gx >> 8, -40, 40), oy = clampi(gy >> 8, -40, 40);
        OFS[c] = YCL[40 + y + oy] * SW + y0 + XCL[40 + 2 * c + ox] - 2 * c;   /* the floor where the bent ray lands */
        OXC[c] = (int8_t)(XCL[40 + 2 * c + ox] - 2 * c); OYC[c] = (int8_t)(YCL[40 + y + oy] - y); KXC[c] = (int8_t)clampi(gx >> 10, -10, 10); KYC[c] = (int8_t)clampi(gy >> 10, -10, 10);   /* koi swim near the top: a quarter of the bend */
        VC[c] = clampi(2048 + lap * 3, 0, 4080) - VIGC[c] - vr;   /* the curve gathers the floor's light; the lens falls off */
        SC[c] = SKT[skr[CYL[40 + cy - oy] * GW + CXL[40 + c - ox]]];   /* the sky in it: the slope again, farther */
        u0 = u1; v0 = v1; d0 = d1; u1 = u2; v1 = v2; d1 = d2; c1 = cn; } }
    for (int yy = 0; yy < 2; yy++) { int y = cy * 2 + yy; uint16_t *row = o + (y - b * BAND) * SW; const uint8_t *d4 = B4[y & 3]; const uint16_t *flt = &FLT[0][0]; const uint8_t *fl = FLOOR + y * SW;
      { int32_t d0 = d4[0] * 8, d1 = d4[1] * 8, d2 = d4[2] * 8, d3 = d4[3] * 8;   /* the floor, the whole row */
        for (int c = 0; c < GW; c += 2) { int x = 2 * c;
          int32_t of = OFS[c], cs = VC[c]; uint32_t S = SC[c];
          row[x] = addS(flt[LVL[(cs + d0) >> 7 & 127] * 256 + fl[of + x]], S); row[x + 1] = addS(flt[LVL[(cs + d1) >> 7 & 127] * 256 + fl[of + x + 1]], S);
          of = OFS[c + 1]; cs = VC[c + 1]; S = SC[c + 1];
          row[x + 2] = addS(flt[LVL[(cs + d2) >> 7 & 127] * 256 + fl[of + x + 2]], S); row[x + 3] = addS(flt[LVL[(cs + d3) >> 7 & 127] * 256 + fl[of + x + 3]], S); } }
      { uint8_t *sm = SHM[y & 15]; int32_t *sr = &SHR[y & 15][0];   /* the shadows marked for this row: the floor under them darkened (bit 7) */
        for (int x = sr[0]; x < sr[1]; x++) if (sm[x]) { int c = x >> 1; sm[x] = 0; PROBE_SHADOW; row[x] = addS(flt[LVL[(VC[c] + d4[x & 3] * 8) >> 7 & 127] * 256 + (fl[OFS[c] + x] | 128)], SC[c]); }
        sr[0] = SW; sr[1] = 0; }
      if (!nk) continue;
      PROBE_SLOW;
      for (int j = 0; j < nk; j++) {   /* each koi, only along the dots its box crosses, a cell (two dots) a test; where a body is, its shadow is marked nine rows down */
        int k = BK[j]; const Koi *K = &KO[k]; int32_t xa, xb; if (!koi_span(K, y, 0, 11, &xa, &xb)) continue; uint8_t *sm = SHM[(y + 9) & 15]; int32_t *sr = &SHR[(y + 9) & 15][0];
        for (int x = xa & ~1; x < xb; x += 2) { int c = x >> 1; int32_t kx = x + KXC[c], kyy = y + KYC[c]; int ki;
          if (kx < K->x0 || kx >= K->x1 || kyy < K->y0 || kyy >= K->y1) continue; int hit = koi_hit(K, k, kx, kyy, &ki); if (!hit) continue;
          int32_t cs = VC[c] + (hit == 1 ? 24 * 16 : 14 * 16); uint32_t S = SC[c];
          for (int e = 0; e < 2; e++) { int xx = x + e; int idx = hit == 1 ? ki : fl[OFS[c] + xx]; row[xx] = addS(flt[LVL[(cs + d4[xx & 3] * 8) >> 7 & 127] * 256 + idx], S); }
          if (hit == 1 && x + 14 < SW) { sm[x + 13] = sm[x + 14] = 1; if (x + 13 < sr[0]) sr[0] = x + 13; if (x + 15 > sr[1]) sr[1] = x + 15; } } } } } }
/* ---------------------------------------------------------------- from low over the water: one band */
#define ZF 9408                                 /* row below the horizon d = ZF / z */
#define ZN 64
#define ZDV 4                                   /* z a cell of depth */
#define ZX 427
static int32_t RGX[GW], RGY[GW];
static FASTF void band_low(int b, uint16_t *o) {
  for (int yy = 0; yy < BAND; yy++) { int y = b * BAND + yy; uint16_t *row = o + yy * SW;
    if (y < HORIZON) { int32_t yq = clampi(y * 16 - 8, 0, (GH - 1) * 32 - 1), cy = yq >> 5, fy = (yq >> 3) & 3; const uint8_t *s0 = SKY + cy * GW, *s1 = s0 + GW;   /* the sky: a row weighed down once, then 3:1 across */
      for (int c = 0; c < GW; c++) VG[c] = SKS[s0[c]] * (uint32_t)(4 - fy) + SKS[s1[c]] * (uint32_t)fy;
      row[0] = unspread((VG[0] >> 2) & 0x07E0F81Fu);
      for (int x = 1; x < SW; x++) { int c = (x - 1) >> 1; uint32_t t = (x & 1) ? VG[c] * 3 + VG[c + 1 < GW ? c + 1 : c] : VG[c] + VG[c + 1 < GW ? c + 1 : c] * 3; row[x] = unspread((t >> 4) & 0x07E0F81Fu); }
      continue; }
    int32_t d = y - HORIZON + 1, z = ZF / d, vq = (GH - 1) * 256 - (z - ZN) * 256 / ZDV;   /* this row's depth into the pond, and its cell row (Q8) */
    int on = vq >= 256 && vq < (GH - 2) * 256; int32_t v0 = vq >> 8, fv = vq & 255;
    int32_t uq0 = 80 * 256 - 160 * z * 256 / ZX, du = z * 256 / ZX;
    if (on) { const int16_t *h0 = wnew + v0 * GW, *h1 = h0 + GW;
      int ua = clampi((uq0 >> 8) - 1, 1, GW - 2), ub = clampi(((uq0 + du * SW) >> 8) + 2, 1, GW - 2);
      for (int u = ua; u <= ub; u++) { int32_t gx0 = h0[u + 1] - h0[u - 1], gx1 = h1[u + 1] - h1[u - 1], gy0 = h1[u] - h0[u - GW + GW] + (h1[u] - h0[u]) , gy1 = h1[u + GW] - h1[u];
        (void)gy0; RGX[u] = gx0 + (((gx1 - gx0) * fv) >> 8); RGY[u] = (h1[u] - h0[u]) + (((gy1 - (h1[u] - h0[u])) * fv) >> 8); } }
    int32_t fres = clampi(30 - d / 6, 9, 30);   /* glancing: a mirror; looking down: the water's own dark */
    uint16_t wb = rgb(LIT.c[6][0] * fade_a >> 8, LIT.c[6][1] * fade_a >> 8, LIT.c[6][2] * fade_a >> 8); int32_t yr = (2 * HORIZON - y) * 16 + 8, kx = 1 + d / 24, ky = 1 + d / 24;
    int32_t uq = uq0;
    uint32_t Sp = 0;
    for (int x = 0; x < SW + 2; x += 2, uq += 2 * du) { int32_t gx = 0, gy = 0; int u = uq >> 8;   /* every other dot read; the dot between takes the mean */
      if (on && u >= 1 && u < GW - 2) { int32_t fu = uq & 255; gx = RGX[u] + (((RGX[u + 1] - RGX[u]) * fu) >> 8); gy = RGY[u] + (((RGY[u + 1] - RGY[u]) * fu) >> 8); }
      uint32_t S = scaleS(sky_bi(x * 16 + 8 + gx * kx / 4, yr - gy * ky / 2), (uint32_t)fres);   /* the far bank in the water, bent by the slope */
      if (x < SW) row[x] = addS(wb, S); if (x > 0) row[x - 1] = addS(wb, ((Sp + S) >> 1) & 0x07E0F81Fu); Sp = S; } } }
/* ---------------------------------------------------------------- things in the air and on the water, laid over a band */
static void glow_dot(uint16_t *o, int b, int32_t cx, int32_t cy, int32_t R, uint16_t col, int al) {   /* light falling off as (R^2 - d^2)^2 */
  if (R < 1 || al <= 0) return; int y0 = imax(cy - R, b * BAND), y1 = imin(cy + R, b * BAND + BAND - 1); uint32_t S = spread(col);
  for (int y = y0; y <= y1; y++) for (int x = imax(cx - R, 0); x <= imin(cx + R, SW - 1); x++) { int32_t d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy), r2 = R * R; if (d2 >= r2) continue;
      int32_t q = (r2 - d2) * 32 / r2; uint32_t a = (uint32_t)(q * q * al >> 10); if (a > 32) a = 32; if (a) o[(y - b * BAND) * SW + x] = addS(o[(y - b * BAND) * SW + x], scaleS(S, a)); } }
static void vline(uint16_t *o, int b, int32_t x, int32_t ya, int32_t yb, int32_t slant, uint16_t col, int al) {   /* a thin streak of light, slanting slant/256 a row */
  uint32_t S = scaleS(spread(col), (uint32_t)clampi(al, 0, 32)); int y0 = imax(imin(ya, yb), b * BAND), y1 = imin(imax(ya, yb), b * BAND + BAND - 1);
  for (int y = y0; y <= y1; y++) { int32_t xx = x + ((y - ya) * slant >> 8); if (xx >= 0 && xx < SW) o[(y - b * BAND) * SW + xx] = addS(o[(y - b * BAND) * SW + xx], S); } }
static uint16_t leaf_col(int sh) { int32_t r = LIT.c[7][0] * (4 + sh) / 10, g = LIT.c[7][1] * (4 + sh) / 10, bb = LIT.c[7][2] * (4 + sh) / 10; return rgb(TM[clampi(r * fade_a >> 8, 0, 1023)], TM[clampi(g * fade_a >> 8, 0, 1023)], TM[clampi(bb * fade_a >> 8, 0, 1023)]); }
static void over_above(int b, uint16_t *o) {
  uint16_t white = rgb(230 * fade_a >> 8, 236 * fade_a >> 8, 255 * fade_a >> 8);
  for (int32_t t = sim_s - 8; t <= sim_s; t++) { if (t < 0) continue; int32_t age = sim_s - t;   /* splashes: a crown for the steps after a drop lands */
    for (int32_t i = drops_from(t), e = drops_from(t + 1); i < e; i++) { int32_t x, y, a; drop_place(i, &x, &y, &a); int32_t cx = x * 2 + 1, cy = y * 2 + 1; if (cy + 12 < b * BAND || cy - 12 > b * BAND + BAND) continue;
      glow_dot(o, b, cx, cy, 2 + age, white, (9 - age) * a / 60); if (age < 3) glow_dot(o, b, cx, cy, 2, white, 30); } }
  int32_t lx, ly, rot, hg; leaf_at(sim_s, &lx, &ly, &rot, &hg);
  if (hg < 60) { int32_t cx = lx * 2 >> 8, cy = ly * 2 >> 8, R = 17 + hg / 2;
    if (cy + R >= b * BAND && cy - R < b * BAND + BAND) for (int y = imax(cy - R, b * BAND); y <= imin(cy + R, b * BAND + BAND - 1); y++) for (int x = imax(cx - R, 0); x <= imin(cx + R, SW - 1); x++) { int32_t sh;
        if (!leaf_in(x - cx, y - cy, R, rot & 4095, &sh)) continue; o[(y - b * BAND) * SW + x] = leaf_col(clampi(sh, 0, 7)); }
    int32_t sx = cx + 7 + hg / 2, sy = cy + 5 + hg / 3;   /* and its shadow, falling on the floor through the water (darkened in place) */
    if (sy + R >= b * BAND && sy - R < b * BAND + BAND) for (int y = imax(sy - R, b * BAND); y <= imin(sy + R, b * BAND + BAND - 1); y++) for (int x = imax(sx - R, 0); x <= imin(sx + R, SW - 1); x++) { int32_t sh;
        if (!leaf_in(x - sx, y - sy, R, rot & 4095, &sh)) continue; int32_t lxx = x - cx, lyy = y - cy; if (leaf_in(lxx, lyy, R, rot & 4095, &sh)) continue;
        uint16_t *p = &o[(y - b * BAND) * SW + x]; *p = (uint16_t)((*p >> 1) & 0x7BEF); } } }
static void proj_low(int32_t gx, int32_t gy, int32_t *sx, int32_t *sy, int32_t *z) { *z = ZN + ((GH - 1) * 256 - gy) * ZDV / 256; *sy = HORIZON - 1 + ZF / *z; *sx = 160 + (gx - 80 * 256) * ZX / (*z * 256); }
static void over_low(int b, uint16_t *o) {
  uint16_t rainc = rgb(200 * fade_a >> 8, 210 * fade_a >> 8, 230 * fade_a >> 8);
  for (int32_t t = sim_s - 4; t <= sim_s + 14; t++) { if (t < 0) continue; int32_t dt = t - sim_s;   /* each drop: falling to where it lands, or the splash after */
    for (int32_t i = drops_from(t), e = drops_from(t + 1); i < e; i++) { int32_t x, y, a; drop_place(i, &x, &y, &a); int32_t sx, sy, z; proj_low(x * 256 + 128, y * 256 + 128, &sx, &sy, &z);
      if (sx < -10 || sx > SW + 10) continue; int32_t sc = ZF / z;   /* dots a world unit, about */
      if (dt > 0) { int32_t up = dt * sc * 3 / 4, len = imax(3, sc * 3 / 4); vline(o, b, sx - up / 5, sy - up - len, sy - up, 50, rainc, 10 + sc / 6); }
      else { int32_t age = -dt; glow_dot(o, b, sx, sy - age, 1 + sc / 12 + age, rainc, (5 - age) * 5); } } }
  int32_t lx, ly, rot, hg; leaf_at(sim_s, &lx, &ly, &rot, &hg);   /* the leaf afloat, seen edge on */
  if (hg == 0) { int32_t sx, sy, z; proj_low(lx, ly, &sx, &sy, &z); int32_t w = imax(2, 7 * ZF / z / 16), h = imax(1, w / 5);
    for (int y = imax(sy - h, b * BAND); y <= imin(sy + h, b * BAND + BAND - 1); y++) for (int x = imax(sx - w, 0); x <= imin(sx + w, SW - 1); x++) if ((x - sx) * (x - sx) * h * h + (y - sy) * (y - sy) * w * w <= w * w * h * h) o[(y - b * BAND) * SW + x] = leaf_col(4 - (y - sy)); } }
/* ---------------------------------------------------------------- the latch */
static int sb;
static void witness(void) {                    /* forward: write the pond's print; back: it must be the print written at the same step */
  if (sim_s & 1) return; uint32_t h = pond_print(); int32_t i = sim_s / 2; if (i > PEAKP) return;
  if (film_p <= PEAKP) PRINT[i] = h; else if (film_p > PEAKP + HOLDP) { if (PRINT[i] == h) mirror_ok++; else mirror_bad++; } }
static void move_film(int dir) {               /* one latch along the film: the pond follows the step read from the position */
  int32_t p1 = dir > 0 ? (film_p + 1) % FILM : (film_p + FILM - 1) % FILM, s1 = step_at(p1);
  while (sim_s < s1) step_fwd(); while (sim_s > s1) step_back();
  film_p = p1; if (film_p == 0 && dir > 0) { if (pond_flat()) homes++; else home_bad++; } }
static uint32_t story_print(void) { return pond_print() ^ ((uint32_t)film_p << 20) ^ (uint32_t)sim_s; }
static void minamo_begin(void) {
  grow_ring(); grow_tm(); grow_leaf(); grow_koi();
  for (int x = 0; x < GW; x++) { int32_t e = imin(x, GW - 1 - x); KX[x] = (uint8_t)(e >= 16 ? 130 : 6 + e * e * 124 / 256); }   /* the shallows by the banks: waves slow and crowd there */
  for (int y = 0; y < GH; y++) { int32_t e = imin(y, GH - 1 - y); KY[y] = (uint8_t)(e >= 16 ? 130 : 6 + e * e * 124 / 256); }
  for (int c = 0; c < GW; c++) { int32_t d = c * 2 + 1 - 160; VIGC[c] = (int16_t)(d * d / 40); } for (int c = 0; c < GH; c++) { int32_t d = c * 2 + 1 - 120; VIGR[c] = (int16_t)(d * d / 30 * 4); }
  for (int i = 0; i < SW + 80; i++) XCL[i] = (int16_t)clampi(i - 40, 0, SW - 2); for (int i = 0; i < SH + 80; i++) YCL[i] = (int16_t)(clampi(i - 40, 0, SH - 2));
  for (int i = 0; i < GW + 80; i++) CXL[i] = (int16_t)clampi(i - 40, 0, GW - 1); for (int i = 0; i < GH + 80; i++) CYL[i] = (int16_t)clampi(i - 40, 0, GH - 1);
  for (int i = 0; i < 128; i++) LVL[i] = (uint8_t)clampi(i - 48, 0, 31);
  paint_floor(); memset(WA, 0, sizeof WA); memset(WB, 0, sizeof WB); wold = WA; wnew = WB; sim_s = 0; film_p = 0; PRINT[0] = pond_print();
#ifdef START_P
  while (film_p < START_P) { move_film(1); witness(); }   /* bench only: begin partway */
#endif
}
static void frame(void) {
  uint32_t c0 = clock_now();
  if (frame_n > 0) move_film(play);
  witness();
  uint32_t c1 = clock_now();
  int si = shot_of(film_p); if (si != shot_i) { shot_i = si; cut_now = 1; if ((int)SHOTS[si].kind != sky_kind) paint_sky(SHOTS[si].kind); }
  int low = SHOTS[si].kind == 1;
  if (!low && (film_p & 1) && !cut_now) { held_n++; cut_now = 0; frame_n++; uint32_t c2 = clock_now(); cyc_sim = c1 - c0; cyc_draw = c2 - c1; return; }   /* from above, the water is drawn on 2s: the odd latch holds the drawing (the pond still steps) */
  light_at(film_p);
  fade_a = 256; if (film_p < 40) fade_a = film_p * 256 / 40; if (film_p > FILM - 40) fade_a = (FILM - film_p) * 256 / 40;
  flash_a = 0; if (film_p >= PEAKP - 2 && film_p < PEAKP + HOLDP) { int32_t k = film_p - (PEAKP - 2); static const uint8_t FL[18] = { 0, 110, 36, 0, 140, 80, 26, 8, 0, 0, 90, 28, 8, 0, 0, 0, 0, 0 }; flash_a = FL[k < 18 ? k : 17]; }
  if (film_p == PEAKP - 1 && low) lightning_paint(77); if (film_p == PEAKP + 9 && low) paint_sky(1);
  { uint32_t q0 = clock_now(); tables(); cyc_tab = clock_now() - q0; } cyc_band = cyc_over = 0;
  if (!low) { koi_pose(sim_s); memset(SHM, 0, sizeof SHM); for (int i = 0; i < 16; i++) { SHR[i][0] = SW; SHR[i][1] = 0; } }
  { uint8_t a[4] = { 0, 0, (uint8_t)((SW - 1) >> 8), (uint8_t)(SW - 1) }; wire_cmd(0x2A, a, 4); }
  { uint8_t a[4] = { 0, 0, (uint8_t)((SH - 1) >> 8), (uint8_t)(SH - 1) }; wire_cmd(0x2B, a, 4); }
  wire_cmd(0x2C, 0, 0);
  for (int b = 0; b < NBAND; b++) { uint16_t *o = STRIP[sb]; wire_hold(sb);
    { uint32_t q0 = clock_now(); if (low) band_low(b, o); else band_above(b, o); uint32_t q1 = clock_now(); if (low) over_low(b, o); else over_above(b, o); cyc_band += q1 - q0; cyc_over += clock_now() - q1; }
    if (home_bad || mirror_bad) if (b == 0) for (int y = 2; y < 7; y++) for (int x = SW - 8; x < SW - 3; x++) o[y * SW + x] = 0xF800;   /* a broken witness is shown, not hidden */
    { uint32_t *w = (uint32_t *)(void *)o; for (int i = 0; i < SW * BAND / 2; i++) { uint32_t v = w[i]; w[i] = ((v >> 8) & 0x00FF00FFu) | ((v << 8) & 0xFF00FF00u); } }   /* high byte first down the wire */
    wire_data(o, SW * BAND * 2); wire_mark(sb); sb ^= 1; }
  cut_now = 0; frame_n++;
  uint32_t c2 = clock_now(); cyc_sim = c1 - c0; cyc_draw = c2 - c1; }

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
