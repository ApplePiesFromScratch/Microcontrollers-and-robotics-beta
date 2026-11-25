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
// ---------------------------------------------------------------- NAGARE itself (core/sim.h, core/nagare.h, core/draw.inc, pasted by tools/make_ino.py)
/* NAGARE: the screen. Left the textbook, right ours, the same box, the same puffs, the same colour for the same warmth.
   Whole numbers on our side; the textbook in fixed point. The clock is not read: a frame is one step of each. */
/* NAGARE (流れ, flowing): the same heated box, two ways.
   TEXT: the textbook. Continuity, Navier-Stokes and the energy equation, discretised with a time step DT, solved by the stable-fluids
         method (Stam 1999): lift as a body force (Boussinesq), viscous diffusion, pressure projection for continuity, semi-Lagrangian
         advection, heat advected and conducted. Fixed point (Q16), as an embedded engineer would on a chip with no FPU.
   OURS: no equation imported. Each place holds stuff, a carried rate (as momentum) and warmth. Every change is a hand-on between
         neighbours, or a booked feed or vent:
           MOVE   each place's packet goes where its rate takes it, split over the places it lands on; leftovers to the biggest share
           PUSH   each face: the crowding on both sides becomes rate, taken from one side and given to the other
           DRAG   each face: neighbours trade rate, and warmth, by how much they differ
           LIFT   each face: the warmer of two neighbours is handed upward rate by the cooler, by their warmth difference
           FEED / VENT   warmth in at the heater, out at the cold lid, both counted; the walls' pushes are counted
         So stuff, warmth and both components of momentum are kept by construction, to the unit, every latch. No DT: a latch is a trade. */
typedef struct { int64_t m, q, px, py; } books_t;
#define GW 50
#define GH 40
#define NC (GW * GH)
#define AT(i, j) ((j) * GW + (i))
/* ---------------------------------------------------------------- shared: the heater and the scale of warmth */
#define W1 256                                                        /* ours: warmth per stuff of 1.0 */
#define M0 1024                                                       /* ours: stuff in a place at the start */
#define ONE 65536                                                     /* the textbook's 1.0 (Q16) */
#ifndef HEATER0
#define HEATER0 0
#endif
#ifndef BLOB_W
#define BLOB_W 0
#endif
#define BLOB_X (GW / 2)
#define BLOB_Y (GH - 8)
#define BLOB_R 5
static int heater_x = GW / 2, heater_on = HEATER0;                          /* the heater: 4 places of the floor, centred at heater_x */
#ifndef HEAT_W
#define HEAT_W 6                                                      /* warmth per stuff added at a heater place per latch, in 1/256 */
#endif
#ifndef COOL_SH
#define COOL_SH 4                                                     /* the lid hands 1/16 of a place's warmth out per latch */
#endif
static int is_heater(int i) { return heater_on && i >= heater_x - 2 && i < heater_x + 2; }

/* ================================================================ OURS: hand-ons only */
static int32_t M[NC], PX[NC], PY[NC], Q[NC];                         /* stuff; momentum (stuff x rate in 1/256 place a latch); warmth (stuff x warmth / 256) */
static int32_t NM[NC], NPX[NC], NPY[NC], NQ[NC];                     /* where MOVE puts what it hands on */
static int64_t fed_q, vented_q, walls_px, walls_py; static uint32_t ours_latch;                  /* the books' outside entries */
static int64_t start_m, start_q, start_px, start_py;
#ifndef KP
#define KP 3000                                                         /* PUSH: momentum a face hands per unit of crowding on its two sides, x 1/16 */
#endif
#ifndef KN
#define KN 1                                                          /* DRAG of rate: share of the difference traded per latch, in 1/16 */
#endif
#ifndef KB
#define KB 4                                                          /* DRAG along a face's own line (parting, converging), in 1/16 */
#endif
#ifndef KX
#define KX 4                                                          /* EXCHANGE of crowding, in 1/32 of the difference */
#endif
#ifndef KK
#define KK 0                                                          /* DRAG of warmth, in 1/16 */
#endif
#ifndef KL
#define KL 24                                                          /* LIFT: upward momentum per unit of warmth difference, in 1/16 */
#endif
#ifndef LIFT_ALL
#define LIFT_ALL 0
#endif
#ifndef VMAX
#define VMAX 200                                                      /* MOVE: a packet lands at most this far (in 1/256 place); its momentum is not changed */
#endif
static int32_t qdiv(int64_t a, int64_t b) {                          /* toward zero: a trade and its mirror are the same size */
  if (a >= -2147483647LL && a <= 2147483647LL && b >= -2147483647LL && b <= 2147483647LL) return (int32_t)a / (int32_t)b;   /* 32 bits when both fit: one instruction */
  return (int32_t)(a / b); }
static void ours_begin(void) {
  for (int k = 0; k < NC; k++) { M[k] = M0; PX[k] = 0; PY[k] = 0; Q[k] = 0; }
  /* a faint seed so the two sides are not mirror-locked: the heater's left neighbour a touch warmer (the textbook gets the same) */
  for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int dx = i - BLOB_X, dy = j - BLOB_Y; if (dx * dx + dy * dy <= BLOB_R * BLOB_R) Q[AT(i, j)] = M0 * BLOB_W / W1; }   /* a warm blob */
  Q[AT(GW / 2 - 3, GH - 1)] += M0 * 8 / W1;
  fed_q = vented_q = walls_px = walls_py = 0; ours_latch = 0;
  start_m = start_q = start_px = start_py = 0; for (int k = 0; k < NC; k++) { start_m += M[k]; start_q += Q[k]; start_px += PX[k]; start_py += PY[k]; } }
