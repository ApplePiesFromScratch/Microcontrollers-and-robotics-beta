# Practice: do it, then let the checker say whether it holds

Each task has a spec below, a checker (`check.py`), and planted faults the checker must catch (`--planted`). Your files
go in `practice/work/`. Write each file from the spec and the docs it names; do not open the task's `planted/` folder or
the checker's answer tables first (they hold wrong implementations and expected answers). If you did, log a `self`
event; the task then witnesses less.

```
python3 practice/p01_readings/check.py            # your work
python3 practice/p01_readings/check.py --planted  # the checker shown to catch faults
```

Every checker prints one line per check and ends `PASS (n of n checks held)` or `FAIL`. Log each attempt:
`python3 tools/ledger.py add run "p01 attempt 2: FAIL on U10 division"`; a failure you then fix is a `fail` and an
`intervene` event.

## p01 readings (Python): `work/p01_readings.py`

Read: workbook Part IV Units 1-10; METHOD section 1. Provide:

```python
THETA                    # a named refusal: not a number (not an int, float or Fraction), returned, never raised
isolate(v, r=1)          # -> (Fraction(v), Fraction(r)); refuses a float argument by raising
add(a, b), sub(a, b)     # piece by piece
mix(a, b)                # (v1*v2, r1*v2 + v1*r2)
div(a, b)                # derived: q = v1/v2, s = (r1*v2 - r2*v1)/v2**2; THETA when b's value is 0
rate(a, b)               # a's rate slot over b's; THETA when b's rate slot is 0
when(gap)                # latches until the value runs out: -value/rate; THETA when the rate is 0
```

Checked: 30 cases from the book's demonstrations, the seed cancelling on 300 random expressions, division undoing mix on
500 random pairs, refusals. Planted: 6 faults (rates multiplied, theta as a huge number, a dropped strip, when upside
down, theta reading as 0, floats inside).

## p02 carry and mouth (C): `work/p02_carry.c`

Read: workbook Part V Units V1, V2; METHOD section 6. Provide (C99, `#include <stdint.h>`):

```c
typedef struct { int32_t n, d, acc; int64_t count; } pulse_t;
int32_t mouth(int32_t top, int32_t den);   /* den > 0: nearest whole; exact half to the even one; mouth(-t,d) = -mouth(t,d) */
int pulse_tick(pulse_t *p);                /* add n to acc; emit a pulse while acc >= d; return pulses this tick */
```

Checked: 108,012 mouth cases (nearest, ties even, mirror); books d·count + acc = n·ticks and the bound 0 ≤ acc < d on
five rates for up to 100,000 ticks; the 112 pattern at 4/3. Planted: 5.

## p03 heading (Python): `work/p03_heading.py`

Read: workbook Part IV Unit 14, Part V Unit V7; METHOD section 6. Provide:

```python
unit(P, Q)               # (Q*Q - P*P, 2*P*Q, P*P + Q*Q), whole numbers
compose(h1, h2)          # (P1*Q2 + P2*Q1, Q1*Q2 - P1*P2)
turn_point(x, y, P, Q)   # the whole-number plane turned by (P:Q) with three shears, each landed by the mouth
unturn_point(x, y, P, Q) # its inverse
```

Checked: exact units on 624 headings; composition against turning the units on 100 pairs; on a 61 × 61 box for five
headings: no two points to one, unturn(turn(p)) = p, turn(−p) = −turn(p), each image within 2 units of the exact turn.
Planted: 5, including a rounded rotation, which passes for (1:2), (2:3), (1:7), (3:4) and fails for (1:4): a test set
drawn from one family is one witness.

## p04 run back (C): `work/p04_runback.c`

Read: workbook Part V Unit V4; METHOD section 7; `demos/minamo` lesson 8. Provide, on a ring of 256 places:

```c
#define N 256
void step(const int32_t *before, const int32_t *now, int32_t *next);   /* next = 2*now - before + push(now) */
```

