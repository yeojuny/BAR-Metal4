#!/usr/bin/env python3
"""CPU float32 setup/transport and exact coverage checks; not MSL/GPU evidence."""
import pathlib, hashlib, json, math, struct
import numpy as np
P=pathlib.Path(__file__).resolve().parent
base=(P/'reference/R03.h').read_text()
new=(P/'NativeSensorLOSShaders.h').read_text()
assert hashlib.sha256(base.encode()).hexdigest()=='043e006fc06d8ff14f201abf5ef9413cc7987b1c70e789e3b0da971ffe9a6c7c'
a=new.index('// R04: shared, exact segment setup.');b=new.index('struct SensorLOSVarying',a)
stripped=new[:a]+new[b:]
a=stripped.index('inline bool SensorLineCoveredPrepared(');b=stripped.index('inline bool SensorAlpha(',a)
stripped=stripped[:a]+stripped[b:]
stripped=stripped.replace('\tint4 coverageEndpoints [[flat]],coverageSetup [[flat]];\n','')
stripped=stripped.replace('\tSensorLOSPrepareCoverage(out.lineA.xyz,out.lineB.xyz,u.parameters.x,u.counts.w,out.coverageEndpoints,out.coverageSetup);\n','')
newcall='SensorLineCoveredPrepared(in.lineA.xyz,in.lineB.xyz,glPixel,u.parameters.x,u.counts.w,in.coverageEndpoints,in.coverageSetup,coverageDepth)'
oldcall='SensorLineCoveredEarly(in.lineA.xyz,in.lineB.xyz,glPixel,u.parameters.x,u.counts.w,coverageDepth)'
assert stripped.replace(newcall,oldcall)==base
# Prepared body is literally R03 from pixel to return, except rounded count load.
bod=lambda s,name:s[s.index(' const int2 pixel=',s.index(name)):s.index('\n}\n',s.index(' const int2 pixel=',s.index(name)))+3]
assert bod(new,'inline bool SensorLineCoveredPrepared(').replace(' const int count=setup.z;',' const int count=max(1,int(floor(width+0.5f)));')==bod(base,'inline bool SensorLineCoveredEarly(')
f32=lambda x:float(np.float32(x))
q=lambda x:math.floor(f32(f32(x)*256.+.5))
# Multiplication by power of two is exact in range; separately rounded add.
def setup(a,b,w,s):
 if s!=1 or not all(math.isfinite(x) for x in (*a,*b,w)) or not 0<w<=64 or max(map(abs,(*a[:2],*b[:2])))>16384:return (0,)*8
 aa=tuple(q(x) for x in a[:2]);bb=tuple(q(x) for x in b[:2]);d=tuple(y-x for x,y in zip(aa,bb))
 data=(*aa,*bb,*d,max(1,math.floor(f32(w+.5))),int(abs(d[0])>=abs(d[1])))
 return struct.unpack('<8i',struct.pack('<8i',*data))
# Reuse R03's independent exact diamond predicate and full corpus, augment
# every case with an independently transported prepared input and coverage result.
src=(P/'reference/coverage_corpus.py').read_text()
start=src.index('R=random.Random');src=src[start:]
src=src.replace("print(json.dumps(dict(status=", "print(json.dumps(dict(r04_prepared=PC,status=")
insert='''
 prep=setup((*a,0.),(*b,1.),w,s)
 eligible=prep[6]>0 and all(math.isfinite(v) for v in p) and max(map(abs,p))<=32768
 assert eligible==g
 if eligible:
  PA,PB=prep[:2],prep[2:4];PD=prep[4:6];PX=bool(prep[7]);PJ=0 if PX else 1;PK=1-PJ
  assert PA==aa and PB==bb and PD==d and PX==x and prep[6]==count
  PN=PA[PK]*PD[PJ]+(pixel[PJ]*256+128-PA[PJ])*PD[PK];DD=PD[PJ]*256
  if DD<0:PN,DD=-PN,-DD
  LL=(2*PN-(prep[6]-1)*DD-1)//(2*DD)
  candidate=False
  if LL<=pixel[PK]<LL+prep[6]:
   CC=PN//DD
   candidate=any(diamond(PA,PB,(pixel[PJ],i) if PX else (i,pixel[PJ])) for i in range(CC-1,CC+2))
  assert candidate==new_result
  PC['prepared_nonzero']+=1
 else:PC['fallback']+=1
'''
src=src.replace(' assert old_result==new_result',' assert old_result==new_result'+insert)
# Additional zero-length and nonfinite setup sentinels and nonfinite Z guards.
PC={'prepared_nonzero':0,'fallback':0,'zero_delta':0,'sentinel_checks':0}
for z in [float('nan'),float('inf'),-float('inf')]:
 assert setup((0.,0.,z),(1.,2.,0.),1.,1)==(0,)*8;PC['sentinel_checks']+=1
for s in [0,2,4,8]:
 assert setup((0.,0.,0.),(1.,2.,1.),1.,s)==(0,)*8;PC['sentinel_checks']+=1
for w in [0.,-1.,65.,float('nan'),float('inf')]:
 assert setup((0.,0.,0.),(1.,2.,1.),w,1)==(0,)*8;PC['sentinel_checks']+=1
for a in [(0.,0.,0.),(16384.,-16384.,1.),(.001,.001,0.)]:
 t=setup(a,a,1.,1);assert t[4:6]==(0,0) and t[6]>0;PC['zero_delta']+=1
import random
exec(src)
# GPU helper must initialize packed fields and compare against R03, not itself.
h=(P/'gpu_pixel_parity.mm').read_text()
assert 'SensorLOSPrepareCoverage(out.lineA.xyz' in h
assert '@"'+newcall+'"' in h and '@"'+oldcall+'"' in h

assert 'if(!r04PreparedWitness)Fail(' in h
assert 'const int count=0; // test-only disabled prepared coverage' in h
