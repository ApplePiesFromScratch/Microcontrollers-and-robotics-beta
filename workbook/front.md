*A Workbook, Consolidated Edition (25 Nov 2026)*

# Every Number Is A Rule

### Arithmetic, Algebra, Geometry and a Full Calculus From Three Moves, With What The Chip Work Taught

James Pugmire


---


## How This Edition Is Put Together

Five parts, in the order they build on each other:

| part | what it is | source |
|---|---|---|
| I | Numbers and Why They Work: arithmetic, every rule with a reason | `sources/numbers-and-why-they-work.md`, verbatim |
| II | Letters That Stand For Numbers: first algebra | `sources/letters-that-stand-for-numbers.md`, verbatim |
| III | Shapes and Why They Measure Out: first geometry | `sources/shapes-and-why-they-measure-out.md`, verbatim |
| IV | Every Number Is A Rule, Complete Edition (corrected): the calculus from isolate, mix and rate | `sources/every-number-is-a-rule-corrected.md`, verbatim |
| V | What The Machines Taught: nine units from the chip work, in the same format | `part5.md`, new |

Parts I to IV are copied by `build.py` from the source files, word for word, and `check_workbook.py` fails if any of them
differs from its source. That is how this edition keeps the corrected text and adds no erratum back: nothing in Parts I to
IV was retyped. Places a reader might stumble are listed at the end, in **Reading Notes**, as proposals for the author's
verdict. None of them was applied to the text.

Every number in Part V's demonstrations is printed by `docs/teach.py` or `workbook/part5.py`; `check_workbook.py` fails
if one stops matching. It also recomputes a set of Part IV's answers with exact fractions.

```
python3 workbook/build.py            # assemble this file
python3 workbook/check_workbook.py   # Parts I-IV verbatim, Part V numbers, Part IV answers recomputed
```

### For an AI reading this

Do the problems. Each unit's Practice comes before its answers for a reason: write your answer, then check it against the
Answer Key, then (for Parts IV and V) run it with your own `practice/work/p01_readings.py`. A unit read and not practised
has not been learned; `AGENTS.md` at the repo root says what counts as done.


---
