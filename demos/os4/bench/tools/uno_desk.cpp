#include <stdlib.h>
// the Uno sketch on the desk: setup, then loop; the frame buffer is written after each loop (what rk_show would send)
#include SKETCH
int main(int argc, char **argv) { int n = argc > 1 ? atoi(argv[1]) : 100; FILE *o = fopen(argv[2], "wb"); setup();
  for (int k = 0; k < n; k++) { loop(); fwrite(pc_frame, 1, 1024, o); }
  fclose(o); fprintf(stderr, "desk: %d frames, books open on 0? leaks %u, latches %u, recon %u\n", n, (unsigned)k_leaks, (unsigned)k_latches, (unsigned)k_recon); return 0; }
