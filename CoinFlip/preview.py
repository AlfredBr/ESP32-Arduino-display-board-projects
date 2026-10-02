"""Render the actual coin animation and test the touchscreen press latch."""
from pathlib import Path
import argparse, shutil, subprocess, tempfile
from PIL import Image
ROOT=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir',type=Path,default=ROOT)
out=parser.parse_args().output_dir
out.mkdir(parents=True,exist_ok=True)
ADAPTER=r'''
#pragma once
#include <cstdint>
#include <cstdio>
#include <stdexcept>
namespace lgfx {
class LGFXBase {
public:
 uint16_t pixels[240*240]={};
 void setSwapBytes(bool) {}
 void drawPixel(int x,int y,uint16_t c) {
  if(x<0||x>=240||y<0||y>=240) throw std::runtime_error("Pixel bounds");
  pixels[y*240+x]=c;
 }
 uint16_t readPixel(int x,int y) { return pixels[y*240+x]; }
 void fillRect(int x,int y,int w,int h,uint16_t c) {
  for(int j=0;j<h;++j) for(int i=0;i<w;++i) drawPixel(x+i,y+j,c);
 }
 void fillScreen(uint16_t c) { for(auto& p:pixels) p=c; }
 void pushImage(int x,int y,int w,int h,const uint16_t* src) {
  for(int j=0;j<h;++j) for(int i=0;i<w;++i) drawPixel(x+i,y+j,src[j*w+i]);
 }
 void write(FILE* out) {
  for(uint16_t p:pixels) {
   unsigned char b[]={static_cast<unsigned char>((p>>11)*255/31),
    static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};
   if(fwrite(b,1,3,out)!=3) throw std::runtime_error("Write failure");
  }
 }
};
}
'''
ARDUINO=r'''
#pragma once
#include <cstdint>
constexpr int INPUT_PULLUP=0,OUTPUT=1,LOW=0,HIGH=1;
inline void pinMode(int,int) {}
inline void digitalWrite(int,int) {}
inline void delay(int) {}
'''
WIRE=r'''
#pragma once
#include <cstdint>
struct MockWire {
 bool fail=false; uint8_t fingers=0; int index=0;
 void begin(int,int,int) {} void setTimeOut(int) {}
 void beginTransmission(uint8_t) {} void write(uint8_t) {}
 int endTransmission(bool=true) { return fail?2:0; }
 int requestFrom(uint8_t,uint8_t count) { index=0; return fail?0:count; }
 int available() { return index<5; }
 int read() { const uint8_t sample[]={fingers,0,120,0,120}; return sample[index++]; }
};
inline MockWire Wire;
'''
HARNESS=r'''
#include "CoinAnimation.h"
#include "TouchInput.h"
int main(int argc,char** argv) {
 if(argc!=2) return 1;
 TouchInput touch; touch.begin();
 if(!touch.ready()) return 2;
 Wire.fingers=1;
 if(!touch.poll(100).pressed || touch.poll(108).pressed) return 3;
 for(unsigned t=116;t<200;t+=8) if(touch.poll(t).pressed) return 4;
 Wire.fingers=0; touch.poll(208); touch.poll(232); touch.poll(256);
 Wire.fingers=1; if(!touch.poll(264).pressed) return 5;
 Wire.fail=true; if(touch.poll(272).pressed || touch.ready()) return 6;
 Wire.fail=false; touch.poll(1400); if(!touch.ready()) return 7;
 FILE* out=fopen(argv[1],"wb"); if(!out) return 8;
 GoldCoin::Animation coin; lgfx::LGFXBase canvas; coin.begin();
 for(int tick=0;tick<240;++tick) {
  if(tick==15) coin.flip(0x1280); // Heads.
  if(tick==135) coin.flip(0x1351); // Tails.
  coin.update(1.0f/30);
  if(tick%2==0) { coin.draw(canvas); canvas.write(out); }
  if(tick==100 && (!coin.settled() || coin.outcome()!=0)) return 9;
 }
 fclose(out);
 if(!coin.settled() || coin.outcome()!=1) return 10;
 coin.flip(0); coin.update(.1f); coin.flip(1);
 for(int i=0;i<90;++i) coin.update(1.0f/30);
 if(!coin.settled() || coin.outcome()!=1) return 11;
}
'''

def main():
 compiler=shutil.which('g++')
 if not compiler: raise SystemExit('g++ must be on PATH')
 with tempfile.TemporaryDirectory(prefix='coin-preview-') as folder:
  tmp=Path(folder)
  for name,body in [('LovyanGFX.hpp',ADAPTER),('Arduino.h',ARDUINO),('Wire.h',WIRE),('preview.cpp',HARNESS)]:
   (tmp/name).write_text(body)
  exe,raw=tmp/'preview.exe',tmp/'frames.rgb'
  subprocess.run([compiler,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I',str(tmp),'-I',str(ROOT),str(tmp/'preview.cpp'),'-o',str(exe)],check=True)
  subprocess.run([str(exe),str(raw)],check=True)
  data=raw.read_bytes()
 size=240*240*3
 assert len(data)==120*size
 frames=[Image.frombytes('RGB',(240,240),data[i:i+size]) for i in range(0,len(data),size)]
 frames[50].resize((720,720),Image.Resampling.NEAREST).save(out/'preview.png')
 contact=Image.new('RGB',(960,240))
 for i,index in enumerate([0,18,50,115]): contact.paste(frames[index],(240*i,0))
 contact.save(out/'preview-contact.png')
 palette=contact.quantize(colors=256)
 indexed=[f.quantize(palette=palette,dither=Image.Dither.NONE) for f in frames]
 indexed[0].save(out/'preview.gif',save_all=True,append_images=indexed[1:],duration=[60,70,70]*40,loop=0,optimize=False)
 print('Rendered heads/tails flips; held touch, release, I2C failure/recovery and interrupted flip tests passed.')

if __name__=='__main__': main()
