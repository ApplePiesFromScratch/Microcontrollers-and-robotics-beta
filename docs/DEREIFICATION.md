# De-reification: turning a scientific description into mechanics you can run and test

A textbook description is a map. It is usually written as things with properties: a signal has a frequency, a fluid has
a viscosity, a controller has gains, time flows. De-reifying it means finding, under each property, the relation it
stands for and the cut it was read at, and then building that relation from the book's moves so it can be run, checked
and broken. Nothing in the result is claimed as what the subject ultimately is. The result is a mechanism that is
reproducible and has kill lines, and it stays a map, open to review (`REVIEW.md`).

One example first (PID control, from METHOD section 5), then the procedure, then the conversions this project made.

## 1. The worked example: PID

| the map | what it reifies | the relation | what the rebuild uses |
|---|---|---|---|
| error e = target − x | (nothing) | a relation between two readings | the gap, a top over the plant's bottom |
| ∫ e dt | time as the variable; dt as a thing | the gap's loaded history | H: each latch deposits the gap into it |
| de/dt | two samples subtracted over dt | the plant's own rate slot | momentum over mass; a target has no rate, so no kick |
| Kp, Ki, Kd | gains as constants | one demand, landed once | (KP·gap + KI·H − KD·p) / (M·KDEN), landed by the mouth at the actuator |
| anti-windup | a fix added afterwards | demand past the available gradient vents history | booked: deposits − vented = H |
| e^(−t/τ) | an exponential in time | a carried transfer | reaches 0 in finitely many latches |

The vent that came out of the books landed on the known best cure (overshoot 65.9, the same as Åström and Hägglund's
cures; naive 240.8). No better control was claimed. What changed: nothing needs dt, float or a tuned fix, and the result
is exact across machines.

## 2. The procedure

1. **Read the target's alphabet whole**, tables and asm included. A skim replaces the rules with a guess about their output.
2. **List the maps the subject is coded in and what each reifies.** Keep a map for what it buys; drop what it says exists.
3. **Separate rules from readings.** A rule may use only rules; a reading may use rules and readings; no rule reads a
   reading. Anything that must act acts on a relation that holds across a band of cuts.
4. **Split every property by what pays for it.** The turning transform: unit turns alone pay for the undo and the books;
   closure pays for the shift and convolution rules; rational closure stops at the quarter turn, so a rational ring is
   a group or closed, never both. The classical theory takes all of it from one assumed angle and cannot see the split.
5. **State the condition, not the method.** "No air through the skin at each patch" is a linear system; solve that.
6. **Choose the construction in which the correction never arises.** HIGH NOON builds each ray as look + lean × camera,
   so no fisheye correction exists to apply.
7. **Rebuild with the original as the witness.** MOLD ran James's original lab headless to 45,000 frames at five
   settings and rebuilt the port until it reproduced them.
8. **Say the cut your build can hold.** The Uno shows fronts on a ring of 64, not swirls, and says why.
9. **Name what you knew beforehand**, then let the tests decide.

**Kill line for the procedure:** a rebuild whose output matches the original but whose rules came from outside the
book's alphabet. EDDY is the recorded case, kept beside the framework as an experiment and retired from it.

## 3. Conversions made in this project

