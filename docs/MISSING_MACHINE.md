# The missing simple machine: the lever, with its carry

The simple machine computing doesn't have is the **lever**: a stored ratio between a change here and a change there, carried with every value, together with the set of sources the value has ever touched. A computer holds values (nouns). Every assignment throws away how the value moves with what made it (the verbs). Most of the tooling built since then is a reconstruction of the relation that assignment threw away.

`python3 lever.py` (beside this file; exact fractions, one pass) runs a ten-line quote calculation. From the same reading it answers four questions that computing answers with four separate machines:

| question | the field that answers it today | what it keeps | in the reading |
|---|---|---|---|
| after X changes, must this output be recomputed? | build systems, caches, reactive UIs | a dependency set | X is latched (not absent) |
| does a secret reach this output? | taint tracking, information-flow security | a dependency set | the secret is latched |
| which input moves this most, here? | sensitivity analysis, backprop, finance "greeks" | a gradient | the rate slot; elasticity beside it |
| why is this value what it is? | debuggers, lineage, blame | a trace | the moving sources |

## Why each shadow fails alone

Both cases are in the script's output:
- `total` touched `margin_floor` through a check multiplied away. A taint tracker (set only) reports a leak. The rate is exactly 0.
- `fee` enters as `fee*fee` at fee = 0. A gradient (rate only) says fee doesn't matter. But fee is still, not absent: total moves once fee leaves 0.

The book already has the distinction both shadows lose: the three zeros of a slot (CI 45).
- **absent:** never latched;
- **still:** latched, with rate 0 here;
- **moving:** latched, with rate r.

A taint tracker keeps only latched or not. A gradient keeps only the rate. A cache keeps only "absent" as safe. One slot with three states answers all of them, and keeping it costs one pass.

## The other half: the carry

Computing's second silent drop is the remainder:
- division throws it away;
- a float throws away bits every step;
- assignment throws away the old value.

Undo stacks, version control, logs, event sourcing, backups and replay debuggers all rebuild what was dropped. TIDE showed the other way: when no instruction loses a bit, rewinding 360 steps costs 213 bytes of hand input against 28 MB of snapshots.

So the machine is one register that:
- holds (value; rate per source; latched set);
- changes only by transfer, with its remainder carried;
- refuses by name where there is nothing to land on.

## The pattern behind it

The industry joke names three hard things: cache invalidation, naming, and off-by-one errors. Read as cuts (METHOD §3), all three are the same kind of thing:
- **Cache invalidation** is not keeping which outputs latched which inputs.
- **Naming:** `rate(rope, A)` flips sign with the naming of the ends; `grow` doesn't (CI 40).
- **Off-by-one** is a landing at a boundary: counting posts where latches were meant.

That is a mapping, not a proof.

Each field rebuilt one projection of the slot — a boolean, a set, a timestamp, a gradient — and treats its problem as its own. That is the Dunning-Kruger shape: each projection is competent inside its own cut and blind to the others.

## Known, and not claimed

Forward-mode carried derivatives (dual numbers), taint analysis, incremental computation and reversible computing each exist, and their parts are not new here. The claim is narrower: they are one slot with three states plus a carry, and building them as separate tools is the gap.

## Limits and kill lines

- **The rate is local.** "Still" does not mean "won't change". The cache must use latched, not rate.
- **Latched is conservative.** `x - x` latches x though it is constant everywhere.
- **Cost.** It is one slot per source. Programs with many sources need sparse slots.
- **Kill lines:**
  - a question among the four that one pass can't answer;
  - a case where absent, still or moving gives the wrong call for cache, leak, sensitivity or blame.
