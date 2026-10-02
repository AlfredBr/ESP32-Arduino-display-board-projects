"""Render the actual hyperspace firmware using a desktop RGB565 adapter."""
from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile
from PIL import Image

ROOT=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir',type=Path,default=ROOT)
out=parser.parse_args().output_dir
out.mkdir(parents=True,exist_ok=True)
adapter=r'''
#pragma once
#include <cstdint>
#include <cstdio>
#include <stdexcept>
namespace lgfx {
class LGFXBase {
public:
  uint16_t pixels[240*240]={};
  void setSwapBytes(bool) {}
  void drawPixel(int x,int y,uint16_t color) {
    if(x<0 || x>=240 || y<0 || y>=240) throw std::runtime_error("Pixel bounds");
    pixels[y*240+x]=color;
  }
  uint16_t readPixel(int x,int y) {
    if(x<0 || x>=240 || y<0 || y>=240) throw std::runtime_error("Read bounds");
    return pixels[y*240+x];
  }
  void pushImage(int x,int y,int w,int h,const uint16_t* source) {
    if(x<0 || y<0 || x+w>240 || y+h>240) throw std::runtime_error("Scanline bounds");
    for(int j=0;j<h;++j) for(int i=0;i<w;++i) pixels[(y+j)*240+x+i]=source[j*w+i];
  }
  void write(FILE* out) {
    for(uint16_t p:pixels) {
      const unsigned char rgb[]={static_cast<unsigned char>((p>>11)*255/31),
        static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};
      if(fwrite(rgb,1,3,out)!=3) throw std::runtime_error("Write failed");
    }
  }
};
}
'''
harness=r'''
#include "HyperspaceAnimation.h"
int main(int argc,char** argv) {
  if(argc!=2) return 1;
  FILE* out=fopen(argv[1],"wb");
  if(!out) return 2;
  Hyperspace::Animation flight;
  lgfx::LGFXBase canvas;
  flight.begin(0x48595045);
  for(int tick=0;tick<360;++tick) {
    flight.update(1.0f/30);
    if(tick%2==0) {
      flight.draw(canvas);
      if(canvas.pixels[0]!=0 || canvas.pixels[239]!=0 || canvas.pixels[239*240]!=0) return 3;
      canvas.write(out);
    }
  }
  fclose(out);
  // Exercise repeated star recycling and bounded wave phases for ten minutes.
  for(int tick=0;tick<18000;++tick) {
    flight.update(1.0f/30);
    if(tick%90==0) flight.draw(canvas);
  }
  flight.begin(0x48595045);
  Hyperspace::Animation replay;
  lgfx::LGFXBase other;
  replay.begin(0x48595045);
  for(int tick=0;tick<240;++tick) { flight.update(1.0f/30); replay.update(1.0f/30); }
  flight.draw(canvas); replay.draw(other);
  for(int i=0;i<240*240;++i) if(canvas.pixels[i]!=other.pixels[i]) return 4;
}
'''
compiler=shutil.which('g++')
if not compiler:
    raise SystemExit('g++ must be on PATH')
with tempfile.TemporaryDirectory(prefix='hyperspace-preview-') as folder:
    tmp=Path(folder)
    (tmp/'LovyanGFX.hpp').write_text(adapter)
    (tmp/'preview.cpp').write_text(harness)
    exe,raw=tmp/'preview.exe',tmp/'frames.rgb'
    subprocess.run([compiler,'-std=c++17','-O2','-Wall','-Wextra','-Werror',
                    '-I',str(tmp),'-I',str(ROOT),str(tmp/'preview.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(raw)],check=True)
    data=raw.read_bytes()
size=240*240*3
assert len(data)==180*size
frames=[Image.frombytes('RGB',(240,240),data[i:i+size]) for i in range(0,len(data),size)]
assert frames[0].tobytes()!=frames[1].tobytes(), 'Starfield did not move'
frames[30].resize((720,720),Image.Resampling.NEAREST).save(out/'preview.png')
contact=Image.new('RGB',(960,240))
for i,index in enumerate([10,50,95,150]):
    contact.paste(frames[index],(240*i,0))
contact.save(out/'preview-contact.png')
swatches=Image.new('RGB',(240*6,240))
for i,index in enumerate([0,30,60,90,120,150]):
    swatches.paste(frames[index],(240*i,0))
palette=swatches.quantize(colors=256)
indexed=[frame.quantize(palette=palette,dither=Image.Dither.NONE) for frame in frames]
indexed[0].save(out/'preview.gif',save_all=True,append_images=indexed[1:],
                duration=[60,70,70]*60,loop=0,optimize=False)
print('Rendered 180 frames; motion, bounds, star recycling and seeded replay checks passed.')
