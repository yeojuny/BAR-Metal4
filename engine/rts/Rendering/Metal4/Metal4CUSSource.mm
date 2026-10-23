/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4CUSSource.h"
#include "Rendering/Backend/NativeUITextureHistory.h"
#include "NativeCUSGeneratedPrograms.h"
#include "Metal4NativeEffectState.h"
#include "Rendering/Backend/NativeCUSSourceIdentity.h"
#include "Rendering/Backend/NativeCUSRenderInputs.h"
#include <cstring>
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
};
id<MTLBuffer> Allocate(RuntimeLease& lease,size_t bytes)
{
 Require(bytes&&bytes<=lease.Device().maxBufferLength,"CUS original buffer exceeds actual native device capacity");
 auto b=[lease.Device()newBufferWithLength:bytes options:MTLResourceStorageModeShared];Require(b!=nil,"Cannot allocate CUS original buffer");lease.AddAllocation(b);return b;
}
id<MTLBuffer> Upload(RuntimeLease& lease,const void* data,size_t bytes){auto b=Allocate(lease,bytes);std::memcpy(b.contents,data,bytes);return b;}
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
#include "Metal4ModelTrace.inc"
}

struct PreparedCUSSource::Impl {
 struct Object {__strong id<MTLBuffer> vertices=nil,indices=nil;uint32_t count=0,instance=0;};
 struct Draw {
  uint64_t sequence=0;__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;__strong id<MTL4ArgumentTable> table=nil;
  NativeWorldDrawState state;effectstate::Raster raster;std::vector<Object> objects;
  std::vector<CUSSourceImage> images;
  float shadowPolygonOffsetUnitsScale=1.f;
  bool encoded=false;
 };
 std::shared_ptr<const NativeCUSSourceFrame> owner;std::vector<Draw> draws;bool dependenciesEncoded=false,completed=false;
};
PreparedCUSSource::PreparedCUSSource():impl(std::make_unique<Impl>()){}
PreparedCUSSource::~PreparedCUSSource()=default;
size_t PreparedCUSSource::DrawCount()const{return impl->draws.size();}
uint64_t PreparedCUSSource::SequenceAt(size_t n)const{return impl->draws.at(n).sequence;}
void PreparedCUSSource::EncodeDependencies(id<MTL4CommandBuffer> command)
{
 Require(command!=nil&&!impl->dependenciesEncoded,"CUS source dependencies repeated or missing command");
 std::set<const void*> seen;
 for(const auto& d:impl->draws)for(const auto& image:d.images)if(seen.insert(image.owner.get()).second&&image.encodeDependencies)image.encodeDependencies(command);
 impl->dependenciesEncoded=true;
}
void PreparedCUSSource::EncodeDraw(size_t n,id<MTL4RenderCommandEncoder> encoder)
{
 Require(encoder!=nil&&impl->dependenciesEncoded&&!impl->completed,"CUS source draw lacks its actual dependency stage");auto& d=impl->draws.at(n);
 Require(!d.encoded,"CUS source draw was already encoded");
 for(const auto& image:d.images)if(image.requireReadable)image.requireReadable();
 effectstate::RasterState(encoder,d.state,d.raster);
 if(d.state.polygonOffset)[encoder setDepthBias:d.state.offsetUnits*d.shadowPolygonOffsetUnitsScale slopeScale:d.state.offsetFactor clamp:0];
 [encoder setCullMode:!d.state.cullEnabled?MTLCullModeNone:d.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];
 [encoder setTriangleFillMode:d.state.frontPolygonMode==NativePolygonMode::Line?MTLTriangleFillModeLines:MTLTriangleFillModeFill];
 [encoder setRenderPipelineState:d.pipeline];[encoder setDepthStencilState:d.depth];
 for(const auto& object:d.objects){
  [d.table setAddress:object.vertices.gpuAddress atIndex:0];[encoder setArgumentTable:d.table atStages:MTLRenderStageVertex|MTLRenderStageFragment];
  [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:object.count indexType:MTLIndexTypeUInt32 indexBuffer:object.indices.gpuAddress indexBufferLength:object.indices.length instanceCount:1 baseVertex:0 baseInstance:object.instance];
 }
 d.encoded=true;
}
void PreparedCUSSource::DidComplete()
{
 Require(!impl->completed&&impl->dependenciesEncoded&&std::all_of(impl->draws.begin(),impl->draws.end(),[](const auto& d){return d.encoded;}),"CUS source cannot complete an unencoded/repeated submission");
 std::set<const void*> seen;
 for(const auto& d:impl->draws)for(const auto& image:d.images)if(seen.insert(image.owner.get()).second&&image.didComplete)image.didComplete();impl->completed=true;
}

