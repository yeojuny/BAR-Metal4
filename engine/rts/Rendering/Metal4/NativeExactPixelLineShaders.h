/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
inline constexpr char ExactPixelLineShader[]=R"MSL(
inline Line120Quad pixelQuad122(Line120Segment s,Line120Span span,U32 localQuad){
 Line120Quad out={};const auto one=D120::f(1),two=D120::f(2),halfValue=D120::f(.5f);

 if(s.flags&1){
  for(unsigned end=0;end<2;++end){
   auto t=D120::bits(end?span.t1:span.t0);Line120Vertex base={};base.position[2]=mix120(s.sa[2],s.sb[2],t);base.position[3]=1;
   for(unsigned c=0;c<4;++c)base.color[c]=mix120(s.a.color[c],s.b.color[c],t);
   auto bx=D120::f(s.sa[0])+(D120::f(s.sb[0])-D120::f(s.sa[0]))*t,by=D120::f(s.sa[1])+(D120::f(s.sb[1])-D120::f(s.sa[1]))*t;
   for(unsigned n=0;n<2;++n){unsigned corner=end?3-n:n;auto side=D120::f(n?-1.f:1.f);auto v=base;v.position[0]=(bx+D120::bits(s.nx)*side).f();v.position[1]=(by+D120::bits(s.ny)*side).f();out.v[corner]=v;}
  }
 }else{
  bool xm=(s.flags&2)!=0,stipple=(s.flags&4)!=0,edge=(s.flags&8)!=0;int major=s.majorFirst+int(localQuad/s.count)*s.direction;
  U64 ordinal=U64(max(0,(major-s.unitFirst)*s.direction));if(stipple&&!(s.pattern>>(((s.counter+ordinal)/s.factor)%16)&1))return out;
  auto ax=D120::f(float(s.snapped[0]))/D120::f(256),ay=D120::f(float(s.snapped[1]))/D120::f(256),bx=D120::f(float(s.snapped[2]))/D120::f(256),by=D120::f(float(s.snapped[3]))/D120::f(256);
  auto ta=(D120::f(float(major))+halfValue-(xm?ax:ay))/((xm?bx:by)-(xm?ax:ay));auto minor=(xm?ay:ax)+ta*(xm?by-ay:bx-ax),shift=D120::f(-float(s.count-1))*halfValue;
  auto eps=D120::bits((stipple||edge)?U64(0x3e112e0be826d695):U64(0xbe112e0be826d695));int lower=floor120(minor+shift+eps),replica=int(localQuad%s.count);int px=xm?major:lower+replica,py=xm?lower+replica:major;
  if(px<0||py<0||px>=int(s.width)||py>=int(s.height))return out;
  auto tr=(D120::f(float(xm?px:py))+halfValue-D120::f(s.sa[xm?0:1]))/(D120::f(s.sb[xm?0:1])-D120::f(s.sa[xm?0:1]));float depth=gradient120(s.sa[2],s.sb[2],tr);if(depth< -1||depth>1)return out;
  Line120Vertex base={};base.position[2]=depth;base.position[3]=1;for(unsigned c=0;c<4;++c)base.color[c]=gradient120(s.a.color[c],s.b.color[c],tr);
  float xs[2],ys[2];for(unsigned i=0;i<2;++i){xs[i]=float(px+i);ys[i]=float(py+i);}
  for(unsigned i=0;i<4;++i){out.v[i]=base;out.v[i].position[0]=xs[i==1||i==2];out.v[i].position[1]=ys[i>=2];}
 }
 out.valid=1;return out;
}

kernel void ExactPixelLineGeometry(device const Line120Segment* segments [[buffer(0)]],device const Line120Span* spans [[buffer(1)]],device Line120Vertex* vertices [[buffer(2)]],constant uint& spanCount [[buffer(3)]],device uint* indices [[buffer(4)]],uint id [[thread_position_in_grid]]){
 uint lo=0,hi=spanCount;while(lo+1<hi){uint mid=(lo+hi)/2;if(spans[mid].first<=id)lo=mid;else hi=mid;}
 auto span=spans[lo];auto q=pixelQuad122(segments[span.segment],span,id-span.first);
 for(uint i=0;i<4;++i){auto v=q.v[i];if(!q.valid)v.position[3]=1;vertices[id*4+i]=v;}
 const uint corners[6]={0,1,2,0,2,3};for(uint i=0;i<6;++i)indices[id*6+i]=id*4+corners[i];
}
)MSL";
} // namespace rendering::metal4
