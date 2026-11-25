# Rigor: the procedure every claim in this repo goes through

A claim here is a relation that should hold, with its books, and the list of contexts it was run in. This file is the
procedure, in the order you do it. `docs/METHOD.md` sections 4, 9 and 10 hold the reasons and the project's cases.

## 1. Before any run

1. **Write the claim in one sentence with its scope.** "On the desk and the emulated C3, MINAMO's pond returns to flat
   water at the end of the film" — not "MINAMO is reversible".
2. **Write the kill lines:** the outputs that would show the claim false. Number them (N1, N2, ...). Include a control
   that must fail (NAGARE's N4: one line that sets a quantity instead of moving it must open the books).
3. **Fix every pass mark now.** A mark set after the data describes the data.
4. **Seal it:** `python3 tools/seal.py path/KILL_LINES.md`. The ledger records the hash and the order.
5. **Seal a witness set** if the claim is tuned on data: hold some back, read it once (`practice/p07_kill_line`).
6. **Name what you knew beforehand** ("I expected 16:4:1 from lattice-fluid practice; here it is not assumed").

## 2. During the runs

7. **Take every reading at two cuts** (seed, direction, mirror, naming, scale, grain, loop size, number system, step
   shape, compiler, machine). A reading that moves with a cut belongs to the cut.
8. **Count witnesses by kind, not runs.** A thousand runs in one context is one witness. The kinds: a second
   implementation; exact fractions; desk against emulated chip; float beside it; a sealed set; a mirror test; an
   ablation; a control twin; a unit test against known readings; an outside measure labelled outside; the checker
   checked; a person holding the board.
9. **Every self-check has a planted fault beside it.** A check that has never failed has not been shown to look. Each
   practice checker here has `--planted`; `tools/lessons.py`, `workbook/check_workbook.py --planted` and
   `tools/ledger.py verify` have their planted cases in `check_all.sh`.
10. **Desk == emulated chip, byte for byte.** Build each sketch two ways from one source: a desk program with the panel
    decoded from the sketch's own command stream, and the real chip image in Espressif's QEMU (`-icount 3`). Compare a
    print of every frame. IDENTICAL, or the work stops. Emulator frame times are instruction counts; say so.
11. **When two witnesses disagree, the disagreement does not say which is wrong.** The suspect list is open: source,
    compiler, instrument, the desk itself, a 16-bit draw, argument order, the input's reading, a binary written to a
    path that was a directory. Hold the context and step to the first differing line.
12. **Test at the cut where the failure lives.** Floats came home 200 of 200 after one step and 0 of 20 after 300.

## 3. When something fails

13. **Log it, append-only:** `python3 tools/ledger.py add fail "..."`. Nothing is erased; a correction is a new event
    that names the old one.
14. **Intervene on one relation at a time, least-witnessed first:** usually the newest instrument, then the
    implementation, then the claim. Read before and after.
15. **Amend before the read it governs.** Write the amendment (what failed, why, what changes, which kill lines are
    re-read), seal it, then read. NAGARE's A1 to A4 are the model: two kill lines failed as written and stayed failed in
    the record; the scene changed; a tuning was named as a tuning.
16. **At L >= C, reconfigure; do not patch.** Keep the core that needed no patch, vent the rest as a separate claim at
    L = 0. Effort already spent is not a term.
17. **Re-test everything after any alteration.**

## 4. Reporting

18. **Run, and not run, in the same document.** Label every number desk, emulator or board, and take it from tool output.
19. **Tier every claim:** FORCED (follows from fixed rules, checked every step), EMPIRICAL (measured), CONDITIONAL
    (holds if a stated premise holds), STIPULATED (chosen, and the choice named), PRESUMED (a citation, priced at zero),
    UNPAID ("it looks right", listed, never hidden). No claim is stronger than what it rests on. Record EMPIRICAL as
    absent when nothing ran on a board.
20. **A board report outranks the emulator and every model.** Log it as open and look for what the emulator does not
    model. Do not argue with it.
21. **Never claim "impossible".** Say what was built, how it was checked, and which parts are known techniques.
22. **Prior art, named,** and the narrow claim of what is new. Not finding it is not proof it does not exist.

## 5. The ledger

`ledger/ledger.jsonl`, one JSON line per event, each chained to the last by its hash (`tools/ledger.py`). Kinds:
claim, killline, seal, amend, predict, run, result, witness, fail, intervene, reconfigure, self, read. Your own changes
of approach are `self` events in the same ledger as the model's. Read the ledger's shape, not your memory of it: where
two failures in a row have the same shape, the habit has to change, not the model.

Limits, stated: the chain catches a partial edit; it does not catch a full rewrite of a local file. Anchor it by pushing
it, or by pasting its last hash somewhere you do not control.
