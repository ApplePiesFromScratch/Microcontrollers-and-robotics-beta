// TATER-CHAN ASCENDS, for the ESP32-C3 SuperMini + 2.0" ST7789 (320 x 240 across). About 70 seconds that loop, each loop worse:
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
// ---------------------------------------------------------------- every type first (tater.h)
/* tater.h  TATER-CHAN ASCENDS, on the ESP32-C3's 320 x 240 (RISC-V, 160 MHz, no floating point, no GPU).
   A potato with anime eyes becomes, in order: aware, a magical girl, a kaleidoscope, a Julia set, the sun over a world of
   potato hills, a planet, and everything at once. Then it is just a potato again, and it goes round once more, a little worse.

   Nothing here knows what time it is. Every motion is a rate carried from the frame before; every stage is a countdown.
   Turns come from one quarter ring of 64 headings (LANTERN's Pythagorean ring). Angles of the screen's dots are found by
   asking that ring which heading each dot is not past (the sign of a cross product): no arctangent. Square roots are counted
   by bits. The swarm's orientation is a carried rotation, nudged each frame and landed back on true by the mouth.

   Needs the glue: wire_cmd(c, p, n), wire_data(p, n), wire_mark(s), wire_hold(s), clock_now(), hud_fps. */
#include <stdint.h>
#include <string.h>
#ifndef FASTF
#define FASTF
#endif
#define SW 320
#define SH 240
#define BAND 16
#define LW 160
#define LH 120
#define NOBJ 96
#define NCAP 24
#define NSPARK 48
#define NWORD 6
#define NSTAR 180
#define NSWARM 72
typedef struct { int16_t x, y; } Pt;
typedef struct { int32_t ax, ay, bx, by; int16_t ra, rb; uint8_t mat; } Cap;
typedef struct { int32_t ax, ay, vx, vy, invL, invL2, ra, rb, ira, irb, pl; int16_t x0, x1, y0, y1; uint8_t mat; uint8_t cls[65]; } CapRun;
typedef struct { int16_t x, y, rx, ry; int32_t z; int16_t y0, y1; int32_t ivx, ivy, inkq, auraq, ka; uint8_t kind, mat, expr, mouth, hue; } Obj;
typedef struct { int32_t x, y; int16_t vx, vy; uint8_t life, hue, size; } Spark;
typedef struct { int16_t x, y; uint8_t life, hue, sc, melt; const char *s; } Word;
typedef struct { int16_t x0, y0, x1, y1; uint16_t c; } Seg;
typedef struct {
  int32_t x, y, tx, ty, vx, vy;                 /* the centre in 1/16 dot, where it is pulled, its rate */
  int32_t r, rv, rt;                            /* the radius in 1/16 dot, its rate, where it is pulled */
  int16_t sq, sqv;                              /* squash on 256 (wide > 0), carried and sprung */
  uint16_t yaw, pitch_ph; int16_t yawv, yawt, pitch_amp;
  int16_t arm[4], armv[4], armt[4];             /* shoulder L, elbow L, shoulder R, elbow R (4096 a turn; 0 hangs down) */
  int32_t hx[3], hy[3]; int16_t hvx[3], hvy[3]; /* the ahoge: three points, each lagging the one below it */
  uint8_t expr, mouth, blink, wand, limbs, aura; uint16_t bob, bob_amp, blink_t;
} Tater;
enum { K_TATER, K_MINI, K_RINGB, K_RINGF, K_STAR4 };
enum { E_NORMAL, E_SPARK, E_HEART, E_SHOCK, E_GLOW, E_CLOSED, E_STAR };
enum { M_SMILE, M_OPEN, M_CAT, M_TINY };
enum { MT_SPUD, MT_INK, MT_WHITE, MT_IRISB, MT_IRISP, MT_IRISG, MT_BLUSH, MT_MOUTH, MT_TONGUE, MT_LEAF, MT_STICK, MT_SHOE, MT_GOLD, MT_RINGA, MT_RINGB, MT_DIMPLE, MT_POTATO, MT_N };   /* MT_SPUD is her skin: blueberry. She doesn't know. */
enum { BG_SKY, BG_HALF, BG_TUNNEL, BG_KALEI, BG_FIELD, BG_SPACE };
enum { S_MORNING, S_SPARK, S_HENSHIN, S_SENPAI, S_SOUL, S_WORLD, S_COSMOS, S_MADNESS, S_END, S_N };
enum { P_DOWN, P_WAVE, P_FISTS, P_UP, P_HEART, P_DANCE };
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
// ---------------------------------------------------------------- TATER-CHAN (tater.h)
#define PROGMEM_OR_NOT
/* the quarter ring: 64 headings grown by one Pythagorean turn (26565, 652, 26573), landed by the mouth (LANTERN's rule), on 16384 */
static const int16_t RING_Q[65] PROGMEM_OR_NOT = {0,402,804,1205,1606,2006,2404,2801,3196,3589,3980,4369,4755,5138,5518,5895,6268,6638,7004,7365,7722,8074,8421,8763,9100,9432,9758,10078,10392,10700,11001,11296,11584,11865,12139,12405,12664,12915,13159,13395,13623,13842,14053,14256,14450,14635,14811,14979,15138,15287,15427,15558,15680,15792,15895,15988,16071,16145,16209,16263,16307,16342,16367,16382,16384};


/* ================================================================ the ring, the scatter, counted roots */
static int16_t fsin(int16_t h) {                       /* 4096 a turn, on 16384, from the quarter ring: 16 steps between its headings */
  uint16_t u = (uint16_t)h & 4095; uint8_t q = (uint8_t)(u >> 10); uint16_t r = u & 1023; if (q & 1) r = (uint16_t)(1024 - r);
  uint8_t i = (uint8_t)(r >> 4), f = (uint8_t)(r & 15); int16_t a = RING_Q[i];
  int16_t v = (int16_t)(a + (int16_t)((((int32_t)(i < 64 ? RING_Q[i + 1] : a) - a) * f + 8) >> 4));
  return (q & 2) ? (int16_t)-v : v; }
static int16_t fcos(int16_t h) { return fsin((int16_t)(h + 1024)); }
static uint32_t f_seed = 0x7A7E;
static uint16_t f_draw(void) { uint32_t x = f_seed; x ^= x << 13; x ^= x >> 17; x ^= x << 5; f_seed = x; return (uint16_t)(x >> 8); }
static int32_t isq(int32_t v) { if (v <= 0) return 0; int32_t r = 0, bit = 1 << 30; while (bit > v) bit >>= 2; while (bit) { if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; } else r >>= 1; bit >>= 2; } return r; }
static uint32_t mix32(uint32_t x) { x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16; return x; }

/* ================================================================ colour */
static uint16_t glass(int32_t r, int32_t g, int32_t b) {              /* 8-bit channels to 565 in wire order */
  r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
  uint16_t v = (uint16_t)(((r * 249 + 1014) >> 11) << 11 | ((g * 253 + 505) >> 10) << 5 | ((b * 249 + 1014) >> 11));
  return (uint16_t)((v >> 8) | (v << 8)); }
#define SWP(v) ((uint16_t)(((v) >> 8) | ((v) << 8)))
static inline uint16_t avgN(uint16_t x, uint16_t y) { return (uint16_t)((x & y) + (((x ^ y) & 0xF7DE) >> 1)); }
static inline uint16_t avgW(uint16_t a, uint16_t b) { return SWP(avgN(SWP(a), SWP(b))); }
static const uint8_t HUES[16][3] = {
  {255, 40, 120}, {255, 90, 30}, {255, 170, 0}, {250, 230, 40}, {170, 240, 20}, {40, 220, 90}, {0, 210, 190}, {20, 160, 255},
  {80, 90, 255}, {150, 60, 255}, {220, 40, 240}, {255, 60, 180}, {255, 120, 90}, {250, 200, 120}, {120, 255, 200}, {255, 255, 255} };
static const uint8_t MATC[MT_N][3] = { {84, 100, 214}, {22, 12, 34}, {255, 255, 255}, {255, 156, 30}, {255, 60, 170}, {255, 190, 20}, {255, 120, 160},
  {150, 20, 50}, {255, 110, 140}, {70, 210, 80}, {120, 70, 34}, {235, 40, 70}, {255, 214, 40}, {255, 150, 220}, {110, 220, 255}, {52, 50, 140}, {232, 176, 92} };
static uint16_t PALC[256], PALN[256]; static uint32_t PAL2[256];      /* (hue << 4 | level) -> colour: on the wire, unswapped, doubled */
static uint16_t RAMP[MT_N][16];                                       /* shadow (leaning violet) .. base (10) .. highlight (15), on the wire */
static uint16_t INK, WHITE, ACC, ACCN, BLACK;
static void palette_turn(uint8_t ph, uint8_t sat) {
  for (int k = 0; k < 256; k++) { int h = ((k >> 4) + ph) & 15, l = k & 15; if (h == 15) h = 3;
    int32_t r = HUES[h][0] * l / 15, g = HUES[h][1] * l / 15, b = HUES[h][2] * l / 15;
    if (sat < 255) { int32_t m = (r + g + b) / 3; r = m + (r - m) * sat / 255; g = m + (g - m) * sat / 255; b = m + (b - m) * sat / 255; }
    if (l >= 14) { r += (255 - r) / 3; g += (255 - g) / 3; b += (255 - b) / 3; }
    if (l == 0) { r = 8; g = 4; b = 20; } else { r += 8; g += 4; b += 20; }
    PALC[k] = glass(r, g, b); PALN[k] = SWP(PALC[k]); PAL2[k] = (uint32_t)PALC[k] | ((uint32_t)PALC[k] << 16); } }
static void grow_ramps(void) {
  for (int m = 0; m < MT_N; m++) for (int s = 0; s < 16; s++) { int32_t c[3];
    for (int j = 0; j < 3; j++) { int32_t b = MATC[m][j], sh = j == 2 ? b / 3 + 70 : j == 0 ? b / 3 + 30 : b / 4;   /* shadows lean violet */
      c[j] = s < 10 ? sh + (b - sh) * s / 10 : b + (255 - b) * (s - 10) / 9; }
    RAMP[m][s] = glass(c[0], c[1], c[2]); }
  INK = RAMP[MT_INK][2]; WHITE = glass(255, 255, 255); BLACK = glass(6, 2, 14); }

/* ================================================================ tables grown at boot */
static uint8_t PA[LH][LW], PR[LH][LW];               /* a quarter of the screen: each dot's heading (64 a quarter) and distance */
static int16_t SIN8[256];                            /* 256 a turn, on 16384 */
#define COS8(a) SIN8[(uint8_t)((a) + 64)]
static uint8_t DEP[256], FOGL[256];
static uint16_t NZ1[257], NZ2[257];                  /* the rest of a unit: across k/256 -> sqrt(1 - k^2); squared q = k*256 -> sqrt(1 - q) */
static uint16_t INVZ[1025];
static uint8_t HM[128][128];                         /* the potato hills */
static uint8_t DOT8[64];                             /* a halftone cell: each dot's distance from the middle, squared */
static uint32_t hill_hash(int i, int j, int o) { return mix32((uint32_t)i * 73856093u ^ (uint32_t)j * 19349663u ^ (uint32_t)o * 83492791u); }
static void grow_tables(void) {
  for (int qy = 0; qy < LH; qy++) for (int qx = 0; qx < LW; qx++) {
    int32_t X = 2 * qx + 1, Y = 2 * qy + 1, r = isq(X * X + Y * Y) >> 1; PR[qy][qx] = (uint8_t)(r > 255 ? 255 : r);
    int lo = 0, hi = 63;                              /* the last heading of the ring the dot is not behind: a cross product's sign */
    while (lo < hi) { int h = (lo + hi + 1) >> 1; int32_t c = (int32_t)RING_Q[64 - h] * Y - (int32_t)RING_Q[h] * X; if (c >= 0) lo = h; else hi = h - 1; }
    PA[qy][qx] = (uint8_t)lo; }
  for (int a = 0; a < 256; a++) SIN8[a] = fsin((int16_t)(a * 16));
  for (int r = 0; r < 256; r++) { int32_t d = 3200 / (r + 3); DEP[r] = (uint8_t)(d > 255 ? 255 : d); int f = r / 6; FOGL[r] = (uint8_t)(f > 13 ? 13 : f); }
  for (int k = 0; k <= 256; k++) { NZ1[k] = (uint16_t)isq(65536 - k * k); NZ2[k] = (uint16_t)isq(65536 - k * 256); }
  INVZ[0] = 65535; for (int z = 1; z <= 1024; z++) { int32_t v = 65536 / z; INVZ[z] = (uint16_t)(v > 65535 ? 65535 : v); }
  for (int k = 0; k < 64; k++) { int x = (k & 7) * 2 - 7, y = (k >> 3) * 2 - 7; DOT8[k] = (uint8_t)((x * x + y * y) / 4); }
  for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) {     /* value noise, four octaves, tiling: the hills */
    int32_t h = 0; static const uint8_t S[4] = { 64, 32, 16, 8 }, A[4] = { 130, 70, 34, 12 };
    for (int o = 0; o < 4; o++) { int s = S[o], n = 128 / s, i = x / s, j = y / s, fx = x % s, fy = y % s;
      int32_t a = hill_hash(i % n, j % n, o) & 255, b = hill_hash((i + 1) % n, j % n, o) & 255, c = hill_hash(i % n, (j + 1) % n, o) & 255, d = hill_hash((i + 1) % n, (j + 1) % n, o) & 255;
      int32_t t = a * (s - fx) + b * fx, u = c * (s - fx) + d * fx, v = (t * (s - fy) + u * fy) / (s * s); h += v * A[o] / 255; }
    HM[y][x] = (uint8_t)(h > 255 ? 255 : h); } }

