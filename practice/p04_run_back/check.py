#!/usr/bin/env python3
"""p04 check: next = 2 now - before + push(now), in whole numbers, is its own undoing.

  python3 practice/p04_run_back/check.py             # checks practice/work/p04_runback.c
  python3 practice/p04_run_back/check.py --planted
"""
import os, sys, tempfile, re
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "common"))
import harness
TASK = "p04_run_back"
HERE = os.path.dirname(os.path.abspath(__file__))


def check(path):
    R = []
    src = open(path).read()
    R.append(("no float or double in your file", not re.search(r"\b(float|double)\b", src), "found float or double"))
    with tempfile.TemporaryDirectory() as tmp:
        exe = os.path.join(tmp, "p04")
        ok, err = harness.compile_c([path, os.path.join(HERE, "harness.c")], exe)
        R.append(("compiles with gcc -std=c99 -Wall", ok, err[:200]))
        if not ok:
            return R
        _, out, _ = harness.run([exe])
        mo = re.search(r"OUT moved (\d+) of 256 places, largest change (\d+)", out)
        mb = re.search(r"BACK (\d+) of 256 places home", out)
        R.append(("the field moved: at least 128 of 256 places changed after 2000 latches", bool(mo) and int(mo.group(1)) >= 128, out.strip()[:120]))
        R.append(("all 256 places home exactly after 2000 back", bool(mb) and int(mb.group(1)) == 256, out.strip()[:160]))
    return R


if __name__ == "__main__":
    harness.main(TASK, check, os.path.join(harness.PRACTICE, "work", "p04_runback.c"), os.path.join(HERE, "planted"))
