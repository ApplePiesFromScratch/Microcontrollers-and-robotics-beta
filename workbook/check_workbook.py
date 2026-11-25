#!/usr/bin/env python3
"""check_workbook.py: (1) Parts I-IV of the consolidated workbook are the source files, byte for byte; (2) every line in a
Part V text block is printed by docs/teach.py or workbook/part5.py; (3) a set of Part IV's answers, recomputed with exact
fractions, appear in Part IV's Answer Key as written. Exit 0 only if all three hold."""
import os, re, subprocess, sys
from fractions import Fraction as F
H = os.path.dirname(os.path.abspath(__file__)); REPO = os.path.dirname(H)
DOC = os.environ.get("WB_DOC", os.path.join(H, "EVERY_NUMBER_IS_A_RULE_CONSOLIDATED.md"))
if sys.argv[1:2] == ["--planted"]:
    import tempfile
    real = open(DOC, encoding="utf-8").read(); caught = 0
    plants = [("Part IV word", "rate(A, B)  =  4 / 2  =  2", "rate(A, B)  =  4 / 2  =  3"),
              ("Part V number", "C 4, L 3, D 1: v = 1/2", "C 4, L 3, D 1: v = 2/3"),
              ("Part I word", "38 + 47", "38 + 48")]
    for name, a, b in plants:
        assert a in real, a
        with tempfile.NamedTemporaryFile("w", suffix=".md", delete=False, encoding="utf-8") as t:
            t.write(real.replace(a, b, 1))
        r = subprocess.run([sys.executable, __file__], env={**os.environ, "WB_DOC": t.name}, capture_output=True, text=True)
        os.unlink(t.name); caught += r.returncode != 0
        print(f"  planted {name}: {'CAUGHT' if r.returncode else 'MISSED'}")
    print(f"check_workbook --planted: {'PASS' if caught == len(plants) else 'FAIL'} (caught {caught} of {len(plants)})")
    sys.exit(0 if caught == len(plants) else 1)
doc = open(DOC, encoding="utf-8").read()
fails = []

for roman, f in (("I", "numbers-and-why-they-work.md"), ("II", "letters-that-stand-for-numbers.md"),
                 ("III", "shapes-and-why-they-measure-out.md"), ("IV", "every-number-is-a-rule-corrected.md")):
    m = re.search(rf"<!-- PART {roman} BEGIN: sources/{re.escape(f)}, verbatim -->\n\n\*Part {roman}\*\n\n(.*)\n<!-- PART {roman} END -->", doc, re.S)
    src = open(os.path.join(H, "sources", f), encoding="utf-8").read()
    ok = bool(m) and m.group(1) == src
    print(f"  {'ok  ' if ok else 'FAIL'} Part {roman} is sources/{f}, byte for byte ({len(src)} characters)")
    if not ok: fails.append(f"part {roman}")

out = set()
for script in (os.path.join(REPO, "docs", "teach.py"), os.path.join(H, "part5.py")):
    out |= set(subprocess.run([sys.executable, script], capture_output=True, text=True, check=True, cwd=os.path.dirname(script)).stdout.splitlines())
p5 = re.search(r"<!-- PART V BEGIN: part5.md -->(.*)<!-- PART V END -->", doc, re.S).group(1)
quoted = [l for b in re.findall(r"```text\n(.*?)```", p5, re.S) for l in b.splitlines() if l.strip()]
missing = [l for l in quoted if l not in out]
for l in missing: print("  FAIL Part V line not printed by teach.py or part5.py:", l[:100])
print(f"  {'ok  ' if not missing else 'FAIL'} Part V: {len(quoted) - len(missing)} of {len(quoted)} quoted lines printed by the scripts")
if missing: fails.append("part V")