/* ================================================================ the field: a picture that remembers (160 x 120, hue << 4 | level) */
static uint8_t FA[LH][LW], FB[LH][LW]; static uint8_t (*fnow)[LW] = FA, (*fnext)[LW] = FB;
static inline void fpour(int x, int y, uint8_t hue, uint8_t lev) { if ((unsigned)x < LW && (unsigned)y < LH) { uint8_t *p = &fnext[y][x]; if ((*p & 15) < lev) *p = (uint8_t)(hue << 4 | lev); } }
static void fdisc(int cx, int cy, int r, uint8_t hue, uint8_t lev) { for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) if (x * x + y * y <= r * r) fpour(cx + x, cy + y, hue, lev); }
/* each dot takes what was a little nearer the middle, turned by a small heading, a level dimmer; rows ripple */
static void FASTF field_step(int32_t zoom, int16_t ang, uint8_t dec, int16_t rip, uint16_t rph) {
  int32_t zc = (int32_t)fcos(ang) * zoom >> 14, zs = (int32_t)fsin(ang) * zoom >> 14;
  const int32_t cx = LW << 15, cy = LH << 15;
  for (int y = 0; y < LH; y++) {
    int32_t ry = ((y << 16) + 32768) - cy, rp = (int32_t)fsin((int16_t)(y * 90 + rph)) * rip;
    int32_t u = cx + (int32_t)(((-(int64_t)(LW << 15)) * zc - (int64_t)ry * zs) >> 16) + rp, v = cy + (int32_t)(((-(int64_t)(LW << 15)) * zs + (int64_t)ry * zc) >> 16);
    uint8_t *o = fnext[y];
    for (int x = 0; x < LW; x++) { int sx = u >> 16, sy = v >> 16; uint8_t s = ((unsigned)sx < LW && (unsigned)sy < LH) ? fnow[sy][sx] : 0;
      uint8_t l = (uint8_t)(s & 15); o[x] = l > dec ? (uint8_t)(s - dec) : 0; u += zc; v += zs; } } }
static void field_swap(void) { uint8_t (*t)[LW] = fnow; fnow = fnext; fnext = t; }

/* ================================================================ the potato's 3-D face */
static int32_t RM[9];                                 /* the face's turn this frame, on 16384, from the carried yaw and pitch */
static const int32_t LX = -112, LY = -142, LZ = 180;  /* toward the light, on 256 */
static int heart(int32_t u, int32_t v) { int32_t a = u < 0 ? -u : u, cx = a - 8, cy = v + 5; if (cx * cx + cy * cy < 90) return 1; return v >= -5 && v < 17 && a * 22 < (17 - v) * 18; }
static int star4(int32_t u, int32_t v, int32_t s) { int32_t a = u < 0 ? -u : u, b = v < 0 ? -v : v; return (a < s && b < s && a * b * 7 < s * s) || a * a + b * b < s * s / 14; }
static uint16_t FASTF face_px(int32_t mx, int32_t my, int32_t mz, int x, int y, uint8_t expr, uint8_t mouth, int16_t gaze) {
  if (my < -150) {                                    /* her crown: the five-lobed calyx every blueberry has on top. She calls it a sprout scar. */
    int32_t a = mx, b = my + 196, c = mz - 160, d = a * a + b * b + c * c;
    if (d < 2600) { int32_t q = a * a + c * c; uint16_t lobe = (uint16_t)((a * 3 + c * 5) & 32); return d > 1400 ? RAMP[MT_SPUD][lobe ? 2 : 4] : q < 160 ? RAMP[MT_SPUD][1] : RAMP[MT_SPUD][6]; }
    return 0; }
  if (mz < 70) {                                      /* round the back: the potato's own eyes (the dimples) */
    static const int16_t DIM[5][3] = { {180, -90, -150}, {-160, 120, -150}, {60, 200, -150}, {-90, -200, -130}, {236, 70, 50} };
    for (int k = 0; k < 5; k++) { int32_t a = mx - DIM[k][0], b = my - DIM[k][1], c = mz - DIM[k][2]; if (a * a + b * b + c * c < 600) return RAMP[MT_DIMPLE][a + b < 0 ? 4 : 7]; }
    if (mz < 40) return 0; }
  int32_t du = mx < 0 ? mx + 76 : mx - 76, dv = my + 14, ad = du < 0 ? -du : du;
  if (expr == E_CLOSED) { if (ad < 30) { int32_t c = 8 - du * du / 70; if (dv - c < 4 && dv - c > -4) return INK; } }
  else { int32_t e = du * du * 73 + dv * dv * 37;     /* the eye: an upright ellipse, 30 x 42 on 256 of the body */
    if (e < 65536) {
      if (e > 54000 || dv < -30) return INK;          /* the outline, and the heavy upper lash */
      if (expr == E_GLOW) return e < 26000 ? WHITE : ACC;
      int32_t iu = du - gaze, iv = dv - 7, ei = iu * iu * 135 + iv * iv * 57;
      { int32_t a = iu + 9, b = iv + 13; if (a * a + b * b < (expr == E_SPARK ? 120 : 66)) return WHITE; }
      { int32_t a = iu - 8, b = iv - 12; if (a * a + b * b < 22) return WHITE; }
      if (ei < 65536) {
        uint8_t im = expr == E_HEART ? MT_IRISP : expr == E_STAR ? MT_IRISG : MT_IRISB;
        if (expr == E_SHOCK) return iu * iu + iv * iv < 26 ? INK : WHITE;
        if (expr == E_HEART) { if (heart(iu, iv + 2)) return RAMP[MT_IRISP][13]; }
        else if (expr == E_STAR) { if (star4(iu, iv, 17)) return RAMP[MT_GOLD][15]; }
        else if (iu * iu * 650 + iv * iv * 250 < 65536) return RAMP[im][1];
        if (expr == E_SPARK && star4(iu + 5, iv - 9, 8)) return WHITE;
        if (ei > 50000) return RAMP[im][2];
        int s = 3 + (int)((iv + 34) * 11 / 68); s = s < 2 ? 2 : s > 14 ? 14 : s; return RAMP[im][s]; }
      return dv < -20 ? RAMP[MT_WHITE][8] : WHITE; } }
  { int32_t bu = du + (du < 0 ? -8 : 8), bv = my - 38, eb = bu * bu * 36 + bv * bv * 330;   /* the blush: wide, pink, hatched */
    if (eb < 65536) return ((x + y) & 3) == 0 ? RAMP[MT_BLUSH][7] : RAMP[MT_BLUSH][12]; }
  int32_t am = mx < 0 ? -mx : mx;
  switch (mouth) {
    case M_SMILE: if (am < 26) { int32_t c = 58 + (676 - mx * mx) / 46; if (my - c < 4 && my - c > -4) return INK; } break;
    case M_OPEN: { int32_t q = mx * mx * 202 + (my - 70) * (my - 70) * 114; if (q < 65536) return q > 50000 ? INK : my > 78 ? RAMP[MT_TONGUE][11] : RAMP[MT_MOUTH][6]; } break;
    case M_CAT: if (am < 27) { int32_t c = 66 - ((am - 13) * (am - 13)) / 9; if (my - c < 4 && my - c > -4) return INK; } break;
    default: { int32_t q = mx * mx + (my - 66) * (my - 66); if (q > 20 && q < 80) return INK; } }
  return 0; }

/* ================================================================ objects: shaded spheres, in depth order */
static Obj OB[NOBJ]; static int nobj; static Obj *ORD[NOBJ]; static Obj *BOBJ[NOBJ]; static int nbobj;
static void obj_prep(Obj *b) {
  int32_t rx = b->rx < 2 ? 2 : b->rx, ry = b->ry < 2 ? 2 : b->ry; b->ivx = 65536 / rx; b->ivy = 65536 / ry;
  int32_t t = 256 - (int32_t)(2 * 256 + 128) / (rx < ry ? rx : ry); if (t < 0) t = 0; b->inkq = t * t;
  int32_t aur = (b->kind == K_TATER && b->mat) ? 10 : 0; b->ka = 256 + aur * 256 / ry; b->auraq = b->ka * b->ka;
  int32_t rr = b->kind == K_RINGB || b->kind == K_RINGF ? b->ry : ry;
  b->y0 = (int16_t)(b->y - (rr * b->ka >> 8) - 1); b->y1 = (int16_t)(b->y + (rr * b->ka >> 8) + 1);
  if (b->kind == K_STAR4) { b->y0 = (int16_t)(b->y - b->rx); b->y1 = (int16_t)(b->y + b->rx); } }
static Obj *obj_new(uint8_t kind, int x, int y, int rx, int ry, int32_t z) { if (nobj >= NOBJ) return 0; Obj *b = &OB[nobj++]; memset(b, 0, sizeof *b); b->kind = kind; b->x = (int16_t)x; b->y = (int16_t)y; b->rx = (int16_t)rx; b->ry = (int16_t)ry; b->z = z; return b; }

/* ---- capsules (limbs, the ahoge): from THE FISH AFFECT, a bone with a radius at each end, shaded across */
static CapRun CR[NCAP]; static int ncap;
static void cap_add(const Cap *c) {
  if (ncap >= NCAP) return; CapRun *r = &CR[ncap];
  int32_t vx = c->bx - c->ax, vy = c->by - c->ay, L2 = (vx * vx + vy * vy) >> 8, L = isq(L2 * 256);
  if (L < 16) { L = 16; L2 = 1; vx = 0; vy = 16; }
  r->ax = c->ax; r->ay = c->ay; r->vx = vx; r->vy = vy; r->invL = (int32_t)(((int64_t)1 << 28) / L); r->invL2 = L2 ? (int32_t)(((int64_t)1 << 24) / L2) : 0;
  r->ra = c->ra; r->rb = c->rb; r->ira = (int32_t)((256 << 8) / (c->ra > 16 ? c->ra : 16)); r->irb = (int32_t)((256 << 8) / (c->rb > 16 ? c->rb : 16));
  r->pl = (int32_t)(((-vy) * LX + vx * LY) / L);
  int32_t rm = c->ra > c->rb ? c->ra : c->rb, xa = c->ax < c->bx ? c->ax : c->bx, xb = c->ax > c->bx ? c->ax : c->bx, ya = c->ay < c->by ? c->ay : c->by, yb = c->ay > c->by ? c->ay : c->by;
  if ((xb + rm) < 0 || (xa - rm) >> 4 >= SW || (yb + rm) < 0 || (ya - rm) >> 4 >= SH) return;
  r->x0 = (int16_t)(((xa - rm) >> 4) < 0 ? 0 : (xa - rm) >> 4); r->x1 = (int16_t)(((xb + rm) >> 4) > SW - 1 ? SW - 1 : (xb + rm) >> 4);
  r->y0 = (int16_t)((ya - rm) >> 4); r->y1 = (int16_t)((yb + rm) >> 4); r->mat = c->mat;
  for (int k = 0; k <= 64; k++) { int32_t a = k * 8 - 256, aa = a < 0 ? -a : a;
    if (aa > 200) { r->cls[k] = 16; continue; }
    int32_t lam = (a * r->pl + (int32_t)NZ1[aa] * LZ) >> 8; r->cls[k] = (uint8_t)(lam < -30 ? 5 : lam < 80 ? 10 : 14); }
  ncap++; }
