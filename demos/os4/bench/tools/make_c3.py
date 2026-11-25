#!/usr/bin/env python3
"""make_c3.py: RULE OS 4 for the ESP32-C3, made from OS 3 by named edits (every edit must match exactly once, or this stops).
Everything not named here is OS 3's, line for line: the panel, FAREY's queue, the arena, the files, the apps, the ghost."""
import os, sys
H = os.path.dirname(os.path.abspath(__file__)) + '/..'
s = open(H + '/base/os3.ino').read()
ker = open(H + '/kernel.h').read().split('\n')
cut = next(i for i, l in enumerate(ker) if l.startswith('typedef struct { uint16_t C, THETA')) + 1
KTYPES, KBODY = '\n'.join(ker[:cut]), '\n'.join(ker[cut:])
def ed(old, new, count=1):
    global s
    n = s.count(old)
    if n != count: sys.exit('edit matched %d times (wanted %d): %r' % (n, count, old[:90]))
    s = s.replace(old, new)
def cut_between(a, b, new):                 # replace from the line starting with a up to (not including) the line starting with b
    global s
    i = s.index(a); j = s.index(b, i); s = s[:i] + new + s[j:]

ed('// RULE OS 3: an operating system for the ESP32-C3 + 2.0" ST7789 (320 x 240 across) whose memory says "when", not "no".',
   '''// RULE OS 4 for the ESP32-C3 + 2.0" ST7789 (320 x 240 across). Made from RULE OS 3 by tools/make_c3.py; what is new:
//  - THE FLUX. No budgets. Each place carries a loaded history L; every step moves its weight from the kernel's field into L; the place
//    whose next step is due soonest runs (min), and a step pushes its due mark on by weight x (C + L + drag) (+). A place that gives the
//    machine back vents a share of L; one that ends or is refused vents all of it; one past THETA reconfigures (keeps KAPPA/16, vents
//    the rest, goes to the front). Drag is the L of every other place sharing a cell with it. Books every latch: field + every L = G0.
//  - THE SAME KERNEL AS THE UNO'S OS 4 (one kernel.h, joined into both sketches): the same files give the same readings, slot for slot,
//    and the same flux, latch for latch (checked: the kernel on this chip's instruction set in Espressif's emulator == the desk).
//  - RULES, an app: the kernel's own readings by hand, kept in flash with the files. MAKE IT YOURS starts there.
// What follows is OS 3's own description: still true, except that "a budget of steps a latch" is now the flux above.
// RULE OS 3: an operating system for the ESP32-C3 + 2.0" ST7789 (320 x 240 across) whose memory says "when", not "no".''')
# ---- types: the kernel's in place of OS 3's Rd and Proc
ed('typedef struct { int32_t v, r, d; } Rd;                                   // a reading: tops over one shared bottom\n'
   'typedef struct { Rd reg[8]; uint8_t pc, n, state, file, budget, used, sleep, cut, win; int16_t pen_x, pen_y; uint32_t steps; uint8_t code[98]; } Proc;\n',
   '#define KTOP32\n#define NPL 4\n#define PROC_EXTRA uint8_t win; uint32_t steps; uint8_t code[98];\n' + KTYPES + '\n')
# ---- the readings and the machine: the kernel's
cut_between('// ---------------------------------------------------------------- readings: exact fractions over one shared bottom, 32-bit tops',
            'static const char OP_NAME[32][6]', KBODY + '\n')
