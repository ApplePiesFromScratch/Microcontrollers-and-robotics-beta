#!/usr/bin/env python3
"""make_ino.py  board/irori_board.cpp + core/irori.h -> irori_c3/irori_c3.ino, one file for the Arduino IDE (irori.h pasted in place;
every type of irori.h already sits before its first function, which is where the IDE writes its prototypes)"""
import pathlib, re
D = pathlib.Path(__file__).resolve().parent.parent
body = (D / 'board/irori_board.cpp').read_text(); h = (D / 'core/irori.h').read_text().replace('#include <stdint.h>\n', '').replace('#include <stddef.h>\n', '')
assert body.count('#include "irori.h"') == 1
head = body[:body.index('#include "irori.h"')]
assert not re.search(r'^\s*(static\s+)?[a-z_0-9]+\s+\**[a-z_0-9]+\s*\([^;]*\)\s*\{', head, re.M), 'a function before the types'
first_fn = re.search(r'^static [^;=]*\([^;]*\)\s*\{', h, re.M).start(); last_type = max(m.end() for m in re.finditer(r'^\} [a-z_]+_t;|^typedef [^\n]*_t;', h, re.M))
assert last_type < first_fn, 'a type after the first function'
out = D / 'irori_c3'; out.mkdir(exist_ok=True)
merged = body.replace('#include "irori.h"', '// ---------------------------------------------------------------- IRORI itself (core/irori.h, pasted by tools/make_ino.py)\n' + h)
(out / 'irori_c3.ino').write_text(merged)
print('irori_c3/irori_c3.ino', len(merged.splitlines()), 'lines')
