#!/usr/bin/env python3
"""make_ino.py: joins RuleKit (from OS 2), the kernel and the Uno body into one .ino, every type first (the Arduino IDE needs that)."""
import sys, os
H = os.path.dirname(os.path.abspath(__file__)) + '/..'
os2 = open(H + '/base/os2.ino').read().split('\n')
ker = open(H + '/kernel.h').read().split('\n')
cut = next(i for i, l in enumerate(ker) if l.startswith('typedef struct { uint16_t C, THETA')) + 1
body = open(H + '/uno_body.inc').read()
top = '''// os4.ino  RULE OS 4 for the Arduino Uno R3. ONE FILE. Paste all of it into an empty Arduino IDE sketch, pick board "Arduino Uno", upload.
// SH1106 128x64 OLED on SPI (SCK 13, MOSI 11, CS 10, DC 9, RST 8) and a KY-023 stick (VRx A0, VRy A1, SW 2), as OS 2.
// This is RuleKit.h, the shared kernel and the sketch joined, with every type first (the IDE needs that). Made by tools/make_ino.py.
// MAKE IT YOURS: the kernel's readings (C THETA G0 S KAPPA VENT DRAG) are edited on the RULES screen and kept; the programs are
// edited on the EDIT screen and kept; the starting readings are RULES_FIRST in the kernel; the factory programs are DEMO below.
'''
cfg = '''#ifndef KTOP16
#define KTOP32                                       // 32-bit tops on the Uno: the same answers as the ESP32-C3 build (see README)
#endif
#define NPL 4
#define PROC_EXTRA uint16_t base;
'''
out = top + '\n'.join(os2[2:13]) + '\n' + cfg + '\n'.join(ker[:cut]) + '\n\n' + '\n'.join(os2[15:413]) + '\n' + '\n'.join(ker[cut:]) + '\n' + body
extra = sys.argv[2] if len(sys.argv) > 2 else ''
open(sys.argv[1], 'w').write(extra + out)
