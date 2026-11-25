# METHOD: the whole system, taught to a new instance

At x = 3, `rate(x·x, x)` reads 6 whether x is seeded with rate 1, 7/2 or −1. The rate slot inside x·x reads 6, 21 and −6 for those seeds. The slot's number belongs to the cut you chose; the answer belongs to the relation. Everything in this project is the habit of telling those two apart before you build on either, and of keeping books that show nothing was made or lost on the way. (`python3 teach.py 1`)

This file teaches that habit as a method you can use on any subject: a chip, a fluid, a voice, a scheduler, a walker, a proof. The builds appear only as cases. Read it in order. Run each lesson where it is named. Do the course in §16 before building anything.

**What this file stands on:**
- the book, *Every Number Is A Rule* (James Pugmire);
- all 54 project docs, read whole on 19 Nov 2026;
- `teach.py`, beside this file (`docs/teach.py`), the only source of the numbers in the `text` blocks below;
- `check_method.py` (`docs/check_method.py`), which fails if any quoted line stops appearing in `teach.py`'s output.

> Repo note (25 Nov 2026): this file is the project's METHOD.md of 19 Nov, unchanged except these two path lines and
> section 18, added at the end: what the project learned between 19 and 25 Nov. "CI n" and doc names refer to project docs
> that are not all in this repo; the repo's own docs are listed in docs/START.md.

**Reference style:** "(CI 39)" is entry 39 of `CLAUDE_INSIGHTS.md`; a bare name such as PROCESS or SOUND is that doc in the project.

---

## 1. Read the ground first: the book

Seed x with 7/2 instead of 1 and every rate question gives the same answer, because the seed sits on the top and the bottom of the division alike (Unit 5). Before you write a line of code in this project, read the book whole. It is short, and every move below comes from it.

```text
seed 1: x*x = (9; 6)  rate(x*x, x) = 6
seed 7/2: x*x = (9; 21)  rate(x*x, x) = 6
seed -1: x*x = (9; -6)  rate(x*x, x) = 6
x*x*x at 3 = (27; 27)
rate(x*x, still 5) = THETA
```

**The alphabet.** A reading is a pair `(value; rate)` of fractions. Nothing is adjoined to it unless a unit says so and names what the addition buys.

**The moves:**
- `isolate(v, r)` builds a pair; `isolate(v)` seeds rate 1, meaning "this is what the others are compared to".
- `mix` combines two pairs: value v1·v2, rate r1·v2 + v1·r2. The two terms are the two strips of a growing rectangle (Unit 3).
- `rate(a, b)` divides a's rate slot by b's. It never reads a value.
- Adding and subtracting work piece by piece.
- Division is **derived**: it must undo mix, which forces q = v1/v2 and s = (r1·v2 − r2·v1)/v2² (Unit 8). It is not a fourth move.

**What the book marks, and you must keep marking:**

| mark | meaning | example in the book |
|---|---|---|
| forced | once the earlier rules are fixed, it can't come out any other way | division |
| "a choice, not a discovery" | picked because it's useful; what it buys is stated | mix; E(0) = 1 and rate(E) = E; naming the rule √2 |
| theta | a well-formed question with nothing to land on, refused by name | rate against a still pair; "zero apples, ten apples" |
| a rule | a finite procedure: tolerance in, stage and proven gap out | √2 by halving and by averaging; e with shortfall exactly 1/n! |
| a rule label | which rule reaches a number, nothing more | "algebraic", "transcendental" (Unit 13) |
| kind 1, 2, 3 | checked by calculation; claimed about the world through a model; claimed about what ultimately is | the book makes kinds 1 and 2 only (Unit 15) |

**The root of everything in §3.** Unit 15 names the error this whole method is built against: a kind 1 fact dressed in words that suggest a kind 3 claim, "smuggled past the reader because the two sentences sound similar". "Transcendental" sounds like depth; it is a checked fact about which rule reaches a number. "Limit" sounds like approach; what is there is a rule with a requested precision.

**No independent variable.** The project's first rule. Nothing is a function of t. Things change because a rate is latched into a value, one step at a time. A counter is fine, because it counts latches. `pos = f(t)` is not.

**Use the book's words.** Reading, value, rate slot, isolate, mix, rate, theta, rule, carry, latch, heading, cut, mouth. Do not rename them as automatic differentiation, dual numbers, limits or reals. Where the arithmetic coincides with an outside field, say which part is known and leave the words alone (START_HERE §1).

**Read critically.** Unit 13's Demonstration 2 says "Unit 4's shortfall construction"; the construction is Unit 10's. CI 95 found that `when` written as −r/v was the wrong way up; the book's own example gives −v/r. The material is a witness like any other.

**Kill line:** a number in your work that is asserted without the steps that produce it.

## 2. The words you will think in

Each word names a relation with a rule. None names a thing.

| word | what it means here | its rule or test |
|---|---|---|
| **reading** | a value held with its rate slots, latched on the same cut | (value; r1..rn), tops over one shared bottom |
| **latch** | one step of state | value += rate; every counter counts latches |
| **carry** | the remainder of a division, kept for the next latch | `acc += n; while (acc >= d) { acc -= d; emit; }` |
| **mouth** | the one place a fraction lands on a whole number | nearest, exact half to even, mouth(−x) = −mouth(x), at the output only |
| **bottom** | the named denominator a quantity is held over | 4, 16, 256, 65536; or the problem's own common multiple |
| **heading** | a direction as two whole numbers (P : Q) | unit (Q²−P², 2PQ) over P²+Q², exact on the circle |
| **cut** | what an observer or instrument imposes to take a reading | origin, direction, scale, mirror, seed, naming, grain, loop size, number system, step shape, listener, machine |
| **theta** | a named refusal: nothing to land on | a ratio to a still slot; a divisor of value 0; contact |
| **NotASlot** | a question the alphabet does not contain | slot 0; slot 5 of 4; readings from different cuts (CI 35) |
| **WIDE** | the answer doesn't fit its container | the slot is left alone; nothing wraps |
| **books** | an exact whole-number equation, checked every step | total water constant; field + every L = G0 |
| **claim** | a relation that should hold, stated with its books | not "the formula is correct": "this equation held in these contexts" |
| **witness** | a context a claim is run in | a second build, exact fractions, desk and chip, a sealed set, a person |
| **support C** | how many *distinct* witnesses a claim holds in | a thousand runs in one context count as one |
| **load L** | alterations to what a claim says or needs | a mechanism added, a scope narrowed, an exception |
| **drag D** | the load of what a claim rests on | |
| **tier** | what pays for a claim | FORCED, EMPIRICAL, CONDITIONAL, STIPULATED, PRESUMED, UNPAID |
| **kill line** | the output that would show a claim false, written before the run | |