static void hand(int k, int32_t m, int32_t px, int32_t py, int32_t q) { NM[k] += m; NPX[k] += px; NPY[k] += py; NQ[k] += q; }
static void split4(int32_t a, const int32_t w[4], int32_t out[4]) {   /* a split by weights summing to 65536; the leftover to the biggest share */
  int32_t s = 0; int big = 0; for (int c = 0; c < 4; c++) { out[c] = (int32_t)(((int64_t)a * w[c]) / 65536); s += out[c]; if (w[c] > w[big]) big = c; } out[big] += a - s; }
static void ours_move(void) {
  for (int k = 0; k < NC; k++) { NM[k] = 0; NPX[k] = 0; NPY[k] = 0; NQ[k] = 0; }
  for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int k = AT(i, j); int32_t m = M[k]; if (m <= 0) { hand(k, m, PX[k], PY[k], Q[k]); continue; }
    int32_t px = PX[k], py = PY[k], ux = qdiv(px, m), uy = qdiv(py, m);
    if (ux > VMAX) ux = VMAX; if (ux < -VMAX) ux = -VMAX; if (uy > VMAX) uy = VMAX; if (uy < -VMAX) uy = -VMAX;
    int32_t tx = i * 256 + ux, ty = j * 256 + uy;
    if (tx < 0) { tx = -tx; walls_px += -2 * (int64_t)px; px = -px; } if (tx > (GW - 1) * 256) { tx = 2 * (GW - 1) * 256 - tx; walls_px += -2 * (int64_t)px; px = -px; }   /* a wall turns the packet back: counted */
    if (ty < 0) { ty = -ty; walls_py += -2 * (int64_t)py; py = -py; } if (ty > (GH - 1) * 256) { ty = 2 * (GH - 1) * 256 - ty; walls_py += -2 * (int64_t)py; py = -py; }
    int i0 = tx >> 8, j0 = ty >> 8, fx = tx & 255, fy = ty & 255; if (i0 >= GW - 1) { i0 = GW - 2; fx = 256; } if (j0 >= GH - 1) { j0 = GH - 2; fy = 256; }
    int32_t w[4] = { (256 - fx) * (256 - fy), fx * (256 - fy), (256 - fx) * fy, fx * fy }; int kk[4] = { AT(i0, j0), AT(i0 + 1, j0), AT(i0, j0 + 1), AT(i0 + 1, j0 + 1) };
    int32_t sm[4], sx[4], sy[4], sq[4]; split4(m, w, sm); split4(px, w, sx); split4(py, w, sy); split4(Q[k], w, sq);
    for (int c = 0; c < 4; c++) hand(kk[c], sm[c], sx[c], sy[c], sq[c]); }
  for (int k = 0; k < NC; k++) { M[k] = NM[k]; PX[k] = NPX[k]; PY[k] = NPY[k]; Q[k] = NQ[k]; } }
static void face(int a, int b, int horizontal) {                       /* one face between places a and b (b right of a, or b below a): PUSH, DRAG, LIFT */
  int32_t ma = M[a], mb = M[b]; int64_t s = (int64_t)ma + mb; if (s <= 0) return;   /* every trade is read from the places as the latch found them, */
  int32_t pf = (int32_t)((s * KP) >> 5);                              /* and handed on afterwards: no face sees another face's trade (no sweep order) */
  if (horizontal) { NPX[a] -= pf; NPX[b] += pf; } else { NPY[a] -= pf; NPY[b] += pf; }                                  /* PUSH: a hands pf to b */
  int32_t dx = qdiv(((int64_t)PX[a] * mb - (int64_t)PX[b] * ma) * KN, s * 16), dy = qdiv(((int64_t)PY[a] * mb - (int64_t)PY[b] * ma) * KN, s * 16);   /* DRAG */
  NPX[a] -= dx; NPX[b] += dx; NPY[a] -= dy; NPY[b] += dy;
  { int32_t pa = horizontal ? PX[a] : PY[a], pb = horizontal ? PX[b] : PY[b];          /* DRAG along the face's own line: parting and converging, never turning */
    int32_t dn = qdiv(((int64_t)pa * mb - (int64_t)pb * ma) * KB, s * 16); if (horizontal) { NPX[a] -= dn; NPX[b] += dn; } else { NPY[a] -= dn; NPY[b] += dn; } }
  { int32_t gap = ma - mb;                                             /* EXCHANGE: crowding handed down its own difference to the next place, with the */
    int32_t dm = qdiv((int64_t)gap * KX, 32); int src = dm > 0 ? a : b, dst = dm > 0 ? b : a; int32_t am = dm > 0 ? dm : -dm, ms = M[src];   /* rate and warmth it carries */
    if (am > ms / 8) am = ms / 8;                                       /* PUSH reads two places apart and is blind to an alternation; this sees it */
    if (am > 0 && ms > 0) { int32_t tpx = qdiv((int64_t)PX[src] * am, ms), tpy = qdiv((int64_t)PY[src] * am, ms), tq = qdiv((int64_t)Q[src] * am, ms);
      NM[src] -= am; NM[dst] += am; NPX[src] -= tpx; NPX[dst] += tpx; NPY[src] -= tpy; NPY[dst] += tpy; NQ[src] -= tq; NQ[dst] += tq; } }
  int32_t e = qdiv((int64_t)Q[a] * mb - (int64_t)Q[b] * ma, s);       /* warmth difference, for a place of average stuff */
  int32_t dq = qdiv((int64_t)e * KK, 16); NQ[a] -= dq; NQ[b] += dq;
  (void)horizontal; }
