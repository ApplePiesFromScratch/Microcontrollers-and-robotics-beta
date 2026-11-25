# TATER-CHAN ASCENDS

A blueberry who is certain she is a potato, with anime eyes, on an ESP32-C3 and a 2.0" ST7789 at 320 × 240. It runs about 70 seconds and loops, a little worse every time:

1. **Morning:** she is just a potato in a meadow.
2. **The spark:** speed lines, sparkle eyes and ゴゴゴ, ending in "NANI?!".
3. **Henshin:** a magical-girl transformation in a light tunnel lined with her own face.
4. **Senpai:** heart eyes inside a kaleidoscope, with a ring of little potatoes.
5. **Soul:** her soul is a Julia set.
6. **World:** she is the sun over potato hills.
7. **Cosmos:** she is a ringed planet with potato moons.
8. **Madness:** a kaleidoscope tunnel, a swarm on a torus knot, glitches, colour split and impact frames, all at once.
9. **The end:** "...just a potato." (She is a blueberry.) Her denial gets louder every loop.

In the transformation she announces **AGGRESSIVE TRIPPING!!**

One file: `tater_c3.ino`. Wiring as HIGH NOON and THE FISH AFFECT: SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3. No controls. **Colour order:** the default now matches your panel (it showed red and blue swapped under HIGH NOON's setting); `#define BGR_ORDER` flips it back. Arduino IDE: "ESP32C3 Dev Module", core 3.x, USB CDC On Boot: Enabled. The Serial Monitor at 115200 prints `TATER REAL frame avg … us (… fps)`, and the FPS in the corner during madness is measured on the board.

![two loops, desk twin](pics/sheet_two_loops.png)

`pics/tater_preview.mp4` is one loop, rendered from the desk twin.

## The math

No floating point, no libraries, no GPU. Nothing knows what time it is: every motion is a rate carried from the frame before, and every stage is a countdown.

- **Headings without an arctangent.** At boot, each dot of one quarter of the screen is given its heading and its distance from the middle. The heading is found by asking LANTERN's Pythagorean quarter ring (64 headings) which heading the dot is not behind: the sign of one cross product, in a 6-step binary search. Distances are square roots counted by bits. The other three quarters are mirrors, so 160 × 120 entries serve the whole screen.
- **The tunnel.** Everything about a tunnel dot except its heading depends only on its distance: how deep it is, how twisted, how bent, how fogged. So once a frame each of the 256 distances gets its turn and its 16 colours (64 for the face lining). A dot is then one add and one look-up, 18 instructions.
  - **Kaleidoscope tunnel:** the heading goes through a mirror table first.
  - **Speed lines:** a heading table.
  - **The iris wipe:** a distance test.
- **The potato (a blueberry).** An ellipsoid shaded per dot.
  - The surface direction comes from the dot's place across the body; the rest of a unit is a counted root from a table.
  - Her face is painted on the *turned* body: the direction goes through a 3 × 3 turn built from her carried yaw and pitch. So the anime eyes (lash, iris gradient, two highlights, heart or star pupils), the blush hatching, the mouth and her crown (a blueberry's calyx), dimples round the back all turn with her in 3-D.
  - Cel bands, an ink line, a rim catching the scene's light, a squash that is sprung, not animated.
  - Arms, legs, wand and ahoge are THE FISH AFFECT's capsules. The ahoge is a chain, each point lagging the one below it.
- **The swarm.** Little potatoes on a (2, 3) torus knot. Their orientation is a carried rotation: nudged about two axes each frame, then landed back on true by Gram-Schmidt in whole numbers. Depth-sorted, with perspective.
- **The field.** 160 × 120 levels that feed back each frame (turn, zoom, ripple, a level dimmer), as in THE FISH AFFECT's sky. Folded into wedges, it is the kaleidoscope.
- **The Julia set.** z → z² + c with c = 0.7885·e^(iφ), on 4,096 = 1, poured into the field. The phase is carried, so the set morphs and leaves trails.
- **The hills.** A voxel landscape: 128 × 128 heights of four-octave value noise grown at boot. 160 columns are walked near to far, each drawn only where nothing nearer stands, then lit by slope and contoured in bands. The sky is the field.
- **Space.** 180 stars with depth, streaked by their speed; the planet's rings drawn as a back half and a front half around her.
- **Post.**
  - *Glitch:* rows shifted by even amounts, so whole words move.
  - *Colour split:* red pulled aside, two dots a word.
  - *Impact frames:* a 256-entry table on the wire's low byte.
  - *Flash:* one OR a word.

## What was run (18 Nov 2026)

| check | result |
|---|---|
| emulated chip (Espressif QEMU) against the desk twin | **8,520 frames (four loops) identical**: the story's print and every byte down the wire |
| no float | 0 floating instructions, 0 soft-float helpers |
| sanitizers (address, undefined behaviour) | clean over 8,520 frames |
| the IDE's own prototype step (arduino-cli, Arduino's ctags), then the RISC-V g++ | compiles, against stand-ins of the ESP32 core headers |
| work a frame (emulator count at 160 MHz), worst stage, four loops | every stage averages **32.3 ms or less** (the wire is 30.7). The worst single frame is **39.4 ms (25.4 fps)**: madness in loop 4 |
| image | 35.7 KB code, 185 KB of tables and buffers |

## Found on the way (`ledger.jsonl`)

- **Your panel's red and blue were swapped.** The board mouth inherited from HIGH NOON set the panel's BGR bit with the comment "as this panel is". It wasn't. So HIGH NOON and THE FISH AFFECT most likely showed swapped colours too.
- **"Tearing" from loop 1 on was my glitch**, leaking into every stage as random row shifts. Outside madness it is now a smooth wave, and madness's torn rows are tinted so they read as a glitch.

- **The chip drew the transformation differently from the desk** (frames 427–630). The wand's star carried an unmasked hue, which indexed past the palette. Caught by the sanitizer; every hue is now masked where it indexes a palette.
- **First cost:** tunnel 64 ms, madness 88 ms, speed lines 37 ms. The per-distance tables, pointer-walking loops, a face that only dots on it ask about, and word-at-a-time post brought every stage to the wire.
- **The bench's float check had been reading the hex column** (`fadd53e3` is a branch). It now reads the mnemonics only.
- **The hills first read as noise.** I had judged them by the cost table and not by the picture.

## Not run

- **Not on your board yet.** It uses the same panel start and DMA wire as HIGH NOON and THE FISH AFFECT, which run on it.
- **Frame rates are estimated** from QEMU's instruction count, not measured. The Serial Monitor and the corner counter give the real figure.
- No full ESP32 core build here (the board package downloads are blocked).
- No sound.

## Run it

```
bash tools/check_c3.sh 8520     # desk twin vs the emulated C3, no float, cost per stage
python3 tools/make_c3.py         # writes tater_c3.ino
bash tools/ide_check.sh          # the IDE's prototype step, then the compiler
```
