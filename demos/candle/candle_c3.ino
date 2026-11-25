// LESSON-BEGIN 1 of 15: LESSON MAP: CANDLE, the lever: a value carried with its rates, read every frame
// SKILL      : readings with three rate slots computed once by the book's moves (mix, derived division, averaging root);
//              then each frame every dot is value + rates x the flame's move: four multiplies a cell. A film look by rules.
// BENCH      : cd bench && gcc -O2 -w -o /tmp/ctwin desk/twin.c && FRAMELINES=1 /tmp/ctwin 300 x | tail -3
//              bench/desk/lever_check.c compares the lever against a full re-cast at moved flames.
//              desk == emulated chip: bash bench/tools/check_c3.sh 300   (needs MCR_X)
// READ FIRST : demos/candle/README.md; docs/MISSING_MACHINE.md (the lever: value, rate per source, latched set); workbook Part V unit V6.
// SECTIONS   : 2 types | 3 the mouth | 4 CANDLE | 5 tools | 6 THE READING | 7 the room | 8 casting at boot | 9 stuff
//              10 THE GRID OF READINGS | 11 camera flaws | 12 the flame | 13 flicker and hand | 14 drawing | 15 run
// THE IDEA   : light(flame at rest + move) = L0 + RATEX*dx + RATEY*dy + RATEZ*dz, per cell, per frame. The rates came out of
//              the same arithmetic as the value, at boot, with no derivative written anywhere.
// LESSON-END
// CANDLE, for the ESP32-C3 SuperMini + 2.0" ST7789 (320 x 240 across). A candle on a table in a dark room, made to look filmed.
// No video is stored. At boot (about 4 seconds, the screen dark) every dot of the room is worked out once as a reading: its light from the
// flame, with three rate slots for how that light moves when the flame moves. Each frame the flame flickers by carried springs and every
// dot's light is its value plus its rates times the flame's move. Then a film curve, the flame's glow, lens falloff, a handheld drift,
// an exposure that lags the light, and grain that changes every frame. Whole numbers only: no floats, no libraries, no clock variable.
// Wiring (as HIGH NOON, THE FISH AFFECT, TATER-CHAN): SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3. No controls.
// Arduino IDE: "ESP32C3 Dev Module", core 3.x, USB CDC On Boot: Enabled. Serial Monitor 115200: real frame times.
// ---- fixes (uncomment):
//#define BGR_ORDER   // only if red and blue come out swapped
//#define NO_INVERT
//#define ROT180
//#define SAFE_SPI
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
#ifdef SAFE_SPI
#define WIRE_HZ 20000000
#else
#define WIRE_HZ 40000000
#endif
// LESSON-BEGIN 2 of 15: Types first
// WHAT    : Rd (a reading: v and r[3]), V3, Spring and the room's arrays sit above the first function (IDE prototype step).
// LESSON-END
// ---------------------------------------------------------------- every type first (candle.h)
/* candle.h  CANDLE: a candle on a table in a dark room, made to look filmed, on the ESP32-C3's 320 x 240 (RISC-V, 160 MHz, no FPU).

   No video is stored and no frame is rendered the usual way. At boot, every dot of the room (a half-resolution grid with a
   margin for the camera's shake) is worked out once as a READING: its light from the flame, carried with three rate slots,
   how that light moves when the flame moves across, up and toward the wall. The rates come out of the book's moves (mix,
   the derived division, the averaging root) on the reading of the flame's offset: no derivative formula is written.
   Each frame, the flame's flicker is stepped by carried springs, and every dot's light is its value plus its rates times
   the flame's offset: four multiplies a dot, the lever. Then the camera's flaws are laid on: a film curve, the flame's
   glow, a lens falloff, a handheld drift, an exposure that lags the light, sensor grain that changes every frame.

   Whole numbers only. Nothing is a function of time: every motion is a rate latched into a value, every event a countdown.
   Shadows: the share of the flame each dot sees is counted by rays at boot; its rates are MEASURED there (the share at the
   flame moved half a step each way), not carried. That is the one stipulated part of the light; the README says so.

   Needs the glue: wire_cmd(c, p, n), wire_data(p, n), wire_mark(s), wire_hold(s), clock_now(). */
#include <stdint.h>
#include <string.h>
#ifndef FASTF
#define FASTF
#endif
#define SW 320
#define SH 240
#define BAND 16
#define GX 176                 /* the room's grid: half resolution, 8 cells of margin each side for the shake */
#define GY 136
#define HX 161                 /* half-resolution cells a screen row needs */
#define FIX 20                 /* the readings at boot: tops over 2^20, lengths in decimetres */
typedef struct { int64_t v, r[3]; } Rd;                       /* (value; rate against the flame's x, y, z) */
typedef struct { int32_t x, y, z; } V3;                        /* lengths in 1/64 cm */
typedef struct { int32_t s, v; } Spring;                       /* a carried place and its rate */
typedef struct { int32_t cx, cz, r, y0, y1; uint8_t mat; } Cyl;
// LESSON-BEGIN 3 of 15: The mouth: HIGH NOON's DMA queue
// WHAT    : the same panel queue as HIGH NOON and MINAMO. The CPU fills the next row strip while the wire sends the last.
// LESSON-END
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
// LESSON-BEGIN 4 of 15: CANDLE's core begins (candle.h, pasted by bench/tools/make_c3.py)
// LESSON-END
// ---------------------------------------------------------------- CANDLE (candle.h)

// LESSON-BEGIN 5 of 15: Whole-number tools
// WHAT    : c_draw is the stated scatter (xorshift, fixed seed): desk and chip draw the same sequence. mouth_div lands once,
//           half to even. isqrt64 finds the root a bit at a time with no divide (the C3 divides 64 bits only in software).
// LESSON-END
/* ================================================================ whole-number tools */
static uint32_t c_seed = 0xC0FFEEu;
static uint32_t c_draw(void) { uint32_t x = c_seed; x ^= x << 13; x ^= x >> 17; x ^= x << 5; c_seed = x; return x; }
static int32_t c_kick(int32_t a) { int32_t r = (int32_t)(c_draw() >> 16) - 32768; return (int32_t)(((int64_t)r * a) >> 15); }   /* -a .. a */
static uint32_t mix32(uint32_t x) { x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16; return x; }
static uint64_t isqrt64(uint64_t n) {                    /* the floor of the root, a bit at a time: no divide (the C3 divides 64 bits only in software) */
  uint64_t r = 0, bit = 1ull << 62; while (bit > n) bit >>= 2;
  while (bit) { if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; } else r >>= 1; bit >>= 2; } return r; }
static int32_t mouth_div(int64_t n, int64_t d) {          /* nearest whole, an exact half to even, mouth(-x) = -mouth(x); d > 0 */
  int64_t a = n < 0 ? -n : n, q = a / d, r = a % d; if (2 * r > d || (2 * r == d && (q & 1))) q++; return (int32_t)(n < 0 ? -q : q); }
#define CM10(t) ((int32_t)((t) * 64 / 10))               /* tenths of a centimetre to 1/64 cm */

