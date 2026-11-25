# Review: the map is always up for review

Everything in this repo is a map: a description of relations under constraint, written so it can be run and broken. When
you think part of it is wrong, write the disagreement here as a claim someone can check. Do not edit a verbatim source
(`workbook/sources/`, `docs/USEFUL_PATTERN.md`, any demo's `original/`).

## How to open an entry

Copy this block to the end of the file and fill it in:

```
### R<n>. <one line: what you think is wrong>
- where: <file and line, or doc and section>
- claim: <the relation you think holds instead, with its scope>
- kill line: <the output that would show your claim false>
- witness: <what you ran, or "none yet">; ledger event <n>
- tier: FORCED | EMPIRICAL | CONDITIONAL | STIPULATED | PRESUMED | UNPAID
- status: open
```

Verdicts: James's outrank the team's; the team's outrank an AI's. A closed entry stays here with its verdict.

## Open entries

### R1. Workbook reading notes N1-N6 await the author's verdict
- where: `workbook/notes.md` (end of the consolidated workbook)
- claim: six places a reader may stumble, each with a proposal; none applied
- kill line: the author's verdict that a note misreads the text
- witness: `workbook/check_workbook.py` recomputes Part IV answers (21 of 21 as written)
- tier: STIPULATED (readings of a text)
- status: open

### R2. The p11 reconfiguration (CS tied low, RST by RC) is unrun on a board
- where: `docs/BOARD_MAPPING.md` section 4; `practice/p11_board_map`
- claim: the ST7789 runs with CS tied to ground and RST by an RC plus software reset 0x01, freeing GPIO 7 and 10
- kill line: a panel that stays dark or garbled after power-up with that wiring
- witness: none yet (checker PASS is against listed facts only)
- tier: UNPAID
- status: open

### R3. The lesson blocks' TRY predictions are unrun
- where: every `demos/*/*.ino`
- claim: each TRY line's prediction
- kill line: a TRY run whose output contradicts its prediction (log it with `practice/p10`)
- witness: none yet
- tier: UNPAID
- status: open
