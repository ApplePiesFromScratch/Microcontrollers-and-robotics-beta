# NAGARE (流れ, flowing): the same warm box, two ways

ESP32-C3 SuperMini + 2.0" ST7789 (320 × 240 across). One file: `nagare_c3/nagare_c3.ino`.

The screen is split down the middle. Both halves show the same box of air: a warm puff laid at the floor, or a heater. Both use the same colour for the same warmth, and both take one step per frame.

## Left: the textbook

The three laws are written on the screen as they are taught:

```
MASS    ∂ρ/∂t + ∇·(ρu) = 0
MOTION  ∂u/∂t + (u·∇)u = −∇p/ρ + ν∇²u + g
ENERGY  ∂T/∂t + u·∇T = κ∇²T + Q
```

They are solved the way a course would solve them: a time step DT = 1/4, the stable-fluids method (Stam 1999) and lift as a body force (Boussinesq). Each step:
- viscous diffusion;
- pressure projection with 20 Jacobi sweeps, for continuity;
- semi-Lagrangian advection of velocity and heat;
- heat conduction;
- a source term and a cold lid.

It runs in fixed point because the chip has no floating-point unit. The same steps in double precision on the desk agree with it to about a percent, so what it shows is the method, not the fixed point.

Its books are measured live:
- **MASS:** how far the velocity is from divergence-free, ∇·u.
- **HEAT:** how much heat it holds beyond (or short of) what came in minus what went out.
- **MOTION:** it keeps no book of momentum.

## Right: ours

No equation is imported. Each place holds three things: **stuff**, a **carried rate** (kept as momentum) and **warmth**. Every change is a hand-on between neighbours, or an outside entry that is counted:

