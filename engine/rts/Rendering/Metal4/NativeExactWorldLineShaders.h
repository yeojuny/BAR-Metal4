/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
inline constexpr const char* ExactWorldLineShader=R"LINE120(
// Diagnostic IEEE754 binary64 RNE arithmetic for Metal line interpolation.
// Host mirror and GPU compile the same integer code. No production integration.
#ifdef __METAL_VERSION__
#include <metal_stdlib>
using namespace metal;
using U64=ulong;using U32=uint;
inline U32 clz64(U64 x){return clz(x);}
inline U32 bits32(float x){return as_type<U32>(x);}
inline float float32(U32 x){return as_type<float>(x);}
#else
#include <cstdint>
#include <bit>
using U64=uint64_t;using U32=uint32_t;
inline U32 clz64(U64 x){return std::countl_zero(x);}
inline U32 bits32(float x){return std::bit_cast<U32>(x);}
inline float float32(U32 x){return std::bit_cast<float>(x);}
#endif
namespace line64 {
enum : U64 { Sign=U64(1)<<63,Frac=(U64(1)<<52)-1,One=U64(1)<<52,Inf=U64(0x7ff)<<52 };
struct Norm {U64 m;int e;};
struct Wide {U64 hi,lo;};
inline U64 jam(U64 x,unsigned n){return !n?x:n<64?(x>>n)|U64((x<<(64-n))!=0):U64(x!=0);}
inline Norm norm(U64 x){int e=int((x>>52)&2047);U64 m=x&Frac;if(e)return {m|One,e-1023};if(!m)return {0,-1022};unsigned n=clz64(m)-11;return {m<<n,-1022-int(n)};}
inline U64 pack(bool sign,int e,U64 sig){
 if(!sig)return sign?Sign:0;
 int shift=55-int(63-clz64(sig));
 if(shift>0)sig<<=shift;else if(shift<0)sig=jam(sig,unsigned(-shift));e-=shift;
 if(e < -1022){sig=jam(sig,unsigned(-1022-e));e=-1022;}
 U64 m=sig>>3,r=sig&7;m+=U64(r>4||(r==4&&(m&1)));
 if(m==(One<<1)){m>>=1;++e;}
 if(e>1023)return (sign?Sign:0)|Inf;
 return (sign?Sign:0)|(m>=One?U64(e+1023)<<52:0)|(m&Frac);
}
inline U64 fromFloat(float f){U32 b=bits32(f),e=(b>>23)&255,m=b&0x7fffff;bool s=(b>>31)!=0;if(e==255)return (s?Sign:0)|Inf|(m?U64(1)<<51:0);return pack(s,e?int(e)-127:-126,U64(m|(e?1u<<23:0))<<32);}
inline U32 toFloatBits(U64 v){
 bool sign=(v&Sign)!=0;U32 e=U32((v>>52)&2047);U64 m=v&Frac;
 if(e==2047)return (sign?0x80000000u:0)|0x7f800000u|(m?0x400000u:0);
 if(!(v&~Sign))return sign?0x80000000u:0;
 auto n=norm(v);U64 sig=jam(n.m,26);int exp=n.e;
 if(exp< -126){sig=jam(sig,unsigned(-126-exp));exp=-126;}
 U32 mant=U32(sig>>3),r=U32(sig&7);mant+=U32(r>4||(r==4&&(mant&1)));
 if(mant==(1u<<24)){mant>>=1;++exp;}
 if(exp>127)return (sign?0x80000000u:0)|0x7f800000u;
 return (sign?0x80000000u:0)|(mant>=(1u<<23)?U32(exp+127)<<23:0)|(mant&0x7fffffu);
}
inline U64 add(U64 a,U64 b){
 const U64 aa=a&~Sign,bb=b&~Sign;
 if(aa>=Inf||bb>=Inf){if(aa>Inf||bb>Inf||(aa==Inf&&bb==Inf&&((a^b)&Sign)))return Inf|(U64(1)<<51);return aa==Inf?a:b;}
 if(!aa&&!bb)return a&b&Sign;
 if(!aa)return b;if(!bb)return a;
 if(aa<bb){U64 t=a;a=b;b=t;}
 auto x=norm(a),y=norm(b);U64 large=x.m<<3,small=jam(y.m<<3,unsigned(x.e-y.e));
 bool sign=(a&Sign)!=0;U64 sig;
 if((a^b)&Sign){sig=large-small;if(!sig)return 0;}else sig=large+small;
 return pack(sign,x.e,sig);
}
inline U64 sub(U64 a,U64 b){return add(a,b^Sign);}
inline Wide product(U64 a,U64 b){
 const U64 mask=0xffffffff;U64 a0=a&mask,a1=a>>32,b0=b&mask,b1=b>>32;
 U64 p=a0*b0,t=a1*b0+(p>>32),u=a0*b1+(t&mask);
 return {a1*b1+(t>>32)+(u>>32),(u<<32)|(p&mask)};
}
inline U64 mul(U64 a,U64 b){
 bool sign=((a^b)&Sign)!=0;U64 aa=a&~Sign,bb=b&~Sign;
 if(aa>=Inf||bb>=Inf){if(aa>Inf||bb>Inf||!aa||!bb)return Inf|(U64(1)<<51);return (sign?Sign:0)|Inf;}
 if(!aa||!bb)return sign?Sign:0;
 auto x=norm(a),y=norm(b);auto p=product(x.m,y.m);
 U64 sig=(p.hi<<15)|(p.lo>>49)|U64((p.lo&((U64(1)<<49)-1))!=0);
 return pack(sign,x.e+y.e,sig);
}
inline bool gt(Wide a,Wide b){return a.hi>b.hi||(a.hi==b.hi&&a.lo>b.lo);}
inline Wide minus(Wide a,Wide b){return {a.hi-b.hi-U64(a.lo<b.lo),a.lo-b.lo};}
inline U32 quotientChunk(U64 remainder,U64 denominator,unsigned bits){
 Wide n{remainder>>(64-bits),remainder<<bits};
 // A binary32 estimate supplies at most24 quotient bits. Exact limb
 // comparisons repair its few-ULP error; approximation never decides output.
 U32 q=U32((float(remainder)/float(denominator))*float(U32(1)<<bits));
 Wide p=product(denominator,q);
 while(gt(p,n)){--q;p=minus(p,{0,denominator});}
 Wide rest=minus(n,p);
 while(rest.hi||rest.lo>=denominator){++q;rest=minus(rest,{0,denominator});}
 return q;
}
inline U64 div(U64 a,U64 b){
 bool sign=((a^b)&Sign)!=0;U64 aa=a&~Sign,bb=b&~Sign;
 if(aa>=Inf||bb>=Inf){if(aa>Inf||bb>Inf||(aa==Inf&&bb==Inf))return Inf|(U64(1)<<51);return aa==Inf?((sign?Sign:0)|Inf):(sign?Sign:0);}
 if(!bb)return aa?((sign?Sign:0)|Inf):(Inf|(U64(1)<<51));if(!aa)return sign?Sign:0;
 auto x=norm(a),y=norm(b);U64 rem=x.m;U64 q=U64(rem>=y.m);if(q)rem-=y.m;
 for(unsigned n=0;n<3;++n){unsigned bits=n<2?24:7;U32 digit=quotientChunk(rem,y.m,bits);q=(q<<bits)|digit;Wide num{rem>>(64-bits),rem<<bits};rem=minus(num,product(y.m,digit)).lo;}
 q|=U64(rem!=0);return pack(sign,x.e-y.e,q);
}
} // namespace line64