static void ours_faces(void) {
  for (int k = 0; k < NC; k++) { NM[k] = 0; NPX[k] = 0; NPY[k] = 0; NQ[k] = 0; }
  for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { if (i + 1 < GW) face(AT(i, j), AT(i + 1, j), 1); if (j + 1 < GH) face(AT(i, j), AT(i, j + 1), 0); }
  for (int j = 0; j < GH; j++) { int32_t p0 = (int32_t)(((int64_t)2 * M[AT(0, j)] * KP) >> 5), p1 = (int32_t)(((int64_t)2 * M[AT(GW - 1, j)] * KP) >> 5);   /* the walls push back */
    NPX[AT(0, j)] += p0; walls_px += p0; NPX[AT(GW - 1, j)] -= p1; walls_px -= p1; }                                                                  /* as a face would: counted */
  for (int i = 0; i < GW; i++) { int32_t p0 = (int32_t)(((int64_t)2 * M[AT(i, 0)] * KP) >> 5), p1 = (int32_t)(((int64_t)2 * M[AT(i, GH - 1)] * KP) >> 5);
    NPY[AT(i, 0)] += p0; walls_py += p0; NPY[AT(i, GH - 1)] -= p1; walls_py -= p1; }
  for (int k = 0; k < NC; k++) { M[k] += NM[k]; PX[k] += NPX[k]; PY[k] += NPY[k]; Q[k] += NQ[k]; } }
static int32_t spoil_at = -1;                                          /* the control: a latch at which one line SETS a warmth instead of moving it */
static void ours_step(void) {
  ours_move();
  ours_faces();
  { int64_t mt = 0, qt = 0, given = 0; for (int k = 0; k < NC; k++) { mt += M[k]; qt += Q[k]; }   /* LIFT: a place warmer than the box as a whole is handed upward rate */
    int32_t r = (int32_t)((qt << 16) / mt);                             /* the box's warmth per stuff, once a latch (x 65536) */
    for (int k = 0; k < NC; k++) { int32_t dev = Q[k] - (int32_t)(((int64_t)M[k] * r) >> 16); int32_t l = qdiv((int64_t)dev * KL * W1, 16 * 64); PY[k] -= l; given -= l; }   /* by the ground, cooler */
    walls_py += given; }                                                                                                                                 /* sinks; what it gives is counted */
  for (int i = 0; i < GW; i++) { if (is_heater(i)) { int32_t f = M[AT(i, GH - 1)] * HEAT_W / W1; Q[AT(i, GH - 1)] += f; fed_q += f; }   /* FEED */
    int32_t v = Q[AT(i, 0)] >> COOL_SH; Q[AT(i, 0)] -= v; vented_q += v; }                                                           /* VENT */
  if ((int32_t)ours_latch == spoil_at) Q[AT(GW / 2, GH / 2)] = 0;     /* the planted fault: a quantity set, not moved */
  ours_latch++; }
static books_t ours_books(void) { books_t b = { 0, 0, 0, 0 }; for (int k = 0; k < NC; k++) { b.m += M[k]; b.q += Q[k]; b.px += PX[k]; b.py += PY[k]; } return b; }
static int ours_books_shut(void) { books_t b = ours_books();
  return b.m == start_m && b.q == start_q + fed_q - vented_q && b.px == start_px + walls_px && b.py == start_py + walls_py; }
static int32_t ours_warmth(int k) { return M[k] > 0 ? qdiv((int64_t)Q[k] * W1, M[k]) : 0; }   /* a reading: warmth per stuff, 256 = 1.0 */

/* ================================================================ TEXT: the textbook, stable fluids in Q16 */
#define TW_ (GW + 2)
#define TN ((GW + 2) * (GH + 2))
#define TI(i, j) ((j + 1) * TW_ + (i + 1))                             /* interior i 0..GW-1, j 0..GH-1; a ring of boundary cells round it */
static int32_t U[TN], V[TN], T[TN], U0[TN], V0[TN], T0[TN];          /* velocity (places a unit of t), temperature; U0/V0 also hold p and div */
#ifndef DT
#define DT (ONE / 4)                                                  /* the time step: an independent variable */
#endif
#ifndef BETA
#define BETA (ONE / 4)                                            /* lift per unit temperature */
#endif
#ifndef NU
#define NU (ONE / 50)                                                 /* viscosity */
#endif
#ifndef KAPPA
#define KAPPA (ONE / 40)                                              /* conductivity */
#endif
#ifndef JACOBI
#define JACOBI 20
#endif
static int64_t t_fed, t_vented, t_start; static int32_t t_div_max;
static int32_t qmul(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b + 32768) >> 16); }   /* rounded: a floor here would drain every field a little each step */
static void set_bnd(int b, int32_t *x) {                               /* walls: normal velocity 0 (b 1: u, 2: v), others copied */
  for (int j = 0; j < GH; j++) { x[TI(-1, j)] = b == 1 ? -x[TI(0, j)] : x[TI(0, j)]; x[TI(GW, j)] = b == 1 ? -x[TI(GW - 1, j)] : x[TI(GW - 1, j)]; }
  for (int i = 0; i < GW; i++) { x[TI(i, -1)] = b == 2 ? -x[TI(i, 0)] : x[TI(i, 0)]; x[TI(i, GH)] = b == 2 ? -x[TI(i, GH - 1)] : x[TI(i, GH - 1)]; }
  x[TI(-1, -1)] = (x[TI(0, -1)] + x[TI(-1, 0)]) / 2; x[TI(GW, -1)] = (x[TI(GW - 1, -1)] + x[TI(GW, 0)]) / 2;
  x[TI(-1, GH)] = (x[TI(0, GH)] + x[TI(-1, GH - 1)]) / 2; x[TI(GW, GH)] = (x[TI(GW - 1, GH)] + x[TI(GW, GH - 1)]) / 2; }
static void text_begin(void) {
  for (int k = 0; k < TN; k++) { U[k] = V[k] = T[k] = U0[k] = V0[k] = T0[k] = 0; }
  for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int dx = i - BLOB_X, dy = j - BLOB_Y; if (dx * dx + dy * dy <= BLOB_R * BLOB_R) T[TI(i, j)] = BLOB_W * ONE / W1; }   /* the same blob */
  T[TI(GW / 2 - 3, GH - 1)] += 8 * ONE / W1;                          /* the same faint seed as ours */
  t_fed = t_vented = 0; t_start = 0; for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) t_start += T[TI(i, j)]; t_div_max = 0; }
