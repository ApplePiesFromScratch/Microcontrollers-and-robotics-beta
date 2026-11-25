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
#include <stdint.h>
#include <stddef.h>
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
