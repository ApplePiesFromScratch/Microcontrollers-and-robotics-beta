> Project doc `claude/TIDE_README.md` (9 Nov 2026), copied unchanged below this line. `tools/check_tide.sh` and
> `tools/tide_test.cpp` live in the project's tree, not here. Here: `bench/desk.c` (the desk twin) and
> `../../tools/desk_panel.c`. Reading note for the reviewer: the sketch's header comment says 400 balls; this README
> and the runs say 232.

# TIDE: a computer that runs backward

TIDE is a small computer, with its own instruction set, running on the ESP32-C3 SuperMini with the 2.0" ST7789. **Every instruction it has can be undone exactly**, and a checker reads each program before it runs and refuses any instruction that could lose a bit. So the whole machine runs backward to the last bit:
- its registers;
- its memory;
- a 76,800-dot picture;
- 232 colliding balls.

Nothing is stored to make that possible except what came in from outside: the hands.

## What it shows

**Rewinding costs only the hands.** On screen at all times is what's been kept and what would have had to be kept:
- In MARBLE, a picture is stirred for 360 steps. Running it back needs **213 bytes** of hand input. Rewinding by keeping a snapshot of every step would need **28 MB**, about seventy times the chip's memory.
- In SHATTER, it needs **15 bytes** against **4.9 MB**.

The machine doesn't save the past; it **works the past out again**, one undone instruction at a time.

**Running back is checked, not assumed.** Every step forward records a print of the entire machine state. Every step back compares against it: "BACK MATCHED 1,040/1,040". At step 0 the whole state must equal the start: "STATE = START". In every run it has.

**The same rule in floating point can't come back.** FORGET is SHATTER instruction for instruction, with the same formulas in the same order; only the kind of number changes, to the chip's own 32-bit float.
- Run forward and back, **0 of 232 balls** end within a dot of home, and only 1 of 1,040 steps back matched.
- Every float step rounds, and a rounding is a bit thrown away. Thrown-away bits can't be worked out again.

**The machine refuses what it can't undo.** At boot the checker reads all six programs. It passes the three that run and refuses three on purpose:
- `ADD R3,R3`: doubling forgets R3's top bit;
- `MULO R2,6`: an even multiplier forgets it too;
- `ADDI R7,1 IF R7`: a step that changes its own condition can't tell, going back, whether it ran.

## The machine

16 registers and 256 words of memory, all 32-bit whole numbers that wrap. Each instruction:

| forward | back | why it can be undone |
|---|---|---|
| `ADD a,b` / `SUB a,b` | `SUB a,b` / `ADD a,b` | a changes by an amount read from b, and b is untouched |
| `XOR a,b`, `XORI a,k`, `NEG a`, `SWAP a,b`, `EXCH a,[b]` | the same | each is its own inverse |
| `ADDI a,k`, `ROL a,k` | `ADDI a,-k`, `ROR a,k` | |
| `MULO a,k` (k odd) | `MULO a,k⁻¹` | an odd number has a whole-number inverse when numbers wrap; the checker finds it |
| `ADDM a,b,c` (a += b·c) | `SUBM a,b,c` | any rule of other registers may be added, because the same rule is subtracted back |
| `TURN a,b,(P:Q)` | `TURN a,b,-(P:Q)` | an exact turn of a pair by a heading, as three shears (see below) |
| `IN a,HANDk` | the same | a ^= the hands' byte at this step, read from the tape both ways |
| `STIRX`, `STIRY` | `UNSTIRX`, `UNSTIRY` | each row (or column) of the picture slides by its own amount: dots only move |
| `LEAP g` | `UNLEAP g` | every ball latches its rate: next = 2·now − before + push(now) |
| `BLAST x,y,s` | `UNBLAST` | a kick out from a point, from where each ball is now |

Any instruction can carry `IF r`: it runs only when r isn't 0, and the checker makes sure the instruction never writes r.

**A step of a program** runs the list downward, then counts the step up. **Running back** counts the step down, then runs the list *upward*, each instruction undone. When time runs back, the listing on the screen turns over to show exactly that.

**The programs** are written in this assembly. They use the classic pattern of compute, use, uncompute. In SHATTER the blast point is computed into R10 from the hands, the blast uses it, and then R10 is subtracted back to 0. Nothing is left lying around that would have to be remembered.

## Where the book comes in

- **The step label is a count of latches, never a variable.** It counts down when the machine runs back. Nothing anywhere is a function of t.
- **The exact turn is the heading's own numbers.** A heading (P : Q) turns by an angle whose half has slope P/Q, with unit sine 2PQ/(P²+Q²). Three shears do the turn: slide x by y·P/Q (landed with the mouth), slide y by x·2PQ/(P²+Q²), slide x again. Each slide is undone by sliding back by the same amount, so the turn is an exact one-to-one map of the whole-number plane.
- **Running back is flipping the cut.** SHATTER's balls are kept as (now, before). Forward writes 2·now − before + push(now) over "before" and swaps the two. Back swaps first, then applies the very same formula. The push can be any rule of where things are now, because it is added one way and taken away the other.
- **A float rounding is the book's "landing" done everywhere, silently.** TIDE lands only where the landing can be undone.