static void advect(int b, int32_t *d, const int32_t *d0, const int32_t *u, const int32_t *v) {   /* semi-Lagrangian: d(x) = d0(x - DT u(x)) */
  for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) {
    int32_t x = (i << 16) - qmul(DT, u[TI(i, j)]), y = (j << 16) - qmul(DT, v[TI(i, j)]);
    if (x < -(ONE / 2)) x = -(ONE / 2); if (x > ((GW - 1) << 16) + ONE / 2) x = ((GW - 1) << 16) + ONE / 2;
    if (y < -(ONE / 2)) y = -(ONE / 2); if (y > ((GH - 1) << 16) + ONE / 2) y = ((GH - 1) << 16) + ONE / 2;
    int i0 = (x + ONE) >> 16, j0 = (y + ONE) >> 16; i0--; j0--; int32_t s1 = x - i0 * ONE, t1 = y - j0 * ONE, s0 = ONE - s1, t0 = ONE - t1;
    d[TI(i, j)] = qmul(s0, qmul(t0, d0[TI(i0, j0)]) + qmul(t1, d0[TI(i0, j0 + 1)])) + qmul(s1, qmul(t0, d0[TI(i0 + 1, j0)]) + qmul(t1, d0[TI(i0 + 1, j0 + 1)])); }
  set_bnd(b, d); }
static void diffuse(int b, int32_t *x, int32_t k) {                    /* explicit: x += DT k lap(x) */
  int32_t a = qmul(DT, k); for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int c = TI(i, j); T0[c] = x[c] + qmul(a, x[c - 1] + x[c + 1] + x[c - TW_] + x[c + TW_] - 4 * x[c]); }
  for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) x[TI(i, j)] = T0[TI(i, j)]; set_bnd(b, x); }
static void project(void) {                                            /* continuity: remove the part of the velocity that is not divergence-free */
  int32_t *p = U0, *dv = V0;
  for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int c = TI(i, j); dv[c] = -(U[c + 1] - U[c - 1] + V[c + TW_] - V[c - TW_]) / 2; p[c] = 0; }
  set_bnd(0, dv); set_bnd(0, p);
  for (int it = 0; it < JACOBI; it++) { for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int c = TI(i, j); T0[c] = (dv[c] + p[c - 1] + p[c + 1] + p[c - TW_] + p[c + TW_]) / 4; }
    for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) p[TI(i, j)] = T0[TI(i, j)]; set_bnd(0, p); }
  for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int c = TI(i, j); U[c] -= (p[c + 1] - p[c - 1]) / 2; V[c] -= (p[c + TW_] - p[c - TW_]) / 2; }
  set_bnd(1, U); set_bnd(2, V); }
static void text_div_read(void) { int32_t mx = 0; for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int c = TI(i, j); int32_t d = (U[c + 1] - U[c - 1] + V[c + TW_] - V[c - TW_]) / 2; if (d < 0) d = -d; if (d > mx) mx = d; } t_div_max = mx; }
static void text_step(void) {
  for (int i = 0; i < GW; i++) { if (is_heater(i)) { int32_t f = HEAT_W * ONE / W1; T[TI(i, GH - 1)] += f; t_fed += f; }   /* the source term Q */
    int32_t v = T[TI(i, 0)] >> COOL_SH; T[TI(i, 0)] -= v; t_vented += v; }                                                /* the cold lid */
  for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) V[TI(i, j)] -= qmul(DT, qmul(BETA, T[TI(i, j)]));            /* body force: the Boussinesq lift (up is -y) */
  diffuse(1, U, NU); diffuse(2, V, NU);                                                                                   /* viscous stress */
  project();                                                                                                               /* continuity */
  for (int k = 0; k < TN; k++) { U0[k] = U[k]; V0[k] = V[k]; }
  advect(1, U, U0, U0, V0); advect(2, V, V0, U0, V0);                  /* inertia: (u.grad)u, read from the copies */
  project();
  for (int k = 0; k < TN; k++) T0[k] = T[k]; advect(0, T, T0, U, V); /* energy: u.grad T */
  diffuse(0, T, KAPPA);                                                                                                    /* energy: conduction */
  text_div_read(); }
static int64_t text_heat(void) { int64_t h = 0; for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) h += T[TI(i, j)]; return h; }
static int32_t text_warmth(int i, int j) { return T[TI(i, j)] >> 8; }   /* on ours' scale: 256 = 1.0 */

/* ================================================================ a puff: a disc of warmth laid at the floor, the same on both sides, booked as fed */
#define PUFF_R 4
#define PUFF_W 256                                                    /* warmth per stuff added inside the disc: 1.0 */
static void puff(int x) {
  for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int dx = i - x, dy = j - (GH - 1 - PUFF_R); if (dx * dx + dy * dy > PUFF_R * PUFF_R) continue;
    int32_t f = (int32_t)((int64_t)M[AT(i, j)] * PUFF_W / W1); Q[AT(i, j)] += f; fed_q += f;
    int32_t g = PUFF_W * ONE / W1; T[TI(i, j)] += g; t_fed += g; } }

