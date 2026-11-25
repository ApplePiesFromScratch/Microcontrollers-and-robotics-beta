#!/usr/bin/env python3
"""build.py: assembles EVERY_NUMBER_IS_A_RULE_CONSOLIDATED.md from the four source workbooks (verbatim, each under a Part
heading) and part5.md. Run it after any change to part5.md; check_workbook.py proves Parts I-IV are the sources unchanged."""
import os
H = os.path.dirname(os.path.abspath(__file__))
PARTS = [("I", "numbers-and-why-they-work.md"), ("II", "letters-that-stand-for-numbers.md"),
         ("III", "shapes-and-why-they-measure-out.md"), ("IV", "every-number-is-a-rule-corrected.md")]
FRONT = open(os.path.join(H, "front.md"), encoding="utf-8").read()
out = [FRONT]
for roman, f in PARTS:
    body = open(os.path.join(H, "sources", f), encoding="utf-8").read()
    out.append(f"\n\n<!-- PART {roman} BEGIN: sources/{f}, verbatim -->\n\n*Part {roman}*\n\n{body}\n<!-- PART {roman} END -->\n")
out.append("\n\n<!-- PART V BEGIN: part5.md -->\n\n" + open(os.path.join(H, "part5.md"), encoding="utf-8").read() + "\n<!-- PART V END -->\n")
out.append("\n\n" + open(os.path.join(H, "notes.md"), encoding="utf-8").read())
open(os.path.join(H, "EVERY_NUMBER_IS_A_RULE_CONSOLIDATED.md"), "w", encoding="utf-8").write("".join(out))
print("EVERY_NUMBER_IS_A_RULE_CONSOLIDATED.md:", sum(len(x) for x in out), "characters")
