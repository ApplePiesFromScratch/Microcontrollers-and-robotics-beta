#!/usr/bin/env python3
"""teach.py: the lessons behind METHOD.md. Every number METHOD.md quotes from a lesson is printed here.
Exact fractions and whole numbers only, except where a float is put beside them on purpose (marked FLOAT).
Run: python3 teach.py            (all lessons)
     python3 teach.py 4          (one lesson)
"""
import sys
from fractions import Fraction as F

THETA = "THETA"

# ---------- the book's moves on (value, rate) pairs ----------
def isolate(v, r=1): return (F(v), F(r))
def add(a, b): return (a[0] + b[0], a[1] + b[1])
def sub(a, b): return (a[0] - b[0], a[1] - b[1])
def mix(a, b): return (a[0] * b[0], a[1] * b[0] + a[0] * b[1])
def div(a, b):
    if b[0] == 0: return THETA
    q = a[0] / b[0]
    return (q, (a[1] * b[0] - b[1] * a[0]) / (b[0] * b[0]))
def rate(a, b):
    if b[1] == 0: return THETA
    return a[1] / b[1]

def mouth(top, den):
    """nearest whole, exact half to even, mouth(-x) == -mouth(x); den > 0"""
    q, r = divmod(abs(top), den)
    if 2 * r > den or (2 * r == den and q % 2 == 1): q += 1
    return q if top >= 0 else -q

def scatter(state):
    """a stated rule for well-mixed numbers (xorshift 7/9/8 on 16 bits, as pc_scatter)"""
    state ^= (state << 7) & 0xFFFF; state ^= state >> 9; state ^= (state << 8) & 0xFFFF
    return state

def lesson(n, title):
    print(f"\n== {n}. {title}")

# ---------- lessons ----------
def L1():
    lesson(1, "the alphabet, three moves, the seed cancels, theta")
    for seed in (1, F(7, 2), -1):
        x = isolate(3, seed)
        print(f"seed {seed}: x*x = ({mix(x,x)[0]}; {mix(x,x)[1]})  rate(x*x, x) = {rate(mix(x,x), x)}")
    x = isolate(3)
    xxx = mix(mix(x, x), x)
    print(f"x*x*x at 3 = ({xxx[0]}; {xxx[1]})")
    print(f"rate(x*x, still 5) = {rate(mix(x,x), isolate(5, 0))}")

def L2():
    lesson(2, "division is forced; at the hole, rate answers")
    x = isolate(3)
    y = div(add(mix(x, x), isolate(1, 0)), sub(x, isolate(1, 0)))
    print(f"(x*x+1)/(x-1) at 3 = ({y[0]}; {y[1]})")
    x = isolate(1)
    top, bot = sub(mix(x, x), isolate(1, 0)), sub(x, isolate(1, 0))
    print(f"(x*x-1)/(x-1) at 1 = {div(top, bot)}; top ({top[0]}; {top[1]}), bottom ({bot[0]}; {bot[1]}), rate(top, bottom) = {rate(top, bot)}")

def L3():
    lesson(3, "the mouth: one landing, half to even, mirror-true")
    print("mouth 5/2, 7/2, -5/2, -7/2, 10/3:", [mouth(5,2), mouth(7,2), mouth(-5,2), mouth(-7,2), mouth(10,3)])
    fails = sum(1 for d in range(1, 10) for t in range(-3000, 3001) if mouth(-t, d) != -mouth(t, d))
    shift = sum(1 for t in range(-3000, 3001) if (-t >> 1) != -(t >> 1))
    print(f"mouth mirror fails: {fails} of {9*6001}; floor shift (-x>>1) != -(x>>1) on {shift} of 6001")

def L4():
    lesson(4, "carry the remainder: a rate is already a pulse train")
    n, d, ticks = 4, 3, 100000
    acc = count = 0; train = []
    fcount = 0; facc = 0.0; fwrong = 0           # FLOAT beside it
    for k in range(1, ticks + 1):
        acc += n
        while acc >= d: acc -= d; count += 1
        assert d * count + acc == n * k          # the books, every tick
        facc += n / d
        while facc >= 1.0: facc -= 1.0; fcount += 1
        if fcount != (k * n) // d: fwrong += 1
        if k <= 24: train.append(str(count - sum(map(int, train))))
    print(f"first 24 ticks: {''.join(train)}; after {ticks}: count {count}, d*count+acc == n*ticks held every tick; FLOAT accumulator wrong on {fwrong} ticks")

