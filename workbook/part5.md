*Part V*

# What The Machines Taught

### Nine Units From Building On A Chip, In The Book's Moves

Parts I to IV built numbers, letters, shapes and a calculus out of three moves. Part V takes those moves onto a chip with no floating point and 400 KB of memory, where every quantity has to be held as whole numbers. Nine habits came out of that work, each one found by a build that failed first. Every number in the demonstrations below is printed by `docs/teach.py` or `workbook/part5.py`, and `workbook/check_workbook.py` fails if any of them stops matching.

Same shape as before: a big question, the reason, demonstrations, a Try It Yourself, practice, and answers at the back of this part. Part V adds one line to each unit, **On the chip**, naming a sketch in `demos/` where the habit does work.


---


*Unit V1*

## The Carry: A Rate As Whole Pulses


> **Big Question:** A wheel should turn 4/3 of a step every tick, and a motor can only take whole steps. How do you send it whole steps without ever drifting?

Part I, Unit 4 kept the remainder of 17 / 5 instead of throwing it away: "the 2 left over is real." The carry keeps it for the next tick. Each tick adds the rate's top to a running remainder; every time the remainder reaches the bottom, one whole pulse leaves and the bottom is taken off.


*Demonstration 1: 7/3 by hand*

```text
tick 1: emit 2, carry 1, count 2, 3*count + carry = 7 = 7*1
tick 2: emit 2, carry 2, count 4, 3*count + carry = 14 = 7*2
tick 3: emit 3, carry 0, count 7, 3*count + carry = 21 = 7*3
```

The last column is the **books**: bottom × count + carry = top × ticks, on every tick. Nothing is lost, so nothing drifts.


*Demonstration 2: 4/3 for 100,000 ticks, against a float*

```text
first 24 ticks: 112112112112112112112112; after 100000: count 133333, d*count+acc == n*ticks held every tick; FLOAT accumulator wrong on 33333 ticks
```

A float adds 1.333… each tick and rounds where 4/3 can't be written in binary. It was wrong on a third of the ticks.


> **Why does the carry need "while" and not "if"?** At 7/3 one tick can owe three pulses. An `if` sends one and lets the remainder grow without bound. The books still hold (they count the remainder), so add the bound 0 ≤ carry < bottom to the check. Books that hold are not a rule that works.

**On the chip:** LACE's forgetting (`demos/lace`), every pulse train, every line drawn by the carry rule in HIGH NOON's ray walk.

**Try It Yourself:** Run 7/3 for ticks 4, 5 and 6. *Hint: start from carry 0 after tick 3.* Answer: the same three lines again with counts 9, 11, 14.


### Practice: Unit V1

