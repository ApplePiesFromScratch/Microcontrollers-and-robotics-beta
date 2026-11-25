# The demos, and the skill each one teaches best

Ten sketches, chosen from the project's builds for what they teach, not for how many there are. Each `.ino` here carries
lesson blocks: a lesson map at the top, then a block above every section with fixed fields (WHAT, WHY, RULE, RELATION,
HOLDING, BOOKS, CUT, BUDGET, TRAP, FOUND, TRY, WITNESS, TIER, KNOWN). The blocks are comments only:
`python3 tools/lessons.py verify` strips them and checks the result byte for byte against the base sketch.

| demo | board | the skill | start with lesson | bench (desk) |
|---|---|---|---|---|
| `lace` | Uno + OLED | theta as a working part, shown by ablation; books every frame; the glass as the only store | 5, 7, 8 | none here (simavr bench in the project) |
| `tide` | C3 | a machine with no two-to-one instruction; a checker that refuses lossy code; float beside it failing | 8, 11, 14 | `bench/desk.c`, `tools/desk_panel.c` |
| `minamo` | C3 | a wave field that runs back exactly; a witness written going out and checked coming back; pushes read from the step number | 6, 7, 8, 15 | `bench/desk/twin.c` |
| `nagare` | C3 | a textbook fluid and a letters fluid side by side; kill lines that failed and were kept; amendments sealed before reads | 1, 4, 5, 6 | `bench/desk/selftest.c` (N1-N5) |
| `candle` | C3 | the lever: (value; three rates) computed once, read with four multiplies a dot | 6, 10, 14 | `bench/desk/twin.c`, `lever_check.c` |
| `highnoon` | C3 | the construction in which the correction never arises (no fisheye); palettes for distance; a ghost so desk and chip can be compared | 8, 13, 19, 21 | `tools/desk_panel.c` |
| `ace4` | C3 | a rate law (line-of-sight turn) where pursuit uses a position; parts as a ledger that reconfigures; a whole-number 3D rasteriser | 11, 15, 7 | `tools/desk_panel.c` |
| `tater` | C3 | a film with no clock and no float: countdowns, carried rates, per-distance work once a frame, a feedback field | 7, 8, 13 | `bench/desk/twin.c` |
| `os4` | C3 (and Uno) | a scheduler made of the useful pattern: loaded history, min and +, drag by contact, reconfigure past theta | 3, 10, 15 | `bench/tools/c3_desk.cpp` |
| `irori` | C3 | an everyday OS: one owner a pin, the clock only as its change, settings kept twice, a self-test with planted faults | 11, 12, 19 | `bench/desk/selftest.c` (46 checks) |

## Why these ten

- **The method's core moves each have one demo where they carry the weight:** theta (LACE), the carry and run-back (TIDE,
  MINAMO), transfer and books (NAGARE), the lever (CANDLE), the heading and the carry rule as geometry (HIGH NOON),
  rate laws and the part ledger (ACE 4), the useful pattern as scheduling (OS 4), the board as relations (IRORI).
- **The rigor has its model case:** NAGARE's kill lines and amendments, with two failures kept in the record.
- **The best graphics and video:** TATER (anime film), CANDLE (a phone-filmed candle), MINAMO (a pond film), HIGH NOON and
  ACE 4 (real-time 3D games).
- **Operating systems:** OS 4 and IRORI, with OS 2 and OS 3 in `os4/bench/base/`.

Left in the project, and why: KAERU (James found it repeats TIDE), KAGEROU, HANAKAZE, JUMON and LANTERN (their lessons
overlap MINAMO, HIGH NOON and CANDLE; LANTERN's argument-order trap is taught in HIGH NOON and `practice/p09`), SHINEN and
SHINKAI (deep zooms, a narrower skill), HITOMI (a face driven over serial: a good next demo for robotics, not yet
annotated).

## How the lesson blocks were written, and what that means for trusting them

Written on 25 Nov 2026 by the AI that assembled this repo, from each sketch's own header and section comments, its
README, and a reading of the code in every section that carries a claim (the step, the books, the checker, the law).
Long drawing and table code was read at the level of its functions and comments, not line by line. So:

- WHAT, RULE, BOOKS and HOLDING lines restate the code and its comments; check them against the code you are reading.
- Numbers come from the demo's README (labelled "README records") or from runs made for this repo (labelled re-run).
- TRY lines are predictions, not results. None was run by the author unless the line says so. `practice/p10` asks you to
  run them; a TRY whose prediction fails is a finding to log, not an error to hide.
- Three lessons found defects while being written: HIGH NOON's ignored refusal (fixed, `highnoon/FIXES.md`), TIDE's
  header comment (fixed, `tide/FIXES.md`), and the rounded-rotation family (`practice/p03`).

## Runs made for this repo (desk)

| demo | run | result |
|---|---|---|
| tide | `bench/desk.c`, 2,440 frames | BOOKS forward 2440 back 2440, marble 1, shatter 232/232, forget 0/232 (matches README) |
| tide, highnoon, ace4 | `tools/desk_panel.c`, 1,200 frames, gcc -O0 / gcc -O2 / clang -O2 | identical prints in all three |
| highnoon | original against fixed, 3,000 frames | identical prints; planted small pool: original overflows, fixed runs |
| minamo | `bench/desk/twin.c`, 2,501 frames | homes 1 bad 0, mirror ok 1243 bad 0 (matches README) |
| nagare | `bench/desk/selftest.c` | N1-N5 pass |
| irori | `bench/desk/selftest.c` | SELFTEST PASSED, 0 failed |
| minamo | `bench/tools/check_c3.sh 300`: desk against the emulated C3 (Espressif QEMU, `-icount 3`) | IDENTICAL, 300 frames, every byte down the wire; 0 float instructions; emulator count 39.7 ms a frame on average, worst 69.1 |
| nagare | `bench/check_chip.sh 300`: desk against the emulated C3 | desk == chip on all 6 prints (every 50 latches); 0 float instructions; textbook 24.5 ms, ours 21.4 ms, drawing 12.9 ms, worst 59.2 ms (emulator counts) |

`check_all.sh` re-runs the desk rows; `CHIP=1 bash check_all.sh` re-runs the two chip rows. Not run for this repo: the
emulated chip for the other eight demos (each README carries the project's own record), the board builds, any board.
