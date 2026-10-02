"""Render a layout preview using the actual LovyanGFX glyph bitmaps.
Requires Python and Pillow; used on the PC only, not by the Arduino sketch.
The LCD's antialiasing may differ slightly from this preview.
"""
from pathlib import Path
import math
import re
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
FONT_ROOT = Path.home() / 'OneDrive/Documents/Arduino/Libraries/LovyanGFX/src/lgfx/Fonts'
SCALE = 4


def rgb565(value):
    return ((value >> 11) * 255 // 31, ((value >> 5) & 63) * 255 // 63, (value & 31) * 255 // 31)


class Font:
    def __init__(self, relative_path):
        text = (FONT_ROOT / relative_path).read_text(encoding='utf-8')
        bitmap_block = re.search(r'Bitmaps\[\].*?=\s*\{(.*?)\};', text, re.S).group(1)
        self.bits = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-fA-F]+)', bitmap_block))
        glyph_block = re.search(r'Glyphs\[\].*?=\s*\{(.*?)\};', text, re.S).group(1)
        self.glyphs = [tuple(map(int, re.findall(r'-?\d+', row))) for row in re.findall(r'\{([^}]+)\}', glyph_block)]

    def width(self, text):
        return sum(self.glyphs[ord(char) - 32][3] for char in text)

    def draw(self, image, text, x, baseline, color, align='center'):
        width = self.width(text)
        if align == 'center':
            x -= width // 2
        elif align == 'right':
            x -= width
        for char in text:
            offset, w, h, advance, dx, dy = self.glyphs[ord(char) - 32]
            mask = Image.new('L', (w, h))
            values = []
            for bit in range(w * h):
                values.append(255 if self.bits[offset + bit // 8] & (0x80 >> (bit % 8)) else 0)
            mask.putdata(values)
            image.paste(color, (x + dx, baseline + dy), mask)
            x += advance


def point(second, radius):
    angle = second * math.tau / 60
    return (round(120 + math.sin(angle) * radius), round(120 - math.cos(angle) * radius))


def render(hour=21, minute=44, second=6):
    drawing = (ROOT / 'WatchFaceDrawing.h').read_text(encoding='utf-8')
    colors = {name: rgb565(int(value, 16)) for name, value in re.findall(r'constexpr uint16_t (\w+) = (0x[0-9A-Fa-f]+)', drawing)}
    large = Image.new('RGB', (240 * SCALE, 240 * SCALE), 'black')
    pen = ImageDraw.Draw(large)
    def circle(x, y, r, color):
        pen.ellipse(tuple(round(v * SCALE) for v in (x-r, y-r, x+r, y+r)), fill=color)
    for tick in range(60):
        major = tick % 5 == 0
        a = point(tick, 100 if major else 103)
        b = point(tick, 112)
        color = colors['PALE_TICK' if tick >= 52 or tick == 0 else 'PURPLE']
        radius = 0.85 if major else 0.55
        pen.line(tuple(v * SCALE for p in (a, b) for v in p), fill=color, width=round(radius*2*SCALE))
        circle(*a, radius, color)
        circle(*b, radius, color)
    circle(*point(second, 108), 5, colors['WHITE'])
    circle(136, 64, 3, colors['WHITE'])
    circle(142, 64, 3, colors['WHITE'])
    pen.polygon([(x*SCALE, y*SCALE) for x, y in [(132,65),(146,65),(139,73)]], fill=colors['WHITE'])
    image = large.resize((240, 240), Image.Resampling.LANCZOS)
    small = Font('GFXFF/FreeSans12pt7b.h')
    big = Font('Custom/DejaVu72.h')
    small.draw(image, '57', 126, 71, colors['HEART_RATE'], align='right')
    for glyph, x in zip(f'{hour:02}:{minute:02}', [48,90,119,148,190]):
        big.draw(image, glyph, x, 138, colors['WHITE'])
    small.draw(image, 'Sep 29', 120, 165, colors['WHITE'])
    return image


if __name__ == '__main__':
    result = render()
    result.save(ROOT / 'preview-240.png')
    # Nearest scaling makes the actual 240 x 240 pixel resolution visible.
    result.resize((720,720), Image.Resampling.NEAREST).save(ROOT / 'preview.png')
    print('Saved preview-240.png and preview.png')
