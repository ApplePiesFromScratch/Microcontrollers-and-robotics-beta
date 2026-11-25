# The ledger

`ledger.jsonl` here is the repo's append-only ledger: one JSON line per event, each line carrying the hash of the line
before it (`tools/ledger.py`). Every run, failure, intervention, seal, prediction and result goes in, and so do your own
changes of approach (`self`). A correction is a new event naming the old one. Nothing is edited or deleted.

```
python3 tools/ledger.py add self "starting stage 1"
python3 tools/seal.py practice/work/p07_KILL_LINES.md
python3 tools/ledger.py show
python3 tools/ledger.py verify
```

The chain catches a partial edit; it does not catch a full rewrite of the file. Anchor it by pushing it, or by pasting
the last line's hash somewhere you do not control. Each team member or AI instance may keep its own ledger with
`--ledger ledger/<name>.jsonl`.
