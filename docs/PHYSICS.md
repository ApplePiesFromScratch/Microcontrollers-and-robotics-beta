# A physics map: the textbook laws this repo runs, as relations under constraint

Each row: the law as taught, what the rebuild holds instead, the books checked, where it runs, what was checked, and
what is not claimed. A row is a map, not a claim about what the world is. A board report or a better witness can move
any row; open it in `REVIEW.md`.

## 1. The common grammar

- **State** is whole numbers over named bottoms. **Change** is a rate latched into a value, or an amount moved from one
  place to another. No quantity is a function of t; a step is a count.
- **Letters are verbs** (MOVE, PUSH, DRAG, EXCHANGE, LIFT, FEED, VENT, LATCH, TURN). **Nouns are readings** at stated
  cuts (a plume, a swirl, a temperature), and no rule reads a reading.
- **Books:** each kept quantity has an exact whole-number equation checked every step.
- **Constants** are readings taken in one regime. James: "carrying constants holds risk of reification in extreme
  dynamics." Name the regime beside each constant; expect it to move outside that regime.
- **Ratios as first order only** (James). A ratio of rates answers a first-order question. A ratio of ratios is not a
  deeper property.

## 2. The rows

| law as taught | held instead | books | runs in | checked | not claimed |
|---|---|---|---|---|---|
| continuity, ∂ρ/∂t + ∇·(ρu) = 0 | stuff moved by MOVE (exact bilinear split, leftover to the biggest share) | stuff = start, every step | `demos/nagare` | N1 over 20,000 latches; desk == chip | that ours equals Navier–Stokes |
| momentum (Navier–Stokes) | PUSH (crowding becomes rate at a face), DRAG (neighbours trade rate), EXCHANGE, walls booked | each component = start + given by walls | `demos/nagare` | N1, N3 (both turnings > 5% of loops by 3,000 latches) | a viscosity or Reynolds number for ours |
| energy, buoyancy (Boussinesq) | warmth moved; LIFT against the box as a whole | warmth = start + fed − vented | `demos/nagare` | N2 failed as written (A1); N2b, N2c with puffs | that the steady heater rises: it did not, recorded |
| the textbook beside it | stable fluids in Q16 (Stam 1999), DT 1/4, 20 Jacobi sweeps | measured, not judged: continuity residual, heat gap | `demos/nagare` left pane | a double-precision run agrees to about 1% | that Q16 is the textbook exactly |
| the wave equation | leapfrog in int16, nine-point Laplacian, speed slowed toward banks | the way back equals the way out; flat at the end | `demos/minamo` | 1,243 of 1,243 latches matched; homes 1 (desk, re-run here; emulator per README) | dispersion matching the continuum |
| Newton for many bodies | next = 2 now − before + push(now): soft contact, gravity, walls | the whole state returns at step 0 | `demos/tide` SHATTER | 232 of 232 home (desk, re-run here) | that soft contact is a real collision law |
| light: inverse square, Lambert, specular | (value; three rates) of each cell's light against the flame's move, from the book's moves | the lever against a full re-cast (lever_check) | `demos/candle` | README's lever witness | physically exact transport; the visible share's rates use one finite step |
| rays and perspective | carry rule on the grid; ray = look + lean × camera; scale PPU / depth | (none: geometry) | `demos/highnoon` | 2,000 frames desk == chip (README) | a camera model beyond the pinhole |
| lift, drag, stall | read from each part's size (a torn wing rolls the jet) | part ledger: load, tears, ground | `demos/ace4` | desk runs here; DAMAGE line on the emulator (not run here) | aerodynamic coefficients |
| guidance | turn by N times the line-of-sight turn (proportional navigation) | (none) | `demos/ace4` | PURE_PURSUIT beside it, not run here | that N = 4 is best |
| diffusion | transfer a share of each face's difference | total held, no negatives | `practice/p05` | 2,000 steps | that holding the total makes it right (the planted fault holds it while half the places go negative) |
| a pendulum's control | no fixed gains: pull from the controller's own recent error | (project) | PENDULUM (project) | the deaf control twin lost it 14 times | the refused word that README used for it (METHOD section 14) |

## 3. How to add a row

1. Take the law as taught and list what it reifies (`docs/DEREIFICATION.md`).
2. Write letters that move kept quantities, and the books each one keeps.
3. Write kill lines before running, including a control that must break the books.
4. Run desk and emulated chip; put the textbook method beside yours on the same screen if you can (NAGARE).
5. Fill the row: books, where it runs, what was checked (labelled desk, emulator, board), and what is not claimed.

## 4. Two cuts on every reading here

A plume, a turning count, a frame time, a match count: each was read at a stated cut (loop size, grain, frame number,
emulator instruction count). Before you quote one, say the cut. Before you believe one, take it at a second cut.
