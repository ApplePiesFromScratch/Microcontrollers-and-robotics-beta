#!/usr/bin/env python3
"""p09 check: a desk twin, built three ways (gcc -O0, gcc -O2, clang -O2), must print the same panel hashes; and your
report must quote what ran and say what did not.

practice/work/p09_REPORT.md: first line `sketch: <path>` (one of demos/tide/tide.ino, demos/highnoon/highnoon.ino,
demos/ace4/ace4.ino, or any sketch with a DESK build); a line quoting the last panel print exactly as the twin printed it
after 1200 frames (`F 1200 panel xxxxxxxx`); a section or line starting `Not run` naming what you did not run (at least
the emulated chip and the board, unless you ran them).

  python3 practice/p09_desk_twin/check.py
  python3 practice/p09_desk_twin/check.py --planted
"""
import os, re, shutil, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "common"))
import harness
TASK = "p09_desk_twin"


def check(path):
    R = []
    rep = open(path, encoding="utf-8").read()
    m = re.match(r"sketch:\s*(\S+)", rep)
    R.append(("first line names the sketch", bool(m), rep.splitlines()[0][:60] if rep else "empty"))
    if not m:
        return R
    sk = m.group(1)
    sk = sk if os.path.isabs(sk) else os.path.join(harness.REPO, sk) if not sk.startswith("planted/") else os.path.join(HERE, sk)
    R.append(("the sketch exists", os.path.exists(sk), sk))
    if not os.path.exists(sk):
        return R
    outs = {}
    with tempfile.TemporaryDirectory() as tmp:
        for name, cc, opt in (("gcc -O0", "gcc", "-O0"), ("gcc -O2", "gcc", "-O2"), ("clang -O2", "clang", "-O2")):
            if not shutil.which(cc):
                R.append((f"{name} available", False, f"{cc} not found")); continue
            exe = os.path.join(tmp, name.replace(" ", "_"))
            rc, _, err = harness.run([cc, opt, "-w", f'-DSKETCH="{sk}"', "-o", exe, os.path.join(harness.REPO, "tools", "desk_panel.c")])
            if rc:
                R.append((f"{name} builds", False, err[:160])); continue
            rc, out, _ = harness.run([exe, "1200"], timeout=300)
            lines = [l for l in out.splitlines() if l.startswith("F ")]
            R.append((f"{name} printed 24 panel lines", len(lines) == 24, f"{len(lines)} lines"))
            outs[name] = lines
    if len(outs) >= 2:
        first = next(iter(outs.values()))
        same = all(v == first for v in outs.values())
        R.append((f"the {len(outs)} builds print identical lines", same, "; ".join(f"{k}: {v[-1] if v else '-'}" for k, v in outs.items())))
        last = first[-1] if first else ""
        R.append((f"report quotes the last print ({last})", bool(last) and last in rep, ""))
    R.append(("report has a 'Not run' line naming what did not run", bool(re.search(r"^\W*Not run", rep, re.M | re.I)), ""))
    return R


if __name__ == "__main__":
    harness.main(TASK, check, os.path.join(harness.PRACTICE, "work", "p09_REPORT.md"), os.path.join(HERE, "planted"))
