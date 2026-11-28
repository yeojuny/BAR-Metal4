R=random.Random(3); C=dict(cases=0,guarded=0,fallback=0,early_reject=0,covered=0,diamonds_old=0,diamonds_new=0)
def ck(x):
 assert -(1<<63)<x<(1<<63); return x
def cmp(a,b):
 for i in [0,2,3]:
  v=ck(ck(a[i]*b[1])-ck(b[i]*a[1]))
  if v:return 1 if v>0 else -1
 return 0
def frac(n,d,e,f):return (n,d,e,f) if d>=0 else (-n,-d,-e,-f)
def diamond(a,b,p):
 st=(a[0]+a[1],a[0]-a[1]);de=(b[0]+b[1]-st[0],b[0]-b[1]-st[1]);ce=((p[0]+p[1]+1)*256,(p[0]-p[1])*256)
 enter=(0,1,0,0);leave=(1,1,0,0)
 for axis in [0,1]:
  lo,hi=ce[axis]-128,ce[axis]+128
  if de[axis]==0:
   if st[axis]<=lo or st[axis]>hi:return False
   continue
  l=frac(lo-st[axis],de[axis],1,1 if axis==0 else -1);h=frac(hi-st[axis],de[axis],1,1 if axis==0 else -1)
  if cmp(l,h)>0:l,h=h,l
  if cmp(l,enter)>0:enter=l
  if cmp(h,leave)<0:leave=h
 return cmp(leave,enter)>0 and cmp(leave,(0,1,0,0))>=0 and cmp(enter,(1,1,0,0))<=0 and cmp(leave,(1,1,0,0))<0
def guard(a,b,p,w,s):return s==1 and all(math.isfinite(x) for x in (*a,*b,*p,w)) and 0<w<=64 and max(map(abs,(*a,*b)))<=16384 and max(map(abs,p))<=32768
def quant(a):return tuple(math.floor(float(np.float32(np.float32(x)*np.float32(256)+np.float32(.5)))) for x in a)
def test(a,b,p,w,s=1):
 a,b,p=tuple(map(lambda x:float(np.float32(x)),a)),tuple(map(lambda x:float(np.float32(x)),b)),tuple(map(lambda x:float(np.float32(x)),p));w=float(np.float32(w));g=guard(a,b,p,w,s)
 C['cases']+=1;C['guarded' if g else 'fallback']+=1
 aa,bb=quant(a),quant(b);d=tuple(y-x for x,y in zip(aa,bb))
 if d==(0,0):return
 x=abs(d[0])>=abs(d[1]);j=0 if x else 1;k=1-j;pixel=tuple(map(math.floor,p));major=pixel[j];mi=aa[k];ma=aa[j];dm=d[j];di=d[k]
 n=ck(ck(mi*dm)+ck((major*256+128-ma)*di));den=ck(dm*256)
 if den<0:n,den=-n,-den
 if n<0:ck(-n+den-1)
 center=n//den;count=max(1,math.floor(float(np.float32(w+np.float32(.5)))))
 lower=ck(ck(2*n)-ck((count-1)*den)-1)//ck(2*den);actual=pixel[k];inside=lower<=actual<lower+count
 assert -(1<<31)<lower<=(1<<31)-1 and -(1<<31)<lower+count<(1<<31)
 def cover():
  calls=0
  for minor in range(center-1,center+2):
   calls+=1
   if diamond(aa,bb,(major,minor) if x else (minor,major)):return True,calls
  return False,calls
 covered,calls=cover();old_result=covered and inside
 if g and not inside:new_result=False;newcalls=0;C['early_reject']+=1
 else:new_result=covered and inside;newcalls=calls
 assert old_result==new_result
 C['covered']+=int(old_result);C['diamonds_old']+=calls;C['diamonds_new']+=newcalls
 # Depth executes iff both coverage predicates pass; expression is byte-identical.
 if g:
  assert abs(n)<2**48 and 0<den<=2**31
  assert abs(2*n-(count-1)*den-1)<2**49
# Endpoints, reversal, zero/subpixel/ties, negative coordinates and width rounding.
lines=[((0,0),(9,3)),((0,0),(3,9)),((-9,-3),(9,3)),((0,0),(8,8)),((0,0),(0,0)),((0,0),(.001,.001)),((0,0),(1/256,0)),((16384,16384),(-16384,-16384)),((-16384,16384),(16384,-16384)),((16384,-16384),(16384,16384)),((16385,0),(16380,8))]
widths=[-.5,0,.49,.5,1,1.4999,1.5,2.5,63.4999,63.5,64,64.001,65]
for a,b in lines:
 for a,b in [(a,b),(b,a)]:
  for w in widths:
   for sample in [1,4]:
    for t in [-.1,0,.5,1,1.1]:
     mid=tuple(a[i]+t*(b[i]-a[i]) for i in [0,1])
     for shift in [-33,-1,0,1,33]:test(a,b,(mid[0]+shift,mid[1]+shift),w,sample)
for _ in range(18000):
 a=tuple(R.uniform(-16384,16384) for _ in [0,1]);b=tuple(R.uniform(-16384,16384) for _ in [0,1]);t=R.uniform(-.01,1.01)
 p=tuple(a[i]+t*(b[i]-a[i])+R.uniform(-40,40) for i in [0,1]);test(a,b,p,R.choice(widths))
for p in [(-32768,-32768),(32768,32768),(32768.5,0),(-32768.5,0)]:
 for a,b in lines[:10]:test(a,b,p,64)
# Explicit adjacent representable float32 values at guard/rounding boundaries.
def neighbors(v):
 v=np.float32(v);return [float(np.nextafter(v,np.float32(-np.inf))),float(v),float(np.nextafter(v,np.float32(np.inf)))]
for boundary in [-16384,16384]:
 for v in neighbors(boundary):
  test((v,0),(v-8,5),(v-4,2.5),1)
  test((0,v),(5,v-8),(2.5,v-4),1)
for boundary in [-32768,32768]:
 for v in neighbors(boundary):test((-16384,-16384),(16384,16384),(v,v),64)
for boundary in [0,.5,1.5,2.5,63.5,64]:
 for v in neighbors(boundary):test((-8,-5),(8,5),(0.5,0.5),v)
for boundary in [-2-.5/256,-.5/256,.5/256,1+.5/256,16383+.5/256]:
 for v in neighbors(boundary):
  test((v,-5),(v+8,5),(v+3,.5),1)
  test((v+8,5),(v,-5),(v+3,.5),1)
for v in [float('inf'),float('nan')]:
 assert not guard((0,0),(v,1),(0,0),1,1)
 assert not guard((0,0),(1,1),(v,0),1,1)
for w in [float('inf'),float('nan')]:assert not guard((0,0),(1,1),(0,0),w,1)
for v in [float('inf'),float('nan')]:assert not guard((v,0),(1,1),(0,0),1,1)
assert C['early_reject'] and C['covered'] and C['fallback']
print(json.dumps(dict(status='PASS',scope='CPU exact coverage/order identity and bounded integer arithmetic; no MSL/GPU/performance proof',counts=C),indent=2))
