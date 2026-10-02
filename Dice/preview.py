"""Render the menu and every die result from the actual Dice C++ code."""
from pathlib import Path
import argparse,shutil,subprocess,tempfile
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
   if((x-119.5f)*(x-119.5f)+(y-119.5f)*(y-119.5f)>119.5f*119.5f) p=0;
   unsigned char b[]={static_cast<unsigned char>((p>>11)*255/31),
    static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};
   if(fwrite(b,1,3,out)!=3) throw std::runtime_error("Write failure");
  }
 }
};
}
'''
HARNESS=r'''
#include "DiceApp.h"
int main(int argc,char** argv) {
 if(argc!=3) return 1;
 FILE* still=fopen(argv[1],"wb");FILE* movie=fopen(argv[2],"wb");
 if(!still||!movie) return 2;
 lgfx::LGFXBase canvas;Dice::App app;
 app.begin();app.draw(canvas);canvas.write(still);
 app.tap(120,210);app.draw(canvas);canvas.write(still);
 for(int choice=0;choice<8;++choice) {
  app.begin();if(choice>=4) app.tap(120,210);
  if(app.tap(76+(choice%2)*87,80+((choice%4)/2)*74)) return 3;
  if(app.choice()!=choice || !app.tap(120,120)) return 4;
  app.roll(app.range()-1,9,0x1231);
  for(int i=0;i<90;++i) app.update(1.0f/30);
  if(!app.resultReady()) return 5;
  const int expected[]={4,6,8,9,12,20,99,1};
  if(app.outcome()!=expected[choice]) return 6;
  app.draw(canvas);canvas.write(still);
 }
 app.begin();app.tap(120,210);app.tap(76,154);app.roll(0,0,0);
 for(int i=0;i<90;++i) app.update(1.0f/30);
 if(app.outcome()!=100) return 7;
 fclose(still);
 app.begin();
 for(int tick=0;tick<300;++tick) {
  if(tick==30) {app.tap(120,210);app.tap(163,80);}
  if(tick==48) app.roll(19,0,0);
  if(tick==140) app.tap(60,30);
  if(tick==180) app.tap(163,154);
  if(tick==195) app.roll(0,0,0);
  app.update(1.0f/30);
  if(tick%2==0) {app.draw(canvas);canvas.write(movie);}
 }
 fclose(movie);
}
'''
compiler=shutil.which('g++')
if not compiler: raise SystemExit('g++ must be on PATH')
with tempfile.TemporaryDirectory(prefix='dice-preview-') as folder:
 tmp=Path(folder)
 (tmp/'LovyanGFX.hpp').write_text(ADAPTER)
 (tmp/'preview.cpp').write_text(HARNESS)
 exe,stills,movie=tmp/'preview.exe',tmp/'stills.rgb',tmp/'movie.rgb'
 subprocess.run([compiler,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-I',str(tmp),'-I',str(ROOT),str(tmp/'preview.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe),str(stills),str(movie)],check=True)
 still_data,movie_data=stills.read_bytes(),movie.read_bytes()
size=240*240*3
assert len(still_data)==10*size and len(movie_data)==150*size
images=[Image.frombytes('RGB',(240,240),still_data[i:i+size]) for i in range(0,len(still_data),size)]
contact=Image.new('RGB',(1200,480))
for i,im in enumerate(images): contact.paste(im,((i%5)*240,(i//5)*240))
contact.save(out/'preview-contact.png')
images[0].resize((720,720),Image.Resampling.NEAREST).save(out/'preview.png')
frames=[Image.frombytes('RGB',(240,240),movie_data[i:i+size]) for i in range(0,len(movie_data),size)]
palette=contact.quantize(colors=256)
indexed=[f.quantize(palette=palette,dither=Image.Dither.NONE) for f in frames]
indexed[0].save(out/'preview.gif',save_all=True,append_images=indexed[1:],duration=[60,70,70]*50,loop=0,optimize=False)
print('Verified both menus, all 8 choices, result faces, percentile 00+0=100, and render bounds.')
