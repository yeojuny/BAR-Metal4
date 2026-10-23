/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4ProjectileEffects.h"
#include <map>
#include <tuple>
#include "NativeProjectileEffectShaders.h"
#include "Rendering/Backend/NativeProjectileEffectRecorder.h"
#include "Rendering/Backend/NativeProjectileEffectPrograms.h"
#import <simd/simd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
namespace rendering::metal4 {
namespace {
struct Uniform {simd_float4x4 cameraViewProjection,cameraViewInverse;simd_float4 timeInfo;std::array<float,42> parameters{};simd_uint4 options;};
static_assert(sizeof(Uniform)==336);
struct Image {std::shared_ptr<const NativeProjectileTexture> owner;__strong id<MTLTexture> texture=nil;bool initialized=false;};
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* data,size_t bytes){Require(bytes&&bytes<=lease.Device().maxBufferLength,"Original projectile buffer exceeds actual device capacity");auto buffer=[lease.Device() newBufferWithBytes:data length:bytes options:MTLResourceStorageModeShared];Require(buffer!=nil,"Cannot allocate original projectile GPU buffer");lease.AddAllocation(buffer);return buffer;}
simd_float4x4 Matrix(const std::array<float,16>& source){simd_float4x4 m;std::memcpy(&m,source.data(),sizeof(m));return m;}
std::shared_ptr<Image> Upload(RuntimeLease& lease,std::shared_ptr<const NativeProjectileTexture> owner){Require(owner&&owner->width&&owner->height&&owner->width<=lease.Capabilities().maxTextureDimension2D&&owner->height<=lease.Capabilities().maxTextureDimension2D&&owner->rgba.size()==size_t(owner->width)*owner->height*4,"Original projectile image exceeds actual extent/bytes contract");auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:owner->width height:owner->height mipmapped:YES];d.storageMode=MTLStorageModeShared;d.usage=MTLTextureUsageShaderRead;auto texture=[lease.Device() newTextureWithDescriptor:d];Require(texture!=nil,"Cannot allocate original projectile image");texture.label=String(owner->sourceName);[texture replaceRegion:MTLRegionMake2D(0,0,owner->width,owner->height) mipmapLevel:0 withBytes:owner->rgba.data() bytesPerRow:size_t(owner->width)*4];auto image=std::make_shared<Image>();image->owner=std::move(owner);image->texture=texture;return image;}
}
struct PreparedProjectileEffects::Impl {
	struct Draw {uint64_t sequence=0;__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;__strong id<MTL4ArgumentTable> arguments=nil;__strong id<MTLBuffer> indices=nil;NSUInteger instances=0,vertexCount=0;MTLIndexType indexType=MTLIndexTypeUInt16;bool indexed=false;};
	std::vector<Draw> draws;
	std::vector<std::shared_ptr<Image>> pending;
};
PreparedProjectileEffects::PreparedProjectileEffects():impl(std::make_unique<Impl>()){}
PreparedProjectileEffects::~PreparedProjectileEffects()=default;
void PreparedProjectileEffects::EncodeDependencies(id<MTL4CommandBuffer> command) const{if(impl->pending.empty())return;auto e=[command computeCommandEncoder];Require(e!=nil,"Cannot encode original projectile image mips");for(const auto& image:impl->pending)[e generateMipmapsForTexture:image->texture];[e barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];}
size_t PreparedProjectileEffects::DrawCount() const{return impl->draws.size();}
uint64_t PreparedProjectileEffects::SequenceAt(size_t n) const{return impl->draws.at(n).sequence;}
void PreparedProjectileEffects::EncodeDraw(size_t n,id<MTL4RenderCommandEncoder> e) const{const auto& d=impl->draws.at(n);[e setRenderPipelineState:d.pipeline];[e setDepthStencilState:d.depth];[e setDepthClipMode:MTLDepthClipModeClip];[e setCullMode:MTLCullModeNone];[e setDepthBias:0 slopeScale:0 clamp:0];[e setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];if(d.indexed)[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:d.vertexCount indexType:d.indexType indexBuffer:d.indices.gpuAddress indexBufferLength:d.indices.length instanceCount:d.instances];else[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:d.vertexCount instanceCount:d.instances];}
void PreparedProjectileEffects::DidComplete() const{for(const auto& image:impl->pending)image->initialized=true;}
struct ProjectileEffectsGPU::Impl {
	__strong id<MTLLibrary> library=nil;
	std::array<__strong id<MTLRenderPipelineState>,7> pipelines{};
	__strong id<MTLDepthStencilState> depth=nil;
	MTLPixelFormat format=MTLPixelFormatInvalid;bool validation=false;
 unsigned samples=1;
 using Programs=std::tuple<id<MTLLibrary>,decltype(pipelines),id<MTLDepthStencilState>>;
 std::map<std::tuple<MTLPixelFormat,bool,unsigned>,Programs> variants;

