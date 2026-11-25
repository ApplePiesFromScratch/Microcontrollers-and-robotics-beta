# HIGH NOON fixes (25 Nov 2026)

`original/highnoon.ino` is the sketch as uploaded. `fixed/highnoon.ino` is the base the lesson blocks go into.
`diff original/highnoon.ino fixed/highnoon.ino` shows every change.

## F1. A refused picture shifted every later picture's id

**Found** while writing lesson 12. `spr_new` returned -1 when the 112,000-byte pool was full, but it did not increment
`sp_n`, and callers ignore the return. Picture ids are positional, so every later picture got the wrong id, and `ospr`
then read entries that were never filled, past the end of the pool. It was a refusal that nobody read, and it turned into
an out-of-bounds read (FRACTIONS rule 8).

**Fix.** A refused picture becomes an empty one: `w = h = 0`, so every later id stays in place, `cput` writes nothing and
`ospr` draws nothing. `sp_refused` counts refusals, and the boot line prints them: `HIGH NOON 3 grown: … picture bytes,
N refused`.

| witness | original | fixed |
|---|---|---|
| desk twin, real pool (112,000), 3,000 frames: panel prints | — | identical to the original's |
| planted pool of 60,000 bytes, ASan + UBSan, 600 frames | global-buffer-overflow in `ospr` | runs; 0 errors |
| real pool, ASan + UBSan, 600 frames | — | 0 errors |

**Not run:** the emulated chip and the board with the fixed file. The pool is not full at 104,972 bytes (README), so on
the board the change shows only as ", 0 refused" in the boot line.
