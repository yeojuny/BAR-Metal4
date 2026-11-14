#include "Metal4PublishedRequestCache.h"
#include "Metal4RetiredArguments.h"
#include "Metal4ModelUpload146.h"
#include "Metal4PublicationProbe.h"
/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4CUSSource.h"
#include "Metal4TerrainOwnerRoster.h"
#include "Metal4InterventionMath.h"
#include "Rendering/Backend/NativeUITextureHistory.h"
#include "NativeCUSGeneratedPrograms.h"
#include "Metal4NativeEffectState.h"
#include "Metal4ModelSetterProbe176.h"
#include "Rendering/Backend/NativeCUSSourceIdentity.h"
#include "Rendering/Backend/NativeS3OTextureSource.h"
#include "Rendering/Backend/NativeCUSRenderInputs.h"
#include "Rendering/Backend/NativeCUSCPUReadScope.h"
#include "Rendering/Backend/NativeCUSDrawRuns147.h"
#include "Rendering/Backend/NativePreparedGeometry154.h"
#include "Metal4ModelGeometry155.h"
#include "Metal4ModelMaterial156.h"
#include "Metal4CUSIdentityChannels194.h"
#include "Rendering/Backend/NativePreparationTrace.h"
#include "Rendering/Backend/NativeCUSPublishedReuse.h"
#include <cstring>
#include "System/Sync/SHA512.hpp"
#include <bit>
#include <map>
#include <set>
#include <unordered_map>
#include "System/Log/ILog.h"
#include "Metal4ModelTraceBudget.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace rendering::metal4 {
namespace {
struct Libraries {__strong id<MTLLibrary> vertex=nil,fragment=nil;};
struct Program {
 const NativeCUSGeneratedProgram* generated=nullptr;
 std::shared_ptr<const NativeCUSProgramSource> source;
 std::shared_ptr<const Libraries> libraries;
 std::unique_ptr<NativeCUSProgramUniformPlan> vertexUniformPlan,fragmentUniformPlan;
};
id<MTLBuffer> Allocate(RuntimeLease& lease,size_t bytes)
{
 Require(bytes&&bytes<=lease.Device().maxBufferLength,"CUS original buffer exceeds actual native device capacity");
 auto b=[lease.Device()newBufferWithLength:bytes options:MTLResourceStorageModeShared];Require(b!=nil,"Cannot allocate CUS original buffer");lease.AddAllocation(b);return b;
}
#include "Metal4CUSUniformUpload.inc"
ScreenAlphaCompare Compare(unsigned x){Require(x>=0x0200&&x<=0x0207,"CUS original depth compare differs");return ScreenAlphaCompare(x-0x0200);}
NativeWorldDrawState WorldState(const NativeCUSSourceDraw& d)
{
 Require(d.rasterObserved,"CUS original callback raster state was not observed");
 auto s=d.world;s.depthTest=d.depthTest;s.depthWrite=d.depthWrite;s.depthCompare=Compare(d.depthFunction);s.colorWrite=d.colorMask;s.cullEnabled=d.cullEnabled;
 switch(d.cullFace){case 0x0404:s.cull=NativeWorldCull::Front;break;case 0x0405:s.cull=NativeWorldCull::Back;break;case 0x0408:s.cull=NativeWorldCull::FrontAndBack;break;default:throw std::runtime_error("CUS original cull face differs");}
 return s;
}
ScreenBlendState BlendState(const NativeCUSSourceDraw& d)
{
 return d.surface.blend;
}
MTLBlendOperation Equation(ScreenBlendEquation x)
{
 switch(x){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}
 throw std::runtime_error("CUS original blend equation differs");
}
MTLColorWriteMask Mask(const std::array<bool,4>& c){return MTLColorWriteMask((c[0]?MTLColorWriteMaskRed:0)|(c[1]?MTLColorWriteMaskGreen:0)|(c[2]?MTLColorWriteMaskBlue:0)|(c[3]?MTLColorWriteMaskAlpha:0));}
std::vector<uint64_t> PipelineKey(const NativeCUSGeneratedProgram* p,const CUSSourceTarget& t,const ScreenBlendState& b,const std::array<bool,4>& mask,bool validation)
{
 std::vector<uint64_t> k{uint64_t(p),t.samples,uint64_t(validation),uint64_t(b.enabled),uint64_t(b.sourceColor),uint64_t(b.destinationColor),uint64_t(b.sourceAlpha),uint64_t(b.destinationAlpha),uint64_t(b.colorEquation),uint64_t(b.alphaEquation),uint64_t(Mask(mask))};
 for(auto c:t.colors)k.push_back(c);return k;
}
bool Empty(const NativeCUSTextureBinding& b)
{
 return ui::NativeUITextureHistory::SamplingIncomplete(b);
}
const NativeTextureSampling& Sampling(const NativeCUSTextureBinding& b)
{
 Require(b.knowledge!=NativeCUSBindingKnowledge::Unobserved,"CUS original sampler binding was never observed");
 if(b.knowledge==NativeCUSBindingKnowledge::DefaultObject)return b.defaultSampling;
 Require(bool(b.object),"CUS bound sampler lacks its original immutable image owner");return b.object->sampling;
}
// Only World::OriginalModelImage's retained S3O branch supplies this owner
// with DescribeSampler -> ScreenGPU::Sampler. Other providers remain generic.
bool CanonicalModelSampler219(const NativeCUSTextureBinding& binding,const CUSSourceImage& image)
{
 if(binding.knowledge!=NativeCUSBindingKnowledge::BoundObject||!binding.object||
    image.owner.get()!=binding.object.get()||image.depthImage||image.clampDepthCompare||
    image.texture.textureType!=MTLTextureType2D)return false;
 const auto& object=*binding.object;
 const auto* model=dynamic_cast<const NativeModelTextureBacking*>(object.backing.get());
 if(!model||!model->pixels||model->pixels!=object.image||
    !object.contextGeneration||!object.identity||object.target!=NativeCUSTextureTarget::Image2D||
    object.storage!=NativeTextureStorageKnowledge::Published||
    model->content!=NativeTextureContent::ProducerVersion||model->storageGeneration!=object.identity)return false;
 const auto& state=object.sampling;
 // The current Screen descriptor leaves R/compare/LOD/reduction at their
 // defaults. Source wrapR/compareFunction do not change that 2D descriptor.
 return state.minFilter==0x2703&&state.magFilter==0x2601&&
  state.wrapS==0x2901&&state.wrapT==0x2901&&state.anisotropy==1.0f&&
  state.compareMode==0&&state.baseLevel==0&&state.lodBias==0.0f&&
  state.borderColor==std::array<float,4>{0,0,0,0};
}
#include "Metal4ModelTrace.inc"
}
// CUSForwardVertexProof220 begin: independently exercised by the source fixture.
namespace {
std::string CUSForwardVertexTableDigest220(std::span<const NativeCUSGeneratedProgram> programs)
{
 // Freeze the actual native source, including its sample-shading prelude, and
 // every admitted source pair/vertex uniform ABI. Original GLSL SHA alone does
 // not identify the MSL actually passed to RuntimeLease::NewLibrary.
 std::vector<uint8_t> bytes;
 const auto number=[&](uint64_t value){for(unsigned n=0;n<8;++n)bytes.push_back(uint8_t(value>>(n*8)));};
 const auto text=[&](const char* value){if(!value)return false;const size_t size=std::strlen(value);number(size);bytes.insert(bytes.end(),value,value+size);return true;};
 number(programs.size());
 for(const auto& program:programs){
  const auto& vertex=program.vertex;
  if(vertex.textureMask||!text(program.family))return {};
  number(program.originalPass);
  if(!text(program.vertexSHA512)||!text(program.fragmentSHA512)||!text(vertex.metalSource))return {};
  number(vertex.uniformBytes);number(vertex.textureMask);number(vertex.fields.size());
  for(const auto& field:vertex.fields){
   if(!text(field.name))return {};
   number(uint64_t(field.scalar));number(field.offset);number(field.components);
   number(field.count);number(field.stride);number(field.active);
  }
 }
 sha512::raw_digest digest;sha512::calc_digest(bytes,digest);return sha512::dump_digest(digest);
}
}
bool CUSForwardVertexInputsAreHostOnly()
{
 // This is a capability of this closed program set and the existing private
 // CPU-upload path, not of arbitrary argument tables or future shaders. Those
 // paths bind host-copied immutable geometry/instances/publications/constants
 // in slots0..6; reviewed MSL never observes fragment slots7/8 or images.
 // One process-wide calculation; additions/changes fail closed without adding
 // hashing or inspection to individual draws. Re-review source/layout/uploads
 // and update the fixture before changing the frozen digest.
 static const bool proven=CUSForwardVertexTableDigest220(NativeCUSGeneratedPrograms)==
  "ed7d1a749f5b5f327af14365f2d72512e53c39289cfdfcbe5930c904c7aacbceda7fa6d474d33f1c6b12c1620648ac0142173c25503b7c6ae5201d4f37fd11a0";
 return proven;
}
// CUSForwardVertexProof220 end


