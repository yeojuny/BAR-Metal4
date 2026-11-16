#!/usr/bin/env python3
"""Independent integer coverage oracle versus float32 candidate geometry.

This is a containment test, not a substitute for the GPU color/depth/stencil
comparison. It deliberately retains the old diamond-exit rule, including its
symbolic epsilon tie breaks, rather than testing only distances to a line.
"""
import hashlib
import json
import math
from pathlib import Path
import random
import struct

ROOT = Path(__file__).resolve().parent.parent
F = lambda x: struct.unpack('f', struct.pack('f', x))[0]
add = lambda a,b: F(F(a)+F(b))
sub = lambda a,b: F(F(a)-F(b))
mul = lambda a,b: F(F(a)*F(b))
div = lambda a,b: F(F(a)/F(b))

def quantize(p):
    return tuple(math.floor(add(mul(v,256),.5)) for v in p)

def compare(a,b):
    for k in (0,2,3):
        v=a[k]*b[1]-b[k]*a[1]
        if v: return -1 if v<0 else 1
    return 0

def fraction(n,d,e1,e2):
    return (n,d,e1,e2) if d>=0 else (-n,-d,-e1,-e2)

def diamond(a,b,pixel):
    start=(a[0]+a[1],a[0]-a[1])
    end=(b[0]+b[1],b[0]-b[1])
    center=((pixel[0]+pixel[1]+1)*256,(pixel[0]-pixel[1])*256)
    zero=(0,1,0,0);one=(1,1,0,0);enter=zero;leave=one
    for k in range(2):
        d=end[k]-start[k];low=center[k]-128;high=center[k]+128
        if d==0:
            if start[k]<=low or start[k]>high:return False
            continue
        lo=fraction(low-start[k],d,1,1 if k==0 else -1)
        hi=fraction(high-start[k],d,1,1 if k==0 else -1)
        if compare(lo,hi)>0:lo,hi=hi,lo
        if compare(lo,enter)>0:enter=lo
        if compare(hi,leave)<0:leave=hi
    return compare(leave,enter)>0 and compare(leave,zero)>=0 and compare(enter,one)<=0 and compare(leave,one)<0

def covered(a,b,pixel,count):
    d=(b[0]-a[0],b[1]-a[1])
    if d==(0,0):return False
    x=abs(d[0])>=abs(d[1]);k=0 if x else 1;j=1-k
    ma=a[k];mi=a[j];dm=d[k];di=d[j];major=pixel[k]
    numerator=mi*dm+(major*256+128-ma)*di;denominator=dm*256
    if denominator<0:numerator=-numerator;denominator=-denominator
    center=numerator//denominator
    if not any(diamond(a,b,(major,v) if x else (v,major)) for v in range(center-1,center+2)):return False
    lower=(2*numerator-(count-1)*denominator-1)//(2*denominator)
    return lower<=pixel[j]<lower+count

def old_bounds(a,b,width):
    extent=add(mul(max(1,math.floor(add(width,.5))),.5),2)
    lo=tuple(sub(min(a[k],b[k]),extent) for k in range(2))
    hi=tuple(add(max(a[k],b[k]),extent) for k in range(2))
    return lo,hi

