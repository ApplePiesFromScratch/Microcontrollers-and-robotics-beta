# The useful pattern as mechanics

`docs/USEFUL_PATTERN.md` is James's text. This file reads it as rules a build can run and a bench can check. Each clause
gets: the rule in the book's words, the books that check it, the sketch where it runs, and the kill line. Nothing here
claims that the pattern is how anything ultimately is (kind 3). It claims that these rules, run, give the readings listed
(kinds 1 and 2), and it is a map, always up for review (`REVIEW.md`).

## 1. Six clauses, as rules

| clause | the rule | books | runs in | kill line |
|---|---|---|---|---|
| UP1 loaded history | each pattern carries L, the history its propagation laid; every step moves its weight from the field into L | field + every L = G0 | OS 4's flux (`demos/os4`, section 15) | a latch where the sum opens |
| UP2 demand set by history | the cost of the next step grows with L: a step pushes the due mark by weight x (C + L + drag) | due marks only move forward | OS 4 | a place whose due mark moves back |
| UP3 simplest propagates fastest | rate of propagation = C / (C + L + D): the lightest history, best supported, goes first | min over due marks picks the next | OS 4 (OR = min); RELATE's learner | a heavier place served before a lighter one at equal due |
| UP4 drag | D is the load of other histories met: found by interaction (a shared cell), never declared | drag computed from contact each latch | OS 4's k_drag | drag present with no shared cell |
| UP5 reconfigure past the boundary | when demand exceeds the available gradient, keep a core (KAPPA/16 of L), vent the rest, start at the front | vented + kept = L before | OS 4 past THETA; ACE 4's parts (keep 2/5, tear the rest) | a place past THETA that keeps all of L |
| UP6 vent simplicity | a pattern that gives back vents a share of L to the field; insufficient venting cascades | vented amount enters the field | OS 4 (SHOW, WAIT); LACE's forgetting carry | a vent that leaves the field unchanged |

**The arithmetic underneath** is the book's: AND as +, OR as min (FLUX, OS 4); every quantity a whole number moved, not
set; refusal by name (theta) when nothing is available to land on.

## 2. Where the clauses produced readings worth checking

- **OS 4:** no budgets and no priorities. A program that gives back stays light and is served first; one that never
  gives back takes what is left, then reconfigures. The latch has a fixed supply, so no program can stretch the frame.
  The README's runs: the flux on the Uno and the C3, books shut every latch.
- **ACE 4:** a jet is what its parts still carry. Load past a part's threshold reconfigures the part (UP5), the torn
  piece falls and becomes ground (the vent, UP6), and flight reads the parts that remain.
- **LACE:** laying (UP1) against a forgetting carry (UP6), with theta as the wall. Removing theta turned the network to
  haze at every setting on the C3 build: the ablation that shows which part does the work.
- **NAGARE's LIFT** relates a place's warmth to the box as a whole (the context), not to a fixed zero. A steady heater
  warmed the context too, and the plume failed to rise (kill line N2 failed, kept in AMENDMENTS A1). The pattern's
  "relative to available gradients" ran literally, and the reading came out different from the textbook's.

## 3. OS logic (ontic structural logic), as a procedure

James's text names three inference shapes: "Because ->" (start from the conclusion), "IF ->" (a chain built relationally
but described as discrete things with intrinsic properties), and "<=> IF ->" (map what must hold for the premise to be
stable, by a method consistent with the premise). As a procedure a build can follow:

1. State the relation, not the thing: "warmth relative to the box", not "hot stuff".
2. Map its loaded history: what came before and stayed stable (the books, the ledger).
3. Map the band of conditions it stays stable in (two cuts at least: METHOD section 4).
4. Ask three questions of your own labels: does a label stand in for a thing? does the account add anything it does not
   use (parsimony)? what output would show it false (a kill line)?

## 4. The paradox breakdown, as a checklist for your own writing

James's five steps, used as a lint on a draft:

| step | what to look for in your text | the fix |
|---|---|---|
| 1 reify a process | a verb turned noun: "the swirl", "the intelligence", "the budget" | name the relation and the cut |
| 2 define it by relations | the definition uses comparisons, couplings, places | keep those; they are the content |
| 3 assert it is intrinsic | "really", "just has", "in itself", "the true value" | delete, or name which cut |
| 4 the contradiction | the thing is said to be non-relational and is only described relationally | the relational description was the answer |
| 5 meta-moves | "in one sense... in another sense", "profound", "mystery" | run a test at two cuts instead |

`tools/wordlint.py` catches the words of step 1 that this project refuses. It cannot catch steps 3 to 5; a reader must.
LINEAGE recorded a word reader flagging denials ("not discrete entities") as assertions: 42 of 51 flags. A word without
its sentence is a reading at the wrong cut.

## 5. What this file does not claim

- That the six clauses are the only reading of James's text. They are the reading the builds above run.
- That any reading here extends to the fields the text speaks of beyond these builds (cosmology, measurement, mind).
  Those are not tested in this repo.
- Tiers: the books in section 1 are FORCED where the code moves amounts (checked every latch in OS 4 and NAGARE); the
  readings in section 2 are EMPIRICAL on the desk and emulated chip; the mapping from James's words to these rules is
  STIPULATED, and James's reading of it outranks this file.
