# RULE OS 4: insights and errors, in the order they happened

## Insights

| id | insight | tier | what pays for it |
|---|---|---|---|
| I1 | **A budget is a reified property.** "This program has 64 steps" is stipulated, and fixed whatever the program does. Replaced by a relation, the program's loaded history against the field, the scheduler is clauses 1 to 4 and 6 of the useful pattern, nothing added: due mark += weight × (C + L + drag) is AND = +, and choosing the least due mark is OR = min | FORCED within the stipulated model | `kernel.h:k_latch`; books shut on every latch of every run |
| I2 | **What the pattern makes a scheduler do, without being told:** a program that gives back stays simple and is served first; a hog takes the remainder and reconfigures. No priority anywhere | EMPIRICAL | desk: BOUNCE took its full pass every latch beside a hog; the hog reconfigured 193 times in 600 latches |
| I3 | **A step is not a unit.** Its load is what it did. Supply counted in steps let one hog stretch the Uno frame to 58.6 ms. Counted in measured beats, four of the worst hogs run in 22.5 ms, where OS 2 takes 74.3 | EMPIRICAL | Timer1 table, 31,962 steps; simavr frames |
| I4 | **Width is a representation, not the chip.** Exact readings in lowest terms don't depend on how wide the slot is. The narrow machine says WIDE sooner and never says anything different. So the Uno reaches 665857/470832, which OS 3's README treated as needing the C3 | FORCED by construction; checked | 0 disagreements before WIDE in 70,000 random programs |
| I5 | **Where width costs on the Uno: the stack at the deepest point, not the slots.** int64 temporaries passed down through mix → fit → gcd → libgcc. One static scratch for the three tops being landed: 18 → 71 untouched bytes (63 with every button pressed) | EMPIRICAL | stack paint on the chip |
| I6 | **A rule can deadlock while its books stay shut.** A place vents only when it gives back, and it can't give back if it can't step. Clause 4 already settles it: demand past the gradient reconfigures, it doesn't wait | FORCED | desk: frozen at 4 steps for 600 latches; after the change, every run moves |
| I7 | **A refusal holding history is history held hostage.** A refused place can't propagate, so all of its L goes back (clause 3: nothing erased, the bill redistributed) | FORCED | C3 kernel lines: the field rises by the 172 beats E held |
| I8 | **One kernel, three machines, one answer:** AVR, RISC-V (the C3's instruction set) and x86 | EMPIRICAL | Uno chip == desk on 2,000 frames; C3 QEMU == desk on 402 lines |
| I9 | **A window refused in silence is a refusal nobody can read.** Opening RULES failed for 150 frames with nothing on screen. TASKS now gives its room to RULES, and a refusal says THETA on the bar | EMPIRICAL | C3 twin: arena refused 1 before the change, 0 after |

## Errors (mine), and what each one taught

| id | error | caught by | pattern |
|---|---|---|---|
| E1 | wrote a gcd fast-path condition that meant nothing (`>> 32 >> 0 & 0`) | reading it back before running | wrote faster than I read |
| E2 | the factory rules out of order: G0 = 4, S = 2,048. Every place froze after one step, **and the books stayed shut** | the latch print | the books guard conservation, not liveness; a frozen system balances perfectly |
| E3 | my stand-ins for OS 2's window drawing didn't clip as OS 2's do: 198 false mismatches | stepping the first failing program to its first differing step | a test is a model of the thing; check the model before blaming the thing |
| E4 | the check then found a **real OS 2 bug** (the `int8_t` pen) | the same stepping | narrowing again; the same family as Tui's 16-bit overflow |
| E5 | supply counted in steps, as if steps were equal: 51.8 ms frames with a hog | side by side against OS 2, frame times | **a unit assumed equal is a property assumed intrinsic** |
| E6 | my cost bench overflowed the Uno's RAM (1,993 B static), and the stack wrote over the bench's own counters: 49 steps in 60 s | the counters disagreeing with the step number read out of RAM | the instrument changes the thing measured; measure the instrument too |
| E7 | a scripted hold ran into the next press, so INS never fired, and I nearly called it an OS bug | the editor buffer printed at the frame | a click is a relation between two frames, not a state |
| E8 | 64-bit math on the Uno at first left 18 bytes of stack | the stack meter, drawn on the frame | width has a cost somewhere; find where before choosing it |

**The pattern under all of them:** a relation treated as a thing. A budget, an equal step, a width belonging to the chip, a stand-in taken for the original, a press taken as a state. Each time the fix was to name the relation and measure it.

## Open

- The C3's own op costs (its weights are the Uno's).
- Either board for real.
- The useful pattern's clause 5 (the cascade engine) as the C3's wire: changed tiles flow in, the wire vents at a set rate, and the fate (steady, cyclic or collapsed) is said before running. Not built.
- An OS that reconfigures its own RULES from its load, as Adapt reconfigured its hand. Not built.