1. Write the first six ticks of 5/4: pulses, carry, count.
2. At 4/3, what are the books after 9 ticks?
3. Why does the pattern 112 repeat every three ticks at 4/3?
4. A float accumulator for 1/3 is checked after one tick and after 300. At which one would you expect it to fail, and why? (Part IV's Unit 11 halving rule is a hint about where errors pile up.)

---


*Unit V2*

## The Mouth: Landing Once


> **Big Question:** A fraction has to become a whole number to light a pixel. Where, and by which rule, should that happen?

Part I's Unit 5 said that two "round down" rules disagree on negatives. Here they are, and the rule this project uses instead.

```text
toward the more negative (//) of -7/2: -4; toward zero of -7/2: -3
mouth of 1/2, 3/2, 5/2, -1/2, -3/2: 0, 2, 2, 0, -2
mouth 5/2, 7/2, -5/2, -7/2, 10/3: [2, 4, -2, -4, 3]
mouth mirror fails: 0 of 54009; floor shift (-x>>1) != -(x>>1) on 3000 of 6001
```

**The mouth:** the nearest whole number; an exact half goes to the even neighbour; and mouth(−x) = −mouth(x). It is used once, at the edge, where a quantity leaves the rules (a pixel, a motor step), never in the middle of a chain.


> **A choice, not a discovery** Half to even is a choice. It is picked because it is the only one of the usual rules that is unbiased over many halves and keeps the mirror: a heading (P : Q) and (−P : −Q) must stay the same heading after landing.

**On the chip:** every sketch here has a `mouth` function. TIDE's turns land inside shears with it, so they run back.

**Try It Yourself:** Land 9/2, 11/2 and −9/2. Answer: 4, 6, −4.


### Practice: Unit V2

1. Land 7/4, 10/4, −10/4.
2. A shift right by one is a division by 2 that lands toward the more negative number. On which of −3, −2, 3 does `x >> 1` disagree with the mouth of x/2?
3. Why land once at the edge and not after every multiply?

---


*Unit V3*

## Which Answer Belongs To Which Cut


> **Big Question:** Two cars, A at 0 moving 5, B at 10 moving −3. You rename the rope, point the road the other way, or seed the rates with −1. Which answers move?

Part IV's Unit 5 showed the seed cancels. The same test can be run on every choice an observer makes. Each choice is a **cut**: a naming, a direction, a seed, a grain.

```text
A-B: rope (-10; 8), rate(rope, A) = 8/5, grow = rope rate / rope value = -4/5
B-A: rope (10; -8), rate(rope, A) = -8/5, grow = rope rate / rope value = -4/5
direction cut flipped: rope (-10; 8), rate(B, A) = -3/5, grow = -4/5
seed -1: rope (10; 8), rate(B, A) = -3/5, grow = 4/5
```

`rate(rope, A)` moves when the ends are renamed. `grow` moves with the seed, because it counts per latch. `rate(B, A)` moves with none of the three.


*Demonstration 2: the same turning, counted at seven grains*

```text
same turning, peaks counted at grain 2^k: k=0: 16, k=12: 16, k=16: 32, k=17: 104, k=18: 62, k=19: 31, k=20: 32
```

At fine grains the count is the turning's: 16. At coarse grains the counting rule counts the stairs of the landed staircase. Read at one grain, 104 would have been reported as a fact about the turning.


> **The rule** No reading is believed until it has been taken at two cuts. A number that moves when you change a cut belongs to the cut as much as to the thing.

**On the chip:** NAGARE reads turning round 4 × 4 loops and says so; a different loop size reads a different turning.

**Try It Yourself:** Scale every place and rate by 3. Which of the three answers move? Answer: none. Each is a ratio of two quantities scaled together. (`practice/p06_two_cuts` asks for the full table, with `when` added.)


### Practice: Unit V3

1. Compute `when` (−value ÷ rate) for the rope named A−B and named B−A. Does the naming move it?
2. Name two cuts that a thermometer imposes on "the temperature of the room."
3. Why is "the swirl" a weaker sentence than "the turning counted round 4 × 4 loops"?

---


*Unit V4*

## Running Back


> **Big Question:** Can a step be undone without storing what came before it?

Keep two readings, now and before. Step with next = 2·now − before + push(now). Solved for the other end, the same line is the step back: before = 2·now − next + push(now). The push can be any rule of where things are now, rounding included, because it appears in both directions unchanged.

```text
32 drops, 2000 out and 2000 back: whole numbers 32 of 32 home exactly; FLOAT 30 of 32 within half a unit
x+x: 128 of 256; x*6: 128 of 256; x*5: 256 of 256; x+37: 256 of 256; x^(x>>3): 256 of 256; x*3//4 (a rounding): 192 of 256
```

The second line counts, for each instruction on 8-bit numbers, how many of the 256 inputs give different outputs. Doubling sends 256 inputs to 128 outputs: two inputs to one output, so a bit is lost and no rule can recover it. Multiplying by 5 loses nothing. **A lost bit is a count of collisions.**


> **Why float fails here** Every float step lands the state itself, and a landing is two-to-one. The whole-number version lands only inside the push, which both directions share.

**On the chip:** TIDE (a whole instruction set with no two-to-one instruction) and MINAMO (a pond that runs 1,240 latches out and exactly back).

**Try It Yourself:** Is x → x + x + 1 one-to-one on 8-bit numbers? Answer: no. 2x + 1 is always odd, so 256 inputs land on 128 outputs.


### Practice: Unit V4

1. Count collisions of x → 3x on 8-bit numbers. Then x → 4x.
2. Why must the push read the step's label of *now*, the same going back?
3. `practice/p04_run_back`: write the step in C and make all 256 places come home.

---


*Unit V5*

## Move A Kept Quantity; Never Set It


> **Big Question:** Heat spreads along a row of places. How do you write the spreading so no heat is made or lost?

Take an amount from one place and give the same amount to its neighbour, in one move. Never write a place's new value from a formula of its neighbours: that line makes or loses amount every step it rounds.

```text
transfer: total 8435, broken on 0 of 2000 steps; assignment (a blur): 8435 -> 4288
three quarters a face, 40 steps: total 8000 (start 8000), places below zero 32 of 64
```

The second line is the warning. Each face moved three quarters of the difference between its two places, so a peak gave away one and a half times what it held. The total stayed exactly 8000 while half the places went below zero. **Conservation is not correctness.** Count the negatives too.

**On the chip:** NAGARE's letters (MOVE, PUSH, DRAG, EXCHANGE, LIFT) are all transfers, so its books are exact every step.

**Try It Yourself:** What share of the difference can a face move before a peak between two empty places goes negative? Answer: the peak gives that share of its height to each side, twice the share in all, so the share must be at most a half. A quarter is safe; three quarters is the failure above.


### Practice: Unit V5

1. Write one transfer step by hand on the row 0, 8, 0 with a quarter of the difference a face.
2. Why does an assignment blur lose total when it rounds down?
3. `practice/p05_transfer`.

---


*Unit V6*

## The Lever: A Value With Its Rate Per Source


> **Big Question:** A total depends on several inputs. Which inputs, if changed, would change it, and by how much?

Part IV gave every reading one rate slot. Give it one rate slot per source instead, and keep which sources ever touched it. Then each source is in one of three states:

- **absent:** never touched the total;
- **still:** touched it, with rate 0 here;
- **moving:** touched it, with a rate.

*Demonstration: total = price × qty + fee × fee + floor × 0, at price 12, qty 3, fee 0, floor 5*

```text
total = 36: price moving, rate 3
total = 36: qty moving, rate 12
total = 36: fee still
total = 36: floor still
total = 36: tax absent
```

Fee is still, not absent: at fee = 0 the rate of fee × fee is 0, but the total moves once fee leaves 0. Floor is still for another reason: it was multiplied by 0. A tool that keeps only "touched or not" would call floor a dependency; a tool that keeps only the rate would call fee irrelevant. The three states keep both.

**On the chip:** CANDLE computes each dot's light once as (value; three rates against the flame's move), then lights the room every frame with four multiplies a dot. `docs/MISSING_MACHINE.md` gives the full argument.

