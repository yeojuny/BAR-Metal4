/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Metal4/Metal4GroundDecalIndexPolicyG01.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <set>
#include <string>
#include <vector>

using namespace rendering::metal4;
using Corner=std::array<float,3>;
static std::size_t Rotate(std::size_t id) {return (id/3)*3+(id%3+2)%3;}
static bool SameOrderedCorners(const std::vector<Corner>& cube,const std::array<std::uint16_t,36>& indices)
{
 for(std::size_t i=0;i<indices.size();++i){
  if(indices[i]>=cube.size()||cube[Rotate(i)]!=cube[Rotate(indices[i])])return false;
 }
 return true;
}
int main(int argc,char** argv)
{
 assert(argc==2);
 std::ifstream in(argv[1]);assert(in.good());
 const std::string shader((std::istreambuf_iterator<char>(in)),{});
 const auto begin=shader.find("constant float3 CUBE_VERT[36] = {");assert(begin!=std::string::npos);
 const auto end=shader.find("};",begin);assert(end!=std::string::npos);
 const auto body=shader.substr(begin,end-begin);
 const std::regex point(R"(float3\(\s*([-0-9.]+),\s*([-0-9.]+),\s*([-0-9.]+)\))");
 std::vector<Corner> cube;
 for(auto it=std::sregex_iterator(body.begin(),body.end(),point);it!=std::sregex_iterator();++it)
  cube.push_back({std::stof((*it)[1]),std::stof((*it)[2]),std::stof((*it)[3])});
 assert(cube.size()==36);
 assert(shader.find("p.Run((vid/3)*3+(vid%3+2)%3);")!=std::string::npos);
 const auto runBegin=shader.find("void Run(uint sourceVertex)");assert(runBegin!=std::string::npos);
 const auto runEnd=shader.find("\n};",runBegin);assert(runEnd!=std::string::npos);
 const auto run=shader.substr(runBegin,runEnd-runBegin);
 const auto read=run.find("CUBE_VERT[sourceVertex]");assert(read!=std::string::npos);
 // The semantic proof requires no other sourceVertex-dependent expression.
 assert(run.find("sourceVertex",run.find("sourceVertex")+1)==read+10);
 assert(run.find("sourceVertex",read+10+12)==std::string::npos);
 assert(SameOrderedCorners(cube,GroundDecalIndicesG01));
 const std::set<std::uint16_t> ids(GroundDecalIndicesG01.begin(),GroundDecalIndicesG01.end());assert(ids.size()==8);
 // Every ordered triple (including its first/provoking corner) is unchanged.
 // Thus identical per-corner outputs preserve flat AND smooth interpolation.
 for(std::size_t t=0;t<12;++t)for(std::size_t c=0;c<3;++c)
  assert(cube[Rotate(3*t+c)]==cube[Rotate(GroundDecalIndicesG01[3*t+c])]);
 // Reject a changed corner or out-of-domain original vertex at every position.
 unsigned rejected=0;
 for(std::size_t i=0;i<36;++i){
  auto altered=GroundDecalIndicesG01;
  for(std::uint16_t v=0;v<36;++v)if(cube[Rotate(v)]!=cube[Rotate(i)]){altered[i]=v;break;}
  assert(!SameOrderedCorners(cube,altered));++rejected;
  altered[i]=36;assert(!SameOrderedCorners(cube,altered));++rejected;
 }
 for(bool enabled:{false,true})for(unsigned samples:{0u,1u,2u,4u,8u,32u})for(std::size_t instances:{std::size_t(0),std::size_t(1),std::size_t(32364)})
  assert(GroundDecalIndexEligibleG01(enabled,samples,instances)==(enabled&&samples==1&&instances>0));
 assert(GroundDecalIndexBufferValidG01(0x1000,72));
 assert(GroundDecalIndexBufferValidG01(0x1002,74));
 assert(!GroundDecalIndexBufferValidG01(0,72));
 assert(!GroundDecalIndexBufferValidG01(0x1001,72));
 assert(!GroundDecalIndexBufferValidG01(0x1000,70));
 assert(!GroundDecalIndexBufferValidG01(0x1000,73));
 assert(!GroundDecalIndexBufferValidG01(std::numeric_limits<std::uint64_t>::max()-1,72));
 std::cout<<"PASS actual shader36corners/12triangles/8IDs; mutations rejected="<<rejected<<"; eligibility36cases; buffer7cases\n";
}
