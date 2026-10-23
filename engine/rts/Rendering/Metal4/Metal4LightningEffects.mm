/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4LightningEffects.h"
#include <map>
#include <tuple>
#include "NativeLightningShaders.h"
#include "Rendering/Backend/NativeLightningEffectRecorder.h"
#include "Rendering/Backend/NativeLightningEffectPrograms.h"
#include "Metal4NativeEffectState.h"
#import <simd/simd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
namespace rendering::metal4 {
namespace {
struct Uniform {simd_float4x4 cameraViewProjection,cameraViewInverse;simd_float4 timeInfo;std::array<float,21> parameters{};simd_uint4 options;simd_float4 alpha;};
static_assert(sizeof(Uniform)==272);
struct Image {std::shared_ptr<const NativeProjectileTexture> owner;__strong id<MTLTexture> texture=nil;bool initialized=false;};
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* data,size_t bytes){Require(bytes&&bytes<=lease.Device().maxBufferLength,"Original projectile buffer exceeds actual device capacity");auto buffer=[lease.Device() newBufferWithBytes:data length:bytes options:MTLResourceStorageModeShared];Require(buffer!=nil,"Cannot allocate original projectile GPU buffer");lease.AddAllocation(buffer);return buffer;}
simd_float4x4 Matrix(const std::array<float,16>& source){simd_float4x4 m;std::memcpy(&m,source.data(),sizeof(m));return m;}
std::shared_ptr<Image> Upload(RuntimeLease& lease,std::shared_ptr<const NativeProjectileTexture> owner){Require(owner&&owner->width&&owner->height&&owner->width<=lease.Capabilities().maxTextureDimension2D&&owner->height<=lease.Capabilities().maxTextureDimension2D&&owner->rgba.size()==size_t(owner->width)*owner->height*4,"Original projectile image exceeds actual extent/bytes contract");auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:owner->width height:owner->height mipmapped:YES];d.storageMode=MTLStorageModeShared;d.usage=MTLTextureUsageShaderRead;auto texture=[lease.Device() newTextureWithDescriptor:d];Require(texture!=nil,"Cannot allocate original projectile image");texture.label=String(owner->sourceName);[texture replaceRegion:MTLRegionMake2D(0,0,owner->width,owner->height) mipmapLevel:0 withBytes:owner->rgba.data() bytesPerRow:size_t(owner->width)*4];auto image=std::make_shared<Image>();image->owner=std::move(owner);image->texture=texture;return image;}
}
struct PreparedLightningEffects::Impl {
	struct Draw {uint64_t sequence=0;__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;__strong id<MTL4ArgumentTable> arguments=nil;__strong id<MTLBuffer> indices=nil;NSUInteger instances=0,indexCount=0;MTLIndexType indexType=MTLIndexTypeUInt16;bool indexed=false;NativeWorldDrawState state;effectstate::Raster raster;};
	std::vector<Draw> draws;
	std::vector<std::shared_ptr<Image>> pending;
};
PreparedLightningEffects::PreparedLightningEffects():impl(std::make_unique<Impl>()){}
PreparedLightningEffects::~PreparedLightningEffects()=default;
void PreparedLightningEffects::EncodeDependencies(id<MTL4CommandBuffer> command) const{if(impl->pending.empty())return;auto e=[command computeCommandEncoder];Require(e!=nil,"Cannot encode original projectile image mips");for(const auto& image:impl->pending)[e generateMipmapsForTexture:image->texture];[e barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];}
size_t PreparedLightningEffects::DrawCount() const{return impl->draws.size();}
uint64_t PreparedLightningEffects::SequenceAt(size_t n) const{return impl->draws.at(n).sequence;}
void PreparedLightningEffects::EncodeDraw(size_t n,id<MTL4RenderCommandEncoder> e) const{const auto& d=impl->draws.at(n);[e setRenderPipelineState:d.pipeline];[e setDepthStencilState:d.depth];effectstate::RasterState(e,d.state,d.raster);[e setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];if(d.indexed)[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:d.indexCount indexType:d.indexType indexBuffer:d.indices.gpuAddress indexBufferLength:d.indices.length instanceCount:d.instances];else[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:d.instances];}
void PreparedLightningEffects::DidComplete() const{for(const auto& image:impl->pending)image->initialized=true;}
struct LightningEffectsGPU::Impl {
	__strong id<MTLLibrary> library=nil;
	std::array<__strong id<MTLRenderPipelineState>,5> pipelines{};
	__strong id<MTLDepthStencilState> depth=nil;
	std::map<std::tuple<MTLPixelFormat,bool,unsigned,unsigned,unsigned,bool>,id<MTLRenderPipelineState>> masks;
	MTLPixelFormat format=MTLPixelFormatInvalid;bool validation=false;
 unsigned samples=1;
 using Programs=std::tuple<id<MTLLibrary>,decltype(pipelines),id<MTLDepthStencilState>>;
 std::map<std::tuple<MTLPixelFormat,bool,unsigned>,Programs> variants;

