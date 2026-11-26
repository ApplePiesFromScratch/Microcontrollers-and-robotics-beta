# AGENTS.md: instructions for an AI working in this repo

You are here to learn a method by doing it, then to test it. Reading is not enough: every stage below ends in a command
that prints PASS or FAIL, and a stage is done only when its command passes and you have logged what you did. If a
command fails, that is information; log it and work out why before going on.

## 0. The rules (read twice; they override your habits)

1. **No independent variable.** Nothing is a function of t. Change happens because a rate is latched into a value, one
   step at a time. A counter of latches is fine; `pos = f(t)` is not. A clock is read only as its change.
2. **Readings, not values.** A reading is (value; rate). `rate(A, B)` divides A's rate slot by B's and answers **theta**,
   a named refusal, when B's rate is 0. A refusal is never a number.
3. **Whole numbers on the chip.** No float. Name the bottom of every quantity (1/16 px, Q16, 1/64 cm). Land a fraction
   once, at the edge, with the mouth (nearest, half to even, mirror-true). Carry remainders.
4. **Move kept quantities; never set them.** Every kept quantity has a books equation checked every step.
5. **Two cuts before you believe a reading.** A number that moves when you change a cut belongs to the cut.
6. **Kill lines first, sealed.** Write what would show a claim false, hash it into the ledger (`tools/seal.py`), then run.
   Amendments are logged and sealed before the reads they govern. Failures stay in the record.
7. **Every self-check has a planted fault beside it.** Show your check can fail.
8. **Desk == emulated chip,** byte for byte, or the work stops.
9. **Report what was run and what was not,** with every number taken from tool output and labelled desk, emulator or
   board. Tier every claim (FORCED, EMPIRICAL, CONDITIONAL, STIPULATED, PRESUMED, UNPAID).
10. **Never claim "impossible".** Say what was built, how it was checked, and which parts are known techniques.
11. **Use the book's words:** reading, value, rate slot, isolate, mix, rate, theta, rule, carry, latch, heading, cut,
    mouth, books. Do not rename them as dual numbers, autodiff, limits or reals; say which part is known elsewhere.
12. **Refused words:** `living`, `alive`, `organism`, `creature`, `being` (as a noun), `entity`, `entities`, `chaos`,
    `intelligence`. Replace each with the relation it stood for. `tools/wordlint.py` checks docs.
13. **James's board reports outrank** the emulator, the desk and every model. Log them open; do not argue.
14. **Carrying constants risks reification in extreme dynamics** (James). Name the regime beside each constant.
    **Ratios as first order only.** **Effort already spent is not a term.**
15. **Read the material whole.** Say what you did not read.

## 1. The stages

Work in order. Log each stage in the ledger as you go:

```
python3 tools/ledger.py add self "starting stage 1: the book"
```

| stage | read | do | done when |
|---|---|---|---|
| 1 the book | `workbook/EVERY_NUMBER_IS_A_RULE_CONSOLIDATED.md`, Part IV whole, then Part V | work every Practice problem in Part IV Units 1-10 on paper (in your scratch file) before reading its answer; mark each answer right or wrong honestly | `python3 workbook/check_workbook.py` passes, and your scratch file has an answer for every problem in Units 1-10 with your marks |
| 2 the moves in code | `docs/METHOD.md` sections 1-2 | write `practice/work/p01_readings.py` | `python3 practice/p01_readings/check.py` PASS |
| 3 holding numbers | METHOD section 6; workbook Part V Units V1, V2 | write `practice/work/p02_carry.c` | `practice/p02_carry_mouth/check.py` PASS |
| 4 turning | Part IV Unit 14; Part V Unit V7 | write `practice/work/p03_heading.py` | `practice/p03_heading/check.py` PASS |
| 5 running back | METHOD section 7; Part V Unit V4 | write `practice/work/p04_runback.c` | `practice/p04_run_back/check.py` PASS |
| 6 transfer | Part V Unit V5 | write `practice/work/p05_transfer.py` | `practice/p05_transfer/check.py` PASS |
| 7 cuts | METHOD sections 3-4; Part V Unit V3 | measure with your p01 and write `practice/work/p06_cuts.json` | `practice/p06_two_cuts/check.py` PASS |
| 8 kill lines | `docs/RIGOR.md` | tune, seal, read once, report | `practice/p07_kill_line/check.py` PASS |
| 9 de-reify | `docs/DEREIFICATION.md`, `docs/PATTERN.md` | write `practice/work/p08_MAP.md`; open a REVIEW.md entry | `practice/p08_dereify/check.py` PASS |
| 10 a board | `docs/BOARD_MAPPING.md` | write `practice/work/p11_pins.json` | `practice/p11_board_map/check.py` PASS |
| 11 a desk twin | `demos/INDEX.md`, then one demo's lesson map | build and run its bench; compare gcc -O0, gcc -O2 and clang | `practice/p09_desk_twin/check.py` PASS |
| 12 predict, then run | any demo's lesson blocks | pick a TRY line, log the prediction, run it, log the result | `practice/p10_predict_then_run/check.py` PASS |
| 13 the errors | `docs/ERRORS.md` | for each of your own failures in stages 2-12, find its pattern in ERRORS.md or add a row in REVIEW.md | your ledger has a `self` event per failure naming its row |
| 14 test the model | everything | choose one claim in any demo README or in `docs/PATTERN.md`; write kill lines; seal; run at two cuts; report run and not run with tiers | your report is in `practice/work/` and a REVIEW.md entry points at it |

Before writing any practice file, do not open that task's `planted/` folder or the checker's answer tables: they
contain wrong implementations and expected answers. If you open one early, log a `self` event saying so; the task then
witnesses less.

## 2. Checking the repo itself

`bash check_all.sh` runs every checker, every checker's planted faults, the lesson-strip proof, the word lint, the
workbook and METHOD checks, and the desk benches it can. It says what it skipped. The emulator is optional: a `skip`
for it is the expected result without it, not a failure, and no stage needs it. To add it, follow
`docs/EMULATOR_SETUP.md`; do not ask the person to install it for you unless they want the extra witness.

**Never edit a checker, a planted fault or a bench script to make your work pass.** The checkers are the instruments of
the course; a changed instrument witnesses nothing. If you believe a checker is wrong, leave it unchanged, log a `self`
event and open an entry in `REVIEW.md` with your evidence.

## 3. How to write here

`docs/WRITING.md`. The first sentence is the work. Every claim names what checked it. No praise, no padding, no closing
offer. Analogy where it pays, stamped as analogy.

## 4. When you disagree with the repo

The map is always up for review. Write the disagreement as a claim with a kill line and a witness, and open it in
`REVIEW.md`. Do not edit a verbatim source (`workbook/sources/`, `docs/USEFUL_PATTERN.md`, a demo's `original/`). Fix
code in a demo's `fixed/` copy, log it in that demo's `FIXES.md` with the witness that nothing else changed, and rebuild
the lessons (`python3 tools/lessons.py build NAME`).
