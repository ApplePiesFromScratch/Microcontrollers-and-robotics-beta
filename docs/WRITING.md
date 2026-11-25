> James's writing guide (uploaded 25 Nov 2026), verbatim, then the project's README parts and debug pass (METHOD section 11),
> then the conventions this repo adds. Every doc and lesson block here was written to all three.

# Writing guide

Do not hunt the author. Hunt the output.

A single word is almost never the bug. The bug is a **pattern**: the same mix running again, leftover dropped, a noun doing the verb’s job.

Same errors as the calculi. Different ink.

---

## Why this matters

Readers ask “who wrote this” instead of “does this hold.”
A cluster of patterns trips that switch. Then the claim is not weighed.

Fix the output so a stranger can check it. The label becomes cheap.

---

## Pattern 1. Fluency with no letter

The sentence sounds finished and names nothing you can rerun.

```
Bad:  This highlights the importance of a robust approach in today’s landscape.
Good: rate(mix(x,x), x) at isolate(3,1) is 6.
```

Why: leftover of the thought was sent to a style-word. Error 5 in prose.

---

## Pattern 2. The same hinge, many times

Not X but Y. Rule of three. “Plays a crucial role in shaping.”
Paragraph starts: Furthermore. Moreover. Additionally.

```
Bad:  It’s not just a tool, it’s a movement: fast, scalable, and future-proof.
Good: The CLI posts G, V, mint. If a write leaves V it raises.
```

One hinge is a sentence. Three in a page is a metronome.
Why: one combinator sold as the shape of thought. Error 4.

---

## Pattern 3. Noun where a verb belongs

```
Bad:  We performed an analysis of the implementation.
Good: We ran mint.py. Bits host+ leaked 1 of 16 products.
```

Search `-tion` `-ment` `-ity`. Turn what you can into a verb plus a number.
Why: output booked as a thing. Error 1.

---

## Pattern 4. Hedge or inflator with no scope

```
Bad:  This could perhaps generally be seen as significant.
Good: On V={0,1}, wrap-channel-only still leaks 2*2=4 when V grows to {0,1,2}.
```

`generally` / `significant` / `crucial` without a listed V or a count are empty channels.
A real hedge names the cut: “on this grid,” “not claimed for unlistable Ω.”

---

## Pattern 5. Comment instead of work

First and last sentence of a paragraph.

```
Bad:  It is worth noting that this section will explore the idea.
Good: Empty certify(values=(), seeds=()) must raise.
```

If the sentence talks about the writing, delete it.

---

## Pattern 6. One word treated as a soul

People ban `delve` and keep the rest of the mix.

```
Bad:  Let’s examine the rich structure of the domain.   (delve gone, pattern stays)
Good: List V. Scan every pair-product. Print leak_v and leak_c.
```

`delve` / `tapestry` / em dash are search glyphs.
The pattern is placeholder plus no paid line.
One glyph is allowed. A cluster in 200 words is the mix running.

---

## Pattern 7. Uncheckable claim

```
Bad:  This changes how we do mathematics.
Good: python3 generated_calc.py prints [(2, 4), (3, 6), (-1, -2)].
```

If they cannot run it, weigh it, or name what would kill it, it is a slogan. Error 18.

---

## Ten-minute pass

1. Find sentences with no number, no command, no V.
2. Find repeated hinges (not-but, triples, Furthermore).
3. Find `-tion` stacks.
4. Find first/last sentences that only announce.
5. Replace each leftover with a letter: a count, a runner, a refuse.

Do not ask what the author felt. Ask what the line posts.

---

# The project's README parts and debug pass (METHOD section 11)

Every README the project trusts has the same parts:
1. what it is, in the book's words;
2. what was run, with numbers taken from tool output, labelled desk, emulator or board;
3. what was not run;
4. what was found and fixed on the way;
5. tiers, with the weakest link named;
6. prior art, named, and the narrow claim of what is new ("not finding it isn't proof it doesn't exist").

Debug pass before sending: check the first 40 tokens; cut praise with no row behind it; delete headings with no number or
command; replace "some" and "others" with the named cases; delete a final paragraph that offers more; stamp or cut
metaphors; run each command you claim, or cut the sentence.

---

# Conventions this repo adds (for AI readers and writers)

- **One statement, one check.** Write claims so each could be a row in a verification table: the statement, its scope,
  the method that checks it (calculation, desk, emulator, board, review), and the result.
- **Units and bottoms every time:** "1/16 px", "Q16 (65536 = 1)", "1/64 cm", "emulator instruction count at 160 MHz".
- **Imperative for procedure, declarative for result.** "Seal the kill lines." / "N1 held on 20,000 latches (desk)."
- **Analogy is kept where it pays** and stamped as analogy: the bathtub, the two strips of a growing rectangle, the rope
  between two cars. An analogy that cannot be turned into a check is cut.
- **Refused words** (tools/wordlint.py): `living`, `alive`, `organism`, `creature`, `being` (as a noun), `entity`,
  `entities`, `chaos`, `intelligence`, `impossible`. A word in quotation marks or backticks is only named and passes.
- **Lesson blocks in sketches** use fixed field names an AI can parse: WHAT, WHY, RULE(S), RELATION, HOLDING, BOOKS,
  CUT, BUDGET, TRAP, FOUND, TRY, WITNESS, TIER, KNOWN. A TRY line is a prediction to run, not a result.