#define SW 320
#define SH 240
static uint8_t FB[SW * SH];
enum { K_BG, K_BAR, K_TEXT, K_DIM, K_GOOD, K_BAD, K_FOCUS, K_LINE, K_WHITE, K_TITLE_T, K_TITLE_O, K_HEAT0, K_DIV0 = K_HEAT0 + 32, NCOLOUR = K_DIV0 + 33 };
static uint8_t PALRGB[NCOLOUR][3];
static void palette_begin(void) {
  static const uint8_t B[11][3] = { { 6, 8, 14 }, { 2, 3, 6 }, { 228, 234, 244 }, { 128, 138, 158 }, { 90, 230, 120 }, { 255, 80, 70 }, { 255, 210, 80 },
    { 60, 70, 100 }, { 255, 255, 255 }, { 120, 170, 255 }, { 255, 170, 90 } };
  for (int k = 0; k < 11; k++) for (int c = 0; c < 3; c++) PALRGB[k][c] = B[k][c];
  static const uint8_t H[6][3] = { { 4, 4, 18 }, { 60, 10, 110 }, { 170, 20, 110 }, { 240, 80, 30 }, { 255, 200, 40 }, { 255, 255, 230 } };   /* warmth: dark, violet, red, orange, yellow, white */
  for (int k = 0; k < 32; k++) { int seg = k * 5 / 32, f = (k * 5) % 32; for (int c = 0; c < 3; c++) PALRGB[K_HEAT0 + k][c] = (uint8_t)(H[seg][c] + (H[seg + 1][c] - H[seg][c]) * f / 32); }
  for (int k = 0; k <= 32; k++) { int d = k - 16, a = d < 0 ? -d : d; PALRGB[K_DIV0 + k][0] = (uint8_t)(d > 0 ? 30 + a * 14 : 10); PALRGB[K_DIV0 + k][1] = (uint8_t)(10 + a * 3); PALRGB[K_DIV0 + k][2] = (uint8_t)(d < 0 ? 30 + a * 14 : 10); } }
/* drawing, letters and small words: taken whole from IRORI (core/irori.h) */
static int clipx0 = 0, clipy0 = 0, clipx1 = SW, clipy1 = SH;
static void px(int x, int y, uint8_t c) { if (x >= clipx0 && x < clipx1 && y >= clipy0 && y < clipy1) FB[y * SW + x] = c; }
static void fill(int x, int y, int w, int h, uint8_t c) {
  int x0 = x < clipx0 ? clipx0 : x, y0 = y < clipy0 ? clipy0 : y, x1 = x + w > clipx1 ? clipx1 : x + w, y1 = y + h > clipy1 ? clipy1 : y + h;
  for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) FB[yy * SW + xx] = c; }
static void box(int x, int y, int w, int h, uint8_t c) { fill(x, y, w, 1, c); fill(x, y + h - 1, w, 1, c); fill(x, y, 1, h, c); fill(x + w - 1, y, 1, h, c); }
static void hline(int x0, int x1, int y, uint8_t c) { if (x1 < x0) { int t = x0; x0 = x1; x1 = t; } fill(x0, y, x1 - x0 + 1, 1, c); }
static void vline(int x, int y0, int y1, uint8_t c) { if (y1 < y0) { int t = y0; y0 = y1; y1 = t; } fill(x, y0, 1, y1 - y0 + 1, c); }

