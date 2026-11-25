# IRORI 2 (囲炉裏, the hearth): an everyday operating system for the ESP32-C3 and small cyberdecks

One flash holds twelve labelled applications. Any number of them run at once, and you switch between them like windows. Profiles say which apps start together and which background rules are on. Everything you set is kept in flash and saves itself. The screens read like a desktop: an icon grid, title bars, a taskbar of running apps, and a bottom strip that always says what each button does right now, worked out from the controls you chose.

Hardware: ESP32-C3 SuperMini + 2.0" ST7789 (320 × 240 across). Wiring as HITOMI / KEA / HIGH NOON:
- Panel: SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3.
- Stick: VRx 0, VRy 1, press 2.
- Buttons (to ground): A 3, B 20, C 21.
- Optional I2C: SDA 8, SCL 9 (off at first; GPIO 8 is also the board's LED).

Other button or stick wiring can be set in Settings > CONTROLS.

Arduino IDE: "ESP32C3 Dev Module" (any C3 board entry works, since pins are GPIO numbers), USB CDC On Boot: Enabled. One file: `irori_c3/irori_c3.ino`.

## The applications

| app | what it is for |
|---|---|
| PINS | every free GPIO: input, output, PWM or analog; switch outputs; readings live, with their change. A pin another part holds says who (STICK, BUTTON A, I2C, SERVO, TIMER, RULES) and is read, not changed |
| PLOTTER | a reading drawn as it goes, a second against it, and rate(A, B) as an exact fraction (THETA when B did not move) |
| SERVO & PWM | a servo by angle (with sweep), a dimmer by duty, or a blink |
| RULES | eight WHEN/THEN rules running in the background:<br>• when: pin high or low, analog above or below, every N seconds<br>• then: pin on, off or toggle, PWM duty, servo angle, or a line on serial<br>Each fires on the change into its condition |
| TERMINAL | the USB serial line: what came in, and a message to send |
| I2C SCAN | who answers on the bus and what they usually are |
| CALCULATOR | exact fractions: 1/3 + 1/6 = 1/2; dividing by nothing says THETA; too big to keep exact says WIDE |
| NOTES | four notes kept in flash |
| TIMER | stopwatch and countdown; the countdown can switch a pin when done |
| PROFILES | six profiles, all editable: BENCH, PLANT CARE, ROBOT, DECK, EVERYTHING (all twelve), SENSOR LOG. Pick the power-on profile here |
| TASKS | start or stop any of the twelve, START EVERY APP, the work each did, latch time, memory, flash copies, whether this was a safe start |
| SETTINGS | five pages: CONTROLS, TEST THE BUTTONS AND STICK, LOOK, HOME SCREEN, SYSTEM |

## Controls

Settings > CONTROLS offers four presets, or your own:

| preset | hands |
|---|---|
| STICK + A B C | stick moves; A or stick press selects; B back, hold B home; C next running app |
| BUTTONS A B C | A next (hold to repeat); B select, hold B home; C back, hold C next app |
| ONE BUTTON (C) | C next; hold C select; hold longer back |
| KEYBOARD ONLY | Serial Monitor (arrows or WASD, Enter, Esc, Tab) or an M5 CardKB on I2C |
| MY OWN | each button's tap, hold and longer hold set to SELECT, BACK, NEXT, PREVIOUS, NEXT APP, HOME or NOTHING |

- **Other settings on the same page:**
  - hold time, repeat delay and repeat speed;
  - the stick: on or off, dead zone, flip either axis, swap axes, CENTRE THE STICK NOW;
  - which GPIO each button and stick axis is on, or NOT FITTED.
- **Tried before kept.** New controls go on trial for 15 s, like a new screen resolution on a desktop. A SELECT made with the new controls keeps them; anything else puts the old ones back by themselves. A button still held from before the change confirms nothing.
- **NEXT reaches everything.** With buttons only, NEXT steps through the home grid, every row, all 20 calculator keys and all 44 keyboard keys. IRORI 1's buttons modes could not reach most calculator columns or keyboard rows.
- **The focus skips information rows**, which do nothing when selected. They still scroll into view.
- **The button test** (Settings) shows each button's GPIO, live DOWN/UP and a count of presses. It also shows the stick's raw X/Y, its centre, the dead zone and the direction pushed, and what the last press did. Every press is also told on the Serial Monitor. Presses act on nothing there; hold any button 3 s to leave.
- **Safe start.** Holding A or B while powering on starts with:
  - factory controls;
  - no apps;
  - rules paused;
  - a SAFE badge in the top bar.

  Three crashes, watchdog resets or brown-outs in a row do the same. SYSTEM > LEAVE SAFE START runs the profile.

## Look

- Five themes: NIGHT, DAY, HIGH CONTRAST, AMBER DECK, GREEN TERMINAL.
- Large text in lists. Labels too long are cut and marked with two dots.
- Hide apps you don't use from the home screen. Settings always shows.
- The panel can be inverted, turned upside down or have red and blue swapped, live.

## Stability

- **Kept twice, mended, saved by itself.**
  - Two copies, each with a sum.
  - A keeper mends a bad copy without throwing away unsaved changes on screen. IRORI 1's keeper did throw them away.
  - Changes save 3 to 6 s after they stop changing (SYSTEM can turn that off).
  - A rule's condition reading flips every latch. It is now kept out of flash, so it causes no writes. In IRORI 1 it sat in the kept store.
- **Carried over.** On the first start, IRORI 1's notes, rules, profiles, panel settings and controls move into IRORI 2's layout.
- **One owner a pin.** Order: hands, I2C, SERVO (when running), TIMER (alarm on), RULES, then PINS. A rule aimed at a pin the servo holds fires but leaves the pin alone. A stopped servo hands its pin back to PINS.
- **Presses counted by interrupt.** Each button has an interrupt with time debouncing, so a tap shorter than one screen latch still counts. IRORI 1 read buttons once a latch, so a quick tap during a long screen send could be missed.
- **Watchdog.** A loop stuck for 5 s restarts the board; repeated restarts lead to a safe start.

## Why selection may have been hard in IRORI 1 (read from the code; not seen on the board)

1. **Pressing A on the first Settings row changed the controls.** That row was CONTROLS, and A cycled it at once. The very next press meant something else (A became NEXT, B became SELECT). Now changes are edited first, then tried for 15 s, then kept or put back.
2. **The buttons-only modes could not reach most keys.** In the calculator they reached only one column of keys, and on the keyboard only one row.
3. **A quick tap could be lost** while the screen was sending.
4. **Selecting an information row did nothing**, which looked like a dead button.

If something else was happening, Settings > TEST THE BUTTONS AND STICK will show it.

## What was run, and what was not

Run:
- **Desk twin**, gcc, 0 warnings.
- **Self-test**: 46 checks, among them 12 planted faults. All pass, also under the address and undefined-behaviour sanitizers. New this version:
  - a tap between latches;
  - B short = BACK, B hold = HOME with progress shown;
  - each preset's hands, including holding NEXT to repeat;
  - NEXT reaching all calculator and keyboard keys;
  - the focus never on an info row;
  - controls trial kept, put back, and not confirmed by a held button;
  - autosave, and no flash writes from flipping rules;
  - the IRORI 1 carry-over, and refusal of a bad sum;
  - pin ownership;
  - safe start, by button and by troubled starts;
  - all twelve apps running with every page drawn;
  - themes;
  - large-text scrolling.
- **Desk == emulated C3** (Espressif QEMU, bare harness): 40 whole-screen prints match over 2,000 latches, for the BENCH profile and for EVERYTHING (all twelve running). About 4.5 ms of compute a latch by QEMU's count, worst 8.1 ms. 0 float instructions.
- **Real Arduino builds** of `irori_c3.ino` with arduino-cli, ESP32C3 Dev Module, USB CDC on boot:

  | core | flash | globals |
  |---|---|---|
  | 3.3.13 | 444,554 bytes | 100,272 bytes |
  | 4.0.0 | 446,731 bytes | 100,424 bytes |

  With all warnings on, only two "defined but not used" helpers remain (the desk uses them).
- **Name check** against 10,045 macros: no clashes. The planted `HZ` is caught.

Not run:
- the board itself;
- the button interrupts, stick, PWM and servo, Wire, CardKB, EEPROM, watchdog and reset-reason counter on hardware;
- the panel send on hardware.

James's board report decides.

On the board, the Serial Monitor shows at start:
- the start reason and troubled-start count;
- `wire bus 0 device 0 (0 is good), strips ok`;
- the profile, free memory and whether this is a safe start.

Every 300 latches it prints a status line. The button test writes `IRORI BUTTON A (GPIO 3) DOWN`, `... TAP = SELECT`, `... UP`.

## Files

`/home/claude/irori/`:
- `core/irori.h`: the whole OS, hardware-free.
- `sim/hal_sim.h`: the bench.
- `desk/desk.c`, `desk/selftest.c`, `desk/screens.c`.
- `bare/` and `check_chip.sh [latches] [profile]`.
- `board/irori_board.cpp`.
- `tools/`.
- `irori_c3/irori_c3.ino`.

IRORI 1 is kept in `/home/claude/irori1/`.
