static uint64_t g_of(uint64_t a, uint64_t b) { while (b) { uint64_t t = a % b; a = b; b = t; } return a; }
static uint8_t rd_fit(int64_t v, int64_t r, int64_t d, Rd *o) {   // lowest terms; a top past 2^31 - 1 is WIDE and the slot is untouched
  if (d < 0) { v = -v; r = -r; d = -d; }
  if (d > 1) { uint64_t g = g_of((uint64_t)d, (uint64_t)(v < 0 ? -v : v)); if (g > 1) g = g_of(g, (uint64_t)(r < 0 ? -r : r)); if (g > 1) { v /= (int64_t)g; r /= (int64_t)g; d /= (int64_t)g; } }
  if (v > INT32_MAX || v < -INT32_MAX || r > INT32_MAX || r < -INT32_MAX || d > INT32_MAX) return ST_WIDE;
  o->v = (int32_t)v; o->r = (int32_t)r; o->d = (int32_t)d; return 0; }
static uint8_t rd_mix(const Rd *a, const Rd *b, Rd *o) { return rd_fit((int64_t)a->v * b->v, (int64_t)a->r * b->v + (int64_t)a->v * b->r, (int64_t)a->d * b->d, o); }
static uint8_t rd_add(const Rd *a, const Rd *b, int way, Rd *o) {
  if (a->d == b->d) return rd_fit((int64_t)a->v + (int64_t)way * b->v, (int64_t)a->r + (int64_t)way * b->r, a->d, o);
  return rd_fit((int64_t)a->v * b->d + way * ((int64_t)b->v * a->d), (int64_t)a->r * b->d + way * ((int64_t)b->r * a->d), (int64_t)a->d * b->d, o); }
static uint8_t rd_back(const Rd *b, Rd *o) { if (b->v == 0) return ST_THETA; return rd_fit((int64_t)b->d * b->v, -((int64_t)b->r * b->d), (int64_t)b->v * b->v, o); }
static int32_t rd_whole(const Rd *a) { return mouth(a->v, a->d); }
static uint8_t vm_step(Proc *p, int s) {                  // one step of one program; 1 when its turn is over for this latch
  if (p->pc >= p->n) { p->state = ST_LOST; return 1; }
  uint8_t b0 = p->code[1 + 2 * p->pc], b1 = p->code[2 + 2 * p->pc], no = 0, next = (uint8_t)(p->pc + 1), over = 0;
  Rd *a = &p->reg[b0 & 7], *b = &p->reg[b1 & 7], t; int8_t imm = (int8_t)b1; uint8_t ink = (uint8_t)(C_INK + s);
  switch (b0 >> 3) {
    case OP_END: p->state = ST_END; return 1;
    case OP_SET: a->v = imm; a->r = 0; a->d = 1; break;
    case OP_ISO: a->v = imm; a->r = 1; a->d = 1; break;
    case OP_SEED: no = rd_fit(a->v, (int64_t)imm * a->d, a->d, a); break;
    case OP_MOV: *a = *b; break;
    case OP_ADD: no = rd_add(a, b, 1, a); break;
    case OP_SUB: no = rd_add(a, b, -1, a); break;
    case OP_MIX: no = rd_mix(a, b, a); break;
    case OP_DIV: no = rd_back(b, &t); if (!no) no = rd_mix(a, &t, a); break;
    case OP_RATE: if (b->r == 0) no = ST_THETA; else no = rd_fit((int64_t)a->r * b->d, 0, (int64_t)a->d * b->r, a); break;   // rate slot over rate slot: no value read
    case OP_WHEN: if (b->r == 0) no = ST_THETA; else no = rd_fit(-(int64_t)b->v, 0, b->r, a); break;                        // latches until b's value runs out
    case OP_VAL: a->v = b->v; a->d = b->d; a->r = 0; break;
    case OP_LATCH: no = rd_fit((int64_t)a->v + a->r, a->r, a->d, a); break;
    case OP_FLIP: a->r = -a->r; break;
    case OP_LAND: if (imm <= 0) no = ST_THETA; else no = rd_fit(mouth64((int64_t)a->v * imm, a->d), mouth64((int64_t)a->r * imm, a->d), imm, a); break;
    case OP_IFLT: if (!((int64_t)a->v * b->d < (int64_t)b->v * a->d)) next++; break;                                           // by crossing, never dividing
    case OP_IFEQ: if ((int64_t)a->v * b->d != (int64_t)b->v * a->d) next++; break;
    case OP_IFBTN: if (!down) next++; break;
    case OP_LOOP: no = rd_fit((int64_t)a->v - a->d, a->r, a->d, a); if (!no && a->v > 0) next = b1; break;
    case OP_JMP: next = b1; break;
    case OP_SHOW: over = 1; break;
    case OP_WAIT: p->sleep = b1; over = 1; break;
    case OP_CLS: if (pw_p) memset(pw_p, C_WIN, PW * PH); break;
    case OP_DOT: pw_dot(rd_whole(a), rd_whole(b), ink); break;
    case OP_PEN: p->pen_x = (int16_t)rd_whole(a); p->pen_y = (int16_t)rd_whole(b); break;
    case OP_LINE: { int32_t x = rd_whole(a), y = rd_whole(b); pw_line(p->pen_x, p->pen_y, x, y, ink); p->pen_x = (int16_t)x; p->pen_y = (int16_t)y; } break;
    case OP_NUM: pw_num((b1 & 15) * 8, (b1 >> 4) * 12, a, ink); break;
    case OP_HEAD: { int32_t hp = rd_whole(a), hq = rd_whole(b);                                                               // heading (P : Q) to its exact unit
      if (hp == 0 && hq == 0) no = ST_THETA; else if (hp > 30000 || hp < -30000 || hq > 30000 || hq < -30000) no = ST_WIDE;
      else { int64_t bot = (int64_t)hp * hp + (int64_t)hq * hq; rd_fit((int64_t)hq * hq - (int64_t)hp * hp, 0, bot, a); rd_fit(2 * (int64_t)hp * hq, 0, bot, b); } } break;
    case OP_STICK: a->v = pad.jx / 32; a->r = 0; a->d = 1; b->v = pad.jy / 32; b->r = 0; b->d = 1; break;
    case OP_RAND: if (imm <= 0) no = ST_THETA; else { a->v = (int32_t)(scatter() % (uint32_t)imm); a->r = 0; a->d = 1; } break;
    case OP_PUT: cell[b1 & 3] = *a; break;
    case OP_GET: *a = cell[b1 & 3]; break; }
  if (no) { p->state = no; return 1; }
  p->pc = next; return over; }
