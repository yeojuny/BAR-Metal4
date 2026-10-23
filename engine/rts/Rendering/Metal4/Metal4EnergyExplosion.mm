/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4EnergyExplosion.h"
#include <map>
#include <tuple>
#include "NativeEnergyExplosionShaders.h"
#include "Rendering/Backend/NativeEnergyExplosionProgram.h"
#import <simd/simd.h>
#include <cmath>
#include <cstring>
namespace rendering::metal4 {
namespace {
struct Uniform {simd_float4x4 cameraViewProj,cameraViewInv;simd_float4 timeInfo;std::array<float,28> parameters{};simd_uint4 options;};
static_assert(sizeof(Uniform)==272);
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* bytes,size_t size){Require(size&&size<=lease.Device().maxBufferLength,"Energy burst buffer exceeds actual device capacity");auto b=[lease.Device()newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared];Require(b!=nil,"Cannot allocate original energy burst buffer");lease.AddAllocation(b);return b;}
}
struct PreparedEnergyExplosion::Impl {
	struct Draw {uint64_t sequence;__strong id<MTLRenderPipelineState> pipeline;__strong id<MTLDepthStencilState> depth;__strong id<MTL4ArgumentTable> args;__strong id<MTLBuffer> indices;NSUInteger count,instances;MTLIndexType indexType;};
	std::vector<Draw> draws;
};
PreparedEnergyExplosion::PreparedEnergyExplosion():impl(std::make_unique<Impl>()){}
PreparedEnergyExplosion::~PreparedEnergyExplosion()=default;
void PreparedEnergyExplosion::EncodeDependencies(id<MTL4CommandBuffer>)const{}
size_t PreparedEnergyExplosion::DrawCount()const{return impl->draws.size();}
uint64_t PreparedEnergyExplosion::SequenceAt(size_t i)const{return impl->draws.at(i).sequence;}
void PreparedEnergyExplosion::EncodeDraw(size_t i,id<MTL4RenderCommandEncoder> e)const{const auto& d=impl->draws.at(i);[e setRenderPipelineState:d.pipeline];[e setDepthStencilState:d.depth];[e setDepthClipMode:MTLDepthClipModeClip];[e setCullMode:MTLCullModeNone];[e setDepthBias:0 slopeScale:0 clamp:0];[e setTriangleFillMode:MTLTriangleFillModeFill];[e setArgumentTable:d.args atStages:MTLRenderStageVertex|MTLRenderStageFragment];[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:d.count indexType:d.indexType indexBuffer:d.indices.gpuAddress indexBufferLength:d.indices.length instanceCount:d.instances];}
struct EnergyExplosionGPU::Impl {__strong id<MTLLibrary> library=nil;__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;MTLPixelFormat format=MTLPixelFormatInvalid;bool validation=false;
 unsigned samples=1;
 using Programs=std::tuple<id<MTLLibrary>,decltype(pipeline),id<MTLDepthStencilState>>;
 std::map<std::tuple<MTLPixelFormat,bool,unsigned>,Programs> variants;
};
EnergyExplosionGPU::EnergyExplosionGPU():impl(std::make_unique<Impl>()){}
EnergyExplosionGPU::~EnergyExplosionGPU(){if(impl->library!=nil)SetNativeEnergyExplosionConsumerAvailable(false);}
void EnergyExplosionGPU::ClearResources(){}
void EnergyExplosionGPU::Initialize(RuntimeLease& lease,MTLPixelFormat format,bool validation,double& milliseconds,unsigned samples){const auto key=std::make_tuple(format,validation,lease.SampleShadingKey(samples));
 if(auto found=impl->variants.find(key);found!=impl->variants.end()){
  std::tie(impl->library,impl->pipeline,impl->depth)=found->second;
  impl->format=format;impl->validation=validation;impl->samples=samples;return;
 }const auto start=Clock::now();auto l=[MTL4LibraryDescriptor new];l.source=String(energy::SHADER);l.name=@"Original test31657 energy explosion actual NoGS";auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;l.options=options;NSError* error=nil;auto library=lease.NewLibrary(l,&error);Require(library!=nil,"Cannot compile original energy burst MSL: "+Error(error));auto v=[MTL4LibraryFunctionDescriptor new];v.library=library;v.name=@"EnergyVertexMain";auto f=[MTL4LibraryFunctionDescriptor new];f.library=library;f.name=@"EnergyFragmentMain";auto d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=samples;d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;auto a=d.colorAttachments[0];a.pixelFormat=format;a.blendingState=MTL4BlendStateEnabled;a.sourceRGBBlendFactor=a.sourceAlphaBlendFactor=MTLBlendFactorOne;a.destinationRGBBlendFactor=a.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;auto po=[MTL4PipelineOptions new];po.shaderValidation=ShaderValidationMode(validation);d.options=po;auto pipeline=lease.NewRenderPipeline(d,&error);Require(pipeline!=nil,"Cannot initialize actual energy burst PSO: "+Error(error));auto dd=[MTLDepthStencilDescriptor new];dd.depthCompareFunction=MTLCompareFunctionLessEqual;dd.depthWriteEnabled=NO;auto depth=[lease.Device()newDepthStencilStateWithDescriptor:dd];Require(depth!=nil,"Cannot initialize original energy burst depth");impl->library=library;impl->pipeline=pipeline;impl->depth=depth;impl->format=format;impl->validation=validation;impl->samples=samples;impl->variants.emplace(key,Impl::Programs{impl->library,impl->pipeline,impl->depth});milliseconds+=Milliseconds(start);SetNativeEnergyExplosionConsumerAvailable(true);}
