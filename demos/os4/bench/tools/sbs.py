#!/usr/bin/env python3
"""sbs.py out.png label1 a.bin label2 b.bin picks...   side by side, the same frames of two runs, 3x"""
import sys; sys.path.insert(0, '/opt/uno/bench')
from view import frames, img
from PIL import Image, ImageDraw
out, la, a, lb, b = sys.argv[1:6]; picks = [int(x) for x in sys.argv[6:]]
A, B = frames(a), frames(b); S = 3
W = Image.new('L', (2 * (128 * S + 10) + 60, len(picks) * (64 * S + 20) + 20), 30); d = ImageDraw.Draw(W)
d.text((70, 4), la, fill=230); d.text((70 + 128 * S + 10, 4), lb, fill=230)
for i, k in enumerate(picks):
    y = 20 + i * (64 * S + 20); d.text((4, y + 90), 'frame %d' % k, fill=200)
    if k < len(A): W.paste(img(A[k], 0, S), (60, y))
    if k < len(B): W.paste(img(B[k], 0, S), (70 + 128 * S, y))
W.save(out)
