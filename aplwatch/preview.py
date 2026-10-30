"""Render and check the actual watch-face code on the desktop with g++ and Pillow.

Writes preview.png (the four faces in tap order) and preview-contact.png (more
times and the unsynchronized state). Fails if a check does not hold.
"""
from pathlib import Path
import argparse, shutil, subprocess, tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
# Windows Controlled Folder Access can block Python from creating files under
# Documents; write elsewhere and copy the PNGs in if that happens.
parser.add_argument('--output-dir', type=Path, default=ROOT)
out = parser.parse_args().output_dir
out.mkdir(parents=True, exist_ok=True)
W, H = 368, 448

HARNESS = r'''
#include "AplFaces.h"
#include <chrono>
#include <cstdio>
#include <vector>
using namespace AplFaces;
static std::vector<uint16_t> screen(W * H);
static FILE *out;
static Window full() { return {screen.data(), 0, 0, W, H}; }
static float angle(float units, float perTurn) { return units / perTurn * 2 * PI_F; }
static void lights(int h, int m, int s, const LightPalette &colors = EASTERN_LIGHTS) {
  float minutes = m + s / 60.0f, hours = h % 12 + minutes / 60;
  renderLights(full(), angle(hours, 12), angle(minutes, 60), true, colors);
  drawSeconds(full(), angle(s, 60), colors.seconds);
  fwrite(screen.data(), 2, screen.size(), out);
}
static void digital(int h, int m, bool use24h, bool ready = true, Color ink = PINK) {
  renderDigital(full(), h, m, ready, use24h, ink);
  fwrite(screen.data(), 2, screen.size(), out);
}
int main(int, char **argv) {
  out = fopen(argv[1], "wb");
  if (!out) return 2;
  // The tap cycle at 10:09:32 EDT, which is 14:09:32 UTC.
  lights(10, 9, 32); digital(10, 9, true);
  lights(14, 9, 32, FRENCH_LIGHTS); digital(14, 9, true, true, WHITE);
  // Contact sheet: digital variants, then the light faces at assorted times.
  digital(9, 41, false); digital(0, 0, true); digital(0, 0, true, false); digital(0, 0, true, false, WHITE);
  lights(12, 0, 0); lights(3, 40, 15); lights(6, 32, 45); lights(8, 20, 10);
  lights(12, 0, 0, FRENCH_LIGHTS); lights(3, 40, 15, FRENCH_LIGHTS); lights(6, 32, 45, FRENCH_LIGHTS); lights(8, 20, 10, FRENCH_LIGHTS);
  renderLights(full(), 0, 0, false, EASTERN_LIGHTS); fwrite(screen.data(), 2, screen.size(), out);
  renderLights(full(), 0, 0, false, FRENCH_LIGHTS); fwrite(screen.data(), 2, screen.size(), out);
  fclose(out);

  // Every digital time stays on screen in both modes (drawLine skips
  // off-screen pixels, so check the layout itself).
  int top = (H - 2 * DigitFont::DIGIT_HEIGHT - LINE_GAP) / 2;
  if (top < 8) return 3;
  for (int h = 0; h < 24; ++h)
    for (int m = 0; m < 60; ++m) {
      char a[4], b[4];
      snprintf(a, sizeof a, "%02d", h); snprintf(b, sizeof b, ":%02d", m);
      if (W - RIGHT_MARGIN - textWidth(a) < 8 || W - RIGHT_MARGIN - textWidth(b) < 8) return 4;
    }
  // The seconds hand never draws outside its reported bounds.
  std::vector<uint16_t> blank(W * H, 0);
  for (int t = 0; t < 600; ++t) {
    float a = t * 2 * PI_F / 600;
    std::fill(screen.begin(), screen.end(), 0);
    drawSeconds(full(), a, EASTERN_LIGHTS.seconds);
    Rect r = secondsBounds(a);
    if (r.x % 2 || r.y % 2 || r.w % 2 || r.h % 2) return 5;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x)
        if (screen[y * W + x] && (x < r.x || x >= r.x + r.w || y < r.y || y >= r.y + r.h)) return 6;
  }
  // Drawing into a sub-window matches drawing into the full screen.
  renderLights(full(), 1.2f, 4.0f, true, FRENCH_LIGHTS);
  std::vector<uint16_t> background = screen;
  float a = 2.5f;
  drawSeconds(full(), a, FRENCH_LIGHTS.seconds);
  Rect r = secondsBounds(a);
  std::vector<uint16_t> part(r.w * r.h);
  Window win{part.data(), r.x, r.y, r.w, r.h};
  copyFrom(win, background.data());
  drawSeconds(win, a, FRENCH_LIGHTS.seconds);
  for (int y = 0; y < r.h; ++y)
    for (int x = 0; x < r.w; ++x)
      if (part[y * r.w + x] != screen[(r.y + y) * W + r.x + x]) return 7;
  // fastExp tracks expf closely enough for 8-bit color.
  for (float x = -90; x <= 0; x += 0.001f)
    if (fabsf(fastExp(x) - expf(x)) > 2e-4f) return 8;
  // Desktop timing of a full light-face render, for scale.
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 10; ++i) renderLights(full(), i * 0.3f, i * 0.7f, true, EASTERN_LIGHTS);
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 10;
  printf("Light face renders in %.1f ms on this PC.\n", ms);
  return 0;
}
'''

compiler = shutil.which('g++')
if not compiler:
    raise SystemExit('g++ must be on PATH')
with tempfile.TemporaryDirectory(prefix='aplwatch-preview-') as folder:
    tmp = Path(folder)
    (tmp / 'preview.cpp').write_text(HARNESS)
    exe, frames = tmp / 'preview.exe', tmp / 'frames.raw'
    subprocess.run([compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT),
                    str(tmp / 'preview.cpp'), '-o', str(exe)], check=True)
    result = subprocess.run([str(exe), str(frames)])
    if result.returncode:
        raise SystemExit(f'Check failed (code {result.returncode}).')
    raw = frames.read_bytes()


def frame(index):
    data = raw[index * W * H * 2:(index + 1) * W * H * 2]
    pixels = bytearray()
    for i in range(0, len(data), 2):
        p = data[i] | data[i + 1] << 8
        pixels += bytes(((p >> 11) * 255 // 31, (p >> 5 & 63) * 255 // 63, (p & 31) * 255 // 31))
    return Image.frombytes('RGB', (W, H), bytes(pixels))


count = len(raw) // (W * H * 2)
images = [frame(i) for i in range(count)]
cycle = Image.new('RGB', (4 * W + 3 * 24, H), (40, 40, 40))
for i in range(4):
    cycle.paste(images[i], (i * (W + 24), 0))
cycle.save(out / 'preview.png')
rest = images[4:]
cols = 4
sheet = Image.new('RGB', (cols * (W + 12), ((len(rest) + cols - 1) // cols) * (H + 12)), (40, 40, 40))
for i, image in enumerate(rest):
    sheet.paste(image, ((i % cols) * (W + 12), (i // cols) * (H + 12)))
sheet.save(out / 'preview-contact.png')
print(f'Checked digit layout for all 1440 times, seconds-hand bounds at 600 angles and window drawing; wrote {count} frames.')