static void cap(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t ra, int32_t rb, uint8_t mat) { Cap c = { ax, ay, bx, by, (int16_t)ra, (int16_t)rb, mat }; cap_add(&c); }
static void FASTF cap_row(const CapRun *c, int y, uint16_t *row) {
  int32_t py = y * 16 + 8 - c->ay, px0 = c->x0 * 16 + 8 - c->ax;
  int32_t T0 = (int32_t)(((int64_t)(px0 * c->vx + py * c->vy) >> 4) * c->invL2 >> 12), dT = (int32_t)(((int64_t)(16 * c->vx) >> 4) * c->invL2 >> 12);
  int32_t S0 = (int32_t)(((int64_t)(py * c->vx - px0 * c->vy)) * c->invL >> 28), dS = (int32_t)(((int64_t)(-16 * c->vy)) * c->invL >> 28);
  int32_t rm = c->ra > c->rb ? c->ra : c->rb, n = c->x1 - c->x0, lo = n + 1, hi = -1;
  { int32_t a0 = 0, a1 = n;
    if (dS == 0) { if (S0 <= -rm || S0 >= rm) a1 = -1; } else { int32_t e0 = (-rm - S0) / dS, e1 = (rm - S0) / dS; if (e0 > e1) { int32_t t = e0; e0 = e1; e1 = t; } e0--; e1++; if (e0 > a0) a0 = e0; if (e1 < a1) a1 = e1; }
    if (dT == 0) { if (T0 < 0 || T0 > 65536) a1 = -1; } else { int32_t e0 = (0 - T0) / dT, e1 = (65536 - T0) / dT; if (e0 > e1) { int32_t t = e0; e0 = e1; e1 = t; } e0--; e1++; if (e0 > a0) a0 = e0; if (e1 < a1) a1 = e1; }
    if (a0 <= a1) { lo = a0; hi = a1; } }
  for (int k = 0; k < 2; k++) { int32_t dy = k ? py - c->vy : py, r = k ? c->rb : c->ra; if (dy <= -r || dy >= r) continue;
    int32_t h = isq(r * r - dy * dy), cx = c->ax + (k ? c->vx : 0), a0 = ((cx - h) >> 4) - c->x0 - 1, a1 = ((cx + h) >> 4) - c->x0 + 1; if (a0 < lo) lo = a0; if (a1 > hi) hi = a1; }
  if (lo < 0) lo = 0; if (hi > n) hi = n; if (lo > hi) return;
  int32_t T = T0 + lo * dT, S = S0 + lo * dS, dxa = px0 + lo * 16, dya = py, dxb = dxa - c->vx, dyb = py - c->vy, ra2 = c->ra * c->ra, rb2 = c->rb * c->rb, dya2 = dya * dya, dyb2 = dyb * dyb;
  const uint16_t *ramp = RAMP[c->mat];
  for (int x = c->x0 + lo; x <= c->x0 + hi; x++, T += dT, S += dS, dxa += 16, dxb += 16) {
    int32_t a; int inside = 0;
    if (T >= 0 && T <= 65536) { int32_t r = c->ra + (((c->rb - c->ra) * T) >> 16), as = S < 0 ? -S : S; if (as < r) { a = (S * (c->ira + (((c->irb - c->ira) * T) >> 16))) >> 8; inside = 1; } }
    if (!inside) { if (dxa * dxa + dya2 < ra2) { a = (S * c->ira) >> 8; inside = 1; } else if (dxb * dxb + dyb2 < rb2) { a = (S * c->irb) >> 8; inside = 1; } }
    if (!inside) continue;
    if (a > 256) a = 256; if (a < -256) a = -256; int s = c->cls[(a + 260) >> 3]; row[x] = s == 16 ? INK : ramp[s]; } }

/* ---- the potato herself: an ellipsoid, shaded per dot; her face is painted on the turned body, so it turns with her */
static int16_t gaze_now;
static void FASTF tater_row(const Obj *b, int y, uint16_t *o) {
  int32_t dy = y - b->y, ny = dy * b->ivy >> 8, aya = ny < 0 ? -ny : ny; if (aya >= b->ka) return;
  int32_t hw = (b->rx * isq(b->ka * b->ka - ny * ny)) >> 8, x0 = b->x - hw - 1, x1 = b->x + hw + 1; if (x0 < 0) x0 = 0; if (x1 > SW - 1) x1 = SW - 1;
  const uint16_t *sk = RAMP[MT_SPUD]; int32_t ny2 = ny * ny;
  int32_t r1 = RM[1] * ny, r4 = RM[4] * ny, r7 = RM[7] * ny;
  for (int x = x0; x <= x1; x++) {
    int32_t nx = (x - b->x) * b->ivx >> 8, q = nx * nx + ny2;
    if (q >= 65536) { if (q < b->auraq) { uint16_t g = (((x >> 1) + (y >> 1) + (gaze_now >> 2)) & 3) ? ACCN : 0xFFFF; o[x] = SWP(avgN(SWP(o[x]), g)); } continue; }
    if (q > b->inkq) { o[x] = INK; continue; }
    int32_t nz = NZ2[q >> 8], mz = (RM[6] * nx + r7 + RM[8] * nz) >> 14, my = (RM[3] * nx + r4 + RM[5] * nz) >> 14;
    if (mz < 70 || my < -150 || (my > -58 && my < 96)) {   /* only a dot that can be on the face, the crown or round the back asks */
      int32_t mx = (RM[0] * nx + r1 + RM[2] * nz) >> 14, am = mx < 0 ? -mx : mx;
      if (mz < 70 || my < -150 || (am > 24 && am < 112 && my < 54) || (am < 32 && my > 46)) { uint16_t f = face_px(mx, my, mz, x, y, b->expr, b->mouth, gaze_now); if (f) { o[x] = f; continue; } } }
    int32_t lam = (nx * LX + ny * LY + nz * LZ) >> 8;
    { int32_t a = nx + 96, c = (ny + 118) * 3; if (a * a + c * c < 760) { o[x] = WHITE; continue; } }
    if (q > 46000 && lam < 10) { o[x] = ACC; continue; }   /* the rim catches the scene's own light */
    o[x] = sk[lam < -36 ? 4 : lam < 84 ? 10 : 13]; } }

/* ---- the little ones: the same sphere, a face that doesn't turn */
static void FASTF mini_row(const Obj *b, int y, uint16_t *o) {
  int32_t dy = y - b->y, ny = dy * b->ivy >> 8, aya = ny < 0 ? -ny : ny; if (aya >= 256) return;
  int32_t hw = (b->rx * NZ1[aya]) >> 8, x0 = b->x - hw, x1 = b->x + hw; if (x0 < 0) x0 = 0; if (x1 > SW - 1) x1 = SW - 1;
  uint8_t hb = (uint8_t)((b->hue & 15) << 4); uint16_t c0 = PALC[hb | 5], c1 = PALC[hb | 10], c2 = PALC[hb | 14];
  if (b->mat) { c0 = RAMP[MT_POTATO][4]; c1 = RAMP[MT_POTATO][10]; c2 = RAMP[MT_POTATO][13]; }   /* a real potato */
  int32_t ny2 = ny * ny, ey = ny + 28, ey2 = ey * ey * 40, my = ny - 62;
  for (int x = x0; x <= x1; x++) {
    int32_t nx = (x - b->x) * b->ivx >> 8, q = nx * nx + ny2; if (q >= 65536) continue;
    if (q > b->inkq) { o[x] = INK; continue; }
    int32_t ax = nx < 0 ? -nx : nx, ex = ax - 84;
    if (ex * ex * 52 + ey2 < 65536) {               /* eyes: dark ovals with a light in each */
      int32_t hx = (nx < 0 ? nx + 84 : nx - 84) + 10, hy = ey + 14;
      if (b->expr == E_HEART) { o[x] = heart((nx < 0 ? nx + 84 : nx - 84) / 2, ey / 2) ? RAMP[MT_IRISP][12] : (ex * ex * 52 + ey2 > 50000 ? INK : WHITE); continue; }
      o[x] = hx * hx + hy * hy < 120 ? WHITE : INK; continue; }
    if (b->mouth == M_OPEN) { int32_t mq = nx * nx * 4 + my * my * 6; if (mq < 3600) { o[x] = mq > 2600 ? INK : RAMP[MT_MOUTH][7]; continue; } }
    else if (ax < 34) { int32_t c = (1156 - nx * nx) / 40 - 4; if (my - c < 7 && my - c > -7) { o[x] = INK; continue; } }
    int32_t lam = (nx * LX + ny * LY + (int32_t)NZ2[q >> 8] * LZ) >> 8;
    { int32_t a = nx + 100, c = (ny + 120) * 3; if (a * a + c * c < 900) { o[x] = WHITE; continue; } }
    o[x] = lam < -36 ? c0 : lam < 84 ? c1 : c2; } }

static void FASTF ring_row(const Obj *b, int y, uint16_t *o) {    /* a planet's ring: an annulus seen at a slant, its back half or its front */
  int32_t dy = y - b->y; if ((b->kind == K_RINGB) != (dy < 0)) { if (dy != 0) return; }
  int32_t ro = b->rx, ri = b->rx * 62 / 100, k = b->rx * 256 / (b->ry ? b->ry : 1), v = dy * k >> 8, v2 = v * v;
  if (v2 >= ro * ro) return; int32_t bk = (12 << 20) / (ro * ro), xo = isq(ro * ro - v2), x0 = b->x - xo, x1 = b->x + xo; if (x0 < 0) x0 = 0; if (x1 > SW - 1) x1 = SW - 1;
  for (int x = x0; x <= x1; x++) { int32_t u = x - b->x, d = u * u + v2; if (d < ri * ri) continue;
    int32_t band = (d * bk) >> 20; o[x] = (band & 1) ? RAMP[MT_RINGA][dy < 0 ? 6 : 11] : RAMP[MT_RINGB][dy < 0 ? 6 : 12]; if (d > ro * ro - ro * 3) o[x] = INK; } }
static void FASTF star4_row(const Obj *b, int y, uint16_t *o) {
  int32_t dy = y - b->y, s = b->rx, ady = dy < 0 ? -dy : dy; if (ady >= s) return;
  uint16_t c = PALC[((b->hue & 15) << 4) | 15], w = WHITE;
  int32_t xs = b->x - s, xe = b->x + s; if (xs < 0) xs = 0; if (xe > SW - 1) xe = SW - 1;
  for (int x = xs; x <= xe; x++) { int32_t dx = x - b->x; if (star4(dx, dy, s)) o[x] = (dx * dx + dy * dy) * 9 < s * s ? w : c; } }

/* ================================================================ the backgrounds, a row at a time */
static uint16_t SKYR[SH]; static int16_t HILL[SW]; static int16_t sun_x, sun_y, hill_top; static uint16_t HG0, HG1, HGI, HGF;
typedef struct { int16_t x, y, r; } Puff;
static Puff CLOUD[15];
static void FASTF sky_row(int y, uint16_t *o) {
  uint16_t c = SKYR[y]; uint32_t c2 = (uint32_t)c | ((uint32_t)c << 16), *o32 = (uint32_t *)o; for (int x = 0; x < SW / 2; x++) o32[x] = c2;
  { int32_t dy = y - sun_y; if (dy > -40 && dy < 40) { int32_t h = isq(1600 - dy * dy), x0 = sun_x - h, x1 = sun_x + h; if (x0 < 0) x0 = 0; if (x1 > SW - 1) x1 = SW - 1;
      for (int x = x0; x <= x1; x++) { int32_t dx = x - sun_x, d = dx * dx + dy * dy; o[x] = d < 500 ? glass(255, 252, 220) : d < 640 ? glass(255, 236, 150) : avgW(o[x], glass(255, 240, 200)); } } }
  for (int k = 1; k <= 3; k++) {                       /* lens flares: hexagons along the line through the sun */
    int32_t fx = sun_x + (160 - sun_x) * k * 2 / 3, fy = sun_y + (120 - sun_y) * k * 2 / 3, rr = 6 + k * 5, dy = y - fy, ady = dy < 0 ? -dy : dy; if (ady >= rr) continue;
    for (int x = fx - rr; x <= fx + rr; x++) { if ((unsigned)x >= SW) continue; int32_t adx = x - fx < 0 ? fx - x : x - fx; if (adx * 222 + ady * 128 < rr * 256 && ady < rr * 7 / 8) o[x] = avgW(o[x], k == 2 ? glass(255, 160, 220) : glass(180, 255, 230)); } }
  for (int k = 0; k < 15; k++) { const Puff *p = &CLOUD[k]; int32_t dy = y - p->y; if (dy <= -p->r || dy >= p->r) continue;
    int32_t h = isq(p->r * p->r - dy * dy), x0 = p->x - h, x1 = p->x + h; if (x0 < 0) x0 = 0; if (x1 > SW - 1) x1 = SW - 1;
    uint16_t cc = dy * 3 > p->r ? glass(222, 214, 255) : WHITE; for (int x = x0; x <= x1; x++) o[x] = cc; }
  if (y < hill_top) return;
  { uint16_t g0 = HG0, g1 = HG1, gi = HGI, gf = HGF; int sh = 2 * y;
    for (int x = 0; x < SW; x++) { int h = HILL[x]; if (y < h) continue; o[x] = y < h + 2 ? gi : (((x + sh) >> 3) & 1) ? g0 : g1; }
    if ((y & 3) == 0) for (int x = (y * 13) & 31; x < SW; x += 32) if (y >= HILL[x] + 3) o[x] = gf; } }

