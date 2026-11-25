# MINAMO (水面, the water's face)

A 100-second film for the ESP32-C3 SuperMini and the 2.0" ST7789, in which a pond runs forward for fifty seconds and then exactly backward. Dusk: a maple leaf falls, its ring spreads, and five koi come in under the surface. Night: rain starts, the moon's road on the water breaks up, and a storm peaks with lightning reflected in the pond. Then the same water runs back. The rain leaves the surface upward and the rings close into their drops. The koi swim out tail first, and the leaf leaps back into the air. Dawn finds the water flat. The last latch is the first, checked word for word.

No video is stored. The one sketch is `minamo_c3.ino`, 570 lines, built by `tools/make_c3.py` from `minamo.h`.

## The one line that runs both ways

The surface is two readings of height, `h(s-1)` and `h(s)`, in whole numbers on a 160 × 120 grid. One step is the leapfrog:

    h(s+1) = 2 h(s) - h(s-1) + F(h(s)) + S(s)

`F` is the nine-point Laplacian times a wave speed that falls toward the banks, rounded toward zero. It reads only `h(s)`. `S` is every push on the water at step `s`: drops, the leaf landing, koi wakes. It is read from the step's number alone. So the same line, solved for the other end, is the step back:

    h(s-1) = 2 h(s) - h(s+1) + F(h(s)) + S(s)

One function, `leap(X, Y, t)`, does both. Forward, X holds `h(s-1)`; back, X holds `h(s+1)`. In whole numbers the inverse is exact for any `F`, including the rounding. That is the book's carry (nothing dropped, so nothing to recover) applied to a wave instead of a remainder.

The film is a position `p`, and the step is read from it: `s = 2p` going out, held at the storm's height for 16 latches, then `s` falls back to 0. Button C (pin 21 to GND) turns the film the other way at any moment. The water follows, because every step is undoable.

## Where the light comes from

Nothing is lit by a model of the scene. Every colour is read out of the surface each latch:

