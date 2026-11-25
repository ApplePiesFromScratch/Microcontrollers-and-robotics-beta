# CANDLE: a candle filmed on a phone, with no film

ESP32-C3 SuperMini + 2.0" ST7789 (320 × 240), wired as HIGH NOON, THE FISH AFFECT and TATER-CHAN: SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3. Open `candle_c3.ino`; board **ESP32C3 Dev Module**, core 3.x, **USB CDC On Boot: Enabled**. No controls. The screen stays dark for about 4 seconds while the room is worked out, then it plays without end.

![frames](pics/candle_frames.png)

`pics/candle_preview.mp4` is 40 seconds of the desk twin at the screen's own pixels, doubled.

## How it gets there

No video is stored, and nothing is rendered the usual way each frame.

1. **At boot, every dot of the room becomes a reading.** The room is a half-resolution grid (176 × 136, a margin for the shake). Each cell casts a ray, one ray where its neighbours hit the same thing and four at edges. It takes its light from the flame as a reading with three rate slots: how that light moves when the flame moves across, up and toward the wall. The rates come out of the book's moves on the flame's offset (mix, the derived division, the averaging root). No derivative is written anywhere. The reading covers:
   - diffuse light;
   - a glossy shine on wood, brass and glass;
   - light passing through the wax;
   - a soft fill from the room.
2. **Each frame, the lever.** The flame's lean, height and brightness are carried springs, kicked by a stated scatter. Drafts arrive and leave on countdowns, and now and then the flame gutters. Every cell's light is its value plus its rates times the flame's move: four multiplies a cell.
3. **Then the phone.** These are what make it read as footage:
   - a film curve with a shoulder and an S;
   - the flame's glow round it;
   - lens falloff fixed to the screen;
   - a hand that drifts and trembles (two slow springs and a quick one);
   - an exposure that follows the light about a second late;
   - grain that changes every frame, heavier in the dark, with blotchy colour noise in the shadows.
4. **The flame** is drawn over at full resolution from a table: the blue at its base, a dark round the wick, a white core that clips, an orange tip. It leans with the same spring that moves the room's light.

Whole numbers only: 0 floating-point instructions in the image. Nothing is a function of time.

## What was run (19 Nov 2026)

| check | result |
|---|---|
| emulated C3 (Espressif QEMU) against the desk twin | the room's print at boot equal; **1,800 frames identical**, the flicker and every byte down the wire |
| no float | 0 floating instructions, 0 soft-float helpers |
| sanitizers (address, undefined) | clean over 1,200 frames |
| the IDE's prototype step, then the RISC-V g++ | compiles (against stand-ins of the ESP32 core headers) |
| boot, emulator count | 3.74 s |
| a frame, emulator count at 160 MHz | **35.4 ms** (half cells 14.4, full dots 20.9) beside a 30.7 ms wire: about 28 a second, if the board matches the count |
| image | 35.8 KB code, 202 KB of tables and buffers |

**The lever's witness** (`desk/lever_check.c`): L0 + R·move against the light worked out afresh with the flame really moved, shadows counted again, every 7th cell, relative to the cell's light:

| flame moved (1/64 dm) | in the open: median, 95th | in or near shadow: median, 95th |
|---|---|---|
| (3, 0, 0) | 0.24 %, 1.8 % | 6.8 %, 11.8× |
| (0, 3, 0) | 0.21 %, 3.1 % | 7.2 %, 9.6× |
| (0, 0, 3) | 0.26 %, 2.3 % | 3.5 %, 32 % |
| (3, 2, −3) | 0.23 %, 1.6 % | 18 %, 26× |

So the lever carries the open light. It doesn't carry shadow edges: an edge is a step, and a straight rate can't hold a step. A shadow's share is also the one part of the light whose rates are measured, not carried: at boot the flame is moved 0.4 cm each way, and only cells that see part of the flame get a rate. So the shadows move a little, inside their own partial cells, and less than real ones would. At the largest sway the holder's shadow shows thin arcs; that is this limit.

## Found on the way (`ledger.jsonl`)

- **Dim cells came out black and the wall posterized.** A shift of 16 before the colour multiply floored every dim cell to 0. I'd made that edit as a speed-up and checked only the cost, not the picture.
- **The exposure never moved.** `(target − expo) / 24` truncated to 0 every frame. The remainder is now carried, the alphabet's carry.
- **Chip and desk disagreed on the wire while the flicker agreed.** The grain rows were 256 wide but read up to 447 places in. The chip read past the table into other memory. Caught by the chip-against-desk check.
- **The sanitizer caught left shifts of negative numbers** in the readings and the lever; they are multiplies now.
- **Boot went from 25.7 s to 3.7 s.** The changes:
  - a root that doesn't divide (the C3 divides 64 bits in software);
  - one divide per ray;
  - the book's box tested by crossing fractions;
  - one ray a cell away from edges;
  - shadow rates only in partial shade.
- **At the largest sway the flame's tip hooked away, and the shadows sprouted arcs.** The move is now held inside the reach the lever was witnessed for, and the whole flame leans.
- **Your IDE refused it: `conflicting declaration 'int8_t RX'`.** The ESP32 core declares `RX`, `TX`, `SDA`, `SCK` and the other pin names in every sketch. My stand-in for the core's headers didn't, so my check passed a sketch yours couldn't build. The stand-in now declares them (and the core's macros), fails the old sketch the same way, and passes the renamed one. TATER, JAM and THE FISH AFFECT pass it too.
- **The physics that made it look real was physics I'd have left out.** A tall candle shades the table round its own base (the first taper put a 19 cm ring in shadow). Wax lets light through near the top, and a room gives some light back.

## Not run

- **Your board, and your eyes.** "Looks like video" is a judgement only you can make. It has not been put beside real phone footage of a candle; that is the test to run.
- Real frame times: the Serial Monitor prints `CANDLE REAL frame avg … us` every 200 frames.
- A full board build: the ESP32 core can't be downloaded here. The sketch passed the IDE's own prototype step and the RISC-V compiler against stand-ins. Static data is about 205 KB; LANTERN ran with 255 KB.

## Prior art

Precomputed light transfer with the light's motion folded in is known: precomputed radiance transfer, and light-gradient methods in offline rendering. So are tone curves, film grain, vignettes and camera shake as post effects.

What this puts together is all of it in whole numbers on a 160 MHz chip without floating point. The light's rates come from the book's moves on a reading, and the frame is a lever per cell plus the phone's flaws, checked chip against desk byte for byte.

## Files

- `candle.h`: the room, the readings, the flicker, the frame.
- `candle_c3.ino`: the one-file sketch (`python3 tools/make_c3.py`).
- `desk/twin.c`: the desk twin.
- `desk/lever_check.c`: the lever's witness.
- `desk/probe.c`, `desk/cellprobe.c`, `desk/motion.c`: probes.
- `bare/emu.c`: the bare image for QEMU.
- `tools/check_c3.sh`: chip against desk, cost, no float.
- `tools/ide_check.sh`: the IDE's step.
- `ledger.jsonl`: the ledger.