static uint8_t LS[256], HT[256]; static uint16_t half_c0, half_c1, half_cl;   /* per heading: where its speed line starts (255: none); per distance: the dot's size */
static void FASTF half_run(const uint8_t *pa, const uint8_t *pr, uint16_t *p, int step, uint8_t base, int neg, const uint8_t *dot, int x0) {
  uint16_t c0 = half_c0, c1 = half_c1, cl = half_cl; int x = x0;
  if (neg) { for (int i = 0; i < 160; i++, p += step, x += step) { uint8_t r = pr[i], a = (uint8_t)(base - pa[i]); *p = r > LS[a] ? cl : dot[x & 7] < HT[r] ? c1 : c0; } }
  else { for (int i = 0; i < 160; i++, p += step, x += step) { uint8_t r = pr[i], a = (uint8_t)(base + pa[i]); *p = r > LS[a] ? cl : dot[x & 7] < HT[r] ? c1 : c0; } } }
static void FASTF half_row(int y, uint16_t *o) {                     /* halftone dots grown outward, and speed lines from the middle */
  int top = y < 120, qy = top ? 119 - y : y - 120; const uint8_t *pa = PA[qy], *pr = PR[qy], *dot = &DOT8[(y & 7) << 3];
  if (top) { half_run(pa, pr, o + 160, 1, 255, 1, dot, 160); half_run(pa, pr, o + 159, -1, 128, 0, dot, 159); }
  else { half_run(pa, pr, o + 160, 1, 0, 0, dot, 160); half_run(pa, pr, o + 159, -1, 127, 1, dot, 159); } }

static uint8_t k_wedges = 6, k_rot; static uint8_t tex_mode, tw_twist, tw_glow; static uint16_t tw_z, tw_spin, tw_bend; static int16_t tw_bamp; static uint8_t tw_hue;
/* Everything about a tunnel dot but its heading depends only on its distance from the middle. So once a frame, for each of 256
   distances: how far round the texture is turned there (twist, spin, the bend), how deep it is, how fogged, and the 16 colours its
   heading can pick. A dot is then one add and one look-up. */
static uint8_t UOFF[256], VV[256], TFOG[256], VPAR[256], FOLDT[256]; static uint16_t COLR[256][16]; static union { uint16_t row64[256][64]; uint8_t vox[SH][LW]; } SCR;   /* the tunnel's lining rows, or the hills: never both */
#define ROW64 SCR.row64
#define VOX SCR.vox
static const uint8_t *TROW[256]; static uint8_t TILE[32][32];
static void grow_tile(void) {                          /* one cell of the tunnel's lining: her face, 32 x 32 (levels; 0 means wall) */
  for (int v = 0; v < 32; v++) for (int u = 0; u < 32; u++) { int32_t cu = u - 16, cv = v - 16, dd = cu * cu + cv * cv, eu = (cu < 0 ? -cu : cu) - 5, ev = cv + 2;
    uint8_t l = 0; if (dd < 196) { l = dd > 150 ? 1 : (eu * eu + ev * ev < 8) ? 1 : (cv > 4 && cv < 7 && cu > -6 && cu < 6) ? 2 : (cu + cv < -8 ? 15 : 12); if (eu * eu + (ev + 1) * (ev + 1) < 2) l = 15; }
    TILE[v][u] = l; } }
static void tunnel_prep(void) {
  if (tex_mode == 3) { uint32_t wd = 65536u / k_wedges; for (int a = 0; a < 256; a++) { uint32_t w = (uint32_t)a * k_wedges, rem = w & 255; if ((w >> 8) & 1) rem = 255 - rem; FOLDT[a] = (uint8_t)(((rem * wd) >> 16) + k_rot); } }
  for (int r = 0; r < 256; r++) { uint8_t d = DEP[r], v = (uint8_t)(d + (tw_z >> 8));
    UOFF[r] = (uint8_t)(((d * tw_twist) >> 6) + (tw_spin >> 8) + ((SIN8[(uint8_t)(v * 2 + (tw_bend >> 8))] * tw_bamp) >> 14)); VV[r] = v; TFOG[r] = FOGL[r];
    uint8_t f = FOGL[r];
    if (tex_mode == 2) TROW[r] = fnow[(v * 15) >> 5];
    if (tex_mode == 1) {                              /* her face in every other cell, the wall between: 64 headings repeat, so one row of 64 per distance */
      const uint8_t *t = TILE[v & 31]; uint8_t hb = (uint8_t)((((v >> 5) + tw_hue) & 15) << 4), par = (uint8_t)((v >> 5) & 1), wl = (uint8_t)(f > 5 ? 5 : f);
      for (int u = 0; u < 64; u++) { uint8_t cell = (uint8_t)(((u >> 5) ^ par) & 1), l = t[u & 31]; uint16_t c;
        if (cell && l) { if (l > f) l = f; c = PALC[hb | l]; } else c = PALC[((((u >> 4) & 3) + tw_hue + 7) & 15) << 4 | wl];
        if (r < tw_glow) c = r < (tw_glow >> 1) ? WHITE : avgW(c, WHITE); ROW64[r][u] = c; } }
    for (int k = 0; k < 16; k++) { uint8_t lev = ((k ^ (v >> 3)) & 1) ? f : (uint8_t)(f * 9 >> 4), hue = (uint8_t)(((v >> 4) + (k >> 2) + tw_hue) & 15);
      if (tex_mode == 1) { hue = (uint8_t)(((k >> 1) + tw_hue + 7) & 15); lev = (uint8_t)(f > 5 ? 5 : f); }
      uint16_t c = PALC[hue << 4 | lev]; if (r < tw_glow) c = r < (tw_glow >> 1) ? WHITE : avgW(c, WHITE); COLR[r][k] = c; } } }
#define TUN_LOOP(BODY) do { if (neg) { for (int i = 0; i < 160; i++, p += step) { uint8_t r = pr[i], a = (uint8_t)(base - pa[i]); BODY } } \
                               else { for (int i = 0; i < 160; i++, p += step) { uint8_t r = pr[i], a = (uint8_t)(base + pa[i]); BODY } } } while (0)
static void FASTF tunnel_run(const uint8_t *pa, const uint8_t *pr, uint16_t *p, int step, uint8_t base, int neg) {   /* 160 dots of one half-row */
  if (tex_mode == 0) { TUN_LOOP(*p = COLR[r][((uint8_t)(a + UOFF[r]) >> 4) & 15];); return; }
  if (tex_mode == 3) { TUN_LOOP(*p = COLR[r][((uint8_t)(FOLDT[a] + UOFF[r]) >> 4) & 15];); return; }   /* the tunnel folded into mirrored wedges */
  if (tex_mode == 1) { TUN_LOOP(*p = ROW64[r][(uint8_t)(a + UOFF[r]) & 63];); return; }
  TUN_LOOP(uint8_t u = (uint8_t)(a + UOFF[r]); uint8_t s = TROW[r][(u * 5) >> 3]; uint16_t c = COLR[r][(u >> 4) & 15];   /* the lining is the field */
    if ((s & 15) > 5 && r >= tw_glow) { uint8_t l = TFOG[r]; c = PALC[(s & 0xF0) | (((u >> 4) ^ (VV[r] >> 3)) & 1 ? l : (l * 9 >> 4))]; }
    *p = c;); }
static void FASTF tunnel_row(int y, uint16_t *o) {
  int top = y < 120, qy = top ? 119 - y : y - 120; const uint8_t *pa = PA[qy], *pr = PR[qy];
  if (top) { tunnel_run(pa, pr, o + 160, 1, 255, 1); tunnel_run(pa, pr, o + 159, -1, 128, 0); }
  else { tunnel_run(pa, pr, o + 160, 1, 0, 0); tunnel_run(pa, pr, o + 159, -1, 127, 1); } }

static int32_t k_zoom; static uint32_t k_wdiv; static uint16_t KROW[SW];
static void FASTF kalei_row(int y, uint16_t *o) {                     /* the field, folded into mirrored wedges: every dot asks where its wedge came from */
  int top = y < 120, qy = top ? 119 - y : y - 120; const uint8_t *pa = PA[qy], *pr = PR[qy]; uint32_t *o32 = (uint32_t *)o;
  for (int h = 0; h < LW; h++) { int x = 2 * h, qx = x >= 160 ? x - 160 : 159 - x; uint8_t a0 = pa[qx];
    uint8_t a = x >= 160 ? (uint8_t)(top ? 255 - a0 : a0) : (uint8_t)(top ? 128 + a0 : 127 - a0);
    uint32_t w = (uint32_t)a * k_wedges, rem = w & 255; if ((w >> 8) & 1) rem = 255 - rem;
    uint8_t a3 = (uint8_t)(((rem * k_wdiv) >> 16) + k_rot); int32_t rr = (pr[qx] * k_zoom) >> 8;
    int sx = 80 + ((rr * COS8(a3)) >> 14), sy = 60 + ((rr * SIN8[a3]) >> 14);
    o32[h] = ((unsigned)sx < LW && (unsigned)sy < LH) ? PAL2[fnow[sy][sx]] : PAL2[0]; } }
static void FASTF field_row(int y, uint16_t *o) {
  int fy = y >> 1; const uint8_t *r0 = fnow[fy], *r1 = fnow[fy + 1 < LH ? fy + 1 : fy]; uint32_t *o32 = (uint32_t *)o;
  if (!(y & 1)) { for (int h = 0; h < LW; h++) o32[h] = PAL2[r0[h]]; return; }
  for (int h = 0; h < LW; h++) { uint16_t c = SWP(avgN(PALN[r0[h]], PALN[r1[h]])); o32[h] = (uint32_t)c | ((uint32_t)c << 16); } }

static void FASTF vox_row(int y, uint16_t *o) {
  const uint8_t *v = VOX[y], *f0 = fnow[y >> 1]; uint32_t *o32 = (uint32_t *)o;
  for (int h = 0; h < LW; h++) { uint8_t c = v[h]; o32[h] = c ? PAL2[c] : PAL2[f0[h]]; } }
static Seg SEG[NSTAR]; static int nseg;
static void FASTF space_row(int y, uint16_t *o) {
  uint16_t c = SKYR[y]; uint32_t c2 = (uint32_t)c | ((uint32_t)c << 16), *o32 = (uint32_t *)o; for (int x = 0; x < SW / 2; x++) o32[x] = c2;
  for (int i = 0; i < nseg; i++) { const Seg *s = &SEG[i]; int ya = s->y0 < s->y1 ? s->y0 : s->y1, yb = s->y0 < s->y1 ? s->y1 : s->y0; if (y < ya || y > yb) continue;
    int xa, xb; if (s->y1 == s->y0) { xa = s->x0; xb = s->x1; } else { xa = s->x0 + (s->x1 - s->x0) * (y - s->y0) / (s->y1 - s->y0); xb = s->x0 + (s->x1 - s->x0) * (y + 1 - s->y0) / (s->y1 - s->y0); }
    if (xa > xb) { int t = xa; xa = xb; xb = t; } if (xb == xa) xb++;
    for (int x = xa; x < xb; x++) if ((unsigned)x < SW) o[x] = s->c; } }

