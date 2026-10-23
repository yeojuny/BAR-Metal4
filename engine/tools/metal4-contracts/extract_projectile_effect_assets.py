#!/usr/bin/env python3
# This file is part of the Spring engine (GPL v2 or later), see LICENSE.html
from pathlib import Path
import gzip,hashlib,json,struct,zlib
import argparse
p=argparse.ArgumentParser(description='Verify and extract exact test30770 projectile source/images');p.add_argument('--assets',type=Path,required=True);p.add_argument('--work',type=Path,required=True);a=p.parse_args();assets=a.assets;work=a.work;work.mkdir(parents=True,exist_ok=True)
wanted={f'bitmaps/projectiletextures/{x}.tga'for x in ('largebeam','flare2','plasmaball','glow2')}|{'luarules/gadgets/gfx_beam_laser_gl4.lua','luarules/gadgets/gfx_plasma_cannon_gl4.lua','modules/graphics/instancevbotable.lua','luarules/gadgets.lua'}
b=gzip.decompress((assets/'packages/0c038cabef0706d6311d828c3eeb794a.sdp').read_bytes());i=0;rows=[]
def tga(raw):
 ident,cmap,kind=raw[0:3];w,h,bits,desc=struct.unpack_from('<HHBB',raw,12);assert not cmap and kind in (2,3,10,11)and bits in(8,24,32)
 p=18+ident;n=bits//8;pixels=[]
 while len(pixels)<w*h:
  count=1;repeat=False
  if kind in (10,11):
   header=raw[p];p+=1;count=(header&127)+1;repeat=bool(header&128)
  if repeat:
   row=raw[p:p+n];p+=n;pixels.extend([row]*count)
  else:
   pixels.extend(raw[p+j*n:p+(j+1)*n]for j in range(count));p+=n*count
 assert len(pixels)==w*h
 rgba=bytearray()
 # IL_ORIGIN_UPPER_LEFT in the real CBitmap loader.
 for y in range(h):
  sy=y if desc&32 else h-1-y
  for x in range(w):
   sx=w-1-x if desc&16 else x;q=pixels[sy*w+sx];rgba.extend((q[0],q[0],q[0],255)if n==1 else(q[2],q[1],q[0],q[3]if n==4 else 255))
 return w,h,rgba
while i<len(b):
 length=b[i];i+=1;name=b[i:i+length].decode();i+=length;md5=b[i:i+16].hex();i+=16;crc,size=struct.unpack_from('>II',b,i);i+=8
 if name.lower()not in wanted:continue
 raw=gzip.decompress((assets/'pool'/md5[:2]/(md5[2:]+'.gz')).read_bytes());assert len(raw)==size and hashlib.md5(raw).hexdigest()==md5 and zlib.crc32(raw)==crc
 p=work/'exact'/name.lower();p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw)
 row={'path':name,'md5':md5,'crc32':crc,'size':size,'sha256':hashlib.sha256(raw).hexdigest(),'sha512':hashlib.sha512(raw).hexdigest()}
 if name.lower().endswith('.tga'):
  width,height,rgba=tga(raw);rgbaPath=p.with_suffix('.rgba');rgbaPath.write_bytes(rgba);row.update(width=width,height=height,rgbaSHA256=hashlib.sha256(rgba).hexdigest())
 rows.append(row)
assert len(rows)==len(wanted)
(work/'original-assets.json').write_text(json.dumps(rows,indent=2)+'\n');print(json.dumps(rows,indent=2))