**Try It Yourself:** Add tax as total × tax with tax = 2. Which sources move, and at what rates? Answer: total becomes 72; price moves at 6, qty at 24, tax at 36; fee and floor stay still.


### Practice: Unit V6

1. For y = a × b + b × b at a = 2, b = 3, give the rate per source.
2. Why does "still" not mean "will not change"?
3. Name a computing tool that keeps only the touched set, and one that keeps only the rate.

---


*Unit V7*

## A Heading Turns The Plane Exactly


> **Big Question:** Part IV's Unit 14 turned a pair of fractions exactly with a heading. Can a heading turn every whole-number point of a grid without two points landing on one?

Rounding the turn of each point does not work in general. Three shears do: slide x by y·P/Q, slide y by x·2PQ/(P² + Q²), slide x by y·P/Q again, each slide landed with the mouth. Each slide is undone by sliding back the same amount, so no two points can land together.

```text
(1:2) -> c 3, s 4, d 5; c*c+s*s == d*d: True
(1:2) then (1:2) -> (4:3), unit (-7, 24)/25
(1:2) on 3721 points: rounded turn 0 collisions; three shears 0 collisions
(1:4) on 3721 points: rounded turn 860 collisions; three shears 0 collisions
```

The third line is a trap that was found while writing this unit's practice checker. For (1:2) rounding happens to lose no point, and so does (2:3), (1:7) and (3:4). A test drawn only from that family would have passed a rounded turn. (1:4) shows the difference.

**On the chip:** TIDE's TURN instruction; every heading table in the C3 demos.

**Try It Yourself:** Compose (1:2) with (1:4). Answer: P = 1·4 + 1·2 = 6, Q = 2·4 − 1·1 = 7, so (6:7), with unit (13, 84)/85.


### Practice: Unit V7

1. Find the unit of (2:3) and check c·c + s·s = d·d.
2. Why is each shear undone exactly, whatever rounding is used inside it?
3. Why does a mouth inside the shears keep turn(−p) = −turn(p), where a floor would not?

---


*Unit V8*

## A Reading Fed Back Into Its Own Rule


> **Big Question:** You tune a threshold until it scores well. What does the score describe?

```text
threshold tuned on 200: 21; right on the tuning set 186/200, on the sealed set 179/200
```

The tuned number describes the set it was tuned on. Only a set read once, after the tuning stopped, witnesses the claim. A reading folds many configurations into one number at one cut; an actor that is fed that number moves the configuration along the cheapest path that changes the number (Goodhart, mechanically). The guards: readings never feed rules; draw the reading's cut after the act; show the books beside every reading; read yourself at cuts you did not tune.

**On the chip:** NAGARE's KL = 24 was chosen after looking, and its amendment says so: "This amendment is a tuning, and is named as one."

**Try It Yourself:** `practice/p07_kill_line` gives you 200 tuning readings and 200 sealed ones. Seal your kill line before the read.


### Practice: Unit V8

1. Why is a thousand runs in one context one witness?
2. Write a pass mark for a claim you have not tested yet.
3. Why can't a closure (a books equation) be gamed the way a score can?

---


*Unit V9*

## Witnesses, Load And Drag


> **Big Question:** A claim has passed some tests and needed some fixes. How strongly does it stand?

Count three things. **C**: the distinct witnesses it held in (a second build, exact fractions, desk against chip, a sealed set, a person with a board). **L**: its load, the alterations it needed (a mechanism added, a scope narrowed, an exception). **D**: its drag, the load of what it rests on. The claim propagates at v = C / (C + L + D).