- **The floor through the water.** Each cell's Sobel slope bends where the floor painting (320 × 240, 8-bit) is read. Pebbles shift under a ring the way they do in a real pond.
- **The light the curve gathers.** The surface's Laplacian picks a light level (32 of them) for the floor below. A converging ripple throws a bright line, and a diverging one a dark one. These are caustics, from one subtraction.
- **The sky in it.** From above, the sky painting (the moon and a maple canopy) is read at the slope again, farther. From low over the water, the far bank is mirrored about the horizon and bent by the slope, with more mirror at a glancing angle. The moon's road and the lightning's reflection come out of this, not from anything drawn.
- **Koi where the bent ray lands.** A koi is not drawn on top. It is tested at the point where the dot's ray meets it, with a quarter of the floor's bend because it swims near the top, so ripples pass over its back. Its shadow falls on the floor nine rows down. Fins let the floor through and brighten it.
- **One painting, five lights.** Dusk, night, storm, before dawn and dawn are palettes over the same two paintings (HANAKAZE's iro-gae). The film curve, the lens falloff and the fades are folded into those palettes, not done per dot.

## What I took from JUMON, KAGEROU and HANAKAZE

- **KAGEROU** makes the film a function of its position, so scrubbing and running backward cost nothing, and exactness lets it check the seed at every swap. MINAMO keeps the idea and swaps the mechanism. KAGEROU moves dye by permutations; MINAMO uses the wave equation itself, whose leapfrog form is its own inverse. KAGEROU's dye cannot spread, but these waves can travel, reflect, interfere and slow in the shallows, and still come home.
- **JUMON's lens** reads the background through a bend ("the dot at p shows the painting at p minus the pull"). Here that becomes the whole renderer: floor and sky are both read through the surface's slope.
- **The exposure sheet.** The shots from above are on 2s (12.5 drawings a second), as the cel engines put layers on 2s. The pond still steps every latch, so nothing about the water's timing changes. The low shots are on 1s.
- **Counted work, not timed work.** JUMON's governor reads a band's cost before doing it, and its constants are fitted to emulator counts. I made every cost decision from the emulator's instruction count, and it caught one thing my estimates missed twice: real per-dot code ran about twice what I had pencilled.
- **A second witness for free.** The LEVER's witness checks that a still layer's print did not change. An invertible step gives a stronger one: the way back must equal the way out. The board writes the pond's print at every latch going out, compares it at every latch coming back, and checks flat water at the end. A broken witness shows as a red square in the top right corner, as in KAGEROU.
- **A pattern worth knowing.** The three sketches share several hundred identical lines: the three mouths, the coverage-row raster, the palette cut and the shot machinery. A shared header, as CANDLE and MINAMO use with `make_c3.py`, would let a fix land once.

## What was run, and what it showed

| check | result |
|---|---|
| chip == desk (Espressif QEMU, `-icount 3`, the bare image) | **identical for all 2,501 latches**: the pond's print and every byte down the wire |
| the way back | 1,243 of 1,243 latches matched their print from the way out, on the desk and on the emulated chip; the pond was flat at the end (homes 1, bad 0) |
| floats instead (`desk/float_pond.c`) | single floats come home 0.0014 high at worst, doubles 3e-12: close but not home, so a word-for-word check would fail. Whole numbers: every cell zero |
| no float on the chip | 0 float instructions, 0 soft-float helpers in the image |
| Arduino IDE's prototype step + RISC-V g++ | compiles (stub headers carry the core's names: RX, TX, SDA, B0…, PI…) |
| ASan + UBSan, the whole film | clean |
| memory | bss 223,708 B (floor 76.8 KB, the two surface readings 76.8 KB, sky 19.2 KB, colour tables 17 KB) |
| cost (emulator count at 160 MHz) | 43.0 ms a latch on average over the film: low shots about 46 ms, a drawing from above 54 ms without koi and 75 ms with five; held latches about 11 ms (the pond's two steps). Worst 92.5 ms, at a cut where the sky is repainted |

At the paced tempo (a latch every 40 ms, a slow latch borrowing from the next) the film should run about 108 s, if the board matches the emulator's count. **It has not been run on the board.** The count is instructions, not cycles. Loads and taken branches cost the real chip more, so expect it somewhat slower. The film's length would grow, but its picture would not change.

## The board

- Wiring as HIGH NOON, KAGEROU and JUMON: SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3. Button C on pin 21 to GND is optional.
- Arduino IDE: "ESP32C3 Dev Module", core 3.x, USB CDC On Boot: Enabled. The Serial Monitor at 115200 prints a line every 125 latches: real latch time, the pond and draw cycles, and the witnesses.
- RGB order is the default (your panel, 20 Nov). Fixes at the top of the sketch: `BGR_ORDER`, `NO_INVERT`, `ROT180`, `SAFE_SPI` (40 MHz), `WIRE_20`, `NO_PACE`.
- The wire runs at 80 MHz on FAREY's DMA queue, as KAGEROU and JUMON do.
- If the linker reports DRAM overflow (224 KB of statics is more than CANDLE's 202 KB), the witness store `PRINT` (5 KB) is the first thing to drop.

## Files

- `minamo.h` is the engine: types above `/*@@ CODE`, then the code.
- `minamo_c3.ino` is the one-file sketch, built by `tools/make_c3.py`.
- `desk/twin.c` is the sketch's own command stream decoded by an ST7789 stand-in. `FRAMELINES=1` prints the pond's print and the wire's, `RAW=1` streams frames for the preview, and `EVERY=n` writes PPMs.
- `desk/float_pond.c` is the ablation: the same pond in floats.
- `bare/emu.c`, `link.ld` and `start.S` are the bare image for the emulator. `tools/check_c3.sh N` runs chip against desk, the cost and the float scan; `tools/ide_check.sh` is the IDE's step.
- `ledger.jsonl` records the development events as they happened, including the failures.
- `pics/minamo_preview.mp4` is the whole film from the desk twin at 2× (held latches held, as on the board). `pics/minamo_frames.png` shows twelve moments.