/* ================================================================ letters (THE FISH AFFECT's face, and ゴ) */
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
static const uint8_t GO[7] = { 0x0A, 0x05, 0xFC, 0x04, 0x04, 0x04, 0xFC };   /* ゴ, eight across: the menacing letter */
static uint8_t GLYPH[128];
static uint16_t *OBUF; static int ob_y0;
static inline void opix(int x, int y, uint16_t c) { y -= ob_y0; if ((unsigned)x < SW && (unsigned)y < BAND) OBUF[y * SW + x] = c; }
static uint16_t aura_ph;
static int text_w(const char *s, int sc) { int n = 0; for (; *s; s++) n += (*s == '#') ? 9 : 6; return n * sc - sc; }
static void otext(int x, int y, const char *s, uint16_t c, uint16_t shade, int sc, int melt) {
  if (y > ob_y0 + BAND + 8 || y + 7 * sc + 12 + melt < ob_y0) return;
  for (; *s; s++) { int ch = *s; if (ch >= 'a' && ch <= 'z') ch -= 32;
    int wcols = ch == '#' ? 8 : 5;
    for (int k = 0; k < wcols; k++) { int dy = melt ? (fsin((int16_t)(aura_ph * 2 + (x + k * sc) * 50)) * melt >> 14) : 0;
      for (int r = 0; r < 7; r++) { int on;
        if (ch == '#') on = (GO[r] >> (7 - k)) & 1; else { int g = GLYPH[ch & 127]; on = g ? (FACE[g - 1][r] >> (4 - k)) & 1 : 0; }
        if (!on) continue;
        int py = y + r * sc + dy; if (py + sc + 2 < ob_y0 || py > ob_y0 + BAND) continue;
        for (int yy = 0; yy < sc; yy++) for (int xx = 0; xx < sc; xx++) { opix(x + k * sc + xx + 2, py + yy + 2, shade); opix(x + k * sc + xx, py + yy, c); } } }
    x += (ch == '#' ? 9 : 6) * sc; } }
static void orect(int x, int y, int w, int h, uint16_t c) { int ya = y > ob_y0 ? y : ob_y0, yb = y + h < ob_y0 + BAND ? y + h : ob_y0 + BAND; for (int yy = ya; yy < yb; yy++) for (int xx = x; xx < x + w; xx++) opix(xx, yy, c); }

/* ================================================================ the story */
static Tater T;
static uint8_t stage, loopn, beat_hit, impact, flash_n, rgb_k, glitch, wob, iris_on, hud_on; static uint16_t st_left, st_len, beat, beat_n, trip;
static uint32_t hud_fps = 30;
static int16_t shake, iris_r, cam_dx, cam_dy;
static Spark SPK[NSPARK]; static int spk_at;
static Word WD[NWORD]; static int wd_at;
static const char *bubble; static uint8_t bubble_t;
static uint16_t field_ang, sw_ph, ring_ph, jul_ph, hue_ph; static int32_t SWR[9];
static int16_t stx[NSTAR], sty[NSTAR]; static uint16_t stz[NSTAR]; static uint16_t star_v;
static int32_t cam_x, cam_y, cam_h; static uint16_t cam_head;
static const uint16_t STAGE_LEN[S_N] = { 210, 210, 250, 240, 240, 250, 250, 330, 150 };
static void word(int x, int y, const char *s, uint8_t hue, uint8_t sc, uint8_t life) { Word *w = &WD[wd_at]; wd_at = (wd_at + 1) % NWORD; w->x = (int16_t)x; w->y = (int16_t)y; w->life = life; w->hue = hue; w->s = s; w->sc = sc; w->melt = (uint8_t)(sc * 2); }
static void spark(int32_t x, int32_t y, int16_t vx, int16_t vy, uint8_t life, uint8_t hue, uint8_t size) { Spark *s = &SPK[spk_at]; spk_at = (spk_at + 1) % NSPARK; s->x = x; s->y = y; s->vx = vx; s->vy = vy; s->life = life; s->hue = hue; s->size = size; }
static void burst(int32_t x, int32_t y, int n, int sp) { for (int k = 0; k < n; k++) { uint16_t a = (uint16_t)(k * 4096 / n + aura_ph); uint16_t d = f_draw(); spark(x, y, (int16_t)(fsin((int16_t)a) * (sp + (d & 15)) >> 14), (int16_t)(fcos((int16_t)a) * (sp + (d & 15)) >> 14), (uint8_t)(18 + (d >> 12)), (uint8_t)((k + (aura_ph >> 9)) & 15), (uint8_t)(4 + ((d >> 4) & 3))); } }
static void set_pose(uint8_t p) {
  static const int16_t PO[6][4] = { {300, 120, 300, 120}, {300, 120, 1750, 300}, {900, 1300, 900, 1300}, {1900, 100, 1900, 100}, {1550, 1500, 1550, 1500}, {1300, 900, 700, 1200} };
  for (int k = 0; k < 4; k++) T.armt[k] = PO[p][k]; }
static void swarm_begin(void) { for (int k = 0; k < 9; k++) SWR[k] = (k % 4 == 0) ? 16384 : 0; }
static void swarm_turn(int16_t a, int16_t b) {                     /* the carried rotation: nudged about two axes, then landed on true (Gram-Schmidt in whole numbers) */
  int32_t ca = fcos(a), sa = fsin(a), cb = fcos(b), sb = fsin(b);
  for (int r = 0; r < 3; r++) { int32_t *v = &SWR[r * 3]; int32_t x = v[0], y = v[1], z = v[2];
    int32_t x1 = (x * ca - z * sa) >> 14, z1 = (x * sa + z * ca) >> 14; int32_t y2 = (y * cb - z1 * sb) >> 14, z2 = (y * sb + z1 * cb) >> 14; v[0] = x1; v[1] = y2; v[2] = z2; }
  int32_t *a0 = &SWR[0], *a1 = &SWR[3], *a2 = &SWR[6];
  int32_t l = isq((a0[0] * a0[0] + a0[1] * a0[1] + a0[2] * a0[2]) >> 4) << 2; if (l) for (int k = 0; k < 3; k++) a0[k] = a0[k] * 16384 / l;
  int32_t d = (a1[0] * a0[0] + a1[1] * a0[1] + a1[2] * a0[2]) >> 14; for (int k = 0; k < 3; k++) a1[k] -= (d * a0[k]) >> 14;
  l = isq((a1[0] * a1[0] + a1[1] * a1[1] + a1[2] * a1[2]) >> 4) << 2; if (l) for (int k = 0; k < 3; k++) a1[k] = a1[k] * 16384 / l;
  a2[0] = (a0[1] * a1[2] - a0[2] * a1[1]) >> 14; a2[1] = (a0[2] * a1[0] - a0[0] * a1[2]) >> 14; a2[2] = (a0[0] * a1[1] - a0[1] * a1[0]) >> 14; }

static void tater_place(int32_t x, int32_t y, int32_t r) { T.tx = x * 16; T.ty = y * 16; T.rt = r * 16; }
static void stage_begin(uint8_t s) {
  stage = s; st_len = st_left = STAGE_LEN[s]; T.aura = 0; T.wand = 0; T.limbs = 1; rgb_k = 0; glitch = 0; T.yawt = 0; T.pitch_amp = 60; bubble = 0; hud_on = 0;
  uint8_t L = loopn > 3 ? 3 : loopn;
  switch (s) {
    case S_MORNING: tater_place(160, 160, 44); T.expr = E_NORMAL; T.mouth = M_SMILE; set_pose(P_DOWN); T.bob_amp = 3; break;
    case S_SPARK: tater_place(160, 150, 64); T.expr = E_SPARK; T.mouth = M_OPEN; set_pose(P_FISTS); word(160, 30, "SUGOI!!", 3, 4, 70); T.sqv = 60; break;
    case S_HENSHIN: tater_place(160, 116, 52); T.expr = E_SPARK; T.mouth = M_CAT; set_pose(P_UP); T.wand = 1; T.aura = 1; T.yawt = (int16_t)(90 + L * 30); T.pitch_amp = 300; word(160, 26, "TATER POWER...", 10, 2, 90); tex_mode = 0; break;
    case S_SENPAI: tater_place(160, 120, 46); T.expr = E_HEART; T.mouth = M_CAT; set_pose(P_HEART); T.aura = 1; word(160, 22, "NOTICE ME", 11, 3, 100); ring_ph = 0; k_wedges = (uint8_t)(6 + L * 2); break;
    case S_SOUL: tater_place(160, 186, 34); T.expr = E_GLOW; T.mouth = M_TINY; set_pose(P_DOWN); T.aura = 1; word(160, 18, "MY SOUL IS A", 6, 2, 110); break;
    case S_WORLD: tater_place(160, 62, 50); T.expr = E_GLOW; T.mouth = M_SMILE; T.limbs = 0; T.aura = 1; word(160, 20, "I AM BECOME FRY", 2, 2, 120); cam_h = 200 * 16; break;
    case S_COSMOS: tater_place(160, 120, 58); T.expr = E_STAR; T.mouth = M_SMILE; T.limbs = 0; T.aura = 1; star_v = 40; word(160, 24, "THE UNIVERSE", 15, 3, 90); T.yawt = 20; break;
    case S_MADNESS: tater_place(160, 120, 48); T.expr = E_STAR; T.mouth = M_OPEN; set_pose(P_DANCE); T.aura = 1; T.wand = 1; T.yawt = (int16_t)(160 + L * 60); T.pitch_amp = 400; tex_mode = 3; k_wedges = (uint8_t)(6 + L * 2); swarm_begin(); break;
    default: tater_place(160, 160, 44); T.expr = E_CLOSED; T.mouth = M_SMILE; set_pose(P_DOWN); T.bob_amp = 2; bubble = "...JUST A POTATO."; bubble_t = 255; break; } }

static void story_begin(void) {
  memset(&T, 0, sizeof T); T.x = T.tx = 160 * 16; T.y = T.ty = 160 * 16; T.r = T.rt = 44 * 16; T.blink_t = 90;
  for (int k = 0; k < 3; k++) { T.hx[k] = T.x; T.hy[k] = T.y - (44 + 6 * k) * 16; }
  for (int k = 0; k < 15; k++) { CLOUD[k].x = (int16_t)(30 + (k / 3) * 70 + (k % 3) * 14); CLOUD[k].y = (int16_t)(40 + ((k / 3) & 1) * 24 + ((k % 3) == 1 ? -6 : 0)); CLOUD[k].r = (int16_t)(12 + ((k % 3) == 1 ? 6 : 0)); }
  for (int i = 0; i < NSTAR; i++) { stx[i] = (int16_t)((int32_t)(f_draw() & 1023) - 512); sty[i] = (int16_t)((int32_t)(f_draw() & 1023) - 512); stz[i] = (uint16_t)(16 + (f_draw() % 1000)); }
  cam_x = 64 << 8; cam_y = 64 << 8; cam_head = 0; swarm_begin(); stage_begin(S_MORNING); }