// Diagnostic compact world-line packet. No production ABI or path change.

#ifdef __METAL_VERSION__
using I32=int;
#else
#include <cstdint>
using I32=int32_t;
#endif
struct Line120Vertex {float position[4],color[4],uv[4];};
struct Line120Segment {
 Line120Vertex a,b;
 // Raster endpoints and tag after the second, screen-space clip.
 float sa[4],sb[4];
 U64 viewport[4],nx,ny,counter;
 I32 snapped[4],majorFirst,unitFirst,direction;
 U32 width,height,count,flags,pattern,factor;
};
// One span for the whole aliased segment, one for each MSAA stipple interval.
struct Line120Span {U32 first,count,segment,reserved;U64 t0,t1;};
struct Line120Output {Line120Vertex v;U32 valid,reserved[3];};
struct Line120Quad {Line120Vertex v[4];U32 valid,reserved[3];};

// The exact same diagnostic evaluator is compiled for host and Metal.
struct D120 {
 U64 b;
 static D120 bits(U64 b){return {b};}
 static D120 f(float x){return {line64::fromFloat(x)};}
 float f()const{return float32(line64::toFloatBits(b));}
};
inline D120 operator+(D120 a,D120 b){return D120::bits(line64::add(a.b,b.b));}
inline D120 operator-(D120 a,D120 b){return D120::bits(line64::sub(a.b,b.b));}
inline D120 operator*(D120 a,D120 b){return D120::bits(line64::mul(a.b,b.b));}
inline D120 operator/(D120 a,D120 b){return D120::bits(line64::div(a.b,b.b));}
inline int floor120(D120 d){
 auto n=line64::norm(d.b);if(!n.m)return 0;
 bool neg=(d.b&line64::Sign)!=0;
 if(n.e<0)return neg?-1:0;
 // All integer conversions here are clipped raster coordinates, <= 32768.
 U64 whole=n.m>>(52-n.e),rem=n.m&((U64(1)<<(52-n.e))-1);
 return neg?-int(whole)-int(rem!=0):int(whole);
}
inline float mix120(float a,float b,D120 t){return (D120::f(a)+(D120::f(b)-D120::f(a))*t).f();}
inline float gradient120(float a,float b,D120 t){return (D120::f(a)+D120::f((D120::f(b)-D120::f(a)).f())*t).f();}
inline Line120Output evaluate120(Line120Segment s,Line120Span span,U32 localQuad,U32 corner){
 Line120Output out={};
 const auto one=D120::f(1),two=D120::f(2),halfValue=D120::f(.5f);
 D120 vp[4];for(unsigned i=0;i<4;++i)vp[i]=D120::bits(s.viewport[i]);
 const auto bottom=D120::f(float(s.height))-vp[1]-vp[3];
 D120 t,x,y,z,w=one;
 if(s.flags&1){
  t=D120::bits(corner<2?span.t0:span.t1);
  auto side=D120::f((corner==0||corner==3)?1.f:-1.f);
  x=D120::f(s.sa[0])+(D120::f(s.sb[0])-D120::f(s.sa[0]))*t+D120::bits(s.nx)*side;
  y=D120::f(s.sa[1])+(D120::f(s.sb[1])-D120::f(s.sa[1]))*t+D120::bits(s.ny)*side;
  z=D120::f(s.sa[2])+(D120::f(s.sb[2])-D120::f(s.sa[2]))*t;
 }else{
  const bool xm=(s.flags&2)!=0,stipple=(s.flags&4)!=0,edge=(s.flags&8)!=0;
  int major=s.majorFirst+int(localQuad/s.count)*s.direction;
  U64 ordinal=U64(max(0,(major-s.unitFirst)*s.direction));
  if(stipple&&!(s.pattern>>(((s.counter+ordinal)/s.factor)%16)&1))return out;
  auto ax=D120::f(float(s.snapped[0]))/D120::f(256),ay=D120::f(float(s.snapped[1]))/D120::f(256);
  auto bx=D120::f(float(s.snapped[2]))/D120::f(256),by=D120::f(float(s.snapped[3]))/D120::f(256);
  auto ta=(D120::f(float(major))+halfValue-(xm?ax:ay))/((xm?bx:by)-(xm?ax:ay));
  auto minor=(xm?ay:ax)+ta*(xm?by-ay:bx-ax);
  auto shift=D120::f(-float(s.count-1))*halfValue;
  // Exact original double constants, not rounded binary32 substitutes.
  auto eps=D120::bits((stipple||edge)?U64(0x3e112e0be826d695):U64(0xbe112e0be826d695));
  int lower=floor120(minor+shift+eps),replica=int(localQuad%s.count);
  int px=xm?major:lower+replica,py=xm?lower+replica:major;
  if(px<0||py<0||px>=int(s.width)||py>=int(s.height))return out;
  auto tr=(D120::f(float(xm?px:py))+halfValue-D120::f(s.sa[xm?0:1]))/(D120::f(s.sb[xm?0:1])-D120::f(s.sa[xm?0:1]));
  float depth=gradient120(s.sa[2],s.sb[2],tr);if(depth< -1||depth>1)return out;
  z=D120::f(depth);
  t=D120::f(gradient120(s.sa[3],s.sb[3],tr));
  x=D120::f(float(px+int(corner==1||corner==2)));y=D120::f(float(py+int(corner>=2)));
 }
 auto inverse=(one-t)/D120::f(s.a.position[3])+t/D120::f(s.b.position[3]);
 auto perspective=(t/D120::f(s.b.position[3]))/inverse;
 for(unsigned i=0;i<4;++i){out.v.color[i]=mix120(s.a.color[i],s.b.color[i],perspective);out.v.uv[i]=mix120(s.a.uv[i],s.b.uv[i],perspective);}
 if(s.flags&1)w=one/inverse;
 out.v.position[0]=(((x-vp[0])*two/vp[2]-one)*w).f();
 out.v.position[1]=(((y-bottom)*two/vp[3]-one)*w).f();
 out.v.position[2]=(z*w).f();out.v.position[3]=w.f();
 if(s.flags&1){
  // Match the intermediate binary32 round before full attachment remapping.
  auto pw=D120::f(out.v.position[3]);
  out.v.position[0]=((vp[0]*two/D120::f(float(s.width))-one)*pw+(D120::f(out.v.position[0])+pw)*vp[2]/D120::f(float(s.width))).f();
  out.v.position[1]=((bottom*two/D120::f(float(s.height))-one)*pw+(D120::f(out.v.position[1])+pw)*vp[3]/D120::f(float(s.height))).f();
 }
 out.valid=1;return out;
}

