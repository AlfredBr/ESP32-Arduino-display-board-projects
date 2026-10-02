#pragma once
#include <LovyanGFX.hpp>
#include <math.h>
#include <stdio.h>
#include "DiceMeshes.h"
#include "UiText.h"
namespace DiceRenderer {
using Vec=DiceMeshes::Vec;
inline Vec add(Vec a,Vec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec mul(Vec a,float s) { return {a.x*s,a.y*s,a.z*s}; }
inline float dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec cross(Vec a,Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Vec unit(Vec a) { return mul(a,1/sqrtf(dot(a,a))); }
inline Vec spin(Vec a,float angle) {
 const float c=cosf(angle),s=sinf(angle),c2=cosf(angle*.73f),s2=sinf(angle*.73f);
 Vec b={a.x,a.y*c-a.z*s,a.y*s+a.z*c};
 return {b.x*c2+b.z*s2,b.y,-b.x*s2+b.z*c2};
}
inline const DiceMeshes::Mesh& mesh(int choice) {
 switch(choice) {
  case 0:return DiceMeshes::D4; case 1:return DiceMeshes::D6;
  case 2:return DiceMeshes::D8; case 3:case 6:return DiceMeshes::D10;
  case 4:return DiceMeshes::D12; default:return DiceMeshes::D20;
 }
}
inline uint16_t material(int choice,float light) {
 static const int rgb[][3]={{87,220,170},{86,177,255},{187,115,252},{238,154,76},
                          {219,91,154},{77,194,223},{189,149,253}};
 if(light<.18f) light=.18f;
 if(light>1) light=1;
 const unsigned r=rgb[choice][0]*light,g=rgb[choice][1]*light,b=rgb[choice][2]*light;
 return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);
}
inline void draw(lgfx::LGFXBase& canvas,int choice,int faceIndex,int cx,int cy,
                 float radius,float motion,bool tens=false,bool labels=true) {
 const auto& m=mesh(choice);
 const auto& chosen=m.face[faceIndex%m.faces];
 Vec center={0,0,0};
 for(int j=0;j<chosen.count;++j) center=add(center,m.vertex[chosen.index[j]]);
 center=mul(center,1.0f/chosen.count);
 const Vec up=unit(add(m.vertex[chosen.index[0]],mul(center,-1)));
 const Vec forward=chosen.normal,right=cross(up,forward);
 auto rotate=[&](Vec p) {
   p=spin(p,motion);
   Vec q={dot(p,right),dot(p,up),dot(p,forward)};
   const float c=.97589745f,s=.21822962f;
   Vec r={q.x*c+q.z*s,q.y,-q.x*s+q.z*c};
   return Vec{r.x,r.y*.9838437f+r.z*.1790296f,-r.y*.1790296f+r.z*.9838437f};
 };
 Vec vertices[20]; int px[20],py[20];
 for(int i=0;i<m.vertices;++i) {
   vertices[i]=rotate(m.vertex[i]);
   const float perspective=1/(1-vertices[i].z*.12f);
   px[i]=lroundf(cx+vertices[i].x*radius*perspective);
   py[i]=lroundf(cy-vertices[i].y*radius*perspective);
 }
 struct Visible { int index; float z; Vec normal; };
 Visible faces[20]; int total=0;
 for(int i=0;i<m.faces;++i) {
   Vec n=rotate(m.face[i].normal);
   if(n.z>.025f) faces[total++]={i,n.z,n};
 }
 for(int i=1;i<total;++i) for(int j=i;j>0 && faces[j].z<faces[j-1].z;--j) {
   const auto temp=faces[j];faces[j]=faces[j-1];faces[j-1]=temp;
 }
 for(int k=0;k<total;++k) {
   const auto& face=m.face[faces[k].index];
   const Vec n=faces[k].normal;
   const float light=.37f+.58f*fmaxf(0,-.35f*n.x+.52f*n.y+.77f*n.z);
   const uint16_t fill=material(choice,light);
   for(int j=1;j<face.count-1;++j)
     canvas.fillTriangle(px[face.index[0]],py[face.index[0]],px[face.index[j]],py[face.index[j]],
                         px[face.index[j+1]],py[face.index[j+1]],fill);
   int tx=0,ty=0;
   for(int j=0;j<face.count;++j) {
     const int a=face.index[j],b=face.index[(j+1)%face.count];
     canvas.drawLine(px[a],py[a],px[b],py[b],material(choice,fminf(1,light+.2f)));
     tx+=px[a];ty+=py[a];
   }
   if(labels && faces[k].z>.5f && radius>=34) {
     char number[4]; const int value=faces[k].index+(choice==3 || choice==6?0:1);
     if(tens) snprintf(number,sizeof(number),"%02d",value*10);
     else snprintf(number,sizeof(number),"%d",value);
     const int scale=radius>55?2:1;
     UiText::draw(canvas,number,tx/face.count,ty/face.count-3*scale,scale,0xFF57);
   }
 }
}
}
