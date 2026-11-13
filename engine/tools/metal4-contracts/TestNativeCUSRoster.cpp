// Production CPU contract: immutable roster + per-event capture/read receipts.
#include "Rendering/Backend/NativeCUSCPUReadScope.h"
#include "Rendering/Backend/WorldFrameData.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <atomic>
#include "Rendering/Backend/NativeSerialRenderWorker.h"
using namespace rendering;
#ifdef BAR_TEST_WORKER_IDLE_HOOK
namespace rendering {
std::atomic<bool> testIdleArmed=false,testIdleEntered=false,testIdleRelease=false;
void NativeTestAfterIdle(){if(testIdleArmed.exchange(false)){testIdleEntered=true;while(!testIdleRelease.load())std::this_thread::yield();}}
}
#endif
int checks=0;
void Check(bool v){++checks;if(!v)throw std::runtime_error("contract check "+std::to_string(checks));}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected);}
int main(){
 auto geometry=std::make_shared<WorldModel>();geometry->pieces.resize(1);
 geometry->pieces[0].vertices.resize(1);geometry->pieces[0].indices={0,0,0};
 geometry->pieces[0].vertices[0].boneWeights[0]=255;
 std::vector<NativeCUSSubmittedObject> objects={{NativeCUSObjectKind::Unit,7},{NativeCUSObjectKind::Feature,9}};
 std::vector<std::shared_ptr<const NativeCUSModelSubmission>> submissions;
 for(auto object:objects){auto s=std::make_shared<NativeCUSModelSubmission>();s->object=object;s->geometry=geometry;s->originalIndexFirst=37;s->originalIndexCount=3;submissions.push_back(s);}
 std::vector<std::string> bins={"bin1","bin2"};
 auto roster=NativeCUSSubmissionRoster::Seal(objects,submissions,bins);
 Check(roster->Objects().size()==2);Check(roster->Submissions()[0]==submissions[0]);Check(roster->UniformBins()==bins);
 Check(roster->Submissions()[0]->originalIndexFirst==37);Check(roster->Submissions()[0]->originalIndexCount==3);
 Reject([&]{NativeCUSSubmissionRoster::Seal({},submissions,bins);});
 auto invalid=submissions;invalid[0].reset();Reject([&]{NativeCUSSubmissionRoster::Seal(objects,invalid,bins);});
 auto bad=std::make_shared<NativeCUSModelSubmission>(*submissions[0]);bad->originalIndexCount=4;invalid[0]=bad;
 Reject([&]{NativeCUSSubmissionRoster::Seal(objects,invalid,bins);});
 bad->originalIndexCount=3;bad->object.id=8;Reject([&]{NativeCUSSubmissionRoster::Seal(objects,invalid,bins);});
 auto badbins=bins;badbins[0].clear();Check(NativeCUSSubmissionRoster::Seal(objects,submissions,badbins)->UniformBins()[0].empty());
 // Swap-remove builder semantics: old draw remains unchanged, no ID re-resolution.
 objects[0]=objects.back();objects.pop_back();submissions[0]=submissions.back();submissions.pop_back();bins[0].swap(bins.back());bins.pop_back();
 auto next=NativeCUSSubmissionRoster::Seal(objects,submissions,bins);Check(next!=roster);Check(next->Objects()[0].id==9);Check(roster->Objects()[0].id==7);
 objects.clear();submissions.clear();bins.clear();Check(roster->Submissions()[0]->geometry==geometry);
 auto instances=std::make_shared<NativeBufferSnapshot>();instances->layout.elements=2;instances->layout.stride=16;
 NativeBufferAttribute attr;attr.id=6;attr.scalarType=NativeScalarType::UInt32;attr.byteSize=16;attr.declaredCount=4;instances->layout.attributes={attr};
 std::array<uint32_t,8> raw={0,0,255,0,0,0,255,0};instances->bytes.resize(32);std::memcpy(instances->bytes.data(),raw.data(),32);
 NativeCUSPublishedStorage transforms(NativeCUSPublishedKind::Transforms),uniforms(NativeCUSPublishedKind::ModelUniforms);transforms.NewContext();uniforms.NewContext();
 std::array<uint8_t,128> bytes{};float scale=1;std::memcpy(bytes.data()+28,&scale,4);std::memcpy(bytes.data()+60,&scale,4);std::memcpy(bytes.data()+92,&scale,4);std::memcpy(bytes.data()+124,&scale,4);
 NativeCUSPublishedWrite write{0,bytes};auto t=transforms.Publish(1,4,true,std::span(&write,1));auto u=uniforms.Publish(1,1,true,std::span(&write,1));
 auto first=MakeNativeCUSModelInputs(1,1,roster,instances,t,u);auto second=MakeNativeCUSModelInputs(1,2,roster,instances,t,u);
 Check(first!=second);Check(first->roster==second->roster);Check(first->Submissions().data()==second->Submissions().data());Check(first->sourceSequence==1&&second->sourceSequence==2);
 Reject([&]{MakeNativeCUSModelInputs(1,2,nullptr,instances,t,u);});
 auto broken=std::make_shared<NativeBufferSnapshot>(*instances);broken->layout.attributes[0].id=5;Reject([&]{MakeNativeCUSModelInputs(1,2,roster,broken,t,u);});
 NativeCUSTextureHistory textures;textures.ResetForNewContext();
 auto tex1=textures.Submit(1,0,"unit","bin1",roster);auto tex2=textures.Submit(1,0,"unit","bin1",roster);
 Check(tex1.roster==roster);Check(tex1.originalSubmissionOrder.empty());Check(tex1.SubmissionOrder().data()==roster->Objects().data());Check(tex1.eventSequence!=tex2.eventSequence);
 // Legacy texture capture remains a reference path, without dual storage on owned captures.
 auto legacy=textures.Submit(1,0,"unit","bin1",roster->Objects());Check(!legacy.roster);Check(legacy.SubmissionOrder().size()==2);Check(legacy.SubmissionOrder()[0].id==7);
 auto program=std::make_shared<NativeCUSProgramConsumer>();program->pipelineLifetime=std::make_shared<int>(1);program->family="unit";program->vertexSHA512="vertex";program->fragmentSHA512="fragment";
 NativeCUSUniformStorage engine;engine.NewContext();std::array<uint8_t,NativeCUSMatrixBytes> matrices{};std::array<uint8_t,NativeCUSParamsBytes> params{};engine.PublishMatrices(1,matrices);engine.PublishParams(1,params,{});
 NativeCUSSourceDraw draw;draw.uniformValues=std::make_shared<const NativeCUSProgramUniforms>();draw.roster=roster;draw.program=program;draw.sequence=1;draw.instances=instances;draw.modelInputs=first;draw.textures=std::make_shared<NativeCUSDrawTextureSnapshot>(tex1);draw.engineUniforms=engine.Current();
 ValidateNativeCUSSourceDraw(draw);auto reads=ValidateNativeCUSRenderReads(draw);Check(reads.teamColors.empty());Check(reads.transforms==std::vector<uint32_t>({0,1,2,3}));
 auto newer=draw;newer.sequence=2;newer.modelInputs=second;newer.textures=std::make_shared<NativeCUSDrawTextureSnapshot>(tex2);ValidateNativeCUSSourceDraw(newer);
 auto frame=std::make_shared<NativeCUSSourceFrame>();frame->draws={draw,newer};
 NativeCUSCPUReadScope scope;scope.Prime(frame);Check(scope.Count()==1);Check(NativeCUSCPUReadScope::Find(draw)==NativeCUSCPUReadScope::Find(newer));
 auto changed=newer;changed.roster=next;Check(!NativeCUSCPUReadScope::Find(changed));Reject([&]{ValidateNativeCUSSourceDraw(changed);});
 changed=newer;changed.instances=std::make_shared<NativeBufferSnapshot>(*instances);Check(!NativeCUSCPUReadScope::Find(changed));
 changed=newer;auto newmodel=std::make_shared<NativeCUSModelInputs>(*second);newmodel->transforms=std::make_shared<NativeCUSPublishedBuffer>(*t);changed.modelInputs=newmodel;Check(!NativeCUSCPUReadScope::Find(changed));
 newmodel=std::make_shared<NativeCUSModelInputs>(*second);newmodel->modelUniforms=std::make_shared<NativeCUSPublishedBuffer>(*u);changed.modelInputs=newmodel;Check(!NativeCUSCPUReadScope::Find(changed));
 changed=newer;changed.engineUniforms=std::make_shared<NativeCUSUniformPublication>(*draw.engineUniforms);Check(!NativeCUSCPUReadScope::Find(changed));
 changed=newer;changed.program=std::make_shared<NativeCUSProgramConsumer>(*program);Check(!NativeCUSCPUReadScope::Find(changed));
 // Equal address with different ownership is not accepted as the same receipt.
 changed=newer;changed.program=std::shared_ptr<const NativeCUSProgramConsumer>(program.get(),[](auto*){});Check(!NativeCUSCPUReadScope::Find(changed));
 changed=newer;auto mismatched=std::make_shared<NativeCUSModelInputs>(*second);mismatched->originalInstances=std::make_shared<NativeBufferSnapshot>(*instances);changed.modelInputs=mismatched;Check(!NativeCUSCPUReadScope::Find(changed));
 changed=newer;changed.modelInputs.reset();Check(!NativeCUSCPUReadScope::Find(changed));
 changed=newer;changed.sequence=99;Reject([&]{ValidateNativeCUSSourceDraw(changed);});
 auto packet=scope.Seal();Check(packet.Count()==1);Check(scope.Count()==0);NativeCUSCPUReadScope receiving;receiving.Adopt(std::move(packet));Check(receiving.Count()==1);Check(NativeCUSCPUReadScope::Find(newer));
 // The actual joined render worker must consume a sealed immutable packet,
 // not main's TLS scope or geometry cache.
 receiving.SetPreparationNanoseconds(12345);
 auto sharedPacket=std::make_shared<const NativeCUSCPUReadScope::Packet>(receiving.Seal());
 const auto mainThread=std::this_thread::get_id();
 NativeSerialRenderWorker worker;
 for(unsigned iteration=0;iteration<1000;++iteration){
  worker.Invoke([sharedPacket,&draw,&newer,&program,&reads]{
   Check(!NativeCUSCPUReadScope::Find(draw));
   const auto cacheBefore=NativeCUSCPUReadScope::GeometryCacheSize();
   {
    NativeCUSCPUReadScope::ReadView view(*sharedPacket);
    Check(NativeCUSCPUReadScope::ActiveCount()==1);
    Check(NativeCUSCPUReadScope::ActivePreparationNanoseconds()==12345);
    auto found=NativeCUSCPUReadScope::Find(draw);
    Check(found && found->transforms==reads.transforms);
    Check(found==NativeCUSCPUReadScope::Find(newer));
    auto mismatch=draw;mismatch.program=std::shared_ptr<const NativeCUSProgramConsumer>(program.get(),[](auto*){});
    Check(!NativeCUSCPUReadScope::Find(mismatch));
    try {
     NativeCUSCPUReadScope::Packet empty;
     NativeCUSCPUReadScope::ReadView nested(empty);
     Check(!NativeCUSCPUReadScope::Find(draw));Check(NativeCUSCPUReadScope::ActiveCount()==0);
     throw std::runtime_error("nested view unwind");
    }catch(const std::runtime_error&){}
    Check(NativeCUSCPUReadScope::Find(draw)==found);
   }
   Check(!NativeCUSCPUReadScope::Find(draw));
   Check(NativeCUSCPUReadScope::GeometryCacheSize()==cacheBefore);
  });
 }
 Reject([&]{worker.Invoke([sharedPacket]{NativeCUSCPUReadScope::ReadView view(*sharedPacket);throw std::runtime_error("joined unwind");});});
 worker.Invoke([&]{Check(!NativeCUSCPUReadScope::Find(draw));Check(NativeCUSCPUReadScope::ActiveCount()==0);});
 Check(NativeCUSCPUReadScope::ActiveCount()==0);
 {
  NativeCUSCPUReadScope::ReadView mainView(*sharedPacket);
  Check(NativeCUSCPUReadScope::Find(draw));
 }
 // Packet owns the last source-program reference; joined return must release
 // the worker task capture before main drops its last packet owner.
 std::atomic<bool> sourceReleased=false;std::thread::id releasedOn;
 auto ownedProgram=std::shared_ptr<const NativeCUSProgramConsumer>(new NativeCUSProgramConsumer(*program),[&](const auto* p){releasedOn=std::this_thread::get_id();delete p;sourceReleased.store(true);});
 auto ownedDraw=draw;ownedDraw.program=ownedProgram;
 auto ownedFrame=std::make_shared<NativeCUSSourceFrame>();ownedFrame->draws={ownedDraw};
 std::shared_ptr<const NativeCUSCPUReadScope::Packet> lastPacket;
 {NativeCUSCPUReadScope build;build.Prime(ownedFrame);lastPacket=std::make_shared<const NativeCUSCPUReadScope::Packet>(build.Seal());}
 ownedFrame.reset();ownedDraw.program.reset();ownedProgram.reset();
#ifdef BAR_TEST_WORKER_IDLE_HOOK
 testIdleArmed=true;
#endif
 worker.Invoke([lastPacket]{NativeCUSCPUReadScope::ReadView view(*lastPacket);Check(NativeCUSCPUReadScope::ActiveCount()==1);});
#ifdef BAR_TEST_WORKER_IDLE_HOOK
 while(!testIdleEntered.load())std::this_thread::yield();
#endif
 Check(!sourceReleased);lastPacket.reset();
 const bool mainReleased=sourceReleased && releasedOn==mainThread;
#ifdef BAR_TEST_WORKER_IDLE_HOOK
 testIdleRelease=true;
#endif
 worker.Invoke([]{}); // Join old closure cleanup before any failing assertion unwinds locals.
 Check(mainReleased);
 // Skinning retains exact original early-return and interpolation-sensitive owners.
 auto skin=std::make_shared<NativeCUSProgramConsumer>(*program);skin->family="unitskinning";auto skindraw=draw;skindraw.program=skin;
 skindraw.textures=std::make_shared<NativeCUSDrawTextureSnapshot>(textures.Submit(1,0,"unitskinning","bin1",roster));
 auto skinreads=ValidateNativeCUSRenderReads(skindraw);Check(skinreads.transforms==reads.transforms);
 // Ordinary missing observed team rows still reject, rather than fabricating zero colors.
 auto team=std::make_shared<NativeBufferSnapshot>(*instances);raw[2]=1;std::memcpy(team->bytes.data(),raw.data(),32);auto teamdraw=draw;teamdraw.instances=team;teamdraw.modelInputs=MakeNativeCUSModelInputs(1,1,roster,team,t,u);Reject([&]{ValidateNativeCUSRenderReads(teamdraw);});
 std::cout<<checks<<" CUS roster/read-receipt production checks passed\n";
}
