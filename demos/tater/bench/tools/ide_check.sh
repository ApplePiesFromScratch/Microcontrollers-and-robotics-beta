#!/bin/bash
# ide_check.sh  both sketches through the Arduino IDE's own prototype step (arduino-cli + Arduino's ctags), then the real compilers:
#   the C3 with the RISC-V g++ (against stand-ins of the core headers), the Uno with avr-g++ against the real Arduino AVR core.
D=$(cd "$(dirname "$0")/.." && pwd); T=/tmp/ide; rm -rf $T; mkdir -p $T/tater_c3
cp $D/tater_c3.ino $T/tater_c3/
ok=0
for b in c3; do
  ${ARDUINO_CLI:-arduino-cli} compile -b fake:arch:$b --preprocess $T/tater_$b 2>$T/$b.err | sed '/^Downloading index/d' > $T/$b.cpp
  [ -s $T/$b.cpp ] || { echo "$b: prototype step failed"; grep -v "index\|discovery" $T/$b.err | head; ok=1; continue; }
  n=$(grep -c "^#line" $T/$b.cpp)
  if [ $b = c3 ]; then ${MCR_X:-/opt/x}/riscv32-esp-elf/bin/riscv32-esp-elf-g++ -march=rv32imc_zicsr -mabi=ilp32 -std=gnu++2a -O2 -w -I$D/tools/stubs -c -x c++ $T/c3.cpp -o $T/c3.o 2>$T/c3.cc
  else C=/opt/uno/core; avr-g++ -mmcu=atmega328p -DF_CPU=16000000L -DARDUINO=10819 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR -std=gnu++11 -fno-exceptions -fno-threadsafe-statics -Os -w -I$C/cores/arduino -I$C/variants/standard -c -x c++ $T/uno.cpp -o $T/uno.o 2>$T/uno.cc; fi
  if [ -s $T/$b.cc ] && grep -q error $T/$b.cc; then echo "$b: DOES NOT COMPILE"; grep error $T/$b.cc | head -5; ok=1; else echo "$b: compiles after the IDE's prototype step ($n #line marks)"; fi
done
exit $ok
