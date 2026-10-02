"""Pack the generated transparent sprite sheet into ESP32 flash arrays.
Requires Pillow and NumPy. Keeps source alpha; does not redraw the artwork.
"""
from pathlib import Path
import argparse
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir', type=Path, default=ROOT)
out = parser.parse_args().output_dir
out.mkdir(parents=True, exist_ok=True)
source = Image.open(ROOT/'assets'/'hamster-sheet.png').convert('RGBA')
WIDTH, HEIGHT = 128, 88
cells = []
boxes = []
for index in range(16):
    col, row = index%4, index//4
    box = (round(col*source.width/4), round(row*source.height/4),
           round((col+1)*source.width/4), round((row+1)*source.height/4))
    cell = source.crop(box)
    # Locate the animal rather than almost-transparent stray fur pixels.
    bounds = cell.getchannel('A').point(lambda a: 255 if a >= 100 else 0).getbbox()
    if not bounds:
        raise RuntimeError(f'Empty sprite cell {index}')
    cells.append(cell)
    boxes.append(bounds)
# Shared scale preserves body size; baseline registration reduces sheet jitter.
scale = min((WIDTH-4)/max(b[2]-b[0] for b in boxes),
            (HEIGHT-4)/max(b[3]-b[1] for b in boxes))
frames = []
for cell, bounds in zip(cells, boxes):
    resized = cell.resize((round(cell.width*scale), round(cell.height*scale)), Image.Resampling.LANCZOS)
    x = round(WIDTH/2-(bounds[0]+bounds[2])*scale/2)
    y = round(HEIGHT-3-bounds[3]*scale)
    frame = Image.new('RGBA', (WIDTH, HEIGHT))
    frame.alpha_composite(resized, (x, y))
    frames.append(np.asarray(frame))
frames = np.asarray(frames, dtype=np.uint16)
rgb = ((frames[:,:,:,0]>>3)<<11) | ((frames[:,:,:,1]>>2)<<5) | (frames[:,:,:,2]>>3)
alpha = frames[:,:,:,3].astype(np.uint8)

# A side-on wheel: the 238-pixel outer diameter hugs the circular LCD edge.
# Baked lighting stays stationary while spokes and tread markers rotate.
y, x = np.mgrid[:240, :240].astype(float)
dx, dy = x-119.5, y-119.5
r = np.sqrt(dx*dx+dy*dy)
light = np.clip(.62-.22*dx/120-.33*dy/120, .1, 1)
back = np.zeros((240, 240, 3), dtype=float)
inside = r < 110
vignette = .45+.55*np.clip(1-r/130, 0, 1)
back[inside] = (np.stack([15*vignette+5*light, 23*vignette+6*light,
                         28*vignette+8*light], axis=-1))[inside]
for low, high, color, brightness in [
    (109, 111, [29, 40, 45], .7),
    (111, 114, [92, 111, 113], .65),
    (114, 117, [168, 177, 170], 1.0),
    (117, 119.4, [54, 69, 73], .85),
]:
    mask = (r >= low) & (r < high)
    back[mask] = np.array(color)*(brightness*(.38+.62*light[mask,None]))
back = np.clip(back, 0, 255).astype(np.uint16)
background = ((back[:,:,0]>>3)<<11) | ((back[:,:,1]>>2)<<5) | (back[:,:,2]>>3)

with (out/'HamsterAssets.h').open('w') as f:
    f.write('// Packed from assets/hamster-sheet.png by prepare_assets.py.\n')
    f.write('#pragma once\n#include <stdint.h>\n#ifdef ARDUINO\n#include <pgmspace.h>\n#else\n#define PROGMEM\n#endif\nnamespace HamsterAssets {\n')
    f.write(f'constexpr int WIDTH={WIDTH}, HEIGHT={HEIGHT}, FRAMES=16;\n')
    for name, arr, kind in [('SPRITES', rgb, 'uint16_t'), ('ALPHA', alpha, 'uint8_t'),
                            ('BACKGROUND', background, 'uint16_t')]:
        flat = arr.ravel()
        f.write(f'const {kind} {name}[{len(flat)}] PROGMEM = {{\n')
        for start in range(0, len(flat), 24):
            f.write(','.join(str(int(v)) for v in flat[start:start+24])+',\n')
        f.write('};\n')
    f.write('}\n')
atlas = Image.new('RGBA', (WIDTH*4, HEIGHT*4))
for index, frame in enumerate(frames):
    atlas.paste(Image.fromarray(frame.astype(np.uint8)), ((index%4)*WIDTH, (index//4)*HEIGHT))
atlas.save(out/'packed-sprites.png')
print(f'Packed 16 {WIDTH}x{HEIGHT} RGBA sprites, alpha preserved, and a 240x240 wheel backdrop.')
