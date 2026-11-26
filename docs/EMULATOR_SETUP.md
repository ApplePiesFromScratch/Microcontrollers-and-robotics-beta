# Emulator setup (optional): desk == emulated chip

**You do not need the emulator to do the course.** Every practice task and every desk bench runs on Python 3 and gcc
alone. When `check_all.sh` prints `skip  no emulator at /opt/x`, that is the expected result on a machine without the
emulator, and it is not a failure. The emulated chip is one more witness, for stage 14 and for checking a sketch before
it goes on a board.

## What the chip checks call

| tool | where the scripts look | version the repo was checked with (25 Nov 2026) |
|---|---|---|
| Espressif RISC-V GCC | `$MCR_X/riscv32-esp-elf/bin/riscv32-esp-elf-gcc` (and `-objcopy`, `-size`) | crosstool-NG esp-14.2.0_20241119 (GCC 14.2.0) |
| Espressif QEMU fork | `$MCR_X/qemu/bin/qemu-system-riscv32` | esp_develop_9.2.2_20250817 |
| esptool | `python3 -m esptool` | v5.6.0 (the scripts use `merge-bin`, the v5 spelling) |
| gcc, python3 | on PATH | any recent |

`MCR_X` is one folder holding `riscv32-esp-elf/` and `qemu/`. The default is `/opt/x`.

## Windows: use WSL2 (Ubuntu)

The bench scripts are bash and the tools are Linux builds. On Windows, run everything inside WSL2.

1. **Turn on WSL2.** In PowerShell as administrator, run `wsl --install -d Ubuntu`, then reboot. If it complains about
   virtualization, enable it in the BIOS/UEFI (Intel VT-x or AMD-V/SVM). `wsl -l -v` should show Ubuntu at VERSION 2.
2. **Put the repo inside WSL**, not under `/mnt/c/...`: `cd ~ && git clone <repo url>`, or unzip there. A checkout made
   by Windows git can turn the bash scripts' line endings into CRLF, and then they fail with `$'\r': command not found`.
   If that happens, run `git config --global core.autocrlf input` and re-clone, or run
   `sed -i 's/\r$//' check_all.sh demos/*/bench/*.sh demos/*/bench/tools/*.sh`.
3. **Install the basics and QEMU's runtime libraries:**
   ```
   sudo apt update
   sudo apt install -y build-essential clang python3 python3-pip git wget libgcrypt20 libglib2.0-0 libpixman-1-0 libsdl2-2.0-0 libslirp0
   pip install --user --break-system-packages "esptool>=5"
   ```
4. **Get the toolchain and QEMU from Espressif's own installer.** It downloads Espressif's official releases and checks
   their hashes:
   ```
   git clone --depth 1 -b v5.5 https://github.com/espressif/esp-idf ~/esp-idf
   cd ~/esp-idf && ./install.sh esp32c3
   python3 tools/idf_tools.py install qemu-riscv32
   ```
   Other ESP-IDF 5.x releases also work; the toolchain version may then differ from the table above, so say which one
   you used in your report.
5. **Make the `MCR_X` folder** from what the installer put under `~/.espressif/tools/`:
   ```
   mkdir -p ~/mcr_x
   ln -sfn $(ls -d ~/.espressif/tools/riscv32-esp-elf/*/riscv32-esp-elf | tail -1) ~/mcr_x/riscv32-esp-elf
   ln -sfn $(ls -d ~/.espressif/tools/qemu-riscv32/*/qemu | tail -1) ~/mcr_x/qemu
   echo 'export MCR_X=$HOME/mcr_x' >> ~/.bashrc && export MCR_X=$HOME/mcr_x
   ```
6. **Verify, before running anything else:**
   ```
   $MCR_X/riscv32-esp-elf/bin/riscv32-esp-elf-gcc --version
   $MCR_X/qemu/bin/qemu-system-riscv32 --version
   python3 -m esptool version
   ```
   If all three print a version, run `CHIP=1 bash check_all.sh`. Each chip comparison takes minutes, and should end with
   `minamo: desk == emulated chip` and `nagare: desk == emulated chip` reported as ok.

## Linux and macOS

On Linux, do steps 3 to 6. On macOS, ESP-IDF's installer supports macOS too: install `brew install gcc python` and then
follow steps 4 to 6, but expect to fix paths yourself. This repo was not checked on macOS.

## When a chip check fails

Two witnesses disagree; the disagreement does not say which one is wrong (`docs/RIGOR.md` item 11). Before you change
anything:
- run the verify commands again;
- record your toolchain, QEMU and esptool versions;
- read the first differing line;
- log it as an open entry in `REVIEW.md`.

**Do not edit a checker or a bench script to make it pass.**
