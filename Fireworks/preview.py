"""Render the actual firmware animation; requires Pillow and g++ on PATH."""
from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output-dir", type=Path, default=ROOT)
OUTPUT = parser.parse_args().output_dir
OUTPUT.mkdir(parents=True, exist_ok=True)
ADAPTER = r"""
#pragma once
#include <cstdint>
#include <cstdio>
#include <stdexcept>
namespace lgfx {
class LGFXBase {
public:
  uint16_t pixels[240 * 240] = {};
  void fillScreen(uint16_t color) { for (auto& p : pixels) p = color; }
  void drawPixel(int x, int y, uint16_t color) {
    if (x < 0 || y < 0 || x >= 240 || y >= 240)
      throw std::runtime_error("Pixel outside framebuffer");
    const float dx = x - 119.5f, dy = y - 119.5f;
    if (dx*dx + dy*dy > 118*118)
      throw std::runtime_error("Pixel outside circular aperture");
    pixels[y * 240 + x] = color;
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
#include "FireworksAnimation.h"
int main(int argc, char** argv) {
  if (argc != 2) return 1;
  FILE* out = fopen(argv[1], "wb");
  if (!out) return 2;
  Fireworks::Animation show;
  lgfx::LGFXBase canvas;
  show.begin(0xF17E2026);
  for (int tick = 0; tick < 360; ++tick) {
    show.update(1.0f / 30);
    if (tick % 2 == 0) { show.draw(canvas); canvas.write(out); }
  }
  fclose(out);
  // Exercise repeated launches and particle reuse for ten simulated minutes.
  for (int tick = 0; tick < 18000; ++tick) {
    show.update(1.0f / 30);
    if (tick % 30 == 0) show.draw(canvas);
  }
  // Restart should return to the same seeded first frame.
  show.begin(0xF17E2026);
  show.update(1.0f / 30);
  show.draw(canvas);
  Fireworks::Animation fresh;
  lgfx::LGFXBase other;
  fresh.begin(0xF17E2026);
  fresh.update(1.0f / 30);
  fresh.draw(other);
  for (int i = 0; i < 240*240; ++i)
    if (canvas.pixels[i] != other.pixels[i]) return 3;
}
"""

compiler = shutil.which("g++")
if not compiler:
    raise SystemExit("Install g++ or add it to PATH.")
with tempfile.TemporaryDirectory(prefix="fireworks-preview-") as folder:
    tmp = Path(folder)
    (tmp / "LovyanGFX.hpp").write_text(ADAPTER)
    (tmp / "preview.cpp").write_text(HARNESS)
    exe, raw = tmp / "preview.exe", tmp / "frames.rgb"
    subprocess.run([compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-I", str(tmp), "-I", str(ROOT), str(tmp / "preview.cpp"),
                    "-o", str(exe)], check=True)
    subprocess.run([str(exe), str(raw)], check=True)
    data = raw.read_bytes()

size = 240 * 240 * 3
assert len(data) == 180 * size
frames = [Image.frombytes("RGB", (240, 240), data[i:i+size])
          for i in range(0, len(data), size)]
assert frames[0].tobytes() != frames[-1].tobytes(), "Animation did not advance"
frames[58].resize((720, 720), Image.Resampling.NEAREST).save(OUTPUT / "preview.png")
contact = Image.new("RGB", (960, 240))
for index, frame in enumerate([26, 51, 80, 106]):
    contact.paste(frames[frame], (index * 240, 0))
contact.resize((1440, 360), Image.Resampling.NEAREST).save(OUTPUT / "preview-contact.png")
# A shared palette keeps colors stable throughout the animation.
swatches = Image.new("RGB", (240 * 12, 240))
for i in range(12):
    swatches.paste(frames[15 * i], (240 * i, 0))
palette = swatches.quantize(colors=256)
indexed = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
indexed[0].save(OUTPUT / "preview.gif", save_all=True, append_images=indexed[1:],
                duration=[60, 70, 70] * 60, loop=0, optimize=False)
print("Rendered 180 frames; ten-minute particle reuse and circular clipping checks passed.")
