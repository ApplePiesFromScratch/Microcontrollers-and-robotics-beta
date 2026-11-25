// ================================================================ KERNEL (shared by the Uno and the ESP32-C3 builds, and by the desk)
// The machine is OS 2's: two-byte steps, 32 moves, eight slots, a slot a reading (value; rate) over one shared bottom in lowest terms.
// The flux is new. There is no budget and no priority number. Each place carries a LOADED HISTORY L (useful pattern, clause 1):
// every step it takes moves one unit out of the FIELD into its L. The place that runs next is the one whose next step is due soonest
// (OR = min, clause 6's tropical rule), and taking a step pushes its due mark on by C + L + drag (AND = +): so a place propagates at
// C / (C + L + drag) of the supply (clauses 2, 3). Drag is the L of every other place that touched the same shared cell this latch or
// the last (a relation found by interaction, never declared). A place that gives the machine back (SHOW, WAIT, END, a refusal) vents a
// share of its L back into the field (all of it, if it ended or was refused); a place whose L passes THETA reconfigures: it keeps KAPPA/16 of it as its core, vents the rest,
// and starts again from the front of the queue (clause 4: the vented simplicity outruns its source).
// Books, every latch: field + every L = G0. Nothing is made or lost; a leak is shown.
// Needs, defined before this file: KTOP32 (or not), NPL, PROC_EXTRA, and the hooks declared at the end of this block.
#include <stdint.h>
#ifdef KTOP32
typedef int32_t ktop; typedef int64_t kwide; typedef uint64_t kuwide;
#define KTOP_MAX 2147483647L
#define KHEAD_MAX 30000
#else
typedef int16_t ktop; typedef int32_t kwide; typedef uint32_t kuwide;
#define KTOP_MAX 32767
#define KHEAD_MAX 127
#endif
typedef struct { ktop v, r, d; } Rd;
typedef struct { Rd reg[8]; uint8_t pc, n, state, file, sleep; int16_t pen_x, pen_y; PROC_EXTRA } Proc;
typedef struct { uint32_t when; uint16_t L, took; uint8_t mask, last_mask, out; } Flow;   // a place's place in the flux
typedef struct { uint16_t C, THETA, G0, S; uint8_t KAPPA, VENT, DRAG, spare; } Rules;    // the kernel's own alphabet: 12 bytes, kept in a file

#define ST_FREE 0
#define ST_RUN 1
#define ST_STOP 2
#define ST_WAIT 3
#define ST_END 4
#define ST_THETA 5
#define ST_WIDE 6
#define ST_LOST 7
#define OP_END 0
#define OP_SET 1
#define OP_ISO 2
#define OP_SEED 3
#define OP_MOV 4
#define OP_ADD 5
#define OP_SUB 6
#define OP_MIX 7
#define OP_DIV 8
#define OP_RATE 9
#define OP_WHEN 10
#define OP_VAL 11
#define OP_LATCH 12
#define OP_FLIP 13
#define OP_LAND 14
#define OP_IFLT 15
#define OP_IFEQ 16
#define OP_IFBTN 17
#define OP_LOOP 18
#define OP_JMP 19
#define OP_SHOW 20
#define OP_CLS 21
#define OP_DOT 22
#define OP_PEN 23
#define OP_LINE 24
#define OP_NUM 25
#define OP_HEAD 26
#define OP_STICK 27
#define OP_RAND 28
#define OP_PUT 29
#define OP_GET 30
#define OP_WAIT 31

// hooks: each board says how a step reaches its glass, its hands and its program bytes
static uint8_t k_code(Proc *p, uint8_t i);
static void k_enter(uint8_t s);
static void k_cls(void);
static void k_dot(int32_t x, int32_t y);
static void k_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
static void k_num(const Rd *a, uint8_t at);
static void k_stick(Rd *a, Rd *b);
static uint8_t k_btn(void);
static int32_t k_rand(int32_t n);