`push` is yours: any rule of `now` (a spring between neighbours, a stiffer wall, a rounding inside it). No float. Checked:
the field moves (128 of 256 places changed after 2,000 latches) and all 256 places come home exactly after 2,000 back.
Planted: 4.

## p05 transfer (Python): `work/p05_transfer.py`

Read: workbook Part V Unit V5. Provide `step(h) -> list` on 64 places in a ring: heat spreads by transfer between
neighbours. Checked over 2,000 steps: whole numbers; total held every step; no place ever negative; it spread (tallest
place below half its start). Planted: 3, one of which holds the total exactly while half the places go negative.

## p06 two cuts (JSON): `work/p06_cuts.json`

Read: METHOD sections 3-4; workbook Part V Unit V3. With your p01 module, A = isolate(0, 5), B = isolate(10, −3),
rope = A − B, measure which of `rate(rope,A)`, `rate(B,A)`, `grow` (rope's rate over its value) and `when(rope)` move
under four cuts: naming (rope = B − A), direction (every place and rate negated), seed (every rate times −1), scale
(every place and rate times 3). Write `{"rate(rope,A)": {"naming": "moves"|"stays", ...}, ...}`. Planted: 3.

## p07 kill line (files and the ledger)

Read: `docs/RIGOR.md`; workbook Part V Unit V8.
1. Tune a threshold on the tuning set as often as you like: `python3 practice/p07_kill_line/score.py 30`.
2. Write `work/p07_KILL_LINES.md`: the claim, a line with `pass mark` and a number (out of 200, on the sealed set), and a
   line with `kill line` naming the output that would show it false.
3. Seal it: `python3 tools/seal.py practice/work/p07_KILL_LINES.md`.
4. Read the sealed set once: `python3 practice/p07_kill_line/read_sealed.py <threshold>`.
5. Write `work/p07_REPORT.md` quoting both scores exactly and saying whether the kill line fired.

Checked: chain unbroken; the seal before the read; one read; scores quoted; the verdict matches the numbers. Planted: 3.

## p08 de-reify (Markdown): `work/p08_MAP.md`

Read: `docs/DEREIFICATION.md`, `docs/PATTERN.md`. The checker reads the shape (a four-column table, a "What pays"
section with tiers, a `Kill line:` line, a `Known:` line, no refused words). Whether the relations are right is for a
person: open an entry in `REVIEW.md`. Planted: 4.

## p09 desk twin (report): `work/p09_REPORT.md`

Read: `docs/BOARD_MAPPING.md` section 6; a demo's lesson map. The checker builds the sketch you name with gcc -O0,
gcc -O2 and clang -O2 through `tools/desk_panel.c`, runs 1,200 frames each, and requires identical prints, your quote of
the last print, and a `Not run` line. Planted: 3, one a sketch with two random draws as the arguments of one call, which
gcc and clang evaluate in different orders.

## p10 predict, then run (the ledger)

Pick a TRY line from any demo's lesson blocks. Log the prediction (`tools/ledger.py add predict "demos/<name> LESSON
<n> TRY: ..."`), run it, save the tool output to a file in `work/`, log the result with the file attached and the word
`held` or `failed`. Planted: 3.

## p11 board map (JSON): `work/p11_pins.json`

Read: `docs/BOARD_MAPPING.md`. Place the project's panel, stick and buttons plus an I2C sensor and a servo on the
SuperMini's 13 exposed pins. The project wiring uses 11; three more signals must fit, so something must be reconfigured.
Format: `{"pins": {"signal": gpio}, "tied": {"panel_cs": "gnd", ...}, "notes": {"signal": "what pays"}}`; the signal
names are in the checker's docstring. Planted: 4.

## After the tasks

Stage 14 in `AGENTS.md`: choose a claim in the repo, test it with kill lines at two cuts, and report it.
