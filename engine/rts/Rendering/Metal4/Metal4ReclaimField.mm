/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4ReclaimField.h"
#include "NativeReclaimShaders131.h"
#include "NativeSourceLineRasterShaders130.h"
#include "NativeReclaimSourceLines131.h"
#include "Rendering/Backend/NativeWorldDrawGeometry.h"
#include "Rendering/Backend/NativeWorldDrawRecorder.h"
#include "Rendering/Backend/NativeReclaimSourceLines131.h"
#include <cstring>
#include <map>
#include <cstdio>
#include <cstdlib>
namespace rendering::metal4 {
namespace {
using Uniform=reclaimSource131::Uniform;
MTLBlendOperation Equation(ScreenBlendEquation x){switch(x){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Reclaim field blend equation differs");}
id<MTLBuffer> Upload131(RuntimeLease& lease,const void* data,size_t bytes){Require(bytes&&bytes<=lease.Device().maxBufferLength,"Reclaim actual input/mesh exceeds device limit");auto b=[lease.Device()newBufferWithBytes:data length:bytes options:MTLResourceStorageModeShared];Require(b,"Cannot retain Reclaim immutable input/mesh");lease.AddAllocation(b);return b;}
#include "Metal4ReclaimSourceInputs131.inc"
struct ReclaimProbe131 {
 const char* path=std::getenv("BAR_NATIVE_RECLAIM_SOURCE_PROBE_FILE");uint64_t fills=0,gpu=0,cpu=0,segments=0,vertices=0,uniformBytes=0,msaa=0;
 bool Enabled()const{return path&&*path;}
 ~ReclaimProbe131(){if(Enabled())if(auto f=std::fopen(path,"w")){std::fprintf(f,"fillDraws,sourceGPUDraws,cpuFallbackDraws,sourceSegments,sourceVertices,uniformBytes,sourceGPUMultisampleDraws\n%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",(unsigned long long)fills,(unsigned long long)gpu,(unsigned long long)cpu,(unsigned long long)segments,(unsigned long long)vertices,(unsigned long long)uniformBytes,(unsigned long long)msaa);std::fclose(f);}}
};
ReclaimProbe131& Probe131(){static ReclaimProbe131 p;return p;}
}
struct PreparedReclaimField::Impl {
 struct Draw {uint64_t sequence=0;std::shared_ptr<const NativeReclaimFieldInput> source;NativeWorldDrawState state;effectstate::Raster raster;
  __strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;__strong id<MTL4ArgumentTable> arguments=nil;bool visible=false;
  __strong id<MTLBuffer> indices=nil;NSUInteger count=0,instances=0;unsigned kind=0;};
 std::vector<Draw> draws;size_t next=0;bool completed=false;
};
PreparedReclaimField::PreparedReclaimField():impl(std::make_unique<Impl>()){}
PreparedReclaimField::~PreparedReclaimField()=default;
size_t PreparedReclaimField::DrawCount()const{return impl->draws.size();}
uint64_t PreparedReclaimField::SequenceAt(size_t n)const{return impl->draws.at(n).sequence;}
void PreparedReclaimField::EncodeDraw(size_t n,id<MTL4RenderCommandEncoder> e){
 Require(e&&n==impl->next&&!impl->completed,"Reclaim paired consumer must consume each actual source ordinal once");const auto& d=impl->draws.at(n);++impl->next;if(!d.visible)return;
 effectstate::RasterState(e,d.state,d.raster);[e setDepthClipMode:d.state.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];[e setCullMode:!d.state.cullEnabled?MTLCullModeNone:d.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];
 [e setRenderPipelineState:d.pipeline];[e setDepthStencilState:d.depth];[e setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
 if(d.kind==1)[e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:d.instances];
 else if(d.kind==2)[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:d.count indexType:MTLIndexTypeUInt32 indexBuffer:d.indices.gpuAddress indexBufferLength:d.indices.length];
 else [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:d.source->first vertexCount:d.source->count];
}
void PreparedReclaimField::DidComplete(){Require(!impl->completed&&impl->next==impl->draws.size(),"Reclaim retirement preceded actual paired draws");impl->completed=true;}
struct ReclaimFieldGPU::Impl {
 struct Cached {std::weak_ptr<const NativeBufferSnapshot> source;uint64_t revision=0;__strong id<MTLBuffer> buffer=nil;};
 std::map<uint64_t,Cached> buffers;__strong id<MTLLibrary> library=nil;
 std::map<std::array<unsigned,16>,id<MTLRenderPipelineState>> pipelines;
 __strong id<MTLLibrary> lineLibrary=nil;
 id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& s){
  Require(s&&!s->bytes.empty()&&s->bytes.size()<=lease.Device().maxBufferLength,"Reclaim source buffer exceeds device bound");auto& b=buffers[s->object];if(b.revision!=s->revision||b.buffer==nil){b.buffer=Upload131(lease,s->bytes.data(),s->bytes.size());b.revision=s->revision;b.source=s;}else lease.AddAllocation(b.buffer);return b.buffer;
 }
 id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,MTLPixelFormat color,const ScreenBlendState& blend,const std::array<bool,4>& mask,bool validation,double& ms,unsigned samples,bool coverage,unsigned kind=0){
  std::array<unsigned,16> key{unsigned(color),unsigned(validation),unsigned(blend.enabled),unsigned(blend.sourceColor),unsigned(blend.destinationColor),unsigned(blend.sourceAlpha),unsigned(blend.destinationAlpha),unsigned(blend.colorEquation),unsigned(blend.alphaEquation),unsigned(mask[0]),unsigned(mask[1]),unsigned(mask[2]),unsigned(mask[3]),lease.SampleShadingKey(samples),unsigned(coverage),kind};if(auto found=pipelines.find(key);found!=pipelines.end())return found->second;
  const auto start=Clock::now();NSError* error=nil;
  const auto compile=[&](NSString* name,const std::string& source){auto d=[MTL4LibraryDescriptor new];d.name=name;d.source=String(source);auto o=[MTLCompileOptions new];o.mathMode=MTLMathModeSafe;o.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;d.options=o;auto next=lease.NewLibrary(d,&error);Require(next,"Cannot compile original paired Reclaim source: "+Error(error));return next;};
  if(!library)library=compile(@"Original31657 Reclaim field",ReclaimFieldShader131);
  if(kind&&!lineLibrary)lineLibrary=compile(@"Original Reclaim source GPU outline131",std::string(ReclaimFieldShader131)+SourceLineRasterShader130+ReclaimSourceLinesShader131);
  auto d=[MTL4RenderPipelineDescriptor new];auto vs=[MTL4LibraryFunctionDescriptor new],fs=[MTL4LibraryFunctionDescriptor new];vs.library=kind?lineLibrary:library;fs.library=kind==1&&samples==1?lineLibrary:library;
  vs.name=kind==1?(samples>1?@"ReclaimSourceVertexMSAA131":@"ReclaimSourceVertex131"):kind==2?@"ReclaimRasterVertex131":@"ReclaimFieldVertex";fs.name=kind==1&&samples==1?@"ReclaimSourceFragment131":@"ReclaimFieldFragment";
  d.vertexFunctionDescriptor=vs;d.fragmentFunctionDescriptor=fs;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
  auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
  auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;auto p=lease.NewRenderPipeline(d,&error);Require(p,"Cannot compile Reclaim actual paired pipeline: "+Error(error));pipelines.emplace(key,p);ms+=Milliseconds(start);return p;
 }
};
ReclaimFieldGPU::ReclaimFieldGPU():impl(std::make_unique<Impl>()){}
ReclaimFieldGPU::~ReclaimFieldGPU(){SetNativeReclaimFieldConsumerAvailable(false);}
void ReclaimFieldGPU::Initialize(RuntimeLease& l,MTLPixelFormat f,bool validation,double& ms,unsigned samples){ScreenBlendState blend;blend.enabled=true;blend.sourceColor=blend.sourceAlpha=ScreenBlendFactor::SourceAlpha;blend.destinationColor=blend.destinationAlpha=ScreenBlendFactor::OneMinusSourceAlpha;impl->Pipeline(l,f,blend,{true,true,true,true},validation,ms,samples,false);SetNativeReclaimFieldConsumerAvailable(true);}
void ReclaimFieldGPU::ClearResources(){impl->buffers.clear();}
std::shared_ptr<PreparedReclaimField> ReclaimFieldGPU::Prepare(RuntimeLease& lease,const NativeWorldDrawSelection167& selected,uint64_t drawFrame,const WorldCamera& view,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& ms,unsigned samples){
 selected.RequireFamily(NativeWorldFamily167::ReclaimField,drawFrame);if(selected.empty())return {};std::erase_if(impl->buffers,[](const auto& b){return b.second.source.expired();});Require(depth==MTLPixelFormatDepth32Float||depth==MTLPixelFormatDepth32Float_Stencil8,"Reclaim requires actual common world target");auto out=std::shared_ptr<PreparedReclaimField>(new PreparedReclaimField);
 for(const auto& c:selected){ValidateNativeWorldDrawCommand(c);const auto& d=*c.reclaimField;Require(d.sourceDrawFrame==drawFrame&&c.stage==NativeWorldDrawStage::PreUnit,"Reclaim belongs to another source frame/stage");PreparedReclaimField::Impl::Draw ready;ready.sequence=c.sequence;ready.source=c.reclaimField;ready.state=c.world;
  if(d.edges)ready.state=reclaimSource131::LineState(ready.state);
  else Require(!c.world.inheritedClipPlane&&c.world.frontPolygonMode==NativePolygonMode::Fill&&c.world.backPolygonMode==NativePolygonMode::Fill,"Reclaim fill inherited polygon/clip state differs");
  auto camera=view;if(c.surface.viewport){const auto& v=*c.surface.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}auto raster=effectstate::Viewport(camera,width,height,logicalWidth,logicalHeight,c.surface.scissor);
  if(!raster||(!d.edges&&c.world.cullEnabled&&c.world.cull==NativeWorldCull::FrontAndBack)){out->impl->draws.push_back(std::move(ready));continue;}ready.raster=*raster;
  const std::array<double,4> viewport{raster->viewport.originX,raster->viewport.originY,raster->viewport.width,raster->viewport.height};auto line=d.edges?reclaimSource131::Admit(d,c.world,viewport,width,height,samples):std::optional<reclaimSource131::Raster>{};ready.kind=d.edges?(line?1:2):0;
  id<MTLBuffer> vb=nil,sb=nil;
  if(ready.kind==2){const auto vertices=EvaluateNativeReclaimFieldOutline(d);auto command=MakeNativeWorldDrawCommand(c.stage,NativeDrawPrimitive::Lines,vertices,c.surface,c.world);auto mesh=BuildNativeWorldDrawGeometry(command,viewport,width,height,samples);if(mesh.indices.empty()){out->impl->draws.push_back(std::move(ready));continue;}vb=Upload131(lease,mesh.vertices.data(),mesh.vertices.size()*sizeof(NativeWorldDrawVertex));ready.indices=Upload131(lease,mesh.indices.data(),mesh.indices.size()*sizeof(uint32_t));ready.count=mesh.indices.size();if(mesh.fullViewport)ready.raster.viewport={0,0,double(width),double(height),0,1};}
  else {vb=impl->Buffer(lease,d.vertices);sb=impl->Buffer(lease,d.params.snapshot);if(line){ready.instances=d.count/2;ready.raster.viewport={0,0,double(width),double(height),0,1};}}
  auto uniform=reclaimSource131::OriginalUniform(d,c.surface.alphaTest);auto input=PrepareReclaimSourceInputs131(lease,uniform,line);auto desc=[MTL4ArgumentTableDescriptor new];desc.maxBufferBindCount=line?4:3;desc.initializeBindings=YES;NSError* error=nil;ready.arguments=[lease.Device()newArgumentTableWithDescriptor:desc error:&error];Require(ready.arguments,"Cannot create Reclaim paired argument table: "+Error(error));
  [ready.arguments setAddress:vb.gpuAddress atIndex:0];if(sb)[ready.arguments setAddress:sb.gpuAddress+d.params.offset atIndex:1];[ready.arguments setAddress:input.buffer.gpuAddress atIndex:2];if(line)[ready.arguments setAddress:input.buffer.gpuAddress+ReclaimSourceInputs131::RasterOffset atIndex:3];
  ready.pipeline=impl->Pipeline(lease,color,c.surface.blend,c.world.colorWrite,validation,ms,samples,c.world.alphaToCoverage,ready.kind);ready.depth=effectstate::Depth(lease,ready.state,stencil);lease.Retain(ready.pipeline);lease.Retain(ready.arguments);lease.Retain(impl->library);if(ready.kind)lease.Retain(impl->lineLibrary);ready.visible=true;
  auto& p=Probe131();if(p.Enabled()){if(!d.edges)++p.fills;else if(line){++p.gpu;p.segments+=d.count/2;p.vertices+=uint64_t(d.count/2)*4;p.uniformBytes+=304;if(samples>1)++p.msaa;}else ++p.cpu;}
  out->impl->draws.push_back(std::move(ready));
 }
 return out;
}
}