static void tater_step(void) {                       /* every part is a rate pulled toward where it is asked to be */
  T.bob = (uint16_t)(T.bob + 96); int32_t by = (int32_t)fsin((int16_t)T.bob) * T.bob_amp >> 10;
  T.vx = (T.vx * 200 >> 8) + ((T.tx - T.x) * 14 >> 8); T.vy = (T.vy * 200 >> 8) + ((T.ty + by - T.y) * 14 >> 8); T.x += T.vx; T.y += T.vy;
  T.rv = (T.rv * 190 >> 8) + ((T.rt - T.r) * 20 >> 8); T.r += T.rv;
  T.sqv = (int16_t)((T.sqv * 220 >> 8) - (T.sq * 30 >> 8)); T.sq = (int16_t)(T.sq + T.sqv); if (T.sq > 64) T.sq = 64; if (T.sq < -64) T.sq = -64;
  T.yawv = (int16_t)(T.yawv + ((T.yawt - T.yawv) >> 4)); T.yaw = (uint16_t)(T.yaw + T.yawv); T.pitch_ph = (uint16_t)(T.pitch_ph + 70);
  if (T.yawt == 0) { int16_t back = (int16_t)(((int16_t)(T.yaw & 4095) > 2048 ? 4096 - (T.yaw & 4095) : -(int16_t)(T.yaw & 4095))); T.yawv = (int16_t)(T.yawv + back / 24); }   /* no spin asked: the face comes home */
  for (int k = 0; k < 4; k++) { T.armv[k] = (int16_t)((T.armv[k] * 180 >> 8) + ((T.armt[k] - T.arm[k]) * 40 >> 8)); T.arm[k] = (int16_t)(T.arm[k] + T.armv[k]); }
  if (T.blink_t) T.blink_t--; else { T.blink = 6; T.blink_t = (uint16_t)(70 + (f_draw() & 127)); }
  if (T.blink) T.blink--;
  int32_t ry = T.r * (256 - T.sq) >> 8, ax = T.x, ay = T.y - ry + 16;    /* the ahoge, each point lagging the one below (THE FISH AFFECT's locks) */
  int16_t sway = (int16_t)(fsin((int16_t)(T.bob * 2)) >> 6);
  for (int k = 0; k < 3; k++) { int32_t seg = T.r / 4, tx = ax + ((int32_t)fsin((int16_t)(2048 - 260 + sway + k * 200)) * seg >> 14), ty = ay + ((int32_t)fcos((int16_t)(2048 - 260 + sway + k * 200)) * seg >> 14);
    T.hvx[k] = (int16_t)((T.hvx[k] * 150 >> 8) + ((tx - T.hx[k]) * 70 >> 8)); T.hvy[k] = (int16_t)((T.hvy[k] * 150 >> 8) + ((ty - T.hy[k]) * 70 >> 8)); T.hx[k] += T.hvx[k]; T.hy[k] += T.hvy[k]; ax = T.hx[k]; ay = T.hy[k]; } }

static void story_step(void) {
  aura_ph = (uint16_t)(aura_ph + 160);
  uint16_t ob = beat; beat = (uint16_t)(beat + 4660 + (trip >> 2)); beat_hit = beat < ob; if (beat_hit) beat_n++;   /* 128 a minute, quickening */
  trip = (uint16_t)(trip + 1 + loopn);
  if (impact) impact--; if (flash_n) flash_n--; if (shake > 0) shake--;
  uint16_t el = (uint16_t)(st_len - st_left);        /* how far into the stage: the countdown read the other way */
  uint8_t L = loopn > 3 ? 3 : loopn;
  iris_on = 0;
  if (st_left < 14 && stage != S_MADNESS) { iris_on = 1; iris_r = (int16_t)(st_left * 15); }
  if (el < 14 && !(stage == S_MORNING && loopn == 0)) { iris_on = 1; iris_r = (int16_t)(el * 15); }
  switch (stage) {
    case S_MORNING:
      if (el == 70) set_pose(P_WAVE);
      if (el == 120) { static const char *const DENY[4] = { "JUST A POTATO.", "DEFINITELY A POTATO.", "100% POTATO. NO BERRY.", "WHAT IS A BLUEBERRY?" }; set_pose(P_DOWN); bubble = DENY[loopn > 3 ? 3 : loopn]; bubble_t = 70; }
      if (el == 170) { spark(330 * 16, 20 * 16, -46, 26, 40, 3, 9); }
      if (el == 190) { T.expr = E_SHOCK; T.mouth = M_TINY; T.sqv = -70; burst(T.x, T.y - T.r, 14, 40); }
      break;
    case S_SPARK:
      if (beat_hit) { shake = 6; T.sqv = (int16_t)(T.sqv + 40); }
      if (el == 60) word(40, 150, "###", 9, 4, 120); if (el == 90) word(282, 120, "###", 9, 4, 110);
      if (st_left == 20) { word(160, 196, "NANI?!", 1, 5, 40); impact = 6; shake = 14; T.expr = E_SHOCK; T.rt = 70 * 16; }
      break;
    case S_HENSHIN:
      if (beat_hit) { flash_n = 2; burst(T.x, T.y, 10 + L * 4, 46); T.sqv = (int16_t)(T.sqv - 50); }
      if (el == 100) { word(160, 156, "AGGRESSIVE", 3, 4, 80); tex_mode = 1; } if (el == 112) word(160, 196, "TRIPPING!!", 11, 4, 70); if (el == 120) T.yawt = (int16_t)(T.yawt + 160);
      if (st_left == 40) { word(160, 32, "MAGICAL", 11, 4, 40); word(160, 190, "TATER-CHAN!", 3, 4, 40); impact = 5; T.yawt = 0; }
      tw_twist = (uint8_t)(20 + el / 3); tw_glow = (uint8_t)(18 + ((beat >> 12) & 7)); tw_bamp = (int16_t)(el * 3 + L * 60); break;
    case S_SENPAI:
      ring_ph = (uint16_t)(ring_ph + 30 + L * 6); if (el == 70) word(160, 196, "SENPAI!!", 11, 4, 100); if (el == 170) word(160, 30, "UWU", 15, 5, 60);
      if (beat_hit) T.sqv = (int16_t)(T.sqv + 35);
      k_wedges = (uint8_t)(6 + (el / 60) * 2 + L * 2); break;
    case S_SOUL:
      jul_ph = (uint16_t)(jul_ph + 7 + L); if (el == 50) word(160, 40, "JULIA SET", 6, 4, 140); if (el == 150) word(160, 210, "Z = Z*Z + C", 15, 2, 90); break;
    case S_WORLD:
      if (el == 120) word(160, 210, "POTATO HILLS", 4, 2, 100); if (el > 30 && T.ty > 40 * 16) T.ty -= 4; break;
    case S_COSMOS:
      if (star_v > 4 && el > 40) star_v = (uint16_t)(star_v - 1); ring_ph = (uint16_t)(ring_ph + 24);
      if (el == 90) word(160, 200, "WAS POTATO", 3, 3, 80); if (el == 170) word(160, 200, "ALL ALONG", 11, 4, 80); if (el == 212) word(160, 10, "(BLUEBERRY, ACTUALLY)", 6, 2, 36);
      if (el == 60) { tater_place(160, 116, 62); } break;
    case S_MADNESS: {
      static const char *const SPAM[16] = { "TATER!", "SPUD!", "BLUEBERRY?", "INFINITE CARBS", "SUGOI", "OMAE WA MOU MASHED", "NOT A BERRY", "FRY FRY FRY", "STARCH!", "NANI?!", "###", "I AM POTATO", "AGGRESSIVE TRIPPING", "UWU", "MASHED!", "ANTIOXIDANTS?!" };
      swarm_turn((int16_t)(18 + L * 6), (int16_t)(11 + L * 4)); sw_ph = (uint16_t)(sw_ph + 20 + L * 4);
      if (beat_hit) { const char *s = SPAM[beat_n % 16]; uint16_t d = f_draw(); word(40 + (d % 240), 30 + ((d >> 8) % 180), s, (uint8_t)(beat_n & 15), (uint8_t)(2 + (d & 1)), 30);
        T.expr = (beat_n & 1) ? E_STAR : E_GLOW; T.sqv = (int16_t)(T.sqv + 50); burst(T.x, T.y, 8, 50); if (st_left > 34) { if ((beat_n & 3) == 0) { impact = 3; shake = 10; } else if (beat_n & 1) flash_n = 1; } }
      rgb_k = (uint8_t)(2 + el / 60 + L); glitch = (uint8_t)(el / 20 + L * 3); tw_twist = (uint8_t)(30 + el / 4); tw_glow = 14; tw_bamp = (int16_t)(400 + el * 4);
      if (st_left == 30) { word(160, 120, "TATER-CHAN", 3, 4, 30); flash_n = 30; }
      break; }
    default:
      if (el == 40) word(160, 214, "(SHE IS A BLUEBERRY.)", 9, 2, 105); if (el == 60) bubble = 0; if (el == 70) hud_on = 1; break; }
  wob = 0; if (L && stage != S_MORNING && stage != S_END) { wob = (uint8_t)(L * 2); if (stage >= S_SOUL && rgb_k < L * 2) rgb_k = (uint8_t)(L * 2); }   /* each loop, the madness leaks further back */
  tater_step();
  for (int i = 0; i < NSPARK; i++) { Spark *s = &SPK[i]; if (!s->life) continue; s->life--; s->x += s->vx; s->y += s->vy; s->vy = (int16_t)(s->vy + 1);
    if (stage == S_MORNING && s->size == 9 && s->life < 4) s->life = 0; }
  for (int i = 0; i < NWORD; i++) if (WD[i].life) WD[i].life--;
  if (bubble_t && bubble_t != 255) { bubble_t--; if (!bubble_t) bubble = 0; }
  if (--st_left == 0) { if (stage == S_END) { loopn++; stage_begin(S_MORNING); } else stage_begin((uint8_t)(stage + 1)); } }