key = doc[doc.index("## Check Your Work", doc.index("<!-- PART IV BEGIN")):doc.index("<!-- PART IV END")]
def mix(a, b): return (a[0] * b[0], a[1] * b[0] + a[0] * b[1])
def div(a, b): return (a[0] / b[0], (a[1] * b[0] - b[1] * a[0]) / (b[0] * b[0]))
def fs(x): return str(x.numerator) if x.denominator == 1 else f"{x.numerator}/{x.denominator}"
def pr(p): return f"({fs(p[0])},{fs(p[1])})"
def power(x, n):
    p = x
    for _ in range(n - 1): p = mix(p, x)
    return p
def heading(P, Q): d = P * P + Q * Q; return (F(Q * Q - P * P, d), F(2 * P * Q, d))
X = lambda v: (F(v), F(1))
checks = [
    ("U3.1 x=7", "outputs " + pr(mix(X(7), X(7)))),
    ("U4.1 x=5 cubed", "Outputs " + pr(power(X(5), 3))),
    ("U4.2 x=6 cubed", "Outputs " + pr(power(X(6), 3))),
    ("U4.4 rate of x^5 at 3", str(power(X(3), 5)[1])),
    ("U5.3 seed -1", pr(mix((F(6), F(-1)), (F(6), F(-1))))),
    ("U7.5 x^3 - x at 3", pr((power(X(3), 3)[0] - 3, power(X(3), 3)[1] - 1))),
    ("U10.1 (10,5)/(5,0)", pr(div((F(10), F(5)), (F(5), F(0))))),
    ("U10.2 1/x at 2", pr(div((F(1), F(0)), X(2)))),
    ("U10.3 x/(x+1) at 1", pr(div(X(1), (F(2), F(1))))),
    ("U10.4 1/(x*x) at 2", pr(div((F(1), F(0)), mix(X(2), X(2))))),
    ("U10.6c (x+1)/(x-2) at 3", "Reading " + pr(div((F(4), F(1)), (F(1), F(1))))),
    ("U12.1 P4 at 1/2", fs(sum(F(1, 2) ** k / F([1, 1, 2, 6, 24][k]) for k in range(5)))),
    ("U12.1 shortfall", fs(F(1, 2) ** 4 / 24)),
    ("U13.2 S*C", pr(mix((F(3, 5), F(4, 5)), (F(4, 5), F(-3, 5)))).replace(",", ", ")),
    ("U14.1 heading(3,4)", f"c=(16-9)/25={fs(heading(3, 4)[0])}"),
    ("U14.2 heading(1,4)", f"c=(16-1)/17={fs(heading(1, 4)[0])}"),
    ("U14.4 turn of (1,4)", f"2*4/17 = {fs(F(8, 17))}"),
    ("U9.3 squared distance 36", "Squared distance at that time: 36"),
    ("U11.3 midpoint 91/64 squared", f"Squared: {fs(F(91, 64) ** 2)}"),
    ("U11.3 midpoint 181/128 squared", f"Squared: {fs(F(181, 128) ** 2)}"),
    ("U15.2 first 2^n > 1000", "n=10"),
]
bad = [(n, s) for n, s in checks if s not in key]
for n, s in bad: print(f"  FAIL Part IV answer {n}: expected text {s!r} not found in the Answer Key")
print(f"  {'ok  ' if not bad else 'FAIL'} Part IV: {len(checks) - len(bad)} of {len(checks)} recomputed answers found in the Answer Key as written")
if bad: fails.append("part IV answers")
# the 2-D closest approach of Unit 9's Try It (t = 15/4), by the rate of the squared distance
# t = -(v . r) / (r . r), v = (10, 5), r = (-2, -2)
t = F(10 * 2 + 5 * 2, 4 + 4)
print(f"  {'ok  ' if t == F(15, 4) else 'FAIL'} Part IV Unit 9 Try It: closest approach at t = {fs(t)} (text: 15/4)")
if t != F(15, 4): fails.append("u9")
print("check_workbook:", "PASS" if not fails else "FAIL " + ", ".join(fails))
sys.exit(1 if fails else 0)