struct PreparedCUSSource::Impl {
 RetiredArgumentOwners argumentOwners;
 ModelUploadOwners146 uniformOwners;
 using Geometry=ModelGeometrySlice155;
 struct Object {uint32_t geometry=0,instance=0,instances=1;};
 struct Draw {
  uint64_t sequence=0;__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;__strong id<MTL4ArgumentTable> table=nil;
  NativeWorldDrawState state;effectstate::Raster raster;std::vector<Object> objects;
  __strong id<MTLBuffer> material156=nil;
  std::vector<CUSSourceImage> images;
  float shadowPolygonOffsetUnitsScale=1.f;
  bool encoded=false;
 };
 TerrainOwnerRoster firstOwners;
 std::vector<const CUSSourceImage*> inspection160;std::vector<id<MTLTexture>> sampledRoots160;
 uint64_t indexedDraws160=0;bool inspectionReady160=false;
 std::unique_ptr<model176::Probe> setterProbe176;
 std::shared_ptr<const NativeCUSSourceFrame> owner;std::vector<Geometry> geometry;std::vector<Draw> draws;bool dependenciesEncoded=false,completed=false;
};
PreparedCUSSource::PreparedCUSSource():impl(std::make_unique<Impl>()){}
PreparedCUSSource::~PreparedCUSSource()=default;
size_t PreparedCUSSource::DrawCount()const{return impl->draws.size();}
bool PreparedCUSSource::HostOnlyNoDependencyCommands220()const
{
 if(impl->completed||impl->dependenciesEncoded||impl->draws.empty()||!CUSForwardVertexInputsAreHostOnly())return false;
 for(const auto& draw:impl->draws)for(const auto& image:draw.images)
  if(InspectSourceDependency160(image).work!=SourceDependencyWork160::NoCommands)return false;
 return true;
}
uint64_t PreparedCUSSource::SequenceAt(size_t n)const{return impl->draws.at(n).sequence;}
void PreparedCUSSource::PreparePassInspection160()
{
 Require(!impl->inspectionReady160&&!impl->dependenciesEncoded&&!impl->completed,"CUS pass inspection prepared after use or twice");
 std::set<const void*> roots;size_t nextOwner=0;
 for(size_t d=0;d<impl->draws.size();++d){
  const auto& draw=impl->draws[d];impl->indexedDraws160+=draw.objects.size();
  for(size_t i=0;i<draw.images.size();++i){
   const auto& image=draw.images[i];
   if(nextOwner<impl->firstOwners.size()&&impl->firstOwners[nextOwner]==std::pair<size_t,size_t>{d,i}){
    ++nextOwner;if(image.encodeDependencies)impl->inspection160.push_back(&image);
   }
   const auto root=SourceTextureRoot160(image.texture);
   if(root&&roots.insert((__bridge const void*)root).second)impl->sampledRoots160.push_back(root);
  }
 }
 impl->inspectionReady160=true;
}
ModelPassDependencyReport160 PreparedCUSSource::InspectDependencies160(MTL4RenderPassDescriptor* pass)const
{
 Require(pass&&impl->inspectionReady160&&!impl->dependenciesEncoded&&!impl->completed,"CUS pass query is outside its prepared dependency phase");
 ModelPassDependencyReport160 report;report.indexedDraws=impl->indexedDraws160;
 const auto color=SourceTextureRoot160(pass.colorAttachments[0].texture);
 const auto depth=SourceTextureRoot160(pass.depthAttachment.texture),stencil=SourceTextureRoot160(pass.stencilAttachment.texture);
 for(const auto root:impl->sampledRoots160)if(root==color||root==depth||root==stencil)report.Add({SourceDependencyWork160::Unknown,false});
 for(const auto* image:impl->inspection160)report.Add(InspectSourceDependency160(*image));
 return report;
}
void PreparedCUSSource::AcceptDependenciesWithoutCommands160()
{
 Require(impl->inspectionReady160&&!impl->dependenciesEncoded&&!impl->completed,"CUS no-command dependency phase repeated or retired");
 // Only PreparedOpaqueModelSource calls this, immediately after every child
 // inspection succeeds. Do not invoke a producer callback or mark completion.
 impl->dependenciesEncoded=true;
}
void PreparedCUSSource::EncodeDependencies(id<MTL4CommandBuffer> command)
{
 Require(command!=nil&&!impl->dependenciesEncoded,"CUS source dependencies repeated or missing command");
 for(const auto& [d,i]:impl->firstOwners){
  const auto& image=impl->draws[d].images[i];
  if(image.encodeDependencies)image.encodeDependencies(command);
 }
 impl->dependenciesEncoded=true;
}
void PreparedCUSSource::EncodeDraw(size_t n,id<MTL4RenderCommandEncoder> encoder)
{
 Require(encoder!=nil&&impl->dependenciesEncoded&&!impl->completed,"CUS source draw lacks its actual dependency stage");auto& d=impl->draws.at(n);
 Require(!d.encoded,"CUS source draw was already encoded");
 for(const auto& image:d.images)if(image.requireReadable)image.requireReadable();
 effectstate::RasterState(encoder,d.state,d.raster);
 // RasterState set the unscaled bias, no culling and fill on this encoder.
 const float resolvedBias=d.state.polygonOffset?d.state.offsetUnits*d.shadowPolygonOffsetUnitsScale:0.f;
 const bool scaledBias=d.state.polygonOffset && std::bit_cast<uint32_t>(resolvedBias)!=std::bit_cast<uint32_t>(d.state.offsetUnits);
 const bool wire=d.state.frontPolygonMode==NativePolygonMode::Line;
 if(scaledBias)[encoder setDepthBias:resolvedBias slopeScale:d.state.offsetFactor clamp:0];
 if(d.state.cullEnabled)[encoder setCullMode:d.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];
 if(wire)[encoder setTriangleFillMode:MTLTriangleFillModeLines];
 [encoder setRenderPipelineState:d.pipeline];[encoder setDepthStencilState:d.depth];
 MTLGPUAddress boundGeometry155=0;
 for(const auto& object:d.objects){
  const auto& model=impl->geometry[object.geometry];
  if(boundGeometry155!=model.vertexAddress){
   [d.table setAddress:model.vertexAddress atIndex:0];[encoder setArgumentTable:d.table atStages:MTLRenderStageVertex|MTLRenderStageFragment];
   boundGeometry155=model.vertexAddress;
  }
  [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:model.count indexType:MTLIndexTypeUInt32 indexBuffer:model.indexAddress indexBufferLength:model.indexLength instanceCount:object.instances baseVertex:model.baseVertex baseInstance:object.instance];
 }
 if(impl->setterProbe176)impl->setterProbe176->Draw(model176::Cull|model176::Fill|(d.state.polygonOffset?model176::Bias:0u),
  (d.state.cullEnabled?model176::Cull:0u)|(wire?model176::Fill:0u)|(scaledBias?model176::Bias:0u),d.objects.size());
 d.encoded=true;
}
void PreparedCUSSource::DidComplete()
{
 Require(!impl->completed&&impl->dependenciesEncoded&&std::all_of(impl->draws.begin(),impl->draws.end(),[](const auto& d){return d.encoded;}),"CUS source cannot complete an unencoded/repeated submission");
 for(const auto& [d,i]:impl->firstOwners){
  const auto& image=impl->draws[d].images[i];
  if(image.didComplete)image.didComplete();
 }
 impl->completed=true;
 if(impl->setterProbe176)impl->setterProbe176->Report();
}

