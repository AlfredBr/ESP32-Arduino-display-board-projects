"""Render the actual Hamster firmware with a desktop RGB565 adapter."""
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
#include <cmath>
#include <cstdlib>
#include <stdexcept>
namespace lgfx {
class LGFXBase {
public:
  uint16_t pixels[240*240] = {};
  void setSwapBytes(bool) {}
  void pixel(int x, int y, uint16_t color) {
    if (x<0 || x>=240 || y<0 || y>=240) throw std::runtime_error("Pixel bounds");
    pixels[y*240+x] = color;
  }
  uint16_t readPixel(int x, int y) {
    if (x<0 || x>=240 || y<0 || y>=240) throw std::runtime_error("Read bounds");
    return pixels[y*240+x];
  }
  void pushImage(int x, int y, int w, int h, const uint16_t* source) {
    for (int j=0; j<h; ++j) for (int i=0; i<w; ++i) pixel(x+i,y+j,source[j*w+i]);
  }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    const int dx=abs(x1-x0), sx=x0<x1?1:-1;
    const int dy=-abs(y1-y0), sy=y0<y1?1:-1;
    int error=dx+dy;
    for (;;) {
      pixel(x0,y0,color);
      if (x0==x1 && y0==y1) break;
      const int twice=2*error;
      if (twice>=dy) { error+=dy; x0+=sx; }
      if (twice<=dx) { error+=dx; y0+=sy; }
    }
  }
  void fillEllipse(int cx, int cy, int rx, int ry, uint16_t color) {
    for (int y=-ry; y<=ry; ++y) for (int x=-rx; x<=rx; ++x)
      if (x*x*ry*ry+y*y*rx*rx<=rx*rx*ry*ry) pixel(cx+x,cy+y,color);
  }
  void fillCircle(int x, int y, int r, uint16_t color) { fillEllipse(x,y,r,r,color); }
  void write(FILE* out) {
    for (uint16_t p: pixels) {
      unsigned char rgb[] = {static_cast<unsigned char>((p>>11)*255/31),
        static_cast<unsigned char>(((p>>5)&63)*255/63),
        static_cast<unsigned char>((p&31)*255/31)};
      if (fwrite(rgb,1,3,out)!=3) throw std::runtime_error("Write failed");
    }
  }
};
}
'''
harness = r'''
#include "HamsterAnimation.h"
int main(int argc, char** argv) {
  if (argc!=2) return 1;
  FILE* out=fopen(argv[1],"wb");
  if (!out) return 2;
  Hamster::Animation hamster;
  lgfx::LGFXBase canvas;
  hamster.begin(0x48414D);
  unsigned poses=0;
  for (int tick=0; tick<300; ++tick) {
    hamster.update(1.0f/30);
    const int pose=hamster.pose();
    if (pose<0 || pose>=16) return 3;
    poses |= 1u<<pose;
    if (tick%2==0) { hamster.draw(canvas); canvas.write(out); }
  }
  fclose(out);
  if ((poses&255)!=255 || (poses&0xF000)==0 || (poses&0xF00)==0) return 4;
  for (int tick=0; tick<18000; ++tick) {
    hamster.update(1.0f/30);
    if (hamster.pose()<0 || hamster.pose()>=16) return 5;
    if (tick%300==0) hamster.draw(canvas);
  }
}
'''
compiler = shutil.which('g++')
if not compiler:
    raise SystemExit('g++ must be on PATH')
with tempfile.TemporaryDirectory(prefix='hamster-preview-') as folder:
    tmp=Path(folder)
    (tmp/'LovyanGFX.hpp').write_text(adapter)
    (tmp/'preview.cpp').write_text(harness)
    exe, raw=tmp/'preview.exe',tmp/'frames.rgb'
    subprocess.run([compiler,'-std=c++17','-O2','-Wall','-Wextra','-Werror',
                    '-I',str(tmp),'-I',str(ROOT),str(tmp/'preview.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(raw)],check=True)
    data=raw.read_bytes()
size=240*240*3
assert len(data)==150*size
frames=[Image.frombytes('RGB',(240,240),data[i:i+size]) for i in range(0,len(data),size)]
frames[14].resize((720,720),Image.Resampling.NEAREST).save(out/'preview.png')
contact=Image.new('RGB',(960,240))
for i,index in enumerate([14,72,83,98]):
    contact.paste(frames[index],(240*i,0))
contact.save(out/'preview-contact.png')
# A shared palette avoids changes in fur colors between frames.
swatches=Image.new('RGB',(240*6,240))
for i,index in enumerate([0,14,45,72,83,98]):
    swatches.paste(frames[index],(240*i,0))
palette=swatches.quantize(colors=256)
indexed=[frame.quantize(palette=palette,dither=Image.Dither.NONE) for frame in frames]
indexed[0].save(out/'preview.gif',save_all=True,append_images=indexed[1:],
                duration=[60,70,70]*50,loop=0,optimize=False)
print('Rendered 150 frames; run, glance, wink, bounds and ten-minute simulation checks passed.')