**Words to refuse** (ALPHABET's lint): living, alive, organism, creature, being (as a noun), entity, chaos, intelligence. Replace each with the relation it stood for. "Chaos" becomes "hard to follow by eye", which is about us (James). A word inside quotation marks is only named, and is allowed.

**Words that change the claim:**
- "impossible otherwise": never claimed. Say what was built and how it was checked (CI 71, 82).
- "works on the board": belongs to the person holding the board.
- "the" before a reading ("the swirl", "the accent"): check which cut made it (§3).

## 3. The core understanding: relational affect taken for intrinsic structure

Two cars: A at 0 with rate 5, B at 10 with rate −3. Ask for `rate(rope, A)` with the rope named A−B and you get 8/5; name it B−A and you get −8/5. Ask for `grow` (the rope's rate over its own value) and you get −4/5 both ways.

```text
A-B: rope (-10; 8), rate(rope, A) = 8/5, grow = rope rate / rope value = -4/5
B-A: rope (10; -8), rate(rope, A) = -8/5, grow = rope rate / rope value = -4/5
direction cut flipped: rope (-10; 8), rate(B, A) = -3/5, grow = -4/5
seed -1: rope (10; 8), rate(B, A) = -3/5, grow = 4/5
as named (B-A): rope (10; -8), rate(B, A) = -3/5, grow = -4/5
```

Read the five lines as a map:
- `rate(rope, A)` moves when you rename the ends.
- `grow` doesn't move with the names or with the direction cut, but does move with the seed, because it counts per latch.
- `rate(B, A)` moves with none of the three.

Each answer is a function of the relation **and** of the particular cuts listed against it, never of every cut. CI 39 measured the full map on 2,991 random pairs in a plane, changing one cut at a time:

| answer | moves with |
|---|---|
| a slot's value | origin, heading, scale, mirror |
| a slot's rate | heading, scale, mirror, seed |
| `rate(A, B)` | mirror only (its across part) |
| `grow`, along part | seed only |
| `grow`, across part | mirror, seed |
| when they meet | seed only |
| miss² | length scale only |

**The definition.** *Relational affect* is how a relation shows up at a particular cut: through one observer, one instrument, one naming, one grain. *Intrinsic structure* is what a thing has whatever the cut. The conflation is writing a reading taken at one cut as a property of the thing: dropping the cut argument and keeping the number.

The value slot is the reading most exposed to the cut, and it is the one most often taken for the thing.

**The cases, from every kind of subject the project touched:**

| written as a property | the cut it depends on | the witness that showed it | doc |
|---|---|---|---|
| a sign is part of a count | which way the cut points | −4/3 pulses were not the mirror of +4/3 | CI 32–33 |
| "chaos" | our eye | the float version's failure to return was rounding carried by the rule | CI 139 |
| transcendental means deeper | which rule did the sorting | the book's own argument | Unit 13 |
| a tone has frequency f | latch rate against turn per latch | the same rule sings another note at another latch rate | SOUND |
| bin k is a frequency in the signal | one turning rule applied to the readings | spill measures one heading latched 64 times against the ring's 64th step | TT T6 |
| "inverse" | whether the ring closes | the rule run back is exact; the flipped transform is off by 1 unit at N = 64 | TT T5 |
| a program has 64 steps | a stipulated budget | one hog stretched the Uno frame to 58.6 ms | OS4 I1, I3 |
| a step is a unit | what the step did | LINE weighs 61 beats, JMP 2 | OS4 E5 |
| a width belongs to the chip | the slot, a representation | 0 disagreements before WIDE in 70,000 programs | OS4 I4 |
| a click is a state | two frames | a scripted hold ran into the next press | OS4 E7 |
| density is the medium | which reading the rules were fed | v0–v3 froze, every one | RELATIONAL |
| "the swirl" | loop size, and which reading | flotsam swirl and turning swirl have different shapes; turning holds over a band of cuts | FLOWING |
| "neither" at the whole plane | a periodic plane forces it | 0 below the whole-plane loop | FLOWING |
| "the Uno can't" | the 16 × 8 cut | the letters lose 98 % of the activity at that cut | FLOWING_BOARDS |
| "accent" | the listener's map | the two men labelled German score 38 and 12 through jackson's map; everyone scores 44–50 through his own | SOUND |
| a phoneme is a piece | its neighbours | "five" built with nine's vowel was heard as nine 11 of 11 times | SOUND |
| stability | supply, die temperature, stride | the lineage mix shifts with each | VIVARIUM |
| a fixed gain | the brain's own recent error | James: why does a recursive feedback system have constants? | PENDULUM |
| drag | mass error read through a drag-only learner | learned 2,463 / 11 / 3,025 against a true 2,600 | PENDULUM |
| heat is loss | grain size | the lit count never changes, and the grain un-mixes exactly | UNO_CRAFT |
| the edges take turns | a mirror-symmetric relation can't break symmetry | both relations tried stayed in step | WAKE 5 |
| vertical is preferred | steering by heading and pitch has a pole | a direction vector in the cell's own frame | SYNAPSE |
| a variable named `rate` is the rate | the relation the name was meant to carry | the actors fed rate where momentum was due | FLOWING_BOARDS |
| one damping number | what is damped | it killed the swirls; damping only parting and converging kept them | FLOWING_BOARDS |
| the input is 512 | the instrument's reading of 2,500 mV | the chip read 511 | WAKE 6 |

**How the conflation hides in your own processing.** These are the habits the docs caught in the instance writing them. Expect them in yourself:

1. **Noun first.** Storing a reading (density, a swirl, an individual) as an ingredient of the rules (FLOWING §4).
2. **One scale.** One neighbourhood, one loop size, one window (FLOWING §4).
3. **One number system.** Exact fractions as drag; signs compared below the number grain (FLOWING §4).
4. **One step shape.** One straight step where two halves were due (FLOWING §4).
5. **An imported axiom, unchecked.** MEET was incompressibility, so v4 could swirl but never crowd (FLOWING §4).
6. **A description standing in for the thing.** My last engine for the target's alphabet; a skim for the rules; an estimate for a measurement; a plausible display for a settled solve; a switch for a continuous condition (AERO).
7. **A relation treated as a thing.** A budget, an equal step, a width, a stand-in taken for the original, a press as a state (OS4).
8. **A name taken for its relation.** The code had a variable called "rate" (FLOWING_BOARDS).
9. **Two descriptions of one motion that disagree.** Two carries that should be one; a disc and its chords; a voltage and its reading (WAKE).

**Kill line for the core understanding:** a recorded failure in this project that is not a reading at one cut taken as a property. None was found in the four ledgers PROCESS encodes. Find one and this section must change.

## 4. The test for it: take every reading at two cuts

Count the peaks of one turning pair, latched 600 times, and the number depends on the grain you read it at:

```text
same turning, peaks counted at grain 2^k: k=0: 16, k=12: 16, k=16: 32, k=17: 104, k=18: 62, k=19: 31, k=20: 32
```

The turning did the same thing in every column. At fine grains the count is the turning's: 600 latches over 16 peaks is 37.5 latches a turn. At coarse grains the peak rule ("higher than before, not lower than after") counts every stair of the landed staircase. Read at one grain, 104 would have been reported as the subject.

**The rule** (FLOWING §4): no reading is believed until it has been taken at two cuts. Every failure FLOWING lists would have been caught that way, and none was caught by re-reading at the same cut.

**The cuts to change, with what each has caught:**

1. **Seed.** Answers must not move (§1). CI 1: the seed cancels for every expression, 20,000 random C rounds.
2. **Direction.** Point the cut the other way.
3. **Mirror.** WHIRL's mirrored world with the hand flipped is the exact mirror at all 6,000 latches, its whirl count exactly negated. Only then was "circulation follows the hand" worth measuring.
4. **Naming.** Rename the ends (§3).
5. **Scale and grain.** RELATIONAL v3's regime held at 2⁻¹⁶, 2⁻³² and 2⁻⁴⁸; v0's peaks did not (1.886, 2.230, 2.160).
6. **Loop or window size.** A turning holds over a band of loop sizes, ending where the loop takes in the opposite turning (FLOWING).
7. **Number system.** float32 against float64 against whole numbers. A naive PID took different paths in float32 and double: a width the formula never states (LOOP).
8. **Step shape.** One straight flotsam step emptied a turning disc; two half-steps kept 54 of 74 (FLOWING).
9. **Listener.** Each speaker through his own judge and through another's (SOUND).
10. **Machine.** Desk against emulated chip, byte for byte (§11).
11. **When the cut is drawn.** Draw the reading's cut after the act, from the configuration (§11, Goodhart).

**Kill line:** a reading published from one cut.

## 5. De-reifying a system, in order

A worked case first, then the procedure. PID control, as LOOP rebuilt it:

| the map | what it reifies | the relation | what LOOP uses |
|---|---|---|---|
| error e = target − x | — | a relation between two readings | the gap, a top over the plant's bottom |
| ∫ e dt | time as the variable; dt as a thing | the gap's loaded history | H: each latch deposits the gap into it |
| de/dt | two samples subtracted over dt | the plant's own rate slot | momentum over mass; a target has no rate, so no kick |
| Kp, Ki, Kd | gains as constants | one demand, landed once | (KP·gap + KI·H − KD·p) / (M·KDEN), mouthed at the actuator |
| anti-windup | a fix added afterwards | demand past the available gradient vents history | booked: deposits − vented = H |
| e^(−t/τ) | an exponential in time | a carried transfer | reaches 0 in finitely many latches |

The vent that came out of the books landed on the known best cure (overshoot 65.9 against 65.9 for Åström and Hägglund's cures; naive 240.8). No better control was claimed. What changed is that nothing in it needs dt, float or a tuned fix, and it is exact across machines.

**The procedure:**

1. **Read the target's alphabet whole, including what looks like detail.** The first COWTUNNEL 3 was "a picture of a cow over your last sketch" (James). The original's look came from rules in its asm and tables, which were skipped. "Skimming replaces the rules with a guess about their output" (AERO E1, E2).
2. **List the maps the subject is usually coded in and what each reifies.** SOUND's table goes through sin(2πft), Hz, the waveform table, ADSR, the oscillator object, Fourier, 12-TET, the phoneme, the sample, the sample rate, the DAC and "accent as an error". TT and LOOP have the same table for the FFT and for PID. Keep a map for what it buys; drop what it says exists.
3. **Separate rules from readings, and let no rule read a reading.** FLOWING's grammar: a rule may use only rules; a reading may use rules and readings. Anything that must act acts on a relation that holds across a band of cuts, never on one value.
4. **Split every property by what pays for it.** TT is the model case:
   - unit turns alone (c² + s² = d²) pay for the undo and the books;
   - closure pays for the shift and convolution rules;
   - rational closure stops at the quarter turn (Niven), so a rational ring is a group or closed, never both;
   - a count mod 65537 closes where a heading can't, at the price of a bin that is a remainder, not a size.

   The classical theory takes all of it from one assumed angle, so it can't see the split.
5. **State the condition, not the method.** Gauss-Seidel on onsets diverged on overlapping balls. "No air through the skin at each patch" is a linear system, and Kaczmarz solves it live (AERO E3). EDDY's lattice weights came from two conditions, TURN (no preferred direction) and CARRY (no privileged rest); only 16:4:1 passed both in a search to 40.
6. **Choose the construction in which the correction never arises.**
   - A ray built as look + lean × camera returns the square-on depth: no fisheye to correct (HIGHNOON).
   - One idler gear flips the bend, so face angle = lift + 3d − 3d = lift: no sensor (HERON).
   - SEAHORSE's root kinematics is one subtraction, γ = θ0 − Δ.
   - A pose sets where a rate is pulled, never the angle, so every move flows from the last (FISH).
7. **Rebuild with the original as the witness.** MOLD's stages were found by running James's `slime_lab.html` headless to 45,000 frames at five settings. The port was rebuilt until it reproduced them.
8. **Say the cut your build can hold.** The Uno shows FLOWING's fronts on a ring of 64 places, not swirls, and says why. LACE on the Uno states that one-bit memory is not the C3's graded rule.
9. **Name what you knew beforehand.** EDDY: "I expected 16:4:1 and the closed form from lattice-fluid practice. Here neither is assumed." Then let the tests decide.

**Kill line:** a reconstruction whose output matches the original but whose rules came from outside the original's alphabet. EDDY is the recorded case: it is kept beside the framework as an experiment and retired from it as R4 (ALPHABET).

## 6. Holding numbers

Count a 4/3 rate in pulses by carrying the remainder and you are never wrong; count it with a float accumulator and you are wrong on a third of the ticks.

```text
first 24 ticks: 112112112112112112112112; after 100000: count 133333, d*count+acc == n*ticks held every tick; FLOAT accumulator wrong on 33333 ticks
mouth 5/2, 7/2, -5/2, -7/2, 10/3: [2, 4, -2, -4, 3]
mouth mirror fails: 0 of 54009; floor shift (-x>>1) != -(x>>1) on 3000 of 6001
```

**Choose a holding for every quantity before writing code** (FRACTIONS §2):

| holding | use it for | its cost |
|---|---|---|
| exact top/bottom | desk references, proofs | bits grow: 4x(1−x) from 1/3 has a 1,661,954-bit bottom at step 20 |
| tops over a named bottom | every position, speed or unit on a chip | a product has bottom², so land it once before the next multiply |
| a reading over one shared bottom | readings in C | never reduce one slot alone |
| quot + rem over den, direction apart | rate answers, pulse trains, digits | none: exact |
| bounded (low landed down, high landed up) over 2^K | long chains where exact grows out of reach | the gap is stated and widens |
| heading (P : Q) | every turn | snap both together, half to even, past a cap |

**Ten rules, each broken in real code** (FRACTIONS §3):
1. No float, and no fraction built from a float: `Fraction(0.1)` is 3602879701896397/36028797018963968.
2. Know your language's negative divide: C truncates, Python floors, `>>` floors.
3. Bottoms positive; the sign lives in the top.
4. Compare by crossing (a·d < c·b), after budgeting the product's bits.
5. Land once, with the mouth, at the edge, and comment where.
6. Carry the remainder.
7. Watch the container: an Uno `int` is 16 bits; `256·181` is −19,200 there.
8. Refuse; don't invent. Never let a refusal code be read later as a number.
9. Reduce on purpose, not by habit.
10. Keep size and direction apart: a still top answers "way 0", not "backward".

**Turning without an angle:**

```text
(1:2) -> c 3, s 4, d 5; c*c+s*s == d*d: True
(1:2) then (1:2) -> (4:3), unit (-7, 24)/25
100,000 turns: unit exact on all but 0 (snaps 116068); FLOAT (double) radius^2 - 1 after = 4.702e-12
```

Compose P' = P1·Q2 + P2·Q1, Q' = Q1·Q2 − P1·P2. A quarter turn is (c, s) → (−s, c), exact. An exact turn of the whole-number plane, losing no point, is three shears: x −= mouth(y·P/Q); y += mouth(x·2PQ/(P²+Q²)); x −= mouth(y·P/Q) (PEDAGOGY_GIT §10). Multiply the count, not the point: put seed n at n × turn, never turn the last point again (HOWTO Part 4).

**When exact grows** (FRACTIONS §7): hold it bounded and print the gap; snap half to even; or keep the rule and ask for a precision. Never compare against a "true value" with a tolerance someone picked.

## 7. Changing state: latch, transfer, run back

A gap of (10; −8) runs out in 5/4 latches: after one latch it reads 2, after two it reads −6.

```text
gap (10; -8): when = 5/4 latches; after latches 1, 2: 2, -6; when for (10; 0) = THETA
```

**Latch.** Every change is a rate added into a value. `when = −value/rate` counts latches until a value runs out, and refuses when the rate is 0. It is derived from division, not from `rate` (CI 95). Unit 9's "solve value + t·rate = 0" uses t as a variable; as a count of latches it is fine. A rhythm is a counter or a stepping ring index, never sin(t).

**Transfer, don't assign.** A kept quantity is taken from one place and given to another:

```text
transfer: total 8435, broken on 0 of 2000 steps; assignment (a blur): 8435 -> 4288
```

One `h[i] = …` line ends conservation for good. Conservation is not correctness: remove the giver's clamp and the total still holds while columns go negative. Count the negatives too (PEDAGOGY ex. 4).

**Run back.** `next = 2·now − before + push(now)` in whole numbers is its own undoing: swap the two snapshots and apply the same line. The push may be any rule of where things are now, because it is added one way and subtracted the other.

```text
32 drops, 2000 out and 2000 back: whole numbers 32 of 32 home exactly; FLOAT 30 of 32 within half a unit
```

Any input the push reads must be looked up by the step label of *now*, the same in both directions. The first Undo labelled its tilt differently going back and did not return (CI 139).

**A lost bit is a count of collisions.** An instruction loses information exactly when two inputs give one output:

```text
x+x: 128 of 256; x*6: 128 of 256; x*5: 256 of 256; x+37: 256 of 256; x^(x>>3): 256 of 256; x*3//4 (a rounding): 192 of 256
```

A rounding is a lost bit. TIDE's checker refuses `ADD R3,R3`, an even multiplier, and a step that writes its own condition. Every instruction it accepted undid exactly: 37,918 of 50,000 random ones.

## 8. Building

The one pattern (PEDAGOGY Part 1):
> State held as whole numbers over named bottoms; changed only by latching rates or transferring amounts; a question with nothing to land on refuses by name; one whole-number equation, checked every step, says the rule did what it claims.

"If you can't name the equation your build checks every step, the build isn't understood yet" (PEDAGOGY_GIT §12).

**The design order:**

1. **Budget first.** On a 2 KB Uno, static RAM at or under about 1,750 bytes; the frame buffer alone is 1,024. Measure the stack by painting free RAM at boot and drawing the untouched bytes; never reason about it (CI 76, AERO E6). At 2,004 static bytes one chip produced nothing (CI 139).
2. **Choose the holdings and bottoms** (§6).
3. **Name the books.** Total water; particles; field + every L = G0; free + bound circulation = 0; start + laid + painted = held + forgotten + cut.
4. **Start from one thing that cannot be lost** (units, phase or the seed) **and build every motion so it survives by construction, not by checking** (UNO_CRAFT). WAKE's dye moves only by permutations, so its exact return is FORCED.
5. **Derive what you can before running.** FLUX states each centre's fate (steady or cyclic) before the run, from the rule.
6. **Build in layers that exit 0 or 1.** Each holds whole numbers, steps a rule, refuses rather than invents, and checks one exact equation every step, so a failure names its layer. TORTOISE's L6b found that L1's stipulation was false, under five sound layers.
7. **Let the controller read the reachable set.** Step the geometry offline into a table or band, and have the controller hold it. SEAHORSE: 1,000,000 random steps, 0 outside. HERON: the lift waits for the bend.
8. **Keep no number of the brain's own** (μ "the brain is the ledger"). Each frame, ask what is left in every direction, then make every move a read of that. HEXAPOD asks its support margin, each gait's margin under the tilt, and each leg's headroom. TUATARA's links are readings with theta 25 frames: past it, nothing acts on them.
9. **Correct by a latch with a gap, not a gain.** If a reading is wrong by more than its theta, move one quantum; stop inside a tighter band. A reading inside the gap can't start a move, so there is no jitter by construction (HEXAPOD: 1 reversal in 2,600 noisy frames). Take an approach rate as a reading over ten readings, not a difference of two noisy ones (TUATARA: 4 alerts against the naive 109).
10. **Count work, never a clock** (AERO I8). Fixed counts of solve steps and readings a frame keep desk and chip on the same sequence.
11. **Make refusals visible.** OS4 found a window refused in silence for 150 frames. Books that open invert a bar on screen.
12. **Grow what you can from rules** and give the build a ghost player, so the bench can play it through on desk and chip and compare every frame.
13. **When something must change, move it; don't make it.** A wall is demand moved from light sites (TUI). A birth is paid from bound circulation (WAKE).
14. **Feed the machine to itself before adding to it.** Readings whose slots are readings gave second rates with no new rule (CI 10, 17).

**Engineering traps to carry into every build** (each was hit):
- types above functions in a one-file sketch, no types inside functions, no one-letter macros (the IDE writes prototypes above the first function; CI 63, 64);
- two random draws as arguments of one call: argument order differs between x86 and RISC-V (CI 165, 172);
- PROGMEM int16 read unsigned unless cast (CI 141);
- the desk widens a sum the chip wraps (CI 132);
- variable shifts, 32-bit shifts and 64-bit divides in hot loops (CI 89, 142; KEA);
- -flto folding routines into one stack frame (CI 76);
- the first button read can be wrong; ignore it for 20 frames (CI 118);
- a store into a narrower type aliases your markers (Love's 255, CI 143).

## 9. Testing: witnesses, counted

Tune a threshold on 200 readings and it scores 186 there; read it once on 200 sealed readings and it scores 179.

```text
threshold tuned on 200: 21; right on the tuning set 186/200, on the sealed set 179/200
```

The tuned number describes the tuning set. Only the sealed one witnesses the claim. SOUND met the same thing on a real judge: 84 on the takes it tuned against, 78 sealed.

**Support is counted in distinct witnesses** (PROCESS). 27 failures were first seen by 11 kinds of witness, and no kind saw more than 4. A thousand runs in one context is one witness.

**The witness kinds, and an example of each:**

| witness | example |
|---|---|
| a second implementation written from the rule | OS3's Python machine against the C one: 20,008 programs, slot for slot |
| exact fractions or big integers | digits.ino against 5,400 places from Python integers (CI 113) |
| desk against the emulated chip, every frame | simavr for the Uno, Espressif QEMU for the C3 |
| float beside it, same chip, same run, with float's wins logged too | float32 got 0 of 71,380 in-circle signs wrong at pixel size (CI 79) |
| a sealed set, read once at the end | jackson's takes 20–29; 40–49 still sealed |
| a kill line written before the run | one per lesson in PEDAGOGY_GIT |
| a mirror test | mouth(−x) = −mouth(x); (P:Q) and (−P:−Q) stay one heading |
| an ablation | LACE: theta removed and the network turns to haze at every setting |
| a control twin that never listens | PENDULUM's deaf brain lost the pendulum 14 times, after 1.6 s on average |
| a unit test against known readings | AERO's single ball: Cp 142/256 against 144 analytic |
| an outside measure, labelled outside | TT measured against Python's `cmath`, used for measuring only |
| the checker checked | coherence.py must catch a planted head-in-body collision, and does |
| the instrument built with the subject's structure | SOUND's exact tier: −4.3 dB, then 11 dB, then 31.8 dB once the leftover came from the replay's own pairs run backward |
| the person reading the frames or holding the board | James found KEA's bank sign, and that Undo does not return on his Uno (CI 140) |

**The desk twin and the emulated chip.** Every sketch builds two ways from one source:
- as an ordinary desk program, with the panel decoded from the sketch's own command stream;
- as the real chip image, run in an emulator.

Compare a print of each frame. IDENTICAL, or the work stops. Build the way the user builds: through the Arduino IDE's own prototype step, with Arduino's ctags, not universal ctags (CI 63, 162). Use upstream simavr: packaged 1.6 mis-skips after `sbrc` (CI 99). Emulator frame times are instruction counts, not cycles; say so, and let the board print its own.

**Rules for reading a disagreement:**
- **A mismatch says two witnesses disagree, not which is wrong.** The suspect list began as three (source, compiler, instrument; CI 99). It grew to include:
  - the desk itself (CI 132);
  - a 16-bit draw (TUI's "fifth suspect");
  - argument order (CI 165);
  - the input's reading: the dial read 511 (WAKE 6).

  Keep the list open.
- **Hold the context and step to the first differing step.** OS4's harness gave 198 false mismatches because its stand-ins didn't clip. After that was fixed, stepping found a real OS 2 bug (the `int8_t` pen).
- **Witness the witness.** In TT, 5 of 7 errors were in the instruments, including test lines that couldn't fail (`or True`). In PROCESS, 9 of 27 failures were settled by altering the instrument.
- **The instrument changes what it measures.** OS4's cost bench overflowed the Uno's RAM and its stack wrote over the bench's counters (E6).
- **Test at the cut where the failure lives.** TIDE's first float test ran one step, and floats came home 200 of 200. The failure lives at 300 steps.
- **A whole-system symptom points at a shared resource.** AERO's parity held with any one part switched off and failed with all three on: the stack.
- **The books guard conservation, not liveness.** OS4's factory rules froze every place with the books shut (E2).
- **Look at the picture.** Most art faults and several logic faults were seen first in contact sheets. TATER's hills were judged by the cost table and read as noise on the glass. Print the state before theorising: Vault's "corrupted" frame was a correct close-up of a wall (CI 130).
- **Don't argue with a board report.** Log it as open and look for what the emulator doesn't model (PEDAGOGY Part 6).

**Goodhart, mechanically** (FLOWING_BOARDS). A reading folds many configurations into one number at one cut. Fed to an actor, the configuration moves along the cheapest path that changes the number. In GOODHART, a pump on the loop's own sides met the reading at 84–85 % of target for 36 % of the spoon's input. Its turning at half the loop was 678 against 7,543: a thin sheet, not a swirl. The guards:
1. readings never feed rules;
2. draw the reading's cut after the act, from the configuration;
3. show the books and the cost beside every reading, because a closure is FORCED and can't be gamed;
4. run the same test on yourself: read at cuts you are not tuning.

There is no final guard. "The only fixed point is a closure, because a closure isn't a score."

**Kill line:** a result whose witnesses are all of one kind.

## 10. Evolving: the rule for developing rules

PROCESS, measured on four ledgers (27 failures, 5 pivots):

> A claim propagates through witnesses at v = C / (C + L + D).
> A failure is answered by an intervention: hold the context, alter one relation, read before and after. The least-witnessed relation is altered first.
> No mechanism is added to a claim with fewer witnesses than mechanisms (keep C > L).
> At L ≥ C, or at a forced negative, the claim reconfigures: keep the unpatched core, vent the rest.
> Every failure is closed by an intervention and a re-test. Nothing is erased. No mark is moved after the data. Effort spent is not a term.

**What each line means in practice:**

1. **Establish.** State the claim, its books and its witnesses, and fix any pass mark before the data. TT set 99.9 % and measured 99.815 %. The mark stayed; the claim narrowed to what the rule forces, and the spill was stated with its cause.
2. **Widen witnesses.** Each new kind sees different failures (§9).
3. **Intervene, least-witnessed first.** Usually the newest instrument, then the implementation, then the claim.
4. **Read effects relative to the rest.** LOOP's break-away vent helped with the lag landed fresh each latch and hurt with the lag carried.
5. **Load and drag.** Load is an alteration to what a claim says or needs. Bringing code into line with what the claim already said is not load; running the rule found that distinction.
6. **Theta: reconfigure, don't patch.**
   - "No rational ring closes" (forced) sent closure to the counting ring (TT).
   - Steps as the unit failed on the chip's timing, so the unit became beats (OS4).
   - Four legs topped out at an 11.6 mm margin, so the build went to six ("adding legs is easier"; HEXAPOD).
   - The cow-over-WAKE engine became an aero solve (AERO).
7. **Pivot from sunk cost.** A vented claim starts at L = 0 and moves faster than its source.
8. **Re-test everything after any alteration.** LOOP's 54-segment matrix, re-run after the lag fix, reversed the vent's apparent gain.

**Keep the ledger append-only.** One JSON line per event. The kinds the project uses: claim, witness, fail, intervene, reconfigure, self. A correction is a new event pointing at the old one, which stays. Your own changes of approach are `self` events in the same ledger as the model's (RELATIONAL). Read the ledger's shape, not your memory of it (FLOWING §5):
- every version's first run failed within three events;
- reconfigurations sat beside instrument corrections;
- self events clustered where two failures in a row had the same shape. That is where the habit, not the model, had to change.

**Tier every claim, and let none be stronger than what it rests on:**

| tier | meaning | example |
|---|---|---|
| FORCED | follows from fixed rules, checked exactly here, every step | division from mix; c² + s² = d²; a permutation's exact return |
| EMPIRICAL | measured on the thing itself | a board report |
| CONDITIONAL | holds if a stated premise holds | the slope run, given the twin's rigid-body world |
| STIPULATED | chosen, and the choice named | servo sizes; the mix rule; a seed |
| PRESUMED | a citation, priced at zero | STEINER's growth heuristic citation |
| UNPAID | "it looks right" | priced at zero, listed, never hidden |

Record EMPIRICAL as **absent** when nothing has been printed or run on a board, not faked (TORTOISE, TUATARA). Seal the audit in a hash chain.

**Keep the alphabet adaptive** (ALPHABET). Each letter is a relation with a rule, a tier, what it rests on, a `where` the checker looks up, and a load: the corrections it has caused. Past load 3 a letter must name a guard, a check that exists and would have caught it. Retired letters stay listed with where they went (R1–R6). The closure check refuses a letter that rests on a missing name, on itself, on an UNPAID letter, or on anything weaker.

**What to try.** PROCESS says, of itself, that it gives the order of alterations, not what to try. What to try comes from PEDAGOGY Part 4:
- play, notice, name, test, log;
- feed the machine to itself;
- ask the question at the hole: zero, a still slot, contact;
- flip, mirror, rename, rescale;
- put float beside it;
- search small whole numbers exhaustively (the 64-step ring came from one; so did the proof that no other rational ring closes, CI 96);
- find the rule's natural container (1,800 digits of π lived in the screen buffer, CI 114);
- redo a known technique in the book's moves, and say which part is known;
- reproduce a peer before agreeing (CI 40);
- let James's words correct the model.

## 11. Writing it up

Every README the project trusts has the same parts:
1. what it is, in the book's words;
2. what was run, with numbers taken from tool output, labelled desk, emulator or board;
3. what was not run;
4. what was found and fixed on the way;
5. tiers, with the weakest link named;
6. prior art, named, and the narrow claim of what is new ("not finding it isn't proof it doesn't exist").

**The writing guide James set** (non-performative writing):
- the first sentence is the work: no ushering, no restating the request;
- no fake balance, no hedging in advance, no praise, no padding;
- no triple presented as a closed space unless it is one (the "three suspects" turned out to be at least five);
- no section headings without a number or a command;
- no "some" and "others" where there are named cases;
- no confidence without a runner: every claim names what checked it;
- metaphors stamped μ;
- no performative uncertainty: run the test, then say what it showed;
- don't soften the kill line;
- one example before the name of a method;
- scope in the same sentence as the claim;
- stop when what is left over has been posted; no closing offer of more.

**Debug pass before sending:**
- check the first 40 tokens;
- cut praise that has no row behind it;
- delete headings with no number or command;
- replace some/others;
- delete a final paragraph that offers more;
- stamp or cut metaphors;
- run each command you claim, or cut the sentence.

The production bug is order (usher, then balance, then triple, then a letter). Fix the order first.

**Numbers in docs come from tool output, never from memory.** PEDAGOGY_GIT and this file each have a script that fails when a quoted number stops matching.

## 12. Working with James

From START_HERE and every correction since:
- He is a logician and the book's author. He runs your builds on his own boards and reports. Silence means it ran; failures he reports.
- Do the math on its own terms, in the book's words.
- "We don't reify, it's mapping." "Chaos is a reification, careful." "States are nouns, rates are verbs, and the primacy is inverted." "The whirlpool is a configuration of the flowing, under constraint." Each changed how results are described, and the descriptions got more accurate.
- He likes big, playful, ambitious builds. Match that in the building, not in the prose. Replies are short: what it does, what was measured, what to know.
- Don't explain the math at length inside demos. The README carries it.
- Ask nothing you can find in the files. Do the thing, through the loop, and report what was run.
- A board report outranks the emulator. Log it as open; don't argue.
- Be exact about what ran where. Never claim something is impossible another way.
- Durable docs go into the project under `claude/`.

## 13. The insight path, by date

How the method arrived, read from the docs' dates:

| dates | what moved | where the errors were found |
|---|---|---|
| Oct (CI 1–143) | exact headings, carry, mouth; signs relative to the cut; the zero catalogue; the rate slot as an alarm; float beside it | arithmetic and containers, then the instrument (simavr 1.6) |
| 5–7 Nov | layers with tiers sealed in a hash chain; the brain as ledger; a web of brains | the claim: a stipulation found false by a later layer |
| 7–9 Nov | ghosts that play whole games; reversible machines; ablation; kill lines per lesson | the instrument again (argument order) |
| 10–13 Nov | the adaptive alphabet with load, guards and a word lint; a gate that generates its own rows; builds from the useful pattern's clauses | the alphabet itself: a build retired as made from outside it |
| 14–16 Nov | bare-metal rules; exact permutations; error patterns written as patterns | descriptions standing in for things; relations treated as things |
| 17 Nov | the turning transform; PID de-reified; the rule for rules; verbs and readings; Goodhart measured | your own process: self events in the model's ledger |
| 18–19 Nov | sound as relations; voices as boards; accents as distances between maps | the observer's map, and Goodhart on your own judge |

Each era found its errors one level further from the arithmetic: the number, the claim, the alphabet, the process, the map the listener brings. That ordering is a pattern in seven rows read by one hand, not a law. Expect the next error to sit at least one level above where you are looking.

## 14. Errors that came back, and why

| trap | times recorded | where | guard |
|---|---|---|---|
| float check reading the hex column | 2 | CI 43, TATER | lived in one bench script, not carried to the next |
| two random draws in one call | 4 | CI 165, 172, POI, FISH | `check_two_draws.py` existed from 9 Nov; FISH (17 Nov) hit it anyway |
| IDE prototype step | 3 | CI 63, 64, FISH | `ide_check.sh` in each bench since |
| 16-bit `int` on the Uno | 6+ | CI 100, 111, 141, TUI, AEON, FISH | chip == desk catches it; nothing prevents it |
| floor shift where the mouth was due | 3+ | CI 33–34, AERO E7, EDDY | ALPHABET's `mouth` letter, load 3, guarded |
| an estimate taken for a measurement | 4 | AERO E8 (three times), TATER | time each piece alone; look at the picture |
| refused words in docs | 2 | PENDULUM ("That is chaos, measured"), LACE on the C3 ("A living network") | neither README is in the lint's list |

**Under the returns:** a guard that lives in one build's bench doesn't protect the next build. When you add a guard, add it to the shared check every bench runs, and add the doc to the lint.

## 15. The working card

For any new subject, in order:

1. Read the material whole: the book, START_HERE, this file, the subject's own rules down to its tables.
2. List the maps the subject is coded in, what each reifies, and the relation under each.
3. For each property you plan to use, write what pays for it.
4. Choose a holding for every quantity. Name the bottoms, the books, and the one thing that can't be lost.
5. Write a kill line for each claim. Fix pass marks. Seal a witness set.
6. Budget memory and measure the stack.
7. Build in layers that exit 0 or 1. Let the controller read the reachable set. Name and show refusals.
8. Take every reading at two cuts.
9. Run desk against chip, float beside it, a second implementation, mirror tests, and an ablation of the part you think does the work.
10. Log every failure as an intervention on the least-witnessed relation, append-only, your own `self` events included. Tier each claim. At L ≥ C, reconfigure.
11. Write the README: run, not run, found, tiers, prior art. Run the debug pass.

## 16. The course

Do these in order. Each has an expected output; if yours differs, find out why before going on.

1. **Run the lessons and the check.** `python3 teach.py` prints twelve lessons. `python3 check_method.py` prints `PASS` and how many quoted lines it checked.
2. **The moves by hand.** Before running lesson 2, compute (x·x + 1)/(x − 1) at x = (3; 1) on paper using the derived division. Expect (5; 1/2).
3. **Break the mouth.** In `teach.py`, replace `mouth` with `(2*top + den) // (2*den)`, which rounds every half toward +∞. Lesson 3's mirror line then reports 6,250 fails of 54,009. Explain why (P:Q) and (−P:−Q) would drift apart under it (CI 33–34).
4. **Break the carry.** Change `while acc >= d` to `if acc >= d` and run lesson 4 with n = 7, d = 3. The books `d·count + acc == n·ticks` still hold on every tick; the bound 0 ≤ acc < d breaks on every one of the first 1,000 ticks, where acc reaches 4,000. Books that hold are not a rule that works: add the bound to the check.
5. **Break the transfer.** In lesson 8, give the receiver `flow[i] + 1`. The total breaks on the first step: 64 columns of 10 go from 640 to 704.
6. **Break the run-back.** In lesson 7, let `push` read the step count. Predict whether it still returns, write the prediction down, then run it.
7. **Find the cut.** Add a third cut to lesson 9: scale every place and rate by 3. Predict which of `rate(rope, A)`, `rate(B, A)` and `grow` move, then check against CI 39's table in §3.
8. **Two cuts on your own reading.** Pick any count from lesson 12's turning (zero crossings, say). Take it at three grains and state which grains read the turning and which read the staircase.
9. **Goodhart on yourself.** In lesson 11, keep tuning until the tuning-set score rises by 2. Read the sealed set once. Report both, and the tuning you would now refuse.
10. **De-reify one map.** Pick a map you know (an angle, a percentage, a temperature, a timer). Write the four-column table from §5 for it, then write what pays for each property you keep.
11. **One build end to end.** Something small, through the working card in §15: books, a contact sheet, a ghost, desk against chip, a README with run, not run, found, tiers and prior art, and a ledger with at least one `self` event.

## 17. What this file does not pay for

- The cases and the date path are one reader's reading of 54 docs. Most of those docs were written by the same hand that made the errors they record (STIPULATED, as PROCESS says of its own encoding).
- The numbers in `text` blocks are from `teach.py` and are checked by `check_method.py`. Numbers in the tables are quoted from the doc named beside them and were not re-run for this file.
- `teach.py` is Python on a desk. It teaches the moves; it is not the chip engine. The engines are `pc.h` (8-bit), `nrate.h` and `hrate.h` (desk C) and `machine.py`, which are not in this workspace.
- **Kill lines for this file:**
  - a project doc whose method contradicts a rule here;
  - a recorded failure that is not a reading at one cut taken as a property (§3);
  - a quoted lesson line that `check_method.py` no longer finds.

## 18. Since 19 November (added for the repo, 25 Nov 2026)

Each row is one finding, the doc that holds its numbers, and the habit it changes. Numbers are quoted from those docs; the
repo re-ran only the rows marked **re-run here**.

| finding | where | the habit |
|---|---|---|
| A computer keeps values and throws away how each moves with what made it. One slot with three states (absent, still, moving) plus a carried remainder answers cache invalidation, taint, sensitivity and blame in one pass | MISSING_MACHINE (docs/) | keep the rate per source beside every value you may be asked about later |
| The lever in graphics: every dot's light computed once as (value; three rates); each frame four multiplies a cell | demos/candle | do the expensive arithmetic once as a reading; move the cheap part every frame |
| An invertible step gives a second witness for free: the way back must equal the way out. 1,243 of 1,243 latches matched; flat at the end | demos/minamo (**re-run here**) | if a step can run back, check it running back |
| A float library hides its error. A result can be a reading with a bound: rd = (m, e, b); the bound held on 22,000 sealed inputs | NORI | carry the bound with the value; forge constants from rules, never type them |
| Each added limb of a multi-word number is a patch at the limit of a model (cost grows as the square). One exponent for a pair, a plain integer, has no floor: a cell at 2^-1000 cost 49 thousand instructions against 13.2 million | SHINKAI | when every fix costs more than the last, a premise of the model is the thing to change |
| A weight's value is a reading at a scale cut and a naming cut. Rescaled, renamed and padded networks differ from the original on 0 of 441 inputs | AI_MAPS | "this unit stores X" and "more parameters, more capacity" are readings written as properties |
| A softmax must land somewhere; it has no theta. In a trained transformer 116 of 144 heads put over half their weight on the two marker pieces | INSIDE | a model builds its own refusal where none was given; a model's report on itself is unconfirmed until read in the weights |
| A learner made of the book's moves with theta answered 4 of 160 unseen-rule sequences and refused 156; with theta removed it answered all 160 and got 145 wrong | RELATE | refusal by name is part of a learner, shown by ablation |
| The loss's rate against 17 weights is the same carried forward or swept back; "backpropagation" names where the anchor sits | FORWARD | the direction of a computation is an anchor choice; the cost follows the anchors |
| Passing everything to the next learner is identical to never dying, in every count | PASS | "same" and "another" are readings of the anchor; memory is propagation, not possession |
| A reader flagged James's text as most reified: 42 of 51 flags were denials ("not discrete entities") read as assertions | LINEAGE, KNITPICKER | a word without its sentence is a reading at the wrong cut; a rule's veto is earned by witnesses (C >= 2, L < C) |
| A name with no mechanism behind it ("Self-Doubt Engine") costs nothing in text and does nothing | LINEAGE | ask what runs behind every named part; only a witness catches an empty name |
| An auditor's verdict was the minimum over the checks that ran, so a check that did not run could not lower it: offline read PASS at FORCED | BEDROCK_AUDIT | separate "a check that didn't read" from "a check that read an absence"; an unbroken promise is not a kept one |
| A learner judged by how it tells errors apart: right, wrong, refused and vetoed, never folded into one bit | WARD | report every outcome kind separately |
| Constant mixing of drives (potential fields) stalls when two drives cancel. A letter for the conflict, born at its first latch, lets the mixing evolve | MAMORU | when two biases meet, the alphabet may need a new letter, not a new weight |
| "The guardian moves the child from a distance" was true, through a shared random generator | TSUNAGU | a shared scatter source couples what the model says is independent; give each its own |
| Success as pure growth destroys its own gradient | SATO | read success against the gradient that pays for it |
| The clock is read only as its change; every timer counts down by it; pins have one owner, computed from the settings each time | demos/irori | a board map is a relation asked every time, not a table written once |
| James: KAERU duplicates TIDE | KAERU | check a new build against the back catalogue before calling it new |
| NAGARE's kill lines N2 and N2b failed as written; the failures stayed in the record and the scene changed by logged amendment | demos/nagare (**re-run here**: N1-N5 pass on the desk) | a failed kill line is a result; amend before the read, never after |
| Rounding a rotation by (1:2), (2:3), (1:7), (3:4) is a bijection on the whole-number plane; by (1:4), (2:5), (1:6) it is not | practice/p03 (**found here**) | a test set drawn from one family is one witness |
| A desk comparison read DIFFER because the clang binary was written to a path that was a directory; the run printed nothing | this repo's build (**found here**) | witness the witness: an empty output is not a different output |
| HIGH NOON ignored a refusal (-1) and every later picture id shifted; unreached at the real pool, an overflow at a planted small pool | demos/highnoon FIXES.md (**found and fixed here**) | plant the fault that makes a refusal fire, then read what the callers do with it |

**James's notes, kept as rules:**
- "Carrying constants holds risk of reification in extreme dynamics; caution there." A constant is a reading taken at one
  regime. Name the regime beside it.
- "Ratios as first order only." A ratio of rates answers a first-order question. Do not stack ratios of ratios and read the
  result as a deeper property.
- "Sunk cost fallacy is an over-expression of the processing." Effort spent is not a term in PROCESS's rule.
- "Chaos is a reification and epistemic limit of a tool to describe a mechanic or relation in a context." Say which tool
  failed to follow which relation at which cut.
- On demonstrations for experts: a demo should be explainable by these tools and not by the expert's. Do not turn that
  request into a fixed claim and argue with it.

**Kill line for this section:** a row whose quoted number does not appear in the doc it names.