| letter | what it does |
|---|---|
| MOVE | each place's packet goes where its own rate takes it, split over the places it lands on; the leftover goes to the biggest share |
| PUSH | at each face, the crowding on both sides becomes rate, taken from one side and given to the other |
| DRAG | at each face, neighbours trade rate by how much they differ; along the face's own line (parting and converging) more strongly than across it |
| EXCHANGE | crowding is handed down its own difference to the next place, carrying its rate and warmth. PUSH reads two places apart and cannot see an alternation; this sees it (FLOWING's letter) |
| LIFT | a place warmer than the box as a whole is handed upward rate, a cooler one downward. The context is the box's own warmth. The ground's hand is counted |
| FEED, VENT | a puff or the heater puts warmth in; the cold lid takes it out; both counted. The walls' pushes are counted |

So three books close to the unit on every step, and the screen says **EXACT** for each:
- stuff = start;
- warmth = start + in − out;
- each component of momentum = start + what the walls and the ground gave.

There is no DT: a step is a trade. Whole numbers only.

What it is not: a solution of the textbook equations. The letters were built from your method and FLOWING's alphabet. The results are compared with the textbook's by eye and by the readings below.

## What it shows, side by side

- **A puff.** Both rise.
  - The textbook makes a sharp mushroom that splits to the top corners.
  - Ours makes a softer cap that rolls into two lobes and spreads under the lid.
  - Ours rises at about three-quarters of the textbook's pace. Both sides' constants are chosen, so neither pace is a result.
- **The books.**
  - **Textbook:** during a puff its heat book swings from −0.4% to +46.5%: heat it holds that nothing put in. Its continuity residual reaches 0.056. These are properties of the method at the iteration count usually taught: the double-precision run gives the same numbers, and 80 sweeps instead of 20 lowers the swing to about +26%.
  - **Ours:** its books never open.
- **Speed and turning** are read on both sides over the same 2 × 2 cut.
  - Ours carries crowding ripples, which show as diagonal stripes in the speed view. That is sound, which our air keeps. The textbook removes it by projection, because the textbook's air cannot be squeezed.
- **The heater.** The textbook's heater sends up a thin column. In ours the heat pools at the floor and leans to one side. This was a failure against my own pass/fail line N2 (below).

## Hands

- **A:** a puff of warmth at the marker.
- **B:** the heater on or off at the marker.
- **C:** the view: warmth, speed or turning.
- **Stick:** move the marker.
- **Stick press:** start again.
- **Nothing touched for 30 s:** the demo hand plays.

Fixes at the top of the sketch: `RGB_ORDER`, `NO_INVERT`, `ROT180`, `JOY_FLIP_X`. The panel defaults are TIDE's.

## What was run

The kill lines were hashed before any run (`KILL_LINES.md`). Amendments were logged and hashed before the reads they govern (`AMENDMENTS.md`; each hash is of that file as it then stood).

| line | result |
|---|---|
| N1 books | **PASS.** 20,000 latches with puffs, and the heater on, off and moved: stuff, warmth and both momentum components shut to the unit on every latch. Stuff 2,048,000 throughout; warmth 270,546 = 32 + 2,185,751 fed − 1,915,237 vented. |
| N2 steady heater plume | **FAIL.** The warmest tenth's centre sat at 4.7 to 13.7 rows (middle 20); the textbook's at 24 to 26. Reason, from the rules: LIFT relates warmth to the box as a whole, and a steady heater warms the whole box (A1). |
| N2b puff rises within 300 | **FAIL** at the first setting: the centre puff took 352 latches (A2). |
| N2c the same, after a named tuning (lift 16 → 24) | **PASS:** 150 to 197 latches; the textbook 114 to 117. |
| N3 turning | **PASS.** With the heater on: 293 loops one way and 944 the other, of 1,656. |
| N4 the books can see a set | **PASS.** One line that writes a warmth opens the books at exactly that latch. |
| N5 the textbook | Measured, as above. |
| N6 desk = chip | **PASS.** 30 of 30 whole-screen prints match over 1,500 frames of the demo (Espressif QEMU). |
| N7 frame under 66 ms | **FAIL** twice: 78.5 ms (A3), then 67.8 ms (A4). Then **PASS:** 63.4 ms worst; average textbook 24.5, ours 21.7, drawing 14.9 ms. The screen's print was unchanged across both speed-ups, so they changed nothing in the picture. |

Other checks:
- No float instructions and no soft-float helpers in the chip image.
- The address and undefined-behaviour sanitizers are clean over the desk tour and the self-test. They found a left shift of a negative number in the textbook's advection; it is fixed, and the print did not change.

Real Arduino build (arduino-cli, ESP32 core 4.0.0, ESP32C3 Dev Module, all warnings): flash 346,527 bytes, globals 221,656 bytes, leaving 106,024.

Found on the way:
- **Our first faces swept left to right,** each reading the trades of the face before. A still box drifted. Every trade now reads the places as the latch found them and is handed on afterwards.
- **The first PUSH could not see an alternation** (the classic collocated-grid blind spot), and places emptied. EXCHANGE, FLOWING's letter for exactly this, fixed it.
- **LIFT between neighbours only shredded a puff into streaks.** Relating each place to the box as a whole made it rise as one.
- **The textbook's fixed point first floored its products.** That drained every field a little each step, and once lost 84% of the heat. Products are now rounded; the double-precision run then agrees to about a percent.

Not run:
- the board itself (its real frame time; the Serial Monitor prints the time each side takes);
- the buttons, stick and panel on hardware;
- the 3.3 core build.

## TIDE and FLUID, played (25 Nov)

- **TIDE** was built on the desk with its own panel command stream decoded and the ghost playing, 2,500 frames.
  - MARBLE came back every dot, with a match on every step back (360/360).
  - SHATTER brought 232 of 232 balls home, every bit.
  - FORGET (floats) brought back 0 of 232.
  - The ledger matches. Sheets: `shots/older/tide_sheet_1.png`, `tide_sheet_2.png`.
- **FLUID** (Uno) was built on the desk with its own witness, 3,000 frames.
  - The lit count never changed.
  - It returned to the exact seed 3 times out of 3.
  - Its shears keep area, so its continuity is a permutation. Sheet: `shots/older/fluid_sheet.png`.

  FLUID's "nothing set, only moved" is the same grammar as NAGARE's books. N4 is FLUID's lesson ("one h[i] = … line ends conservation for good"), made into a control.

## Files

`/home/claude/nagare/`:
- `core/sim.h`: both methods.
- `core/nagare.h`: the screen.
- `core/draw.inc`: IRORI's letters, plus ∂ ∇ · ² ρ ν κ.
- `sim/hal_sim.h`.
- `desk/desk.c`, `desk/selftest.c`, `desk/probe.c`, `desk/n2b.c`.
- `desk/text_double.c`: the textbook in doubles.
- `bare/` and `check_chip.sh`.
- `board/nagare_board.cpp`.
- `tools/make_ino.py`.
- `KILL_LINES.md`, `AMENDMENTS.md` and their hashes.
- `older_desk/`: the TIDE and FLUID harnesses.