// LESSON-BEGIN 6 of 15: THE READING: the book's moves with three rate slots
// WHAT    : rd_add, rd_sub piece by piece; rd_mix is the two strips (r = r1*v2 + v1*r2); rd_div is the derived division
//           (s = (r1*v2 - r2*v1)/v2^2); rd_sqrt takes the root of the value and the rate slot divided by twice it.
// HOLDING : every slot is a top over 2^FIX. A product has bottom 2^(2 FIX), so rd_mix lands it once with >> FIX.
// SEED    : dm_of(g, slot) seeds rate 1 in one slot: the flame's offset is the reading's isolate, one slot per direction.
// TRAP    : rd_div's value of b is never 0 here: a light at the surface is refused upstream. Find where (light_reading).
// TRY     : do practice/p01_readings first. Then check one cell: rate of 1/|d|^2 against the flame's x should be
//           -2 dx / |d|^4. Predict the sign for a cell left of the flame, then print RATEX for it.
// LESSON-END
/* ================================================================ the reading: the book's moves with three rate slots */
static Rd rd_k(int64_t v) { Rd a; a.v = v; a.r[0] = a.r[1] = a.r[2] = 0; return a; }
static Rd rd_add(Rd a, Rd b) { Rd c; c.v = a.v + b.v; for (int k = 0; k < 3; k++) c.r[k] = a.r[k] + b.r[k]; return c; }
static Rd rd_sub(Rd a, Rd b) { Rd c; c.v = a.v - b.v; for (int k = 0; k < 3; k++) c.r[k] = a.r[k] - b.r[k]; return c; }
static Rd rd_mix(Rd a, Rd b) {                            /* the two strips */
  Rd c; c.v = (a.v * b.v) >> FIX; for (int k = 0; k < 3; k++) c.r[k] = (a.r[k] * b.v + a.v * b.r[k]) >> FIX; return c; }
static Rd rd_div(Rd a, Rd b) {                            /* the derived division; b's value is never 0 here (a light at a surface is refused upstream) */
  Rd c; c.v = (a.v * ((int64_t)1 << FIX)) / b.v;
  for (int k = 0; k < 3; k++) { int64_t t = (a.r[k] * b.v - b.r[k] * a.v) / b.v; c.r[k] = (t * ((int64_t)1 << FIX)) / b.v; } return c; }
static Rd rd_sqrt(Rd a) {                                 /* the averaging rule for the value; the rate slot divided by twice it */
  Rd c; c.v = (int64_t)isqrt64((uint64_t)a.v << FIX); if (c.v < 1) c.v = 1; for (int k = 0; k < 3; k++) c.r[k] = (a.r[k] * ((int64_t)1 << (FIX - 1))) / c.v; return c; }
static Rd rd_scale(Rd a, int32_t num, int32_t sh) { Rd c; c.v = (a.v * num) >> sh; for (int k = 0; k < 3; k++) c.r[k] = (a.r[k] * num) >> sh; return c; }

// LESSON-BEGIN 7 of 15: The room in 1/64 cm
// HOLDING : lengths are tops over 64 (1/64 cm). A table, a wall, a book, a brass holder and glass, each a stated shape.
// LESSON-END
/* ================================================================ the room (lengths in 1/64 cm) */
#define ZW CM10(220)                                      /* the wall, 22 cm behind the candle */
static const V3 EYE = { 0, CM10(135), CM10(-300) };       /* the phone, 30 cm in front, 13.5 cm up */
static const V3 LOOK = { 0, CM10(95), 0 };
static const V3 FLAME0 = { 0, CM10(139), 0 };             /* the light's middle at rest: 1.9 cm over the wax */
enum { M_WOOD, M_WALL, M_WAX, M_BRASS, M_GLASS, M_CLOTH, M_PAGES, M_N };
#define NCYL 5
static const Cyl CYL[NCYL] = {
  { 0, 0, CM10(11), CM10(22), CM10(120), M_WAX },         /* a taper candle */
  { 0, 0, CM10(16), CM10(6), CM10(22), M_BRASS },         /* the holder's socket */
  { 0, 0, CM10(38), 0, CM10(6), M_BRASS },                /* the holder's dish */
  { CM10(105), CM10(50), CM10(30), 0, CM10(105), M_GLASS }, /* an amber bottle */
  { CM10(105), CM10(50), CM10(9), CM10(105), CM10(150), M_GLASS }, /* its neck */
};
static const int32_t BOOK[6] = { CM10(-200), CM10(-75), 0, CM10(32), CM10(-45), CM10(75) };   /* x0 x1 y0 y1 z0 z1 */
/* how each stuff answers the light: diffuse, shine, shine's power (squarings), glow through (wax); on 256 */
static const uint8_t KD[M_N] = { 200, 230, 110, 60, 14, 150, 230 };
static const uint8_t KS[M_N] = { 26, 0, 6, 255, 70, 0, 0 };
static const uint8_t KP[M_N] = { 3, 0, 2, 5, 7, 0, 0 };   /* (n.h)^(2^KP) */
static const uint8_t KT[M_N] = { 0, 0, 255, 0, 0, 0, 0 };
static const uint8_t ALB[M_N][3] = { {150, 92, 52}, {176, 160, 142}, {238, 226, 204}, {236, 178, 86}, {150, 70, 16}, {92, 22, 30}, {222, 210, 182} };
static const uint8_t FLC[3] = { 255, 156, 72 };           /* the flame's light, warm */

// LESSON-BEGIN 8 of 15: Casting rays at boot
// WHAT    : each cell of a half-resolution grid casts a ray; one ray where its neighbours hit the same thing, four at edges.
// BUDGET  : about 4 seconds at boot, once, so every frame after is four multiplies a cell. The trade is boot time for frame time.
// LESSON-END
/* ================================================================ casting rays at boot */
static int32_t FX, FY, FZ, UY, UZ;                        /* the camera's forward and up, on 16384 (right is +x) */
#define FOCAL_H 174                                       /* half-resolution cells: about 38 degrees top to bottom */
static void camera_begin(void) {
  int64_t dy = LOOK.y - EYE.y, dz = LOOK.z - EYE.z; int64_t L = (int64_t)isqrt64((uint64_t)(dy * dy + dz * dz));
  FX = 0; FY = (int32_t)(dy * 16384 / L); FZ = (int32_t)(dz * 16384 / L); UY = FZ; UZ = -FY; }
static V3 cell_dir(int i, int j, int sx, int sy) {        /* toward cell (i, j), a quarter cell (sx, sy = -1 or 1) off its middle, scaled to 512 */
  int64_t a = 4 * i - 2 * (GX - 1) + sx, b = 4 * j - 2 * (GY - 1) + sy;
  int64_t dx = (int64_t)16384 * a, dy = (int64_t)FY * 4 * FOCAL_H - (int64_t)UY * b, dz = (int64_t)FZ * 4 * FOCAL_H - (int64_t)UZ * b;
  int64_t L = (int64_t)isqrt64((uint64_t)(dx * dx + dy * dy + dz * dz)), inv = ((int64_t)1 << 40) / L;   /* one divide, then three multiplies */
  V3 d = { (int32_t)((dx * inv) >> 31), (int32_t)((dy * inv) >> 31), (int32_t)((dz * inv) >> 31) }; return d; }
