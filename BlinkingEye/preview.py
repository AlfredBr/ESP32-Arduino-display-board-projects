"""Render the actual eye firmware on the desktop; requires Pillow and g++."""
from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir', type=Path, default=ROOT)
out = parser.parse_args().output_dir
out.mkdir(parents=True, exist_ok=True)
adapter = r'''
#pragma once
#include <cstdint>
#include <cstdio>
#include <stdexcept>
namespace lgfx {
class LGFXBase {
public:
  uint16_t pixels[240*240] = {};
  void setSwapBytes(bool) {}
  void pushImage(int x, int y, int w, int h, const uint16_t* source) {
    if (x<0 || y<0 || x+w>240 || y+h>240) throw std::runtime_error("Scanline bounds");
    for (int j=0; j<h; ++j) for (int i=0; i<w; ++i)
      pixels[(y+j)*240+x+i] = source[j*w+i];
  }
  void write(FILE* out) {
    for (uint16_t p: pixels) {
      const unsigned char rgb[] = {
        static_cast<unsigned char>((p>>11)*255/31),
        static_cast<unsigned char>(((p>>5)&63)*255/63),
        static_cast<unsigned char>((p&31)*255/31)};
      if (fwrite(rgb, 1, 3, out) != 3) throw std::runtime_error("Write failed");
    }
  }
};
}
'''
harness = r'''
#include "EyeAnimation.h"
int main(int argc, char** argv) {
  if (argc != 2) return 1;
  FILE* out = fopen(argv[1], "wb");
  if (!out) return 2;
  BlinkingEye::Animation eye;
  lgfx::LGFXBase canvas;
  eye.begin(0xE1E2026);
  bool closed = false, reopened = false;
  for (int frame=0; frame<240; ++frame) {
    eye.update(1.0f/30);
    eye.draw(canvas);
    canvas.write(out);
    if (eye.blinkAmount() == 1) closed = true;
    if (closed && eye.blinkAmount() == 0) reopened = true;
    if (canvas.pixels[0] != 0 || canvas.pixels[239] != 0) return 3;
    if (frame >= 53 && frame <= 65)
      printf("frame %d closure %.3f\n", frame, eye.blinkAmount());
  }
  fclose(out);
  if (!closed || !reopened) return 4;
  // Exercise repeated blinks and saccades without accumulating large times.
  for (int frame=0; frame<18000; ++frame) {
    eye.update(1.0f/30);
    if (frame%300 == 0) eye.draw(canvas);
  }
}
'''
compiler = shutil.which('g++')
if not compiler:
    raise SystemExit('g++ must be on PATH')
with tempfile.TemporaryDirectory(prefix='eye-preview-') as folder:
    tmp = Path(folder)
    (tmp/'LovyanGFX.hpp').write_text(adapter)
    (tmp/'preview.cpp').write_text(harness)
    exe, raw = tmp/'preview.exe', tmp/'frames.rgb'
    subprocess.run([compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', str(tmp), '-I', str(ROOT), str(tmp/'preview.cpp'),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe), str(raw)], check=True)
    data = raw.read_bytes()
size = 240*240*3
assert len(data) == 240*size
frames = [Image.frombytes('RGB', (240, 240), data[i:i+size])
          for i in range(0, len(data), size)]
frames[15].resize((720, 720), Image.Resampling.NEAREST).save(out/'preview.png')
contact = Image.new('RGB', (960, 240))
for i, index in enumerate([15, 56, 58, 62]):
    contact.paste(frames[index], (240*i, 0))
contact.save(out/'preview-contact.png')
swatches = Image.new('RGB', (240*8, 240))
for i in range(8):
    swatches.paste(frames[i*30], (240*i, 0))
palette = swatches.quantize(colors=256)
indexed = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
indexed[0].save(out/'preview.gif', save_all=True, append_images=indexed[1:],
                duration=[30, 30, 40]*80, loop=0, optimize=False)
print('Rendered 240 frames. Full blink, reopening, bounds and ten-minute simulation passed.')
