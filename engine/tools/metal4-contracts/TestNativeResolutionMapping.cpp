#include "Metal4GroundDecalDepthMapping.h"
#include "Metal4WeaponRangeViewport.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace rendering::metal4;
int main(){
 struct C{std::array<int32_t,4> view;std::array<uint32_t,2> logical,render;bool legacy;};
 C cases[]={
  {{0,0,1512,950},{1512,982},{1512,950},false},
  {{0,0,1512,950},{1512,982},{1512,982},true},
  {{0,0,1920,1080},{1920,1080},{1920,1080},true},
  {{0,0,1920,1080},{1920,1080},{3840,2160},false},
  {{0,0,1920,1080},{1920,1080},{2560,1440},false},
  {{0,0,2560,1440},{2560,1440},{1920,1080},false},
  {{0,0,1365,767},{1365,768},{2731,1537},false},
  {{300,50,1200,801},{1920,1080},{3840,2160},false},
  {{-100,100,800,600},{1512,982},{2560,1440},false},
  {{910,100,900,900},{1512,982},{1280,720},false},
  {{0,0,800,600},{1600,900},{1600,900},true}
 };
 unsigned checked=0;
 for(auto c:cases){auto w=MapWorldViewport(c.view,c.logical[0],c.logical[1],c.render[0],c.render[1]);assert(w);
  auto k=MatchGroundDecalDepthMapping(c.view,c.logical,c.render,w->scissor);assert(k&&k->legacyExact==c.legacy);
  auto stale=w->scissor;stale[3]--;assert(!MatchGroundDecalDepthMapping(c.view,c.logical,c.render,stale));
  for(unsigned xi:{0u,w->scissor[2]/2,w->scissor[2]-1})for(unsigned yi:{0u,w->scissor[3]/2,w->scissor[3]-1})for(double off:{.125,.25,.5,.75,.875}){
   double px=w->scissor[0]+xi+off,py=w->scissor[1]+yi+off;
   assert(unsigned(px)-w->scissor[0]==xi&&unsigned(py)-w->scissor[1]==yi);
   auto m=k->fragmentToLogical;double lx=px*m[0]+m[2],ly=py*m[1]+m[3];
   double nx=(lx-c.view[0])/c.view[2],ny=(ly-c.view[1])/c.view[3];auto v=w->viewport;
   assert(std::abs(nx-(px-v[0])/v[2])<.000001&&std::abs(ny-(v[1]+v[3]-py)/v[3])<.000001);
   if(c.legacy){assert(unsigned(lx)==xi);assert(w->scissor[3]-1-unsigned(ly)==yi);}
   ++checked;
  }
  auto r=w->scissor;std::cout<<c.logical[0]<<"x"<<c.logical[1]<<" -> "<<c.render[0]<<"x"<<c.render[1]<<" crop="<<r[0]<<","<<r[1]<<","<<r[2]<<","<<r[3]<<" legacy="<<k->legacyExact<<"\n";
 }
 auto a=ClassifyWeaponRangeViewport({-11047.619047619,-1548.513238289,18204.444444444,22523.828920570},false);assert(a.valid&&a.extended&&a.rebaseFill);
 auto b=ClassifyWeaponRangeViewport({-11047.619047619,-1548.513238289,18204.444444444,22523.828920570},true);assert(b.valid&&b.extended&&!b.rebaseFill);
 assert(!ClassifyWeaponRangeViewport({0,0,std::numeric_limits<double>::infinity(),100},false).valid);
 assert(!ClassifyWeaponRangeViewport({524288,0,10,100},false).valid);
 assert(!ClassifyWeaponRangeViewport({0,0,-5,100},true).valid);
 assert(ClassifyWeaponRangeViewport({-524288,-524288,1048576,1048576},true).valid);
 assert(!ClassifyWeaponRangeViewport({0,0,1048576,1048576},true).valid);
 std::cout<<"PASS "<<checked<<" sample-coordinate checks across "<<std::size(cases)<<" arbitrary geometry/resize cases; stale crops rejected; original PiP bounds and safety checked\n";
}
