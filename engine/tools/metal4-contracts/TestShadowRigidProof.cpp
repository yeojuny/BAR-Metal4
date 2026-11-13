#include <array>
#include <vector>
#include <memory>
#include <set>
#include <string>
#include <stdexcept>
#include <cassert>
#include <random>
#include <cstdint>
#include <cstdlib>
#include <new>
static int failAfter=-1;static size_t fastVisited=0,referenceVisited=0;
void* operator new(std::size_t n){if(failAfter==0)throw std::bad_alloc();if(failAfter>0)--failAfter;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
enum class WorldModelGL4BoneLayout:uint8_t{Unclassified,RigidPerPiece,Unsupported};
struct Vertex{std::array<uint16_t,4> boneIds{};std::array<uint8_t,4> boneWeights{};};
struct Piece{std::string name="piece";std::vector<Vertex> vertices;std::vector<uint32_t> indices;bool HasGeometryData()const{return indices.size()>=3;}};
struct WorldModel{std::string sourceName="source";std::vector<Piece> pieces;WorldModelGL4BoneLayout gl4BoneLayout=WorldModelGL4BoneLayout::Unclassified;};
struct Object{std::shared_ptr<const WorldModel> model;int modelType=1,owner=0,id=12;std::vector<int> pieces;};
struct NativeEngineShadowDraw{std::shared_ptr<const Object> object;};
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
#include "shadow-rigid.inc"
// Generated from the actual capture predicate, with copied vertices as its input.
#include "capture-rigid.inc"
std::string Run(const NativeEngineShadowDraw& d,bool reference,std::set<const WorldModel*>& seen){try{if(reference)ReferenceRigid(d,seen);else Rigid(d,seen);return {};}catch(const std::runtime_error& e){return e.what();}}
NativeEngineShadowDraw Draw(std::shared_ptr<const WorldModel> model){auto o=std::make_shared<Object>();o->model=model;if(model)o->pieces.resize(model->pieces.size());return {o};}
int main(){
 std::mt19937 rng(0x561);
 for(int iteration=0;iteration<4000;++iteration){auto m=std::make_shared<WorldModel>();m->pieces.resize(rng()%12);
  for(size_t p=0;p<m->pieces.size();++p){auto& x=m->pieces[p];x.vertices.resize(rng()%12);for(auto& v:x.vertices){v.boneIds[0]=p;v.boneWeights={255,0,0,0};if(rng()%9==0)v.boneWeights[1]=1;}if(!x.vertices.empty()){x.indices.resize(rng()%7);for(auto& i:x.indices)i=rng()%x.vertices.size();}}
  m->gl4BoneLayout=CaptureClassification(*m);auto d=Draw(m);std::set<const WorldModel*> a,b;assert(Run(d,false,a)==Run(d,true,b));assert(a==b);
  // Unclassified and Unsupported retain full original fallback, even for valid geometry.
  for(auto state:{WorldModelGL4BoneLayout::Unclassified,WorldModelGL4BoneLayout::Unsupported}){m->gl4BoneLayout=state;a.clear();b.clear();assert(Run(d,false,a)==Run(d,true,b));assert(a==b);}
 }
 // Instrumented actual loop verifies elimination, not merely equal outcomes.
 {auto m=std::make_shared<WorldModel>();m->pieces.resize(1);m->pieces[0].vertices.resize(4);for(auto& v:m->pieces[0].vertices)v.boneWeights={255,0,0,0};m->pieces[0].indices={0,1,2};m->gl4BoneLayout=CaptureClassification(*m);std::set<const WorldModel*> a,b;fastVisited=referenceVisited=0;assert(Run(Draw(m),false,a)==Run(Draw(m),true,b));assert(fastVisited==0&&referenceVisited==4);}
 // Emitter-only vertices need not have rigid bones, but drawable unreferenced vertices do.
 for(unsigned count:{0u,1u,2u,3u}){auto m=std::make_shared<WorldModel>();m->pieces.resize(1);m->pieces[0].vertices.resize(2);m->pieces[0].vertices[0].boneWeights={255,0,0,0};m->pieces[0].indices.assign(count,0);m->gl4BoneLayout=CaptureClassification(*m);assert((m->gl4BoneLayout==WorldModelGL4BoneLayout::RigidPerPiece)==(count<3));std::set<const WorldModel*> a,b;assert(Run(Draw(m),false,a)==Run(Draw(m),true,b));}
 // Cached/proven success must not bypass per-draw format/pose validation.
 {auto m=std::make_shared<WorldModel>();m->gl4BoneLayout=CaptureClassification(*m);auto d=Draw(m);std::set<const WorldModel*> a,b;Rigid(d,a);ReferenceRigid(d,b);for(int mode=0;mode<4;++mode){auto bad=std::make_shared<Object>(*d.object);if(mode==0)bad->modelType=2;if(mode==1)bad->pieces.resize(1);if(mode==2)bad->model.reset();NativeEngineShadowDraw x{mode==3?nullptr:bad};auto e=Run(x,false,a);assert(!e.empty()&&e==Run(x,true,b));}}
 // Empty model success still performs the original insertion and its allocation.
 {auto m=std::make_shared<WorldModel>();m->gl4BoneLayout=CaptureClassification(*m);auto d=Draw(m);for(bool ref:{false,true}){std::set<const WorldModel*> a;failAfter=0;bool failed=false;try{if(ref)ReferenceRigid(d,a);else Rigid(d,a);}catch(const std::bad_alloc&){failed=true;}failAfter=-1;assert(failed&&a.empty());}}
 // Public metadata is a trusted capture contract, NOT proof against fabricated owners.
 {auto m=std::make_shared<WorldModel>();m->pieces.resize(1);m->pieces[0].vertices.resize(1);m->pieces[0].indices={0,0,0};assert(CaptureClassification(*m)==WorldModelGL4BoneLayout::Unsupported);m->gl4BoneLayout=WorldModelGL4BoneLayout::RigidPerPiece;std::set<const WorldModel*> a,b;assert(Run(Draw(m),false,a).empty());assert(!Run(Draw(m),true,b).empty());}
}