/* ---------------------------------------------------------------- letters: 5x7 (OS 4's face, with a few more) */
static const char FACE_CH[] = "0123456789ABCDEFGHIKLMNOPRSTUVWXY:-!.,/%=+()'?JQZ><*_#;\"&@[]|$^~`{}\\";   /* $ partial, ^ nabla, ~ dot, ` squared, { rho, } nu, \\ kappa */
static const uint8_t FACE[68][7] = {
  {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31}, {31,2,4,2,1,17,14}, {2,6,10,18,31,2,2},
  {31,16,30,1,1,17,14}, {6,8,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14}, {14,17,17,15,1,2,12},
  {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31},
  {31,16,16,30,16,16,16}, {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14}, {17,18,20,24,20,18,17},
  {16,16,16,16,16,16,31}, {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
  {30,17,17,30,20,18,17}, {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14}, {17,17,17,17,10,10,4},
  {17,17,17,21,21,27,17}, {17,17,10,4,10,17,17}, {17,17,10,4,4,4,4}, {0,12,12,0,12,12,0}, {0,0,0,31,0,0,0}, {4,4,4,4,4,0,4},
  {0,0,0,0,0,12,12}, {0,0,0,0,12,4,8}, {1,1,2,4,8,16,16}, {24,25,2,4,8,19,3}, {0,0,31,0,31,0,0}, {0,4,4,31,4,4,0}, {2,4,8,8,8,4,2},
  {8,4,2,2,2,4,8}, {4,4,8,0,0,0,0}, {14,17,1,2,4,0,4}, {7,2,2,2,2,18,12}, {14,17,17,17,21,18,13}, {31,1,2,4,8,16,31}, {8,4,2,1,2,4,8},
  {2,4,8,16,8,4,2}, {0,4,21,14,21,4,0}, {0,0,0,0,0,0,31}, {10,10,31,10,31,10,10}, {0,12,12,0,12,4,8}, {10,10,0,0,0,0,0},
  {12,18,20,8,21,18,13}, {14,17,23,21,23,16,14}, {14,8,8,8,8,8,14}, {14,2,2,2,2,2,14}, {4,4,4,4,4,4,4},
  { 0x0E, 0x01, 0x01, 0x0F, 0x11, 0x11, 0x0E }, { 0x1F, 0x11, 0x11, 0x0A, 0x0A, 0x04, 0x04 }, { 0, 0, 0, 0x04, 0, 0, 0 }, { 0x0C, 0x02, 0x04, 0x0E, 0, 0, 0 }, { 0, 0, 0x0E, 0x11, 0x1E, 0x10, 0x10 }, { 0, 0, 0x11, 0x11, 0x0A, 0x0A, 0x04 }, { 0, 0, 0x12, 0x14, 0x18, 0x14, 0x12 }
};
static uint8_t GLYPH[128];
static void face_begin(void) { for (int i = 0; FACE_CH[i]; i++) GLYPH[(uint8_t)FACE_CH[i]] = (uint8_t)(i + 1); }
static int text(int x, int y, const char *s, uint8_t c, int sc) {      /* returns where the next letter goes */
  for (; *s; s++) { int ch = (uint8_t)*s; if (ch >= 'a' && ch <= 'z') ch -= 32; int g = ch < 128 ? GLYPH[ch] : 0;
    if (g) for (int r = 0; r < 7; r++) { uint8_t b = FACE[g - 1][r]; for (int k = 0; k < 5; k++) if (b >> (4 - k) & 1) { if (sc == 1) px(x + k, y + r, c); else fill(x + k * sc, y + r * sc, sc, sc, c); } }
    x += 6 * sc; }
  return x; }
static int text_w(const char *s, int sc) { int n = 0; while (s[n]) n++; return n * 6 * sc - sc; }
static void text_c(int cx, int y, const char *s, uint8_t c, int sc) { text(cx - text_w(s, sc) / 2, y, s, c, sc); }
static int text_fit(int x, int y, const char *s, uint8_t c, int sc, int maxw) {   /* as many letters as fit in maxw; a cut is marked with two dots */
  int n = 0; while (s[n]) n++; int fit = (maxw + sc) / (6 * sc); if (fit < 0) fit = 0; if (n <= fit) return text(x, y, s, c, sc);
  char b[64]; int k = fit - 1; if (k < 0) k = 0; if (k > 62) k = 62; for (int i = 0; i < k; i++) b[i] = s[i]; b[k] = 0; int e = text(x, y, b, c, sc);
  if (fit >= 1) { fill(e, y + 6 * sc, sc, sc, c); fill(e + 2 * sc, y + 6 * sc, sc, sc, c); } return e + 4 * sc; }

/* ---------------------------------------------------------------- small whole-number words (no libc: the bare build has none) */
static int s_len(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void s_cpy(char *d, const char *s, int cap) { int i = 0; while (s[i] && i < cap - 1) { d[i] = s[i]; i++; } d[i] = 0; }
static void s_cat(char *d, const char *s, int cap) { int n = s_len(d); s_cpy(d + n, s, cap - n); }
static void s_u(char *b, uint32_t v) { char t[11]; int n = 0; do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v); for (int i = 0; i < n; i++) b[i] = t[n - 1 - i]; b[n] = 0; }
static void s_i(char *b, int64_t v) { if (v < 0) { b[0] = '-'; v = -v; b++; } char t[21]; int n = 0; uint64_t u = (uint64_t)v; do { t[n++] = (char)('0' + u % 10); u /= 10; } while (u); for (int i = 0; i < n; i++) b[i] = t[n - 1 - i]; b[n] = 0; }
static void s_cat_i(char *d, int64_t v, int cap) { char b[24]; s_i(b, v); s_cat(d, b, cap); }
static void s_two(char *d, uint32_t v, int cap) { char b[3] = { (char)('0' + v / 10 % 10), (char)('0' + v % 10), 0 }; s_cat(d, b, cap); }
static void s_hex2(char *d, int v, int cap) { const char *H = "0123456789ABCDEF"; char b[3] = { H[(v >> 4) & 15], H[v & 15], 0 }; s_cat(d, b, cap); }

/* ---------------------------------------------------------------- what the board gives (readings only) */
static void hal_buttons(uint8_t down[3]);                             /* A, B, C */
static int  hal_stick(int *x, int *y, int *press);                     /* 0..4095, the press; 0 if no stick */
static uint32_t hal_cycles(void);                                      /* the CPU's cycle count; 0 on the desk */
/* ---------------------------------------------------------------- the frame */
#define PX0_T 4                                                        /* the two boxes on the screen: 50 x 40 places at 3 dots a place */
#define PX0_O 166
#define PY0 62
#define CELL 3
static int view, cursor_x = GW / 2, frames, puffs, ghost_on = 1, idle, g_t; static uint8_t was[3]; static int cx0 = 2048, centred, stick_held;
static const char *VIEW_NAME[3] = { "WARMTH", "SPEED (2 X 2)", "TURNING (LOOP 2)" };
static uint32_t cyc_t, cyc_o, cyc_d;                                   /* cycles of the last step of each side, and of drawing */
static int32_t mag(int32_t u, int32_t v) { if (u < 0) u = -u; if (v < 0) v = -v; return u > v ? u + v / 2 : v + u / 2; }
static void rate_t(int i, int j, int32_t *u, int32_t *v) {             /* the textbook's velocity over a 2 x 2 cut, as places a step (DT x U), in 1/256 */
  int i1 = i < GW - 1 ? i + 1 : i, j1 = j < GH - 1 ? j + 1 : j; int64_t su = (int64_t)U[TI(i, j)] + U[TI(i1, j)] + U[TI(i, j1)] + U[TI(i1, j1)], sv = (int64_t)V[TI(i, j)] + V[TI(i1, j)] + V[TI(i, j1)] + V[TI(i1, j1)];
  *u = (int32_t)((su * DT) >> 26); *v = (int32_t)((sv * DT) >> 26); }
static void rate_o(int i, int j, int32_t *u, int32_t *v) {             /* ours over the same cut: the momentum there over the stuff there, places a latch, in 1/256 */
  int i1 = i < GW - 1 ? i + 1 : i, j1 = j < GH - 1 ? j + 1 : j; int a = AT(i, j), b = AT(i1, j), c = AT(i, j1), d = AT(i1, j1);
  int64_t m = (int64_t)M[a] + M[b] + M[c] + M[d]; if (m <= 0) m = 1; *u = qdiv((int64_t)PX[a] + PX[b] + PX[c] + PX[d], m); *v = qdiv((int64_t)PY[a] + PY[b] + PY[c] + PY[d], m); }
static int32_t *RU = NPX, *RV = NPY;                                  /* the 2 x 2 rates, read once a frame into ours' scratch (free while drawing) */
static void rates_read(int ours) { for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { if (ours) rate_o(i, j, &RU[AT(i, j)], &RV[AT(i, j)]); else rate_t(i, j, &RU[AT(i, j)], &RV[AT(i, j)]); } }
static int32_t field_any(int i, int j, int ours) {
  if (view == 0) return ours ? ours_warmth(AT(i, j)) : text_warmth(i, j);
  if (view == 1) return mag(RU[AT(i, j)], RV[AT(i, j)]) * 3 / 2;
  int il = i > 1 ? i - 2 : 0, jl = j > 1 ? j - 2 : 0, ir = i < GW - 2 ? i + 2 : i, jd = j < GH - 2 ? j + 2 : j;   /* turning: the rate round a loop of 2 places */
  return (RU[AT(i, jl)] + RV[AT(ir, j)] - RU[AT(i, jd)] - RV[AT(il, j)]) * 2; }
static int32_t field_t(int i, int j) { return field_any(i, j, 0); }
static int32_t field_o(int i, int j) { return field_any(i, j, 1); }
static uint8_t colour_of(int32_t v) {
  if (view == 2) { int k = 16 + (v >> 3); return (uint8_t)(K_DIV0 + (k < 0 ? 0 : k > 32 ? 32 : k)); }
  int k = (int)(v / 6); return (uint8_t)(K_HEAT0 + (k < 0 ? 0 : k > 31 ? 31 : k)); }   /* warmth 0..186 of 256 across the 32 colours */
static int16_t DI[GW * CELL], DT_[GW * CELL], DJ[GH * CELL], DTY[GH * CELL]; static int dtab;   /* which places and shares each dot reads: made once */
static void draw_tables(void) { for (int x = 0; x < GW * CELL; x++) { int fx = x * 256 / CELL - 128, i0 = fx >> 8, tx = fx & 255; if (fx < 0) { i0 = 0; tx = 0; } if (i0 >= GW - 1) { i0 = GW - 2; tx = 255; } DI[x] = (int16_t)i0; DT_[x] = (int16_t)tx; }
  for (int y = 0; y < GH * CELL; y++) { int fy = y * 256 / CELL - 128, j0 = fy >> 8, ty = fy & 255; if (fy < 0) { j0 = 0; ty = 0; } if (j0 >= GH - 1) { j0 = GH - 2; ty = 255; } DJ[y] = (int16_t)j0; DTY[y] = (int16_t)ty; } dtab = 1; }
static void draw_box(int x0, int ours) {                               /* each place's reading, smoothed across its 3 x 3 dots */
  static int32_t F[NC]; if (!dtab) draw_tables(); if (view) rates_read(ours); for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) F[AT(i, j)] = ours ? field_o(i, j) : field_t(i, j);
  static uint8_t LUT[512]; static int lut_view = -1; if (lut_view != view) { for (int k = 0; k < 512; k++) LUT[k] = colour_of(k - 256); lut_view = view; }   /* a colour for each reading -256..255 */
  for (int y = 0; y < GH * CELL; y++) { int j0 = DJ[y], ty = DTY[y]; const int32_t *r0 = F + j0 * GW, *r1 = r0 + GW; uint8_t *row = FB + (PY0 + y) * SW + x0;
    for (int x = 0; x < GW * CELL; x++) { int i0 = DI[x], tx = DT_[x];
      int32_t a = r0[i0] * (256 - tx) + r0[i0 + 1] * tx, b = r1[i0] * (256 - tx) + r1[i0 + 1] * tx, v = (a * (256 - ty) + b * ty) >> 16;
      row[x] = v < -256 ? LUT[0] : v > 255 ? LUT[511] : LUT[v + 256]; } }
  box(x0 - 1, PY0 - 1, GW * CELL + 2, GH * CELL + 2, K_LINE);
  int hx = x0 + cursor_x * CELL + 1; fill(hx - 3, PY0 + GH * CELL + 2, 7, 2, ghost_on ? K_DIM : K_FOCUS); fill(hx - 1, PY0 + GH * CELL, 3, 2, ghost_on ? K_DIM : K_FOCUS);
  if (heater_on) fill(x0 + (heater_x - 2) * CELL, PY0 + GH * CELL - 2, 4 * CELL, 2, K_BAD); }
