#!/bin/bash
# check_all.sh: every check in the repo, every checker's planted faults, and the desk benches gcc can run.
# Prints one line per check and a summary. Skips (and says so) what needs a tool that is not here.
# Your own practice files are checked only if present in practice/work/.
cd "$(dirname "$0")"
pass=0; fail=0; skip=0
ok()   { echo "  ok    $1"; pass=$((pass+1)); }
bad()  { echo "  FAIL  $1"; fail=$((fail+1)); }
skp()  { echo "  skip  $1"; skip=$((skip+1)); }
run()  { local name="$1"; shift; if out=$("$@" 2>&1); then ok "$name"; else bad "$name"; echo "$out" | tail -5 | sed 's/^/        /'; fi; }

echo "== the repo's own checks"
run "METHOD's quoted lesson lines (docs/check_method.py)"      python3 docs/check_method.py
run "workbook: Parts I-IV verbatim, Part V numbers, Part IV answers" python3 workbook/check_workbook.py
run "workbook checker catches its planted faults"              python3 workbook/check_workbook.py --planted
run "lesson blocks are comments only (tools/lessons.py verify)" python3 tools/lessons.py verify
run "refused words (tools/wordlint.py)"                         python3 tools/wordlint.py
printf 'the chaos of it\n' > /tmp/_lint_plant.md
if python3 tools/wordlint.py /tmp/_lint_plant.md >/dev/null 2>&1; then bad "word lint catches a planted refused word"; else ok "word lint catches a planted refused word"; fi
run "ledger chain (tools/ledger.py verify)"                     python3 tools/ledger.py verify
cp ledger/ledger.jsonl /tmp/_led_plant.jsonl; python3 tools/ledger.py add self "plant" --ledger /tmp/_led_plant.jsonl >/dev/null
python3 tools/ledger.py add self "plant 2" --ledger /tmp/_led_plant.jsonl >/dev/null; sed -i '1s/"text": "/"text": "EDITED /' /tmp/_led_plant.jsonl
if python3 tools/ledger.py verify --ledger /tmp/_led_plant.jsonl >/dev/null 2>&1; then bad "ledger catches a planted edit"; else ok "ledger catches a planted edit"; fi
run "strip proof catches a changed code byte in every sketch" python3 tools/lessons.py verify --planted

echo "== practice checkers against their planted faults"
for c in practice/p*/check.py; do run "$(dirname $c | xargs basename) --planted" python3 $c --planted; done

echo "== your practice work (practice/work/)"
for spec in p01_readings:p01_readings.py p02_carry_mouth:p02_carry.c p03_heading:p03_heading.py p04_run_back:p04_runback.c \
            p05_transfer:p05_transfer.py p06_two_cuts:p06_cuts.json p08_dereify:p08_MAP.md p09_desk_twin:p09_REPORT.md p11_board_map:p11_pins.json; do
  t=${spec%%:*}; f=${spec##*:}
  if [ -f practice/work/$f ]; then run "$t on your work" python3 practice/$t/check.py; else skp "$t: practice/work/$f not written yet"; fi; done
for t in p07_kill_line p10_predict_then_run; do
  if grep -q "\"$( [ $t = p07_kill_line ] && echo read || echo predict )\"" ledger/ledger.jsonl 2>/dev/null; then run "$t on the ledger" python3 practice/$t/check.py; else skp "$t: nothing in the ledger yet"; fi; done

echo "== desk benches (gcc)"
B=/tmp/_mcr_bench; mkdir -p $B
run "tide: desk twin 2,440 frames, books" bash -c "cd demos/tide && gcc -O2 -w -I. -o $B/tide bench/desk.c && $B/tide 2440 | tail -1 | grep -q 'shatter 232/232 1 forget 0/232'"
for n in tide highnoon ace4; do
  run "$n: desk panel, gcc -O0 == gcc -O2 == clang -O2 over 1,200 frames" bash -c "
    gcc -O0 -w -DSKETCH='\"$PWD/demos/$n/$n.ino\"' -o $B/a tools/desk_panel.c && gcc -O2 -w -DSKETCH='\"$PWD/demos/$n/$n.ino\"' -o $B/b tools/desk_panel.c &&
    $B/a 1200 > $B/a.txt && $B/b 1200 > $B/b.txt && [ \$(wc -l < $B/a.txt) = 24 ] && cmp -s $B/a.txt $B/b.txt &&
    { ! command -v clang >/dev/null || { clang -O2 -w -DSKETCH='\"$PWD/demos/$n/$n.ino\"' -o $B/c tools/desk_panel.c && $B/c 1200 | cmp -s - $B/a.txt; }; }"; done
run "minamo: the full film on the desk, way back matched and flat" bash -c "cd demos/minamo/bench && gcc -O2 -w -o $B/mt desk/twin.c && FRAMELINES=1 $B/mt 2501 x 2>&1 | grep -q 'homes 1 bad 0 mirror ok 1243 bad 0'"
run "nagare: N1-N5 self-test" bash -c "cd demos/nagare/bench && gcc -O2 -w -o $B/ns desk/selftest.c && $B/ns | grep -q 'SELFTEST PASSED: 0 failed'"
run "irori: 46-check self-test" bash -c "cd demos/irori/bench && gcc -O2 -w -o $B/is desk/selftest.c && $B/is | grep -q 'SELFTEST PASSED: 0 failed'"
run "candle: desk twin 120 frames" bash -c "cd demos/candle/bench && gcc -O2 -w -o $B/ct desk/twin.c && FRAMELINES=1 $B/ct 120 x | grep -q '^F 120 '"
run "tater: desk twin 300 frames" bash -c "cd demos/tater/bench && gcc -O2 -w -o $B/tt desk/twin.c && FRAMELINES=1 $B/tt 300 x | grep -q '^F 300 '"

echo "== emulated chip (needs MCR_X with riscv32-esp-elf and Espressif QEMU)"
X=${MCR_X:-/opt/x}
if [ -x $X/qemu/bin/qemu-system-riscv32 ] && [ "$CHIP" = 1 ]; then
  run "minamo: desk == emulated chip, 300 frames" bash -c "cd demos/minamo/bench && bash tools/check_c3.sh 300 | grep -q IDENTICAL"
  run "nagare: desk == emulated chip, 300 frames" bash -c "cd demos/nagare/bench && bash check_chip.sh 300 | grep -q 'desk == chip'"
elif [ -x $X/qemu/bin/qemu-system-riscv32 ]; then skp "the emulator is here; run with CHIP=1 to compare desk and chip (minutes each)"
else skp "no emulator at $X (set MCR_X; docs/BOARD_MAPPING.md section 6)"; fi

echo "check_all: $pass passed, $fail failed, $skip skipped"
[ $fail = 0 ]
