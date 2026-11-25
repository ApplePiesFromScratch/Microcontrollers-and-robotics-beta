> Project doc `claude/HIGHNOON_README.md` (7 Nov 2026), copied unchanged below this line (the picture links dropped).
> `tools/check_highnoon.sh` and `tools/make_map.py` live in the project's tree. Here: `../../tools/desk_panel.c`
> runs the sketch's own DESK build (`gcc -O2 -w -DSKETCH='"../demos/highnoon/highnoon.ino"' -o /tmp/hn tools/desk_panel.c`).

# HIGH NOON: a bright first-person shooter for the ESP32-C3

Midday, a walled arena, and robots coming in waves. Four guns, barrels that go up in chains, and debris and sparks everywhere.

It runs on the ESP32-C3 SuperMini with the 2.0" ST7789 (320 × 240), full screen, every dot drawn every frame. Everything is whole numbers, and every wall, floor, cloud, robot, flame and gun is grown from rules at boot.

## Play

| hands | |
|---|---|
| stick up/down | move |
| stick left/right | turn |
| hold **C** + stick left/right | strafe |
| **A** (hold) | fire |
| **B** | next gun |
| press the stick | dash (a burst of speed with a moment's armour) |

If nothing is touched on the title, the ghost plays. Touch anything to take over.

**The guns:** pulse rifle (fast, with tracers), scattergun (eight pellets), rockets (splash damage that rocks the screen), plasma (rapid glowing bolts). Gun pickups lie around the arena, and big robots drop them.

**The robots:**
- **Grunts** walk and shoot red bolts.
- **Drones** bob in the air and come in close.
- **Brutes** fire three fireballs in a spread.
- **The Titan** walks in every fifth wave, firing volleys of fire and rockets.

**Barrels** go up when shot, damage everything near them, and set each other off.

**Kills in quick succession multiply the score** (double kill, triple kill, rampage, up to ×8). Clearing a wave pays 500 × the wave number, refills the pickups and rolls the barrels back.

## How it's drawn (the pattern, not borrowed formulas)

- **320 rays a frame walk the grid by the carry rule:** take the nearer of the next x-line and the next y-line, add its step. Each ray is built as look + lean × camera, so the length taken last *is* the square-on depth. There's no fisheye to correct and no angle.
- **Floors are laid per dot.** Each row's distance is one divide (242 / (2·row + 1)). The row then starts at a place and steps by a fixed rate across the 320 columns, so a floor dot is two adds and a lookup.
- **One table of headings, grown from a Pythagorean turn,** turns everything: the look, the bob, the spreads, the pulsing lights. No sine is computed anywhere.
- **Shaded palettes make distance and sun free per dot.** Every picture is 8-bit (a ramp and a shade). It's read through 17 palettes: sun side and shade side at eight distances fading toward a clear day's haze, plus "struck white" for the frame a robot is hit.
- **The sky:** its own gradient, clouds drifting over a 256 × 64 density map, and two ridges of mountains (snow on the high peaks), all keyed to the look.
- **Things stand in the world by the one view rule:** depth along the look, side across it, scaled by 242 / depth. They're sorted far to near and clipped against each column's wall depth. Soft shadows sit under the robots, asked the same way (the near and far rows of an oval on the floor).
- **Additive light** for muzzle flashes, tracers, explosions and the hurt edge: light added to a dot already drawn, never a lit scene.
- **Grown at boot:** 6 walls, 3 floors, 32 pictures (robots in walk and fire poses, 6 flame frames, scrap, pickups, shots, 4 held guns), lit by a dome rule (a slope from each shape's own outline against a sun up and to the left) and outlined dark so they read at a glance.

## What was run

**The arena.** `tools/make_map.py` lays out the 32 × 32 map and checks it: walled all round, all 798 open squares reachable, with 8 spawns, 8 barrels and 11 pickups.

**The desk twin.** The ghost plays for 9,000 frames, decoded from the sketch's own command stream. The panel equalled the picture on every frame.
- It reached wave 5 (the first Titan) at frame 5,772 and fell to it at frame 6,729, with 26,200 points and 48 kills. Then it started again, so death and restart were played too.
- The ghost looks for the nearest robot it can see, and shoots the barrel beside it if there is one. It turns until the line passes through it and fires.
- With nothing in sight, it walks a flood toward a gun it lacks, health when hurt, or the nearest robot, steering for the farthest square of the path it can see.

**The board build** (arduino-cli, ESP32 core 3.3.12, Arduino's own ctags): flash **374,104 bytes**, globals **252,964 bytes**, 74,716 left. The pictures use 104,972 of a 112,000-byte pool.

**The emulated chip** (Espressif QEMU, esp32c3, the ghost playing): 2,000 frames, the glass **identical to the desk on all 2,000**. Its books match the desk's exactly (wave 2, 12 kills, 63 shots, 35 damage taken, 1 barrel chain). Stack: 6,092 words never touched. Heap left: 40,044 bytes.

**Work per frame**, by the emulator's count at 160 MHz: **29.4 ms on average (34 a second), worst 56.3 ms.** The 15 bands take about 21 ms of it (sky, walls and floors); the world, the robots and the ghost about 3 ms.

That is QEMU's instruction count, not real cycles. The panel takes the whole screen every frame (153,600 bytes), about 31 ms of wire at 40 MHz, sent by DMA while the next band is drawn. So on the board, the wire, not the chip, sets the pace at roughly 30 frames a second. The Serial Monitor prints `HIGH NOON REAL frame avg … us` every 200 frames.

**Found and fixed on the way:**
- The rocket launcher covered the middle of the screen, and the rifle was too thin to read; both were redrawn.
- Snow ran along every ridge; now it caps only the high peaks.
- Every rifle shot brightened the whole screen. Only explosions do that now, and muzzles get a local flare.
- The ghost hugged walls, steering for the next square's centre. Now it string-pulls: the farthest square of its path it can see.
- It never picked up a gun; now guns are a goal.
- **The chip drew 75 of 2,000 frames differently from the desk while the game stayed in step.** A rocket's smoke trail passed two random draws as arguments of one call, and x86 and RISC-V evaluate arguments in different orders (the trap LANTERN met). The same number of draws either way kept the game identical; only the smoke differed. It now draws into named values first. The undefined-behaviour sanitizer is clean over 3,000 frames.

## Not run

- Your board, stick and buttons. The flags at the top fix a reversed stick, swapped axes, colour order, inversion and a 180° panel.
- Real frame times and the panel at 40 MHz on your wires (`SAFE_SPI` drops to 20 MHz).
- No sound.

## Wiring

SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL to 3V3. Stick VRx 0, VRy 1, press 2. Buttons A 3, B 20, C 21, to ground. Board **ESP32C3 Dev Module**, core 3.x, USB CDC On Boot: Enabled.

## What's known, and what I think is new

Grid ray casting, floor casting and shaded palette tables are long-established (Wolfenstein 3D, Doom's colormaps, Lode Vandevenne's tutorials).

What I don't know of before:
- this at full 320 × 240, every frame, on a no-FPU 160 MHz RISC-V in whole numbers, with all its art grown from rules at boot;
- a ghost that plays it on the bench, so the chip and the desk can be compared dot for dot.