ed('#define NP 4\n', '#define NP NPL\n')
ed('static Proc proc[NP];\nstatic Rd cell[4];                                         // four shared cells: PUT and GET\n', '')
cut_between('static uint8_t vm_step(Proc *p, int s) {', '// ---------------------------------------------------------------- windows', '''static uint8_t rsum(const uint8_t *b) { uint8_t s = 0x5A; for (int k = 0; k < 12; k++) s = (uint8_t)(((s << 1) | (s >> 7)) ^ b[k]); return s; }
static void rules_save(void) { for (int c = 0; c < 2; c++) { RULEDISK[c][0] = 0xAA; memcpy(RULEDISK[c] + 1, &rules, 12); RULEDISK[c][13] = rsum(RULEDISK[c] + 1); } disk_save(); }
static int rules_load(void) { for (int c = 0; c < 2; c++) if (RULEDISK[c][0] == 0xAA && RULEDISK[c][13] == rsum(RULEDISK[c] + 1)) { memcpy(&rules, RULEDISK[c] + 1, 12); field = rules.G0; return 1; } return 0; }
static uint8_t pw_ink, said[NP];
// ---- the hooks: how a step reaches this board
static uint8_t k_code(Proc *p, uint8_t i) { return p->code[i]; }
static void k_enter(uint8_t s) { pw_p = pic_of(proc[s].win); pw_ink = (uint8_t)(C_INK + s); }
static void k_cls(void) { if (pw_p) memset(pw_p, C_WIN, PW * PH); }
static void k_dot(int32_t x, int32_t y) { pw_dot(x, y, pw_ink); }
static void k_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1) { pw_line(x0, y0, x1, y1, pw_ink); }
static void k_num(const Rd *a, uint8_t at) { pw_num((at & 15) * 8, (at >> 4) * 12, a, pw_ink); }
static void k_stick(Rd *a, Rd *b) { a->v = pad.jx / 32; a->r = 0; a->d = 1; b->v = pad.jy / 32; b->r = 0; b->d = 1; }
static uint8_t k_btn(void) { return (uint8_t)down; }
static int32_t k_rand(int32_t n) { return (int32_t)(scatter() % (uint32_t)n); }
static void run_places(void) {                             // one latch of the flux for every place, then what a refusal says
  for (int s = 0; s < NP; s++) { Proc *p = &proc[s]; if (p->state == ST_WAIT && pic_of(p->win)) { p->state = ST_RUN; memset(pic_of(p->win), C_WIN, PW * PH); } }   // the promise came in
  k_latch(); pw_p = 0; steps_all = k_steps;
  for (int s = 0; s < NP; s++) { Proc *p = &proc[s]; p->steps += flow[s].took;
    if (p->state >= ST_THETA && !said[s] && pic_of(p->win)) { uint8_t *q = pic_of(p->win); on(q, PW, PH); fill(0, PH - 9, PW, 9, C_WIN);
      int x = text(2, PH - 8, p->state == ST_THETA ? "THETA" : p->state == ST_WIDE ? "WIDE" : "LOST", C_RED); number(x + 6, PH - 8, p->pc, C_RED); said[s] = 1; }
    if (p->state < ST_THETA) said[s] = 0; } }

''')
# ---- places: budgets gone; a place's history goes back to the field when it is killed or reloaded
ed('static void place_kill(int s) { Proc *p = &proc[s]; uint8_t keep = p->budget; memset(p, 0, sizeof *p); p->budget = keep ? keep : 64; }',
   'static void place_kill(int s) { Proc *p = &proc[s]; k_release((uint8_t)s); memset(p, 0, sizeof *p); }')
ed('Proc *p = &proc[s]; uint8_t keep = p->budget; memset(p, 0, sizeof *p); p->budget = keep; memcpy(p->code, b, FB_BYTES);',
   'Proc *p = &proc[s]; k_release((uint8_t)s); memset(p, 0, sizeof *p); memcpy(p->code, b, FB_BYTES);')
# ---- windows: a RULES app
ed('#define K_PLACE 5\n', '#define K_PLACE 5\n#define K_RULES 6\n')
ed('static const int16_t KW[6] = {0, 150, 164, 196, 182, PW}, KH[6] = {0, 118, 92, 92, 110, PH};\nstatic const char *KNAME[6] = {"", "FILES", "TASKS", "HEAP", "EDIT", ""};',
   'static const int16_t KW[7] = {0, 150, 164, 196, 182, PW, 150}, KH[7] = {0, 118, 92, 92, 110, PH, 86};\nstatic const char *KNAME[7] = {"", "FILES", "TASKS", "HEAP", "EDIT", "", "RULES"};')
# ---- a window the arena refuses says so (OS 3 refused it silently)
ed('static int cascade;', 'static int cascade;\nstatic int theta_kind, theta_left, want_rules;')
ed('static int theta_kind, theta_left;\nstatic int drag_w', 'static int drag_w')
ed('  if (got < 0) { n->used = 0; return -1; }                 // refused: no stretch can be cleared (counted)',
   '  if (got < 0) { n->used = 0; theta_kind = kind; theta_left = 90; return -1; }   // refused: no stretch can be cleared (counted, and said on the bar)')
# ---- TASKS: the flux readings in place of the budgets
ed('    int x = number(106, y + 2, p->used, C_TEXT); x = text(x, y + 2, "/", C_DIM); number(x, y + 2, p->budget, C_TEXT); }',
   '    int x = text(106, y + 2, "L", C_DIM); x = number(x, y + 2, flow[s].L, C_TEXT); number(x + 6, y + 2, flow[s].took, C_DIM); }')