```text
C 4, L 0, D 1: v = 4/5
C 4, L 3, D 1: v = 1/2
C 2, L 3, D 1: v = 1/3; L >= C: reconfigure
```

At L ≥ C, stop patching. Keep the core that needed no patch, and vent the rest as a separate claim at L = 0. Effort already spent is not a term.


> **A choice, not a discovery** The formula is a rule for developing rules, measured on the project's ledgers (27 failures, 5 pivots). It orders what to alter first (the least-witnessed relation). It does not say what to try.

**On the chip:** every README in `demos/` lists what was run, what was not, and tiers its claims; NAGARE logs four amendments.

**Try It Yourself:** A claim has C = 3, L = 1, D = 0. Give v. One more fix arrives. What changes? Answer: v = 3/4; with L = 2, v = 3/5, still C > L. A third fix makes L = C and the claim reconfigures.


### Practice: Unit V9

1. Name four kinds of witness.
2. Why is bringing code into line with what a claim already said not load?
3. Write a kill line for "this sketch runs the same on the desk and on the chip."

---


*Answer Key, Part V*

## Check Your Work


### Unit V1

**1.** 5/4: tick 1 emit 1 carry 1; tick 2 emit 1 carry 2; tick 3 emit 1 carry 3; tick 4 emit 2 carry 0; tick 5 emit 1 carry 1; tick 6 emit 1 carry 2. Counts 1, 2, 3, 5, 6, 7.

**2.** count 12, carry 0: 3 × 12 + 0 = 36 = 4 × 9.

**3.** 4 × 3 = 12 = 3 × 4: after three ticks the carry is back to 0, so the pattern starts again.

**4.** After 300. One step's rounding is a fraction of the last bit; the errors pile up over many steps.

### Unit V2

**1.** 2, 2, −2 (10/4 = 5/2, an exact half, goes to the even 2).

**2.** On −3: −3 >> 1 = −2, the mouth of −3/2 is −2: they agree. On −2: both −1. On 3: 3 >> 1 = 1, the mouth of 3/2 is 2: they disagree. (They also disagree at −1: −1 >> 1 = −1, the mouth of −1/2 is 0.)

**3.** Each landing loses up to half a unit. Landing once loses it once; landing after every multiply piles the losses up, and a product of landed numbers carries them all.

### Unit V3

**1.** A−B: rope (−10; 8), when = 10/8 = 5/4. B−A: rope (10; −8), when = 10/8 = 5/4. Naming does not move it.

**2.** For example: where the sensor sits (place), how long it averages (grain), and its scale.

**3.** "The swirl" writes a reading as a thing. The second sentence names the rule and the cut that made the reading, so someone else can take it again.

### Unit V4

**1.** x → 3x: 256 of 256 (3 is odd). x → 4x: 64 of 256.

**2.** If the push reads a different label going back, it adds a different amount than it subtracted, and the step back is not the inverse.

**3.** Checked by `python3 practice/p04_run_back/check.py`.

### Unit V5

**1.** Differences 8 and 8; each face moves 2: 2, 4, 2. Total 8 before and after.

**2.** Each place is landed down separately, so the pieces lost to rounding leave the total and go nowhere.

**3.** Checked by `python3 practice/p05_transfer/check.py`.

### Unit V6

**1.** y = 2 × 3 + 3 × 3 = 15. Rate per a: b = 3. Rate per b: a + 2b = 8.

**2.** The rate is read here, at this point. Away from it the rate can be non-zero (fee × fee at fee = 0).

**3.** Taint tracking and build systems keep a touched set; sensitivity analysis and gradients keep only the rate.

### Unit V7

**1.** (2:3): c = 9 − 4 = 5, s = 12, d = 13. 25 + 144 = 169.

**2.** Each shear adds to one coordinate an amount that depends only on the other, which the shear does not change; subtracting the same amount, rounded the same way, restores it.

**3.** The mouth is mirror-true (mouth(−x) = −mouth(x)), so each shear of −p is minus the shear of p. A floor is not mirror-true.

### Unit V8

**1.** It sees one kind of failure. Distinct contexts see different failures.

**2.** For example: "at least 170 of 200 sealed readings right."

**3.** A closure is forced by construction: it holds or it doesn't, and no change of configuration can make it read better without being true.

### Unit V9

**1.** Any four of: a second implementation, exact fractions, desk against emulated chip, float beside it, a sealed set, a kill line written first, a mirror test, an ablation, a control twin, a person holding the board.

**2.** Load is an alteration to what the claim says or needs. Fixing code so it does what the claim already said changes neither.

**3.** For example: "any printed line that differs between the desk twin and the emulated chip over 2,000 frames of the ghost's play."

---
