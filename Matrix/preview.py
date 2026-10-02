"""Render the actual firmware animation on a desktop. Needs Python/Pillow and g++.
Run: python preview.py
Outputs preview.png and preview.gif alongside this script.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parent
ADAPTER = r"""
#pragma once
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <stdexcept>
namespace lgfx {
class LGFXBase {
public:
  uint16_t pixels[240 * 240] = {};
  void fillScreen(uint16_t color) { for (auto& p : pixels) p = color; }
  void drawPixel(int x, int y, uint16_t color) {
    if (x < 0 || y < 0 || x >= 240 || y >= 240)
      throw std::runtime_error("Drawing outside the framebuffer");
    pixels[y * 240 + x] = color;
  }
  void drawCircle(int cx, int cy, int radius, uint16_t color) {
    int x = radius, y = 0, error = 1 - radius;
    while (x >= y) {
      drawPixel(cx+x,cy+y,color); drawPixel(cx+y,cy+x,color);
      drawPixel(cx-y,cy+x,color); drawPixel(cx-x,cy+y,color);
      drawPixel(cx-x,cy-y,color); drawPixel(cx-y,cy-x,color);
      drawPixel(cx+y,cy-x,color); drawPixel(cx+x,cy-y,color);
      ++y;
      if (error < 0) error += 2*y+1;
      else { --x; error += 2*(y-x)+1; }
    }
  }
  void write(FILE* out) {
    for (uint16_t p : pixels) {
      unsigned char rgb[] = {
        static_cast<unsigned char>(((p >> 11) & 31) * 255 / 31),
        static_cast<unsigned char>(((p >> 5) & 63) * 255 / 63),
        static_cast<unsigned char>((p & 31) * 255 / 31)};
      if (fwrite(rgb, 1, 3, out) != 3) throw std::runtime_error("Write failed");
    }
  }
};
}
"""
HARNESS = r"""
#include "MatrixRain.h"
int main(int argc, char** argv) {
  if (argc != 2) return 1;
  FILE* out = fopen(argv[1], "wb");
  if (!out) return 2;
  MatrixRain::Animation rain;
  lgfx::LGFXBase canvas;
  rain.begin(0x4D415452);
  for (int tick = 0; tick < 240; ++tick) {
    rain.update(1.0f / 30.0f);
    if (tick % 2 == 0) { rain.draw(canvas); canvas.write(out); }
  }
  fclose(out);
}
"""

compiler = shutil.which("g++")
if not compiler:
    raise SystemExit("Install g++ or add it to PATH.")
with tempfile.TemporaryDirectory(prefix="matrix-preview-") as folder:
    tmp = Path(folder)
    (tmp / "LovyanGFX.hpp").write_text(ADAPTER)
    (tmp / "preview.cpp").write_text(HARNESS)
    exe = tmp / "preview.exe"
    raw = tmp / "frames.rgb"
    subprocess.run([compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-I", str(tmp), "-I", str(ROOT), str(tmp / "preview.cpp"),
                    "-o", str(exe)], check=True)
    subprocess.run([str(exe), str(raw)], check=True)
    data = raw.read_bytes()

size = 240 * 240 * 3
assert len(data) == 120 * size
frames = [Image.frombytes("RGB", (240, 240), data[i:i+size])
          for i in range(0, len(data), size)]
assert frames[0].tobytes() != frames[-1].tobytes(), "Animation did not advance"
assert all(frame.getpixel((0, 0)) == (0, 0, 0) for frame in frames)
frames[30].resize((720, 720), Image.Resampling.NEAREST).save(ROOT / "preview.png")
frames[0].save(ROOT / "preview.gif", save_all=True, append_images=frames[1:],
               duration=[60, 70, 70] * 40, loop=0, optimize=False)
print("Rendered 120 frames from MatrixRain.h; saved preview.png and preview.gif.")