## What was run

**Every instruction, both ways** (`tools/tide_test.cpp`): 20,000 random states for each of the 14 register instructions, 200 random pictures for the stirs, and 200 random worlds for a blast plus 50 leaps. Every one came back exactly: **280,420 trials, 0 failed.** Address and undefined-behaviour sanitizers clean.
- The checker was given 50,000 random instructions. It accepted 37,918, and every one undid exactly. It refused 12,082.
- The contrast, the same strike and 300 steps forward then back: whole numbers came back exactly in 20 of 20 worlds; floats in 0 of 20, with 4 of 4,640 balls within a dot of home.

**The desk twin** (the sketch's own command stream decoded as the panel would): the ghost ran the whole show, 2,440 steps forward and 2,440 back, and the panel equalled the picture on every frame.
- MARBLE: every dot back.
- SHATTER: 232 of 232 balls home, every bit, 1,040 of 1,040 steps matched on the way back.
- FORGET: 0 of 232 home.

**The board build** (arduino-cli, ESP32 core 3.3.12): flash **348,730 bytes**, globals **127,828 bytes**, 199,852 left.

**The emulated chip** (Espressif QEMU, esp32c3): 2,400 frames, the glass **identical to the desk on all 2,400**, including the floating-point program, so the chip's software float rounds exactly as the desk's does. The books are the same: MARBLE back, SHATTER 232 of 232 home, FORGET 0 of 232. Stack: 6,304 words never touched. Heap left: 164,308 bytes.

**Work per frame**, by the emulator's count at 160 MHz:

| program | average | worst |
|---|---|---|
| MARBLE | 22.7 ms | 42.3 ms |
| SHATTER (4 steps a frame) | 14.5 ms | 19.3 ms |
| FORGET (floats, 4 steps a frame) | 27.7 ms | 46.1 ms |

That's QEMU's instruction count, not real cycles. The panel takes about 31 ms of wire a frame at 40 MHz, so the wire sets the pace.

**Found on the way:**
- The first SHATTER filled the sky like a gas, for two reasons:
  - The contacts were too stiff for the step: with six neighbours touching, a ball's push grew faster than the step could follow, and the stepping added energy. Contacts are now softer, with four smaller steps a frame.
  - The word started high. A world that loses nothing lets nothing settle, so every ball bounces back to the height it fell from. The word now stands on the floor.
- The first float test ran one step forward and back, and floats came home 200 of 200 times. One step isn't where floats fail; the roundings pile up over many. The test now runs 300 steps.
- The exact turn multiplied before wrapping (signed overflow, caught by the sanitizer). It now lands in 64 bits and wraps like ADD.

## Hands

| | |
|---|---|
| hold **B** | time runs back |
| let go | time runs forward again, replaying what was kept |
| stick (MARBLE) | push the stir |
| **A** (SHATTER) | strike the word where the stick points |
| **C** or press the stick | next program |

If you rewind and then do something new, the old future is let go. The ledger counts its bytes as "FUTURES DROPPED", the only bits this machine ever destroys, and only because you chose a different future.

Wiring: SCK 4, MOSI 6, RST 10, DC 5, CS 7, BL to 3V3. Stick VRx 0, VRy 1, press 2. Buttons A 3, B 20, C 21, to ground. Board **ESP32C3 Dev Module**, core 3.x, USB CDC On Boot: Enabled. The same wiring as KEA and HIGH NOON.

## Not run

- Your board, stick and buttons, and real frame times.
- The hands' path (rewind, replay, dropped futures) has been built but not driven by a script on the bench; the ghost plays from a tape it wrote in advance.

## What's known, and what I think is new

Reversible computing is an old and deep field:
- Landauer (1961): erasing a bit has a minimum energy cost.
- Bennett (1973): any computation can be made logically reversible; compute, copy, uncompute.
- Fredkin and Toffoli (1982): conservative logic and the billiard-ball computer.
- Reversible languages and processors: Janus (Lutz and Derby, 1982; Yokoyama and Glück, 2007), and MIT's Pendulum processor (1990s).
- Feistel networks: "add any function of the other half" is how many block ciphers are undone.
- Exact integer rotation by three shears: Paeth (1986), and the lifting scheme.
- Exactly reversible whole-number leapfrog: Levesque and Verlet (1993), and Undo (entry 139).

None of that is claimed here.

What I don't know of before:
- a complete reversible machine (instruction set, checker, assembly programs) on a $2 microcontroller, driving real-time full-screen graphics and many-body physics both ways, checked step for step against a print of the whole state;
- the rewind cost shown live as bytes of input against megabytes of snapshots;
- the same program in floats, on the same chip, failing next to it;
- the emulated chip agreeing with the desk dot for dot, floats included.

Not finding it isn't proof it doesn't exist.

Why it might matter: a computer is usually pictured as a machine that throws information away at every step, and pays for it in heat (Landauer). TIDE is a working, checkable picture of the other kind. Its only losses are the ones you choose: the futures you drop.