// Quad-cooperative numeric prototype: common interpolation once per aliased
// pixel and twice per multisample interval; exact output/rounding is unchanged.
inline Line120Vertex sample120(Line120Segment s,D120 t){
 Line120Vertex out={};auto one=D120::f(1);
 auto inverse=(one-t)/D120::f(s.a.position[3])+t/D120::f(s.b.position[3]);
 auto perspective=(t/D120::f(s.b.position[3]))/inverse;
 for(unsigned i=0;i<4;++i){out.color[i]=mix120(s.a.color[i],s.b.color[i],perspective);out.uv[i]=mix120(s.a.uv[i],s.b.uv[i],perspective);}
 return out;
}
inline Line120Quad quad120(Line120Segment s,Line120Span span,U32 localQuad){
 Line120Quad out={};const auto one=D120::f(1),two=D120::f(2),halfValue=D120::f(.5f);
 D120 vp[4];for(unsigned i=0;i<4;++i)vp[i]=D120::bits(s.viewport[i]);auto bottom=D120::f(float(s.height))-vp[1]-vp[3];
 if(s.flags&1){
  for(unsigned end=0;end<2;++end){
   auto t=D120::bits(end?span.t1:span.t0);
   auto inverse=(one-t)/D120::f(s.a.position[3])+t/D120::f(s.b.position[3]);auto w=one/inverse;
   auto base=sample120(s,t);auto bx=D120::f(s.sa[0])+(D120::f(s.sb[0])-D120::f(s.sa[0]))*t,by=D120::f(s.sa[1])+(D120::f(s.sb[1])-D120::f(s.sa[1]))*t;
   base.position[2]=((D120::f(s.sa[2])+(D120::f(s.sb[2])-D120::f(s.sa[2]))*t)*w).f();base.position[3]=w.f();
   for(unsigned n=0;n<2;++n){unsigned corner=end?3-n:n;auto side=D120::f(n?-1.f:1.f);auto x=bx+D120::bits(s.nx)*side,y=by+D120::bits(s.ny)*side;
    auto v=base;v.position[0]=(((x-vp[0])*two/vp[2]-one)*w).f();v.position[1]=(((y-bottom)*two/vp[3]-one)*w).f();auto pw=D120::f(v.position[3]);
    v.position[0]=((vp[0]*two/D120::f(float(s.width))-one)*pw+(D120::f(v.position[0])+pw)*vp[2]/D120::f(float(s.width))).f();
    v.position[1]=((bottom*two/D120::f(float(s.height))-one)*pw+(D120::f(v.position[1])+pw)*vp[3]/D120::f(float(s.height))).f();out.v[corner]=v;
   }
  }
 }else{
  bool xm=(s.flags&2)!=0,stipple=(s.flags&4)!=0,edge=(s.flags&8)!=0;int major=s.majorFirst+int(localQuad/s.count)*s.direction;
  U64 ordinal=U64(max(0,(major-s.unitFirst)*s.direction));if(stipple&&!(s.pattern>>(((s.counter+ordinal)/s.factor)%16)&1))return out;
  auto ax=D120::f(float(s.snapped[0]))/D120::f(256),ay=D120::f(float(s.snapped[1]))/D120::f(256),bx=D120::f(float(s.snapped[2]))/D120::f(256),by=D120::f(float(s.snapped[3]))/D120::f(256);
  auto ta=(D120::f(float(major))+halfValue-(xm?ax:ay))/((xm?bx:by)-(xm?ax:ay));auto minor=(xm?ay:ax)+ta*(xm?by-ay:bx-ax),shift=D120::f(-float(s.count-1))*halfValue;
  auto eps=D120::bits((stipple||edge)?U64(0x3e112e0be826d695):U64(0xbe112e0be826d695));int lower=floor120(minor+shift+eps),replica=int(localQuad%s.count);int px=xm?major:lower+replica,py=xm?lower+replica:major;
  if(px<0||py<0||px>=int(s.width)||py>=int(s.height))return out;
  auto tr=(D120::f(float(xm?px:py))+halfValue-D120::f(s.sa[xm?0:1]))/(D120::f(s.sb[xm?0:1])-D120::f(s.sa[xm?0:1]));float depth=gradient120(s.sa[2],s.sb[2],tr);if(depth< -1||depth>1)return out;
  auto t=D120::f(gradient120(s.sa[3],s.sb[3],tr));auto base=sample120(s,t);base.position[2]=depth;base.position[3]=1;
  float xs[2],ys[2];for(unsigned i=0;i<2;++i){xs[i]=((D120::f(float(px+i))-vp[0])*two/vp[2]-one).f();ys[i]=((D120::f(float(py+i))-bottom)*two/vp[3]-one).f();}
  for(unsigned i=0;i<4;++i){out.v[i]=base;out.v[i].position[0]=xs[i==1||i==2];out.v[i].position[1]=ys[i>=2];}
 }
 out.valid=1;return out;
}

kernel void ExactWorldLineGeometry(device const Line120Segment* segments [[buffer(0)]],device const Line120Span* spans [[buffer(1)]],device Line120Vertex* vertices [[buffer(2)]],constant uint& spanCount [[buffer(3)]],device uint* indices [[buffer(4)]],uint id [[thread_position_in_grid]]){
 uint lo=0,hi=spanCount;while(lo+1<hi){uint mid=(lo+hi)/2;if(spans[mid].first<=id)lo=mid;else hi=mid;}auto span=spans[lo];auto q=quad120(segments[span.segment],span,id-span.first);
 for(uint i=0;i<4;++i){if(!q.valid){q.v[i].position[0]=q.v[i].position[1]=q.v[i].position[2]=0;q.v[i].position[3]=1;}vertices[id*4+i]=q.v[i];}
 uint corners[6]={0,1,2,0,2,3};for(uint i=0;i<6;++i)indices[id*6+i]=id*4+corners[i];
}

)LINE120";
}
