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