def candidate(a,b,width,viewport=(0.,32.,1512.,950.),samples=1):
    # Float32 operation order follows the candidate helper; the coverage oracle
    # above is independent, exact integer arithmetic with symbolic tie breaks.
    if samples!=1 or not all(map(math.isfinite,(*a,*b,*viewport,width))) or not 0<width<=64 or max(map(abs,(*a,*b,*viewport[:2])))>16384 or min(viewport[2:])<1 or max(viewport[2:])>16384:return None
    qa=quantize(a);qb=quantize(b);delta=tuple(qb[k]-qa[k] for k in range(2))
    k=0 if abs(delta[0])>=abs(delta[1]) else 1;j=1-k;dm=delta[k];di=delta[j]
    if abs(dm)<256:return None
    ma=mul(qa[k],1/256);mb=mul(qb[k],1/256);mi=mul(qa[j],1/256);slope=div(di,dm)
    count=max(1,math.floor(add(width,.5)));band=add(mul(count,.5),4)
    ml=sub(min(ma,mb),4);mh=add(max(ma,mb),4)
    nl=add(mi,mul(sub(ml,ma),slope));nh=add(mi,mul(sub(mh,ma),slope))
    lo,hi=old_bounds(a,b,width);oldarea=mul(sub(hi[0],lo[0]),sub(hi[1],lo[1]));newarea=mul(sub(mh,ml),mul(2,band))
    if not all(map(math.isfinite,(slope,nl,nh,oldarea,newarea))) or abs(slope)>1 or not newarea<oldarea:return None
    q=[]
    for m,n in ((ml,sub(nl,band)),(mh,sub(nh,band)),(mh,add(nh,band)),(ml,add(nl,band))):
        q.append((m,n) if k==0 else (n,m))
    return q,oldarea,newarea

def inside(q,p):
    crosses=[]
    for i in range(4):
        a=q[i];b=q[(i+1)%4]
        crosses.append((b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]))
    return min(crosses)>=-1e-7 or max(crosses)<=1e-7

def clip(a,b):
    # Existing homogeneous near/far/side clip: inputs and operations are f32.
    a=tuple(map(F,a));b=tuple(map(F,b));d=tuple(sub(y,x) for x,y in zip(a,b));enter=F(0);leave=F(1)
    av=(add(a[0],a[3]),sub(a[3],a[0]),add(a[1],a[3]),sub(a[3],a[1]),a[2],sub(a[3],a[2]))
    bv=(add(b[0],b[3]),sub(b[3],b[0]),add(b[1],b[3]),sub(b[3],b[1]),b[2],sub(b[3],b[2]))
    for x,y in zip(av,bv):
        if x<0 and y<0:return None
        if x<0:enter=max(enter,div(x,sub(x,y)))
        if y<0:leave=min(leave,div(x,sub(x,y)))
    if enter>leave:return None
    aa=tuple(add(x,mul(enter,z)) for x,z in zip(a,d));bb=tuple(add(x,mul(leave,z)) for x,z in zip(a,d))
    if aa[3]<=0 or bb[3]<=0:return None
    def screen(v):return (mul(add(div(v[0],v[3]),1),756),sub(-32,mul(sub(1,div(v[1],v[3])),475)))
    return screen(aa),screen(bb)

