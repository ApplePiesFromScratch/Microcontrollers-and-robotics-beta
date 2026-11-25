#!/usr/bin/env python3
"""make_ino.py  board/nagare_board.cpp + core/nagare.h + core/sim.h + core/draw.inc -> nagare_c3/nagare_c3.ino, one file for the Arduino IDE"""
import pathlib
D = pathlib.Path(__file__).resolve().parent.parent
body = (D / 'board/nagare_board.cpp').read_text(); ui = (D / 'core/nagare.h').read_text(); sim = (D / 'core/sim.h').read_text(); inc = (D / 'core/draw.inc').read_text()
ui = ui.replace('#include "sim.h"', sim).replace('#include "draw.inc"', inc).replace('#include <stdint.h>\n', '').replace('#include <stddef.h>\n', '')
out = D / 'nagare_c3'; out.mkdir(exist_ok=True)
merged = body.replace('#include "nagare.h"', '// ---------------------------------------------------------------- NAGARE itself (core/sim.h, core/nagare.h, core/draw.inc, pasted by tools/make_ino.py)\n' + ui)
(out / 'nagare_c3.ino').write_text(merged); print('nagare_c3/nagare_c3.ino', len(merged.splitlines()), 'lines')