static Proc proc[NPL];
static Flow flow[NPL];
static Rd cell[4];                                   // four shared cells: PUT and GET, the way one program hands a reading to another
static Rules rules;
static uint16_t field;                               // the gradient not yet carried by any place
static uint32_t k_latches, k_recon, k_spent, k_steps_all, k_leaks;
static uint16_t k_steps, k_beats;                    // steps, and beats, given out this latch

// ---------------------------------------------------------------- readings: exact fractions over one shared bottom
#ifdef __AVR__
#define K_NOINLINE __attribute__((noinline))
#else
#define K_NOINLINE
#endif
static kwide KV, KR, KD;                             // the three tops being landed: one scratch, not a copy on the stack at every depth
static K_NOINLINE kuwide k_gcd(kuwide a, kuwide b) { // by halving and taking away only; the 32-bit path when both fit (most of the time)
  if (!a) return b; if (!b) return a;
  if (a <= 0xFFFFFFFFu && b <= 0xFFFFFFFFu) {
    uint32_t x = (uint32_t)a, y = (uint32_t)b; uint8_t tw = 0;
    while (!((x | y) & 1)) { x >>= 1; y >>= 1; tw++; }
    while (!(x & 1)) x >>= 1;
    while (y) { while (!(y & 1)) y >>= 1; if (x > y) { uint32_t t = x; x = y; y = t; } y -= x; }
    return (kuwide)x << tw; }
  uint8_t tw = 0; while (!((a | b) & 1)) { a >>= 1; b >>= 1; tw++; }
  while (!(a & 1)) a >>= 1;
  while (b) { while (!(b & 1)) b >>= 1; if (a > b) { kuwide t = a; a = b; b = t; } b -= a; }
  return a << tw; }
static kuwide k_abs(kwide v) { return (kuwide)(v < 0 ? -v : v); }
static K_NOINLINE uint8_t rd_land(Rd *o) {          // KV KR KD into a slot, lowest terms; a top that does not fit is WIDE and the slot is left alone
  if (KD < 0) { KV = -KV; KR = -KR; KD = -KD; }
  if (KD > 1) { kuwide g = k_gcd((kuwide)KD, k_abs(KV)); if (g > 1) g = k_gcd(g, k_abs(KR)); if (g > 1) { KV /= (kwide)g; KR /= (kwide)g; KD /= (kwide)g; } }
  if (KV > KTOP_MAX || KV < -KTOP_MAX || KR > KTOP_MAX || KR < -KTOP_MAX || KD > KTOP_MAX) return ST_WIDE;
  o->v = (ktop)KV; o->r = (ktop)KR; o->d = (ktop)KD; return 0; }
static uint8_t rd_fit(kwide v, kwide r, kwide d, Rd *o) { KV = v; KR = r; KD = d; return rd_land(o); }
static K_NOINLINE uint8_t rd_mix(const Rd *a, const Rd *b, Rd *o) { KV = (kwide)a->v * b->v; KR = (kwide)a->r * b->v + (kwide)a->v * b->r; KD = (kwide)a->d * b->d; return rd_land(o); }
static K_NOINLINE uint8_t rd_add(const Rd *a, const Rd *b, int8_t way, Rd *o) {
  if (a->d == b->d) { KV = (kwide)a->v + (kwide)way * b->v; KR = (kwide)a->r + (kwide)way * b->r; KD = a->d; }
  else { KV = (kwide)a->v * b->d + way * ((kwide)b->v * a->d); KR = (kwide)a->r * b->d + way * ((kwide)b->r * a->d); KD = (kwide)a->d * b->d; }
  return rd_land(o); }
