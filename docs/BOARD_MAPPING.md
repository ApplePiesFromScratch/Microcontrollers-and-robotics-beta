# Mapping a circuit board, and finding a configuration that holds

A board is a set of relations under constraint: which signal owns which pin, what each pin must read at reset, how many
bytes a frame may cost, how long the wire takes. A configuration is stable when every one of those relations holds at
once, at every cut you can test (reset, boot, every frame, every app combination). This file maps the board this project
uses, then gives the procedure for finding a new configuration, with its checker in `practice/p11_board_map`.

## 1. The project's board

ESP32-C3 SuperMini (RISC-V RV32IMC, 160 MHz, no FPU, 400 KB SRAM, in-package flash) and a 2.0" ST7789 panel driven at
320 × 240 landscape. Optional: a KY-023 stick and three buttons to ground.

| signal | GPIO | notes |
|---|---|---|
| panel SCK | 4 | SPI clock, 40 MHz (`SAFE_SPI` drops to 20; `WIRE_80` in ACE 4 raises to 80) |
| panel MOSI | 6 | |
| panel DC | 5 | data/command |
| panel CS | 7 | |
| panel RST | 10 | |
| panel BL | 3V3 | backlight always on |
| stick VRx, VRy | 0, 1 | ADC1 |
| stick press | 2 | strapping pin: must read 1 at reset; the pull-up holds it unless pressed during reset |
| button A | 3 | to ground, pull-up |
| button B | 20 | UART0 RX; serial logs ride USB CDC on 18/19, so UART0 is free |
| button C | 21 | UART0 TX, as B |
| LED | 8 | on the SuperMini; also a strapping pin |
| BOOT | 9 | strapping pin with internal pull-up; 0 at reset enters download boot |

Arduino IDE: board "ESP32C3 Dev Module", core 3.x, **USB CDC On Boot: Enabled** (the serial monitor then runs over the
native USB on GPIO 18/19). The fixes at the top of each sketch (`RGB_ORDER`/`BGR_ORDER`, `NO_INVERT`, `ROT180`,
`SAFE_SPI`, `JOY_*`) adapt a panel batch or a stick without touching a rule. Colour order is a property of the panel
batch: James's panel showed red and blue swapped under HIGH NOON's default.

The Uno builds (LACE, OS 4's Uno side): ATmega328P, 2 KB RAM, SH1106 128 × 64 OLED on SPI (SCK 13, MOSI 11, CS 10, DC 9,
RST 8), stick A0, A1, press D2.

## 2. The chip's facts, with their source

Read from the ESP32-C3 datasheet and Espressif's board guides on 25 Nov 2026 (re-read them for a different chip
revision; these are kind 2 statements, from documents):

- GPIO 2, 8 and 9 are **strapping pins**, sampled at reset. GPIO 9 has an internal pull-up and selects download boot
  when 0. GPIO 2 must be 1 for flash boot. GPIO 8 = 0 with GPIO 9 = 0 is invalid.
- GPIO 11 to 17 serve the **in-package flash** (11 is VDD_SPI); not available.
- GPIO 18 and 19 are **USB D− and D+**.
- GPIO 20 and 21 are **UART0 RX and TX**.
- **ADC1** is GPIO 0 to 4; GPIO 5 is ADC2. IRORI's `is_analog(pin)` is `pin <= 4` for this reason.
- The SuperMini exposes GPIO 0 to 10, 20 and 21: 13 pins. The project wiring uses 11 of them. The two left, 8 and 9,
  are both strapping pins (8 also lights the LED); IRORI lets the user claim them for I2C.

## 3. Budgets as gradients

A budget is not a property of the chip. It is a relation between what the build asks and what the part supplies, read at
a cut (a frame, a boot, a worst case). State each with its cut and its witness.

