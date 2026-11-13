#include "Rendering/Backend/NativeCUSCPUReadWorker220.h"
#include "Rendering/EngineBridge/NativeOpaqueModelCapture.h"
#include "Rendering/Backend/WorldFrameData.h"
#include "Rendering/Backend/NativeWaterSceneData.h"
#include "Rendering/Backend/NativeOriginalShadowData.h"
#include "Rendering/Metal4/Metal4CaptureSurfaceBoundary.h"
#include <cassert>
#include <iostream>
#include <string>
#include <cstring>
#include <atomic>
using namespace rendering;
bool earlyCPURead220=true;
int mode=2;bool CPUReadPacketMeasure220(){return true;}int CPUReadPacketMode220(){return mode;}
#include "receipt.inc"
struct Session {std::unique_ptr<NativeCUSCPUReadWorker220> cpuReadWorker220;bool cpuReadDisabled220=false;int cpuReadFallback220=0;uint64_t cpuReadJobs220=0;};
struct Producers {
 NativeCUSPublishedStorage transforms{NativeCUSPublishedKind::Transforms}, uniforms{NativeCUSPublishedKind::ModelUniforms};
 NativeCUSUniformStorage engine;
 Producers(){transforms.NewContext();uniforms.NewContext();engine.NewContext();}
};
NativeCUSSourceDraw ValidDraw(uint64_t frame,uint32_t offset,Producers* producers=nullptr){
 auto geometry=std::make_shared<WorldModel>();geometry->pieces.resize(1);geometry->pieces[0].vertices.resize(1);geometry->pieces[0].indices={0};
 auto model=std::make_shared<NativeCUSModelSubmission>();model->object={NativeCUSObjectKind::Unit,7};model->geometry=geometry;model->originalIndexCount=1;
 std::vector<NativeCUSSubmittedObject> objects{model->object};std::vector<std::shared_ptr<const NativeCUSModelSubmission>> submissions{model};std::vector<std::string> bins{"bin"};
 auto roster=NativeCUSSubmissionRoster::Seal(objects,submissions,bins);
 auto instances=std::make_shared<NativeBufferSnapshot>();instances->layout.elements=1;instances->layout.stride=16;
 NativeBufferAttribute attr;attr.id=6;attr.scalarType=NativeScalarType::UInt32;attr.byteSize=16;attr.declaredCount=4;instances->layout.attributes={attr};
 const std::array<uint32_t,4> raw{offset,0,255,0};instances->bytes.resize(16);std::memcpy(instances->bytes.data(),raw.data(),16);
 Producers local;auto& transforms=producers?producers->transforms:local.transforms;auto& uniforms=producers?producers->uniforms:local.uniforms;
 std::array<uint8_t,256> tb{};std::array<uint8_t,128> ub{};NativeCUSPublishedWrite tw{0,tb},uw{0,ub};
 auto t=transforms.Publish(frame,8,true,std::span(&tw,1));auto u=uniforms.Publish(frame,1,true,std::span(&uw,1));
 auto inputs=MakeNativeCUSModelInputs(frame,1,roster,instances,t,u);
 NativeCUSTextureHistory history;history.ResetForNewContext();auto textures=history.Submit(frame,0,"unit","bin",roster);
 auto program=std::make_shared<NativeCUSProgramConsumer>();program->pipelineLifetime=std::make_shared<int>(1);program->family="unit";program->vertexSHA512="vertex";program->fragmentSHA512="fragment";
 auto& engine=producers?producers->engine:local.engine;std::array<uint8_t,NativeCUSMatrixBytes> matrices{};std::array<uint8_t,NativeCUSParamsBytes> params{};engine.PublishMatrices(frame,matrices);engine.PublishParams(frame,params,{});
 NativeCUSSourceDraw draw;draw.uniformValues=std::make_shared<const NativeCUSProgramUniforms>();draw.roster=roster;draw.program=program;draw.sequence=1;draw.instances=instances;draw.modelInputs=inputs;draw.textures=std::make_shared<NativeCUSDrawTextureSnapshot>(textures);draw.engineUniforms=engine.Current();
 ValidateNativeCUSSourceDraw(draw);return draw;
}
int main(){
 auto session=std::make_unique<Session>();
 for(int iteration=0;iteration<100;++iteration){
  const auto main=std::this_thread::get_id();bool destroyed=false;
  auto source=std::shared_ptr<NativeCUSSourceFrame>(new NativeCUSSourceFrame,[&](auto* p){assert(std::this_thread::get_id()==main);destroyed=true;delete p;});
  source->sourceDrawFrame=iteration+1;source->draws.resize(40); // invalid draws keep original validation fallback.
  Producers producers;source->draws.push_back(ValidDraw(iteration+1,0,&producers));
  auto models=std::make_shared<NativeOpaqueModelFrame>();models->sourceDrawFrame=iteration+1;models->passes.resize(1);models->passes[0].lua=source;
  WorldFrame frame;frame.sourceDrawFrame=iteration+1;frame.opaqueModelFrame=models;
  // An older packet must not be cleared by creating/finishing the younger local job.
  auto oldSource=std::make_shared<NativeCUSSourceFrame>();oldSource->draws.push_back(ValidDraw(1000+iteration,4));
  NativeCUSCPUReadScope oldScope;oldScope.Prime(oldSource);const auto old=oldScope.Seal();assert(old.Count()==1);const auto* oldIdentity=&old;
  NativeCUSCPUReadScope::ReadView oldView(old);
  std::unique_ptr<CPUReadReceipt220> cpuReceipt220;NativeCUSCPUReadWorker220::Join cpuJoin220;int cpuReadFallback220=0;
#include "launch.inc"
  mode=2+(iteration%2);std::string order;const auto before=session->cpuReadJobs220;
  bool prefixFails=iteration%5==0,drainFails=iteration%5==1,particleFails=iteration%5==2;
  try{
   CaptureAtNativeSurfaceBoundary(true,[]{return true;},[&]{order+='D';assert(oldIdentity==&old);if(drainFails)throw std::runtime_error("drain");},[]{assert(false);},[&]{
    order+='C';if(prefixFails)throw std::runtime_error("prefix");launchCPURead220(frame);order+='L';
   });
   order+='P';if(particleFails)throw std::runtime_error("particle");
#include "tail.inc"
  }catch(const std::runtime_error&){}
  assert(order==(prefixFails?"CD":drainFails?"CLD":"CLDP"));
  const bool handedOff=!prefixFails&&!drainFails&&!particleFails;
  if(handedOff)cpuJoin220.TransferTo(cpuReceipt220->pending);
  cpuJoin220.Finish(); // Transferred guard must no longer own the job.
  // Model subsequent main capture: replace all current frame pins while the
  // immutable receipt remains the sole source owner, then consume or abort.
  if(handedOff){
   // Subsequent original uploads replace live publications, never old pages.
   for(unsigned write=1;write<=16;++write){
    std::array<uint8_t,32> bytes{};bytes.fill(uint8_t(write));NativeCUSPublishedWrite changed{0,bytes};
    producers.transforms.Publish(iteration+2,8,false,std::span(&changed,1));
    std::array<uint8_t,NativeCUSParamsBytes> params{};params.fill(uint8_t(write));
    producers.engine.PublishParams(iteration+2,params,{});
   }
   std::array<uint8_t,32> oldBytes{};
   ReadNativeCUSPublishedElement(*source->draws.back().modelInputs->transforms,0,oldBytes);
   assert((oldBytes==std::array<uint8_t,32>{}));
   assert(source->draws.back().engineUniforms->params->bytes[0]==0);
   frame.opaqueModelFrame.reset();models.reset();source.reset();assert(!destroyed);
   if(iteration%2){cpuReceipt220.reset();assert(destroyed);continue;} // hidden/abort/destructor join
   cpuReceipt220->pending.Finish();
   frame.opaqueModelFrame=cpuReceipt220->models;
   source=std::const_pointer_cast<NativeCUSSourceFrame>(cpuReceipt220->models->passes[0].lua);
  }
  assert(session->cpuReadJobs220==before+!prefixFails);
  if(!prefixFails){
   assert(cpuReceipt220&&cpuReceipt220->job.done&&!cpuReceipt220->job.error&&cpuReceipt220->Matches(frame));
   NativeCUSCPUReadScope expected;expected.Prime(source);assert(expected.Matches(cpuReceipt220->job.packet));
   assert(cpuReceipt220->job.packet.Count()==1&&cpuReceipt220->job.packet.RowDigest()!=old.RowDigest());
   assert(NativeCUSCPUReadScope::ActiveCount()==1);
   assert(NativeCUSCPUReadScope::Find(oldSource->draws[0])&& !NativeCUSCPUReadScope::Find(source->draws.back()));
   {NativeCUSCPUReadScope::ReadView next(cpuReceipt220->job.packet);
    assert(NativeCUSCPUReadScope::Find(source->draws.back())&&!NativeCUSCPUReadScope::Find(oldSource->draws[0]));}
   assert(NativeCUSCPUReadScope::Find(oldSource->draws[0])&&!NativeCUSCPUReadScope::Find(source->draws.back()));
  }else assert(!cpuReceipt220);
  frame.opaqueModelFrame.reset();models.reset();source.reset();assert(prefixFails?destroyed:!destroyed);
  cpuReceipt220.reset();assert(destroyed);assert(cpuReadFallback220==0);
 }
 // One actual worker scope retains ordering/dedup across the sealed tail.
 for(int n=0;n<100;++n){
  NativeCUSCPUReadWorker220 worker;NativeCUSCPUReadWorker220::Job job;
  auto opaque=std::make_shared<NativeCUSSourceFrame>();opaque->sourceDrawFrame=77;opaque->draws.push_back(ValidDraw(77,0));
  auto alpha=std::make_shared<NativeCUSSourceFrame>();alpha->sourceDrawFrame=77;alpha->draws.push_back(ValidDraw(77,4));
  job.sources={opaque};job.ExpectTail();NativeCUSCPUReadWorker220::Join join;
  worker.Start(job);join.Arm(worker,job);
  bool publish=n%3!=0;
  if(publish){job.PublishTail(n%3==1?std::vector<std::shared_ptr<const NativeCUSSourceFrame>>{opaque,alpha}:std::vector<std::shared_ptr<const NativeCUSSourceFrame>>{});
   bool rejected=false;try{job.PublishTail({});}catch(const std::logic_error&){rejected=true;}assert(rejected);
  }
  join.Finish();assert(job.done&&!job.error);
  NativeCUSCPUReadScope expected;expected.Prime(opaque);if(n%3==1)expected.Prime(alpha);
  assert(expected.Matches(job.packet));assert(job.packet.Count()==(n%3==1?2:1));
  if(!publish){bool rejected=false;try{job.PublishTail({});}catch(const std::logic_error&){rejected=true;}assert(rejected);}
 }
 {
  CPUReadReceipt220 receipt;receipt.frame=88;receipt.includesTail=true;
  auto alpha=std::make_shared<NativeOpaqueModelFrame>();alpha->sourceDrawFrame=88;alpha->passes.resize(1);
  auto source=std::make_shared<NativeCUSSourceFrame>();source->sourceDrawFrame=88;alpha->passes[0].lua=source;
  auto shadow=std::make_shared<NativeOriginalShadowFrame>();shadow->sourceDrawFrame=88;shadow->cus=source;
  receipt.alphaModels[1]=alpha;receipt.shadow=shadow;receipt.job.ExpectTail();receipt.job.PublishTail({source,source});
  WorldFrame frame;frame.sourceDrawFrame=88;frame.alphaModelFrames[1]=alpha;frame.originalShadow=shadow;
  assert(receipt.Matches(frame));frame.alphaModelFrames[1]=std::make_shared<NativeOpaqueModelFrame>(*alpha);assert(!receipt.Matches(frame));
  frame.alphaModelFrames[1]=alpha;receipt.job.tailSources.pop_back();assert(!receipt.Matches(frame));
  receipt.job.tailSources.push_back(source);shadow->sourceDrawFrame=89;assert(!receipt.Matches(frame));
 }
 { // Exact cap and duplicate order across the actual continuation boundary.
  auto a=std::make_shared<NativeCUSSourceFrame>(),b=std::make_shared<NativeCUSSourceFrame>();
  auto draw=ValidDraw(99,0);
  for(size_t i=0;i<NativeCUSCPUReadScope::EntryLimit+8;++i){auto d=draw;d.program=std::make_shared<NativeCUSProgramConsumer>(*draw.program);(i<4095?a:b)->draws.push_back(std::move(d));}
  b->draws.insert(b->draws.begin(),a->draws.front());
  NativeCUSCPUReadWorker220 worker;NativeCUSCPUReadWorker220::Job job;job.sources={a};job.ExpectTail();
  NativeCUSCPUReadWorker220::Join join;worker.Start(job);join.Arm(worker,job);job.PublishTail({b});join.Finish();
  NativeCUSCPUReadScope expected;expected.Prime(a);expected.Prime(b);assert(expected.Count()==NativeCUSCPUReadScope::EntryLimit);assert(expected.Matches(job.packet));
 }
 for(int n=0;n<100;++n){
  NativeCUSCPUReadWorker220 worker;NativeCUSCPUReadWorker220::Job job;job.ExpectTail();
  auto tail=std::make_shared<NativeCUSSourceFrame>();tail->draws.push_back(ValidDraw(101,0));
  NativeCUSCPUReadWorker220::Join join;worker.Start(job);join.Arm(worker,job);
  std::atomic<bool> go=false;bool published=false;
  std::thread publisher([&]{while(!go.load()){}try{job.PublishTail({tail});published=true;}catch(const std::logic_error&){} });
  std::thread cancel([&]{while(!go.load()){}job.CancelTail();});go=true;
  publisher.join();cancel.join();join.Finish();
  assert(job.done&&!job.error);assert(job.packet.Count()==size_t(published));
  assert(job.tailState==(published?NativeCUSCPUReadWorker220::Job::TailState::Published:NativeCUSCPUReadWorker220::Job::TailState::Cancelled));
 }
 // Exact already-captured water models are optional, after the old prefix.
 for(int n=0;n<100;++n){
  auto source=std::make_shared<NativeCUSSourceFrame>();source->sourceDrawFrame=91;source->draws.push_back(ValidDraw(91,0));
  auto model=std::make_shared<NativeOpaqueModelFrame>();model->sourceDrawFrame=91;model->passes.resize(1);model->passes[0].lua=source;
  auto reflection=std::make_shared<NativeWaterSceneView>(),refraction=std::make_shared<NativeWaterSceneView>();
  reflection->frame.sourceDrawFrame=refraction->frame.sourceDrawFrame=91;
  auto refrSource=std::make_shared<NativeCUSSourceFrame>();refrSource->sourceDrawFrame=91;refrSource->draws.push_back(ValidDraw(91,4));
  auto refrModel=std::make_shared<NativeOpaqueModelFrame>(*model);refrModel->passes[0].lua=refrSource;
  reflection->frame.opaqueModelFrame=model;refraction->frame.alphaModelFrames[0]=refrModel;
  refraction->frame.alphaModelFrames[2]=std::make_shared<NativeOpaqueModelFrame>(); // Not consumed by renderer, not speculated.
  auto water=std::make_shared<NativeWaterSceneFrame>();water->sourceDrawFrame=91;water->reflection=reflection;water->refraction=refraction;
  WorldFrame frame;frame.sourceDrawFrame=91;frame.waterScene=water;
  CPUReadReceipt220 receipt;receipt.frame=91;receipt.includesTail=true;receipt.waterReads=CPUReadWaterRoster220::Capture(91,water);
  assert(receipt.waterReads->sources.size()==2&&receipt.waterReads->sources[0]==refrSource&&receipt.waterReads->sources[1]==source);receipt.job.ExpectTail();receipt.job.PublishTail({},receipt.waterReads->sources);
  NativeCUSCPUReadWorker220::Build(receipt.job);assert(!receipt.job.error&&!receipt.job.optionalFailed&&receipt.Matches(frame));
  NativeCUSCPUReadScope reference;for(const auto& src:receipt.waterReads->sources)reference.Prime(src);assert(reference.Matches(receipt.job.packet));
  auto changed=std::make_shared<NativeWaterSceneFrame>(*water);frame.waterScene=changed;assert(!receipt.Matches(frame));frame.waterScene=water;
  water->reflection=std::make_shared<NativeWaterSceneView>(*reflection);assert(!receipt.Matches(frame));water->reflection=reflection;
  reflection->frame.opaqueModelFrame=std::make_shared<NativeOpaqueModelFrame>(*model);assert(!receipt.Matches(frame));reflection->frame.opaqueModelFrame=model;
  source->sourceDrawFrame=92;assert(!receipt.Matches(frame));source->sourceDrawFrame=91;
  receipt.job.optionalFailed=true;assert(!receipt.Matches(frame));receipt.job.optionalFailed=false;
  receipt.job.optionalSources.pop_back();assert(!receipt.Matches(frame));
 }
 // Original modes retain their existing caller location; actual launch behavior remains unchanged.
 earlyCPURead220=false;
 for(mode=0;mode<=1;++mode){WorldFrame frame;frame.opaqueModelFrame=std::make_shared<NativeOpaqueModelFrame>();
  std::unique_ptr<CPUReadReceipt220> cpuReceipt220;NativeCUSCPUReadWorker220::Join cpuJoin220;int cpuReadFallback220=0;
#include "launch.inc"
  launchCPURead220(frame);assert(bool(cpuReceipt220)==(mode==1));
 }
 std::cout<<"100 actual worker launch/validation/mode3 receipt comparison, transferred consume/abort ownership, prefix/drain/particle failure and main-owner release cycles PASS\n";
}
