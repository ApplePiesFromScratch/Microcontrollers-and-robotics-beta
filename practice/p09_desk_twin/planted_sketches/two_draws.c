/* planted: two scatter draws as the arguments of one call. The order they are evaluated in is not defined by C, and gcc
   and clang choose differently on x86-64 (the trap HIGH NOON and LANTERN met between x86 and RISC-V). */
static unsigned s = 12345u, frame_n;
static unsigned draw(void) { s = s * 1103515245u + 12345u; return s >> 16; }
static void put(unsigned a, unsigned b) { PANEL[frame_n % 240][(a ^ (b << 1)) % 320] = (uint16_t)(a * 3u + b); }
void setup(void) {}
static void frame(void) { put(draw(), draw()); frame_n++; }
