/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
namespace rendering {
struct NativeCUSProgramUniforms {
 using Table=std::map<std::string,std::vector<float>>;
 Table floats,ints;
};
// Source-owner only. Captured draws retain immutable content; subsequent writes
// do not mutate it. Publication is lazy at the original Submit boundary.
class NativeCUSProgramUniformBuilder {
 NativeCUSProgramUniforms values;
 std::shared_ptr<const NativeCUSProgramUniforms> sealed;
public:
 void Initialize(NativeCUSProgramUniforms::Table floats,NativeCUSProgramUniforms::Table ints) {
  values={std::move(floats),std::move(ints)};sealed.reset();
 }
 void Write(bool integer,const std::string& name,std::vector<float> value) {
  auto& table=integer?values.ints:values.floats;
  const auto it=table.find(name);
  if(it!=table.end()&&it->second.size()==value.size()&&
   (value.empty()||std::memcmp(it->second.data(),value.data(),value.size()*sizeof(float))==0))return;
  table.insert_or_assign(name,std::move(value));sealed.reset();
 }
 std::shared_ptr<const NativeCUSProgramUniforms> Capture() {
  if(!sealed)sealed=std::make_shared<const NativeCUSProgramUniforms>(values);
  return sealed;
 }
};
} // namespace rendering
