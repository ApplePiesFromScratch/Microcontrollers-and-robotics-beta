# Operating systems in this method: OS 2 → OS 3 → OS 4 → IRORI

Four systems, each built on the last, each answering one question the previous one held as a property. The sketches for
OS 4 (C3 side) and IRORI are in `demos/`; OS 2 and OS 3 are in `demos/os4/bench/base/` (OS 4 was made from them).

## 1. The line, one question each

| system | board | the question it answered | what it changed |
|---|---|---|---|
| OS 2 | Uno, SH1106 | can a machine with readings for registers run user programs on 2 KB? | a VM of two-byte steps, 32 moves, eight slots a program; each slot a reading (value; rate) over one shared bottom in lowest terms; programs kept in EEPROM |
| OS 3 | C3, ST7789 | can memory say WHEN instead of NO? | windows over an arena that answers placed / held with a promise / refused (theta); files kept twice with a mark and a sum; a ghost hand that plays the OS; a second implementation in Python checked 20,008 programs slot for slot |
| OS 4 | both | is a budget a property of a program? | no budgets, no priorities: the flux (useful pattern) schedules; one kernel for both chips; step weights measured in beats; refusals written on the bar |
| IRORI 2 | C3 | can the same method carry an everyday OS for projects and cyberdecks? | twelve apps at once; one owner a pin, computed; the clock only as its change; settings kept twice and mended; hands as roles; safe start after troubled resets; 46 self-checks, 12 of them planted faults |

## 2. What each step taught (from the project's READMEs and INSIGHTS)

- **A budget is a relation** between loaded history and the field, not a number a program has (OS 4 I1). One hog
  stretched the Uno frame to 58.6 ms under OS 2's step budgets.
- **A step is not a unit.** LINE weighs 61 beats, JMP 2. Steps as the unit failed on the chip's timing; the unit became
  beats, measured on the chip by Timer1 around 31,962 single steps, plus a stated 25 %.
- **A width is a representation, not the chip** (OS 4 I4). The Uno build keeps 32-bit tops now, and reaches the book's
  fourth root step 665857/470832 where OS 2 said WIDE.
- **A refusal nobody can read is a silent refusal.** OS 3 refused a window for 150 frames without a sign; OS 4 writes it.
- **The books guard conservation, not liveness.** A rule set froze every place with field + every L = G0 shut (OS 4 E2).
- **A click is a relation between two frames**, not a state (OS 4 E7).
- **The instrument changes what it measures:** OS 4's cost bench overflowed the Uno's RAM and its stack wrote over the
  bench's counters (E6).
- **A board map is a relation asked every time** (IRORI's `pin_user`).
- **The clock is read only as its change** (IRORI: `dclock = now − clock_last`; every timer counts down by it).

## 3. Building an OS this way: the card

1. **Kernel first, in one file shared by every build** (desk, emulator, each board). Two chips are two witnesses only if
   they run one kernel.
2. **State as readings over named bottoms;** refuse WIDE rather than wrap.
3. **Schedule by the flux, not by budgets:** each place carries L; the soonest due runs (min); a step pushes its due mark
   by weight × (C + L + drag) (+); drag found by shared cells; past THETA, reconfigure. Books every latch.
4. **Memory that says when:** placed / held with a promise / refused by name; dry-run any plan that moves blocks.
5. **Keep everything twice with a sum,** and a keeper that mends one copy a turn. Plant a spoiled copy to see it mend.
6. **Hands as readings turned into events by roles;** count presses by interrupt, debounce by time, never lose a tap
   shorter than a frame.
7. **Show every refusal** on the glass.
8. **A ghost that plays the OS,** so the bench can drive desk and chip with the same hand.
9. **A second implementation** of the machine, written from the rule, run against the first on thousands of programs.
10. **Self-test with planted faults,** and a safe start that a bad setting cannot lock out.

## 4. Practice

- Read `demos/os4/os4_c3.ino` lessons 3, 10, 11, 15, then `demos/irori/irori_c3.ino` lessons 11, 12, 19.
- Run `demos/irori/bench`: `gcc -O2 -w -o /tmp/isel desk/selftest.c && /tmp/isel`. Then plant a fault of your own in a
  copy of the core (say, `pin_user` never answering I2C) and see which check catches it. Log the prediction first
  (`practice/p10_predict_then_run`).