| budget | supply | how it was read | what pays when it runs short |
|---|---|---|---|
| RAM | 400 KB SRAM; the bare emulator image's loader takes 241,152 B of data with the stack | `size` on the image; heap and stack printed at the end of a bench run | band rendering: 16 rows at a time, never a full frame buffer of 153,600 B |
| the wire | 153,600 B a full frame: about 31 ms at 40 MHz, 15.4 ms at 80 MHz | arithmetic on the clock; board's printed frame time | DMA: draw the next band while the last is sent; send only changed tiles (OS 4, IRORI, ACE 4) |
| CPU | 160 MHz, RV32IMC, no FPU | emulator instruction counts (`-icount 3`), labelled as counts, not cycles; the board prints real times | whole numbers only; tables made once at boot; no 64-bit divide by a variable in hot loops |
| flash | a few MB; HIGH NOON 374,104 B, TIDE 348,730 B (READMEs) | arduino-cli build report | art grown from rules at boot, not stored |
| Uno RAM | 2,048 B; keep static near 1,750 | paint free RAM at boot and read the untouched bytes | 1-bit glass as the only store (LACE) |

## 4. Finding a new configuration

1. **List every signal the build needs**, with its role: output, input with pull-up, analog, PWM, SPI, I2C, UART.
2. **List every constraint as a check** (section 2 and the panel's own: CS low while sending, RST pulsed at power-up).
3. **Place the signals** and run the checks (`practice/p11_board_map/check.py`). Each failure names the relation broken.
4. **When the pins run out, reconfigure a relation, not a pin.** Examples, each with what pays for it:
   - tie the panel's CS to ground when it is the only device on its SPI bus (frees a GPIO; the panel is always selected);
   - reset the panel by an RC to 3V3 at power-up and send the software reset 0x01 (frees RST);
   - put I2C on 8 and 9: the bus pull-ups hold both high at reset, which is what flash boot needs (IRORI does this);
   - move a button off a strapping pin, or accept and write down that pressing it during reset changes the boot.
5. **Write every strapping or UART use down** with its level at reset. The checker refuses a strapping use with no note.
6. **Test at the cut where the failure lives:** a configuration that works after boot can fail at reset. Power-cycle
   with each button held; record what happens.
7. **Tier it.** The checker's PASS is FORCED against the listed facts. Behaviour on a board is UNPAID until a board runs
   it; James's board report outranks the checker.

## 5. A board map as code

IRORI keeps the map as a relation the OS asks every time: `pin_user(pin, me)` returns who owns a pin, in a stated order
(hands, I2C, SERVO, TIMER, RULES, then PINS), computed from the current settings. Changing one setting can never leave two
owners on one pin, because ownership is never stored. See `demos/irori/irori_c3.ino`, lesson 12.

## 6. The bench: desk twin and emulated chip

Every C3 sketch here builds two ways from one source and the two are compared frame by frame.

- **Desk twin:** `gcc -O2` on the sketch's DESK build or its `bench/desk/*.c`; the panel is decoded from the sketch's own
  command stream. Needs only gcc. `tools/desk_panel.c` does this for TIDE, HIGH NOON and ACE 4.
- **Emulated chip:** the bare image (`bench/bare/start.S`, `link.ld`, `emu.c`) built with Espressif's RISC-V GCC and run
  in Espressif's QEMU fork (`-machine esp32c3 -icount 3`). The bench scripts read `MCR_X` (default `/opt/x`) for a folder
  holding `riscv32-esp-elf/bin/` and `qemu/bin/`. Get them from Espressif's releases: the riscv32-esp-elf toolchain
  (github.com/espressif/crosstool-NG) and QEMU (github.com/espressif/qemu). `esptool` (pip) makes the flash image.
- **The real build:** arduino-cli with the esp32 core from
  `https://espressif.github.io/arduino-esp32/package_esp32_index.json` (cores 3.3.x and 4.0.0 were installable on 25 Nov
  2026). The scripts read `ARDUINO_CLI` (default `arduino-cli`). `bench/tools/ide_check.sh` runs the IDE's own prototype
  step, so a sketch that compiles here compiles in the IDE.

Step-by-step setup (Windows via WSL2, Linux): `docs/EMULATOR_SETUP.md`. Not every bench was re-run for this repo; `check_all.sh` says which ran and which were skipped for want of `MCR_X`.