ed('  if (button(82, y, 38, 11, "FAST", 0)) { if (p->budget < 128) p->budget = (uint8_t)(p->budget * 2); }\n'
   '  if (button(122, y, 38, 11, "SLOW", 0)) { if (p->budget > 1) p->budget = (uint8_t)(p->budget / 2); }\n'
   '  int x = text(2, 74, "STEPS ", C_DIM); x = number(x, 74, steps_all, C_TEXT); x = text(x + 6, 74, "CUT ", C_DIM); number(x, 74, p->cut, C_TEXT);',
   '  if (button(82, y, 38, 11, "VENT", 0)) k_vent((uint8_t)task_sel, 16);\n'
   '  if (button(122, y, 38, 11, "RULES", 0)) want_rules = 1;   // TASKS gives its room to RULES, between frames\n'
   '  int x = text(2, 74, "FIELD ", C_DIM); x = number(x, 74, field, C_TEXT); x = text(x, 74, "/", C_DIM); x = number(x, 74, rules.G0, C_DIM); x = text(x + 6, 74, "RECON ", C_DIM); number(x, 74, k_recon, C_TEXT);')
ed('static void app_heap(void) {', '''static int rule_sel = 3;
static void rules_save(void);
static void app_rules(void) {                              // the kernel's own readings, by hand: each with - and +; SAVE keeps them with the files
  fill(0, 0, s_w, s_h, C_WIN);
  static const char *RN[7] = {"C", "THETA", "G0", "S", "KAPPA", "VENT", "DRAG"};
  for (int i = 0; i < 7; i++) { int y = 2 + i * 11; uint32_t v = i == 0 ? rules.C : i == 1 ? rules.THETA : i == 2 ? rules.G0 : i == 3 ? rules.S : i == 4 ? rules.KAPPA : i == 5 ? rules.VENT : rules.DRAG;
    if (button(2, y, 34, 10, RN[i], i == rule_sel)) rule_sel = i; number(39, y + 2, v, C_TEXT); }
  int d = button(104, 2, 20, 12, "-", 0) ? -1 : button(126, 2, 20, 12, "+", 0) ? 1 : 0, i = rule_sel;
  if (d) { if (i == 0) { int v = rules.C + 4 * d; if (v >= 4 && v <= 252) rules.C = (uint16_t)v; }
    if (i == 1) { int v = rules.THETA + 100 * d; if (v >= 100 && v <= 30000) rules.THETA = (uint16_t)v; }
    if (i == 2) { int v = rules.G0 + 500 * d; if (v >= 500 && v <= 60000) k_set_g0((uint16_t)v); }
    if (i == 3) { int v = rules.S + 250 * d; if (v >= 250 && v <= 12000) rules.S = (uint16_t)v; }
    if (i == 4) { int v = rules.KAPPA + d; if (v >= 0 && v <= 15) rules.KAPPA = (uint8_t)v; }
    if (i == 5) { int v = rules.VENT + d; if (v >= 0 && v <= 15) rules.VENT = (uint8_t)v; }
    if (i == 6) { int v = rules.DRAG + d; if (v >= 0 && v <= 15) rules.DRAG = (uint8_t)v; } }
  if (button(74, 18, 72, 12, "SAVE", 0)) rules_save();
  if (button(74, 32, 72, 12, "FIRST", 0)) { Rules f; k_rules_first(&f); uint16_t g = f.G0; f.G0 = rules.G0; f.S = 3000; rules = f; k_set_g0(g); }
  int x = text(74, 50, "BOOKS ", C_DIM); text(x, 50, k_books() ? "SHUT" : "OPEN", k_books() ? C_GREEN : C_RED);
  x = text(74, 62, "RECON ", C_DIM); number(x, 62, k_recon, C_TEXT);
  x = text(74, 74, "FIELD ", C_DIM); number(x, 74, field, C_TEXT); }
static void app_heap(void) {''')
ed('else if (n->kind == K_HEAP) app_heap(); else if (n->kind == K_EDIT) app_edit();',
   'else if (n->kind == K_HEAP) app_heap(); else if (n->kind == K_EDIT) app_edit(); else if (n->kind == K_RULES) app_rules();')
# ---- the rules kept with the files (flash on the board)
ed('static void disk_save(void) {\n#ifdef ON_BOARD\n  for (int f = 0; f < NF; f++) for (int c = 0; c < 2; c++) for (int k = 0; k < COPY; k++) EEPROM.write((f * 2 + c) * COPY + k, DISK[f][c][k]);\n  EEPROM.commit();',
   '#define RULES_AT (NF * 2 * COPY)               // after the files: two copies of mark 0xAA, the 12 bytes of the rules, a sum\n'
   'static uint8_t RULEDISK[2][14];\n'
   'static void disk_save(void) {\n#ifdef ON_BOARD\n  for (int f = 0; f < NF; f++) for (int c = 0; c < 2; c++) for (int k = 0; k < COPY; k++) EEPROM.write((f * 2 + c) * COPY + k, DISK[f][c][k]);\n'
   '  for (int c = 0; c < 2; c++) for (int k = 0; k < 14; k++) EEPROM.write(RULES_AT + 14 * c + k, RULEDISK[c][k]);\n  EEPROM.commit();')