	std::map<std::shared_ptr<const NativeProjectileTexture>,std::shared_ptr<Image>,std::owner_less<std::shared_ptr<const NativeProjectileTexture>>> images;
};
ProjectileEffectsGPU::ProjectileEffectsGPU():impl(std::make_unique<Impl>()){}
ProjectileEffectsGPU::~ProjectileEffectsGPU(){if(impl->library!=nil)SetNativeProjectileEffectsConsumerAvailable(false);}
void ProjectileEffectsGPU::ClearResources(){impl->images.clear();}
void ProjectileEffectsGPU::Initialize(RuntimeLease& lease,MTLPixelFormat colorFormat,bool validation,double& milliseconds,unsigned samples)
{
	const auto key=std::make_tuple(colorFormat,validation,lease.SampleShadingKey(samples));
 if(auto found=impl->variants.find(key);found!=impl->variants.end()){
  std::tie(impl->library,impl->pipelines,impl->depth)=found->second;
  impl->format=colorFormat;impl->validation=validation;impl->samples=samples;return;
 }const auto start=Clock::now();
	auto descriptor=[MTL4LibraryDescriptor new];descriptor.source=String(projectile::SHADER);descriptor.name=@"Original test31657 Beam/Plasma instanced shader families";auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;descriptor.options=options;NSError* error=nil;auto library=lease.NewLibrary(descriptor,&error);Require(library!=nil,"Cannot compile original projectile MSL: "+Error(error));
	constexpr std::array<const char*,7> names{"Beam","BeamFlare","BeamGlow","BeamPulse","Plasma","PlasmaCrossSection","PlasmaGlow"};std::array<__strong id<MTLRenderPipelineState>,7> pipelines{};
	for(size_t n=0;n<names.size();++n){auto v=[MTL4LibraryFunctionDescriptor new];v.library=library;v.name=String(std::string(names[n])+"VertexMain");auto f=[MTL4LibraryFunctionDescriptor new];f.library=library;f.name=String(std::string(names[n])+"FragmentMain");auto d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=samples;d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;auto a=d.colorAttachments[0];a.pixelFormat=colorFormat;a.blendingState=MTL4BlendStateEnabled;a.sourceRGBBlendFactor=a.sourceAlphaBlendFactor=a.destinationRGBBlendFactor=a.destinationAlphaBlendFactor=MTLBlendFactorOne;auto o=[MTL4PipelineOptions new];o.shaderValidation=ShaderValidationMode(validation);d.options=o;auto p=lease.NewRenderPipeline(d,&error);Require(p!=nil,"Cannot compile original projectile PSO: "+Error(error));pipelines[n]=p;}
	auto dd=[MTLDepthStencilDescriptor new];dd.depthCompareFunction=MTLCompareFunctionLessEqual;dd.depthWriteEnabled=NO;auto depth=[lease.Device() newDepthStencilStateWithDescriptor:dd];Require(depth!=nil,"Cannot initialize original projectile depth state");impl->library=library;impl->pipelines=std::move(pipelines);impl->depth=depth;impl->format=colorFormat;impl->validation=validation;impl->samples=samples;impl->variants.emplace(key,Impl::Programs{impl->library,impl->pipelines,impl->depth});milliseconds+=Milliseconds(start);SetNativeProjectileEffectsConsumerAvailable(true);
}
std::unique_ptr<PreparedProjectileEffects> ProjectileEffectsGPU::Prepare(RuntimeLease& lease,const NativeProjectileEffectFrame& frame,const WorldCamera& viewCamera,MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,bool validation,double& milliseconds,unsigned samples)
{
	if(frame.draws.empty())return {};Require(depthFormat==MTLPixelFormatDepth32Float||depthFormat==MTLPixelFormatDepth32Float_Stencil8,"Original projectile effects require scene depth");Require(viewCamera.clipDepth!=ClipDepthConvention::Unsupported,"Original projectile effects require actual camera clip convention");for(const auto* m:{&frame.cameraViewProjection,&frame.cameraViewInverse})for(float f:*m)Require(std::isfinite(f),"Original projectile camera source nonfinite");for(float f:frame.timeInfo)Require(std::isfinite(f),"Original projectile timeInfo source nonfinite");for(const auto& d:frame.draws)ValidateNativeProjectileEffectDraw(d);Initialize(lease,colorFormat,validation,milliseconds,samples);lease.Retain(impl->library);lease.Retain(impl->depth);
	auto result=std::unique_ptr<PreparedProjectileEffects>(new PreparedProjectileEffects);
	for(const auto& d:frame.draws){if(!d.instanceCount)continue;auto mesh=Buffer(lease,d.vertices->bytes.data(),d.vertices->bytes.size());auto instances=Buffer(lease,d.instances->bytes.data(),d.instances->bytes.size());auto indices=Buffer(lease,d.indices->bytes.data(),d.indices->bytes.size());Uniform u{};u.cameraViewProjection=Matrix(frame.cameraViewProjection);u.cameraViewInverse=Matrix(frame.cameraViewInverse);std::memcpy(&u.timeInfo,frame.timeInfo.data(),16);for(size_t n=0;n<NativeProjectileParameterNames.size();++n)if(auto found=d.parameters.find(std::string(NativeProjectileParameterNames[n]));found!=d.parameters.end())u.parameters[n]=found->second;u.options=simd_make_uint4(viewCamera.clipDepth==ClipDepthConvention::MinusOneToOne,0,0,0);auto uniform=Buffer(lease,&u,sizeof(u));
		auto ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=3;ad.maxTextureBindCount=1;ad.initializeBindings=YES;NSError* error=nil;auto args=[lease.Device() newArgumentTableWithDescriptor:ad error:&error];Require(args!=nil,"Cannot allocate original projectile arguments: "+Error(error));lease.Retain(args);[args setAddress:mesh.gpuAddress atIndex:0];[args setAddress:instances.gpuAddress atIndex:1];[args setAddress:uniform.gpuAddress atIndex:2];
		if(d.texture){auto& image=impl->images[d.texture];if(!image)image=Upload(lease,d.texture);lease.AddAllocation(image->texture);[args setTexture:image->texture.gpuResourceID atIndex:0];if(!image->initialized&&std::find(result->impl->pending.begin(),result->impl->pending.end(),image)==result->impl->pending.end())result->impl->pending.push_back(image);}
		auto pipeline=impl->pipelines[static_cast<unsigned>(d.program)];lease.Retain(pipeline);result->impl->draws.push_back({d.sequence,pipeline,impl->depth,args,indices,d.instanceCount,d.vertexCount,d.indices->layout.stride==2?MTLIndexTypeUInt16:MTLIndexTypeUInt32,d.indexed});
	}return result;
}
}
