# ACE 4: a dogfight over a dusk canyon

ESP32-C3 SuperMini + 2.0" ST7789 (320 × 240), wired as every C3 demo here. One file: `ace4.ino` (lesson blocks added;
`original/ace4.ino` is the sketch as uploaded on 25 Nov 2026). This README was written for the repo from the sketch's own
header and code; ACE 4 has no project README yet.

## What it is

- **Airframes grown from one seed each**, regrown every frame from their parts. The parts are a ledger: a hit adds load;
  past its threshold a part keeps 2/5 of the load and tears, and the rest falls burning and becomes ground. Flight reads the
  parts, so a torn wing rolls the jet.
- **The missile reads the turn of the line of sight**, not where the target is, and turns its own heading by 4 times that
  turn (`PN_N`). A line of sight that does not turn is a collision course. `-DPURE_PURSUIT` puts back ACE 3's law (lean
  toward the bearing) for comparing.
- **The seeker is a reading of heat**: the hottest thing in a 37° cone, falling with distance. A flare starts hotter than a
  jet and cools by a sixteenth a frame, so for about 12 frames it can steal the seeker. Bandits flare when a missile reads
  them; hold C to flare yourself.
- **GLIDE**, a whole-number 3D rasteriser woven a band of 16 rows at a time, with per-band depth strips, perspective-true
  texture every 16th dot, light per corner and fog. The far land is laid in 2 × 2 blocks, and a frame that read heavy lays
  the next one a grain coarser.

## Hands

VRx roll, VRy pitch, A guns, B missile, C camera (hold: flares), stick press afterburner. Hold B at power-up for a still
test card (set `RGB_ORDER` and `NO_INVERT` by eye). Nothing touched: the demo pilot flies.

Fixes at the top of the file: `ROT180`, `RGB_ORDER`, `NO_INVERT`, `SAFE_SPI`, `WIRE_80`, `NO_OVERLAP`, `NO_HEAL`, `JOY_*`,
`DEMO`, `NO_ROLL`, `PURE_PURSUIT`. Bench builds: `DESK`, `MOUTH_FIFO`, `NULL_WIRE`, `TRUTH`, `FRAMES=n`.

## What was run for this repo (25 Nov 2026, desk only)

| check | result |
|---|---|
| desk twin, `tools/desk_panel.c`, 1,200 frames, demo pilot | ran; 24 panel prints |
| the same 1,200 frames under gcc -O0, gcc -O2 and clang -O2 | the 24 prints identical across all three |
| frames 100, 300, 600 looked at | a jet, a missile's glow, the canyon land at three distances |

## Not run for this repo

- The emulated C3 (desk == chip), the board build, frame cost, the MISSILES and DAMAGE books.
- `PURE_PURSUIT` against the rate law. The lesson block in section 15 sets this up as a prediction to run.
- James's board. Board reports outrank everything above.

## Known

Proportional navigation (turning by a multiple of the line of sight's rate) is a guidance law in use since the 1950s.
Band rasterising, depth strips and perspective-correct texture by planes in 1/z are standard renderer techniques. The
reading in the book's words, the part ledger, and all of it in whole numbers on a no-FPU chip are what this sketch adds.
