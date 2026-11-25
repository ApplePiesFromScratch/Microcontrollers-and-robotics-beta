#!/usr/bin/env python3
"""lessons.py: lesson blocks in the demo sketches are comments only. This tool proves it.

  python3 tools/lessons.py build NAME      # src: demos/NAME/fixed/*.ino if present, else original/*.ino, + lessons.txt -> demos/NAME/NAME.ino
  python3 tools/lessons.py verify          # every demo: strip the lesson blocks, compare with ORIGINAL.sha256
  python3 tools/lessons.py strip FILE      # print FILE with the lesson blocks removed

A block starts at a line '// LESSON-BEGIN' and ends at '// LESSON-END' (inclusive); stripping them must give back the
original sketch byte for byte. lessons.txt holds entries:

    @ <text that appears in the anchor line, the first such line after the previous anchor>
    <lesson lines, written without the leading '// '>

Each block is inserted directly above its anchor line.
"""
import hashlib, os, re, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEMOS = os.path.join(REPO, "demos")


def strip(text):
    out, skip = [], False
    for line in text.splitlines(keepends=True):
        s = line.strip()
        if s.startswith("// LESSON-BEGIN"):
            skip = True; continue
        if skip and s.startswith("// LESSON-END"):
            skip = False; continue
        if not skip:
            out.append(line)
    if skip:
        raise SystemExit("unclosed LESSON-BEGIN")
    return "".join(out)


def parse(path):
    entries, cur = [], None
    for raw in open(path, encoding="utf-8").read().splitlines():
        if raw.startswith("@ "):
            cur = [raw[2:].strip(), []]; entries.append(cur)
        elif cur is not None:
            cur[1].append(raw)
        elif raw.strip() and not raw.startswith("#"):
            raise SystemExit(f"{path}: text before the first anchor: {raw[:60]}")
    for e in entries:
        while e[1] and not e[1][-1].strip():
            e[1].pop()
    return entries


def build(name):
    d = os.path.join(DEMOS, name)
    base = "fixed" if os.path.isdir(os.path.join(d, "fixed")) else "original"
    src = [f for f in os.listdir(os.path.join(d, base)) if f.endswith(".ino")][0]
    orig = open(os.path.join(d, base, src), encoding="utf-8").read()
    if "LESSON-BEGIN" in orig:
        raise SystemExit("original already holds lesson markers")
    lines = orig.splitlines(keepends=True)
    entries = parse(os.path.join(d, "lessons.txt"))
    at, inserts = 0, {}
    for k, (anchor, body) in enumerate(entries, 1):
        j = next((i for i in range(at, len(lines)) if anchor in lines[i]), None)
        if j is None:
            raise SystemExit(f"{name}: anchor {k} not found after line {at + 1}: {anchor!r}")
        title = body[0] if body else ""
        block = [f"// LESSON-BEGIN {k} of {len(entries)}: {title}\n"] + [("// " + b).rstrip() + "\n" for b in body[1:]] + ["// LESSON-END\n"]
        inserts.setdefault(j, []).extend(block)
        at = j + 1
    out = []
    for i, l in enumerate(lines):
        out.extend(inserts.get(i, []))
        out.append(l)
    text = "".join(out)
    assert strip(text) == orig
    open(os.path.join(d, src), "w", encoding="utf-8").write(text)
    open(os.path.join(d, "ORIGINAL.sha256"), "w").write(f"{hashlib.sha256(orig.encode()).hexdigest()}  {base}/{src}\n")
    print(f"{name}: {len(entries)} lessons into {src} ({len(orig.splitlines())} code lines, {len(text.splitlines())} with lessons)")


def verify():
    bad = n = 0
    for name in sorted(os.listdir(DEMOS)):
        d = os.path.join(DEMOS, name); h = os.path.join(d, "ORIGINAL.sha256")
        if not os.path.exists(h):
            continue
        want, rel = open(h).read().split()
        sketch = os.path.join(d, os.path.basename(rel))
        got = hashlib.sha256(strip(open(sketch, encoding="utf-8").read()).encode()).hexdigest()
        orig = hashlib.sha256(open(os.path.join(d, rel), "rb").read()).hexdigest()
        ok = got == want == orig
        blocks = open(sketch, encoding="utf-8").read().count("// LESSON-BEGIN")
        fx = " (base: fixed/, see FIXES.md)" if rel.startswith("fixed/") else ""
        print(f"  {'ok  ' if ok else 'FAIL'} {name}: {blocks} lesson blocks; stripped == base: {got == want}; base file unchanged: {orig == want}{fx}")
        bad += not ok; n += 1
    print(f"lessons verify: {'PASS' if not bad and n else 'FAIL'} ({n - bad} of {n} sketches: lessons are comments only)")
    return bad == 0 and n > 0


if __name__ == "__main__":
    a = sys.argv[1:]
    if a[:1] == ["build"]:
        for nm in a[1:]:
            build(nm)
    elif a[:2] == ["verify", "--planted"]:
        caught = n = 0                                   # change one code byte in each sketch, in memory: the proof must see it
        for name in sorted(os.listdir(DEMOS)):
            d = os.path.join(DEMOS, name); h = os.path.join(d, "ORIGINAL.sha256")
            if not os.path.exists(h): continue
            want, rel = open(h).read().split(); text = open(os.path.join(d, os.path.basename(rel)), encoding="utf-8").read()
            k = text.index("#include"); planted = text[:k] + "#include " + text[k + 9:].replace("<", "<<", 1)
            n += 1; caught += hashlib.sha256(strip(planted).encode()).hexdigest() != want
        print(f"lessons verify --planted: {'PASS' if caught == n else 'FAIL'} (caught {caught} of {n} changed sketches)")
        sys.exit(0 if caught == n else 1)
    elif a[:1] == ["verify"]:
        sys.exit(0 if verify() else 1)
    elif a[:1] == ["strip"]:
        sys.stdout.write(strip(open(a[1], encoding="utf-8").read()))
    else:
        print(__doc__); sys.exit(2)