static K_NOINLINE uint8_t rd_back(const Rd *b, Rd *o) { if (b->v == 0) return ST_THETA; KV = (kwide)b->d * b->v; KR = -((kwide)b->r * b->d); KD = (kwide)b->v * b->v; return rd_land(o); }
static K_NOINLINE kwide k_mouth(kwide top, kwide den) {   // the only landing: nearest whole, half to even, symmetric about zero; den > 0 (wide: LAND only)
  kwide q = top / den, r = top - q * den, a = r < 0 ? -r : r;
  if (2 * a > den || (2 * a == den && (q & 1))) q += top < 0 ? -1 : 1;
  return q; }
static int32_t k_mouth32(int32_t top, int32_t den) { // the same landing, 32 bits: every count and every dot
  int32_t q = top / den, r = top - q * den, a = r < 0 ? -r : r;
  if (2 * a > den || (2 * a == den && (q & 1))) q += top < 0 ? -1 : 1;
  return q; }
static int32_t rd_whole(const Rd *a) { return k_mouth32(a->v, a->d); }

// ---------------------------------------------------------------- one step of one program; 1 when its turn is over for this latch
static uint8_t k_touch;                              // the cells this step touched (drag is found from these)
static K_NOINLINE uint8_t vm_step(Proc *p) {
  if (p->pc >= p->n) { p->state = ST_LOST; return 1; }
  uint8_t b0 = k_code(p, (uint8_t)(1 + 2 * p->pc)), b1 = k_code(p, (uint8_t)(2 + 2 * p->pc)), no = 0, next = (uint8_t)(p->pc + 1), over = 0;
  Rd *a = &p->reg[b0 & 7], *b = &p->reg[b1 & 7], t; int8_t imm = (int8_t)b1;
  switch (b0 >> 3) {
    case OP_END: p->state = ST_END; return 1;
    case OP_SET: a->v = imm; a->r = 0; a->d = 1; break;
    case OP_ISO: a->v = imm; a->r = 1; a->d = 1; break;
    case OP_SEED: no = rd_fit(a->v, (kwide)imm * a->d, a->d, a); break;
    case OP_MOV: *a = *b; break;
    case OP_ADD: no = rd_add(a, b, 1, a); break;
    case OP_SUB: no = rd_add(a, b, -1, a); break;
    case OP_MIX: no = rd_mix(a, b, a); break;
    case OP_DIV: no = rd_back(b, &t); if (!no) no = rd_mix(a, &t, a); break;
    case OP_RATE: if (b->r == 0) no = ST_THETA; else no = rd_fit((kwide)a->r * b->d, 0, (kwide)a->d * b->r, a); break;   // rate over rate: no value read
    case OP_WHEN: if (b->r == 0) no = ST_THETA; else no = rd_fit(-(kwide)b->v, 0, b->r, a); break;                        // latches until b runs out
    case OP_VAL: a->v = b->v; a->d = b->d; a->r = 0; break;
    case OP_LATCH: no = rd_fit((kwide)a->v + a->r, a->r, a->d, a); break;
    case OP_FLIP: a->r = (ktop)-a->r; break;
    case OP_LAND: if (imm <= 0) no = ST_THETA; else no = rd_fit(k_mouth((kwide)a->v * imm, a->d), k_mouth((kwide)a->r * imm, a->d), imm, a); break;
    case OP_IFLT: if (!((kwide)a->v * b->d < (kwide)b->v * a->d)) next++; break;                                           // by crossing, never dividing
    case OP_IFEQ: if ((kwide)a->v * b->d != (kwide)b->v * a->d) next++; break;
    case OP_IFBTN: if (!k_btn()) next++; break;
    case OP_LOOP: no = rd_fit((kwide)a->v - a->d, a->r, a->d, a); if (!no && a->v > 0) next = b1; break;
    case OP_JMP: next = b1; break;
    case OP_SHOW: over = 1; break;
    case OP_WAIT: p->sleep = b1; over = 1; break;
    case OP_CLS: k_cls(); break;
    case OP_DOT: k_dot(rd_whole(a), rd_whole(b)); break;
    case OP_PEN: p->pen_x = (int16_t)rd_whole(a); p->pen_y = (int16_t)rd_whole(b); break;
    case OP_LINE: { int32_t x = rd_whole(a), y = rd_whole(b); k_line(p->pen_x, p->pen_y, x, y); p->pen_x = (int16_t)x; p->pen_y = (int16_t)y; } break;
    case OP_NUM: k_num(a, b1); break;
    case OP_HEAD: { int32_t hp = rd_whole(a), hq = rd_whole(b);                                                               // heading (P : Q) to its exact unit
      if (hp == 0 && hq == 0) no = ST_THETA; else if (hp > KHEAD_MAX || hp < -KHEAD_MAX || hq > KHEAD_MAX || hq < -KHEAD_MAX) no = ST_WIDE;
      else { kwide bot = (kwide)hp * hp + (kwide)hq * hq; rd_fit((kwide)hq * hq - (kwide)hp * hp, 0, bot, a); rd_fit(2 * (kwide)hp * hq, 0, bot, b); } } break;
    case OP_STICK: k_stick(a, b); break;
    case OP_RAND: if (imm <= 0) no = ST_THETA; else { a->v = (ktop)k_rand(imm); a->r = 0; a->d = 1; } break;
    case OP_PUT: cell[b1 & 3] = *a; k_touch |= (uint8_t)(1 << (b1 & 3)); break;
    case OP_GET: *a = cell[b1 & 3]; k_touch |= (uint8_t)(1 << (b1 & 3)); break; }
  if (no) { p->state = no; return 1; }
  p->pc = next; return over; }

