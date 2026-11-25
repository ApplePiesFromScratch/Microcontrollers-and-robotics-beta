#!/bin/bash
# check_c3.sh [frames]  desk twin vs the emulated C3: the room's print at boot, then every frame (the flicker's print and every byte down the wire); cost; no float
set -e
D=$(cd "$(dirname "$0")/.." && pwd); O=$D/out; N=${1:-600}; T=/tmp/candlec; rm -rf $T; mkdir -p $T $O
X=${MCR_X:-/opt/x}/riscv32-esp-elf/bin/riscv32-esp-elf
gcc -O2 -w -o $O/twin $D/desk/twin.c
$X-gcc -march=rv32imc_zicsr -mabi=ilp32 -O3 -ffreestanding -nostdlib -fno-builtin -w -DFRAMES=$N -T $D/bare/link.ld $D/bare/start.S $D/bare/emu.c -o $O/emu.elf -lgcc
python3 -m esptool --chip esp32c3 elf2image --flash-mode dio --flash-freq 40m --flash-size 4MB -o $O/emu.bin $O/emu.elf >/dev/null 2>&1
python3 -m esptool --chip esp32c3 merge-bin --fill-flash-size 4MB -o $O/emu_flash.bin 0x0 $O/emu.bin >/dev/null 2>&1
$X-size $O/emu.elf | tail -1 | awk '{printf "image: text %d B, data %d B, bss %d B (loader limit 241,152 B of data with the stack)\n", $1, $2, $3}'
F=$($X-objdump -d $O/emu.elf | awk -F'\t' '{print $3}' | grep -cE "^f(add|sub|mul|div|sqrt|mv|cvt|ld|sd|lw|sw|eq|lt|le|class|sgnj|min|max|madd|msub)\.?" || true); S=$($X-nm $O/emu.elf | grep -cE "__(add|sub|mul|div|neg|cmp|eq|ne|lt|le|gt|ge|un|fix|float|extend|trunc)[sdt]f" || true)
echo "float instructions: $F   soft-float helpers: $S"
FRAMELINES=1 $O/twin $N x > $T/desk.txt
timeout 9000 ${MCR_X:-/opt/x}/qemu/bin/qemu-system-riscv32 -nographic -machine esp32c3 -drive file=$O/emu_flash.bin,if=mtd,format=raw -icount 3 -serial file:$T/uart.txt -monitor none >/dev/null 2>&1 & P=$!
until grep -aq "^DONE" $T/uart.txt 2>/dev/null || ! kill -0 $P 2>/dev/null; do sleep 3; done; kill $P 2>/dev/null || true
grep -a "^ROOM" $T/uart.txt | awk '{print "chip: ROOM", $2, "cells", $4, "| boot", sprintf("%.2f", $6/160e6), "s at 160 MHz"}'; grep "^ROOM" $T/desk.txt | awk '{print "desk: ROOM", $2, "cells", $4}'
grep -a "^F " $T/uart.txt | sed 's/ cycles .*//' > $T/e.txt; grep "^F " $T/desk.txt > $T/d.txt
r1=$(grep -a "^ROOM" $T/uart.txt | awk '{print $2}'); r2=$(grep "^ROOM" $T/desk.txt | awk '{print $2}'); [ "$r1" = "$r2" ] && echo "room: chip == desk" || echo "room: DIFFERENT"
cmp -s $T/d.txt $T/e.txt && echo "IDENTICAL: $(wc -l < $T/e.txt) frames: the flicker and every byte down the wire, chip == desk" || { echo DIFFERENT; diff $T/d.txt $T/e.txt | head -5; }
grep -a "^F " $T/uart.txt | awk '{c=$8; h=$10; f=$12; t+=c; H+=h; FF+=f; N++; if (c > M) M = c} END {printf "emulator count at 160 MHz: a frame %.1f ms on average (half cells %.1f, full dots %.1f), worst %.1f. The wire (153,600 B at 40 MHz, 30.7 ms) runs beside it.\n", t/N/160000, H/N/160000, FF/N/160000, M/160000}'
cp $T/uart.txt $O/uart_last.txt