def L5():
    lesson(5, "a direction is two whole numbers; turning without an angle")
    P, Q = 1, 2
    c, s, dd = Q*Q - P*P, 2*P*Q, P*P + Q*Q
    print(f"(1:2) -> c {c}, s {s}, d {dd}; c*c+s*s == d*d: {c*c+s*s == dd*dd}")
    P2, Q2 = P*Q + P*Q, Q*Q - P*P
    print(f"(1:2) then (1:2) -> ({P2}:{Q2}), unit ({Q2*Q2-P2*P2}, {2*P2*Q2})/{P2*P2+Q2*Q2}")
    p, q, fails, snaps = 1, 2, 0, 0
    fx, fy = 1.0, 0.0                            # FLOAT beside it: turn by 0.6, 0.8
    for _ in range(100000):
        p, q = p*2 + q*1, q*2 - p*1              # compose with (1:2)
        while max(abs(p), abs(q)) > (1 << 30):   # snap both together, with the mouth
            p, q = mouth(p, 2), mouth(q, 2); snaps += 1
        cc, ss, d2 = q*q - p*p, 2*p*q, p*p + q*q
        if cc*cc + ss*ss != d2*d2: fails += 1
        fx, fy = 0.6*fx - 0.8*fy, 0.8*fx + 0.6*fy
    print(f"100,000 turns: unit exact on all but {fails} (snaps {snaps}); FLOAT (double) radius^2 - 1 after = {fx*fx+fy*fy-1:.3e}")

def L6():
    lesson(6, "motion is latching; 'when' is a count of latches")
    g = (F(10), F(-8))
    w = THETA if g[1] == 0 else -g[0] / g[1]
    v = g[0]; vals = []
    for _ in range(2): v += g[1]; vals.append(v)
    print(f"gap (10; -8): when = {w} latches; after latches 1, 2: {vals[0]}, {vals[1]}; when for (10; 0) = {THETA}")

def L7():
    lesson(7, "two snapshots and one rule run back exactly")
    N, STEPS = 32, 2000
    def push(xs):  # any rule of present places only: a pull toward 0 and a soft pair push, whole numbers
        out = []
        for i, x in enumerate(xs):
            f = -(x >> 9)
            j = xs[(i + 1) % N]
            if abs(x - j) < 4000: f += (x - j) >> 6
            out.append(f)
        return out
    st = 0xACE1; before = []
    for _ in range(N): st = scatter(st); before.append(st * 3)
    now = [b + 40 for b in before]; start = (list(before), list(now))
    for _ in range(STEPS):
        pp = push(now); nxt = [2*n - b + p for n, b, p in zip(now, before, pp)]; before, now = now, nxt
    before, now = now, before                     # flip the cut
    for _ in range(STEPS):
        pp = push(now); nxt = [2*n - b + p for n, b, p in zip(now, before, pp)]; before, now = now, nxt
    home = sum(1 for a, b in zip(now, start[0]) if a == b)
    # FLOAT beside it, same rule
    fb = [float(x) for x in start[0]]; fn = [float(x) for x in start[1]]
    def fpush(xs):
        out = []
        for i, x in enumerate(xs):
            f = -x / 512.0; j = xs[(i + 1) % N]
            if abs(x - j) < 4000: f += (x - j) / 64.0
            out.append(f)
        return out
    for _ in range(STEPS):
        pp = fpush(fn); nxt = [2*n - b + p for n, b, p in zip(fn, fb, pp)]; fb, fn = fn, nxt
    fb, fn = fn, fb
    for _ in range(STEPS):
        pp = fpush(fn); nxt = [2*n - b + p for n, b, p in zip(fn, fb, pp)]; fb, fn = fn, nxt
    fhome = sum(1 for a, b in zip(fn, start[0]) if abs(a - b) < 0.5)
    print(f"{N} drops, {STEPS} out and {STEPS} back: whole numbers {home} of {N} home exactly; FLOAT {fhome} of {N} within half a unit")

