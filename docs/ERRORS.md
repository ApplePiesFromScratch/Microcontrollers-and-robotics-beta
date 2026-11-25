# Errors to avoid, each one hit for real

Every row here happened in this project, most of them more than once. Each has the pattern under it (what was taken for
what), how it was caught, and the guard. Read the patterns, not only the rows: the same pattern turns up in new code.
Doc names refer to the project; rows marked **here** were found while building this repo (25 Nov 2026).

## 1. Arithmetic and containers

| id | what happened | pattern | caught by | guard |
|---|---|---|---|---|
| A1 | `int` is 16 bits on the Uno: 256 * 181 read -19,200 (6+ times) | a width taken as the number's | desk == chip | cast to int32_t before any product; chip == desk on every build |
| A2 | `x >> 1` used for /2 on negatives: floor, not the mouth; (P:Q) and (-P:-Q) drifted (3+ times) | one landing rule taken for another | mirror test | the `mouth` function; mouth(-x) = -mouth(x) checked |
| A3 | C's `/` truncates, Python's `//` floors | a language's rule taken as arithmetic's | a port that disagreed | name the rule in a comment wherever a negative can be divided |
| A4 | a float accumulator for 4/3 was wrong on 33,333 of 100,000 ticks | rounding carried by the rule | the carry's books | the carry: quotient and remainder kept |
| A5 | exact fractions grew to a 1,661,954-bit bottom in 20 steps of 4x(1-x) | exactness taken as free | memory ran out | choose a holding before code (FRACTIONS): bounded, with the gap printed |
| A6 | `Fraction(0.1)` is 3602879701896397/36028797018963968 | a float taken for the decimal it prints | reading the value | never build a fraction from a float; `practice/p01` refuses floats |
| A7 | a refusal code (-1) read later as a number | a refusal written as a value | (HIGH NOON, **here**) a planted small pool under ASan | an empty result in place of a code, or every caller checks; see `demos/highnoon/FIXES.md` |
| A8 | a store into a narrower type aliased a marker (255) | a container's width | chip == desk | widths named at every store |
| A9 | 64-bit divides by a variable in a hot loop: a library call on RV32; 40 ms of a frame | an operation's cost taken as constant | the emulator's count | divide in 32 bits when operands are proven to fit; tables for per-dot divides |
| A10 | a signed left shift of a negative number | undefined behaviour read as arithmetic | UBSan | `i * ONE`, not `i << 16` |

## 2. Building for the Arduino IDE and the chip

| id | what happened | pattern | caught by | guard |
|---|---|---|---|---|
| B1 | a type declared below the first function: the IDE's generated prototypes fail to compile (3 times) | my build taken for the user's build | the user | every type above the first function; build through the IDE's own prototype step (`bench/tools/ide_check.sh`) |
| B2 | two random draws as arguments of one call: x86 and RISC-V evaluate in different orders; HIGH NOON drew 75 of 2,000 frames differently (4 times) | an evaluation order taken as defined | desk == chip | draw into named values first |
| B3 | PROGMEM int16 read back unsigned | a read taken for a type | desk == chip | cast at the read |
| B4 | the desk widened a sum that the chip wrapped | the desk taken as the reference | desk == chip | fixed-width types everywhere a sum can grow |
| B5 | `-flto` folded routines into one stack frame; the stack overflowed | a build flag's effect on memory | board report | paint free RAM at boot and read it; never reason about the stack |
| B6 | 2,004 bytes of static RAM on an Uno: the chip produced nothing | a budget taken as soft | the user's board | stay near 1,750 bytes on a 2 KB part |
| B7 | the first button read after reset can be wrong | the first reading taken as the state | board report | ignore the first 20 frames |
| B8 | the wrong libgcc multilib: an illegal `frrm` instruction on the C3 | a toolchain default | the emulator | `-march=rv32imc_zicsr_zifencei` for the bare image |
| B9 | `div_t` clashed with stdlib's | a name taken as free | the compiler | prefix the project's names |
| B10 | the clang comparison read DIFFER: the binary went to a path that was a directory and printed nothing (**here**) | an empty output taken for a different output | reading the error line | check the run produced lines before comparing |
| B11 | an end-of-line comment swallowed the statement after it, twice in one hour (**here**): once it broke a compile, once it silently stopped a check from running | an edit to one line of a long line | the compiler; a planted wrong formula | after editing a long line, re-read the whole line; give every check a planted fault |

