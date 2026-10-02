"""Render and check the actual watch drawing code using Pillow and g++."""
from pathlib import Path
import argparse, shutil, subprocess, tempfile
from PIL import Image
ROOT=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir', type=Path, default=ROOT)
out=parser.parse_args().output_dir
out.mkdir(parents=True, exist_ok=True)
ADAPTER=r'''
#pragma once
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <algorithm>
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
 void drawLine(int x0,int y0,int x1,int y1,uint16_t c) {
  int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,error=dx+dy;
  for(;;) {
   drawPixel(x0,y0,c);if(x0==x1&&y0==y1) break;
   int twice=2*error;
   if(twice>=dy) { error+=dy;x0+=sx; }
   if(twice<=dx) { error+=dx;y0+=sy; }
  }
 }
 void fillTriangle(int ax,int ay,int bx,int by,int cx,int cy,uint16_t c) {
  for(int y=std::min({ay,by,cy});y<=std::max({ay,by,cy});++y)
   for(int x=std::min({ax,bx,cx});x<=std::max({ax,bx,cx});++x) {
    int a=(bx-ax)*(y-ay)-(by-ay)*(x-ax),b=(cx-bx)*(y-by)-(cy-by)*(x-bx),d=(ax-cx)*(y-cy)-(ay-cy)*(x-cx);
    if((a>=0&&b>=0&&d>=0)||(a<=0&&b<=0&&d<=0)) drawPixel(x,y,c);
   }
 }
 void fillCircle(int cx,int cy,int r,uint16_t c) {
  for(int y=-r;y<=r;++y) for(int x=-r;x<=r;++x) if(x*x+y*y<=r*r) drawPixel(cx+x,cy+y,c);
 }
 void drawCircle(int cx,int cy,int r,uint16_t c) {
  for(int i=0;i<720;++i) {float a=i*6.2831853f/720;drawPixel(lroundf(cx+r*cosf(a)),lroundf(cy+r*sinf(a)),c);}
 }
 void write(FILE* out) {
  for(int y=0;y<240;++y) for(int x=0;x<240;++x) {
   uint16_t p=pixels[y*240+x];
   if((x-119.5f)*(x-119.5f)+(y-119.5f)*(y-119.5f)>120.0f*120.0f) p=0;
   unsigned char b[]={static_cast<unsigned char>((p>>11)*255/31),
    static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};
   if(fwrite(b,1,3,out)!=3) throw std::runtime_error("Write failure");
  }
 }
};
}
'''

HARNESS=r'''
#include "RolexDrawing.h"
int main(int argc,char** argv) {
 if(argc!=3) return 1;
 FILE* still=fopen(argv[1],"wb");FILE* movie=fopen(argv[2],"wb");
 if(!still||!movie) return 2;
 lgfx::LGFXBase canvas;
 tm clock={};clock.tm_hour=10;clock.tm_min=8;clock.tm_sec=32;clock.tm_mday=30;
 RolexFace::draw(canvas,clock,true);canvas.write(still);
 const int dates[]={1,3,9,10,20,28,31};
 for(int day:dates) {
  clock.tm_mday=day;RolexFace::draw(canvas,clock,true);canvas.write(still);
 }
 RolexFace::draw(canvas,clock,false);canvas.write(still);
 // Before synchronization, only the date dash may alter the static dial.
 for(int y=0;y<240;++y) for(int x=0;x<240;++x) {
  if(x>=173 && x<=179 && y==120) continue;
  if(canvas.pixels[y*240+x]!=DialTexture::PIXELS[y*240+x]) return 3;
 }
 // Exercise every calendar value, then a full day of all four hands.
 for(int day=1;day<=31;++day) RolexFace::drawDate(canvas,day);
 for(int minutes=0;minutes<1440;++minutes) {
  clock.tm_hour=minutes/60;clock.tm_min=minutes%60;
  RolexFace::draw(canvas,clock,true,.5f);
 }
 clock.tm_hour=10;clock.tm_min=8;clock.tm_mday=30;
 for(int tick=0;tick<100;++tick) {
  clock.tm_sec=30+tick/25;
  RolexFace::draw(canvas,clock,true,(tick%25)/25.0f);canvas.write(movie);
 }
 fclose(still);fclose(movie);
}
'''
compiler=shutil.which('g++')
if not compiler: raise SystemExit('g++ must be on PATH')
with tempfile.TemporaryDirectory(prefix='rolex-preview-') as folder:
 tmp=Path(folder)
 (tmp/'LovyanGFX.hpp').write_text(ADAPTER)
 (tmp/'preview.cpp').write_text(HARNESS)
 exe,stills,movie=tmp/'preview.exe',tmp/'stills.rgb',tmp/'movie.rgb'
 subprocess.run([compiler,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I',str(tmp),'-I',str(ROOT),str(tmp/'preview.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe),str(stills),str(movie)],check=True)
 raw,motion=stills.read_bytes(),movie.read_bytes()
size=240*240*3
assert len(raw)==9*size and len(motion)==100*size
frames=[Image.frombytes('RGB',(240,240),raw[i:i+size]) for i in range(0,len(raw),size)]
frames[0].resize((720,720),Image.Resampling.NEAREST).save(out/'preview.png')
contact=Image.new('RGB',(720,720))
for i,frame in enumerate(frames): contact.paste(frame,((i%3)*240,(i//3)*240))
contact.save(out/'preview-contact.png')
frames=[Image.frombytes('RGB',(240,240),motion[i:i+size]) for i in range(0,len(motion),size)]
palette=frames[0].quantize(colors=256)
frames=[frame.quantize(palette=palette,dither=Image.Dither.NONE) for frame in frames]
frames[0].save(out/'preview.gif',save_all=True,append_images=frames[1:],duration=40,loop=0,optimize=False)
print('Verified all 31 date values, unsynchronized state, and hand bounds over 24 hours; rendered 100 animation frames.')


