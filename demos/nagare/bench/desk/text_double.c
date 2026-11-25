/* text_double.c  the same textbook steps in double precision, to see what the fixed point adds to the heat book */
#include <stdio.h>
#include <stdlib.h>
#define GW 50
#define GH 40
#define TW_ (GW + 2)
#define TN (TW_ * (GH + 2))
#define TI(i, j) ((j + 1) * TW_ + (i + 1))
static double U[TN], V[TN], T[TN], A[TN], B[TN], C[TN], P[TN], D[TN];
static const double DT = 0.25, BETA = 0.25, NU = 1.0 / 50, KAPPA = 1.0 / 40; static int JAC = 20;
static void bnd(int b, double *x) { for (int j = 0; j < GH; j++) { x[TI(-1, j)] = b == 1 ? -x[TI(0, j)] : x[TI(0, j)]; x[TI(GW, j)] = b == 1 ? -x[TI(GW - 1, j)] : x[TI(GW - 1, j)]; }
  for (int i = 0; i < GW; i++) { x[TI(i, -1)] = b == 2 ? -x[TI(i, 0)] : x[TI(i, 0)]; x[TI(i, GH)] = b == 2 ? -x[TI(i, GH - 1)] : x[TI(i, GH - 1)]; }
  x[TI(-1, -1)] = (x[TI(0, -1)] + x[TI(-1, 0)]) / 2; x[TI(GW, -1)] = (x[TI(GW - 1, -1)] + x[TI(GW, 0)]) / 2; x[TI(-1, GH)] = (x[TI(0, GH)] + x[TI(-1, GH - 1)]) / 2; x[TI(GW, GH)] = (x[TI(GW - 1, GH)] + x[TI(GW, GH - 1)]) / 2; }
static void advect(int b, double *d, const double *d0, const double *u, const double *v) { for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) {
    double x = i - DT * u[TI(i, j)], y = j - DT * v[TI(i, j)]; if (x < -0.5) x = -0.5; if (x > GW - 0.5) x = GW - 0.5; if (y < -0.5) y = -0.5; if (y > GH - 0.5) y = GH - 0.5;
    int i0 = (int)(x + 1) - 1, j0 = (int)(y + 1) - 1; double s1 = x - i0, t1 = y - j0, s0 = 1 - s1, t0 = 1 - t1;
    d[TI(i, j)] = s0 * (t0 * d0[TI(i0, j0)] + t1 * d0[TI(i0, j0 + 1)]) + s1 * (t0 * d0[TI(i0 + 1, j0)] + t1 * d0[TI(i0 + 1, j0 + 1)]); } bnd(b, d); }
static void diffuse(int b, double *x, double k) { for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int c = TI(i, j); C[c] = x[c] + DT * k * (x[c - 1] + x[c + 1] + x[c - TW_] + x[c + TW_] - 4 * x[c]); }
  for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) x[TI(i, j)] = C[TI(i, j)]; bnd(b, x); }
static void project(void) { for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int c = TI(i, j); D[c] = -(U[c + 1] - U[c - 1] + V[c + TW_] - V[c - TW_]) / 2; P[c] = 0; } bnd(0, D); bnd(0, P);
  for (int it = 0; it < JAC; it++) { for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int c = TI(i, j); C[c] = (D[c] + P[c - 1] + P[c + 1] + P[c - TW_] + P[c + TW_]) / 4; }
    for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) P[TI(i, j)] = C[TI(i, j)]; bnd(0, P); }
  for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int c = TI(i, j); U[c] -= (P[c + 1] - P[c - 1]) / 2; V[c] -= (P[c + TW_] - P[c - TW_]) / 2; } bnd(1, U); bnd(2, V); }
int main(int argc, char **argv) { if (argc > 1) JAC = atoi(argv[1]); double start = 0;
  for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) { int dx = i - 25, dy = j - (GH - 5); if (dx * dx + dy * dy <= 16) T[TI(i, j)] = 1.0; start += T[TI(i, j)]; }
  for (int k = 1; k <= 1200; k++) {
    for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) V[TI(i, j)] -= DT * BETA * T[TI(i, j)];
    diffuse(1, U, NU); diffuse(2, V, NU); project();
    for (int c = 0; c < TN; c++) { A[c] = U[c]; B[c] = V[c]; } advect(1, U, A, A, B); advect(2, V, B, A, B); project();
    for (int c = 0; c < TN; c++) A[c] = T[c]; advect(0, T, A, U, V); diffuse(0, T, KAPPA);
    if (k == 200 || k == 600 || k == 1200) { double h = 0; for (int j = 0; j < GH; j++) for (int i = 0; i < GW; i++) h += T[TI(i, j)]; printf("double, JACOBI %d, step %d: heat %.1f%% of start\n", JAC, k, 100 * h / start); } }
  return 0; }