## 3. Checking and instruments

| id | what happened | pattern | caught by | guard |
|---|---|---|---|---|
| C1 | simavr 1.6 mis-skipped after `sbrc`; blamed on the compiler | the instrument trusted | testing the instrument against itself | upstream simavr; three suspects at least: source, compiler, instrument |
| C2 | test lines that could not fail (`or True`): 5 of 7 errors in one study were in the instruments | a check taken as looking | planting a fault | `--planted` on every checker |
| C3 | the first float test ran one step; floats came home 200 of 200 | the cut where the failure does not live | 300 steps | test at the cut where the failure lives |
| C4 | an OS bench's stand-ins did not clip: 198 false mismatches | a stand-in taken for the original | stepping to the first difference | the stand-in checked first; then a real OS 2 bug (an int8_t pen) was found |
| C5 | the books shut while every place froze | conservation taken as correctness | reading the picture | books guard conservation, not liveness; count negatives and motion too (`practice/p05`) |
| C6 | a threshold tuned on 200 scored 186 there and 179 sealed | a tuned reading taken as a witness | the sealed set | kill lines and pass marks sealed before the read (`practice/p07`) |
| C7 | the cost bench overflowed RAM and its stack wrote over its own counters | the instrument changing what it measures | inconsistent counts | measure the instrument's footprint |
| C8 | a test set of headings all from one family passed a rounded rotation (**here**) | one family taken for the space | planting the fault | draw tests from outside the family you built from (`practice/p03`) |
| C9 | NAGARE's N4 control never fired: `ours_latch` was not reset | a control that could not fail | reading the control's count | every control must be seen to fire |
| C10 | a word reader flagged 42 of 51 denials as assertions | a word without its sentence | the author's verdicts | read at the sentence cut; the author's verdicts outrank the reader |
| C11 | an auditor's minimum over checks that ran: a check that did not run could not lower it | an unbroken promise taken as kept | a planted offline run | count "did not read" apart from "read an absence" |

## 4. Descriptions standing in for the thing

| id | what happened | pattern | guard |
|---|---|---|---|
| D1 | the first COWTUNNEL 3 was "a picture of a cow over your last sketch" | the last construction carried forward instead of the target's alphabet | read the target whole, tables and asm included |
| D2 | comments skimmed, asm and tables skipped | a skim replacing the rules with a guess about their output | read the material whole; say what you did not read |
| D3 | Gauss-Seidel on onsets diverged | an imported method where a condition should be stated | state the condition ("no air through the skin"), then solve it |
| D4 | an estimate written as a measurement (3 times) | an estimate taken for a measurement | time each piece alone; look at the picture |
| D5 | a variable named `rate` was fed where momentum was due | a name taken for its relation | ask what relation the variable carries |
| D6 | one damping number killed every swirl | a constant taken for what it damps | name what is damped |
| D7 | steering by heading and pitch had a pole; cells preferred vertical | a coordinate artefact read as behaviour | a direction vector in the thing's own frame |
| D8 | a shared random generator coupled two things the model said were independent | a shared source taken as neutral | each its own scatter |
| D9 | KAERU repeated TIDE | a new build not checked against the back catalogue | list prior builds before calling one new |
| D10 | "That is chaos, measured"; "a living network" in READMEs | a word that writes a reading as a property | `tools/wordlint.py` over every doc |

## 5. Working with James

| id | what happened | guard |
|---|---|---|
| J1 | a board report was argued with | log it open; look for what the emulator does not model |
| J2 | his request for a demo experts can't explain by their tools was turned into a fixed claim and argued with | do not reify the request; build what his tools explain |
| J3 | outside names (dual numbers, autodiff, limits) used for the book's moves | the book's words; say which part is known elsewhere |
| J4 | "impossible otherwise" claimed | never; say what was built and how it was checked |

## 6. The pattern under all of it

Every row is a reading taken at one cut and written as a property: of the number (a width), of the chip (a budget), of
the code (a name), of the instrument (a check that never failed), of the target (a skim), of the reader (a word without
its sentence). The guard is always the same shape: take it at a second cut, and plant the fault that would show the
first reading wrong. A guard that lives in one build's bench does not protect the next build; put it in `check_all.sh`.
