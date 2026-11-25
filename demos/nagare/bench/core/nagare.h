/* NAGARE: the screen. Left the textbook, right ours, the same box, the same puffs, the same colour for the same warmth.
   Whole numbers on our side; the textbook in fixed point. The clock is not read: a frame is one step of each. */
#include "sim.h"
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
#include "draw.inc"
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
