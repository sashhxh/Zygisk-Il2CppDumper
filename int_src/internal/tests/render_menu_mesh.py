#!/usr/bin/env python3
"""Rasterize the actual ImGui draw data exported by menu_world_test.
Requires numpy and Pillow; this is a host preview, not an Android GPU capture.
"""
from pathlib import Path
import argparse
import numpy as np
from PIL import Image
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('prefix',type=Path)
p.add_argument('output',type=Path)
p.add_argument('--full',action='store_true',help='save the entire framebuffer, including ESP fixtures')
a=p.parse_args()
with a.prefix.with_suffix('.atlas').open('rb') as f:
    tw,th=map(int,f.readline().split())
    texture=np.frombuffer(f.read(),np.uint8).reshape(th,tw,4).astype(np.float32)/255
with a.prefix.with_suffix('.mesh').open() as f:
    width,height,count=map(int,f.readline().split())
    screen=np.empty((height,width,3),np.float32);screen[:]=(.055,.065,.08)
    for _ in range(count):
        nv,ni,nc=map(int,f.readline().split())
        raw=[f.readline().split() for i in range(nv)]
        vertices=np.array([[float(x) for x in row[:4]] for row in raw])
        colors=np.array([[int(row[4])>>shift&255 for shift in (0,8,16,24)] for row in raw],np.float32)/255
        indices=np.array(list(map(int,f.readline().split())));assert len(indices)==ni
        for _ in range(nc):
            row=f.readline().split();elements,offset,base=map(int,row[:3]);clip=list(map(float,row[3:]))
            for start in range(offset,offset+elements,3):
                ids=indices[start:start+3]+base;v=vertices[ids];c=colors[ids]
                left=max(0,int(np.floor(max(v[:,0].min(),clip[0]))));right=min(width,int(np.ceil(min(v[:,0].max(),clip[2]))))
                top=max(0,int(np.floor(max(v[:,1].min(),clip[1]))));bottom=min(height,int(np.ceil(min(v[:,1].max(),clip[3]))))
                if right<=left or bottom<=top:continue
                den=(v[1,1]-v[2,1])*(v[0,0]-v[2,0])+(v[2,0]-v[1,0])*(v[0,1]-v[2,1])
                if abs(den)<1e-8:continue
                y,x=np.mgrid[top:bottom,left:right];x=x+.5;y=y+.5
                b0=((v[1,1]-v[2,1])*(x-v[2,0])+(v[2,0]-v[1,0])*(y-v[2,1]))/den
                b1=((v[2,1]-v[0,1])*(x-v[2,0])+(v[0,0]-v[2,0])*(y-v[2,1]))/den
                b2=1-b0-b1
                # A half-open edge prevents double blending a shared diagonal.
                mask=(b0>=0)&(b1>=0)&(b2>0)
                if not mask.any():continue
                weights=np.stack([b0[mask],b1[mask],b2[mask]],axis=1)
                uv=weights@v[:,2:4];rgba=weights@c
                tx=np.clip((uv[:,0]*tw).astype(int),0,tw-1);ty=np.clip((uv[:,1]*th).astype(int),0,th-1)
                rgba=rgba*texture[ty,tx];alpha=rgba[:,3:4]
                dest=screen[top:bottom,left:right];dest[mask]=rgba[:,:3]*alpha+dest[mask]*(1-alpha)
scale=max(.1,min(1,(width-16)/840,(height-16)/720));mw,mh=840*scale,720*scale
mx=(width-mw)/2;my=(height-mh)/2
image=Image.fromarray(np.clip(screen*255,0,255).astype(np.uint8))
if not a.full:image=image.crop((int(mx-12),int(my-12),int(mx+mw+12),int(my+mh+12)))
image.save(a.output)
print('Saved host preview:',a.output)