/* ================================================================ the frame */
static uint16_t STRIPB[2][SW * BAND]; static uint8_t LUMB[256];
static uint32_t frames, cyc_story, cyc_prep, cyc_draw, cyc_bg, cyc_obj, cyc_post, cyc_over;
static uint8_t bg_now;
static void prep(void) {
  uint8_t L = loopn > 3 ? 3 : loopn; uint16_t el = (uint16_t)(st_len - st_left);
  hue_ph = (uint16_t)(hue_ph + 40 + trip / 64 + L * 20);
  palette_turn((uint8_t)(hue_ph >> 8), 255);
  ACC = glass(255, 120, 220);
  switch (stage) {
    case S_MORNING: case S_END: bg_now = BG_SKY; ACC = glass(255, 236, 160);
      for (int y = 0; y < SH; y++) SKYR[y] = glass(70 + y * 120 / SH, 140 + y * 90 / SH, 255);
      sun_x = 250; sun_y = 52;
      for (int k = 0; k < 15; k++) { CLOUD[k].x = (int16_t)(CLOUD[k].x - ((k / 3) & 1 ? 1 : 0) - (((aura_ph >> 5) & 1) ? 1 : 0)); if (CLOUD[k].x < -50) CLOUD[k].x = (int16_t)(CLOUD[k].x + 420); }
      hill_top = SH; for (int x = 0; x < SW; x++) { HILL[x] = (int16_t)(196 + (fsin((int16_t)(x * 9 + 300)) * 10 >> 14) + (fsin((int16_t)(x * 23 + (aura_ph >> 3))) * 3 >> 14)); if (HILL[x] < hill_top) hill_top = HILL[x]; }
      HG0 = glass(110, 220, 90); HG1 = glass(90, 200, 80); HGI = glass(30, 110, 50); HGF = glass(255, 240, 120);
      break;
    case S_SPARK: { bg_now = BG_HALF; uint8_t h = (uint8_t)((hue_ph >> 9) & 15); half_c0 = glass(255, 214, 236); half_c1 = PALC[(uint8_t)(h << 4 | 12)]; half_cl = WHITE; ACC = PALC[(uint8_t)(((h + 8) & 15) << 4 | 15)];
      for (int r = 0; r < 256; r++) { int32_t t = (r * 4 - (int32_t)((beat >> 10) & 63)) / 9; HT[r] = (uint8_t)(t < 0 ? 0 : t > 40 ? 40 : t); }
      if (beat_hit || el == 0) for (int a = 0; a < 256; a++) { uint16_t d = f_draw(); LS[a] = (d & 7) < 2 + (el > 120) ? (uint8_t)(60 + ((d >> 4) & 63)) : 255; }
      break; }
    case S_HENSHIN: case S_MADNESS: bg_now = BG_TUNNEL; tw_z = (uint16_t)(tw_z + 900 + L * 300 + (stage == S_MADNESS ? 600 : 0)); tw_spin = (uint16_t)(tw_spin + 300 + L * 100); tw_bend = (uint16_t)(tw_bend + 700); tw_hue = (uint8_t)(hue_ph >> 9);
      ACC = PALC[(uint8_t)((((hue_ph >> 9) + 5) & 15) << 4 | 15)];
      if (stage == S_MADNESS) { k_rot = (uint8_t)(k_rot + 2 + L); if (beat_hit && (beat_n & 7) == 0 && k_wedges < 16) k_wedges = (uint8_t)(k_wedges + 2); }
      break;
    case S_SENPAI: bg_now = BG_KALEI; field_ang = (uint16_t)(field_ang + 23); k_rot = (uint8_t)(k_rot + 1 + L); k_zoom = 92 + (fsin((int16_t)(aura_ph >> 1)) >> 9); k_wdiv = 65536u / k_wedges;
      field_step(63200, (int16_t)((fsin((int16_t)field_ang) >> 9) + 6), (uint8_t)(frames & 1), 8, (uint16_t)(aura_ph >> 2)); ACC = PALC[(uint8_t)((((hue_ph >> 9) + 11) & 15) << 4 | 15)]; break;
    case S_SOUL: { bg_now = BG_FIELD; field_ang = (uint16_t)(field_ang + 9); field_step(65000 - L * 400, (int16_t)(4 + L * 2), (uint8_t)((frames % 3) == 0), 3, (uint16_t)(aura_ph >> 3)); ACC = glass(120, 255, 240);
      int32_t cr = (int32_t)fcos((int16_t)jul_ph) * 3230 >> 14, ci = (int32_t)fsin((int16_t)jul_ph) * 3230 >> 14;    /* c = 0.7885 e^(i phase), on 4096 */
      for (int y = 0; y < LH; y++) { uint8_t *o = fnext[y]; int32_t zi0 = (y - 60) * 82;
        for (int x = 0; x < LW; x++) { int32_t zr = (x - 80) * 82, zi = zi0; int n = 0;
          for (; n < 12; n++) { int32_t r2 = (zr * zr) >> 12, i2 = (zi * zi) >> 12; if (r2 + i2 > 16384) break; zi = ((zr * zi) >> 11) + ci; zr = r2 - i2 + cr; }
          uint8_t v = n >= 12 ? (uint8_t)(((hue_ph >> 9) + 8) << 4 | 2) : (uint8_t)((((n + (hue_ph >> 8)) & 15) << 4) | (n > 1 ? 15 - (n >> 1) : 0)); if ((v & 15) >= (o[x] & 15) || n >= 12) o[x] = v; } }
      break; }
    case S_WORLD: { bg_now = BG_FIELD; field_ang = (uint16_t)(field_ang + 11); field_step(64000, (int16_t)(fsin((int16_t)field_ang) >> 10), (uint8_t)(frames & 1), 5, (uint16_t)(aura_ph >> 2)); ACC = glass(255, 200, 80);
      for (int k = 0; k < 6; k++) fdisc(80 + (fsin((int16_t)(aura_ph * 3 + k * 682)) * 70 >> 14), 22 + (fcos((int16_t)(aura_ph * 2 + k * 682)) * 12 >> 14), 2, (uint8_t)((k * 3 + (hue_ph >> 9)) & 15), 15);
      cam_head = (uint16_t)(cam_head + (fsin((int16_t)(aura_ph >> 1)) >> 10)); cam_x += (int32_t)fcos((int16_t)cam_head) * (60 + L * 20) >> 10; cam_y += (int32_t)fsin((int16_t)cam_head) * (60 + L * 20) >> 10;
      { int32_t g = HM[(cam_y >> 8) & 127][(cam_x >> 8) & 127] * 16 + 90 * 16; if (g < 200 * 16) g = 200 * 16; cam_h += (g - cam_h) >> 4; }
      const int hor = 92;                             /* the hills: a column at a time, near to far, each column drawn only where nothing nearer stands */
      memset(VOX, 0, sizeof(SCR.vox));
      for (int c = 0; c < LW; c++) { int16_t ang = (int16_t)(cam_head + (c - 80) * 5); int32_t dx = fcos(ang), dy = fsin(ang); int ybuf = SH; int32_t z = 4;
        while (z < 760 && ybuf > 0) { int32_t px = cam_x + ((dx * z) >> 6), py = cam_y + ((dy * z) >> 6); int mx = (px >> 8) & 127, my = (py >> 8) & 127; int h = HM[my][mx];
          int32_t ys = hor + (int32_t)(((int64_t)(cam_h - h * 16) * INVZ[z < 1024 ? z : 1024]) >> 14);
          if (ys < ybuf) { int sl = h - HM[(my + 1) & 127][(mx + 1) & 127]; int lev = 10 + sl - z / 70; lev = lev < 1 ? 1 : lev > 15 ? 15 : lev;
            uint8_t col = (uint8_t)((((h >> 4) + (hue_ph >> 9)) & 15) << 4 | lev); if (col == 0) col = 1; int y0 = ys < 0 ? 0 : (int)ys; for (int y = y0; y < ybuf; y++) VOX[y][c] = col; ybuf = y0; }
          z += 1 + (z >> 5); } }
      break; }
    case S_COSMOS: { bg_now = BG_SPACE; ACC = glass(255, 230, 120);
      for (int y = 0; y < SH; y++) SKYR[y] = glass(8 + y / 16, 4, 24 + (y * 30 / SH));
      nseg = 0; for (int i = 0; i < NSTAR; i++) { int32_t z0 = stz[i]; stz[i] = (uint16_t)(stz[i] > star_v + 16 ? stz[i] - star_v : 1000 + (f_draw() & 15));
        int32_t z1 = stz[i], z2 = z1 + star_v * 3; if (z2 > 1024) z2 = 1024; if (z1 > z0) continue;
        int32_t x1 = 160 + ((stx[i] * (int32_t)INVZ[z1]) >> 9), y1 = 120 + ((sty[i] * (int32_t)INVZ[z1]) >> 9), x2 = 160 + ((stx[i] * (int32_t)INVZ[z2]) >> 9), y2 = 120 + ((sty[i] * (int32_t)INVZ[z2]) >> 9);
        if ((x1 < 0 && x2 < 0) || (x1 >= SW && x2 >= SW) || (y1 < 0 && y2 < 0) || (y1 >= SH && y2 >= SH)) continue;
        Seg *s = &SEG[nseg++]; s->x0 = (int16_t)x2; s->y0 = (int16_t)y2; s->x1 = (int16_t)x1; s->y1 = (int16_t)y1; s->c = z1 < 300 ? WHITE : PALC[(uint8_t)(((i + (hue_ph >> 9)) & 15) << 4 | (z1 < 600 ? 13 : 8))]; }
      break; }
  }
  ACCN = SWP(ACC);
  if (bg_now == BG_TUNNEL) tunnel_prep();
  /* the objects of this frame */
  nobj = 0; ncap = 0;
  int32_t rx = T.r * (256 + T.sq) >> 12, ry = T.r * (256 - T.sq) >> 12, cx = T.x >> 4, cy = T.y >> 4;
  { int32_t cy_ = fcos((int16_t)T.yaw), sy_ = fsin((int16_t)T.yaw), p = (int32_t)fsin((int16_t)T.pitch_ph) * T.pitch_amp >> 14, cp = fcos((int16_t)p), sp = fsin((int16_t)p);
    RM[0] = cy_; RM[1] = 0; RM[2] = sy_; RM[3] = (sp * sy_) >> 14; RM[4] = cp; RM[5] = -(sp * cy_) >> 14; RM[6] = -(cp * sy_) >> 14; RM[7] = sp; RM[8] = (cp * cy_) >> 14; }
  gaze_now = (int16_t)(fsin((int16_t)(aura_ph >> 2)) >> 11);
  Obj *me = obj_new(K_TATER, (int)cx, (int)cy, (int)rx, (int)ry, 0); me->expr = T.blink && T.expr != E_GLOW ? E_CLOSED : T.expr; me->mouth = T.mouth; me->mat = T.aura;
  if (T.limbs) {                                   /* arms, legs, the wand; the ahoge always */
    for (int s = 0; s < 2; s++) { int32_t fx = s ? 1 : -1; int32_t sx = T.x + fx * (rx * 14), sy = T.y + ry * 2, L1 = T.r * 9 / 20, L2 = T.r * 2 / 5;
      int16_t h1 = T.arm[s * 2], h2 = (int16_t)(h1 + T.arm[s * 2 + 1]);
      int32_t ex = sx + fx * ((int32_t)fsin(h1) * L1 >> 14), ey = sy + ((int32_t)fcos(h1) * L1 >> 14), hx = ex + fx * ((int32_t)fsin(h2) * L2 >> 14), hy = ey + ((int32_t)fcos(h2) * L2 >> 14);
      int32_t lw = T.r / 13 > 24 ? T.r / 13 : 24;
      cap(sx, sy, ex, ey, lw, lw, MT_STICK); cap(ex, ey, hx, hy, lw, lw, MT_STICK); cap(hx, hy, hx, hy, T.r / 6, T.r / 6, MT_WHITE);
      if (s && T.wand) { int32_t wx = hx + ((int32_t)fsin(h2) * T.r * 3 / 5 >> 14), wy = hy + ((int32_t)fcos(h2) * T.r * 3 / 5 >> 14); cap(hx, hy, wx, wy, lw * 3 / 4, lw * 3 / 4, MT_GOLD);
        Obj *st = obj_new(K_STAR4, (int)(wx >> 4), (int)(wy >> 4), (int)(T.r / 64 + 6 + ((beat >> 13) & 3)), 0, -1); if (st) st->hue = (uint8_t)((hue_ph >> 9) & 15); }
      int32_t lx = T.x + fx * rx * 6, ly = T.y + ry * 14, fy = ly + T.r * 2 / 5;
      cap(lx, ly, lx + fx * 16, fy, lw, lw, MT_STICK); cap(lx + fx * 16 - fx * T.r / 10, fy, lx + fx * 16 + fx * T.r / 7, fy, T.r / 9, T.r / 10, MT_SHOE); } }
  { int32_t px = T.hx[0], py = T.hy[0], bx = T.x, byy = T.y - ry * 15; int32_t w = T.r / 14 > 20 ? T.r / 14 : 20;
    cap(bx, byy, px, py, w, w, MT_LEAF); for (int k = 1; k < 3; k++) { cap(T.hx[k - 1], T.hy[k - 1], T.hx[k], T.hy[k], w, w * 4 / 5, MT_LEAF); }
    cap(T.hx[2], T.hy[2], T.hx[2] + (T.r / 4), T.hy[2] - T.r / 7, T.r / 16, T.r / 8, MT_LEAF); cap(T.hx[2], T.hy[2], T.hx[2] - (T.r / 5), T.hy[2] - T.r / 5, T.r / 18, T.r / 10, MT_LEAF); }
  if (stage == S_SENPAI) for (int i = 0; i < 16; i++) {             /* a ring of little ones, round her, tilted */
    int16_t a = (int16_t)(ring_ph + i * 256); int32_t X = (int32_t)fcos(a) * 110 >> 14, Z = (int32_t)fsin(a) * 110 >> 14, Y = -(Z * 7000 >> 14), Zt = Z * 14800 >> 14;
    int32_t s = 65536 * 220 / (220 + Zt); int px = 160 + (int)((X * s) >> 16), py = 120 + (int)((Y * s) >> 16), r = (int)((14 * s) >> 16);
    Obj *m = obj_new(K_MINI, px, py, r, r, Zt); if (m) { m->hue = (uint8_t)((i + (hue_ph >> 9)) & 15); m->expr = E_HEART; m->mouth = M_SMILE; }
    fdisc(px / 2, py / 2, 2, (uint8_t)((i + (hue_ph >> 9)) & 15), 15); }
  if (stage == S_COSMOS) {                                            /* her rings, and her moons */
    int32_t rr = T.r * 2 >> 4; Obj *a = obj_new(K_RINGB, (int)cx, (int)cy, (int)rr, (int)(rr / 4), 200), *b = obj_new(K_RINGF, (int)cx, (int)cy, (int)rr, (int)(rr / 4), -200); (void)a; (void)b;
    for (int i = 0; i < 6; i++) { int16_t ang = (int16_t)(ring_ph * (1 + (i & 1)) + i * 682); int32_t X = (int32_t)fcos(ang) * (rr + 30 + i * 8) >> 14, Z = (int32_t)fsin(ang) * (rr + 30 + i * 8) >> 14, Y = (Z * 4000 >> 14) - (X * 2000 >> 14);
      int32_t s = 65536 * 300 / (300 + Z); Obj *m = obj_new(K_MINI, (int)(cx + ((X * s) >> 16)), (int)(cy + ((Y * s) >> 16)), (int)((9 * s) >> 16), (int)((9 * s) >> 16), Z * 2); if (m) { m->hue = (uint8_t)(i * 3); m->mouth = (uint8_t)(i & 1 ? M_OPEN : M_SMILE); } } }
  if (stage == S_MADNESS) {                                           /* the swarm on a (2, 3) torus knot, turned by the carried rotation */
    int n = 36 + L * 4; if (n > 48) n = 48;
    for (int i = 0; i < n; i++) { int16_t ph = (int16_t)(sw_ph + i * (4096 / n));
      int32_t rr = 70 + ((int32_t)fcos((int16_t)(ph * 3)) * 34 >> 14), X = (int32_t)fcos((int16_t)(ph * 2)) * rr >> 14, Y = (int32_t)fsin((int16_t)(ph * 2)) * rr >> 14, Z = (int32_t)fsin((int16_t)(ph * 3)) * 34 >> 14;
      X *= 2; Y *= 2; Z *= 2;
      int32_t x2 = (SWR[0] * X + SWR[1] * Y + SWR[2] * Z) >> 14, y2 = (SWR[3] * X + SWR[4] * Y + SWR[5] * Z) >> 14, z2 = (SWR[6] * X + SWR[7] * Y + SWR[8] * Z) >> 14;
      int32_t s = 65536 * 260 / (260 + z2); if (s < 8000) continue; if (s > 131072) s = 131072; int px = 160 + (int)((x2 * s) >> 16), py = 120 + (int)((y2 * s * 3 / 4) >> 16), r = (int)((11 * s) >> 16);
      Obj *m = obj_new(K_MINI, px, py, r, r, z2); if (m) { m->hue = (uint8_t)((i + (hue_ph >> 8)) & 15); m->expr = (i % 5) == 0 ? E_HEART : E_NORMAL; m->mouth = (uint8_t)((i + beat_n) & 1 ? M_OPEN : M_SMILE); m->mat = (uint8_t)((i % 3) == 0); }
      } }
  for (int i = 0; i < NSPARK; i++) { Spark *s = &SPK[i]; if (!s->life) continue; Obj *o = obj_new(K_STAR4, (int)(s->x >> 4), (int)(s->y >> 4), s->size > 8 ? s->size : (s->life > 12 ? s->size : s->size * s->life / 12 + 1), 0, -300); if (o) o->hue = s->hue;
    if (bg_now == BG_FIELD || bg_now == BG_KALEI) fpour((int)(s->x >> 5), (int)(s->y >> 5), s->hue, 13); }
  if (bg_now == BG_FIELD || bg_now == BG_KALEI) { fdisc((int)cx / 2, (int)cy / 2, (int)(rx / 3), (uint8_t)((hue_ph >> 9) + 3), 11); field_swap(); }
  for (int i = 0; i < nobj; i++) { obj_prep(&OB[i]); ORD[i] = &OB[i]; }
  for (int i = 1; i < nobj; i++) { Obj *t = ORD[i]; int j = i; while (j > 0 && ORD[j - 1]->z < t->z) { ORD[j] = ORD[j - 1]; j--; } ORD[j] = t; }   /* far to near */
  cam_dx = shake ? (int16_t)((fsin((int16_t)(aura_ph * 7)) * shake) >> 14) : 0; cam_dy = shake ? (int16_t)((fcos((int16_t)(aura_ph * 11)) * shake) >> 14) : 0; }

