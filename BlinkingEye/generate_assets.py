"""Bake original procedural eye textures into flash-resident RGB565 arrays.
Requires NumPy and Pillow. No downloaded photographs or runtime image decoder.
"""
from pathlib import Path
import argparse
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir', type=Path, default=Path(__file__).resolve().parent)
out = parser.parse_args().output_dir
out.mkdir(parents=True, exist_ok=True)
rng = np.random.default_rng(20261001)
y, x = np.mgrid[:240, :240].astype(float)
u = (x - 119.5) / 103
arch = np.maximum(0, 1 - u*u)**0.65
upper = 118 - 47*arch + 3*u
lower = 118 + 36*arch + 3*u

def noise(w, h, blur):
    im = Image.fromarray(rng.integers(0, 256, (h, w), dtype=np.uint8))
    if blur:
        im = im.filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(im).astype(float) / 255 - 0.5

def image(array):
    return Image.fromarray(np.clip(array, 0, 255).astype(np.uint8))

# Warm skin: broad illumination, orbital shading, fine mottling and pores.
grain = noise(240, 240, 0)*4 + noise(240, 240, 1)*10 + noise(240, 240, 7)*42
light = 1.02 - 0.11*(x/240) - 0.09*(y/240)
orbit = np.exp(-((y - upper + 9)/24)**2) * arch * 0.16
under_eye = np.exp(-((y-lower-12)/20)**2)*arch*0.09
skin = np.stack([183*light, 125*light, 101*light], axis=-1)
skin *= (1 - orbit - under_eye)[..., None]
skin += grain[..., None]
skin[..., 0] += noise(240, 240, 5)*13
skin += np.exp(-((y-lower-7)/7)**2)[..., None]*np.array([9, 5, 3])

# Sclera has spherical shading and sparse branching capillaries, not pure white.
side = np.clip(np.abs((x-120)/103), 0, 1)
shade = 1 - 0.32*side**2 - 0.055*((y-117)/55)**2
sclera = np.stack([228*shade, 224*shade-7*side, 214*shade-6*side], axis=-1)
sclera += noise(240, 240, 1)[..., None]*5
veins = Image.new('RGBA', (720, 720))
pen = ImageDraw.Draw(veins)
for side_index in [0, 1]:
    for branch in range(13):
        start_x = 19 if side_index == 0 else 221
        sign = 1 if side_index == 0 else -1
        start_y = rng.uniform(94, 143)
        length = rng.uniform(17, 65)
        points = []
        for k in range(14):
            t = k/13
            px = start_x + sign*length*t
            py = start_y + np.sin(t*6 + branch)*rng.uniform(1, 2.6) + t*rng.uniform(-10, 10)
            points.append((int(px*3), int(py*3)))
        pen.line(points, fill=(161, 69, 65, int(rng.uniform(28, 72))), width=1)
        if branch % 2 == 0:
            px, py = points[7]
            pen.line([(px, py), (px+sign*18, py+8), (px+sign*38, py+18)],
                     fill=(166, 71, 67, 30), width=1)
veins = veins.resize((240, 240), Image.Resampling.LANCZOS)
sclera_image = Image.alpha_composite(image(sclera).convert('RGBA'), veins).convert('RGB')
sclera = np.asarray(sclera_image).astype(float)
# Moist inner canthus at the left corner.
canthus = np.exp(-(((x-23)/10)**2 + ((y-117)/8)**2))*0.7
sclera = sclera*(1-canthus[..., None]) + np.array([192, 118, 112])*canthus[..., None]

# Iris fibers: angular structure persists along the radius; irregular crypts
# and a gold collarette break up the pattern. No simple concentric gradient.
iy, ix = np.mgrid[:96, :96].astype(float)
dx, dy = ix-47.5, iy-47.5
r = np.sqrt(dx*dx + dy*dy)/46
a = np.arctan2(dy, dx)
fibers = np.zeros_like(r)
for frequency, weight in [(53, .16), (97, .18), (173, .13), (281, .09), (397, .05)]:
    fibers += weight*np.sin(a*frequency + np.sin(r*9 + frequency)*1.2 + rng.uniform(0, 7))
wrinkle = .045*np.sin(a*29) + .027*np.sin(a*61+2)
collar = np.exp(-((r-(.47+wrinkle))/.095)**2)
crypt = np.maximum(0, np.sin(a*67 + np.sin(a*17)*2))**7
crypt *= np.exp(-((r-.56)/.18)**2)
brightness = 0.98 + fibers - .32*crypt + noise(96, 96, 0)*.10
brightness *= .92 + .15*(dy/48)
iris = np.stack([60*brightness+43*collar,
                 112*brightness+15*collar,
                 102*brightness-30*collar], axis=-1)
limbal = 1 - .78*np.clip((r-.85)/.14, 0, 1)**1.4
iris *= limbal[..., None]
inner = np.clip((r-.29)/.09, 0, 1)
iris *= (.18+.82*inner)[..., None]
iris[r > 1] = [15, 23, 23]
iris[r < .285] = [2, 4, 5]

# Eyelashes are baked as alpha relative to the upper lid. They move with it.
lash_img = Image.new('L', (720, 720))
pen = ImageDraw.Draw(lash_img)
for i in range(40):
    px = 27 + i*4.65 + rng.uniform(-1, 1)
    uu = (px-119.5)/103
    py = 118 - 47*max(0, 1-uu*uu)**.65 + 3*uu
    length = rng.uniform(7, 15)*(0.6+0.4*(1-abs(uu)))
    lean = (px-102)/100*9
    pts = []
    for step in range(12):
        t = step/11
        pts.append((int((px+lean*t+2*t*t)*3), int((py-length*t+2*t*t)*3)))
    pen.line(pts, fill=int(rng.uniform(135, 230)), width=2)
lash_pixels = np.asarray(lash_img.resize((240, 240), Image.Resampling.LANCZOS))
lashes = np.zeros((28, 240), dtype=np.uint8)
for xx in range(240):
    for j in range(28):
        yy = int(round(upper[0, xx] + j - 24))
        if 0 <= yy < 240:
            lashes[j, xx] = lash_pixels[yy, xx]

def rgb565(arr):
    a = np.clip(arr, 0, 255).astype(np.uint16)
    return ((a[..., 0] >> 3) << 11) | ((a[..., 1] >> 2) << 5) | (a[..., 2] >> 3)

with (out / 'EyeTextures.h').open('w') as f:
    f.write('// Generated by generate_assets.py; original procedural textures.\n')
    f.write('#pragma once\n#include <stdint.h>\n#ifdef ARDUINO\n#include <pgmspace.h>\n#else\n#define PROGMEM\n#endif\nnamespace EyeTextures {\n')
    for name, array, kind in [('SKIN', rgb565(skin), 'uint16_t'),
                               ('SCLERA', rgb565(sclera), 'uint16_t'),
                               ('IRIS', rgb565(iris), 'uint16_t'),
                               ('LASHES', lashes, 'uint8_t')]:
        flat = array.ravel()
        f.write(f'const {kind} {name}[{len(flat)}] PROGMEM = {{\n')
        for start in range(0, len(flat), 20):
            f.write(','.join(str(int(v)) for v in flat[start:start+20])+',\n')
        f.write('};\n')
    f.write('}\n')
print('Baked skin, sclera, iris and eyelash textures into EyeTextures.h')
