/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#if defined(METAL4_LOGICAL_COPY_MAPPING_TEST_MAIN)
#include "Metal4LogicalCopyMapping.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

using namespace rendering::metal4;
static unsigned checks=0;
static void Check(bool ok, const char* message) { ++checks; if(!ok){std::cerr << message << '\n'; std::exit(1);} }
template<class F> static void Reject(F fn) { bool rejected=false;try{fn();}catch(const std::runtime_error&){rejected=true;}Check(rejected,"Invalid logical copy was accepted"); }
static LogicalCopyMapping Mapping(std::array<int32_t,4> camera,uint32_t lw,uint32_t lh,uint32_t rw,uint32_t rh,unsigned samples=1,uint64_t generation=17) {
 const auto mapped=MapWorldViewport(camera,lw,lh,rw,rh);Check(bool(mapped),"Test unexpectedly offscreen");
 return MakeLogicalCopyMapping(camera,lw,lh,rw,rh,mapped->scissor,91,generation,samples);
}
static uint32_t X(const LogicalCopyMapping& m,unsigned x){return m.unitScale ? m.crop[0]+x : m.coordinates.at(1+x);}
static uint32_t Y(const LogicalCopyMapping& m,unsigned y){return m.unitScale ? m.crop[1]+y : m.coordinates.at(1+m.width+y);}
static void Reference(std::array<int32_t,4> camera,uint32_t lw,uint32_t lh,uint32_t rw,uint32_t rh,unsigned samples) {
 auto m=Mapping(camera,lw,lh,rw,rh,samples);Check(m.width==unsigned(camera[2])&&m.height==unsigned(camera[3]),"Logical allocation changed");
 Check(m.unitScale ? m.coordinates.empty() : (m.coordinates.size()==1+m.width+m.height&&m.coordinates[0]==m.width),"GPU map ABI or constant-size unit-scale path differs");
 Check(m.unitScale==(lw==rw&&lh==rh),"Unit-scale flag differs");
 m.RequireSource(91,17,rw,rh,samples);
 for(unsigned x=0;x<m.width;++x){
  // Independent long-double oracle for the exact rational CPU expression.
  const auto expected=unsigned(std::floor((static_cast<long double>(camera[0])+x+0.5L)*rw/lw));
  Check(X(m,x)==expected,"Wrong logical X pixel center");Check(X(m,x)>=m.crop[0]&&X(m,x)<m.crop[0]+m.crop[2],"X escaped crop");
 }
 for(unsigned y=0;y<m.height;++y){
  const auto expected=rh-1-unsigned(std::floor((static_cast<long double>(camera[1]+camera[3]-1-y)+0.5L)*rh/lh));
  Check(Y(m,y)==expected,"Wrong logical Y pixel center");Check(Y(m,y)>=m.crop[1]&&Y(m,y)<m.crop[1]+m.crop[3],"Y escaped crop");
 }
}
int main(){
 // Identity copies must preserve every source texel and the top-down offset.
 for(auto camera:{std::array<int32_t,4>{0,0,17,11},std::array<int32_t,4>{3,2,9,5}}){
  auto m=Mapping(camera,17,11,17,11);for(unsigned y=0;y<m.height;++y)for(unsigned x=0;x<m.width;++x){
   Check(X(m,x)==unsigned(camera[0])+x,"1:1 original X copy changed");
   Check(Y(m,y)==11-unsigned(camera[1]+camera[3])+y,"1:1 original Y copy changed");
  }
 }
 for(unsigned samples:{1u,2u,4u})for(unsigned scale:{1u,2u,4u}){
  Reference({3,2,9,5},17,11,17*scale,11*scale,samples);
  auto m=Mapping({3,2,9,5},17,11,17*scale,11*scale,samples);
  for(unsigned x=0;x<m.width;++x)Check(X(m,x)==(3+x)*scale+scale/2,"Integral scale center differs");
  for(unsigned y=0;y<m.height;++y)Check(Y(m,y)==(4+y)*scale+(scale-1)/2,"Bottom-left nearest Y tie policy differs");
 }
 // Deliberately asymmetric dimensions and an offset fractional crop expose
 // Y inversion, crop stretching, half-texel omission and origin loss.
 Reference({2,1,7,4},13,9,19,14,1);
 auto fractional=Mapping({2,1,7,4},13,9,19,14);
 const unsigned expectedX[]{3,5,6,8,9,10,12};
 const unsigned expectedY[]{6,8,10,11};
 for(unsigned x=0;x<7;++x)Check(X(fractional,x)==expectedX[x],"Fractional X fixture differs");
 for(unsigned y=0;y<4;++y)Check(Y(fractional,y)==expectedY[y],"Fractional Y fixture differs");
 bool distinguishesRoundedStretch=false;
 for(unsigned x=0;x<7;++x){
  const auto rounded=fractional.crop[0]+((2*x+1)*fractional.crop[2])/(2*fractional.width);
  distinguishesRoundedStretch|=X(fractional,x)!=rounded;
 }
 Check(distinguishesRoundedStretch,"Fixture cannot detect incorrect rounded-scissor stretching");
 // Downsampled physical output can repeat logical samples, but cannot escape.
 Reference({1,2,9,7},13,11,5,4,4);
 Reference({0,0,1,1},1,1,1,1,1);
 Reference({0,0,16384,16384},16384,16384,16384,16384,4);
 // Enumerate contained fractional resize/camera combinations.
 for(unsigned lw=3;lw<=25;lw+=2)for(unsigned rw=2;rw<=31;rw+=3)
  for(unsigned offset=0;offset<lw-1;++offset)Reference({int(offset),1,int(lw-offset-1),5},lw,7,rw,11,2);
 auto a=Mapping({2,1,7,4},13,9,19,14,4);
 Reject([&]{a.RequireSource(92,17,19,14,4);});
 Reject([&]{a.RequireSource(91,18,19,14,4);});
 Reject([&]{a.RequireSource(91,17,20,14,4);});
 Reject([&]{a.RequireSource(91,17,19,15,4);});
 Reject([&]{a.RequireSource(91,17,19,14,2);});
 auto resized=Mapping({2,1,7,4},13,9,21,15,4,18);
 Check(a.coordinates!=resized.coordinates,"Resize reused old pixel mapping");
 Reject([&]{resized.RequireSource(91,17,19,14,4);});
 // Relative crop coordinates must select the same immutable source pixels.
 for(unsigned y=0;y<a.height;++y)for(unsigned x=0;x<a.width;++x){
  auto relativeX=X(a,x)-a.crop[0],relativeY=Y(a,y)-a.crop[1];
  Check(relativeX<a.crop[2]&&relativeY<a.crop[3],"Relative terrain coordinate out of range");
  Check((relativeY+a.crop[1])*19+relativeX+a.crop[0]==Y(a,y)*19+X(a,x),"Relative terrain copy changed source texel");
 }
 auto invalid=[&](std::array<int32_t,4> camera,unsigned lw,unsigned lh,unsigned rw,unsigned rh,std::array<uint32_t,4> crop,uint64_t generation=17,unsigned samples=1){Reject([&]{MakeLogicalCopyMapping(camera,lw,lh,rw,rh,crop,91,generation,samples);});};
 invalid({0,0,0,4},13,9,19,14,a.crop);invalid({0,0,7,-1},13,9,19,14,a.crop);
 invalid({-1,0,7,4},13,9,19,14,a.crop);invalid({0,-1,7,4},13,9,19,14,a.crop);
 invalid({8,0,7,4},13,9,19,14,a.crop);invalid({0,7,7,4},13,9,19,14,a.crop);
 invalid({2,1,7,4},0,9,19,14,a.crop);invalid({2,1,7,4},13,9,0,14,a.crop);
 invalid({2,1,7,4},16385,9,19,14,a.crop);invalid({2,1,7,4},13,9,16385,14,a.crop);
 invalid({2,1,7,4},13,9,19,14,{0,0,0,0});invalid({2,1,7,4},13,9,19,14,{19,0,1,1});
 invalid({2,1,7,4},13,9,19,14,{0,0,19,14}); // valid bounds, wrong camera crop
 invalid({2,1,7,4},13,9,19,14,a.crop,0);invalid({2,1,7,4},13,9,19,14,a.crop,17,0);
 std::cout << "{\"checks\":" << checks << ",\"status\":\"pass\"}\n";
}

#endif
