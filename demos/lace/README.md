> Project doc `claude/LACE_UNO_README.md` (13 Nov 2026), copied unchanged below this line. The bench files it lists
> (`selftest.sh`, `kit_check.sh`, `lace_tables.cpp`, `lace_exp.cpp`) live in the project's Arduino tree and are not
> in this repo. The sketch here is `lace.ino`, with lesson blocks added (comments only; `tools/lessons.py verify`).

# LACE on the Uno R3

Arduino Uno R3 + the 128x64 SH1106 (Jaycar XC3728) + the KY-023 stick, wired as the other Uno builds (SCK 13, MOSI 11, CS 10, DC 9, RST 8; stick A0 A1, press D2). Paste `../single/lace.ino` into the IDE, board "Arduino Uno". Nothing to install.

Desk twin frames, left alone (ghost walking the bar): 0, 60, 200, 500, 900, 1400, 2000, 2999.

## What it is

The glass is the ground. 8,192 dots, one reading each: lit (a path laid there) or dark (below theta, so a wall). There is no second copy of anything: the picture is the readings.

Four rules and nothing else, the same four as LACE on the C3:

1. a heading steps straight (a step is 1/8 dot, 64 headings from the kit's exact ring);
2. where it stands it lays a lit dot;
3. forgetting is a carry: each latch adds the bar's number, and each whole FD it passes visits one dot (by an odd step on 8,192, so every dot once a lap and no bands) and, if lit, gives it up;
4. a dark dot ahead (9 dots, at 0 and at plus and minus 4 headings) reads as a wall, so the heading turns 8 headings toward the open side, or a scattered side if both sides are open and the middle is not.

96 headings, 6 latches a frame. The headings and the hand are shown by turning their dot over while the frame is sent, and turned back at the start of the next frame.

## Controls

| | |
|---|---|
| stick | moves the hand and lays path |
| hold the press (12 frames) | cuts a hole under the hand |
| short press | steps the forgetting bar (8 stops, 4 to 64 visits a latch). The ticks along the top row show the stop |
| nothing touched, 120 frames | a ghost hand wanders, lays, cuts, and steps the bar every 512 frames |

## The books

Kept to the unit every frame by the chip: lit = start + laid + painted - forgot - cut. A frame that does not close inverts the top page.

## What was run (13 Nov 2026)

| check | result |
|---|---|
| bench | `selftest.sh` IDENTICAL on Love, Hello, MyOS before anything else was trusted |
| ring | `tools/lace_tables.cpp`: c*c + s*s = d*d exactly on all 64 headings; steps are one `pc_mouth` each |
| chip == desk | the real AVR image in simavr, 900 frames of a played hand (idle, stick, long press, short press): IDENTICAL, byte for byte |
| books | desk loop, 6,000 frames idle (ghost walks all 8 stops) and 6,000 frames with the stick: open on 0 frames; 900-frame hand: open on 0 |
| size | flash 3,616 bytes; static RAM 1,539; deepest stack about 22; 477 bytes never touched |
| frame | 25.6 ms typical, 26.3 worst, pure work (NO_PACE); paced at 40 ms it holds 25 frames a second |
| cells | holes of 6 or more dots, 8,000 latches from a lit start, by bar stop 2, 4, 6, 7, 8: 15, 45, 68, 80, 74 (`tools/lace_exp.cpp`) |

## Not run, and not claimed

- Your board, the stick and the press. The dial (A2) is not read.
- That the C3 LACE ablation (no theta, no network) holds at one bit. Untested here.
- That this is a fine web. At 96 headings on 8,192 dots (0.012 a dot; the C3 build is 0.11) the picture is mottled cells and holes. More headings cost 5 bytes each.
- That 1-bit readings are the same rule as the C3's graded memory. They are not: a dot's loaded history is gone, and how long a path lasts is only how long since a heading last stood there, against the carry.

## Tiers

| claim | tier |
|---|---|
| books close to the unit every frame | FORCED, summed again every frame |
| chip == desk on 900 frames | EMPIRICAL (one hand) |
| a 32 x 16 ground of 4-dot sites forms no cells (55 wall sites in 42 pieces) | EMPIRICAL |
| the bar's stops change the cell count as listed | EMPIRICAL (one seed) |
| 96 headings, step 1/8 dot, sense 9, turn 8 of 64, bar steps, STRIDE 4099 | STIPULATED |
| it runs on the board | UNPAID |
