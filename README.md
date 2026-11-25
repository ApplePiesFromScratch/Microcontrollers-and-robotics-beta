# Microcontrollers and robotics (beta): James Pugmire's method, built to be learned by doing

This repo teaches an AI (and the people working with it) one method for building on microcontrollers: a calculus of
readings (value; rate) with no independent variable, whole numbers on the chip, kept quantities moved and never set, a
named refusal (theta) where a question has nothing to land on, and claims that come with kill lines and witnesses. It
holds the method, the workbook it rests on, practice tasks with checkers, ten annotated demos for the ESP32-C3 and the
Uno, and the procedure for testing the method itself.

**AI: start at `AGENTS.md`.** It lists the stages in order, and each stage ends in a command that prints PASS or FAIL.

**People: start here, then `docs/START.md`.**

## What is in it

| folder | what | check |
|---|---|---|
| `AGENTS.md` | the AI's rules and the 14 stages: read, do, check, log | each stage's command |
| `workbook/` | *Every Number Is A Rule*, consolidated: arithmetic, algebra, geometry, the full calculus (corrected edition, verbatim) and nine new units from the chip work | `python3 workbook/check_workbook.py` (and `--planted`) |
| `docs/` | METHOD (the whole system), RIGOR (the procedure), ERRORS (every trap hit), PATTERN and USEFUL_PATTERN (James's pattern and its mechanics), DEREIFICATION, PHYSICS, OS_GUIDE, BOARD_MAPPING, WRITING, MISSING_MACHINE | `python3 docs/check_method.py`; `python3 tools/wordlint.py` |
| `practice/` | 11 tasks, each with a checker and planted faults the checker must catch | `python3 practice/pNN_*/check.py [--planted]` |
| `demos/` | LACE, TIDE, MINAMO, NAGARE, CANDLE, HIGH NOON, ACE 4, TATER-CHAN, RULE OS 4, IRORI 2: each sketch with lesson blocks, its README, its bench | `python3 tools/lessons.py verify` |
| `tools/` | the ledger, sealing, the lesson builder and strip proof, the word lint, a desk panel for any DESK build | |
| `ledger/` | the append-only, hash-chained ledger every run and failure goes into | `python3 tools/ledger.py verify` |
| `REVIEW.md` | where disagreements with the map go, as claims with kill lines | |

`bash check_all.sh` runs all of it and says what it skipped.

## The hardware

ESP32-C3 SuperMini + 2.0" ST7789 (320 × 240): SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL 3V3; stick VRx 0, VRy 1, press 2;
buttons A 3, B 20, C 21 to ground. Arduino IDE: "ESP32C3 Dev Module", core 3.x, USB CDC On Boot: Enabled. Paste a demo's
`.ino` (one file each) and upload. The Uno demo (LACE) uses an SH1106 OLED. `docs/BOARD_MAPPING.md` has the full map.

## What was run to build this repo, and what was not

Run (25 Nov 2026, in the build workspace): every practice checker against a reference solution kept outside the repo
(PASS) and against its planted faults (all caught); the lesson-strip proof on all ten sketches; the workbook check and
its planted cases; METHOD's 22 quoted lesson lines; the desk benches listed in `demos/INDEX.md`; desk against the
emulated C3 for MINAMO (300 frames, identical) and NAGARE (300 latches, identical). `bash check_all.sh` ends
`38 passed, 0 failed` with reference solutions in `practice/work/` and `29 passed, 0 failed, 12 skipped` without them.



## Provenance

The method, the workbooks, the useful pattern and the sketches are James Pugmire's project
