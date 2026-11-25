#!/usr/bin/env python3
"""p10 check: pick a TRY line from any demo's lesson blocks, write your prediction into the ledger before you run it,
run it, save the tool output to a file, and log the result with that file attached.

  python3 tools/ledger.py add predict "demos/minamo LESSON 6 TRY: <what you will change> -> <what you predict>"
  ... run it, save output to practice/work/p10_output.txt ...
  python3 tools/ledger.py add result "demos/minamo LESSON 6: <what happened>; prediction held|failed" --file practice/work/p10_output.txt

The check: chain unbroken; a predict event naming a demo lesson; a later result event naming the same lesson, saying
held or failed, with an attached output file that still exists and still has the hash the ledger recorded.

  python3 practice/p10_predict_then_run/check.py
  python3 practice/p10_predict_then_run/check.py --planted
"""
import json, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "common")); sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import harness, ledger
TASK = "p10_predict_then_run"


def check(path):
    ls, bad = ledger.verify(path)
    evs = [json.loads(l) for l in ls]
    R = [("ledger chain unbroken", not bad, f"broken at {bad}")]
    key = lambda t: (re.search(r"demos/[\w-]+ LESSON \d+", t) or [None])[0]
    preds = [(e, key(e["text"])) for e in evs if e.get("kind") == "predict" and key(e.get("text", ""))]
    R.append(("a predict event naming demos/<name> LESSON <n>", bool(preds), ""))
    good = None
    for e, k in preds:
        for r in evs:
            if r.get("kind") == "result" and r["n"] > e["n"] and key(r.get("text", "")) == k:
                good = (e, r); break
        if good:
            break
    R.append(("a later result event for the same lesson", bool(good), ""))
    if good:
        r = good[1]
        R.append(("the result says held or failed", bool(re.search(r"\b(held|failed)\b", r["text"])), r["text"][:80]))
        files = r.get("files", {})
        intact = [p for p, h in files.items() if os.path.exists(os.path.join(ledger.REPO, p)) and ledger.sha(os.path.join(ledger.REPO, p)) == h]
        R.append(("an output file is attached, still present, hash unchanged", bool(intact), f"{len(files)} attached, {len(intact)} intact"))
    return R


if __name__ == "__main__":
    harness.main(TASK, check, ledger.DEFAULT, os.path.join(HERE, "planted"))