/* the nearest thing along O + t*D/512 (t in 1/64 cm); returns the stuff, or -1; fills where and which way it faces (on 4096) */
static int cast(V3 O, V3 D, V3 *P, V3 *N, int *obj) {
  int64_t best = (int64_t)1 << 40; int mat = -1; V3 n = { 0, 0, 0 };
  if (D.y < 0) { int64_t t = (int64_t)(-O.y) * 512 / D.y; int64_t z = O.z + t * D.z / 512; if (t > 0 && z < ZW && t < best) { best = t; mat = M_WOOD; n.x = 0; n.y = 4096; n.z = 0; *obj = 10; } }
  if (D.z > 0) { int64_t t = (int64_t)(ZW - O.z) * 512 / D.z; int64_t y = O.y + t * D.y / 512; if (t > 0 && y >= 0 && t < best) { best = t; mat = M_WALL; n.x = 0; n.y = 0; n.z = -4096; *obj = 11; } }
  for (int c = 0; c < NCYL; c++) {
    const Cyl *C = &CYL[c]; int64_t ox = O.x - C->cx, oz = O.z - C->cz, dx = D.x, dz = D.z;
    int64_t a = dx * dx + dz * dz, b = (ox * dx + oz * dz) * 512, cc = (ox * ox + oz * oz - (int64_t)C->r * C->r) * 262144;
    if (a > 0) { int64_t disc = b * b - a * cc;
      if (disc >= 0) { int64_t t = (-b - (int64_t)isqrt64((uint64_t)disc)) / a; int64_t y = O.y + t * D.y / 512;
        if (t > 0 && t < best && y >= C->y0 && y <= C->y1) { best = t; mat = C->mat; int64_t hx = ox + t * dx / 512, hz = oz + t * dz / 512; n.x = (int32_t)(hx * 4096 / C->r); n.y = 0; n.z = (int32_t)(hz * 4096 / C->r); *obj = c; } } }
    if (D.y < 0 && O.y > C->y1) { int64_t t = (int64_t)(C->y1 - O.y) * 512 / D.y; int64_t hx = ox + t * dx / 512, hz = oz + t * dz / 512;
      if (t > 0 && t < best && hx * hx + hz * hz <= (int64_t)C->r * C->r) { best = t; mat = C->mat; n.x = 0; n.y = 4096; n.z = 0; *obj = c; } } }
  { int64_t ln = 0, ld = 1, hn = 1, hd = 0; int ax = -1, sg = 0, out = 0; const int32_t *B = BOOK; int32_t o3[3] = { O.x, O.y, O.z }, d3[3] = { D.x, D.y, D.z };
    /* the slabs, each entry and exit kept as a fraction (top over a positive bottom) and compared by crossing: no divide until the end */
    for (int k = 0; k < 3 && !out; k++) {
      if (d3[k] == 0) { if (o3[k] < B[2 * k] || o3[k] > B[2 * k + 1]) out = 1; continue; }
      int64_t n1 = (int64_t)(B[2 * k] - o3[k]) * 512, n2 = (int64_t)(B[2 * k + 1] - o3[k]) * 512, dd = d3[k]; int s = -1;
      if (dd < 0) { n1 = -n1; n2 = -n2; dd = -dd; int64_t q = n1; n1 = n2; n2 = q; s = 1; }
      if (n1 * ld > ln * dd) { ln = n1; ld = dd; ax = k; sg = s; }
      if (hd == 0 || n2 * hd < hn * dd) { hn = n2; hd = dd; } }
    if (!out && ax >= 0 && ln > 0 && ln * hd < hn * ld) { int64_t t = ln / ld;
      if (t < best) { best = t; n.x = n.y = n.z = 0; if (ax == 0) n.x = 4096 * sg; else if (ax == 1) n.y = 4096 * sg; else n.z = 4096 * sg; mat = (ax == 2 && sg < 0) ? M_PAGES : M_CLOTH; *obj = 12; } } }
  if (mat < 0) return -1;
  P->x = (int32_t)(O.x + best * D.x / 512); P->y = (int32_t)(O.y + best * D.y / 512); P->z = (int32_t)(O.z + best * D.z / 512); *N = n; return mat; }
/* does anything stop the segment from X to L?  (the planes never do: the flame is above the table, in front of the wall) */
static int blocked(V3 X, V3 L, int skip) {
  int64_t wx = L.x - X.x, wy = L.y - X.y, wz = L.z - X.z;
  int32_t nx = X.x < L.x ? X.x : L.x, xx = X.x < L.x ? L.x : X.x, nz = X.z < L.z ? X.z : L.z, xz = X.z < L.z ? L.z : X.z, ny = X.y < L.y ? X.y : L.y, xy = X.y < L.y ? L.y : X.y;
  for (int c = 0; c < NCYL; c++) { if (c == skip) continue; const Cyl *C = &CYL[c];
    if (xx < C->cx - C->r || nx > C->cx + C->r || xz < C->cz - C->r || nz > C->cz + C->r || xy < C->y0 || ny > C->y1) continue;   /* the segment's box misses it */
    int64_t ox = X.x - C->cx, oz = X.z - C->cz, a = wx * wx + wz * wz, b = ox * wx + oz * wz, cc = ox * ox + oz * oz - (int64_t)C->r * C->r;
    if (a > 0) { int64_t disc = b * b - a * cc;
      if (disc > 0) { int64_t sq = (int64_t)isqrt64((uint64_t)disc);
        for (int e = 0; e < 2; e++) { int64_t sn = -b + (e ? sq : -sq);                 /* the root is sn / a */
          if (sn > a / 512 && sn < a) { int64_t ya = X.y * a + sn * wy; if (ya >= (int64_t)C->y0 * a && ya <= (int64_t)C->y1 * a) return 1; } } } }
    if (wy != 0) { int64_t sn = (int64_t)(C->y1 - X.y), sd = wy; if (sd < 0) { sn = -sn; sd = -sd; }   /* through the top */
      if (sn > 0 && sn < sd) { int64_t hx = ox * sd + sn * wx, hz = oz * sd + sn * wz; if (hx * hx + hz * hz <= (int64_t)C->r * C->r * sd * sd) return 1; } } }
  if (skip != 12 && !(xx < BOOK[0] || nx > BOOK[1] || xy < BOOK[2] || ny > BOOK[3] || xz < BOOK[4] || nz > BOOK[5])) {
    int64_t ln = 0, ld = 1, hn = 1, hd = 1; const int32_t *B = BOOK; int64_t o3[3] = { X.x, X.y, X.z }, w3[3] = { wx, wy, wz }; int out = 0;   /* the segment's part inside, as fractions of it */
    for (int k = 0; k < 3 && !out; k++) {
      if (w3[k] == 0) { if (o3[k] < B[2 * k] || o3[k] > B[2 * k + 1]) out = 1; continue; }
      int64_t n1 = B[2 * k] - o3[k], n2 = B[2 * k + 1] - o3[k], dd = w3[k];
      if (dd < 0) { n1 = -n1; n2 = -n2; dd = -dd; int64_t q = n1; n1 = n2; n2 = q; }
      if (n1 * ld > ln * dd) { ln = n1; ld = dd; } if (n2 * hd < hn * dd) { hn = n2; hd = dd; } }
    if (!out && ln * hd < hn * ld && ln * 512 > ld && hn > 0) return 1; }
  return 0; }
#define NLS 6
static const int8_t LSAMP[NLS][3] = { {0, -20, 0}, {22, 10, 0}, {-22, 10, 0}, {0, 10, 22}, {0, 10, -22}, {0, 60, 0} };   /* the flame's body, in 1/64 cm */
static int32_t seen(V3 X, int skip, int32_t ox, int32_t oy, int32_t oz) {        /* the share of the flame X sees, on 2^20 */
  int n = 0; for (int s = 0; s < NLS; s++) { V3 L = { FLAME0.x + ox + LSAMP[s][0], FLAME0.y + oy + LSAMP[s][1], FLAME0.z + oz + LSAMP[s][2] }; if (!blocked(X, L, skip)) n++; }
  return (int32_t)(((int64_t)n << FIX) / NLS); }

