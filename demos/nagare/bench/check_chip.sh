#!/bin/bash
# check_chip.sh [frames]  NAGARE, desk == the emulated C3 (Espressif QEMU), the demo hand driving both; floats only in the float copy
set -e; D=$(cd "$(dirname "$0")" && pwd); N=${1:-1500}; O=$D/out; T=/tmp/nagc; rm -rf $T; mkdir -p $T $O
X=${MCR_X:-/opt/x}/riscv32-esp-elf/bin/riscv32-esp-elf
gcc -O2 -w -o $O/desk $D/desk/desk.c
$X-gcc -march=rv32imc_zicsr_zifencei -mabi=ilp32 -O2 -ffreestanding -nostdlib -fno-builtin -w -DFRAMES=$N -T $D/bare/link.ld $D/bare/start.S $D/bare/emu.c -o $O/emu.elf -lgcc
$X-size $O/emu.elf | tail -1 | awk '{printf "image: text %d B, data %d B, bss %d B\n", $1, $2, $3}'
python3 -m esptool --chip esp32c3 elf2image --flash-mode dio --flash-freq 40m --flash-size 4MB -o $O/emu.bin $O/emu.elf >/dev/null 2>&1
python3 -m esptool --chip esp32c3 merge-bin --fill-flash-size 4MB -o $O/emu_flash.bin 0x0 $O/emu.bin >/dev/null 2>&1
F=$($X-objdump -d $O/emu.elf | awk -F'\t' '{print $3}' | grep -cE "^f(add|sub|mul|div|sqrt|mv|cvt|ld|sd|lw|sw|eq|lt|le|class|sgnj|min|max|madd|msub)\.?" || true); SF=$($X-nm $O/emu.elf | grep -cE "__(add|sub|mul|div|neg|cmp|eq|ne|lt|le|gt|ge|un|fix|float|extend|trunc)[sdt]f" || true)
echo "float instructions $F, soft-float helpers $SF"
$O/desk $N | grep "^F " > $T/desk.txt
timeout 6000 ${MCR_X:-/opt/x}/qemu/bin/qemu-system-riscv32 -nographic -machine esp32c3 -drive file=$O/emu_flash.bin,if=mtd,format=raw -icount 3 -serial file:$T/uart.txt -monitor none >/dev/null 2>&1 & Q=$!
until grep -aqE "^DONE|Guru" $T/uart.txt 2>/dev/null || ! kill -0 $Q 2>/dev/null; do sleep 2; done; grep -aq Guru $T/uart.txt && echo "THE CHIP STOPPED: $(grep -a -m1 Guru $T/uart.txt)"; kill $Q 2>/dev/null || true
grep -a "^F " $T/uart.txt > $T/chip.txt
cmp -s $T/desk.txt $T/chip.txt && echo "desk == chip ($(wc -l < $T/chip.txt) lines, the whole screen's print every 50 latches): $(tail -1 $T/chip.txt)" || { echo "desk != chip"; diff $T/desk.txt $T/chip.txt | head -4; }
grep -a "^CYCLES" $T/uart.txt | awk '{printf "chip: a frame: the textbook %.1f ms, ours %.1f ms, drawing %.1f ms, worst whole frame %.1f ms (at 160 MHz)\n", $3/160000, $5/160000, $7/160000, $10/160000}'
