# RULE OS 4: one kernel for the Uno R3 and the ESP32-C3

Two sketches, one kernel. The Uno build is OS 2's interface; the C3 build is OS 3's windows, arena and ghost. Both run the same `kernel.h`, joined into each file by `tools/make_ino.py` and `tools/make_c3.py`.

| file | board | paste into the IDE as |
|---|---|---|
| `uno/os4.ino` | Arduino Uno R3, SH1106 OLED, KY-023 stick (OS 2's wiring) | board "Arduino Uno" |
| `c3/os4_c3.ino` | ESP32-C3 SuperMini, 2.0" ST7789, stick and buttons A, B, C (OS 3's wiring) | "ESP32C3 Dev Module", core 3.x, USB CDC On Boot: Enabled |

## What is new

**1. The flux, in place of budgets.** OS 2 and OS 3 gave every program a fixed number of steps per frame. OS 4 has no budgets and no priority numbers. The useful pattern does the scheduling:
- Each place carries a **loaded history L**. Every step moves its weight out of the kernel's **field** into L (clause 1).
- The place whose next step is due soonest runs. That's OR = min. Taking a step pushes the place's due mark on by weight × (C + L + drag), which is AND = +. So a place propagates at C / (C + L + drag) of the supply (clauses 2, 3, 6).
- **Drag** is the L of every other place that touched the same shared cell (PUT or GET) this latch or the last. It's found by interaction, never declared.
- A place that gives the machine back (SHOW, WAIT) **vents** a share of L back to the field. A place that ends or is refused vents all of it.
- A place past **THETA reconfigures**. It keeps KAPPA/16 of its history as a core, vents the rest, and starts again at the front (clause 4). When the field can't cover the next step, that place reconfigures rather than waiting.
- **The books, checked every latch:** field + every L = G0. On the Uno the top bar inverts if they ever open; on the C3 FIELD turns red.

What this buys:
- A program that gives back stays light and is always served first.
- A program that never gives back takes what is left, then reconfigures.
- The latch has a fixed supply, so no program can stretch the frame.

**2. A step weighs what it costs.** The supply and the history are counted in beats (256 cycles of the Uno), not steps. Each op's weight is its worst cost, measured on the chip by Timer1 around 31,962 single steps of random programs, plus a stated 25 %. LINE weighs 61 beats and JMP weighs 2.

**3. The same files, the same answers, on both boards.** The Uno build now keeps 32-bit tops, as the C3 does. So ROOT reaches the book's fourth step, **665857/470832**, on the Uno, where OS 2 stopped at WIDE.

**4. RULES.** The kernel's own readings (C, THETA, G0, S, KAPPA, VENT, DRAG) are edited by hand and kept:
- On the Uno, they're kept in the 24 bytes of EEPROM that OS 2 left unused. An Uno that ran OS 2 keeps its five programs, which sit byte for byte where OS 2 put them.
- On the C3, they're kept in flash after the files.

## Hands

- **Uno:** as OS 2. TASKS now shows each place's L and the beats it took this latch. Its buttons are STOP/GO, KILL, VENT (give back all its history now) and RULES. RULES shows each reading; select one, then use − and +, SAVE or FIRST.
- **C3:** as OS 3. TASKS shows L and beats, FIELD and RECON, with VENT and RULES. RULES takes TASKS' place in the arena. A window the arena refuses now says so on the bar; OS 3 refused it silently.

## What was run (17 Nov 2026)

| check | result |
|---|---|
| the kernel against OS 2's and OS 3's own step code (`tools/vmcheck.cpp`), every slot after every step and every dot, line and number drawn | **32-bit kernel equals OS 3** on all 8 files and 20,000 random programs. **16-bit kernel equals OS 2** on all 20,000 but one; that one is a real OS 2 bug, below |
| 16-bit against 32-bit | **0 disagreements** before a WIDE in 20,000 programs (50,000 in a second run). The 16-bit machine refused WIDE on 8; the 32-bit went further on all 8 |
| Uno, chip against desk (simavr, the real AVR image) | **2,000 frames byte-identical**, with a scripted hand visiting every screen. Earlier runs: 3,000 and 700 frames, also identical |
| Uno books | open on 0 latches in every run |
| Uno size | flash 21,836 B (68 %), static RAM 1,781 B |
| Uno stack | **63 bytes never touched**, with the hand pressing every button except ZAP, DEMO and file SAVE. Thin; stated |
| Uno frame (NO_PACE) | 10.9 ms average, 27.1 ms worst over every screen. Four line-drawing hogs: **OS 4 22.5 ms, OS 2 74.3 ms**. One counting hog: OS 4 26.2 ms (29.8 worst) |
| the kernel on the C3's instruction set (bare, Espressif QEMU, `c3k/`) | **402 lines identical** to the desk: 400 latches of the flux (field, histories, a print of every slot and of every dot drawn) and the print of 2,000 random programs. 0 floating instructions in the image. A 3,000-beat latch is about 52,000 counted instructions, under 1 ms at 160 MHz (QEMU's count, not real cycles) |
| C3 desk twin (the sketch's own command stream decoded by a stand-in ST7789) | 2,400 frames of the ghost: **panel equal to the picture on every frame**, flux leaks 0, arena late 0, refused 0, bad moves 0 |
| C3 board path | type-checked by Espressif's RISC-V g++ against stand-ins of the core's headers: 0 errors, 0 warnings. **Not a real board build** |

Pictures, in `pics/`:
- `uno_os2_vs_os4.png`
- `uno_four_hogs.png`
- `uno_os4_tour_sheet.png`
- `c3_os3_vs_os4.png`
- `c3_rules.png`

## Not run

- **Either real board.** Nothing here has run on hardware.
- **The full C3 sketch, built or emulated.** The Arduino ESP32 core couldn't be downloaded in this session. The OS 3 parts are unchanged from a build that compiled with core 3.3.12, and the kernel was run on the C3's own instruction set. Still, the sketch as a whole has not been compiled for the board.
- **Saving across a real power cycle**, on either board.
- **The C3's own op costs.** Its weights are the Uno's, used as relative weights only.

## Limits

- **Uno stack margin: 63 bytes.** The 64-bit landing is what costs it, even with the scratch moved off the stack. If a board misbehaves only under heavy editing, rebuild with `#define KTOP16`: every answer stays the same until an earlier WIDE, and 355 bytes stay free.
- **A latch can overrun S by one step's weight,** at most 61 beats (about 1 ms on the Uno).
- **The weights are the worst seen, not a proof.** A step that costs more than its weight overruns the latch by the difference.
- **Prior art.** "Pick the least due mark, push it on by a stride" is stride scheduling (Waldspurger and Weihl, 1995). What is not in that work, as far as I know:
  - the stride being the program's own loaded history, deposited by what it did;
  - venting on giving back;
  - reconfiguring at theta;
  - drag found from shared cells;
  - books checked every latch.

## The OS 2 bug the check found

OS 2 keeps the pen as `int8_t`. A LINE whose end lies past x = 127 wraps the pen to a negative number, so the next line starts from the wrong place. OS 3 and OS 4 keep 16 bits. Widening OS 2's pen in the harness makes all 50,000 programs agree.

## Make it yours

- **Programs:** EDIT, on either board. Files are the same two-byte steps on both, so a program you write on the Uno means the same on the C3.
- **The kernel:** RULES. A small C (or THETA) keeps every place light and the share even. A large VENT makes giving back pay more. A small G0 makes the field run out, and the places reconfigure all the time.
- **The first hand:** `k_rules_first()` in `kernel.h`.
- **The factory programs:** `DEMO`.

## Files

```
kernel.h              the machine, the weights, the flux, the books (shared)
uno_body.inc          the Uno's screens and hooks          tools/make_ino.py  -> uno/os4.ino
tools/make_c3.py      OS 3 -> c3/os4_c3.ino, by named edits that must each match once
tools/vmcheck.cpp     the kernel against OS 2 and OS 3       tools/flux.cpp     the flux on the desk with a hog
tools/uno_desk.cpp    the Uno sketch on the desk             tools/c3_desk.cpp  the ST7789 desk twin
c3k/c3k.c             the kernel bare on the C3 (QEMU) and on the desk, one source
tools/sbs.py          side-by-side frames                    INSIGHTS.md        what was learned, and every error
```