// LESSON-BEGIN 9 of 15: Stuff: plaster, weave, grain
// WHAT    : value noise from hash2 on whole numbers. A surface's look is a rule of its place, the same every boot.
// LESSON-END
/* ================================================================ stuff: grain, plaster, weave, page edges (value noise, whole numbers) */
static uint32_t hash2(int32_t x, int32_t y, uint32_t s) { return mix32((uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ s); }
static int32_t vnoise(int32_t x, int32_t y, uint32_t s) {   /* x, y with 8 bits of fraction -> 0..255, eased between corners */
  int32_t xi = x >> 8, yi = y >> 8, fx = x & 255, fy = y & 255; fx = (fx * fx * (768 - 2 * fx)) >> 16; fy = (fy * fy * (768 - 2 * fy)) >> 16;
  int32_t a = hash2(xi, yi, s) & 255, b = hash2(xi + 1, yi, s) & 255, c = hash2(xi, yi + 1, s) & 255, d = hash2(xi + 1, yi + 1, s) & 255;
  int32_t ab = a + (((b - a) * fx) >> 8), cd = c + (((d - c) * fx) >> 8); return ab + (((cd - ab) * fy) >> 8); }
static uint8_t stuff_shade(int mat, V3 P) {
  switch (mat) {
    case M_WOOD: { int32_t w = vnoise(P.x * 2, P.z * 40, 11), g = vnoise(P.x * 9, P.z * 9, 12); int32_t ring = (w * 7) & 255; ring = ring < 128 ? ring : 255 - ring;
      return (uint8_t)(150 + ring / 3 + (g - 128) / 8); }
    case M_WALL: { int32_t a = vnoise(P.x * 6, P.y * 6, 21), b = vnoise(P.x * 40, P.y * 40, 22); return (uint8_t)(200 + (a - 128) / 6 + (b - 128) / 10); }
    case M_WAX: return (uint8_t)(244 + (vnoise(P.x * 30, P.y * 12, 31) - 128) / 24);
    case M_GLASS: return (uint8_t)(220 + (vnoise(P.y * 20, P.x * 4, 41) - 128) / 6);
    case M_CLOTH: return (uint8_t)(180 + ((hash2(P.x / 2, P.z / 2 + P.y / 2, 51) & 63) - 32) + (vnoise(P.x * 6, P.z * 6 + P.y * 6, 52) - 128) / 3);
    case M_PAGES: return (uint8_t)(226 - (((P.y / 3) & 1) ? 18 : 0) + (vnoise(P.x * 20, P.y * 40, 61) - 128) / 16);
    default: return 230; } }

// LESSON-BEGIN 10 of 15: THE GRID: each cell's light as a reading
// WHAT    : light_reading computes the light at X, facing N, as a reading against the flame's offset: diffuse n.d/|d|^3,
//           shine (n.h)^p/|d|^2, light through wax, soft fill. Value and rates fall out of the moves together.
// STORED  : L0 (value at the flame's rest place), RATEX, RATEY, RATEZ (int8, each scaled by the cell's own shift e in ME).
// TIER    : the visible share of the flame's disc has its rates MEASURED by moving the flame 0.4 cm each way (a finite step).
//           That is the one estimated part, named STIPULATED in the code. Everything else is the moves, exact to the bottom.
// KILL    : bench/desk/lever_check.c: lever against a full re-cast at moved flames. Read README for the stated gap.
// LESSON-END
/* ================================================================ the room's grid: each cell's light as a reading */
static uint16_t L0[GY][GX];                               /* light at the flame's rest place */
static int8_t RATEX[GY][GX], RATEY[GY][GX], RATEZ[GY][GX];         /* its rates per 1/64 dm of the flame's move, each shifted down by the cell's e */
static uint8_t ME[GY][GX];                                /* the cell's e (high nibble) and stuff (low) */
#define SCALE_L 8192
static Rd dm_of(int32_t g, int slot) { Rd a = rd_k(((int64_t)g * ((int64_t)1 << FIX)) / 640); if (slot >= 0) a.r[slot] = (int64_t)1 << FIX; return a; }   /* 1/64 cm -> dm, on 2^20 */
/* the light at X, facing N, of stuff mat: a reading against the flame's offset (value; per dm of move) */
static Rd light_reading(V3 X, V3 N, int mat, int obj, int32_t ox, int32_t oy, int32_t oz) {
  Rd d[3]; int32_t pf[3] = { FLAME0.x + ox, FLAME0.y + oy, FLAME0.z + oz }, xs[3] = { X.x, X.y, X.z };
  for (int k = 0; k < 3; k++) d[k] = rd_sub(dm_of(pf[k], k), dm_of(xs[k], -1));      /* the flame's offset is the reading's isolate */
  Rd dd = rd_add(rd_add(rd_mix(d[0], d[0]), rd_mix(d[1], d[1])), rd_mix(d[2], d[2]));
  Rd len = rd_sqrt(dd);
  int32_t nn[3] = { N.x, N.y, N.z };
  Rd nd = rd_k(0); for (int k = 0; k < 3; k++) nd = rd_add(nd, rd_scale(d[k], nn[k], 12));
  Rd out = rd_k(0);
  int32_t vis = seen(X, obj, ox, oy, oz);
  if (KD[mat] || KS[mat]) {
    Rd V = rd_k(vis); int32_t e = CM10(4);
    if (vis > 0 && vis < ((int32_t)1 << FIX))                                            /* only where it sees part of the flame: elsewhere the rates are taken as 0 */
    V.r[0] = ((int64_t)(seen(X, obj, ox + e, oy, oz) - vis) * 640) / e;                   /* the share's rates: measured, the flame moved 0.4 cm */
    if (vis > 0 && vis < ((int32_t)1 << FIX)) { V.r[1] = ((int64_t)(seen(X, obj, ox, oy + e, oz) - vis) * 640) / e;   /* each way in turn (stipulated: the one estimated part) */
      V.r[2] = ((int64_t)(seen(X, obj, ox, oy, oz + e) - vis) * 640) / e; }
    if (nd.v > 0 && KD[mat]) {                                                           /* diffuse: n.d / |d|^3 */
      Rd E = rd_div(nd, rd_mix(dd, len)); out = rd_add(out, rd_scale(rd_mix(E, V), KD[mat], 8)); }
    if (KS[mat] && nd.v > 0) {                                                           /* shine: (n.h)^p / |d|^2, h halfway to the eye */
      int32_t es[3] = { EYE.x - X.x, EYE.y - X.y, EYE.z - X.z }; int64_t el = (int64_t)isqrt64((uint64_t)((int64_t)es[0] * es[0] + (int64_t)es[1] * es[1] + (int64_t)es[2] * es[2]));
      Rd h[3]; for (int k = 0; k < 3; k++) h[k] = rd_add(rd_div(d[k], len), rd_k(((int64_t)es[k] * ((int64_t)1 << FIX)) / el));
      Rd hl = rd_sqrt(rd_add(rd_add(rd_mix(h[0], h[0]), rd_mix(h[1], h[1])), rd_mix(h[2], h[2])));
      Rd nh = rd_k(0); for (int k = 0; k < 3; k++) nh = rd_add(nh, rd_scale(h[k], nn[k], 12)); nh = rd_div(nh, hl);
      if (nh.v > 0) { for (int q = 0; q < KP[mat]; q++) nh = rd_mix(nh, nh);
        out = rd_add(out, rd_scale(rd_mix(rd_div(nh, dd), V), KS[mat], 6)); } } }
  { Rd F = rd_div(rd_k((int64_t)1 << (FIX - 4)), rd_add(dd, rd_k((int64_t)1 << (FIX + 1))));   /* bounce: the room giving back a little, 1/16 / (2 + |d|^2) (stipulated) */
    out = rd_add(out, rd_scale(F, KD[mat] + KS[mat] / 2, 8)); }                         /* metal and glass give back the lit room too */
  if (KT[mat]) {                                                                         /* wax lets light through: a / (b + |d|^2), unshadowed */
    Rd b = rd_add(dd, rd_k((int64_t)1 << (FIX - 7))); Rd T = rd_div(rd_k((int64_t)1 << (FIX - 8)), rd_mix(b, b));   /* falls off fast down the wax */
    out = rd_add(out, rd_scale(T, KT[mat], 8)); }
  return out; }
static uint32_t boot_cells, boot_rays;
static const int8_t SUBS[4][2] = { {-1, -1}, {1, -1}, {-1, 1}, {1, 1} };
static uint8_t EDGE_OFF[GY * GX / 8];                     /* boot only, a bit a cell: 1 where it and its four neighbours hit the same thing */
static void grow_room(void) {                             /* four rays a cell; one light reading for each thing they hit, at the rays' middle */
  camera_begin();
  for (int j = 0; j < GY; j++) for (int i = 0; i < GX; i++) { V3 D = cell_dir(i, j, 0, 0), P, N; int obj = -1; int m = cast(EYE, D, &P, &N, &obj); ME[j][i] = (uint8_t)(m < 0 ? 255 : obj); }   /* pass 1: what each cell's middle hits */
  for (int j = 0; j < GY; j++) for (int i = 0; i < GX; i++) {
    int edge = 0; { int o = ME[j][i]; for (int q = 0; q < 4; q++) { int u = i + (q == 0) - (q == 1), w = j + (q == 2) - (q == 3); if (u >= 0 && u < GX && w >= 0 && w < GY && ME[w][u] != o) edge = 1; } }
    if (!edge) EDGE_OFF[(j * GX + i) >> 3] |= (uint8_t)(1 << ((j * GX + i) & 7));                         /* remembered so the second pass doesn't read a neighbour already overwritten */
  }
  for (int j = 0; j < GY; j++) for (int i = 0; i < GX; i++) {
    int32_t shade = 0, hits = 0, nmat[M_N] = { 0 }, gob[4], gmat[4], gn = 0, gc[4]; int64_t gp[4][3], gnn[4][3];
    int nq = ((EDGE_OFF[(j * GX + i) >> 3] >> ((j * GX + i) & 7)) & 1) ? 1 : 4;
    for (int q = 0; q < nq; q++) {
      V3 D = nq == 1 ? cell_dir(i, j, 0, 0) : cell_dir(i, j, SUBS[q][0], SUBS[q][1]), P, N; int obj = -1; int mat = cast(EYE, D, &P, &N, &obj);
      if (mat < 0) continue;
      shade += stuff_shade(mat, P); nmat[mat]++; hits++;
      int g = 0; while (g < gn && (gob[g] != obj || gmat[g] != mat)) g++;
      if (g == gn) { gob[g] = obj; gmat[g] = mat; gc[g] = 0; gp[g][0] = gp[g][1] = gp[g][2] = gnn[g][0] = gnn[g][1] = gnn[g][2] = 0; gn++; }
      gc[g]++; gp[g][0] += P.x; gp[g][1] += P.y; gp[g][2] += P.z; gnn[g][0] += N.x; gnn[g][1] += N.y; gnn[g][2] += N.z; }
    int mat = M_WALL; for (int m = 0; m < M_N; m++) if (nmat[m] > nmat[mat]) mat = m;
    if (!hits) { L0[j][i] = 0; RATEX[j][i] = RATEY[j][i] = RATEZ[j][i] = 0; ME[j][i] = M_WALL; continue; }
    Rd S = rd_k(0);
    for (int g = 0; g < gn; g++) {
      V3 P = { (int32_t)(gp[g][0] / gc[g]), (int32_t)(gp[g][1] / gc[g]), (int32_t)(gp[g][2] / gc[g]) };
      V3 N = { (int32_t)(gnn[g][0] / gc[g]), (int32_t)(gnn[g][1] / gc[g]), (int32_t)(gnn[g][2] / gc[g]) };
      Rd L = light_reading(P, N, gmat[g], gob[g], 0, 0, 0); S = rd_add(S, rd_scale(L, gc[g], 0)); }
    boot_cells++;
    int64_t shd = shade / hits;                                                           /* the stuff's shade, folded into its light once */
    int64_t v = (S.v * SCALE_L / hits * shd / 256) >> FIX; if (v < 0) v = 0; if (v > 65535) v = 65535;
    int64_t r[3]; int64_t mx = 0; for (int k = 0; k < 3; k++) { r[k] = (S.r[k] * SCALE_L / 64 / hits * shd / 256) >> FIX; int64_t a = r[k] < 0 ? -r[k] : r[k]; if (a > mx) mx = a; }
    int e = 0; while ((mx >> e) > 127 && e < 15) e++;
    L0[j][i] = (uint16_t)v; RATEX[j][i] = (int8_t)mouth_div(r[0], (int64_t)1 << e); RATEY[j][i] = (int8_t)mouth_div(r[1], (int64_t)1 << e); RATEZ[j][i] = (int8_t)mouth_div(r[2], (int64_t)1 << e);
    ME[j][i] = (uint8_t)(e << 4 | mat); } }

// LESSON-BEGIN 11 of 15: The camera's flaws, grown once
// WHAT    : film curve, lens falloff, grain tables. The phone look is rules applied after the light, never stored frames.
// LESSON-END
/* ================================================================ the camera's flaws */
static uint8_t TM[4096];                                  /* the film curve: linear light -> 0..255, a shoulder that never quite clips, then a square root */
static uint8_t GLOWT[1024];                               /* the flame's glow by distance squared (half cells) */
#define NZW 448
static int8_t NZ[32][NZW];                                /* grain: four draws summed; rows wide enough that no index wraps (320 dots + 127 of offset) */
static uint8_t GAMP[256];                                 /* grain's size by brightness: more in the dark, as a small sensor gives */
static uint8_t VX[HX], VY[121];                           /* the lens falloff, separable */
static uint8_t CLAMP8[512];
static uint16_t TR[512], TG[512], TB[512];               /* value + 128 -> its share of a 565 dot, clamped, already in wire order */

static uint8_t MC[M_N][3];                                /* stuff colour times the flame's colour */
static void grow_camera(void) {
  const int32_t X0 = 520; uint64_t zm = ((uint64_t)4095 << 16) / (4095 + X0);
  for (int x = 0; x < 4096; x++) { uint64_t z = ((uint64_t)x << 16) / (uint64_t)(x + X0);
    int64_t g = (int64_t)isqrt64(z * 65536u / zm * 65536u) >> 8;                       /* a square root of the shoulder, on 256 */
    int64_t sc = (g * g * (768 - 2 * g)) >> 16;                                          /* an S through it: deeper shadows, the same top */
    int64_t y = (g * 5 + sc * 11) >> 4; TM[x] = (uint8_t)(y > 255 ? 255 : y); }
  for (int q = 0; q < 1024; q++) { int32_t d2 = q * 4;                                  /* a hot core and a wide faint halo */
    int32_t core = (int32_t)((int64_t)2600 * 64 / (64 + d2 * 3)), halo = (int32_t)((int64_t)150 * 900 / (900 + d2)); int32_t g = (core + halo) >> 4; if (q < 400) g = g * (400 - q) / 400; else g = 0; GLOWT[q] = (uint8_t)(g > 255 ? 255 : g); }
  for (int y = 0; y < 32; y++) for (int x = 0; x < NZW; x++) { int32_t s = 0; for (int k = 0; k < 4; k++) s += (int32_t)(c_draw() & 255) - 128; NZ[y][x] = (int8_t)(s / 8); }
  for (int l = 0; l < 256; l++) GAMP[l] = (uint8_t)(14 + (255 - l) / 10 - (l > 200 ? (l - 200) / 6 : 0));
  for (int i = 0; i < HX; i++) { int32_t d = i - 80; VX[i] = (uint8_t)(255 - (d * d * 70) / 6400); }
  for (int j = 0; j < 121; j++) { int32_t d = j - 60; VY[j] = (uint8_t)(255 - (d * d * 70) / 3600); }
  for (int k = 0; k < 512; k++) { int32_t v = k - 128; v = v < 0 ? 0 : v > 255 ? 255 : v; CLAMP8[k] = (uint8_t)v;
    uint16_t r = (uint16_t)((v & 0xF8) << 8), g = (uint16_t)((v & 0xFC) << 3), b = (uint16_t)(v >> 3);
    TR[k] = (uint16_t)((r >> 8) | (r << 8)); TG[k] = (uint16_t)((g >> 8) | (g << 8)); TB[k] = (uint16_t)((b >> 8) | (b << 8)); }
  for (int m = 0; m < M_N; m++) for (int c = 0; c < 3; c++) MC[m][c] = (uint8_t)(ALB[m][c] * FLC[c] / 255); }

// LESSON-BEGIN 12 of 15: The flame, full resolution, drawn over
// WHAT    : grow_flame builds the flame's picture once; each frame it is placed by the flame's lean and height.
// LESSON-END
/* ================================================================ the flame itself (full resolution, drawn over) */
static uint8_t FT[64][32][4];                             /* (r, g, b, cover) premultiplied, by height (0 base .. 63 tip) and distance across */
static void grow_flame(void) {
  for (int v = 0; v < 64; v++) {
    int32_t p = v * 4;                                                                   /* height on 256 */
    int32_t w = (int32_t)((83 * (int64_t)isqrt64((uint64_t)p << 8) * (256 - p)) >> 16);  /* sqrt(p) (1 - p), widest a third of the way up */
    if (v < 6) w = w * (v + 3) / 9;
    for (int u = 0; u < 32; u++) {
      uint8_t *o = FT[v][u]; o[0] = o[1] = o[2] = o[3] = 0;
      if (w <= 0) continue;
      int32_t d = (u * 256) / w;                                                         /* 0 middle .. 256 edge */
      if (d >= 300) continue;
      int32_t a = d < 200 ? 255 : (300 - d) * 255 / 100;                                 /* the edge is soft */
      int32_t r, g, b;
      if (v > 40) { int32_t t = (v - 40) * 255 / 23; r = 255; g = 200 - t * 110 / 255; b = 110 - t * 90 / 255; a = a * (255 - t * 3 / 5) / 255; }
      else if (v < 12 && d > 120) { r = 70; g = 90; b = 255; a = a * (60 + v * 10) / 255; }   /* the blue at the base */
      else if (v < 9 && d < 110) { r = 140; g = 70; b = 40; a = a * 200 / 255; }       /* the dark round the wick */
      else { int32_t core = d < 150 ? 255 : (256 - d) * 255 / 106; if (core < 0) core = 0; r = 255; g = 160 + core * 92 / 255; b = 60 + core * 170 / 255; }
      if (u < 2 && v < 10 && v >= 1) { r = 30; g = 18; b = 12; a = 255; }               /* the wick */
      o[0] = (uint8_t)(r * a / 255); o[1] = (uint8_t)(g * a / 255); o[2] = (uint8_t)(b * a / 255); o[3] = (uint8_t)((u < 2 && v < 10 && v >= 1) ? 255 : a / 5); } } }

// LESSON-BEGIN 13 of 15: Flicker and the hand: carried springs
// WHAT    : spring_step: v += -(s >> pull) - (v >> damp) + kick; s += v. Each spring is a reading latched once a frame: the
//           rate (v) is carried, never recomputed from two positions. Drafts arrive and leave on countdowns, not timers.
// RELATION: the flame's move (DX, DY, DZ) is what the lever multiplies. The hand's drift moves the picture, not the light.
// TRAP    : >> floors, so a spring's rest drifts by up to one unit below zero. Harmless here (nothing runs back); it would not
//           be in TIDE or MINAMO. Name which kind of build you are in before choosing a landing.
// LESSON-END
/* ================================================================ the flame's flicker and the hand that holds the phone */
static Spring SWX = { 0, 0 }, SWZ = { 0, 0 }, HGT = { 0, 0 }, SHX = { 0, 0 }, SHY = { 0, 0 }, WDX = { 0, 0 }, WDY = { 0, 0 };
static int32_t flick = 0, draft = 0, draft_left = 0, gutter = 0, calm = 600;
static int32_t inten = 256, expo = 256, expo_tgt = 256, lite_avg = 256 << 8, expo16 = 256 << 4, expo_acc = 0;
static int32_t DX, DY, DZ, cam_ox, cam_oy, flame_h, flame_bx, flame_by, flame_sway;
static uint32_t frames, beat;
static void spring_step(Spring *s, int32_t pull_sh, int32_t damp_sh, int32_t kick) { s->v += -(s->s >> pull_sh) - (s->v >> damp_sh) + kick; s->s += s->v; }
static void story_step(void) {
  beat++;
  /* drafts: a breath of air arrives now and then, pushes the flame for a while, and leaves */
  if (draft_left > 0) draft_left--; else if (--calm <= 0) { draft = c_kick(900); draft_left = 40 + (int32_t)(c_draw() % 120); calm = 200 + (int32_t)(c_draw() % 500);
    if ((c_draw() & 7) == 0) gutter = 70; }
  int32_t push = draft_left > 0 ? draft / 6 : 0;
  spring_step(&SWX, 5, 4, c_kick(draft_left ? 260 : 70) + push);      /* the flame leans across ... */
  spring_step(&SWZ, 5, 4, c_kick(draft_left ? 200 : 50));               /* ... and toward the wall */
  spring_step(&HGT, 4, 3, c_kick(draft_left ? 420 : 120) - (gutter > 0 ? 260 : 0));
  if (gutter > 0) gutter--;
  flick += c_kick(14) - (flick >> 2);                                   /* a quick shimmer in the brightness */
  int32_t h = 256 + (HGT.s >> 6); if (h < 120) h = 120; if (h > 330) h = 330;
  inten = h + flick; if (inten < 90) inten = 90;
  DX = SWX.s >> 9; DZ = SWZ.s >> 9; DY = (h - 256) / 24;                /* the flame's offset, in 1/64 dm */
  if (DX > 3) DX = 3; if (DX < -3) DX = -3; if (DZ > 3) DZ = 3; if (DZ < -3) DZ = -3;   /* within the reach the lever was witnessed for */
  flame_h = 25 * h / 256; flame_sway = (SWX.s >> 9) + DX;
  /* the phone in a hand: two slow wanders and a small tremor, each a carried spring (whole cells of 1/16 dot) */
  spring_step(&WDX, 8, 5, c_kick(9)); spring_step(&WDY, 8, 5, c_kick(7));
  spring_step(&SHX, 3, 2, c_kick(30)); spring_step(&SHY, 3, 2, c_kick(26));
  cam_ox = mouth_div((int64_t)(WDX.s >> 4) + (SHX.s >> 6), 16); cam_oy = mouth_div((int64_t)(WDY.s >> 4) + (SHY.s >> 6), 16);
  if (cam_ox > 14) cam_ox = 14; if (cam_ox < -14) cam_ox = -14; if (cam_oy > 14) cam_oy = 14; if (cam_oy < -14) cam_oy = -14;
  /* the exposure follows the light, late, as a phone's does */
  lite_avg += inten - (lite_avg >> 8); expo_tgt = (256 * 256 * 16) / ((lite_avg >> 8) + 1);
  expo_acc += expo_tgt - expo16; { int32_t st = expo_acc / 40; expo_acc -= st * 40; expo16 += st; }   /* a fortieth of the gap a frame, the remainder carried */
  expo = expo16 >> 4;
  frames++; }

// LESSON-BEGIN 14 of 15: Drawing: the lever, then the full dots
// WHAT    : half_row computes each half-res cell's light: L0 + (RATEX*DX + RATEY*DY + RATEZ*DZ) shifted by e. hrow and
//           screen_row expand to full dots with the film curve, glow, falloff, exposure that lags, and grain.
// COST    : four multiplies and a shift a cell. Compare a re-cast: a ray, a shadow test and the light model per cell per frame.
// TRY     : set RATEX, RATEY, RATEZ to 0 after grow_room. Predict: the room is lit but still while the flame moves; the shadows
//           stop breathing. That is the lever ablated.
// LESSON-END
/* ================================================================ drawing: half cells lit each frame, then the full dots */
static uint32_t HROW[3][HX + 1]; static int hrow_j[3];   /* a lit grid row: r | g << 10 | b << 20 */
static int32_t gx0, gy0, glow_x, glow_y, gain;            /* this frame: where the screen sits on the grid, the flame on the screen, the light's gain */
static uint32_t grain_x, grain_y, chroma_x, chroma_y;
static uint16_t GN[256];                                  /* this frame: grain's size by brightness, the exposure's gain folded in */
static void FASTF half_row(int j, uint32_t *out) {           /* grid row j: HX cells from gx0, lit by the lever, glowed, curved */
  int sy = j - gy0; if (sy < 0) sy = 0; if (sy > 120) sy = 120;
  int32_t vy = VY[sy], dyg = j - glow_y, dy2 = dyg * dyg; int near_rows = dyg > -40 && dyg < 40;
  const uint16_t *l0 = &L0[j][gx0]; const int8_t *rx = &RATEX[j][gx0], *ry = &RATEY[j][gx0], *rz = &RATEZ[j][gx0]; const uint8_t *me = &ME[j][gx0];
  const int8_t *cn = NZ[(j + chroma_y) & 31] + chroma_x; int32_t gvy = (gain * vy) >> 8; int32_t dx0 = gx0 - glow_x;
  static uint16_t GV[HX]; for (int i = 0; i < HX; i++) GV[i] = (uint16_t)((gvy * VX[i]) >> 8);   /* the gain and the lens falloff for this row */
  for (int i = 0; i < HX; i++) {
    uint32_t mv = me[i]; int32_t e = (int32_t)(mv >> 4); const uint8_t *mc = MC[mv & 15];
    int32_t L = l0[i] + (rx[i] * DX + ry[i] * DY + rz[i] * DZ) * (1 << e);              /* the lever: value plus rates times the flame's move */
    if (L < 0) L = 0; else if (L > 65535) L = 65535;
    uint32_t t = ((uint32_t)L * GV[i]) >> 8;                                            /* keep the bits: a shift of 16 here floored every dim cell to black */
    int32_t gr = 0, gg = 0, gb = 0;
    if (near_rows) { int32_t dx = dx0 + i; if (dx > -40 && dx < 40) { int32_t q = (dx * dx + dy2) >> 2; if (q < 400) { int32_t g = GLOWT[q] * inten; gr = (g * FLC[0]) >> 14; gg = (g * FLC[1]) >> 14; gb = (g * FLC[2]) >> 14; } } }
    int32_t c = cn[gx0 + i] >> 3;
    int32_t vr = (int32_t)((t * mc[0]) >> 10) + 5 + gr + c, vg = (int32_t)((t * mc[1]) >> 10) + 5 + gg, vb = (int32_t)((t * mc[2]) >> 10) + 8 + gb - c;
    if (vr < 0) vr = 0; else if (vr > 4095) vr = 4095; if (vg > 4095) vg = 4095; if (vb < 0) vb = 0; else if (vb > 4095) vb = 4095;
    *out++ = (uint32_t)TM[vr] | ((uint32_t)TM[vg] << 10) | ((uint32_t)TM[vb] << 20); }
  out[0] = out[-1]; }
static const uint32_t *hrow(int j) {
  for (int k = 0; k < 3; k++) if (hrow_j[k] == j) return HROW[k];
  static int nxt; int k = nxt; nxt = (nxt + 1) % 3; half_row(j, HROW[k]); hrow_j[k] = j; return HROW[k]; }
static uint16_t STRIPB[2][SW * BAND];
static uint32_t cyc_story, cyc_half, cyc_full, cyc_draw;
static void FASTF screen_row(int y, uint16_t *o) {
  int Y = y + cam_oy + 16, j = Y >> 1; int py = Y & 1; int X0 = cam_ox + 16;
  uint32_t a0 = clock_now(); const uint32_t *A = hrow(j), *B = py ? hrow(j + 1) : A; cyc_half += clock_now() - a0;
  static uint32_t M[HX + 1];
#define AVG(P, Q) ((((P) + (Q) + 0x100401u) >> 1) & 0x0FF3FCFFu)   /* three lanes at once, each rounded half up */
  if (py) { for (int k = 0; k <= HX; k++) M[k] = AVG(A[k], B[k]); A = M; }
  const int8_t *gn = NZ[(y + grain_y) & 31] + grain_x;
  int fy0 = flame_by - flame_h - 2, fy1 = flame_by + 3, inflame = (y >= fy0 && y <= fy1), fv = 0, fbend = 0;
  if (inflame) { fv = ((flame_by - y) * 64) / (flame_h + 1); if (fv < 0 || fv > 63) inflame = 0; else fbend = (flame_sway * fv * (fv + 64)) >> 13; }   /* the whole flame leans, the tip most */
  int fx0 = flame_bx + fbend - 5, fx1 = flame_bx + fbend + 5;
#define DOT(P, X) do { uint32_t p_ = (P); int32_t g_ = (int32_t)((p_ >> 10) & 1023); int32_t n_ = (gn[X] * GN[g_]) >> 8; \
    o[X] = (uint16_t)(TR[(int32_t)(p_ & 1023) + n_ + 128] | TG[g_ + n_ + 128] | TB[(int32_t)(p_ >> 20) + n_ + 128]); } while (0)
  if (!inflame) {                                          /* most rows: two dots a cell, the second halfway to the next */
    const uint32_t *s = A;
    if (!(X0 & 1)) for (int x = 0; x < SW; x += 2, s++) { DOT(s[0], x); DOT(AVG(s[0], s[1]), x + 1); }
    else for (int x = 0; x < SW; x += 2, s++) { DOT(AVG(s[0], s[1]), x); DOT(s[1], x + 1); }
    return; }
  for (int x = 0; x < SW; x++) {                           /* the flame's rows: the same, and the flame laid over */
    int u = x + (X0 & 1); const uint32_t *s = A + (u >> 1); uint32_t p = (u & 1) ? AVG(s[0], s[1]) : s[0];
    if (x >= fx0 && x <= fx1) { int32_t uu = x - flame_bx - fbend; uu = uu < 0 ? -uu : uu; uu = uu * 32 / 5;
      if (uu < 32) { const uint8_t *f = FT[fv][uu]; int32_t keep = f[3] == 255 ? 0 : 255 - f[3] * 5; if (keep < 0) keep = 0;
        int32_t r = (int32_t)(p & 1023), g = (int32_t)((p >> 10) & 1023), b = (int32_t)(p >> 20);
        r = f[0] + ((r * keep) >> 8); g = f[1] + ((g * keep) >> 8); b = f[2] + ((b * keep) >> 8); if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
        p = (uint32_t)r | ((uint32_t)g << 10) | ((uint32_t)b << 20); } }
    DOT(p, x); }
#undef DOT
#undef AVG
}
static int32_t FB_X, FB_Y;                                /* where the wick's top lands on the screen at rest */
static void flame_place(void) {
  int64_t ex = FLAME0.x - EYE.x, ey = CM10(124) - EYE.y, ez = FLAME0.z - EYE.z;
  int64_t f = (ex * FX + ey * FY + ez * FZ) >> 14, up = (ey * UY + ez * UZ) >> 14;
  FB_X = (int32_t)(160 + ex * 2 * FOCAL_H / f); FB_Y = (int32_t)(120 - up * 2 * FOCAL_H / f); }
