"""Pack the two original generated gold coin faces; requires NumPy and Pillow."""
from pathlib import Path
import argparse
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir',type=Path,default=ROOT)
out=parser.parse_args().output_dir
out.mkdir(parents=True,exist_ok=True)
source=Image.open(ROOT/'assets'/'gold-coin.png').convert('RGBA')
faces=[]
for side in range(2):
    cell=source.crop((round(source.width*side/2),0,round(source.width*(side+1)/2),source.height))
    box=cell.getchannel('A').point(lambda a:255 if a>=100 else 0).getbbox()
    if box is None: raise RuntimeError('Coin face missing')
    face=cell.crop(box).resize((172,172),Image.Resampling.LANCZOS)
    padded=Image.new('RGBA',(176,176))
    padded.alpha_composite(face,(2,2))
    faces.append(np.asarray(padded,dtype=np.uint16))
faces=np.asarray(faces)
rgb=((faces[:,:,:,0]>>3)<<11)|((faces[:,:,:,1]>>2)<<5)|(faces[:,:,:,2]>>3)
with (out/'CoinAssets.h').open('w') as f:
    f.write('// Packed from assets/gold-coin.png; original generated artwork.\n#pragma once\n#include <stdint.h>\n#ifdef ARDUINO\n#include <pgmspace.h>\n#else\n#define PROGMEM\n#endif\nnamespace CoinAssets {\nconstexpr int SIZE=176;\n')
    for name,array,kind in [('COLOR',rgb,'uint16_t'),('ALPHA',faces[:,:,:,3],'uint8_t')]:
        flat=array.ravel()
        f.write(f'const {kind} {name}[{len(flat)}] PROGMEM={{\n')
        for i in range(0,len(flat),24): f.write(','.join(str(int(n)) for n in flat[i:i+24])+',\n')
        f.write('};\n')
    f.write('}\n')
print('Packed both 176x176 gold coin faces with alpha.')
