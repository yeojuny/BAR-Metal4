#!/usr/bin/env python3
"""CPU geometry oracle, not Metal execution. Run with Python 3 + numpy."""
import hashlib, itertools, json, math, pathlib, re
import numpy as np

ROOT=pathlib.Path(__file__).resolve().parent
old=(ROOT.parent/'original/NativeSensorLOSShaders.h').read_text()
new=(ROOT/'NativeSensorLOSShaders.h').read_text()
change=' // Swapping major/minor axes reverses orientation. Reverse Y-major\n // triangle order only, preserving the old AABB front/back stencil contract.\n constexpr uint indicesYMajor[6]={0,2,1,0,3,2};\n const float2 c=corners[(xMajor?indices:indicesYMajor)[corner]];'
assert new.replace(change,' const float2 c=corners[indices[corner]];')==old
assert hashlib.sha256(old.encode()).hexdigest()=='b11a26584e84abca5c6addb19267637f400b92ee671f9e3d71786b5ae04e1ac8'
I=(0,1,2,0,2,3); Y=(0,2,1,0,3,2)
C=np.array(((0,0),(1,0),(1,1),(0,1)),dtype=np.float32)
f=np.float32
counts=dict(cases=0,tight=0,x_major=0,y_major=0,legacy_negative_controls=0,fallback=0,aa4_fallback=0,clipped=0)

def geometry(a,b,width,viewport=(0,0,128,128),samples=1):
    a,b=np.asarray(a,dtype=np.float32),np.asarray(b,dtype=np.float32)
    width=f(width);vp=np.asarray(viewport,dtype=np.float32)
    extent=f(max(1,math.floor(float(width)+.5))*.5+2) if np.isfinite(width) else f(3)
    low=np.minimum(a,b)-extent; high=np.maximum(a,b)+extent
    fallback=np.array([low+(high-low)*C[i] for i in I])
    if samples!=1 or not np.all(np.isfinite(a)) or not np.all(np.isfinite(b)) or not np.all(np.isfinite(vp)) or not np.isfinite(width) or width<=0 or width>64 or np.any(abs(a)>16384) or np.any(abs(b)>16384) or np.any(abs(vp[:2])>16384) or np.any(vp[2:]<1) or np.any(vp[2:]>16384): return fallback,None,None
    qa=np.floor(a*f(256)+f(.5)).astype(np.int64); qb=np.floor(b*f(256)+f(.5)).astype(np.int64)
    d=qb-qa; x=abs(d[0])>=abs(d[1]); j=0 if x else 1;k=1-j
    dm,di=d[j],d[k]
    if abs(dm)<256:return fallback,None,None
    ma,mb,mi=f(qa[j])*f(1/256),f(qb[j])*f(1/256),f(qa[k])*f(1/256)
    slope=f(di)/f(dm); band=f(max(1,math.floor(float(width)+.5))*.5+4)
    ml,mh=f(min(ma,mb)-4),f(max(ma,mb)+4)
    il,ih=f(mi+(ml-ma)*slope),f(mi+(mh-ma)*slope)
    area=f((high[0]-low[0])*(high[1]-low[1]));na=f((mh-ml)*(2*band))
    if not all(np.isfinite(t) for t in (slope,il,ih,area,na)) or abs(slope)>1 or not na<area:return fallback,None,None
    corners=[]
    for cx,cy in C:
        major=f(ml+(mh-ml)*cx);minor=f(il+(ih-il)*cx+(2*cy-1)*band)
        corners.append((major,minor) if x else (minor,major))
    return fallback,np.array([corners[i] for i in (I if x else Y)]),np.array([corners[i] for i in I])

def area(t):
    a,b,c=t.astype(np.float64)
    return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])

def check(a,b,width,viewport=(0,0,128,128),samples=1):
    base,fixed,legacy=geometry(a,b,width,viewport,samples);counts['cases']+=1
    if fixed is None:
        counts['fallback']+=1
        if samples==4:counts['aa4_fallback']+=1
        return
    counts['tight']+=1
    qa=np.floor(np.array(a)*256+.5);qb=np.floor(np.array(b)*256+.5);d=qb-qa;x=abs(d[0])>=abs(d[1]);counts['x_major' if x else 'y_major']+=1
    assert sorted(map(tuple,fixed))==sorted(map(tuple,legacy)), 'vertex multiset changed'
    for off in (0,3):
        assert area(base[off:off+3])>0
        assert area(fixed[off:off+3])>0, (a,b,width,fixed)
        assert abs(area(fixed[off:off+3]))==abs(area(legacy[off:off+3]))
        if not x:assert area(legacy[off:off+3])<0
    if not x:counts['legacy_negative_controls']+=1
    else:assert np.array_equal(fixed,legacy)

dirs=((1,0),(0,1),(1,1),(1,-1),(-1,1),(-1,-1),(2,1),(1,2),(2,-1),(1,-2),(-2,1),(-1,2),(-2,-1),(-1,-2))
for start,width,d,length,samples in itertools.product(((0,0),(64.25,-64.25),(-24,-64),(16300,-16000)),(1,1.49,1.5,3,4.5,16,64),dirs,(.2,1,48,140), (1,4)):
    end=np.array(start)+np.array(d)*length
    check(start,end,width,samples=samples);check(end,start,width,samples=samples)
# Exact major ties are X-major; clipping preserves order because only endpoints change.
def clip(a,b):
    a,b=np.array(a,dtype=np.float64),np.array(b,dtype=np.float64);delta=b-a;enter,leave=0.,1.
    av=(a[0]+a[3],a[3]-a[0],a[1]+a[3],a[3]-a[1],a[2],a[3]-a[2]);bv=(b[0]+b[3],b[3]-b[0],b[1]+b[3],b[3]-b[1],b[2],b[3]-b[2])
    for x,y in zip(av,bv):
        if x<0 and y<0:return None
        if x<0:enter=max(enter,x/(x-y))
        if y<0:leave=min(leave,x/(x-y))
    if enter>leave:return None
    a,b=a+enter*delta,a+leave*delta
    if a[3]<=0 or b[3]<=0:return None
    return ((a[:2]/a[3]+1)*64,(b[:2]/b[3]+1)*64)
for a,b in itertools.product(((-2,-.2,-.5,1),(.1,-2,.2,1),(-.5,.2,.2,.3)),((.5,.8,.8,1),(2,.1,.5,1),(.8,-.9,.8,2))):
    pair=clip(a,b)
    if pair:
        counts['clipped']+=1
        check(*pair,3);check(*pair[::-1],3)
rng=np.random.default_rng(20261116)
for _ in range(10000):
    a=rng.uniform(-16000,16000,2);b=rng.uniform(-16000,16000,2);check(a,b,rng.uniform(.01,64))
for a,b,w,vp,samples in [((0,0),(100,75),0,(0,0,128,128),1),((0,0),(100,75),65,(0,0,128,128),1),((17000,0),(100,75),3,(0,0,128,128),1),((0,0),(100,75),3,(0,0,0,128),1),((0,0),(.2,.1),3,(0,0,128,128),1),((0,0),(100,75),3,(0,0,128,128),4)]:
    assert geometry(a,b,w,vp,samples)[1] is None
    check(a,b,w,vp,samples)
assert all(counts[k]>0 for k in ('x_major','y_major','legacy_negative_controls','aa4_fallback','clipped'))
print(json.dumps({'status':'PASS','scope':'CPU geometry and exact source-diff checks; not Metal/GPU execution','counts':counts},indent=2))
