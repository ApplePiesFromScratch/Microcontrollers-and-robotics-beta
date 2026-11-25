#!/usr/bin/env python3
"""name_check.py  every name in a sketch (SKETCH=path; default hitomi_c3.ino) against every macro the real build can put in scope:
   ESP-IDF's headers, the Arduino ESP32 core and its esp32c3 variant, and the toolchain's newlib. A macro silently turns a
   variable into a number (HZ is 60 in sys/param.h: that is how AEON's first build on the board failed, 21 Nov 2026).
   usage: name_check.py [idf_dir] [arduino_core_dir]   (sparse clones: see README)"""
import re, os, sys
D = os.path.dirname(os.path.abspath(__file__)) + '/..'
roots = [sys.argv[1] if len(sys.argv) > 1 else '/tmp/idf/components', sys.argv[2] if len(sys.argv) > 2 else '/tmp/ae', '/opt/x/riscv32-esp-elf/riscv32-esp-elf/include']
macros = {}
for root in roots:
    for d, _, fs in os.walk(root):
        for f in fs:
            if f.endswith('.h'):
                t = open(os.path.join(d, f), errors='ignore').read()
                for m in re.finditer(r'^\s*#\s*define\s+([A-Za-z_]\w*)(\()?', t, re.M): macros.setdefault(m.group(1), (os.path.relpath(os.path.join(d, f), root), bool(m.group(2))))
src = open(os.environ.get('SKETCH', f'{D}/hitomi_c3.ino')).read(); print('sketch:', os.environ.get('SKETCH', 'hitomi_c3.ino'))
own = set(re.findall(r'^\s*#\s*define\s+([A-Za-z_]\w*)', src, re.M))
code = re.sub(r'/\*.*?\*/', '', src, flags=re.S); code = re.sub(r'//[^\n]*', '', code); code = re.sub(r'"(\\.|[^"\\])*"', '""', code)
code = re.sub(r'^\s*#.*$', '', code, flags=re.M)
toks = set(re.findall(r'\b[A-Za-z_]\w*\b', code))
USED = {'IRAM_ATTR', 'portMAX_DELAY', 'INPUT_PULLUP', 'OUTPUT', 'HIGH', 'LOW', 'SPI2_HOST', 'SPI_DMA_CH_AUTO', 'SPI_TRANS_USE_TXDATA', 'GPIO_MODE_OUTPUT',
        'memcpy', 'memset', 'char', 'const', 'inline', 'int', 'unsigned', 'signed', 'long', 'short', 'void', 'volatile', 'static', 'NULL', 'true', 'false', 'bool'}
USED |= {'Serial', 'digitalRead', 'digitalWrite', 'pinMode', 'delay', 'millis', 'micros', 'INPUT', 'analogRead', 'ledcAttach', 'ledcDetach', 'ledcWrite', 'CHANGE', 'RTC_NOINIT_ATTR', 'attachInterruptArg', 'detachInterrupt', 'portENTER_CRITICAL', 'portEXIT_CRITICAL', 'portENTER_CRITICAL_ISR', 'portEXIT_CRITICAL_ISR', 'portMUX_INITIALIZER_UNLOCKED'}   # the core's own API, used as the core means it
hit = sorted(h for h in (toks & set(macros)) - own - USED if not macros[h][1] or re.search(r'\b' + h + r'\s*\(', code))   # a function-like macro bites only where the name meets a bracket
print(f'{len(macros)} macros known from {len(roots)} trees')
for h in hit: print(f'  CLASH: {h}  (a macro in {macros[h][0]})')
print('no clashes' if not hit else f'{len(hit)} clashes'); sys.exit(1 if hit else 0)