static void pct_text(char *b, int64_t num, int64_t den, int cap) {   /* a signed percentage with one decimal, from whole numbers */
  if (den == 0) { s_cat(b, "0.0%", cap); return; } int64_t t = num * 1000 / den; if (t < 0) { s_cat(b, "-", cap); t = -t; } else s_cat(b, "+", cap);
  s_cat_i(b, t / 10, cap); s_cat(b, ".", cap); s_cat_i(b, t % 10, cap); s_cat(b, "%", cap); }
static void frame_draw(void) {
  fill(0, 0, SW, SH, K_BG); fill(0, 0, SW, 10, K_BAR);
  char b[64]; text(4, 2, "NAGARE", K_FOCUS, 1); text(48, 2, "THE SAME WARM BOX, TWO WAYS", K_TEXT, 1);
  s_cpy(b, VIEW_NAME[view], 64); text(SW - 4 - text_w(b, 1), 2, b, K_DIM, 1);
  /* the textbook's three laws, as written */
  int y = 13; text(PX0_T, y, "THE TEXTBOOK", K_TITLE_T, 1); text(PX0_T + 80, y, "DT = 1/4", K_BAD, 1); y += 10;
  text(PX0_T, y, "MASS   ${/$T+^~({U)=0", K_TEXT, 1); y += 9;
  text(PX0_T, y, "MOTION $U/$T+(U~^)U", K_TEXT, 1); y += 9;
  text(PX0_T, y, "       =-^P/{+}^`U+G", K_TEXT, 1); y += 9;
  text(PX0_T, y, "ENERGY $T/$T+U~^T=\\^`T+Q", K_TEXT, 1);
  /* ours: the letters */
  y = 13; text(PX0_O, y, "OURS", K_TITLE_O, 1); text(PX0_O + 30, y, "NO DT. NO EQUATION.", K_GOOD, 1); y += 10;
  text(PX0_O, y, "MOVE  EVERY PLACE HANDS ON", K_TEXT, 1); y += 9;
  text(PX0_O, y, "PUSH  CROWDING TO RATE", K_TEXT, 1); y += 9;
  text(PX0_O, y, "DRAG  NEIGHBOURS TRADE", K_TEXT, 1); y += 9;
  text(PX0_O, y, "LIFT  WARMER THAN BOX", K_TEXT, 1);
  draw_box(PX0_T, 0); draw_box(PX0_O, 1);
  /* the books */
  y = PY0 + GH * CELL + 6;
  { s_cpy(b, "MASS: ^~U OFF BY ", 64); int32_t d = t_div_max * 1000 / ONE; s_cat_i(b, d / 1000, 64); s_cat(b, ".", 64); s_cat(b, d % 1000 < 100 ? (d % 1000 < 10 ? "00" : "0") : "", 64); s_cat_i(b, d % 1000, 64);
    text(PX0_T, y, b, d ? K_BAD : K_GOOD, 1); }
  { int64_t gap = text_heat() - (t_start + t_fed - t_vented); s_cpy(b, "HEAT: ", 64); pct_text(b, gap, t_start + t_fed, 64); s_cat(b, " UNACCOUNTED", 64); text(PX0_T, y + 9, b, gap ? K_BAD : K_GOOD, 1); }
  text(PX0_T, y + 18, "MOTION: NOT BOOKED", K_DIM, 1);
  { books_t k = ours_books(); int ok;
    ok = k.m == start_m; text(PX0_O, y, "STUFF: AS AT START", K_TEXT, 1); text(PX0_O + 120, y, ok ? "EXACT" : "OPEN", ok ? K_GOOD : K_BAD, 1);
    ok = k.q == start_q + fed_q - vented_q; text(PX0_O, y + 9, "WARMTH: IN - OUT", K_TEXT, 1); text(PX0_O + 120, y + 9, ok ? "EXACT" : "OPEN", ok ? K_GOOD : K_BAD, 1);
    ok = k.px == start_px + walls_px && k.py == start_py + walls_py; text(PX0_O, y + 18, "MOTION: WALLS GAVE", K_TEXT, 1); text(PX0_O + 120, y + 18, ok ? "EXACT" : "OPEN", ok ? K_GOOD : K_BAD, 1); }
  fill(0, SH - 11, SW, 11, K_BAR);
  text(4, SH - 9, ghost_on ? "DEMO  A PUFF  B HEATER  C VIEW  STICK" : "A PUFF  B HEATER  C VIEW  STICK MOVES", K_DIM, 1);
  s_cpy(b, "STEP ", 64); s_cat_i(b, (int64_t)ours_latch, 64); text(SW - 4 - text_w(b, 1), SH - 9, b, K_DIM, 1); }
