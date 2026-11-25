#!/usr/bin/env python3
"""p02 check: the mouth (one landing, nearest, half to even, mirror-true) and the carry (a rate as a pulse train).

  python3 practice/p02_carry_mouth/check.py             # checks practice/work/p02_carry.c
  python3 practice/p02_carry_mouth/check.py --planted
"""
import os, sys, tempfile
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "common"))
import harness
TASK = "p02_carry_mouth"
HERE = os.path.dirname(os.path.abspath(__file__))


def check(path):
    R = []
    with tempfile.TemporaryDirectory() as tmp:
        exe = os.path.join(tmp, "p02")
        ok, err = harness.compile_c([path, os.path.join(HERE, "harness.c")], exe)
        R.append(("compiles with gcc -std=c99 -Wall", ok, err[:200]))
        if not ok:
            return R
        _, out, _ = harness.run([exe, "m"])
        rows = [tuple(map(int, l.split())) for l in out.splitlines()]
        tab = {(t, d): m for t, d, m in rows}
        for t, d, want in ((5, 2, 2), (7, 2, 4), (-5, 2, -2), (-7, 2, -4), (10, 3, 3), (-10, 3, -3), (1, 2, 0), (3, 2, 2)):
            got = tab.get((t, d))
            R.append((f"mouth({t}/{d}) = {want}", got == want, f"got {got}"))
        near = sum(1 for (t, d), m in tab.items() if abs(2 * (t - m * d)) > d)
        tie = sum(1 for (t, d), m in tab.items() if abs(2 * (t - m * d)) == d and m % 2)
        mirror = sum(1 for (t, d), m in tab.items() if tab.get((-t, d)) != -m)
        R.append((f"nearest on all {len(tab)} cases", near == 0, f"{near} not nearest"))
        R.append(("exact halves land on the even neighbour", tie == 0, f"{tie} odd landings"))
        R.append(("mouth(-x) = -mouth(x)", mirror == 0, f"{mirror} mirror fails"))
        for n, d, ticks in ((4, 3, 100000), (7, 3, 3000), (3, 7, 3000), (1, 1, 50), (65535, 256, 4000)):
            _, out, _ = harness.run([exe, "p", str(n), str(d), str(ticks)])
            lines = [tuple(map(int, l.split())) for l in out.splitlines()]
            if len(lines) != ticks:
                R.append((f"pulse {n}/{d}: {ticks} ticks printed", False, f"{len(lines)} lines")); continue
            books = sum(1 for k, (e, acc, cnt) in enumerate(lines, 1) if d * cnt + acc != n * k)
            bound = sum(1 for e, acc, cnt in lines if not (0 <= acc < d))
            R.append((f"pulse {n}/{d}: books d*count + acc == n*ticks on all {ticks} ticks", books == 0, f"{books} broken"))
            R.append((f"pulse {n}/{d}: 0 <= acc < d on all {ticks} ticks", bound == 0, f"{bound} out of bound"))
            if (n, d) == (4, 3):
                pat = "".join(str(e) for e, _, _ in lines[:24])
                R.append(("pulse 4/3: first 24 ticks 112112...", pat == "112" * 8, pat))
                R.append(("pulse 4/3: count 133333 after 100000 ticks", lines[-1][2] == 133333, str(lines[-1][2])))
    return R


if __name__ == "__main__":
    harness.main(TASK, check, os.path.join(harness.PRACTICE, "work", "p02_carry.c"), os.path.join(HERE, "planted"))