def L8():
    lesson(8, "move a kept quantity; never set it")
    st = 0x1234; h = []
    for _ in range(64): st = scatter(st); h.append(st & 255)
    total = sum(h); a = list(h); b = list(h); broken = 0
    for _ in range(2000):
        flow = [(a[i] - a[(i + 1) % 64]) >> 2 for i in range(64)]
        for i in range(64): a[i] -= flow[i]; a[(i + 1) % 64] += flow[i]
        if sum(a) != total: broken += 1
        b = [(b[i - 1] + 2*b[i] + b[(i + 1) % 64]) // 4 for i in range(64)]   # assignment: a blur
    print(f"transfer: total {total}, broken on {broken} of 2000 steps; assignment (a blur): {total} -> {sum(b)}")

def L9():
    lesson(9, "which answer belongs to which cut")
    A, B = (F(0), F(5)), (F(10), F(-3))
    for name, (p, q) in (("A-B", (A, B)), ("B-A", (B, A))):
        rope = sub(p, q)
        print(f"{name}: rope ({rope[0]}; {rope[1]}), rate(rope, A) = {rate(rope, A)}, grow = rope rate / rope value = {rope[1]/rope[0]}")
    whole = lambda r: (-r[0], -r[1])                # the direction cut pointed the other way: places and rates
    seedm = lambda r: (r[0], -r[1])                 # seed -1: what one latch counts, rates only
    for name, f in (("direction cut flipped", whole), ("seed -1", seedm)):
        rp = sub(f(B), f(A))
        print(f"{name}: rope ({rp[0]}; {rp[1]}), rate(B, A) = {rate(f(B), f(A))}, grow = {rp[1]/rp[0]}")
    rope = sub(B, A)
    print(f"as named (B-A): rope ({rope[0]}; {rope[1]}), rate(B, A) = {rate(B, A)}, grow = {rope[1]/rope[0]}")

def L10():
    lesson(10, "a lost bit is a count of collisions")
    def distinct(f): return len({f(x) & 255 for x in range(256)})
    rows = [("x+x", lambda x: x + x), ("x*6", lambda x: x * 6), ("x*5", lambda x: x * 5),
            ("x+37", lambda x: x + 37), ("x^(x>>3)", lambda x: x ^ (x >> 3)), ("x*3//4 (a rounding)", lambda x: x * 3 // 4)]
    print("; ".join(f"{n}: {distinct(f)} of 256" for n, f in rows))

def L11():
    lesson(11, "Goodhart: a tuned reading against a sealed one")
    # 400 readings: a true class (bit) and a noisy score; tune a threshold on 0..199, read 200..399 once.
    st = 0xBEEF; data = []
    for _ in range(400):
        st = scatter(st); cls = st & 1
        st = scatter(st); noise = (st % 61) - 30
        data.append((cls, 50 * cls + noise))
    tune, sealed = data[:200], data[200:]
    def score(th, rows): return sum(1 for c, x in rows if (x > th) == bool(c))
    best = max(range(-30, 81), key=lambda th: (score(th, tune), -th))
    print(f"threshold tuned on 200: {best}; right on the tuning set {score(best, tune)}/200, on the sealed set {score(best, sealed)}/200")

def L12():
    lesson(12, "a reading taken at two cuts")
    # count 'peaks' of one stepped turning pair (a heading latched) read at two grains
    p, q = 1, 12; c, s, d = q*q - p*p, 2*p*q, p*p + q*q
    x, y = 4096 * d, 0; ys = []
    for _ in range(600):
        x, y = mouth(x * c - y * s, d), mouth(x * s + y * c, d); ys.append(y)
    def peaks(seq, grain):
        g = [mouth(v, grain) for v in seq]
        return sum(1 for i in range(1, len(g) - 1) if g[i] > g[i-1] and g[i] >= g[i+1])
    print("same turning, peaks counted at grain 2^k:", ", ".join(f"k={k}: {peaks(ys, 1 << k)}" for k in (0, 12, 16, 17, 18, 19, 20)))

LESSONS = [L1, L2, L3, L4, L5, L6, L7, L8, L9, L10, L11, L12]
if __name__ == "__main__":
    pick = [int(a) for a in sys.argv[1:]] or range(1, len(LESSONS) + 1)
    for k in pick: LESSONS[k - 1]()