static void frame(void) {
  uint32_t c0 = clock_now(); story_step(); uint32_t c1 = clock_now();
  gx0 = 8 + ((cam_ox + 16) >> 1) - 8; gy0 = 8 + ((cam_oy + 16) >> 1) - 8;
  gain = (inten * expo) >> 8;
  flame_bx = FB_X - cam_ox + DX * 2; flame_by = FB_Y - cam_oy - DY;
  glow_x = (flame_bx + cam_ox + 16) >> 1; glow_y = (flame_by - flame_h / 2 + cam_oy + 16) >> 1;
  grain_x = c_draw() & 127; grain_y = c_draw() & 63; chroma_x = c_draw() & 127; chroma_y = c_draw() & 63;
  for (int l = 0; l < 256; l++) GN[l] = (uint16_t)((GAMP[l] * expo * 3) >> 9);
  hrow_j[0] = hrow_j[1] = hrow_j[2] = -1; cyc_half = 0;
  { uint8_t a[4] = { 0, 0, (uint8_t)((SW - 1) >> 8), (uint8_t)(SW - 1) }; wire_cmd(0x2A, a, 4); }
  { uint8_t a[4] = { 0, 0, (uint8_t)((SH - 1) >> 8), (uint8_t)(SH - 1) }; wire_cmd(0x2B, a, 4); }
  wire_cmd(0x2C, 0, 0);
  int sb = 0; wire_hold(sb);
  for (int bnd = 0; bnd < SH / BAND; bnd++) {
    uint16_t *strip = STRIPB[sb];
    for (int r = 0; r < BAND; r++) screen_row(bnd * BAND + r, strip + r * SW);
    wire_data(strip, SW * BAND * 2); wire_mark(sb); sb ^= 1; wire_hold(sb); }
  uint32_t c2 = clock_now(); cyc_story = c1 - c0; cyc_draw = c2 - c1; cyc_full = cyc_draw - cyc_half; }