| textbook statement | what it reifies | the relation held instead | where it runs | tier of the rebuild |
|---|---|---|---|---|
| a signal has frequency k | a bin as a property of the readings | one turning rule applied to the readings; bin k is a relation between them | TURNING TRANSFORM (project) | FORCED for the undo and books; EMPIRICAL for spill |
| a tone has frequency f | f as a property of the sound | turn per latch against the latch rate; the same rule sings another note at another latch rate | SOUND (project) | EMPIRICAL |
| sin, cos of an angle | an angle as a thing | a heading (P : Q): two whole numbers, exact on the circle | every C3 demo's ring; `practice/p03` | FORCED (c² + s² = d²) |
| a limit | a target being approached | a rule: tolerance in, stage and proven gap out | workbook Part IV, Unit 15 | FORCED |
| dx/dt | time as the variable | the rate slot of a reading, latched into the value | every sketch here | FORCED |
| ∂/∂t + Navier–Stokes | a continuum and time as variables | hand-ons between neighbours: MOVE, PUSH, DRAG, EXCHANGE, LIFT, FEED/VENT, each a transfer | `demos/nagare` | FORCED books; EMPIRICAL plume and turning |
| the wave equation | a continuous field in time | h(s+1) = 2h(s) − h(s−1) + F(h(s)) + S(s), its own inverse | `demos/minamo` | FORCED return |
| "chaos" | a property of a system | the rounding carried by a rule, read by an eye that cannot follow it; the whole-number version returns exactly | TIDE's FORGET beside SHATTER | FORCED (whole numbers return; float does not) |
| heat as loss, entropy rising | loss as a property of the process | a reading at a grain: the lit count never changes, and the grain un-mixes exactly | UNO_CRAFT, WAKE (project) | FORCED (permutations) |
| erasing a bit costs energy (computing as lossy) | loss as a property of computing | a lost bit is a count of collisions; refuse two-to-one instructions and nothing is lost | `demos/tide` | FORCED |
| a controller's fixed gains | constants as properties | the brain's own recent error ("why does a recursive feedback system have constants?") | PENDULUM (project) | EMPIRICAL |
| backpropagation | a direction as part of learning | the anchor's place: the same rates carried forward or swept back | FORWARD (project) | FORCED (equal on 17 of 17) |
| attention weights sum to 1 | landing as a property of the model | a softmax has no theta; the trained model made its own refusal on marker pieces | INSIDE (project) | EMPIRICAL (116 of 144 heads) |
| a budget, a priority | capacity as a property of a program | loaded history against the field: due += weight × (C + L + drag) | `demos/os4` | FORCED books |
| a timer of 500 ms | time as a property of the wait | a countdown of latches against the clock's change | `demos/irori` | FORCED |
| "pin 8 is I2C" | a role as a property of a pin | an owner computed from the settings each time | `demos/irori` | FORCED (one owner a pin) |
| hit points | health as a number a thing has | a part ledger: load past a threshold reconfigures the part | `demos/ace4` | STIPULATED thresholds; the reconfiguration is the rule |
| pursuit: aim at the target | where the target is | how fast the line of sight turns | `demos/ace4` | known (proportional navigation); whole-number rebuild |
| a pixel's light from the light model | recomputing a property each frame | a reading with rates against the flame's move, read with four multiplies | `demos/candle` | one STIPULATED finite step; the rest FORCED to its bottom |

## 4. "Intrinsic structure of ad hoc entities", converted back to relational affect

The project's definition (METHOD section 3): *relational affect* is how a relation shows up at a particular cut (one
observer, instrument, naming, grain). *Intrinsic structure* is what something has whatever the cut. The conflation is
writing a reading taken at one cut as a property of the thing: dropping the cut and keeping the number.

To convert one back, run this on it:

1. Write the property as it is usually stated: "the swirl has strength S".
2. Find every cut in it: loop size, grain, which reading (flotsam or turning), seed, naming.
3. Move each cut in turn and record which numbers moved (two cuts at least; `practice/p06_two_cuts`).
4. Write what is left, with every cut named. The shape (a template, not a measured sentence): "turning, counted round
   loops of size k, read T at latch n, from the turning reading and not the flotsam; it holds over loop sizes a to b
   and reads neither at the whole plane, which the periodic closure forces." FLOWING (project) has the measured case.
5. Tier it and give it a kill line.

The value slot is the reading most exposed to the cut, and the one most often taken for the thing.

## 5. Practice

`practice/p08_dereify`: pick a map you know (an angle, a percentage, a temperature, a timer), write the four-column table
of section 1 for it, say what pays for each property you keep, give a kill line and name what is known. Then open an
entry in `REVIEW.md` so a person can read it.
