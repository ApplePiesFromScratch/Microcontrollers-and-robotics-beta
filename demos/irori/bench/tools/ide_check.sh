#!/bin/bash
# ide_check.sh  irori_c3.ino through the Arduino IDE's own prototype step (arduino-cli + Arduino's ctags), then the RISC-V g++ against stand-ins of the core headers (strict: as C++)
D=$(cd "$(dirname "$0")/.." && pwd); T=/tmp/ide_t; rm -rf $T; mkdir -p $T/irori_c3
cp $D/irori_c3/irori_c3.ino $T/irori_c3/
${ARDUINO_CLI:-arduino-cli} compile -b fake:arch:c3 --preprocess --build-property "build.extra=-march=rv32imc_zicsr -mabi=ilp32 -std=gnu++2a -I$D/tools/stubs" $T/irori_c3 2>$T/c3.err | sed '/^Downloading index/d' > $T/c3.cpp
[ -s $T/c3.cpp ] || { echo "prototype step failed"; grep -v "index\|discovery" $T/c3.err | head; exit 1; }
n=$(grep -c "^#line" $T/c3.cpp)
${MCR_X:-/opt/x}/riscv32-esp-elf/bin/riscv32-esp-elf-g++ -march=rv32imc_zicsr -mabi=ilp32 -std=gnu++2a -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-unused-variable -I$D/tools/stubs -c -x c++ $T/c3.cpp -o $T/c3.o 2>$T/c3.cc
if grep -q "error" $T/c3.cc; then echo "DOES NOT COMPILE"; grep error $T/c3.cc | head -8; exit 1; fi
echo "compiles after the IDE's prototype step ($n #line marks); warnings: $(grep -c warning $T/c3.cc)"; grep warning $T/c3.cc | head -5
${MCR_X:-/opt/x}/riscv32-esp-elf/bin/riscv32-esp-elf-size $T/c3.o | tail -1 | awk '{printf "object: text %d B, data %d B, bss %d B\n", $1, $2, $3}'