std::unique_ptr<PreparedEnergyExplosion> EnergyExplosionGPU::Prepare(RuntimeLease& lease,const NativeEnergyExplosionFrame& frame,const WorldCamera& view,MTLPixelFormat color,MTLPixelFormat depth,bool validation,double& milliseconds,unsigned samples){if(frame.draws.empty())return {};Require(depth==MTLPixelFormatDepth32Float||depth==MTLPixelFormatDepth32Float_Stencil8,"Energy burst requires real scene depth");Require(view.clipDepth!=ClipDepthConvention::Unsupported,"Energy burst requires actual camera clip convention");for(const auto* m:{&frame.cameraViewProjection,&frame.cameraViewInverse})for(float v:*m)Require(std::isfinite(v),"Energy burst camera is nonfinite");for(float v:frame.timeInfo)Require(std::isfinite(v),"Energy burst time is nonfinite");for(const auto& d:frame.draws)ValidateNativeEnergyExplosionDraw(d);Initialize(lease,color,validation,milliseconds,samples);lease.Retain(impl->library);lease.Retain(impl->pipeline);lease.Retain(impl->depth);auto out=std::unique_ptr<PreparedEnergyExplosion>(new PreparedEnergyExplosion);
	for(const auto& d:frame.draws){if(!d.instanceCount)continue;auto slots=Buffer(lease,d.vertexSlots->bytes.data(),d.vertexSlots->bytes.size());auto instances=Buffer(lease,d.instances->bytes.data(),d.instances->bytes.size());auto indices=Buffer(lease,d.indices->bytes.data(),d.indices->bytes.size());Uniform u{};std::memcpy(&u.cameraViewProj,frame.cameraViewProjection.data(),64);std::memcpy(&u.cameraViewInv,frame.cameraViewInverse.data(),64);std::memcpy(&u.timeInfo,frame.timeInfo.data(),16);for(const auto& spec:NativeEnergyUniforms)std::copy(d.uniforms.at(std::string(spec.name)).begin(),d.uniforms.at(std::string(spec.name)).end(),u.parameters.begin()+spec.offset);u.options.x=view.clipDepth==ClipDepthConvention::MinusOneToOne;auto uniforms=Buffer(lease,&u,sizeof(u));auto ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=3;ad.initializeBindings=YES;NSError* error=nil;auto args=[lease.Device()newArgumentTableWithDescriptor:ad error:&error];Require(args!=nil,"Cannot bind original energy burst buffers: "+Error(error));lease.Retain(args);[args setAddress:slots.gpuAddress atIndex:0];[args setAddress:instances.gpuAddress+NSUInteger(d.firstInstance)*64 atIndex:1];[args setAddress:uniforms.gpuAddress atIndex:2];out->impl->draws.push_back({d.sequence,impl->pipeline,impl->depth,args,indices,d.indices->layout.elements,d.instanceCount,d.indices->layout.stride==2?MTLIndexTypeUInt16:MTLIndexTypeUInt32});}return out;
}
}
