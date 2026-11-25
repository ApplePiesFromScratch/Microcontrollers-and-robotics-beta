#!/usr/bin/env python3
"""log.py  append one development event to ledger.jsonl, as it happens (never edited afterwards; a correction is a new event).
usage: log.py kind key=value ...   kinds: claim, witness, fail, culprit, reconfig, pass"""
import sys, json, time
e = {'n': sum(1 for _ in open('ledger.jsonl')) if __import__('os').path.exists('ledger.jsonl') else 0, 'kind': sys.argv[1]}
for a in sys.argv[2:]:
    k, v = a.split('=', 1); e[k] = v
open('ledger.jsonl', 'a').write(json.dumps(e) + '\n'); print('logged', e['n'], e['kind'])