static void bubble_draw(void) {                       /* a speech bubble over her head */
  if (!bubble) return; int w = text_w(bubble, 2) + 20, h = 30, x = 160 - w / 2, y = (int)(T.y >> 4) - (int)(T.r >> 4) - 64; if (y < 4) y = 4;
  orect(x - 2, y - 2, w + 4, h + 4, INK); orect(x, y, w, h, WHITE);
  for (int k = 0; k < 10; k++) { orect(150 + k, y + h + 2 + k, 12 - k, 1, INK); orect(152 + k, y + h + k, 8 - k > 0 ? 8 - k : 1, 1, WHITE); }
  otext(x + 10, y + 8, bubble, INK, RAMP[MT_WHITE][8], 2, 0); }
static void overlay_band(void) {
  uint16_t ink = BLACK;
  if (stage == S_MORNING && loopn == 0 && st_left > 90) {
    otext(160 - text_w("TATER-CHAN", 4) / 2, 30, "TATER-CHAN", glass(255, 120, 190), ink, 4, 5);
    otext(160 - text_w("ASCENDS", 3) / 2, 70, "ASCENDS", glass(255, 230, 90), ink, 3, 3); }
  if (stage == S_MORNING && loopn > 0 && st_left > 150) { char b[12] = "LOOP 0"; b[5] = (char)('0' + (loopn % 10)); otext(160 - text_w(b, 3) / 2, 40, b, glass(255, 80, 120), ink, 3, 8); }
  bubble_draw();
  for (int i = 0; i < NWORD; i++) { Word *w = &WD[i]; if (!w->life) continue; int tw = text_w(w->s, w->sc), x = w->x - tw / 2; if (x < 2) x = 2; if (x + tw > SW - 2) x = SW - 2 - tw;
    otext(x, w->y, w->s, PALC[(uint8_t)(((w->hue + (aura_ph >> 9)) & 15) << 4 | 15)], ink, w->sc, w->melt); }
  if (hud_on) { static const char *const H[4] = { "ALL OF THAT:", "1 RISC-V CORE, 160 MHZ", "NO FLOATS. NO GPU.", "EVERY DOT, EVERY FRAME" };
    for (int k = 0; k < 4; k++) otext(160 - text_w(H[k], 2) / 2, 30 + k * 22, H[k], k == 0 ? glass(255, 230, 90) : WHITE, ink, 2, 0); }
  if (stage == S_MADNESS || hud_on) { char b[24] = "FPS 00  NO FPU  160MHZ"; uint32_t f = hud_fps > 99 ? 99 : hud_fps; b[4] = (char)('0' + f / 10); b[5] = (char)('0' + f % 10); otext(6, 228, b, glass(180, 255, 200), ink, 1, 0); } }

static void FASTF post_row(int y, uint16_t *o) {
  if (iris_on) { int32_t dy = y - (T.y >> 4), r = iris_r, x0 = SW, x1 = SW - 1; if (dy > -r && dy < r) { int32_t h = isq(r * r - dy * dy); x0 = (T.x >> 4) - h; x1 = (T.x >> 4) + h; }
    if (x0 > SW) x0 = SW; if (x1 < -1) x1 = -1; for (int x = 0; x < x0; x++) o[x] = BLACK; for (int x = x1 + 1; x < SW; x++) o[x] = BLACK; }
  if ((glitch || wob) && !impact) { int s = 0, burst = 0;
    if (glitch) { uint32_t h = mix32((uint32_t)(y >> 2) * 2654435761u ^ (uint32_t)beat_n * 40503u ^ (uint32_t)(aura_ph >> 6)); if ((h & 63) < glitch) { s = (int)((h >> 8) & 63) - 32; burst = (int)(h >> 20) & 1 ? 1 : 2; } s += (fsin((int16_t)(y * 40 + aura_ph * 3)) * glitch) >> 15; }
    s += (fsin((int16_t)(y * 22 + aura_ph * 5)) * wob) >> 14; s &= ~1;   /* the wave: smooth, so it reads as meant; even, so whole words move */
    if (s > 0) { memmove(o + s, o, (size_t)(SW - s) * 2); } else if (s < 0) { memmove(o, o - s, (size_t)(SW + s) * 2); }
    if (burst) { uint32_t t = burst == 1 ? 0x10801080u : 0x10041004u, *w = (uint32_t *)o; for (int x = 0; x < SW / 2; x++) w[x] |= t; } }   /* a torn row is tinted magenta or cyan: a glitch, not a tear */
  if (rgb_k && !impact) { int k = (rgb_k + 1) >> 1; uint32_t *w = (uint32_t *)o;    /* red pulled aside, two dots a word: a red edge one side, cyan the other */
    for (int x = 0; x < SW / 2 - k; x++) w[x] = (w[x] & ~0x00F800F8u) | (w[x + k] & 0x00F800F8u); }
  if (impact) { uint16_t a = (impact & 1) ? WHITE : BLACK, b = (impact & 1) ? BLACK : WHITE;   /* the impact frame: two inks, by the dot's red and upper green */
    for (int x = 0; x < SW; x++) o[x] = LUMB[o[x] & 255] ? a : b; }
  else if (flash_n) { uint32_t *w = (uint32_t *)o; for (int x = 0; x < SW / 2; x++) w[x] |= 0x18C318C3u; } }   /* the flash: every channel's two top bits lit */

static void frame(void) {
  uint32_t c0 = clock_now(); story_step(); uint32_t c1 = clock_now(); prep(); uint32_t c2 = clock_now();
  { uint8_t a[4] = { 0, 0, (uint8_t)((SW - 1) >> 8), (uint8_t)(SW - 1) }; wire_cmd(0x2A, a, 4); }
  { uint8_t a[4] = { 0, 0, (uint8_t)((SH - 1) >> 8), (uint8_t)(SH - 1) }; wire_cmd(0x2B, a, 4); }
  wire_cmd(0x2C, 0, 0);
  int sb = 0; wire_hold(sb); cyc_bg = cyc_obj = cyc_post = cyc_over = 0;
  for (int bnd = 0; bnd < SH / BAND; bnd++) {
    int y0 = bnd * BAND; uint16_t *strip = STRIPB[sb]; OBUF = strip; ob_y0 = y0;
    nbobj = 0; for (int i = 0; i < nobj; i++) { Obj *b = ORD[i]; if (b->y1 + cam_dy >= y0 && b->y0 + cam_dy < y0 + BAND) BOBJ[nbobj++] = b; }
    for (int r = 0; r < BAND; r++) { int y = y0 + r, ys = y - cam_dy; uint16_t *o = strip + r * SW; if (ys < 0) ys = 0; if (ys > SH - 1) ys = SH - 1;
      uint32_t q0 = clock_now();
      switch (bg_now) {
        case BG_SKY: sky_row(ys, o); break;
        case BG_HALF: half_row(ys, o); break;
        case BG_TUNNEL: if (stage == S_MADNESS) { if (!(r & 1)) tunnel_row(ys, KROW); memcpy(o, KROW, SW * 2); } else tunnel_row(ys, o); break;
        case BG_KALEI: if (!(r & 1)) kalei_row(ys, KROW); memcpy(o, KROW, SW * 2); break;   /* each folded row serves two */
        case BG_FIELD: if (stage == S_WORLD) vox_row(ys, o); else field_row(ys, o); break;
        default: space_row(ys, o); break; }
      uint32_t q1 = clock_now(); cyc_bg += q1 - q0;
      for (int i = 0; i < nbobj; i++) { const Obj *b = BOBJ[i]; if (ys < b->y0 || ys > b->y1) continue;
        switch (b->kind) {
          case K_TATER: tater_row(b, ys, o); for (int c = 0; c < ncap; c++) { const CapRun *cr = &CR[c]; if (ys >= cr->y0 && ys <= cr->y1) cap_row(cr, ys, o); } break;
          case K_MINI: mini_row(b, ys, o); break;
          case K_STAR4: star4_row(b, ys, o); break;
          default: ring_row(b, ys, o); break; } }
      if (cam_dx) { if (cam_dx > 0) memmove(o + cam_dx, o, (size_t)(SW - cam_dx) * 2); else memmove(o, o - cam_dx, (size_t)(SW + cam_dx) * 2); }
      uint32_t q2 = clock_now(); cyc_obj += q2 - q1;
      post_row(y, o); cyc_post += clock_now() - q2; }
    { uint32_t q3 = clock_now(); overlay_band(); cyc_over += clock_now() - q3; }
    wire_data(strip, SW * BAND * 2); wire_mark(sb); sb ^= 1; wire_hold(sb); }
  uint32_t c3 = clock_now(); cyc_story = c1 - c0; cyc_prep = c2 - c1; cyc_draw = c3 - c2; frames++; }
static uint32_t tater_print(void) { uint32_t z = 2166136261u; z = (z ^ (uint32_t)T.x) * 16777619u; z = (z ^ (uint32_t)T.y) * 16777619u; z = (z ^ (uint32_t)T.r) * 16777619u; z = (z ^ T.yaw) * 16777619u; z = (z ^ stage) * 16777619u; z = (z ^ trip) * 16777619u; z = (z ^ f_seed) * 16777619u; return z; }
static void tater_begin(void) {
  for (int i = 0; FACE_CH[i]; i++) GLYPH[(uint8_t)FACE_CH[i]] = (uint8_t)(i + 1);
  GLYPH['#'] = 0; grow_ramps(); grow_tables(); grow_tile();
  for (int k = 0; k < 256; k++) LUMB[k] = (uint8_t)(((k >> 3) * 2 + (k & 7) * 5) > 40);   /* the wire's low byte is red and green's top three */ palette_turn(0, 255); story_begin(); }

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
