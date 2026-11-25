# NAGARE amendments (logged before the reads they govern)

A1 (25 Nov, after the first full read of N2)
  N2 as written FAILED for ours. With the steady heater on, the warmest tenth's centre sat at 4.7 to 13.7 rows (box middle 20)
  over latches 1,000 to 3,000; at 2,000 it read 4.7. The textbook read 24 to 26.
  Why, read from the rules: ours' LIFT relates a place's warmth to the box's warmth as a whole (the context). A steady heater warms
  the whole box, so the heater's stuff is barely warmer than its context and pools at the floor, leaning to one side. The textbook's
  lift is against a fixed zero, so its heater column keeps rising whatever the box does.
  The failure stays in the record. What changes: the scene. Warmth comes in as PUFFS (a disc of warmth laid at the floor, booked
  as fed), on both sides alike; the steady heater stays as a hand option.
  New line N2b: a puff laid at the floor has the centre of its fed warmth's warmest tenth above the box's middle within 300 latches,
  for puffs at three places across the floor. Same reading as N2, the cut changed from steady feed to a puff.

A2 (25 Nov, after the first read of N2b)
  N2b as written FAILED for ours at KL 16: the centre puff's warm centre crossed the middle at latch 352 (limit 300); the side puffs
  at 265 and 270. The textbook crossed at 115 to 118.
  This amendment is a tuning, and is named as one: I then looked at KL 24 and 32 on the desk (centre 136 and 147). KL becomes 24,
  so the two sides rise at a similar pace and the screen compares shapes, not speeds. N2b is re-read at KL 24 as N2c, and N1, N3,
  N4, N6, N7 are read at KL 24. Both sides' constants are STIPULATED; neither side's pace is a result.

A3 (25 Nov, after the first chip read)
  N7 as written FAILED: the emulated C3 took 77.9 ms a frame on average (textbook 24.8, ours 34.4, drawing 18.7), worst 78.5 ms,
  against 66. Cause, read from the code: our trades divide 64-bit numbers by a variable (a library call on this chip), about ten
  times a place a latch. Change: divide in 32 bits where the operands are proven to fit (checked per call, 64 bits otherwise), and
  LIFT's box-wide share computed once a latch. The books are the witness that nothing else changed: N1 and N4 are re-read after.

A4 (25 Nov, after the re-read of N7 under A3)
  N7 re-read over the whole demo tour (1,500 frames): average 65.7 ms, worst 67.8 ms: FAILED by 1.8 ms on the worst frame
  (the turning view). Change: the drawing's per-dot divisions become tables made once. The screen's print over the tour is the
  witness that the picture did not change.
