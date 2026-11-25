static uint32_t os_gcd(uint32_t a, uint32_t b) {     // the largest whole number that divides both, by halving and taking away only
  if (!a) return b; if (!b) return a;
  uint8_t twos = 0; while (!((a | b) & 1)) { a >>= 1; b >>= 1; twos++; }
  while (!(a & 1)) a >>= 1;
  while (b) { while (!(b & 1)) b >>= 1; if (a > b) { uint32_t t = a; a = b; b = t; } b -= a; }
  while (twos--) a <<= 1;
  return a;
}
// Put three tops into a slot in lowest terms. If any of them does not fit sixteen bits the slot is left alone and the answer is WIDE.
static uint8_t rd_fit(int32_t v, int32_t r, int32_t d, Rd *o) {
  if (d < 0) { v = -v; r = -r; d = -d; }
  if (d > 1) {
    uint32_t g = os_gcd((uint32_t)d, (uint32_t)(v < 0 ? -v : v)); if (g > 1) g = os_gcd(g, (uint32_t)(r < 0 ? -r : r));
    if (g > 1) { v /= (int32_t)g; r /= (int32_t)g; d /= (int32_t)g; }
  }
  if (v > 32767 || v < -32767 || r > 32767 || r < -32767 || d > 32767) return ST_WIDE;
  o->v = (int16_t)v; o->r = (int16_t)r; o->d = (int16_t)d; return 0;
}
static uint8_t rd_mix(const Rd *a, const Rd *b, Rd *o) {                 // value: v1*v2. rate: the two strips. bottoms multiply.
  return rd_fit((int32_t)a->v * b->v, (int32_t)a->r * b->v + (int32_t)a->v * b->r, (int32_t)a->d * b->d, o);
}
static uint8_t rd_add(const Rd *a, const Rd *b, int8_t way, Rd *o) {     // piece by piece, over a common bottom
  if (a->d == b->d) return rd_fit((int32_t)a->v + (int32_t)way * b->v, (int32_t)a->r + (int32_t)way * b->r, a->d, o);
  return rd_fit((int32_t)a->v * b->d + way * ((int32_t)b->v * a->d), (int32_t)a->r * b->d + way * ((int32_t)b->r * a->d), (int32_t)a->d * b->d, o);
}
static uint8_t rd_back(const Rd *b, Rd *o) {                             // the reading that mixes with b back to (1; 0): value d/v, rate -r*d/(v*v)
  if (b->v == 0) return ST_THETA;                                        // nothing mixes back
  return rd_fit((int32_t)b->d * b->v, -((int32_t)b->r * b->d), (int32_t)b->v * b->v, o);
}
static int16_t rd_whole(const Rd *a) { return pc_mouth((int32_t)a->v, a->d); }   // onto the glass, or into a count: through the mouth
static uint8_t vm_step(Proc *p) __attribute__((noinline));
static uint8_t vm_step(Proc *p) {                    // returns 1 when the program's turn is over for this frame
  if (p->pc >= p->n) { p->state = ST_LOST; return 1; }
  uint16_t at = (uint16_t)(p->base + 2 + 2 * (uint16_t)p->pc);
  uint8_t b0 = RK_EE_RD(at), b1 = RK_EE_RD(at + 1), no = 0, next = (uint8_t)(p->pc + 1), over = 0;
  Rd *a = &p->reg[b0 & 7], *b = &p->reg[b1 & 7]; int8_t imm = (int8_t)b1; Rd t;
  switch (b0 >> 3) {
    case OP_END: p->state = ST_END; return 1;
    case OP_SET: a->v = imm; a->r = 0; a->d = 1; break;                                      // a plain number: rate 0
    case OP_ISO: a->v = imm; a->r = 1; a->d = 1; break;                                      // isolate, seeded with rate 1
    case OP_SEED: no = rd_fit(a->v, (int32_t)imm * a->d, a->d, a); break;                    // state the rate slot
    case OP_MOV: *a = *b; break;
    case OP_ADD: no = rd_add(a, b, 1, a); break;
    case OP_SUB: no = rd_add(a, b, -1, a); break;
    case OP_MIX: no = rd_mix(a, b, a); break;
    case OP_DIV: no = rd_back(b, &t); if (!no) no = rd_mix(a, &t, a); break;
    case OP_RATE: if (b->r == 0) no = ST_THETA; else no = rd_fit((int32_t)a->r * b->d, 0, (int32_t)a->d * b->r, a); break;   // rate slot over rate slot; no value is read
    case OP_WHEN: if (b->r == 0) no = ST_THETA; else no = rd_fit(-(int32_t)b->v, 0, b->r, a); break;                        // latches until b's value runs out
    case OP_VAL: a->v = b->v; a->d = b->d; a->r = 0; break;                                  // the value alone, as a plain number
    case OP_LATCH: no = rd_fit((int32_t)a->v + a->r, a->r, a->d, a); break;
    case OP_FLIP: a->r = (int16_t)-a->r; break;
    case OP_LAND: if (imm <= 0) no = ST_THETA; else no = rd_fit(pc_mouth32((int32_t)a->v * imm, a->d), pc_mouth32((int32_t)a->r * imm, a->d), imm, a); break;
    case OP_IFLT: if (!((int32_t)a->v * b->d < (int32_t)b->v * a->d)) next++; break;         // the next step is taken only if a is less than b
    case OP_IFEQ: if ((int32_t)a->v * b->d != (int32_t)b->v * a->d) next++; break;
    case OP_IFBTN: if (!rk_held) next++; break;
    case OP_LOOP: no = rd_fit((int32_t)a->v - a->d, a->r, a->d, a); if (!no && a->v > 0) next = b1; break;   // one fewer to go; back again while any are left
    case OP_JMP: next = b1; break;
    case OP_SHOW: over = 1; break;
    case OP_WAIT: p->sleep = b1; over = 1; break;
    case OP_CLS: win_cls(); break;
    case OP_DOT: win_dot(rd_whole(a), rd_whole(b)); break;
    case OP_PEN: p->pen_x = (int8_t)rd_whole(a); p->pen_y = (int8_t)rd_whole(b); break;
    case OP_LINE: { int16_t x = rd_whole(a), y = rd_whole(b); win_line(p->pen_x, p->pen_y, x, y); p->pen_x = (int8_t)x; p->pen_y = (int8_t)y; } break;
    case OP_NUM: tr(0x2B); tr((uint64_t)(int64_t)a->v); tr((uint64_t)(int64_t)a->d); tr(b1); break;
    case OP_HEAD: {                                                                          // heading (P : Q) to its exact unit: c = (QQ-PP)/(PP+QQ), s = 2PQ/(PP+QQ)
      int16_t hp = rd_whole(a), hq = rd_whole(b);
      if (hp == 0 && hq == 0) no = ST_THETA; else if (hp > 127 || hp < -127 || hq > 127 || hq < -127) no = ST_WIDE;
      else { int32_t bot = (int32_t)hp * hp + (int32_t)hq * hq; rd_fit((int32_t)hq * hq - (int32_t)hp * hp, 0, bot, a); rd_fit(2 * (int32_t)hp * hq, 0, bot, b); }
    } break;
    case OP_STICK: a->v = rk_sx; a->r = 0; a->d = 1; b->v = rk_sy; b->r = 0; b->d = 1; break;
    case OP_RAND: if (imm <= 0) no = ST_THETA; else { a->v = pc_scatter(0, imm); a->r = 0; a->d = 1; } break;
    case OP_PUT: cell[b1 & 3] = *a; break;
    case OP_GET: *a = cell[b1 & 3]; break;
  }
  if (no) { p->state = no; return 1; }               // refused: the program stops on this step
  p->pc = next; return over;
}
