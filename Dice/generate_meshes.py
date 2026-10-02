"""Generate actual convex polyhedra for all six DnD die shapes (NumPy)."""
from pathlib import Path
import argparse,itertools
import numpy as np
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir',type=Path,default=Path(__file__).resolve().parent)
out=parser.parse_args().output_dir
out.mkdir(parents=True,exist_ok=True)
phi=(1+np.sqrt(5))/2
shapes={
4:[(1,1,1),(1,-1,-1),(-1,1,-1),(-1,-1,1)],
6:list(itertools.product([-1,1],repeat=3)),
8:[tuple(s if axis==i else 0 for axis in range(3)) for i in range(3) for s in [-1,1]],
10:[(0,0,1.1),(0,0,-1.1)]+[(np.cos(i*np.pi/5),np.sin(i*np.pi/5),
       (1 if i%2 else -1)*1.1*(1-np.cos(np.pi/5))/(1+np.cos(np.pi/5))) for i in range(10)],
12:list(itertools.product([-1,1],repeat=3))+[(0,a/phi,b*phi) for a,b in itertools.product([-1,1],repeat=2)]+
   [(b*phi,0,a/phi) for a,b in itertools.product([-1,1],repeat=2)]+
   [(a/phi,b*phi,0) for a,b in itertools.product([-1,1],repeat=2)],
20:[(0,a,b*phi) for a,b in itertools.product([-1,1],repeat=2)]+
   [(b*phi,0,a) for a,b in itertools.product([-1,1],repeat=2)]+
   [(a,b*phi,0) for a,b in itertools.product([-1,1],repeat=2)]}
with (out/'DiceMeshes.h').open('w') as f:
 f.write('#pragma once\nnamespace DiceMeshes {\nstruct Vec { float x,y,z; };\nstruct Face { unsigned char count,index[5]; Vec normal; };\nstruct Mesh { int vertices,faces; const Vec* vertex; const Face* face; };\n')
 for count,points in shapes.items():
  vertices=np.array([list(p) for p in points],dtype=float)
  vertices/=np.linalg.norm(vertices,axis=1).max()
  facets={}
  for a,b,c in itertools.combinations(range(len(vertices)),3):
   normal=np.cross(vertices[b]-vertices[a],vertices[c]-vertices[a])
   length=np.linalg.norm(normal)
   if length<1e-6: continue
   normal/=length
   distance=vertices@normal-vertices[a]@normal
   if np.all(distance<=1e-6): pass
   elif np.all(distance>=-1e-6): normal=-normal; distance=-distance
   else: continue
   ids=tuple(np.where(abs(distance)<1e-6)[0])
   facets[ids]=normal
  assert len(facets)==count,(count,len(facets))
  f.write(f'const Vec V{count}[]={{\n')
  for vertex in vertices: f.write('{'+','.join(f'{v:.8f}f' for v in vertex)+'},\n')
  f.write('};\n'+f'const Face F{count}[]={{\n')
  for ids,normal in facets.items():
   center=vertices[list(ids)].mean(axis=0)
   axis=vertices[ids[0]]-center; axis/=np.linalg.norm(axis)
   cross=np.cross(normal,axis)
   ordered=sorted(ids,key=lambda i:np.arctan2((vertices[i]-center)@cross,(vertices[i]-center)@axis))
   padded=ordered+[0]*(5-len(ordered))
   f.write('{'+str(len(ids))+',{'+','.join(str(i) for i in padded)+'},{'+','.join(f'{v:.8f}f' for v in normal)+'}},\n')
  f.write('};\n'+f'const Mesh D{count}={{ {len(vertices)}, {count}, V{count}, F{count} }};\n')
 f.write('}\n')
print('Verified face counts for D4, D6, D8, D10, D12 and D20.')
