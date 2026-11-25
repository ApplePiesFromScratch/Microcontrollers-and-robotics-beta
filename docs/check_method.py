#!/usr/bin/env python3
"""check_method.py: every line inside a ```text block of METHOD.md must appear in teach.py's output."""
import os, re, subprocess, sys
os.chdir(os.path.dirname(os.path.abspath(__file__)))
out = subprocess.run([sys.executable, "teach.py"], capture_output=True, text=True, check=True).stdout.splitlines()
doc = open("METHOD.md").read()
quoted = [l for block in re.findall(r"```text\n(.*?)```", doc, re.S) for l in block.splitlines() if l.strip()]
missing = [l for l in quoted if l not in out]
for l in missing: print("MISSING:", l)
print("PASS" if not missing else "FAIL", f"({len(quoted) - len(missing)} of {len(quoted)} quoted lines found)")
sys.exit(1 if missing else 0)