ed('  EEPROM.begin(NF * 2 * COPY);\n  for (int f = 0; f < NF; f++) for (int c = 0; c < 2; c++) for (int k = 0; k < COPY; k++) DISK[f][c][k] = EEPROM.read((f * 2 + c) * COPY + k);\n',
   '  EEPROM.begin(NF * 2 * COPY + 28);\n  for (int f = 0; f < NF; f++) for (int c = 0; c < 2; c++) for (int k = 0; k < COPY; k++) DISK[f][c][k] = EEPROM.read((f * 2 + c) * COPY + k);\n'
   '  for (int c = 0; c < 2; c++) for (int k = 0; k < 14; k++) RULEDISK[c][k] = EEPROM.read(RULES_AT + 14 * c + k);\n')

# ---- the frame, the top bar, setup
ed('  for (int s = 0; s < NP; s++) run_place(s);\n', '  if (want_rules) { want_rules = 0; int w = win_find(K_TASKS, 0); if (w >= 0) win_close(w); win_open(K_RULES, 0); }\n  run_places();\n')
ed('{ uint32_t blocks = (uint32_t)hn; if (blocks != L_est - L_del) keep_bad++; }   // KEEP, checked against the table every latch',
   '{ uint32_t blocks = (uint32_t)hn; if (blocks != L_est - L_del) keep_bad++; }   // KEEP, checked against the table every latch')
ed('int x = text(4, 3, "RULE OS 3", C_GOLD);', 'int x = text(4, 3, "RULE OS 4", C_GOLD);')
ed('  x = text(x + 10, 3, "HEAP ", C_DIM); x = number(x, 3, h_free(), C_TEXT); x = text(x, 3, "/", C_DIM); x = number(x, 3, HU, C_DIM);',
   '  x = text(x + 10, 3, "HEAP ", C_DIM); x = number(x, 3, h_free(), C_TEXT); x = text(x, 3, "/", C_DIM); x = number(x, 3, HU, C_DIM);\n'
   '  if (!theta_left) { x = text(x + 10, 3, "FIELD ", k_books() ? C_DIM : C_RED); x = number(x, 3, field, k_books() ? C_TEXT : C_RED); }')
ed('  for (int s = 0; s < NP; s++) proc[s].budget = 64;\n', '  k_begin(); rules.S = 3000; rules_load();   // S on the C3: 3,000 beats (see README)\n')
# ---- the ghost: SLOW was a budget; it now vents, and visits RULES
ed('{G_BTN, K_TASKS, "2", 0, 0}, {G_BTN, K_TASKS, "SLOW", 0, 0}, {G_BTN, K_TASKS, "SLOW", 0, 0},',
   '{G_BTN, K_TASKS, "2", 0, 0}, {G_BTN, K_TASKS, "VENT", 0, 0}, {G_BTN, K_TASKS, "RULES", 0, 0}, {G_WAIT, 0, 0, 40, 0},\n'
   '  {G_BTN, K_RULES, "S", 0, 0}, {G_BTN, K_RULES, "-", 0, 0}, {G_BTN, K_RULES, "+", 0, 0}, {G_CLOSE, K_RULES, 0, 0, 0},')
# ---- the bench line: the flux in place of budgets
ed('  Serial0.printf("F %u glass %08x pre %u draw %u send %u bytes %u tiles %u free %d held %d\\n", (unsigned)frame_n, (unsigned)glass_print(), (unsigned)c_pre, (unsigned)c_draw, (unsigned)c_send,\n    (unsigned)f_bytes, (unsigned)f_tiles, (int)h_free(), (int)(h_need != 0));',
   '  Serial0.printf("F %u glass %08x pre %u draw %u send %u bytes %u tiles %u free %d held %d field %u recon %u books %d\\n", (unsigned)frame_n, (unsigned)glass_print(), (unsigned)c_pre, (unsigned)c_draw, (unsigned)c_send,\n    (unsigned)f_bytes, (unsigned)f_tiles, (int)h_free(), (int)(h_need != 0), (unsigned)field, (unsigned)k_recon, (int)k_books());')
ed('  if (++rep_n >= 300) { Serial.printf("OS3 REAL latch', '  if (++rep_n >= 300) { Serial.printf("OS4 REAL field %u/%u recon %u books %s | latch", (unsigned)field, (unsigned)rules.G0, (unsigned)k_recon, k_books() ? "shut" : "OPEN"); Serial.printf(" ')
ed('STAGE("OS3 ', 'STAGE("OS4 ', 6)
open(sys.argv[1], 'w').write(s)
print('made', sys.argv[1], len(s.split(chr(10))), 'lines')
