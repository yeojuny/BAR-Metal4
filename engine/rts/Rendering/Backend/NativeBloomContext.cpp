/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeBloomContext.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include <cmath>
#include <charconv>
#include <utility>
namespace rendering {
namespace {void Check(bool x,const char*t){if(!x)throw std::runtime_error(t);}uint64_t nextImage=1,nextFBO=1;uint64_t Next(uint64_t&v){Check(v&&v<UINT64_MAX,"Original Bloom resource identity overflow");return v++;}}
NativeBloomContext::NativeBloomContext(NativeBloomRecorder&r,uint32_t m,NativeBloomEvents e):recorder(r),maxDimension(m),events(std::move(e)){Check(m&&events.state&&events.binding&&events.sourceDrawFrame,"Bloom requires actual renderer/context callbacks");}
NativeBloomContext::~NativeBloomContext(){for(auto&[name,cell]:images)if(!cell->deleted&&events.deleted)events.deleted(name);}
std::string NativeBloomContext::CreateTexture(uint32_t w,uint32_t h,NativeBloomFormat format,NativeTextureSampling sampling){Check(w&&h&&w<=maxDimension&&h<=maxDimension,"Original Bloom texture exceeds actual native extent");Check((sampling.minFilter==0x2600||sampling.minFilter==0x2601)&&(sampling.magFilter==0x2600||sampling.magFilter==0x2601)&&sampling.wrapS==0x812f&&sampling.wrapT==0x812f,"Original Bloom sampler policy differs");auto im=std::make_shared<NativeBloomImage>();im->identity=Next(nextImage);im->width=w;im->height=h;im->format=format;im->sampling=sampling;auto cell=std::make_shared<NativeBloomImageCell>();cell->image=im;std::string name="!nativeBloom"+std::to_string(im->identity);auto [i,ok]=images.emplace(name,cell);Check(ok,"Duplicate Bloom texture identity");try{if(events.created)events.created(name,cell);}catch(...){images.erase(i);throw;}return name;}
void NativeBloomContext::DeleteTexture(const std::string&name){auto i=images.find(name);if(i==images.end())return;if(!i->second->deleted&&events.deleted)events.deleted(name);i->second->deleted=true;images.erase(i);}
std::shared_ptr<NativeBloomFBO>NativeBloomContext::CreateFBO(const std::string&name){auto i=images.find(name);Check(i!=images.end()&&!i->second->deleted,"Original Bloom color attachment is missing/deleted");auto f=std::make_shared<NativeBloomFBO>();f->identity=Next(nextFBO);f->color=i->second;fbos.emplace(f->identity,f);return f;}
void NativeBloomContext::DeleteFBO(const std::shared_ptr<NativeBloomFBO>&f){if(!f)return;if(target==f)target.reset();f->deleted=true;fbos.erase(f->identity);}
uint64_t NativeBloomContext::BindFBO(std::shared_ptr<NativeBloomFBO>f){Check(f&&!f->deleted&&f->color&&!f->color->deleted,"Invalid original Bloom FBO");uint64_t prior=target?target->identity:0;target=std::move(f);return prior;}
void NativeBloomContext::RestoreFBO(uint64_t name){if(!name){target.reset();return;}auto i=fbos.find(name);Check(i!=fbos.end()&&!i->second->deleted,"Bloom raw FBO restoration names an unknown/deleted object");target=i->second;}
std::shared_ptr<NativeBloomProgramState>NativeBloomContext::CreateProgram(NativeBloomProgram kind,NativeBloomDefines d){ValidateNativeBloomDefines(d);auto p=std::make_shared<NativeBloomProgramState>();p->kind=kind;p->defines=std::move(d);
 std::vector<std::string> uniforms;
 switch(kind){case NativeBloomProgram::Bright:uniforms={"illuminationThreshold","kneeWidth","fragGlowAmplifier","maxBrightContribution"};break;
 case NativeBloomProgram::Downsample:break;case NativeBloomProgram::Upsample:uniforms={"filterRadius"};break;
 case NativeBloomProgram::FinalUpsample:uniforms={"sourceTexelSize","filterRadius","historyMix"};break;
 case NativeBloomProgram::Combine:uniforms={"bloomNorm","debugDraw"};break;}
 for(size_t n=0;n<uniforms.size();++n)p->uniformLocations.emplace(int(n),uniforms[n]);return p;}
void NativeBloomContext::Activate(std::shared_ptr<NativeBloomProgramState>p){Check(p&&!p->deleted,"Deleted Bloom program activation");active=std::move(p);}
void NativeBloomContext::Deactivate(const std::shared_ptr<NativeBloomProgramState>&p){if(active==p)active.reset();}
void NativeBloomContext::Finalize(const std::shared_ptr<NativeBloomProgramState>&p){if(!p)return;Deactivate(p);p->deleted=true;}
void NativeBloomContext::Uniform(const std::shared_ptr<NativeBloomProgramState>&p,std::string_view name,std::span<const float>v){Check(p&&!p->deleted,"Deleted Bloom uniform target");for(float x:v)Check(std::isfinite(x),"Nonfinite original Bloom uniform");auto&u=p->uniforms;
 if(name=="sourceTexelSize"){Check(v.size()==2,"Bloom source texel arity");std::copy(v.begin(),v.end(),u.sourceTexelSize.begin());return;}
 Check(v.size()==1,"Original Bloom scalar uniform arity");float x=v[0];
 if(name=="illuminationThreshold")u.illuminationThreshold=x;else if(name=="kneeWidth")u.kneeWidth=x;else if(name=="fragGlowAmplifier")u.glowAmplifier=x;else if(name=="maxBrightContribution")u.maxBrightContribution=x;else if(name=="filterRadius")u.filterRadius=x;else if(name=="historyMix")u.historyMix=x;else if(name=="bloomNorm")u.bloomNorm=x;
 else if(name=="debugDraw"){Check(std::floor(x)==x&&x>=0&&x<=1,"Original Bloom int uniform differs");u.debugDraw=int(x);}
 else throw std::runtime_error("Unknown original Bloom uniform");}
int NativeBloomContext::GetUniformLocation(const std::shared_ptr<NativeBloomProgramState>&p,std::string_view name)const{Check(p&&!p->deleted,"Deleted Bloom uniform location target");for(const auto&[location,uniform]:p->uniformLocations)if(uniform==name)return location;return -1;}
void NativeBloomContext::UniformCurrent(int location,std::span<const float>values){if(location==-1)return;Check(active&&!active->deleted,"Bloom gl.Uniform requires its actual active program");auto found=active->uniformLocations.find(location);Check(found!=active->uniformLocations.end(),"Bloom uniform location does not belong to its active program");Uniform(active,found->second,values);}
void NativeBloomContext::Texture(unsigned unit,const std::string&name){Check(unit<8&&events.bind,"Bloom texture binding outside original sampler slots");events.bind(unit,name);}
void NativeBloomContext::DisableTexture(unsigned unit){Check(unit<8&&events.disable,"Bloom texture disable outside original sampler slots");events.disable(unit);}
void NativeBloomContext::Commit(NativeBloomDraw d){Check(recorder.Active(),"Bloom draw outside original DrawWorld capture");d.sequence=NextNativeWorldDrawSequence();auto cell=target?target->color:nullptr;std::string name;if(cell){Check(!cell->deleted&&cell->nextGeneration<UINT64_MAX,"Deleted/exhausted Bloom target");auto v=std::make_shared<NativeBloomImageVersion>();v->image=cell->image;v->generation=cell->nextGeneration;v->sourceDrawFrame=events.sourceDrawFrame();v->producerSequence=d.sequence;d.output=v;d.previous=cell->current;for(auto&[key,value]:images)if(value==cell){name=key;break;}Check(!name.empty(),"Bloom original target texture name lost");}
 auto next=d.output,previous=d.previous;auto cancel=std::make_shared<std::function<void()>>();
 auto rollback=[cell,next,previous,cancel]()noexcept{if(*cancel)(*cancel)();if(cell&&cell->current==next)cell->current=previous;};
 recorder.RecordTransactional(std::move(d),[&]{if(cell&&events.publish)*cancel=events.publish(name,next);if(cell){cell->current=next;++cell->nextGeneration;}},cell?std::function<void()>(std::move(rollback)):std::function<void()>{});
}
void NativeBloomContext::Draw(std::array<std::array<float,4>,6>vertices){Check(active&&!active->deleted,"Bloom VAO draw has no actual program");NativeBloomDraw d;d.program=active->kind;d.defines=active->defines;d.uniforms=active->uniforms;d.vertices=vertices;auto state=events.state();d.surface=std::move(state.first);d.world=std::move(state.second);for(unsigned i=0;i<8;++i){d.inputs[i].actual=events.binding(i);auto object=d.inputs[i].actual.object;if(object&&object->backing)if(auto b=std::dynamic_pointer_cast<const NativeBloomImageBacking>(object->backing)){d.inputs[i].local=b->version;d.inputs[i].actual={};}}Commit(std::move(d));}
std::shared_ptr<const NativeBloomImage> NativeBloomContext::Image(const std::string&name)const{auto i=images.find(name);Check(i!=images.end()&&!i->second->deleted,"Original Bloom RTT texture missing/deleted");return i->second->image;}
}
