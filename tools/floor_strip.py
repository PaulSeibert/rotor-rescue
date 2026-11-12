#!/usr/bin/env python3
# usage: python3 tools/floor_strip.py build/agk/a500/floor_strip OUT.png (after: agk run -f tools/floor_strip.agk -o build/agk/a500/floor_strip)
# The perspective floor's proof strip: the lines 160-228 of 12 pictures, 4
# frames (12 px of camera) apart, side by side, with each band's speed marked.
import sys
from PIL import Image, ImageDraw
src, out = sys.argv[1], sys.argv[2]
Y0, Y1, SCALE = 162, 226, 2
shots = [Image.open('%s/s%02d.screen.png' % (src, i)).convert('RGB') for i in range(12)]
W = 320 * SCALE
H = (Y1 - Y0) * SCALE
LABEL = 150
img = Image.new('RGB', (W + LABEL, 24 + 12 * (H + 4)), (16, 16, 24))
d = ImageDraw.Draw(img)
d.text((8, 6), 'Rotor Rescue perspective floor (placeholder art), a500, camera 3 px/frame, 4 frames apart', fill=(230, 230, 230))
for i, s in enumerate(shots):
    y = 24 + i * (H + 4)
    img.paste(s.crop((0, Y0, 320, Y1)).resize((W, H), Image.NEAREST), (0, y))
    d.text((W + 6, y + 2), 'frame +%d' % (4 * i), fill=(200, 200, 200))
    if i == 0:
        for line, txt in ((170, 'far strip 0.75'), (181, 'floor j1 0.79'), (186, 'floor j6 1.00'),
                          (191, 'road j11 1.21'), (197, 'floor j17 1.46'), (210, 'hills 1.50')):
            yy = y + (line - Y0) * SCALE
            d.line((W, yy, W + 4, yy), fill=(255, 255, 0))
            d.text((W + 6, yy - 5), '%d %s' % (line, txt), fill=(255, 255, 0))
img.save(out)
print(out, img.size)