def main():
    rng=random.Random(0x53454e534f52)
    cases=[];widths=[.01,.49,.5,.999,1,1.49,1.5,2,2.5,4.4999,4.5,5.01923084,6,8,31.5,63.5,64]
    # All octants, reversed endpoints, diagonal ties, half/256 rounding ties,
    # negative screen coordinates, translated large viewports, short segments.
    for shift in [0.,-127.5,511.998046875,16000.]:
        for dx,dy in [(0,0),(.0001,.0002),(1,1),(8,0),(0,8),(8,8),(8,7.996),(32,17),(300,299.998),(1024,1),(1024,1024)]:
            for sx in [-1,1]:
                for sy in [-1,1]:
                    for w in widths:
                        a=(F(shift+.498046875),F(-shift-.501953125));b=(add(a[0],sx*dx),add(a[1],sy*dy))
                        cases.extend([(a,b,F(w)),(b,a,F(w))])
    for _ in range(1800):
        a=(F(rng.uniform(-16000,16000)),F(rng.uniform(-16000,16000)))
        angle=rng.uniform(-math.pi,math.pi);length=10**rng.uniform(-3,3.4)
        b=(add(a[0],length*math.cos(angle)),add(a[1],length*math.sin(angle)))
        cases.append((a,b,F(rng.choice(widths))))
    clipped=0
    for _ in range(400):
        a=(rng.uniform(-2,2),rng.uniform(-2,2),rng.uniform(-1,2),rng.uniform(.01,2))
        b=(rng.uniform(-2,2),rng.uniform(-2,2),rng.uniform(-1,2),rng.uniform(.01,2))
        c=clip(a,b)
        if c:cases.append((*c,F(rng.choice(widths))));clipped+=1
    counters={'cases':len(cases),'homogeneousClippedCases':clipped,'tightCases':0,'fallbackCases':0,'acceptedSamplesChecked':0,'helperCentersChecked':0,'oldAreaSum':0.,'tightAreaSum':0.}
    for case_id,(a,b,w) in enumerate(cases):
        tight=candidate(a,b,w)
        if tight is None:counters['fallbackCases']+=1;continue
        q,oldarea,newarea=tight;counters['tightCases']+=1;counters['oldAreaSum']+=oldarea;counters['tightAreaSum']+=newarea
        qa=quantize(a);qb=quantize(b);k=0 if abs(qb[0]-qa[0])>=abs(qb[1]-qa[1]) else 1;j=1-k
        low=math.floor(min(qa[k],qb[k])/256)-2;high=math.ceil(max(qa[k],qb[k])/256)+2
        if high-low<=70:majors=range(low,high+1)
        else:majors=sorted(set([low,low+1,low+2,low+3,high-3,high-2,high-1,high,(low+high)//2]+[rng.randint(low,high) for _ in range(24)]))
        count=max(1,math.floor(add(w,.5)));lo,hi=old_bounds(a,b,w)
        for major in majors:
            dm=qb[k]-qa[k];di=qb[j]-qa[j];num=qa[j]*dm+(major*256+128-qa[k])*di;den=dm*256
            if den<0:num=-num;den=-den
            lower=(2*num-(count-1)*den-1)//(2*den)
            # Extreme accepted minors plus interior and adjacent rejected ones.
            for minor in sorted(set([lower-1,lower,lower+count//2,lower+count-1,lower+count])):
                pixel=(major,minor) if k==0 else (minor,major)
                if not covered(qa,qb,pixel,count):continue
                p=(pixel[0]+.5,pixel[1]+.5)
                assert all(lo[t]<p[t]<hi[t] for t in (0,1)),('old-box',case_id,a,b,w,p)
                assert inside(q,p),('lost-pixel',case_id,a,b,w,p,q)
                counters['acceptedSamplesChecked']+=1
                for ox in [-1,0,1]:
                    for oy in [-1,0,1]:
                        assert inside(q,(p[0]+ox,p[1]+oy)),('lost-helper',case_id,p,ox,oy,q)
                        counters['helperCentersChecked']+=1
    for samples in [0,2,4,8]:assert candidate((0,0),(200,200),5,samples=samples) is None
    for a,b,w,v in [((0,0),(0,0),5,(0,0,100,100)),((math.nan,0),(20,20),5,(0,0,100,100)),((0,0),(200,200),math.inf,(0,0,100,100)),((0,0),(200,200),0,(0,0,100,100)),((0,0),(200,200),65,(0,0,100,100)),((0,0),(20000,20000),5,(0,0,100,100)),((0,0),(200,200),5,(0,0,0,100)),((0,0),(200,200),5,(0,0,20000,100))]:assert candidate(a,b,w,v) is None
    report={'status':'PASS','method':'Exact integer diamond-exit symbolic-epsilon oracle plus independent float32 quad construction; sampled major positions for long lines; endpoint minors and interior checked; all 3x3 centers contain every possible adjacent 2x2 helper. Homogeneous clipping tested. GPU parity remains separate.',**counters,'testSetAreaRatio':counters['tightAreaSum']/counters['oldAreaSum'],'notRuntimeReductionEstimate':True,'scriptSHA256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (ROOT/'validation/CONTAINMENT-RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
