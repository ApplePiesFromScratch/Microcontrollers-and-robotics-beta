#!/usr/bin/env python3
"""part5.py: every number in Part V of the consolidated workbook that teach.py does not already print.
check_workbook.py fails if a line quoted in a Part V text block appears in neither output."""
from fractions import Fraction as F

def mouth(t, d):
    a = abs(t); q, r = divmod(a, d)
    if 2 * r > d or (2 * r == d and q % 2): q += 1
    return -q if t < 0 else q

# V1: the carry at 7/3, by hand-sized numbers
acc = cnt = 0; out = []
for k in range(1, 7):
    acc += 7; e = 0
    while acc >= 3: acc -= 3; e += 1; cnt += 1
    out.append(f"tick {k}: emit {e}, carry {acc}, count {cnt}, 3*count + carry = {3 * cnt + acc} = 7*{k}")
print(*out, sep="\n")

# V2: the mouth on a short table, and the two "round down" rules
print("mouth of 1/2, 3/2, 5/2, -1/2, -3/2: " + ", ".join(str(mouth(t, 2)) for t in (1, 3, 5, -1, -3)))
print("toward the more negative (//) of -7/2: " + str(-7 // 2) + "; toward zero of -7/2: " + str(int(-7 / 2)))

# V5: conservation is not correctness (the p05 planted fault)
h = [0] * 64
for i in range(20, 28): h[i] = 1000
for s in range(40):
    new = list(h)
    for i in range(64):
        j = (i + 1) % 64; d = h[i] - h[j]; f = 3 * d // 4 if d >= 0 else -(3 * -d // 4)
        new[i] -= f; new[j] += f
    h = new
print(f"three quarters a face, 40 steps: total {sum(h)} (start 8000), places below zero {sum(1 for v in h if v < 0)} of 64")

# V6: the lever's three zeros on one small sum: total = price*qty + fee*fee - floor*0
def rd(v, src=None):
    return (F(v), {src: F(1)} if src else {})
def mul(a, b):
    v = a[0] * b[0]; r = {}
    for k in set(a[1]) | set(b[1]):
        r[k] = a[1].get(k, 0) * b[0] + a[0] * b[1].get(k, 0)
    return (v, r)
def add(a, b):
    return (a[0] + b[0], {k: a[1].get(k, 0) + b[1].get(k, 0) for k in set(a[1]) | set(b[1])})
price, qty, fee, floor_, tax = rd(12, "price"), rd(3, "qty"), rd(0, "fee"), rd(5, "floor"), rd(2)
total = add(add(mul(price, qty), mul(fee, fee)), mul(floor_, rd(0)))
for k in ("price", "qty", "fee", "floor", "tax"):
    state = "absent" if k not in total[1] else ("still" if total[1][k] == 0 else f"moving, rate {total[1][k]}")
    print(f"total = {total[0]}: {k} {state}")

# V7: a heading turns the plane; rounding the turn does not always
box = [(x, y) for x in range(-30, 31) for y in range(-30, 31)]
for P, Q in ((1, 2), (1, 4)):
    c, s, d = Q * Q - P * P, 2 * P * Q, P * P + Q * Q
    rounded = {(mouth(c * x - s * y, d), mouth(s * x + c * y, d)) for x, y in box}
    sheared = set()
    for x, y in box:
        a = x - mouth(y * P, Q); b = y + mouth(a * 2 * P * Q, d); a = a - mouth(b * P, Q); sheared.add((a, b))
    print(f"({P}:{Q}) on {len(box)} points: rounded turn {len(box) - len(rounded)} collisions; three shears {len(box) - len(sheared)} collisions")

# V9: propagation v = C / (C + L + D)
for C, L, D in ((4, 0, 1), (4, 3, 1), (2, 3, 1)):
    v = F(C, C + L + D)
    print(f"C {C}, L {L}, D {D}: v = {v}" + ("; L >= C: reconfigure" if L >= C else ""))
