"""Pack the generated handless dial as RGB565; requires Pillow and NumPy."""
from pathlib import Path
import argparse
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir',type=Path,default=ROOT)
out=parser.parse_args().output_dir
out.mkdir(parents=True,exist_ok=True)
source=Image.open(ROOT/'assets'/'dial.png').convert('RGB')
# The photographed rim is slightly off-center and has a black margin.
# Fit an inscribed 1210px circle to the 1254px source, with a little overscan
# so the brushed bezel reaches every edge of the physical circular display.
left,top,side=18*source.width/1254,17*source.height/1254,1210*source.width/1254
image=source.resize((240,240),Image.Resampling.LANCZOS,
                    box=(left,top,left+side,top+side))
scale=source.width/side
offset_x=(scale-1)*.5-left*240/side
offset_y=(scale-1)*.5-top*240/side
rgb=np.asarray(image,dtype=np.uint16)
y,x=np.mgrid[:240,:240]
rgb=rgb.copy()
rgb[(x-119.5)**2+(y-119.5)**2>120**2]=0
packed=((rgb[:,:,0]>>3)<<11)|((rgb[:,:,1]>>2)<<5)|(rgb[:,:,2]>>3)
with (out/'DialTexture.h').open('w') as f:
 f.write('// Packed from assets/dial.png by prepare_assets.py.\n#pragma once\n#include <stdint.h>\n#ifdef ARDUINO\n#include <pgmspace.h>\n#else\n#define PROGMEM\n#endif\nnamespace DialTexture {\n')
 f.write(f'constexpr float SCALE={scale:.8f}f, OFFSET_X={offset_x:.8f}f, OFFSET_Y={offset_y:.8f}f;\n')
 f.write('const uint16_t PIXELS[57600] PROGMEM={\n')
 flat=packed.ravel()
 for i in range(0,len(flat),24): f.write(','.join(str(int(n)) for n in flat[i:i+24])+',\n')
 f.write('};\n}\n')
print('Packed the dial background at 240x240 RGB565.')