struct CUSSourceGPU::Impl {
 std::map<const NativeCUSGeneratedProgram*,std::shared_ptr<const Libraries>> libraries;
 std::map<std::vector<uint64_t>,id<MTLRenderPipelineState>> pipelines;
 std::unordered_map<const void*,std::weak_ptr<const Program>> programs;
 struct Buffer {std::weak_ptr<const void> owner;__strong id<MTLBuffer> value=nil;};
 struct Geometry {std::weak_ptr<const WorldModel> owner;__strong id<MTLBuffer> vertices=nil,indices=nil;uint32_t count=0;};
 std::unordered_map<const void*,Buffer> buffers;std::unordered_map<const WorldModel*,Geometry> geometry;
 std::shared_ptr<const Libraries> Library(RuntimeLease& lease,const NativeCUSGeneratedProgram& p,double& ms)
 {
  if(auto it=libraries.find(&p);it!=libraries.end())return it->second;
  auto out=std::make_shared<Libraries>();const auto start=Clock::now();
  for(bool vertex:{true,false}){
   auto d=[MTL4LibraryDescriptor new];d.name=String(std::string("Original CUS ")+p.family+" pass"+std::to_string(p.originalPass));d.source=String(vertex?p.vertex.metalSource:p.fragment.metalSource);
   auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;d.options=options;
   NSError* error=nil;auto library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile full original CUS native stage: "+Error(error));
   if(vertex)out->vertex=library;else out->fragment=library;
  }
  ms+=Milliseconds(start);libraries.emplace(&p,out);return out;
 }
 id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const Program& p,const CUSSourceTarget& target,const ScreenBlendState& blend,const std::array<bool,4>& mask,bool validation,double& ms,bool coverage=false)
 {
  auto key=PipelineKey(p.generated,target,blend,mask,validation);key.push_back(coverage);key.push_back(target.combinedColor);key.push_back(lease.SampleShadingEnabled(target.samples));if(auto it=pipelines.find(key);it!=pipelines.end())return it->second;
  const auto start=Clock::now();auto d=[MTL4RenderPipelineDescriptor new];auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];
  v.library=p.libraries->vertex;v.name=@"NativeCUSVertex";f.library=p.libraries->fragment;f.name=@"NativeCUSFragment";
  d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.rasterSampleCount=target.samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
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
  return Cached(lease,source,source->capacityElements*source->stride,[&](uint8_t* dst){
   if(ModelTrace().window)++ModelTrace().publishedUploads;
   for(const auto& range:NativeCUSObservedRanges(*source))for(size_t offset=range.offset;offset<range.offset+range.size;){
    const size_t page=offset/NativeCUSPublishedPage::Bytes,local=offset%NativeCUSPublishedPage::Bytes;
    const size_t bytes=std::min(range.offset+range.size-offset,NativeCUSPublishedPage::Bytes-local);
    std::memcpy(dst+offset,source->pages.at(page)->bytes.data()+local,bytes);offset+=bytes;
    if(ModelTrace().window)ModelTrace().publishedBytes+=bytes;
   }
  });
 }
 Geometry& Model(RuntimeLease& lease,std::shared_ptr<const WorldModel> source)
 {
  auto& item=geometry[source.get()];if(item.owner.expired()){
   auto data=BuildNativeCUSRenderGeometry(source);Require(!data.vertices.empty()&&!data.indices.empty(),"CUS submitted original model has no indexed geometry");
   auto vertices=Upload(lease,data.vertices.data(),data.vertices.size()*sizeof(NativeCUSRenderVertex));auto indices=Upload(lease,data.indices.data(),data.indices.size()*4);
   item={source,vertices,indices,uint32_t(data.indices.size())};
  }else{lease.AddAllocation(item.vertices);lease.AddAllocation(item.indices);}return item;
 }
};
CUSSourceGPU::CUSSourceGPU():impl(std::make_unique<Impl>()){}
CUSSourceGPU::~CUSSourceGPU()=default;
void CUSSourceGPU::ClearResources(){impl->buffers.clear();impl->geometry.clear();std::erase_if(impl->programs,[](const auto& x){return x.second.expired();});}
std::shared_ptr<const NativeCUSProgramConsumer> CUSSourceGPU::LinkOriginalStages(RuntimeLease& lease,const NativeCUSProgramSource& input,double& ms)
{
 auto source=std::make_shared<NativeCUSProgramSource>(input);const auto identity=ValidateNativeCUSProgramSource(*source);
 const NativeCUSGeneratedProgram* generated=nullptr;
 for(const auto& p:NativeCUSGeneratedPrograms){
  if(p.family==identity.family&&p.originalPass==identity.pass&&
     p.vertexSHA512==source->vertexSHA512&&p.fragmentSHA512==source->fragmentSHA512){generated=&p;break;}
 }
 Require(generated!=nullptr,"CUS original shader variant has no full native generated consumer");
 auto program=std::make_shared<Program>();program->generated=generated;program->source=source;program->libraries=impl->Library(lease,*generated,ms);
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
 if(!source||source->draws.empty())return {};
 auto& trace=ModelTrace();trace.window=TraceFrame(*source);
 struct TraceEnd {~TraceEnd(){ModelTrace().window=false;}} traceEnd;
 const bool sampled=trace.window;const auto prepareStart=sampled?TraceClock::now():TraceClock::time_point{};
 double validationUS=0,publicationUS=0,diagnosticUS=0;
 trace.allocations=trace.allocationBytes=trace.cacheHits=trace.publishedUploads=trace.publishedBytes=0;
 Require(bool(images)&&target.width&&target.height&&target.logicalWidth&&target.logicalHeight,"CUS original target/resource provider is incomplete");
 std::erase_if(impl->buffers,[](const auto& x){return x.second.owner.expired();});std::erase_if(impl->geometry,[](const auto& x){return x.second.owner.expired();});
 auto out=std::unique_ptr<PreparedCUSSource>(new PreparedCUSSource);out->impl->owner=source;
 for(const auto& d:source->draws){
  ValidateNativeCUSSourceDraw(d);auto state=WorldState(d);const auto blend=BlendState(d);
  if(state.cullEnabled&&state.cull==NativeWorldCull::FrontAndBack)continue;
  if(HasNoNativeCUSAttachmentEffects(d))continue; // Same source proof used by the deferred writer; the source record remains retained.
  Require(state.frontPolygonMode==state.backPolygonMode&&state.frontPolygonMode!=NativePolygonMode::Point,"CUS original polygon raster requires a matching native consumer");
  Require(!state.inheritedClipPlane,"CUS shader uses its own original clipDistance0; a generic inherited plane cannot substitute for it");
  const auto validationStart=sampled?TraceClock::now():TraceClock::time_point{};
  const auto reads=ValidateNativeCUSRenderReads(d);
  if(sampled)validationUS+=TraceUS(validationStart);
  auto it=impl->programs.find(d.program->pipelineLifetime.get());Require(it!=impl->programs.end(),"CUS draw program belongs to another native compiler owner");auto program=it->second.lock();Require(bool(program),"CUS native program lifetime expired");
  if(d.surface.viewport&&(!d.surface.viewport->width||!d.surface.viewport->height))continue; // Actual observed zero viewport has no raster samples.
  WorldCamera camera=target.camera;if(d.surface.viewport){const auto& v=*d.surface.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
  auto raster=effectstate::Viewport(camera,target.width,target.height,target.logicalWidth,target.logicalHeight,d.surface.scissor);if(!raster)continue;
  PreparedCUSSource::Impl::Draw draw;draw.sequence=d.sequence;draw.state=state;draw.raster=*raster;
  draw.shadowPolygonOffsetUnitsScale=target.shadowPolygonOffsetUnitsScale;
  auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=8;descriptor.maxTextureBindCount=12;descriptor.maxSamplerStateBindCount=12;descriptor.initializeBindings=YES;
  NSError* error=nil;draw.table=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(draw.table!=nil,"Cannot allocate CUS original argument table: "+Error(error));lease.Retain(draw.table);
  const auto& u=*d.engineUniforms;
  auto matrices=impl->Cached(lease,u.matrices,u.matrices->bytes.size(),[&](uint8_t* dst){std::memcpy(dst,u.matrices->bytes.data(),u.matrices->bytes.size());});
  auto params=impl->Cached(lease,u.params,u.params->bytes.size(),[&](uint8_t* dst){std::memcpy(dst,u.params->bytes.data(),NativeCUSParamsPrefixBytes);for(size_t row=0;row<NativeCUSTeamRows;++row)if(u.params->writtenTeamRows[row])std::memcpy(dst+NativeCUSParamsPrefixBytes+row*16,u.params->bytes.data()+NativeCUSParamsPrefixBytes+row*16,16);});
  auto instances=impl->Cached(lease,d.instances,d.instances->bytes.size(),[&](uint8_t* dst){std::memcpy(dst,d.instances->bytes.data(),d.instances->bytes.size());});
  const auto publicationStart=sampled?TraceClock::now():TraceClock::time_point{};
  auto transforms=impl->Published(lease,d.modelInputs->transforms),models=impl->Published(lease,d.modelInputs->modelUniforms);
  if(sampled)publicationUS+=TraceUS(publicationStart);
  [draw.table setAddress:instances.gpuAddress atIndex:1];[draw.table setAddress:matrices.gpuAddress atIndex:2];[draw.table setAddress:params.gpuAddress atIndex:3];[draw.table setAddress:transforms.gpuAddress atIndex:4];[draw.table setAddress:models.gpuAddress atIndex:5];
  std::array<TraceImage,12> traceImages{};
  NativeCUSSampleTransport transport;uint32_t caps;std::memcpy(&caps,u.params->bytes.data()+12,4);transport.clipFlags=uint32_t(!(caps&1u))|uint32_t(target.invertClipY)<<1|uint32_t(d.clipDistance0)<<2;
  static constexpr const char* samplerNames[]={"texture1","texture2","normalTex","texture1w","texture2w","normalTexw","shadowTex","reflectTex","losMapTex","brdfLUT","noisetex3dcube","envLUT"};
  for(unsigned slot=0;slot<12;++slot){
   if(!(program->generated->fragment.textureMask&(1u<<slot)))continue;
   const auto& binding=d.textures->samplers[slot];transport.sampleCodes[slot]=NativeCUSSampleCode(Sampling(binding));
   const auto uniform=d.intUniforms.find(samplerNames[slot]);Require(uniform!=d.intUniforms.end()&&uniform->second.size()==1&&uniform->second[0]==slot,"CUS original sampler unit changed outside selected source contract");
   if(Empty(binding)){
    if(slot==6){
     const auto bits=d.intUniforms.find("bitOptions");
     Require(bits!=d.intUniforms.end()&&bits->second.size()==1&&!(uint32_t(bits->second[0])&1u),"CUS enabled original shadow branch lacks its actual depth image");
     continue; // Exact source OPTION_SHADOWMAPPING=0 gates every shadow sample/size read.
    }
    transport.emptyMask|=1u<<slot;continue;
   }
   auto image=images(lease,binding,slot);Require(image.texture!=nil&&image.sampler!=nil&&image.owner&&image.requireReadable,"CUS actual image has no paired resource/sampler/producer lifetime");
   const auto expected=slot==7?MTLTextureTypeCube:slot==10?MTLTextureType3D:MTLTextureType2D;Require(image.texture.textureType==expected,"CUS original texture target differs from its actual image");
   if(image.flipV){Require(expected==MTLTextureType2D,"CUS cube/volume requires its original orientation producer");transport.flipMask|=1u<<slot;}
   if(image.depthImage)transport.depthMask|=1u<<slot;
   if(image.clampDepthCompare){Require(image.depthImage,"Fixed-depth compare semantics require the actual depth image");transport.clampDepthCompareMask|=1u<<slot;}
   if(sampled)traceImages[slot]={image.texture.gpuResourceID._impl,image.sampler.gpuResourceID._impl,unsigned(image.texture.pixelFormat),unsigned(image.texture.width),unsigned(image.texture.height),unsigned(image.texture.mipmapLevelCount)};
   lease.AddAllocation(image.texture);lease.Retain(image.sampler);[draw.table setTexture:image.texture.gpuResourceID atIndex:slot];[draw.table setSamplerState:image.sampler.gpuResourceID atIndex:slot];draw.images.push_back(std::move(image));
  }
  const auto vertex=PackNativeCUSProgramUniforms(program->generated->vertex,d,*program->source,transport),fragment=PackNativeCUSProgramUniforms(program->generated->fragment,d,*program->source,transport);
  auto vb=Upload(lease,vertex.data(),vertex.size()),fb=Upload(lease,fragment.data(),fragment.size());[draw.table setAddress:vb.gpuAddress atIndex:6];[draw.table setAddress:fb.gpuAddress atIndex:7];
  draw.pipeline=impl->Pipeline(lease,*program,target,blend,state.colorWrite,validation,ms,state.alphaToCoverage);draw.depth=effectstate::Depth(lease,state,target.stencil);lease.Retain(draw.pipeline);lease.Retain(program->libraries->vertex);lease.Retain(program->libraries->fragment);
  for(size_t object=0;object<d.submissions.size();++object){auto& model=impl->Model(lease,d.submissions[object]->geometry);Require(model.count==d.submissions[object]->originalIndexCount,"CUS GPU model index count differs from original submission");draw.objects.push_back({model.vertices,model.indices,model.count,uint32_t(object)});}
  if(sampled){const auto begin=TraceClock::now();TraceDraw(*source,d,target,state,transport,traceImages,reads,instances,transforms,models,vertex,fragment);diagnosticUS+=TraceUS(begin);}
  out->impl->draws.push_back(std::move(draw));
 }
 if(sampled)TraceRecord("perf f=%llu sample=%zu maxSeconds=300 sourceDraws=%zu prepared=%zu prepareCPUus=%.3f validateCPUus=%.3f publishedCPUus=%.3f traceCPUus=%.3f immutableCacheAllocs=%llu immutableCacheBytes=%llu cacheHits=%llu publishedUploads=%llu publishedCopyBytes=%llu",(unsigned long long)source->sourceDrawFrame,trace.budget.samples,source->draws.size(),out->impl->draws.size(),TraceUS(prepareStart)-diagnosticUS,validationUS,publicationUS,diagnosticUS,(unsigned long long)trace.allocations,(unsigned long long)trace.allocationBytes,(unsigned long long)trace.cacheHits,(unsigned long long)trace.publishedUploads,(unsigned long long)trace.publishedBytes);
 return out;
}
} // namespace rendering::metal4
