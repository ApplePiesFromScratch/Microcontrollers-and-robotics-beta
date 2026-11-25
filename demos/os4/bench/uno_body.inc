// ======================= the sketch: RULE OS 4 (Uno) =======================
// Stick: the pointer. Press: click. DESK shows four windows, one for each place; click a window to make it big, click again for four.
// FILES: pick a file, RUN it, EDIT it, put its factory program back (DEMO), spoil its spare copy (ZAP) and watch the keeper mend it.
// TASKS: each place's loaded history L and the steps it took this latch; STOP/GO, KILL, VENT (it gives back all its history now),
//        and RULES. RULES: the kernel's own readings, each with - and +, SAVE (kept in lasting memory, two copies) and FIRST.
// The files are OS 2's, byte for byte, in the same places in lasting memory: an Uno that ran OS 2 keeps its programs.
// The top page inverts if the kernel's books ever open (field + every L must equal G0 on every latch).
#define SCR_DESK 0
#define SCR_FILES 1
#define SCR_TASKS 2
#define SCR_EDIT 3
#define SCR_RULES 4
#define MAX_STEPS 48
static const uint8_t DEMO[5][98] PROGMEM = {
  { 33, 8, 3, 24, 2, 9, 5, 25, 1, 10, 1, 11, 58, 12, 23, 15, 3, 168, 0, 96, 0, 120, 7, 104, 0, 123, 0, 104, 0, 97, 0, 121, 7, 105, 0, 124, 1, 105, 0, 37, 0, 53, 2, 38, 0, 46, 2, 189, 1, 198, 1, 37, 1, 53, 2, 38, 1, 46, 2, 184, 5, 192, 6, 160, 0, 152, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 25, 8, 1, 9, 2, 15, 2, 200, 0, 34, 1, 66, 0, 40, 2, 64, 7, 200, 4, 34, 1, 66, 0, 40, 2, 64, 7, 200, 16, 34, 1, 66, 0, 40, 2, 64, 7, 200, 32, 34, 1, 66, 0, 40, 2, 64, 7, 200, 48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 39, 13, 7, 14, 100, 61, 6, 15, 9, 63, 6, 14, 13, 16, 226, 33, 0, 57, 0, 57, 0, 34, 0, 58, 5, 49, 2, 65, 7, 34, 1, 74, 0, 89, 1, 11, 2, 58, 3, 91, 0, 12, 31, 43, 4, 36, 6, 52, 1, 9, 2, 51, 1, 44, 2, 187, 4, 43, 1, 43, 1, 52, 2, 52, 2, 195, 4, 9, 4, 40, 1, 9, 31, 120, 1, 152, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 31, 8, 248, 24, 1, 13, 8, 14, 248, 15, 12, 168, 0, 89, 0, 34, 5, 209, 2, 201, 48, 35, 1, 59, 1, 36, 2, 60, 2, 43, 4, 203, 0, 57, 7, 58, 7, 11, 31, 12, 13, 187, 4, 43, 1, 52, 2, 195, 4, 96, 0, 128, 5, 104, 0, 128, 6, 104, 0, 248, 1, 152, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 31, 8, 1, 24, 2, 9, 61, 25, 253, 14, 1, 15, 61, 13, 10, 12, 16, 168, 0, 34, 1, 50, 0, 35, 2, 83, 2, 202, 0, 203, 8, 184, 5, 192, 4, 185, 5, 193, 4, 96, 0, 97, 0, 128, 1, 104, 0, 128, 1, 105, 0, 128, 6, 104, 0, 129, 7, 105, 0, 248, 2, 152, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };   // BOUNCE, ROOT, SLOPE, TURN, MEET
static const char OP_NAME[32][6] PROGMEM = { "END", "SET", "ISO", "SEED", "MOV", "ADD", "SUB", "MIX", "DIV", "RATE", "WHEN", "VAL", "LATCH", "FLIP", "LAND", "IFLT", "IFEQ", "IFBTN", "LOOP", "JMP", "SHOW", "CLS", "DOT", "PEN", "LINE", "NUM", "HEAD", "STICK", "RAND", "PUT", "GET", "WAIT" };
static const uint8_t OP_KIND[32] PROGMEM = { 0, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 3, 2, 2, 0, 5, 4, 0, 0, 2, 2, 2, 3, 2, 2, 3, 3, 3, 6 };
static uint8_t ed[RK_FILE_BYTES];                    // the program being edited: ed[0] steps, then two bytes a step
static uint8_t screen = SCR_DESK, zoom = 0, repaint = 1, frame_no = 0, file_sel = 0, task_sel = 0, ed_file = 0, ed_sel = 0, ed_top = 0, rule_sel = 0;
static uint8_t win_on = 0, win_big = 0, win_x = 0, win_y = 0, ptr_on = 0, ptr_x = 0, ptr_y = 0, books_open = 0;

// ---------------------------------------------------------------- a window: where a place's dots land
static void rect_clear(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  uint8_t y1 = (uint8_t)(y + h - 1);
  for (uint8_t page = y >> 3; page <= (y1 >> 3); page++) {
    uint8_t m = 0xFF;
    if (page == (y >> 3)) m &= (uint8_t)(0xFF << (y & 7));
    if (page == (y1 >> 3)) m &= (uint8_t)(0xFF >> (7 - (y1 & 7)));
    uint8_t *q = &pc_frame[(uint16_t)page * 128 + x]; m = (uint8_t)~m;
    for (uint8_t i = 0; i < w; i++) *q++ &= m;
  }
}
static void win_set(uint8_t s) {                     // whose window the next dots go to
  win_big = 0; win_on = 0;
  if (screen != SCR_DESK) return;
  if (zoom) { if (zoom - 1 == s) { win_on = 1; win_big = 1; win_x = 2; win_y = 9; } return; }
  win_on = 1; win_x = (uint8_t)((s & 1) * 64 + 1); win_y = (uint8_t)((s >> 1) * 29 + 8);
}
static void win_dot(int16_t x, int16_t y) {
  if (!win_on || x < 0 || x > 61 || y < 0 || y > 25) return;
  if (win_big) { uint8_t X = (uint8_t)(win_x + 2 * x), Y = (uint8_t)(win_y + 2 * y); PC_PLOT(X, Y); PC_PLOT(X + 1, Y); PC_PLOT(X, Y + 1); PC_PLOT(X + 1, Y + 1); }
  else PC_PLOT(win_x + x, win_y + y);
}
static int16_t win_put(int16_t x, int16_t y, char ch) {
  uint16_t g = RD16(&GLYPH[glyph_of(ch)]);
  for (uint8_t r = 0; r < 5; r++) for (uint8_t c = 0; c < 3; c++) if (g & (0x4000 >> (r * 3 + c))) win_dot(x + c, y + r);
  return x + 4;
}
static int16_t win_count(int16_t x, int16_t y, int32_t v) {
  char b[11]; uint8_t n = 0; uint32_t u = (uint32_t)(v < 0 ? -v : v);
  if (v < 0) x = win_put(x, y, '-');
  do { b[n++] = (char)('0' + u % 10); u /= 10; } while (u);
  while (n) x = win_put(x, y, b[--n]);
  return x;
}

#ifdef STACKCHECK                                    // bench only: paint free RAM at reset, count what the stack never reached

static uint16_t stack_free = 0xFFFF;
static void stack_paint(void) __attribute__((naked, used, section(".init3")));
static void stack_paint(void) { uint8_t *p = (uint8_t *)&__bss_end; while (p < (uint8_t *)SP - 4) *p++ = 0xC5; }
static void stack_note(void) { uint8_t *p = (uint8_t *)&__bss_end; uint16_t n = 0; while (p < (uint8_t *)0x8FF && *p == 0xC5) { p++; n++; } stack_free = n; num(100, 57, n); }
#endif
#ifdef GHOST                                         // bench only: a scripted hand that visits every screen (it never saves, spoils or installs)
typedef struct { uint16_t at; uint8_t x, y, hold; } GHand;
static const GHand GH[] PROGMEM = { {40, 30, 20, 1}, {70, 30, 20, 1}, {100, 114, 3, 1}, {130, 20, 42, 1}, {150, 107, 40, 1}, {170, 107, 54, 1},
  {200, 10, 34, 1}, {215, 73, 14, 40}, {270, 10, 26, 1}, {285, 109, 14, 30}, {330, 4, 3, 1}, {350, 4, 3, 1}, {380, 85, 3, 1}, {400, 98, 27, 1},
  {430, 111, 14, 20}, {460, 111, 58, 25}, {500, 79, 14, 20}, {535, 79, 47, 1}, {550, 111, 47, 1}, {565, 4, 3, 1}, {580, 4, 3, 1}, {600, 85, 3, 1}, {620, 4, 3, 1},
  {650, 114, 3, 1}, {680, 107, 54, 1}, {700, 90, 27, 1}, {720, 90, 40, 1}, {740, 4, 3, 1}, {760, 4, 3, 1} };
static uint16_t g_frame; static uint8_t g_was;
static void ghost_input(void) {
  g_frame++; uint8_t down = 0;
  for (uint8_t i = 0; i < sizeof GH / sizeof GH[0]; i++) { uint16_t at = RD16(&GH[i].at); uint8_t h = RD8(&GH[i].hold);
    if (g_frame >= at - 8 && g_frame < at + h) { rk_px = (int16_t)(RD8(&GH[i].x) * 4); rk_py = (int16_t)(RD8(&GH[i].y) * 4); if (g_frame >= at) down = 1; } }
  rk_clicked = down && !g_was; rk_held = down; g_was = down; }
#endif
// ---------------------------------------------------------------- the hooks: how a step reaches this board
#ifdef OPCOST
static uint8_t BCODE[98];
static uint8_t k_code(Proc *p, uint8_t i) { (void)p; return BCODE[i]; }
#else
static uint8_t k_code(Proc *p, uint8_t i) { return RK_EE_RD(p->base + 1 + i); }       // programs run straight out of lasting memory
#endif
static void k_enter(uint8_t s) { win_set(s); }
static void k_cls(void) { if (!win_on) return; if (win_big) rect_clear(2, 9, 124, 52); else rect_clear(win_x, win_y, 62, 26); }
static void k_dot(int32_t x, int32_t y) { if (x < 0 || x > 61 || y < 0 || y > 25) return; win_dot((int16_t)x, (int16_t)y); }
static void k_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {     // the carry rule, as in pc_line, landing its dots in the window
  if (!win_on || x0 < -99 || x0 > 160 || x1 < -99 || x1 > 160 || y0 < -99 || y0 > 125 || y1 < -99 || y1 > 125) return;
  int16_t X = (int16_t)x0, Y = (int16_t)y0, dx = (int16_t)(x1 - x0), dy = (int16_t)(y1 - y0); int8_t sx = pc_way(dx), sy = pc_way(dy);
  uint16_t ax = (uint16_t)(dx < 0 ? -dx : dx), ay = (uint16_t)(dy < 0 ? -dy : dy);
  uint8_t steep = ay > ax; uint16_t lng = steep ? ay : ax, sht = steep ? ax : ay, acc = lng >> 1;
  for (uint16_t k = 0; k <= lng; k++) {
    win_dot(X, Y);
    if (steep) Y += sy; else X += sx;
    acc += sht; if (acc >= lng) { acc -= lng; if (steep) X += sx; else Y += sy; }
  }
}
static void k_num(const Rd *a, uint8_t at) { int16_t y = (int16_t)((at >> 4) * 6); int16_t x = win_count((int16_t)((at & 15) * 4), y, a->v); if (a->d != 1) { x = win_put(x, y, '/'); win_count(x, y, a->d); } }
static void k_stick(Rd *a, Rd *b) { a->v = rk_sx; a->r = 0; a->d = 1; b->v = rk_sy; b->r = 0; b->d = 1; }
static uint8_t k_btn(void) { return rk_held; }
static int32_t k_rand(int32_t n) { return pc_scatter(0, (int16_t)n); }

#ifdef OPCOST                                        // bench only: every op's cost on this chip, measured by Timer1 around single steps of random programs
static volatile uint8_t bench_last_op; static volatile uint16_t bench_last_n, bench_last_k; static uint16_t OPMAX[32], OPN[32]; static uint32_t brs = 12345;
static uint32_t brnd(void) { brs ^= brs << 13; brs ^= brs >> 17; brs ^= brs << 5; return brs; }
static void bench_run(void) {
  TCCR1A = 0; TCCR1B = 1;                            // Timer1 at the chip's own clock
  screen = SCR_DESK; zoom = 0;
  for (uint16_t n = 0; n < BENCH_N; n++) {
    uint8_t ns = (uint8_t)(4 + brnd() % 30); BCODE[0] = ns;
    for (uint8_t i = 0; i < ns; i++) { uint8_t op = (uint8_t)(brnd() % 32); if (op == 17 || op == 27) op = 7;
      BCODE[1 + 2 * i] = (uint8_t)(op << 3 | (brnd() & 7)); uint8_t b1 = (uint8_t)brnd(); if (op == 18 || op == 19) b1 = (uint8_t)(brnd() % ns); if (op == 1 || op == 2 || op == 3) b1 = (uint8_t)((int8_t)(brnd() % 13) - 6); BCODE[2 + 2 * i] = b1; }
    Proc *p = &proc[0]; for (uint8_t i = 0; i < sizeof(Proc); i++) ((uint8_t *)p)[i] = 0; p->n = ns; for (uint8_t i = 0; i < 8; i++) p->reg[i].d = 1; p->state = ST_RUN;
    win_set((uint8_t)(n & 3));
    for (uint16_t k = 0; k < 600 && p->state == ST_RUN; k++) { uint8_t op = (uint8_t)(BCODE[1 + 2 * p->pc] >> 3); if (p->pc >= p->n) op = 0;
      bench_last_op = op; bench_last_n = n; bench_last_k = k; uint16_t t0 = TCNT1; vm_step(p); uint16_t dt = (uint16_t)(TCNT1 - t0); p->sleep = 0;
      if (dt > OPMAX[op]) OPMAX[op] = dt; if (OPN[op] < 65535) OPN[op]++; }
    if ((n & 63) == 0) oled_clear(); }
}
#endif
// ---------------------------------------------------------------- places and files
static void proc_start(uint8_t s, uint8_t f) {       // load file f into place s and set it going from its first step
  Proc *p = &proc[s]; uint8_t st = rk_file_state(f);
  k_release(s);
  for (uint8_t i = 0; i < sizeof(Proc); i++) ((uint8_t *)p)[i] = 0;
  p->file = f;
  if (st == RK_EMPTY || st == RK_LOST) return;       // nothing there to run: the place stays free
  p->base = rk_copy_at(f, st == RK_BAD_A ? 1 : 0); p->n = RK_EE_RD(p->base + 1); if (p->n > MAX_STEPS) p->n = MAX_STEPS;
  for (uint8_t i = 0; i < 8; i++) p->reg[i].d = 1;
  p->state = ST_RUN;
}
static void spawn(uint8_t f) {                       // run file f: in the place already running it, else a free place, else a finished one
  uint8_t s = NPL;
  for (uint8_t i = 0; i < NPL; i++) if (proc[i].state != ST_FREE && proc[i].file == f) { s = i; break; }
  if (s == NPL) for (uint8_t i = 0; i < NPL; i++) if (proc[i].state == ST_FREE) { s = i; break; }
  if (s == NPL) for (uint8_t i = 0; i < NPL; i++) if (proc[i].state >= ST_END) { s = i; break; }
  if (s < NPL) { proc_start(s, f); task_sel = s; }
}
static void restart_file(uint8_t f) { for (uint8_t i = 0; i < NPL; i++) if (proc[i].state != ST_FREE && proc[i].file == f) proc_start(i, f); }
static void install(uint8_t f) { for (uint8_t i = 0; i < RK_FILE_BYTES; i++) ed[i] = RD8(&DEMO[f][i]); rk_file_write(f, ed); }

// ---------------------------------------------------------------- the kernel's rules, kept in the 24 bytes OS 2 left unused (1000 to 1023)
#define RULES_AT 1002                                // two copies of: mark 0xAA, 9 bytes, a sum (1002 to 1023)
#define RULES_N 9
static void rules_pack(uint8_t *d) { d[0] = (uint8_t)rules.C; d[1] = (uint8_t)rules.THETA; d[2] = (uint8_t)(rules.THETA >> 8); d[3] = (uint8_t)rules.G0; d[4] = (uint8_t)(rules.G0 >> 8);
  d[5] = (uint8_t)rules.S; d[6] = (uint8_t)(rules.S >> 8); d[7] = (uint8_t)(rules.KAPPA | (rules.VENT << 4)); d[8] = rules.DRAG; }
static void rules_save(void) { uint8_t d[RULES_N]; rules_pack(d);
  for (uint8_t c = 0; c < 2; c++) { uint16_t at = (uint16_t)(RULES_AT + (RULES_N + 2) * c); uint8_t s = 0x5A; RK_EE_WR(at, 0xAA); for (uint8_t i = 0; i < RULES_N; i++) { RK_EE_WR(at + 1 + i, d[i]); s = rk_sum_step(s, d[i]); } RK_EE_WR(at + 1 + RULES_N, s); } }
static uint8_t rules_load(void) {                    // 1 if a sound copy was found; the field is then grown from the loaded G0
  for (uint8_t c = 0; c < 2; c++) { uint16_t at = (uint16_t)(RULES_AT + (RULES_N + 2) * c); if (RK_EE_RD(at) != 0xAA) continue;
    uint8_t d[RULES_N], s = 0x5A; for (uint8_t i = 0; i < RULES_N; i++) { d[i] = RK_EE_RD(at + 1 + i); s = rk_sum_step(s, d[i]); } if (s != RK_EE_RD(at + 1 + RULES_N)) continue;
    rules.C = d[0]; rules.THETA = (uint16_t)(d[1] | (d[2] << 8)); rules.G0 = (uint16_t)(d[3] | (d[4] << 8)); rules.S = (uint16_t)(d[5] | (d[6] << 8)); rules.KAPPA = (uint8_t)(d[7] & 15); rules.VENT = (uint8_t)(d[7] >> 4); rules.DRAG = d[8];
    field = rules.G0; return 1; }
  return 0; }

// ---------------------------------------------------------------- the pointer, and small helpers for the screens
static void pointer_flip(void) {                     // drawn by turning dots over, so drawing it twice puts the picture back
  for (uint8_t r = 0; r < 7; r++) { uint8_t bits = RD8(&RK_ARROW[r]);
    for (uint8_t c = 0; c < 5; c++) if ((bits >> c) & 1) { uint8_t x = (uint8_t)(ptr_x + c), y = (uint8_t)(ptr_y + r); if (x < 128 && y < 64) pc_frame[x + (uint16_t)(y >> 3) * 128] ^= (uint8_t)(1 << (y & 7)); } }
}
static uint8_t held_for = 0, last_screen = 0;
static uint8_t held_button(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char *label) {   // a button that repeats once it has been held a while
  uint8_t hit = rk_button(x, y, w, h, label);
  return hit || (held_for > 12 && (frame_no % 3 == 0) && rk_over(x, y, w, h));
}
static uint8_t signed_num(uint8_t x, uint8_t y, int16_t v) { if (v < 0) { x = put(x, y, '-'); v = (int16_t)-v; } return num(x, y, (uint32_t)v); }
static uint8_t back_button(void) { if (rk_button(0, 0, 9, 7, P("X"))) { screen = SCR_DESK; repaint = 1; return 1; } return 0; }
static uint8_t step_text(uint8_t x, uint8_t y, uint8_t i, uint8_t b0, uint8_t b1) {
  if (i < 10) x = put(x, y, '0'); x = num(x, y, i); x += 2;
  uint8_t op = b0 >> 3, kind = RD8(&OP_KIND[op]); char ch;
  for (uint8_t k = 0; k < 6 && (ch = (char)RD8(&OP_NAME[op][k])); k++) x = put(x, y, ch);
  x += 3;
  if (kind == 1 || kind == 2 || kind == 3 || kind == 5) x = (uint8_t)(put(x, y, (char)('A' + (b0 & 7))) + 3);
  if (kind == 2) x = put(x, y, (char)('A' + (b1 & 7)));
  if (kind == 3) x = signed_num(x, y, (int8_t)b1);
  if (kind == 4 || kind == 5 || kind == 6) x = num(x, y, b1);
  return x;
}
static const char *state_word(uint8_t st) { return st == ST_RUN ? P("RUN") : st == ST_STOP ? P("STOP") : st == ST_END ? P("END") : st == ST_THETA ? P("THETA") : st == ST_WIDE ? P("WIDE") : st == ST_LOST ? P("LOST") : P("FREE"); }

// ---------------------------------------------------------------- one latch of every place, then what a refusal says
static uint8_t said[NPL];                            // a refusal is written into its window once, not every latch
static void turns(void) {
  k_latch();
  for (uint8_t s = 0; s < NPL; s++) { Proc *p = &proc[s];
    if (p->state >= ST_THETA && !said[s]) { win_set(s); int16_t x = 0; const char *w = state_word(p->state); char ch; while ((ch = (char)RD8(w++))) x = win_put(x, 20, ch); win_count(x + 4, 20, p->pc); said[s] = screen == SCR_DESK; }
    if (p->state < ST_THETA) said[s] = 0; }
  books_open = !k_books();
}

// ---------------------------------------------------------------- the screens
static void desk(void) {
  if (repaint) {                                     // the whole desk again: frames, and every finished program run again to remake its picture
    oled_clear(); repaint = 0;
    if (zoom) rk_box(1, 8, 126, 54); else for (uint8_t s = 0; s < 4; s++) rk_box((uint8_t)((s & 1) * 64), (uint8_t)((s >> 1) * 29 + 7), 64, 28);
    for (uint8_t s = 0; s < NPL; s++) { said[s] = 0; if (proc[s].state >= ST_END) proc_start(s, proc[s].file); }
  }
  for (uint8_t i = 0; i < 128; i++) pc_frame[i] &= 0x80;                 // the bar is drawn afresh every frame; the windows are not
  turns();
  uint8_t x = say(1, 1, P("OS4")); x = num((uint8_t)(x + 3), 1, k_beats); x = say((uint8_t)(x + 3), 1, P("F")); num((uint8_t)(x + 1), 1, field);
  if (rk_button(72, 0, 27, 7, P("FILES"))) { screen = SCR_FILES; return; }
  if (rk_button(100, 0, 28, 7, P("TASKS"))) { screen = SCR_TASKS; return; }
  if (rk_clicked && (rk_py >> 2) >= 8) {
    if (zoom) { zoom = 0; repaint = 1; }
    else { uint8_t s = (uint8_t)(((rk_px >> 2) >= 64) + 2 * ((rk_py >> 2) >= 36)); if (proc[s].state != ST_FREE) { zoom = (uint8_t)(s + 1); repaint = 1; } }
  }
}
static void files(void) {
  turns(); oled_clear();
  if (back_button()) return;
  uint8_t x = say(14, 1, P("FILES   MENDED ")); num(x, 1, rk_mended);
  for (uint8_t f = 0; f < RK_FILES; f++) {
    uint8_t y = (uint8_t)(10 + 10 * f), st = rk_file_state(f);
    x = put(3, y, (char)('A' + f)); x += 4;
    x = say(x, y, st == RK_EMPTY ? P("EMPTY") : st == RK_SOUND ? P("SOUND") : st == RK_BAD_A ? P("BAD A") : st == RK_BAD_B ? P("BAD B") : P("LOST")); x += 4;
    if (st != RK_EMPTY && st != RK_LOST) num(x, y, RK_EE_RD(rk_copy_at(f, st == RK_BAD_A ? 1 : 0) + 1));
    if (f == file_sel) rk_box(0, (uint8_t)(y - 2), 62, 9);
    if (rk_clicked && rk_over(0, (uint8_t)(y - 2), 62, 9)) file_sel = f;
  }
  if (rk_button(70, 9, 56, 11, P("RUN"))) { spawn(file_sel); screen = SCR_DESK; repaint = 1; zoom = 0; return; }
  if (rk_button(70, 22, 56, 11, P("EDIT"))) { ed_file = file_sel; if (!rk_file_read(ed_file, ed)) ed[0] = 0; if (ed[0] > MAX_STEPS) ed[0] = MAX_STEPS; ed_sel = 0; ed_top = 0; screen = SCR_EDIT; return; }
  if (rk_button(70, 35, 56, 11, P("DEMO"))) { install(file_sel); restart_file(file_sel); }
  if (rk_button(70, 48, 56, 11, P("ZAP"))) { uint16_t at = (uint16_t)(rk_copy_at(file_sel, 1) + 5); RK_EE_WR(at, (uint8_t)(RK_EE_RD(at) ^ 0xFF)); }
}
static void tasks(void) {
  turns(); oled_clear();
  if (back_button()) return;
  uint8_t x = say(12, 1, P("TASKS F")); x = num(x, 1, field); x = put(x, 1, '/'); num(x, 1, rules.G0);
  for (uint8_t s = 0; s < NPL; s++) {
    Proc *p = &proc[s]; uint8_t y = (uint8_t)(10 + 10 * s);
    x = num(3, y, (uint32_t)(s + 1)); x += 3;
    if (p->state == ST_FREE) say(x, y, P("FREE"));
    else { x = put(x, y, (char)('A' + p->file)); x += 3; x = say(x, y, state_word(p->state)); x = (uint8_t)(x < 46 ? 46 : x + 2);
      x = say(x, y, P("L")); x = num(x, y, flow[s].L); x += 3; num(x, y, flow[s].took); }
    if (s == task_sel) rk_box(0, (uint8_t)(y - 2), 84, 9);
    if (rk_clicked && rk_over(0, (uint8_t)(y - 2), 84, 9)) task_sel = s;
  }
  Proc *p = &proc[task_sel];
  if (rk_button(87, 9, 40, 11, p->state == ST_STOP ? P("GO") : P("STOP"))) { if (p->state == ST_RUN) p->state = ST_STOP; else if (p->state == ST_STOP) p->state = ST_RUN; }
  if (rk_button(87, 22, 40, 11, P("KILL"))) { p->state = ST_FREE; k_release(task_sel); }
  if (rk_button(87, 35, 40, 11, P("VENT"))) k_vent(task_sel, 16);
  if (rk_button(87, 48, 40, 11, P("RULES"))) { screen = SCR_RULES; return; }
  x = say(3, 52, P("RECON ")); num(x, 52, k_recon);
}
static const char RULE_NAME[7][6] PROGMEM = { "C", "THETA", "G0", "S", "KAPPA", "VENT", "DRAG" };
static void rules_screen(void) {                     // the kernel's alphabet, by hand: each reading with - and +
  turns(); oled_clear();
  if (rk_button(0, 0, 9, 7, P("X"))) { screen = SCR_TASKS; return; }
  uint8_t x = say(12, 1, P("RULES RECON ")); num(x, 1, k_recon);
  for (uint8_t i = 0; i < 7; i++) { uint8_t y = (uint8_t)(9 + 8 * i); char ch; x = 3;
    for (uint8_t k = 0; k < 6 && (ch = (char)RD8(&RULE_NAME[i][k])); k++) x = put(x, y, ch);
    uint16_t v = i == 0 ? rules.C : i == 1 ? rules.THETA : i == 2 ? rules.G0 : i == 3 ? rules.S : i == 4 ? rules.KAPPA : i == 5 ? rules.VENT : rules.DRAG;
    num(28, y, v); if (i == rule_sel) rk_box(0, (uint8_t)(y - 1), 54, 7);
    if (rk_clicked && rk_over(0, (uint8_t)(y - 1), 54, 7)) rule_sel = i; }
  int8_t d = (int8_t)(held_button(92, 9, 35, 11, P("+")) - held_button(56, 9, 35, 11, P("-")));
  if (d) { uint8_t i = rule_sel;                      // each reading has its own step and its own floor and ceiling; the field takes any change to G0
    if (i == 0) { int16_t v = (int16_t)(rules.C + 4 * d); if (v >= 4 && v <= 252) rules.C = (uint16_t)v; }
    if (i == 1) { int32_t v = (int32_t)rules.THETA + 100 * d; if (v >= 100 && v <= 30000) rules.THETA = (uint16_t)v; }
    if (i == 2) { int32_t v = (int32_t)rules.G0 + 500 * d; if (v >= 500 && v <= 60000) k_set_g0((uint16_t)v); }
    if (i == 3) { int16_t v = (int16_t)(rules.S + 50 * d); if (v >= 100 && v <= 3000) rules.S = (uint16_t)v; }
    if (i == 4) { int8_t v = (int8_t)(rules.KAPPA + d); if (v >= 0 && v <= 15) rules.KAPPA = (uint8_t)v; }
    if (i == 5) { int8_t v = (int8_t)(rules.VENT + d); if (v >= 0 && v <= 15) rules.VENT = (uint8_t)v; }
    if (i == 6) { int8_t v = (int8_t)(rules.DRAG + d); if (v >= 0 && v <= 15) rules.DRAG = (uint8_t)v; } }
  if (rk_button(56, 22, 71, 11, P("SAVE"))) rules_save();
  if (rk_button(56, 35, 71, 11, P("FIRST"))) { Rules f; k_rules_first(&f); uint16_t g = f.G0; f.G0 = rules.G0; rules = f; k_set_g0(g); }
  x = say(56, 52, P("BOOKS ")); say(x, 52, books_open ? P("OPEN") : P("SHUT"));
}
static void ed_shift(uint8_t from, int8_t by) {
  for (uint8_t i = 0; i < ed[0]; i++) { uint8_t kind = RD8(&OP_KIND[ed[1 + 2 * i] >> 3]); if ((kind == 4 || kind == 5) && ed[2 + 2 * i] > from) ed[2 + 2 * i] = (uint8_t)(ed[2 + 2 * i] + by); }
}
static void editor(void) {
  turns(); oled_clear();
  if (back_button()) return;
  uint8_t x = say(12, 1, P("EDIT ")); x = put(x, 1, (char)('A' + ed_file)); x += 4; x = num(x, 1, ed[0]); x = say(x, 1, P("/48"));
  if (rk_button(74, 0, 25, 7, P("SAVE"))) { rk_file_write(ed_file, ed); restart_file(ed_file); }
  if (rk_button(102, 0, 26, 7, P("RUN"))) { rk_file_write(ed_file, ed); spawn(ed_file); screen = SCR_DESK; repaint = 1; zoom = 0; return; }
  if (ed_sel < ed_top) ed_top = ed_sel; if (ed_sel > ed_top + 8) ed_top = (uint8_t)(ed_sel - 8);
  for (uint8_t row = 0; row < 9; row++) {
    uint8_t i = (uint8_t)(ed_top + row), y = (uint8_t)(9 + 6 * row); if (i >= ed[0]) break;
    step_text(3, y, i, ed[1 + 2 * i], ed[2 + 2 * i]);
    if (i == ed_sel) { PC_PLOT(0, y + 1); PC_PLOT(0, y + 2); PC_PLOT(0, y + 3); PC_PLOT(1, y + 2); }
    if (rk_clicked && rk_over(0, y, 62, 6)) ed_sel = i;
  }
  uint8_t *b0 = &ed[1 + 2 * ed_sel], *b1 = &ed[2 + 2 * ed_sel], has = ed[0] > 0, kind = has ? RD8(&OP_KIND[*b0 >> 3]) : 0;
  int8_t d;
  d = (int8_t)(held_button(96, 9, 31, 10, P("OP+")) - held_button(64, 9, 31, 10, P("OP-")));
  if (d && has) *b0 = (uint8_t)((((*b0 >> 3) + d) & 31) << 3 | (*b0 & 7));
  d = (int8_t)(held_button(96, 20, 31, 10, P("A+")) - held_button(64, 20, 31, 10, P("A-")));
  if (d && has) *b0 = (uint8_t)((*b0 & 0xF8) | ((*b0 + d) & 7));
  d = (int8_t)(held_button(96, 31, 31, 10, P("B+")) - held_button(64, 31, 31, 10, P("B-")));
  if (d && has) { if (kind == 2) *b1 = (uint8_t)((*b1 + d) & 7); else if (kind == 4 || kind == 5) { if (!(d < 0 && *b1 == 0) && !(d > 0 && *b1 >= MAX_STEPS - 1)) *b1 = (uint8_t)(*b1 + d); } else *b1 = (uint8_t)(*b1 + d); }
  if (rk_button(64, 42, 31, 10, P("INS")) && ed[0] < MAX_STEPS) {
    uint8_t at = has ? (uint8_t)(ed_sel + 1) : 0;
    ed_shift(has ? ed_sel : 0, 1);
    for (uint8_t i = (uint8_t)(2 * ed[0]); i >= 2 * at + 1; i--) ed[i + 2] = ed[i];
    ed[1 + 2 * at] = (uint8_t)(OP_SHOW << 3); ed[2 + 2 * at] = 0; ed[0]++; ed_sel = at;
  }
  if (rk_button(96, 42, 31, 10, P("DEL")) && has) {
    for (uint8_t i = (uint8_t)(1 + 2 * ed_sel); i + 2 < RK_FILE_BYTES; i++) ed[i] = ed[i + 2];
    ed[0]--; ed_shift(ed_sel, -1); if (ed_sel && ed_sel >= ed[0]) ed_sel--;
  }
  if (held_button(64, 53, 31, 10, P("UP")) && ed_sel) ed_sel--;
  if (held_button(96, 53, 31, 10, P("DN")) && ed_sel + 1 < ed[0]) ed_sel++;
}
void setup() {
#ifdef OPCOST
  bench_run(); for (;;) oled_flush();
#endif
  oled_begin(); pinMode(RK_PIN_SW, INPUT_PULLUP);
  k_begin(); rules_load();
  uint8_t any = 0; for (uint8_t f = 0; f < RK_FILES; f++) if (rk_file_state(f) != RK_EMPTY) any = 1;
  if (!any) for (uint8_t f = 0; f < RK_FILES; f++) install(f);            // a new chip: the five factory programs go into the files
  for (uint8_t s = 0; s < NPL; s++) { cell[s].d = 1; proc_start(s, s); }  // and the first four are set going
}
void loop() {
  frame_no++;
  if (ptr_on) { pointer_flip(); ptr_on = 0; }        // take the pointer off the picture before anything else touches it
  rk_input();
#ifdef GHOST
  ghost_input();
#endif
  if (screen != last_screen) { last_screen = screen; held_for = 0; }
  if (rk_clicked) held_for = 1; else if (!rk_held) held_for = 0; else if (held_for && held_for < 255) held_for++;
  if (frame_no % 12 == 0) rk_keeper_turn();          // the keeper looks at one file and mends it from its good copy
  if (screen == SCR_DESK) desk(); else if (screen == SCR_FILES) files(); else if (screen == SCR_TASKS) tasks(); else if (screen == SCR_RULES) rules_screen(); else editor();
  if (books_open) for (uint8_t c = 0; c < 128; c++) pc_frame[c] ^= 0x7F;   // a leak is shown at once: the bar inverts (the windows below it are left alone)
  ptr_x = (uint8_t)(rk_px >> 2); ptr_y = (uint8_t)(rk_py >> 2); pointer_flip(); ptr_on = 1;
#ifdef STACKCHECK
  stack_note();
#endif
  rk_show(40);
}