// ---------------------------------------------------------------- what a step costs: its weight in beats (256 cycles of the Uno)
// The worst cost of each move, measured on the chip by Timer1 around 31,962 single steps of random programs (tools/opcost), plus a
// stated quarter for what the bench did not reach, landed upward. A step's weight is what it moves from the field into its place's
// history, and what the latch's supply is counted in: so the supply bounds the latch's time, which a count of steps did not.
#ifdef __AVR__
#define K_ROM PROGMEM
#define K_ROM8(a) pgm_read_byte(a)
#else
#define K_ROM
#define K_ROM8(a) (*(a))
#endif
static const uint8_t OPW[32] K_ROM = { 2, 2, 2, 5, 3, 17, 13, 17, 36, 17, 4, 2, 5, 2, 31, 4, 4, 4, 5, 2, 2, 18, 10, 9, 61, 32, 38, 3, 3, 3, 3, 2 };
// ---------------------------------------------------------------- the flux: who steps next, and what each step carries
static void k_rules_first(Rules *r) { r->C = 64; r->THETA = 4000; r->G0 = 16000; r->S = 1200; r->KAPPA = 4; r->VENT = 6; r->DRAG = 4; r->spare = 0; }   // the first hand (see the README for how each was chosen)
static void k_vent(uint8_t s, uint16_t share16) {   // a share of the place's history goes back to the field (the mouth lands it)
  uint16_t L = flow[s].L, v = (uint16_t)k_mouth32((int32_t)L * share16, 16); if (v > L) v = L;
  flow[s].L = (uint16_t)(L - v); field = (uint16_t)(field + v); }
static void k_release(uint8_t s) { k_vent(s, 16); flow[s].when = 0; flow[s].mask = flow[s].last_mask = 0; }   // all of it: an ended or killed place keeps nothing
static uint8_t k_books(void) { uint32_t h = field; for (uint8_t s = 0; s < NPL; s++) h += flow[s].L; return (uint8_t)(h == rules.G0); }
static void k_begin(void) { k_rules_first(&rules); field = rules.G0; for (uint8_t s = 0; s < NPL; s++) { flow[s].L = 0; flow[s].when = 0; flow[s].mask = flow[s].last_mask = 0; } }
static uint8_t k_set_g0(uint16_t g) {                // a new total: the difference is taken from (or given to) the field only; theta if it is not there
  if (g < rules.G0 && (uint16_t)(rules.G0 - g) > field) return 0;
  field = (uint16_t)(field + g - rules.G0); rules.G0 = g; return 1; }