static uint32_t candle_print(void) { uint32_t z = 2166136261u; int32_t v[8] = { SWX.s, SWZ.s, HGT.s, SHX.s, SHY.s, inten, expo, (int32_t)c_seed };
  for (int k = 0; k < 8; k++) z = (z ^ (uint32_t)v[k]) * 16777619u; return z; }
static uint32_t room_print(void) { uint32_t z = 2166136261u; for (int j = 0; j < GY; j++) for (int i = 0; i < GX; i++) { z = (z ^ L0[j][i]) * 16777619u; z = (z ^ (uint8_t)RATEX[j][i] ^ ((uint32_t)(uint8_t)RATEY[j][i] << 8) ^ ((uint32_t)(uint8_t)RATEZ[j][i] << 16) ^ ((uint32_t)ME[j][i] << 24)) * 16777619u; } return z; }
static void candle_begin(void) { grow_camera(); grow_flame(); grow_room(); flame_place(); }

// LESSON-BEGIN 15 of 15: Run
// WHAT    : setup grows everything (4 s dark), loop draws frames. Serial prints real frame times on the board.
// LESSON-END
// ---------------------------------------------------------------- run
void setup(void) {
  Serial.begin(115200); delay(300); Serial.print("CANDLE: panel\n");
  panel_begin(); Serial.printf("CANDLE: wire bus %d device %d (0 is good)\n", m_e1, m_e2);
  { static uint16_t black[SW * BAND]; memset(black, 0, sizeof black);                   /* a dark screen while the room is worked out */
    uint8_t a[4] = { 0, 0, (uint8_t)((SW - 1) >> 8), (uint8_t)(SW - 1) }; wire_cmd(0x2A, a, 4); uint8_t b[4] = { 0, 0, (uint8_t)((SH - 1) >> 8), (uint8_t)(SH - 1) }; wire_cmd(0x2B, b, 4);
    wire_cmd(0x2C, 0, 0); for (int k = 0; k < SH / BAND; k++) wire_data(black, sizeof black); wire_wait_all(); }
  uint32_t t0 = micros(); candle_begin(); Serial.printf("CANDLE: room worked out in %u ms (%u cells)\n", (unsigned)((micros() - t0) / 1000), (unsigned)boot_cells); }
static uint32_t rep_n, rep_sum, rep_max, last_us;
void loop(void) {
  uint32_t now = micros(), us = last_us ? now - last_us : 0; last_us = now;
  frame();
  if (us) { rep_sum += us; if (us > rep_max) rep_max = us; }
  if (++rep_n >= 200) { Serial.printf("CANDLE REAL frame avg %u us (%u fps) worst %u us | half cells %u, full dots %u cycles | flame %d expo %d\n", (unsigned)(rep_sum / rep_n), (unsigned)(rep_n * 1000000ull / (rep_sum ? rep_sum : 1)), (unsigned)rep_max,
      (unsigned)cyc_half, (unsigned)cyc_full, (int)inten, (int)expo); rep_n = 0; rep_sum = 0; rep_max = 0; } }
