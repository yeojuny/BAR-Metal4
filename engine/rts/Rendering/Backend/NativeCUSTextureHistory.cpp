/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSTextureHistory.h"
#include <limits>
#include <stdexcept>
#include <utility>
namespace rendering {
namespace {
size_t Target(unsigned unit,NativeCUSTextureTarget target){
 if(unit>=NativeCUSTextureHistory::Units||target>=NativeCUSTextureTarget::Count)
  throw std::invalid_argument("CUS texture event has an invalid original unit/target");
 return size_t(target);
}
}
void NativeCUSTextureHistory::Advance(){
 if(sequence==std::numeric_limits<uint64_t>::max())throw std::overflow_error("CUS texture history sequence overflow");
 ++sequence;
}
void NativeCUSTextureHistory::ResetForNewContext(){
 if(context==std::numeric_limits<uint64_t>::max())throw std::overflow_error("CUS texture context generation overflow");
 Advance();++context;state={};stack.clear();activeUnit=0;defaultStorageKnownEmpty.fill(true);defaultSampling={};
 for(auto&unit:state)for(auto&binding:unit){binding.knowledge=NativeCUSBindingKnowledge::DefaultObject;binding.enableObserved=true;binding.defaultStorageKnownEmpty=true;}
}
void NativeCUSTextureHistory::CloseContext()noexcept{
 state={};stack.clear();activeUnit.reset();defaultStorageKnownEmpty.fill(false);defaultSampling={};
}
void NativeCUSTextureHistory::SetObjectResolvers(ObjectResolver current,ObjectResolver restoreSaved){
 if(bool(current)!=bool(restoreSaved))throw std::invalid_argument("Texture history requires both registry resolvers");
 resolveObject=std::move(current);restoreObject=std::move(restoreSaved);
}
void NativeCUSTextureHistory::ObjectStateChanged(){Advance();}
void NativeCUSTextureHistory::DeletedObject(uint64_t identity){
 if(!identity)throw std::invalid_argument("Texture deletion needs an observed object identity");
 Advance();
 for(auto&unit:state)for(size_t t=0;t<unit.size();++t){auto&binding=unit[t];
  if(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object->identity==identity){
   binding.knowledge=NativeCUSBindingKnowledge::DefaultObject;binding.object.reset();binding.defaultStorageKnownEmpty=defaultStorageKnownEmpty[t];
  }
 }
}
void NativeCUSTextureHistory::ActiveTexture(unsigned unit){Target(unit,NativeCUSTextureTarget::Image2D);Advance();activeUnit=unit;}
void NativeCUSTextureHistory::Bind(unsigned unit,std::shared_ptr<const NativeCUSTextureObject> object){
 if(!object||!object->identity||object->source.empty())throw std::invalid_argument("CUS binding needs an actual owned texture object");
 const auto target=Target(unit,object->target);Advance();auto&binding=state[unit][target];
 binding.knowledge=NativeCUSBindingKnowledge::BoundObject;binding.object=std::move(object);binding.defaultStorageKnownEmpty=false;
}
void NativeCUSTextureHistory::BindDefault(unsigned unit,NativeCUSTextureTarget target){
 auto index=Target(unit,target);Advance();auto&binding=state[unit][index];binding.knowledge=NativeCUSBindingKnowledge::DefaultObject;binding.object.reset();binding.defaultStorageKnownEmpty=defaultStorageKnownEmpty[index];
}
void NativeCUSTextureHistory::ObserveDefaultStorage(NativeCUSTextureTarget target,bool knownEmpty)
{
 const auto index=Target(0,target);Advance();defaultStorageKnownEmpty[index]=knownEmpty;
 for(auto& unit:state)if(unit[index].knowledge==NativeCUSBindingKnowledge::DefaultObject)
  unit[index].defaultStorageKnownEmpty=knownEmpty;
}
void NativeCUSTextureHistory::ObserveDefaultSampling(NativeCUSTextureTarget target,const NativeTextureSampling& actual)
{
 const auto index=Target(0,target);Advance();defaultSampling[index]=actual;
}
void NativeCUSTextureHistory::Enable(unsigned unit,NativeCUSTextureTarget target,bool enabled){
 auto index=Target(unit,target);Advance();auto&binding=state[unit][index];binding.enabled=enabled;binding.enableObserved=true;
}
void NativeCUSTextureHistory::LuaEnable2D(unsigned unit,bool enabled){Enable(unit,NativeCUSTextureTarget::Image2D,enabled);}
void NativeCUSTextureHistory::FailedImageLookup(unsigned unit){Target(unit,NativeCUSTextureTarget::Image2D);Advance();}
unsigned NativeCUSTextureHistory::BeginLuaTexture(unsigned requestedUnit)
{
	Target(requestedUnit,NativeCUSTextureTarget::Image2D);
	if (requestedUnit != 0) ActiveTexture(requestedUnit);
	if (!activeUnit) throw std::logic_error("CUS Lua texture call has no observed active texture unit");
	return *activeUnit;
}
bool NativeCUSTextureHistory::LuaTexture(unsigned requestedUnit,bool enabled)
{
	const unsigned unit = BeginLuaTexture(requestedUnit);
	LuaEnable2D(unit,enabled);
	if (requestedUnit != 0) ActiveTexture(0);
	return true;
}
bool NativeCUSTextureHistory::LuaTexture(unsigned requestedUnit,std::shared_ptr<const NativeCUSTextureObject> image)
{
	const bool parsed=bool(image);
	return LuaTextureLookup(requestedUnit,parsed,std::move(image));
}
bool NativeCUSTextureHistory::LuaTextureLookup(unsigned requestedUnit,bool parsed,std::shared_ptr<const NativeCUSTextureObject> image,
 const std::function<void(unsigned)>& afterOriginalBind)
{
 if(!parsed&&image)throw std::invalid_argument("Failed texture parse cannot have a bound object");
 if(image){Target(requestedUnit,image->target);if(!image->identity||image->source.empty())throw std::invalid_argument("Parsed Lua image lacks its actual owner");}
 return LuaTextureLookup(requestedUnit,[parsed,image]{return std::make_pair(parsed,image);},afterOriginalBind);
}
bool NativeCUSTextureHistory::LuaTextureLookup(unsigned requestedUnit,const LuaImageParse& parse,
 const std::function<void(unsigned)>& afterOriginalBind)
{
 if(!parse)throw std::invalid_argument("Original texture parser missing");
 const unsigned unit=BeginLuaTexture(requestedUnit);
 auto [parsed,image]=parse(); // Original active-unit change precedes actual loader writes.
 if(!parsed&&image)throw std::invalid_argument("Failed texture parse cannot bind an object");
 if(image){Bind(unit,image);if(image->target==NativeCUSTextureTarget::Image2D)Enable(unit,image->target,true);}
 else FailedImageLookup(unit);
 if(parsed&&afterOriginalBind)afterOriginalBind(unit);
 if(requestedUnit!=0)ActiveTexture(0);
 return parsed;
}
void NativeCUSTextureHistory::PushAttributes(bool bindings,bool enables){
 if(!bindings&&!enables)throw std::invalid_argument("CUS texture history attribute push must own state");
 auto saved=state;
 if(bindings&&resolveObject)for(auto&unit:saved)for(auto&binding:unit)if(binding.object)binding.object=resolveObject(binding.object);
 stack.push_back({std::move(saved),activeUnit,bindings,enables,defaultSampling});
 try{Advance();}catch(...){stack.pop_back();throw;}
}
void NativeCUSTextureHistory::PopAttributes(){
 if(stack.empty())throw std::logic_error("CUS texture history attribute stack underflow");
 Advance();auto saved=std::move(stack.back());stack.pop_back();if(saved.bindings){activeUnit=saved.active;defaultSampling=saved.defaultSampling;}
 for(size_t u=0;u<Units;++u)for(size_t t=0;t<state[u].size();++t){auto&current=state[u][t];const auto&prior=saved.state[u][t];
  if(saved.bindings){current.knowledge=prior.knowledge;current.object=prior.object&&restoreObject?restoreObject(prior.object):prior.object;current.defaultStorageKnownEmpty=prior.knowledge==NativeCUSBindingKnowledge::DefaultObject?defaultStorageKnownEmpty[t]:false;}
  if(saved.enables){current.enabled=prior.enabled;current.enableObserved=prior.enableObserved;}
 }
}
NativeCUSTextureBinding NativeCUSTextureHistory::Binding(unsigned unit,NativeCUSTextureTarget target)const{
 auto result=state[unit][Target(unit,target)];
 if(result.object&&resolveObject)result.object=resolveObject(result.object);
 if(result.knowledge==NativeCUSBindingKnowledge::DefaultObject)result.defaultSampling=defaultSampling[size_t(target)];
 return result;
}
NativeCUSDrawTextureSnapshot NativeCUSTextureHistory::Submit(uint64_t frame,unsigned pass,std::string shader,std::string bin,std::span<const NativeCUSSubmittedObject> objects){
 if(pass!=0&&pass!=1&&pass!=5&&pass!=16)throw std::invalid_argument("CUS draw requires an actual selected30770 pass");
 if(shader.empty()||bin.empty()||objects.empty())throw std::invalid_argument("CUS draw requires original shader/bin/submission identities");
 NativeCUSDrawTextureSnapshot out;out.sourceDrawFrame=frame;out.originalPass=pass;out.originalShader=std::move(shader);out.originalUniformBin=std::move(bin);
 out.originalSubmissionOrder.assign(objects.begin(),objects.end());
 for(unsigned i=0;i<out.samplers.size();++i)out.samplers[i]=Binding(i,i==7?NativeCUSTextureTarget::Cube:i==10?NativeCUSTextureTarget::Image3D:NativeCUSTextureTarget::Image2D);
 for(size_t i=0;i<3;++i)out.wreck[i]=out.samplers[i+3];
 Advance();out.contextGeneration=context;out.eventSequence=sequence;return out;
}
} // namespace rendering