static uint16_t k_drag(uint8_t s) {                  // the L of every other place sharing a cell with s, this latch or the last
  uint8_t m = (uint8_t)(flow[s].mask | flow[s].last_mask); if (!m) return 0;
  uint32_t d = 0; for (uint8_t j = 0; j < NPL; j++) if (j != s && ((flow[j].mask | flow[j].last_mask) & m)) d += flow[j].L;
  d >>= rules.DRAG; return (uint16_t)(d > 60000u ? 60000u : d); }
static uint8_t k_weight(Proc *p) { uint8_t op = p->pc < p->n ? (uint8_t)(k_code(p, (uint8_t)(1 + 2 * p->pc)) >> 3) : 0; return K_ROM8(&OPW[op]); }
static void k_latch(void) {
  k_latches++; k_steps = 0; k_beats = 0;
  for (uint8_t s = 0; s < NPL; s++) { Flow *f = &flow[s]; Proc *p = &proc[s]; f->took = 0; f->last_mask = f->mask; f->mask = 0;
    if (p->state != ST_RUN) { f->out = 1; continue; }
    if (p->sleep) { p->sleep--; f->out = 1; continue; }
    f->out = 0; }
  uint8_t cur = 255;
  while (k_beats < rules.S) {
    uint8_t s = 255; uint32_t best = 0xFFFFFFFFu;
    for (uint8_t j = 0; j < NPL; j++) if (!flow[j].out && flow[j].when < best) { best = flow[j].when; s = j; }   // OR = min
    if (s == 255) break;
    Flow *f = &flow[s]; Proc *p = &proc[s]; uint8_t w = k_weight(p);
    if (field < w) {                                 // its demand is past what the field holds: it reconfigures (clause 4), it does not wait
      k_spent++; if (!f->L) break;                   // nothing to vent and nothing to take: the latch ends
      uint16_t keep = (uint16_t)k_mouth32((int32_t)f->L * rules.KAPPA, 16); if (keep == f->L) keep--; field = (uint16_t)(field + f->L - keep); f->L = keep; k_recon++; continue; }
    if (s != cur) { k_enter(s); cur = s; }
    field = (uint16_t)(field - w); f->L = (uint16_t)(f->L + w);   // a step carries its weight of the field into the place's history
    k_touch = 0; uint8_t over = vm_step(p); f->mask |= k_touch;
    k_steps++; k_beats = (uint16_t)(k_beats + w); f->took = (uint16_t)(f->took + w); k_steps_all++;
    f->when += (uint32_t)w * ((uint32_t)rules.C + f->L + k_drag(s));   // AND = +, scaled by what the step weighed
    if (p->state != ST_RUN) { f->out = 1; k_release(s); }   // it ended, or was refused: it cannot go on, so its whole history goes back
    else if (over) { f->out = 1; k_vent(s, rules.VENT); }   // it gave the machine back: it vents
    if (f->L > rules.THETA) {                        // past theta: reconfigure, keep a core, vent the rest, back to the front
      uint16_t keep = (uint16_t)k_mouth32((int32_t)f->L * rules.KAPPA, 16); field = (uint16_t)(field + f->L - keep); f->L = keep; k_recon++; f->when = best; } }
  uint32_t m = 0xFFFFFFFFu; for (uint8_t j = 0; j < NPL; j++) if (proc[j].state == ST_RUN && flow[j].when < m) m = flow[j].when;
  if (m == 0xFFFFFFFFu) m = 0;
  for (uint8_t j = 0; j < NPL; j++) flow[j].when = flow[j].when > m ? flow[j].when - m : 0;   // a place that sat out comes back level with the front, never with banked credit
  if (!k_books()) k_leaks++; }
// ================================================================ end of KERNEL
