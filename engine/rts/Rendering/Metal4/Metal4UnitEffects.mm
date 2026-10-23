/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4UnitEffects.h"
#include "NativeAirjetsShaders.h"
#include "NativeOrbShaders.h"
#include "NativeParalyzeShaders.h"
#include <limits>
#include <simd/simd.h>
#include <cstring>
#include <map>
namespace rendering::metal4 {namespace {
struct Uniform {simd_float4x4 vp,reflectionVP,viewInv;simd_float4 time,alpha;simd_uint4 options;};static_assert(sizeof(Uniform)==240);
MTLBlendOperation Equation(ScreenBlendEquation x){switch(x){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Airjets blend equation differs");}
id<MTLBuffer> Upload(RuntimeLease& lease,const void* data,size_t size){Require(data&&size&&size<=lease.Device().maxBufferLength,"Airjets original upload exceeds device");auto b=[lease.Device()newBufferWithBytes:data length:size options:MTLResourceStorageModeShared];Require(b!=nil,"Airjets original buffer allocation failed");lease.AddAllocation(b);return b;}
}
struct PreparedUnitEffect::Impl {
 std::shared_ptr<const NativeUnitEffectInput> source;NativeWorldDrawState state;effectstate::Raster raster;bool visible=false;
 __strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;__strong id<MTL4ArgumentTable> arguments=nil;__strong id<MTLBuffer> indices=nil;
 struct ModelDraw {__strong id<MTL4ArgumentTable> arguments=nil;__strong id<MTLBuffer> indices=nil;uint32_t count=0;};
 std::vector<ModelDraw> models;
 std::vector<PreparedScreenExternalImage> external;
};
PreparedUnitEffect::PreparedUnitEffect():impl(std::make_unique<Impl>()){}PreparedUnitEffect::~PreparedUnitEffect()=default;
void PreparedUnitEffect::Encode(id<MTL4RenderCommandEncoder> e)const{
 const auto& d=*impl;if(!d.visible)return;for(const auto& image:d.external)image.requireReadable();effectstate::RasterState(e,d.state,d.raster);[e setDepthClipMode:d.state.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];
 [e setCullMode:d.state.cullEnabled?(d.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack):MTLCullModeNone];[e setRenderPipelineState:d.pipeline];[e setDepthStencilState:d.depth];if(d.source->paralyze){for(const auto& model:d.models){[e setArgumentTable:model.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:model.count indexType:MTLIndexTypeUInt32 indexBuffer:model.indices.gpuAddress indexBufferLength:model.indices.length instanceCount:1 baseVertex:0 baseInstance:0];}return;}[e setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
 [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:d.source->count indexType:MTLIndexTypeUInt16 indexBuffer:d.indices.gpuAddress+size_t(d.source->indexOffset)*2 indexBufferLength:d.indices.length-size_t(d.source->indexOffset)*2 instanceCount:d.source->instanceCount baseVertex:d.source->baseVertex baseInstance:d.source->baseInstance];
}
struct UnitEffectsGPU::Impl {
 struct Cached{std::weak_ptr<const NativeBufferSnapshot> source;uint64_t revision=0;__strong id<MTLBuffer> buffer=nil;};
 struct ModelMesh {std::weak_ptr<const WorldModel> source;__strong id<MTLBuffer> vertices=nil,indices=nil;uint32_t count=0;};
 std::map<const WorldModel*,ModelMesh> models;
 const ModelMesh& Mesh(RuntimeLease& lease,const std::shared_ptr<const WorldModel>& source){
  auto& mesh=models[source.get()];if(mesh.source.lock()!=source){
   struct Vertex {float x,y,z;uint32_t piece;};static_assert(sizeof(Vertex)==16);
   std::vector<Vertex> vertices;std::vector<uint32_t> indices;
   for(const auto& piece:source->pieces){Require(vertices.size()+piece.vertices.size()<=std::numeric_limits<uint32_t>::max(),"Paralyze original model exceeds index space");const auto base=uint32_t(vertices.size());for(const auto& v:piece.vertices)vertices.push_back({v.position[0],v.position[1],v.position[2],uint32_t(v.boneIds[0]&255)});for(auto index:piece.indices){Require(index<piece.vertices.size(),"Paralyze model index invalid");indices.push_back(base+index);}}
   Require(!vertices.empty()&&!indices.empty()&&indices.size()<=std::numeric_limits<uint32_t>::max(),"Paralyze actual source geometry missing or oversized");mesh.vertices=Upload(lease,vertices.data(),vertices.size()*sizeof(Vertex));mesh.indices=Upload(lease,indices.data(),indices.size()*sizeof(uint32_t));mesh.count=indices.size();mesh.source=source;
  }else{lease.AddAllocation(mesh.vertices);lease.AddAllocation(mesh.indices);}return mesh;
 }
 std::map<uint64_t,Cached> buffers;std::shared_ptr<const void> compiled;__strong id<MTLLibrary> library=nil;
 std::map<std::array<unsigned,19>,id<MTLRenderPipelineState>> pipelines;
 id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& s){auto& c=buffers[s->object];if(c.revision!=s->revision||c.buffer==nil){c.buffer=Upload(lease,s->bytes.data(),s->bytes.size());c.source=s;c.revision=s->revision;}else lease.AddAllocation(c.buffer);return c.buffer;}
 id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,const ScreenBlendState& b,const std::array<bool,4>& mask,bool validation,double& ms,bool orb,bool paralyze,unsigned samples,bool coverage){
 std::array<unsigned,19> key{unsigned(color),unsigned(depth),unsigned(stencil),unsigned(validation),unsigned(b.enabled),unsigned(b.sourceColor),unsigned(b.destinationColor),unsigned(b.sourceAlpha),unsigned(b.destinationAlpha),unsigned(b.colorEquation),unsigned(b.alphaEquation),unsigned(mask[0]),unsigned(mask[1]),unsigned(mask[2]),unsigned(mask[3]),unsigned(orb),unsigned(paralyze),lease.SampleShadingKey(samples),unsigned(coverage)};if(auto i=pipelines.find(key);i!=pipelines.end())return i->second;
 auto start=Clock::now();NSError* error=nil;if(library==nil){auto d=[MTL4LibraryDescriptor new];d.source=String(std::string(AirjetsMSL)+OrbMSL+ParalyzeMSL);d.name=@"Original31657 Airjets";auto o=[MTLCompileOptions new];o.mathMode=MTLMathModeSafe;o.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;d.options=o;library=lease.NewLibrary(d,&error);Require(library!=nil,"Airjets original MSL failed: "+Error(error));}
 auto d=[MTL4RenderPipelineDescriptor new];auto vs=[MTL4LibraryFunctionDescriptor new],fs=[MTL4LibraryFunctionDescriptor new];vs.library=fs.library=library;vs.name=paralyze?@"ParalyzeVertexMain":orb?@"OrbVertexMain":@"AirjetsVertex";fs.name=paralyze?@"ParalyzeFragment":orb?@"OrbFragmentMain":@"AirjetsFragment";d.vertexFunctionDescriptor=vs;d.fragmentFunctionDescriptor=fs;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
 auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=b.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=effectstate::Factor(b.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(b.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(b.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(b.destinationAlpha);a.rgbBlendOperation=Equation(b.colorEquation);a.alphaBlendOperation=Equation(b.alphaEquation);a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
 auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;auto p=lease.NewRenderPipeline(d,&error);Require(p!=nil,"Airjets pipeline failed: "+Error(error));pipelines.emplace(key,p);ms+=Milliseconds(start);return p;
 }
};
UnitEffectsGPU::UnitEffectsGPU():impl(std::make_unique<Impl>()){}UnitEffectsGPU::~UnitEffectsGPU(){if(NativeUnitEffectsConsumer()==impl->compiled)SetNativeUnitEffectsConsumer({});}
void UnitEffectsGPU::Initialize(RuntimeLease& lease,MTLPixelFormat color,bool validation,double& ms,unsigned samples){
 ScreenBlendState b;b.enabled=true;b.sourceColor=b.sourceAlpha=b.destinationColor=b.destinationAlpha=ScreenBlendFactor::One;
 impl->Pipeline(lease,color,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,b,{true,true,true,true},validation,ms,false,false,samples,false);impl->Pipeline(lease,MTLPixelFormatRGBA8Unorm,MTLPixelFormatDepth32Float,MTLPixelFormatInvalid,b,{true,true,true,true},validation,ms,false,false,1,false);
 b.sourceColor=b.sourceAlpha=ScreenBlendFactor::SourceAlpha;b.destinationColor=b.destinationAlpha=ScreenBlendFactor::OneMinusSourceAlpha;
 impl->Pipeline(lease,color,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,b,{true,true,true,true},validation,ms,true,false,samples,false);impl->Pipeline(lease,MTLPixelFormatRGBA8Unorm,MTLPixelFormatDepth32Float,MTLPixelFormatInvalid,b,{true,true,true,true},validation,ms,true,false,1,false);
 impl->Pipeline(lease,color,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,b,{true,true,true,true},validation,ms,false,true,samples,false);
 impl->compiled=std::make_shared<int>(0);SetNativeUnitEffectsConsumer(impl->compiled);
}
void UnitEffectsGPU::ClearResources(){impl->buffers.clear();impl->models.clear();}
std::shared_ptr<PreparedUnitEffect> UnitEffectsGPU::Prepare(RuntimeLease& lease,const NativeWorldDrawCommand& c,ScreenGPU& images,const PreparedScreenFrame* producers,const WorldCamera& view,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& ms,unsigned samples){
 Require(c.unitEffect!=nullptr,"Airjets source absent");ValidateNativeWorldDrawCommand(c);const auto& d=*c.unitEffect;Require(d.compiled==impl->compiled,"Airjets compiled consumer changed");std::erase_if(impl->buffers,[](const auto& b){return b.second.source.expired();});std::erase_if(impl->models,[](const auto& b){return b.second.source.expired();});
 Require(!c.world.inheritedClipPlane&&c.world.frontPolygonMode==NativePolygonMode::Fill&&c.world.backPolygonMode==NativePolygonMode::Fill,"Airjets original inherited raster profile differs");
 auto result=std::shared_ptr<PreparedUnitEffect>(new PreparedUnitEffect);auto& out=*result->impl;out.source=c.unitEffect;out.state=c.world;auto camera=view;if(c.surface.viewport){const auto& v=*c.surface.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
 auto raster=effectstate::Viewport(camera,width,height,logicalWidth,logicalHeight,c.surface.scissor);if(!raster||(c.world.cullEnabled&&c.world.cull==NativeWorldCull::FrontAndBack))return result;out.raster=*raster;out.visible=true;

 Uniform u{};const auto* m=d.original->matrices->bytes.data();const auto* p=d.original->params->bytes.data();std::memcpy(&u.vp,m+5*64,64);std::memcpy(&u.reflectionVP,m+15*64,64);std::memcpy(&u.viewInv,m+7*64,64);std::memcpy(&u.time,p+16,16);uint32_t caps;std::memcpy(&caps,p+12,4);u.alpha=simd_make_float4(c.surface.alphaTest.reference,unsigned(c.surface.alphaTest.compare),0,0);u.options=simd_make_uint4(d.reflectionPass,caps&1u,c.surface.alphaTest.enabled,d.paralyze?unsigned(d.empRework):d.orb?0u:(unsigned(d.images[0]->flipV)|(unsigned(d.images[1]->flipV)<<1)));auto ub=Upload(lease,&u,sizeof(u));
 if(d.paralyze){
  for(size_t row=0;row<d.submissions.size();++row){const auto pub=ReadNativeParalyzePublished(d,row);const auto& mesh=impl->Mesh(lease,d.submissions[row].model);auto transforms=Upload(lease,pub.transforms.data(),pub.transforms.size()*32),model=Upload(lease,pub.model.data(),pub.model.size());
   auto desc=[MTL4ArgumentTableDescriptor new];desc.maxBufferBindCount=5;desc.initializeBindings=YES;NSError* error=nil;auto arguments=[lease.Device()newArgumentTableWithDescriptor:desc error:&error];Require(arguments!=nil,"Paralyze model argument table failed: "+Error(error));[arguments setAddress:mesh.vertices.gpuAddress atIndex:0];[arguments setAddress:transforms.gpuAddress atIndex:2];[arguments setAddress:ub.gpuAddress atIndex:3];[arguments setAddress:model.gpuAddress atIndex:4];out.models.push_back({arguments,mesh.indices,mesh.count});lease.Retain(arguments);
  }
  out.pipeline=impl->Pipeline(lease,color,depth,stencil,c.surface.blend,c.world.colorWrite,validation,ms,false,true,samples,c.world.alphaToCoverage);out.depth=effectstate::Depth(lease,c.world,stencil);lease.Retain(out.pipeline);lease.Retain(impl->library);return result;
 }
 const auto vertices=impl->Buffer(lease,d.vertices),instances=impl->Buffer(lease,d.instances);out.indices=impl->Buffer(lease,d.indices);id<MTLBuffer> pub=nil;if(d.orb){const auto values=ReadNativeOrbPublished(d);pub=Upload(lease,values.data(),values.size()*sizeof(NativeOrbPublished));}else{const auto values=ReadNativeAirjetPublished(d);pub=Upload(lease,values.data(),values.size()*sizeof(NativeAirjetPublished));}
 auto desc=[MTL4ArgumentTableDescriptor new];desc.maxBufferBindCount=4;desc.maxTextureBindCount=2;desc.maxSamplerStateBindCount=2;desc.initializeBindings=YES;NSError* error=nil;out.arguments=[lease.Device()newArgumentTableWithDescriptor:desc error:&error];Require(out.arguments!=nil,"Airjets argument table failed: "+Error(error));
 [out.arguments setAddress:vertices.gpuAddress atIndex:0];[out.arguments setAddress:instances.gpuAddress atIndex:1];[out.arguments setAddress:pub.gpuAddress atIndex:2];[out.arguments setAddress:ub.gpuAddress atIndex:3];
 // These actual named images are immutable; shared uploader owns mip generation.
 const auto consumer=NativePostprocessConsumer{NativePostprocessStage::World,c.sequence,0};
 if(!d.orb)for(unsigned n=0;n<2;++n){auto sample=images.PrepareImage(lease,d.images[n],d.samplers[n],producers,consumer,[&](PreparedScreenExternalImage image){out.external.push_back(std::move(image));});[out.arguments setTexture:sample.texture.gpuResourceID atIndex:n];[out.arguments setSamplerState:sample.sampler.gpuResourceID atIndex:n];}
 out.pipeline=impl->Pipeline(lease,color,depth,stencil,c.surface.blend,c.world.colorWrite,validation,ms,d.orb,false,samples,c.world.alphaToCoverage);out.depth=effectstate::Depth(lease,c.world,stencil);lease.Retain(out.arguments);lease.Retain(out.pipeline);lease.Retain(impl->library);return result;
}
}