/* ---------------------------------------------------------------- hands and the demo hand */
static void nagare_reset(void) { ours_begin(); text_begin(); ours_latch = 0; puffs = 0; heater_on = 0; }
static void hands(void) {
  uint8_t d[3] = { 0, 0, 0 }; hal_buttons(d); int sx, sy, sp = 0, any = 0;
  if (hal_stick(&sx, &sy, &sp)) { if (!centred) { if (sx > 1300 && sx < 2800) cx0 = sx; centred = 1; }
    int dx = sx - cx0; if (dx > 700 || dx < -700) { if (stick_held++ % 3 == 0) { cursor_x += dx > 0 ? 1 : -1; if (cursor_x < 3) cursor_x = 3; if (cursor_x > GW - 4) cursor_x = GW - 4; } any = 1; } else stick_held = 0;
    if (sp) { nagare_reset(); any = 1; } }
  if (d[0] && !was[0]) { puff(cursor_x); puffs++; any = 1; }
  if (d[1] && !was[1]) { heater_on = !heater_on; heater_x = cursor_x; any = 1; }
  if (d[2] && !was[2]) { view = (view + 1) % 3; any = 1; }
  for (int k = 0; k < 3; k++) was[k] = d[k];
  if (any) { ghost_on = 0; idle = 0; } else if (++idle > 900 && !ghost_on) { ghost_on = 1; g_t = 0; nagare_reset(); } }
static void ghost(void) {                                              /* puffs across the floor, the three views, the heater for a while */
  static const struct { int16_t at; int8_t act, x; } S[] = { { 10, 1, 25 }, { 330, 1, 13 }, { 560, 1, 37 }, { 700, 3, 1 }, { 900, 1, 25 }, { 1100, 3, 2 }, { 1300, 1, 18 },
    { 1500, 3, 0 }, { 1550, 2, 25 }, { 2300, 2, 0 }, { 2700, 4, 0 } };
  for (unsigned k = 0; k < sizeof S / sizeof S[0]; k++) if (S[k].at == g_t) {
    if (S[k].act == 1) { cursor_x = S[k].x; puff(cursor_x); puffs++; } else if (S[k].act == 2) { heater_on = S[k].x != 0; heater_x = 25; cursor_x = 25; }
    else if (S[k].act == 3) view = S[k].x; else if (S[k].act == 4) { nagare_reset(); g_t = -1; view = 0; } }
  g_t++; }
static void nagare_begin(void) { face_begin(); palette_begin(); nagare_reset(); }
static void nagare_frame(void) {
  frames++; hands(); if (ghost_on) ghost();
  uint32_t c0 = hal_cycles(); text_step(); uint32_t c1 = hal_cycles(); ours_step(); uint32_t c2 = hal_cycles(); frame_draw(); uint32_t c3 = hal_cycles();
  cyc_t = c1 - c0; cyc_o = c2 - c1; cyc_d = c3 - c2; }
static uint32_t fb_print(void) { uint32_t h = 2166136261u; for (int i = 0; i < SW * SH; i++) h = (h ^ FB[i]) * 16777619u; return h; }

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