	std::map<std::shared_ptr<const NativeProjectileTexture>,std::shared_ptr<Image>,std::owner_less<std::shared_ptr<const NativeProjectileTexture>>> images;
 id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,unsigned kind,const std::array<bool,4>& mask,double& ms,bool coverage){unsigned bits=0;for(unsigned n=0;n<4;++n)bits|=unsigned(mask[n])<<n;if(bits==15&&!coverage)return pipelines.at(kind);auto key=std::tuple{format,validation,lease.SampleShadingKey(samples),kind,bits,coverage};if(auto found=masks.find(key);found!=masks.end())return found->second;const auto start=Clock::now();constexpr const char* names[]{"CannonBolt","CannonGlow","CannonImpact","EnvironmentBolt","EnvironmentGlow"};auto v=[MTL4LibraryFunctionDescriptor new];v.library=library;v.name=String(std::string(names[kind])+"VertexMain");auto f=[MTL4LibraryFunctionDescriptor new];f.library=library;f.name=String(std::string(names[kind])+"FragmentMain");auto descriptor=[MTL4RenderPipelineDescriptor new];descriptor.rasterSampleCount=samples;descriptor.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;descriptor.vertexFunctionDescriptor=v;descriptor.fragmentFunctionDescriptor=f;descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;auto a=descriptor.colorAttachments[0];a.pixelFormat=format;a.blendingState=MTL4BlendStateEnabled;a.sourceRGBBlendFactor=a.sourceAlphaBlendFactor=a.destinationRGBBlendFactor=a.destinationAlphaBlendFactor=MTLBlendFactorOne;a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;NSError* error=nil;auto result=lease.NewRenderPipeline(descriptor,&error);Require(result!=nil,"Cannot initialize actual lightning color-mask PSO: "+Error(error));masks.emplace(key,result);ms+=Milliseconds(start);return result;}

};
LightningEffectsGPU::LightningEffectsGPU():impl(std::make_unique<Impl>()){}
LightningEffectsGPU::~LightningEffectsGPU(){if(impl->library!=nil)SetNativeLightningConsumerAvailable(false);}
void LightningEffectsGPU::ClearResources(){impl->images.clear();}
void LightningEffectsGPU::Initialize(RuntimeLease& lease,MTLPixelFormat colorFormat,bool validation,double& milliseconds,unsigned samples)
{
	const auto key=std::make_tuple(colorFormat,validation,lease.SampleShadingKey(samples));
 if(auto found=impl->variants.find(key);found!=impl->variants.end()){
  std::tie(impl->library,impl->pipelines,impl->depth)=found->second;
  impl->format=colorFormat;impl->validation=validation;impl->samples=samples;return;
 }const auto start=Clock::now();
	auto descriptor=[MTL4LibraryDescriptor new];descriptor.source=String(lightning::SHADER);descriptor.name=@"Original test31657 Lightning Cannon/Environmental families";auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;descriptor.options=options;NSError* error=nil;auto library=lease.NewLibrary(descriptor,&error);Require(library!=nil,"Cannot compile original projectile MSL: "+Error(error));
	constexpr std::array<const char*,5> names{"CannonBolt","CannonGlow","CannonImpact","EnvironmentBolt","EnvironmentGlow"};std::array<__strong id<MTLRenderPipelineState>,5> pipelines{};
	for(size_t n=0;n<names.size();++n){auto v=[MTL4LibraryFunctionDescriptor new];v.library=library;v.name=String(std::string(names[n])+"VertexMain");auto f=[MTL4LibraryFunctionDescriptor new];f.library=library;f.name=String(std::string(names[n])+"FragmentMain");auto d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=samples;d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;auto a=d.colorAttachments[0];a.pixelFormat=colorFormat;a.blendingState=MTL4BlendStateEnabled;a.sourceRGBBlendFactor=a.sourceAlphaBlendFactor=a.destinationRGBBlendFactor=a.destinationAlphaBlendFactor=MTLBlendFactorOne;auto o=[MTL4PipelineOptions new];o.shaderValidation=ShaderValidationMode(validation);d.options=o;auto p=lease.NewRenderPipeline(d,&error);Require(p!=nil,"Cannot compile original projectile PSO: "+Error(error));pipelines[n]=p;}
	auto dd=[MTLDepthStencilDescriptor new];dd.depthCompareFunction=MTLCompareFunctionLessEqual;dd.depthWriteEnabled=NO;auto depth=[lease.Device() newDepthStencilStateWithDescriptor:dd];Require(depth!=nil,"Cannot initialize original projectile depth state");impl->library=library;impl->pipelines=std::move(pipelines);impl->depth=depth;impl->format=colorFormat;impl->validation=validation;impl->samples=samples;impl->variants.emplace(key,Impl::Programs{impl->library,impl->pipelines,impl->depth});milliseconds+=Milliseconds(start);SetNativeLightningConsumerAvailable(true);
}
std::unique_ptr<PreparedLightningEffects> LightningEffectsGPU::Prepare(RuntimeLease& lease,const NativeLightningEffectFrame& frame,const WorldCamera& viewCamera,std::uint32_t width,std::uint32_t height,std::uint32_t logicalWidth,std::uint32_t logicalHeight,MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,bool validation,double& milliseconds,unsigned samples)
{
	if(frame.draws.empty())return {};Require(depthFormat==MTLPixelFormatDepth32Float||depthFormat==MTLPixelFormatDepth32Float_Stencil8,"Original projectile effects require scene depth");Require(viewCamera.clipDepth!=ClipDepthConvention::Unsupported,"Original projectile effects require actual camera clip convention");for(const auto* m:{&frame.cameraViewProjection,&frame.cameraViewInverse})for(float f:*m)Require(std::isfinite(f),"Original projectile camera source nonfinite");for(float f:frame.timeInfo)Require(std::isfinite(f),"Original projectile timeInfo source nonfinite");for(const auto& d:frame.draws)ValidateNativeLightningEffectDraw(d);Initialize(lease,colorFormat,validation,milliseconds,samples);lease.Retain(impl->library);lease.Retain(impl->depth);
	auto result=std::unique_ptr<PreparedLightningEffects>(new PreparedLightningEffects);
	for(const auto& d:frame.draws){if(!d.instanceCount)continue;auto raster=effectstate::Viewport(viewCamera,width,height,logicalWidth,logicalHeight,d.surface.scissor);if(!raster)continue;auto mesh=Buffer(lease,d.vertices->bytes.data(),d.vertices->bytes.size());auto instances=Buffer(lease,d.instances->bytes.data(),d.instances->bytes.size());auto indices=d.indices?Buffer(lease,d.indices->bytes.data(),d.indices->bytes.size()):nil;Uniform u{};u.cameraViewProjection=Matrix(frame.cameraViewProjection);u.cameraViewInverse=Matrix(frame.cameraViewInverse);std::memcpy(&u.timeInfo,frame.timeInfo.data(),16);for(size_t n=0;n<NativeLightningParameterNames.size();++n)if(auto found=d.parameters.find(std::string(NativeLightningParameterNames[n]));found!=d.parameters.end())u.parameters[n]=found->second;u.options=simd_make_uint4(viewCamera.clipDepth==ClipDepthConvention::MinusOneToOne,d.surface.alphaTest.enabled,uint32_t(d.surface.alphaTest.compare),0);u.alpha=simd_make_float4(d.surface.alphaTest.reference,0,0,0);auto uniform=Buffer(lease,&u,sizeof(u));
		auto ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=3;ad.maxTextureBindCount=1;ad.initializeBindings=YES;NSError* error=nil;auto args=[lease.Device() newArgumentTableWithDescriptor:ad error:&error];Require(args!=nil,"Cannot allocate original projectile arguments: "+Error(error));lease.Retain(args);[args setAddress:mesh.gpuAddress atIndex:0];[args setAddress:instances.gpuAddress+NSUInteger(d.firstInstance)*96 atIndex:1];[args setAddress:uniform.gpuAddress atIndex:2];
		if(d.texture){auto& image=impl->images[d.texture];if(!image)image=Upload(lease,d.texture);lease.AddAllocation(image->texture);[args setTexture:image->texture.gpuResourceID atIndex:0];if(!image->initialized&&std::find(result->impl->pending.begin(),result->impl->pending.end(),image)==result->impl->pending.end())result->impl->pending.push_back(image);}
		auto pipeline=impl->Pipeline(lease,static_cast<unsigned>(d.program),d.world.colorWrite,milliseconds,d.world.alphaToCoverage);auto depth=effectstate::Depth(lease,d.world,stencilFormat);lease.Retain(pipeline);result->impl->draws.push_back({d.sequence,pipeline,depth,args,indices,d.instanceCount,d.indexCount,!d.indices||d.indices->layout.stride==2?MTLIndexTypeUInt16:MTLIndexTypeUInt32,d.indexed,d.world,*raster});
	}return result;
}
}