struct CUSSourceGPU::Impl {
 RetiredArguments argumentPool;
 ModelUploadPool146 uniformPool;bool reported146=false,reported147=false;
 uint64_t preparation154=0;
 bool reported155=false;
 ModelMaterials156 materials156;bool reportedMaterial156=false;
 NativeCUSGeometryReadCache geometryReads;
 std::map<const NativeCUSGeneratedProgram*,std::shared_ptr<const Libraries>> libraries;
 std::map<const NativeCUSGeneratedProgram*,id<MTLLibrary>> identityFragments194;
 std::map<std::vector<uint64_t>,id<MTLRenderPipelineState>> pipelines;
 std::unordered_map<const void*,std::weak_ptr<const Program>> programs;
 struct Buffer {std::weak_ptr<const void> owner;__strong id<MTLBuffer> value=nil;};
 struct Geometry {std::weak_ptr<const WorldModel> owner;ModelGeometrySlice155 gpu;size_t logicalBytes=0;NativePreparedGeometry154 prepared;};
 std::unordered_map<const void*,Buffer> buffers;std::unordered_map<const WorldModel*,Geometry> geometry;
 std::shared_ptr<const Libraries> Library(RuntimeLease& lease,const NativeCUSGeneratedProgram& p,double& ms)
 {
  if(auto it=libraries.find(&p);it!=libraries.end())return it->second;
  auto out=std::make_shared<Libraries>();const auto start=Clock::now();
  for(bool vertex:{true,false}){
   auto d=[MTL4LibraryDescriptor new];d.name=String(std::string("Original CUS ")+p.family+" pass"+std::to_string(p.originalPass));d.source=String(vertex?std::string(p.vertex.metalSource):ModelMaterialFragment156(p.fragment.metalSource,p.fragment.textureMask));
   auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;ConfigureInterventionColorMath(options);d.options=options;
   NSError* error=nil;auto library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile full original CUS native stage: "+Error(error));
   if(vertex)out->vertex=library;else out->fragment=library;
  }
  ms+=Milliseconds(start);libraries.emplace(&p,out);return out;
 }
 id<MTLLibrary> IdentityFragment194(RuntimeLease& lease,const NativeCUSGeneratedProgram& p,double& ms)
 {
  if(auto it=identityFragments194.find(&p);it!=identityFragments194.end())return it->second;
  const auto start=Clock::now();auto d=[MTL4LibraryDescriptor new];
  d.name=String(std::string("Original CUS identity channels194 ")+p.family+" pass"+std::to_string(p.originalPass));
  d.source=String(NativeCUSIdentityFragment194(ModelMaterialFragment156(p.fragment.metalSource,p.fragment.textureMask),p.fragment.textureMask));
  auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;ConfigureInterventionColorMath(options);d.options=options;
  NSError* error=nil;auto library=lease.NewLibrary(d,&error);Require(library,"Cannot compile exact CUS identity channels194: "+Error(error));
  identityFragments194.emplace(&p,library);ms+=Milliseconds(start);return library;
 }
 id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const Program& p,const CUSSourceTarget& target,const ScreenBlendState& blend,const std::array<bool,4>& mask,bool validation,double& ms,bool coverage=false,bool identityChannels194=false,ModelSamplerPolicy219 samplerPolicy=ModelSamplerPolicy219::Generic)
 {
  auto key=PipelineKey(p.generated,target,blend,mask,validation);key.push_back(coverage);key.push_back(target.combinedColor);key.push_back(lease.SampleShadingEnabled(target.samples));key.push_back(identityChannels194);key.push_back(uint64_t(samplerPolicy));if(auto it=pipelines.find(key);it!=pipelines.end()){if(identityChannels194)lease.Retain(identityFragments194.at(p.generated));return it->second;}
  auto fragmentLibrary=identityChannels194?IdentityFragment194(lease,*p.generated,ms):p.libraries->fragment;
  const auto start=Clock::now();auto d=[MTL4RenderPipelineDescriptor new];auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];
  v.library=p.libraries->vertex;v.name=@"NativeCUSVertex";f.library=fragmentLibrary;f.name=@"NativeCUSFragment";lease.Retain(f.library);
  d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.rasterSampleCount=target.samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
  if(ModelSamplerPair219(p.generated->fragment.textureMask)){
   // Both generic and canonical PSOs use the same already-linked library.
   // Set false explicitly too: an unspecialized function is reflection-only.
   const bool canonical=samplerPolicy==ModelSamplerPolicy219::CanonicalPair;
   auto values=[MTLFunctionConstantValues new];
   [values setConstantValue:&canonical type:MTLDataTypeBool atIndex:ModelSamplerConstant219];
   auto specialized=[MTL4SpecializedFunctionDescriptor new];
   specialized.functionDescriptor=f;specialized.constantValues=values;
   d.fragmentFunctionDescriptor=specialized;
  }
  auto layout=[MTLVertexDescriptor vertexDescriptor];
  for(unsigned n=0;n<4;++n){layout.attributes[n].format=MTLVertexFormatFloat3;layout.attributes[n].offset=n*12;layout.attributes[n].bufferIndex=0;}
  layout.attributes[4].format=MTLVertexFormatFloat4;layout.attributes[4].offset=48;layout.attributes[4].bufferIndex=0;
  layout.attributes[5].format=MTLVertexFormatUInt3;layout.attributes[5].offset=64;layout.attributes[5].bufferIndex=0;
  layout.attributes[6].format=MTLVertexFormatUInt4;layout.attributes[6].offset=0;layout.attributes[6].bufferIndex=1;
  layout.layouts[0].stride=sizeof(NativeCUSRenderVertex);layout.layouts[0].stepFunction=MTLVertexStepFunctionPerVertex;
  layout.layouts[1].stride=16;layout.layouts[1].stepFunction=MTLVertexStepFunctionPerInstance;layout.layouts[1].stepRate=1;d.vertexDescriptor=layout;
  for(unsigned n=0;n<target.colors.size();++n){auto a=d.colorAttachments[n];a.pixelFormat=target.colors[n];a.writeMask=Mask(mask);a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
   a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);}
  auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
  if(target.combinedColor!=MTLPixelFormatInvalid){d.colorAttachments[5].pixelFormat=target.combinedColor;d.colorAttachments[5].writeMask=MTLColorWriteMaskNone;}
  NSError* error=nil;auto pipeline=lease.NewRenderPipeline(d,&error);Require(pipeline!=nil,"Cannot compile full CUS native target pipeline: "+Error(error));
  pipelines.emplace(std::move(key),pipeline);ms+=Milliseconds(start);return pipeline;
 }
 template<class T,class F>id<MTLBuffer> Cached(RuntimeLease& lease,std::shared_ptr<const T> owner,size_t bytes,F fill)
 {
  Require(bool(owner),"CUS cached buffer has no actual immutable owner");auto& item=buffers[owner.get()];
  if(item.owner.expired()){if(ModelTrace().window){++ModelTrace().allocations;ModelTrace().allocationBytes+=bytes;}auto buffer=Allocate(lease,bytes);fill(static_cast<uint8_t*>(buffer.contents));item={owner,buffer};}
  else{if(ModelTrace().window)++ModelTrace().cacheHits;Require(item.value.length==bytes,"CUS immutable buffer changed extent");lease.AddAllocation(item.value);}
  return item.value;
 }
 id<MTLBuffer> Published(RuntimeLease& lease,std::shared_ptr<const NativeCUSPublishedBuffer> source)
 {
  PublicationProbe publicationProbe(lease,source,0);
  Require(bool(source),"CUS published buffer has no immutable owner");
  Require(source->kind==NativeCUSPublishedKind::Transforms||source->kind==NativeCUSPublishedKind::ModelUniforms,"CUS published buffer kind differs");
  PublishedUploadStats stats;
  auto shared=SharePublishedRequest(lease,source,publicationProbe,ModelTrace().window?&stats:nullptr);
  if(ModelTrace().window){auto& t=ModelTrace();t.allocations+=stats.allocations;t.allocationBytes+=stats.allocatedBytes;t.publishedUploads+=stats.uploads;t.publishedBytes+=stats.copiedBytes;t.cacheHits+=stats.hits;}
  publicationProbe.Finish(shared);
  return shared;
}
 template<class Table>
 uint32_t Model(RuntimeLease& lease,const std::shared_ptr<const WorldModel>& source,uint64_t generation,Table& prepared,ModelGeometryBatch155& batch)
 {
  auto& item=geometry[source.get()];
  return item.prepared.Resolve(generation,prepared,[&]{
   if(item.owner.expired()){
    auto data=BuildNativeCUSRenderGeometry(source);Require(!data.vertices.empty()&&!data.indices.empty(),"CUS submitted original model has no indexed geometry");
    ModelGeometrySlice155 pending;pending.count=uint32_t(data.indices.size());
    item.logicalBytes=data.vertices.size()*sizeof(NativeCUSRenderVertex)+data.indices.size()*4;
    batch.Add(uint32_t(prepared.size()),std::move(data));return pending;
   }
   lease.AddAllocation(item.gpu.storage);return item.gpu;
  });
 }
};
CUSSourceGPU::CUSSourceGPU():impl(std::make_unique<Impl>()){}
CUSSourceGPU::~CUSSourceGPU()=default;
void CUSSourceGPU::ClearResources(){impl->materials156.Clear();impl->argumentPool.Clear();impl->geometryReads.Clear();impl->buffers.clear();impl->geometry.clear();std::erase_if(impl->programs,[](const auto& x){return x.second.expired();});}
std::shared_ptr<const NativeCUSProgramConsumer> CUSSourceGPU::LinkOriginalStages(RuntimeLease& lease,const NativeCUSProgramSource& input,double& ms)
{
 auto source=std::make_shared<NativeCUSProgramSource>(input);const auto identity=ValidateNativeCUSProgramSource(*source);
 const NativeCUSGeneratedProgram* generated=nullptr;
 for(const auto& p:NativeCUSGeneratedPrograms){
  if(p.family==identity.family&&p.originalPass==identity.pass&&
     p.vertexSHA512==source->vertexSHA512&&p.fragmentSHA512==source->fragmentSHA512){generated=&p;break;}
 }
 Require(generated!=nullptr,"CUS original shader variant has no full native generated consumer");
 auto program=std::make_shared<Program>();program->generated=generated;program->source=source;
 program->vertexUniformPlan=std::make_unique<NativeCUSProgramUniformPlan>(generated->vertex);
 program->fragmentUniformPlan=std::make_unique<NativeCUSProgramUniformPlan>(generated->fragment);
 program->libraries=impl->Library(lease,*generated,ms);
 Require([program->libraries->vertex newFunctionWithName:@"NativeCUSVertex"]!=nil&&
  [program->libraries->fragment newFunctionWithName:@"NativeCUSFragment"]!=nil,
  "Original CUS compiled stages lack their exact native entrypoints");
 auto result=std::make_shared<NativeCUSProgramConsumer>();result->pipelineLifetime=program;result->family=identity.family;result->originalPass=identity.pass;result->vertexSHA512=source->vertexSHA512;result->fragmentSHA512=source->fragmentSHA512;
 PopulateNativeCUSProgramUniformMetadata(*generated,*result);impl->programs[program.get()]=program;return result;
}
std::shared_ptr<const NativeCUSProgramConsumer> CUSSourceGPU::CompileProgram(RuntimeLease& lease,const NativeCUSProgramSource& input,
 const CUSSourceTarget& target,bool validation,double& ms)
{
 auto actual=LinkOriginalStages(lease,input,ms);
 const auto program=impl->programs.at(actual->pipelineLifetime.get()).lock();Require(bool(program),"Original CUS stage owner expired before target pipeline construction");
 ScreenBlendState blend;blend.enabled=false;impl->Pipeline(lease,*program,target,blend,{true,true,true,true},validation,ms);
 return actual;
}
std::unique_ptr<PreparedCUSSource> CUSSourceGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeCUSSourceFrame> source,
 const CUSSourceTarget& target,const TextureProvider& images,bool validation,double& ms)
{
 if(!source||source->draws.empty())return {};impl->argumentPool.BeginBatch(lease);impl->materials156.BeginBatch(lease);
 auto& trace=ModelTrace();trace.window=TraceFrame(*source);
 struct TraceEnd {~TraceEnd(){ModelTrace().window=false;}} traceEnd;
 const bool sampled=trace.window;const auto prepareStart=sampled?TraceClock::now():TraceClock::time_point{};
 double validationUS=0,publicationUS=0,diagnosticUS=0;
 trace.allocations=trace.allocationBytes=trace.cacheHits=trace.publishedUploads=trace.publishedBytes=0;
 Require(bool(images)&&target.width&&target.height&&target.logicalWidth&&target.logicalHeight,"CUS original target/resource provider is incomplete");
 std::erase_if(impl->buffers,[](const auto& x){return x.second.owner.expired();});std::erase_if(impl->geometry,[](const auto& x){return x.second.owner.expired();});
 impl->geometryReads.Prune();
 auto out=std::unique_ptr<PreparedCUSSource>(new PreparedCUSSource);out->impl->owner=source;
 out->impl->setterProbe176=model176::Probe::Create(source->sourceDrawFrame,target.shadowPolygonOffsetUnitsScale==2.f?"cus_shadow_scale2":"cus");
 const auto preparation154=NextNativePreparation154(impl->preparation154);
 ModelGeometryBatch155 geometryBatch155(lease.Device().maxBufferLength,[&](size_t bytes){return Allocate(lease,bytes);},
  [&](const auto& owner,uint32_t index,const ModelGeometrySlice155& slice){
   auto& cached=impl->geometry.at(owner.get());cached.owner=owner;cached.gpu=slice;
   out->impl->geometry.at(index)=slice;
  });
  impl->uniformPool.BeginBatch(lease);ModelUploadArena146 uniformArena(impl->uniformPool,out->impl->uniformOwners);
 uint64_t originalObjects147=0,preparedRuns147=0;
 size_t priorResourceSetters156=0,materialBindings156=0;
 for(const auto& d:source->draws){
  ValidateNativeCUSSourceDraw(d);auto state=WorldState(d);const auto blend=BlendState(d);
  if(state.cullEnabled&&state.cull==NativeWorldCull::FrontAndBack)continue;
  if(HasNoNativeCUSAttachmentEffects(d))continue; // Same source proof used by the deferred writer; the source record remains retained.
  Require(state.frontPolygonMode==state.backPolygonMode&&state.frontPolygonMode!=NativePolygonMode::Point,"CUS original polygon raster requires a matching native consumer");
  Require(!state.inheritedClipPlane,"CUS shader uses its own original clipDistance0; a generic inherited plane cannot substitute for it");
  const auto validationStart=sampled?TraceClock::now():TraceClock::time_point{};
  NativeCUSRenderReadSet fallbackReads;
  const auto* preparedReads=NativeCUSCPUReadScope::Find(d);
  if(!preparedReads)fallbackReads=ValidateNativeCUSRenderReads(d,impl->geometryReads);
  const auto& reads=preparedReads?*preparedReads:fallbackReads;
  CountPreparationTrace(preparedReads?36:37,1);
  if(sampled)validationUS+=TraceUS(validationStart);
  auto it=impl->programs.find(d.program->pipelineLifetime.get());Require(it!=impl->programs.end(),"CUS draw program belongs to another native compiler owner");auto program=it->second.lock();Require(bool(program),"CUS native program lifetime expired");
  if(d.surface.viewport&&(!d.surface.viewport->width||!d.surface.viewport->height))continue; // Actual observed zero viewport has no raster samples.
  WorldCamera camera=target.camera;if(d.surface.viewport){const auto& v=*d.surface.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
  auto raster=effectstate::Viewport(camera,target.width,target.height,target.logicalWidth,target.logicalHeight,d.surface.scissor);if(!raster)continue;
  PreparedCUSSource::Impl::Draw draw;draw.sequence=d.sequence;draw.state=state;draw.raster=*raster;
  draw.shadowPolygonOffsetUnitsScale=target.shadowPolygonOffsetUnitsScale;
  auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=9;descriptor.maxTextureBindCount=0;descriptor.maxSamplerStateBindCount=0;descriptor.initializeBindings=YES;
  NSError* error=nil;draw.table=impl->argumentPool.Acquire(lease,descriptor,out->impl->argumentOwners,&error);if(draw.table==nil)Require(false,"Cannot allocate CUS original argument table: "+Error(error));lease.Retain(draw.table);
  const auto& u=*d.engineUniforms;
  auto matrices=impl->Cached(lease,u.matrices,u.matrices->bytes.size(),[&](uint8_t* dst){std::memcpy(dst,u.matrices->bytes.data(),u.matrices->bytes.size());});
  auto params=impl->Cached(lease,u.params,u.params->bytes.size(),[&](uint8_t* dst){std::memcpy(dst,u.params->bytes.data(),NativeCUSParamsPrefixBytes);for(size_t row=0;row<NativeCUSTeamRows;++row)if(u.params->writtenTeamRows[row])std::memcpy(dst+NativeCUSParamsPrefixBytes+row*16,u.params->bytes.data()+NativeCUSParamsPrefixBytes+row*16,16);});
  auto instances=impl->Cached(lease,d.instances,d.instances->bytes.size(),[&](uint8_t* dst){std::memcpy(dst,d.instances->bytes.data(),d.instances->bytes.size());});
  const auto publicationStart=sampled?TraceClock::now():TraceClock::time_point{};
  auto transforms=impl->Published(lease,d.modelInputs->transforms),models=impl->Published(lease,d.modelInputs->modelUniforms);
  if(sampled)publicationUS+=TraceUS(publicationStart);
  [draw.table setAddress:instances.gpuAddress atIndex:1];[draw.table setAddress:matrices.gpuAddress atIndex:2];[draw.table setAddress:params.gpuAddress atIndex:3];[draw.table setAddress:transforms.gpuAddress atIndex:4];[draw.table setAddress:models.gpuAddress atIndex:5];
  std::array<TraceImage,12> traceImages{};
  std::array<id<MTLTexture>,12> materialTextures156{};std::array<id<MTLSamplerState>,12> materialSamplers156{};
  NativeCUSSampleTransport transport;uint32_t caps;std::memcpy(&caps,u.params->bytes.data()+12,4);transport.clipFlags=uint32_t(!(caps&1u))|uint32_t(target.invertClipY)<<1|uint32_t(d.clipDistance0)<<2;
  static constexpr const char* samplerNames[]={"texture1","texture2","normalTex","texture1w","texture2w","normalTexw","shadowTex","reflectTex","losMapTex","brdfLUT","noisetex3dcube","envLUT"};
  bool canonicalPair219=ModelSamplerPair219(program->generated->fragment.textureMask);
  for(unsigned slot=0;slot<12;++slot){
   if(!(program->generated->fragment.textureMask&(1u<<slot)))continue;
   const auto& binding=d.textures->samplers[slot];transport.sampleCodes[slot]=NativeCUSSampleCode(Sampling(binding));
   const auto uniform=d.IntUniforms().find(samplerNames[slot]);Require(uniform!=d.IntUniforms().end()&&uniform->second.size()==1&&uniform->second[0]==slot,"CUS original sampler unit changed outside selected source contract");
   if(Empty(binding)){
    if(slot<2)canonicalPair219=false;
    if(slot==6){
     const auto bits=d.IntUniforms().find("bitOptions");
     Require(bits!=d.IntUniforms().end()&&bits->second.size()==1&&!(uint32_t(bits->second[0])&1u),"CUS enabled original shadow branch lacks its actual depth image");
     continue; // Exact source OPTION_SHADOWMAPPING=0 gates every shadow sample/size read.
    }
    transport.emptyMask|=1u<<slot;continue;
   }
   auto image=images(lease,binding,slot);Require(image.texture!=nil&&image.sampler!=nil&&image.owner&&image.requireReadable,"CUS actual image has no paired resource/sampler/producer lifetime");
   const auto expected=slot==7?MTLTextureTypeCube:slot==10?MTLTextureType3D:MTLTextureType2D;Require(image.texture.textureType==expected,"CUS original texture target differs from its actual image");
   if(slot<2)canonicalPair219=canonicalPair219&&CanonicalModelSampler219(binding,image);
   if(image.flipV){Require(expected==MTLTextureType2D,"CUS cube/volume requires its original orientation producer");transport.flipMask|=1u<<slot;}
   if(image.depthImage)transport.depthMask|=1u<<slot;
   if(image.clampDepthCompare){Require(image.depthImage,"Fixed-depth compare semantics require the actual depth image");transport.clampDepthCompareMask|=1u<<slot;}
   if(sampled)traceImages[slot]={image.texture.gpuResourceID._impl,image.sampler.gpuResourceID._impl,unsigned(image.texture.pixelFormat),unsigned(image.texture.width),unsigned(image.texture.height),unsigned(image.texture.mipmapLevelCount)};
   lease.AddAllocation(image.texture);lease.Retain(image.sampler);materialTextures156[slot]=image.texture;materialSamplers156[slot]=image.sampler;priorResourceSetters156+=2;draw.images.push_back(std::move(image));
  }
  if(program->generated->fragment.textureMask){draw.material156=impl->materials156.Get(lease,materialTextures156,materialSamplers156);[draw.table setAddress:draw.material156.gpuAddress atIndex:8];++materialBindings156;}
  const auto uniforms=UploadCUSUniforms(lease,uniformArena,*program,d,transport);
  const auto vertex=uniforms.vertex.Bytes(),fragment=uniforms.fragment.Bytes();
  [draw.table setAddress:uniforms.vertex.Address() atIndex:6];[draw.table setAddress:uniforms.fragment.Address() atIndex:7];
  // The same resolved transport is uploaded above. Every selected color
  // slot must be identity/non-depth for the entire fragment-library variant.
  const bool identityChannels194=NativeCUSIdentityChannels194(program->generated->fragment.textureMask,transport);
  const auto samplerPolicy219=canonicalPair219?ModelSamplerPolicy219::CanonicalPair:ModelSamplerPolicy219::Generic;
  draw.pipeline=impl->Pipeline(lease,*program,target,blend,state.colorWrite,validation,ms,state.alphaToCoverage,identityChannels194,samplerPolicy219);draw.depth=effectstate::Depth(lease,state,target.stencil);lease.Retain(draw.pipeline);lease.Retain(program->libraries->vertex);lease.Retain(program->libraries->fragment);
  ForEachNativeCUSDrawRun147(d.Submissions(),[&](uint32_t first,uint32_t count,const NativeCUSModelSubmission& submission){
   const auto index=impl->Model(lease,submission.geometry,preparation154,out->impl->geometry,geometryBatch155);
   const auto& model=out->impl->geometry[index];
   Require(model.count==submission.originalIndexCount,"CUS GPU model index count differs from original submission");
   draw.objects.push_back({index,first,count});
  });
  originalObjects147+=d.Submissions().size();preparedRuns147+=draw.objects.size();
  if(sampled){const auto begin=TraceClock::now();TraceDraw(*source,d,target,state,transport,traceImages,reads,instances,transforms,models,vertex,fragment);diagnosticUS+=TraceUS(begin);}
  out->impl->draws.push_back(std::move(draw));
 }
 geometryBatch155.Flush();
 if(!impl->reportedMaterial156&&source->sourceDrawFrame>=512&&preparedRuns147){
  const auto& material=impl->materials156.Stats();
  LOG("[NativeModel156Material] prior_resource_setters=%zu material_address_bindings=%zu cache_hits=%zu new_buffers=%zu encoded_resource_slots=%zu encoded_bytes=%zu cache_entries=%zu cache_bytes=%zu cache_allocated_bytes=%zu direct155_loop=1 icb_cache=0 icb_execution=0 geometry_registration_unchanged=1 source_order_unchanged=1",priorResourceSetters156,materialBindings156,material.hits,material.created,material.created*24,material.encodedBytes,impl->materials156.Entries(),impl->materials156.Bytes(),impl->materials156.AllocatedBytes());
  impl->reportedMaterial156=true;
 }
 // Seal exact first-owner order only after all image vectors are stable.
 out->impl->firstOwners=BuildTerrainOwnerRoster(out->impl->draws);
 if(sampled)TraceRecord("perf f=%llu sample=%zu maxSeconds=300 sourceDraws=%zu prepared=%zu prepareCPUus=%.3f validateCPUus=%.3f publishedCPUus=%.3f traceCPUus=%.3f immutableCacheAllocs=%llu immutableCacheBytes=%llu cacheHits=%llu publishedUploads=%llu publishedCopyBytes=%llu",(unsigned long long)source->sourceDrawFrame,trace.budget.samples,source->draws.size(),out->impl->draws.size(),TraceUS(prepareStart)-diagnosticUS,validationUS,publicationUS,diagnosticUS,(unsigned long long)trace.allocations,(unsigned long long)trace.allocationBytes,(unsigned long long)trace.cacheHits,(unsigned long long)trace.publishedUploads,(unsigned long long)trace.publishedBytes);
 if(!impl->reported155&&source->sourceDrawFrame>=512&&preparedRuns147){
  size_t binds=0,logical=0,stored=0,allocated=0;std::set<MTLGPUAddress> pages,cachePages;
  for(const auto& draw:out->impl->draws){MTLGPUAddress previous=0;for(const auto& object:draw.objects){const auto address=out->impl->geometry[object.geometry].vertexAddress;if(previous!=address){++binds;previous=address;}pages.insert(address);}}
  for(const auto& [key,item]:impl->geometry){logical+=item.logicalBytes;if(cachePages.insert(item.gpu.vertexAddress).second){stored+=item.gpu.storage.length;allocated+=item.gpu.storage.allocatedSize;}}
  LOG("[NativeModel155] prepared_runs=%llu unique_geometry=%zu prepared_pages=%zu vertex_bindings=%zu omitted_bindings=%llu cache_models=%zu cache_pages=%zu cache_live_bytes=%zu cache_storage_bytes=%zu cache_allocated_bytes=%zu new_models=%zu new_pages=%zu uploaded_bytes=%zu geometry_size=%zu cache_entry_size=%zu immutable_pages=1 source_order_unchanged=1",
   (unsigned long long)preparedRuns147,out->impl->geometry.size(),pages.size(),binds,(unsigned long long)(preparedRuns147-binds),impl->geometry.size(),cachePages.size(),logical,stored,allocated,geometryBatch155.uploadedModels,geometryBatch155.uploadedPages,geometryBatch155.uploadedBytes,sizeof(ModelGeometrySlice155),sizeof(Impl::Geometry));
  impl->reported155=true;
 }
 uniformArena.Report("CUS",source->sourceDrawFrame,impl->reported146);
 if(!impl->reported147&&source->sourceDrawFrame>=512&&originalObjects147){
  LOG("[NativeModel147] source_draws=%zu original_objects=%llu prepared_runs=%llu omitted_model_lookups=%llu omitted_geometry_registration_calls=%llu instance_order_retained=1",out->impl->draws.size(),(unsigned long long)originalObjects147,(unsigned long long)preparedRuns147,(unsigned long long)(originalObjects147-preparedRuns147),(unsigned long long)(2*(originalObjects147-preparedRuns147)));
  impl->reported147=true;
 }
 return out;
}
} // namespace rendering::metal4
