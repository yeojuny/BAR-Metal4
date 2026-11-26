/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4BumpWater.h"
#include "Metal4InterventionMath.h"
#include "Metal4WorldBCDecode.h"
#include "NativeBumpWaterShaders.h"
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include "System/Log/ILog.h"
#include <bit>
#include <cmath>
#include <map>
#include <tuple>

namespace rendering::metal4 {
namespace {
uint32_t Levels(uint32_t w,uint32_t h) {return std::bit_width(std::max(w,h));}
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* bytes,size_t size,const char* name) {
	Require(bytes&&size,"Original water buffer is empty");auto result=[lease.Device() newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared];
	Require(result!=nil,"Cannot allocate original water buffer");result.label=String(name);lease.AddAllocation(result);return result;
}
id<MTL4ArgumentTable> Arguments(RuntimeLease& lease,uint32_t buffers,uint32_t textures,uint32_t samplers) {
	auto d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=buffers;d.maxTextureBindCount=textures;d.maxSamplerStateBindCount=samplers;d.initializeBindings=YES;
	auto out=[lease.Device() newArgumentTableWithDescriptor:d error:nil];Require(out!=nil,"Cannot allocate original water argument table");lease.Retain(out);return out;
}
id<MTLTexture> Texture(RuntimeLease& lease,MTLPixelFormat format,uint32_t w,uint32_t h,uint32_t levels,MTLStorageMode storage,MTLTextureUsage usage,const char* name) {
	auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:levels>1];d.mipmapLevelCount=levels;d.storageMode=storage;d.usage=usage;
	auto out=[lease.Device() newTextureWithDescriptor:d];Require(out!=nil,"Cannot allocate original water image");out.label=String(name);return out;
}
id<MTLSamplerState> Sampler(RuntimeLease& lease,MTLSamplerMinMagFilter min,MTLSamplerMinMagFilter mag,
	MTLSamplerMipFilter mip,MTLSamplerAddressMode wrap,uint32_t anisotropy=1,bool compare=false) {
	auto d=[MTLSamplerDescriptor new];d.supportArgumentBuffers=YES;d.minFilter=min;d.magFilter=mag;d.mipFilter=mip;d.sAddressMode=d.tAddressMode=wrap;
	d.maxAnisotropy=anisotropy;d.borderColor=MTLSamplerBorderColorOpaqueWhite;d.compareFunction=compare?MTLCompareFunctionLessEqual:MTLCompareFunctionNever;
	auto out=[lease.Device() newSamplerStateWithDescriptor:d];Require(out!=nil,"Cannot allocate original water sampler");lease.Retain(out);return out;
}
MTLPixelFormat Format(WorldTextureFormat f) {
	switch(f){case WorldTextureFormat::RGBA8Unorm:return MTLPixelFormatRGBA8Unorm;case WorldTextureFormat::BGRA8Unorm:return MTLPixelFormatBGRA8Unorm;
	case WorldTextureFormat::BC1RGBA:return MTLPixelFormatBC1_RGBA;case WorldTextureFormat::BC2RGBA:return MTLPixelFormatBC2_RGBA;case WorldTextureFormat::BC3RGBA:return MTLPixelFormatBC3_RGBA;
	case WorldTextureFormat::BC4Unorm:return MTLPixelFormatBC4_RUnorm;case WorldTextureFormat::BC5Unorm:return MTLPixelFormatBC5_RGUnorm;
	case WorldTextureFormat::BC7RGBA:return MTLPixelFormatBC7_RGBAUnorm;case WorldTextureFormat::BC7RGBASRGB:return MTLPixelFormatBC7_RGBAUnorm_sRGB;}
	throw std::invalid_argument("Unknown original water bitmap format");
}
using MaterialBits=std::array<uint32_t,48>;
using WaterMaterialValues=std::array<std::pair<const char*,std::array<float,4>>,12>;
WaterMaterialValues WaterValues(const NativeBumpWaterUniforms& u){
 return {{{"mapMidSpecularPower",u.mapMidSpecularPower},{"screenInverseTextureInverse",u.screenInverseTextureInverse},
 {"texGenPlane",u.texGenPlane},{"shadingPlane",u.shadingPlane},{"surfaceColor",u.surfaceColor},{"diffuseColorFactor",u.diffuseColorFactor},
 {"specularColorFactor",u.specularColorFactor},{"sunDirectionAmbient",u.sunDirectionAmbient},{"fresnelDistortion",u.fresnelDistortion},
 {"blurPerlin",u.blurPerlin},{"perlinWave",u.perlinWave},{"waveCaustic",u.waveCaustic}}};
}
MaterialBits WaterBits(const NativeBumpWaterUniforms& u){
 MaterialBits bits{};unsigned i=0;for(const auto& field:WaterValues(u))for(float value:field.second)bits[i++]=std::bit_cast<uint32_t>(value);return bits;
}
std::string LiteralWaterVec(const std::array<float,4>& values){
 std::string result="float4(";for(unsigned i=0;i<4;++i){if(i)result+=",";result+="as_type<float>("+std::to_string(std::bit_cast<uint32_t>(values[i]))+"u)";}return result+")";
}
unsigned WaterVariant(){
 static const unsigned value=[] {
  const char* text=std::getenv("BAR_NATIVE_BUMPWATER_SURFACE_VARIANT");
  if(!text||!*text)return 6u;char* end=nullptr;const auto n=std::strtoul(text,&end,10);return end&&!*end&&n<7?unsigned(n):0u;
 }();
 return value;
}
std::string WaterVariantSource(uint32_t flags,const NativeBumpWaterUniforms& u,unsigned profile){
 std::string defs="#define NATIVE_WATER_FLAGS "+std::to_string(flags)+"\n";
 if(profile==1||profile>=3)defs+="#define NATIVE_WATER_FIXED_FLIPS "+std::to_string(u.flags[1])+"u\n";
 std::string source=NATIVE_BUMP_WATER_SHADERS;
 if(profile<4)return defs+source;
 const auto begin=source.find("fragment float4 BumpWaterFragment"),end=source.find("struct WaterUpdateVertex",begin);
 Require(begin!=std::string::npos&&end!=std::string::npos,"BumpWater source boundary changed");
 std::string body=source.substr(begin,end-begin);
 auto replace=[&](std::string from,const std::string& to,bool must=false){
  size_t at=0,count=0;while((at=body.find(from,at))!=std::string::npos){body.replace(at,from.size(),to);at+=to.size();++count;}
  if(must)Require(count==1,"Unexpected BumpWater MVK-style expression: "+from);
 };
 if(profile==6){
  replace("normalize(octave1+octave2+octave3+octave4)","WaterNormalizeMVK(octave1+octave2+octave3+octave4)",true);
  replace("normalize(i.eyeVec)","WaterNormalizeMVK(i.eyeVec)",true);
  replace("normalize(-i.ligVec)","WaterNormalizeMVK(-i.ligVec)",true);
  replace("pow(max(dot(reflectDir,eVec),0.0),u.mapMidSpecularPower.w)","powr(max(dot(reflectDir,eVec),0.0),u.mapMidSpecularPower.w)",true);
  replace("pow(i.position.z,80.0)","powr(i.position.z,80.0)",true);
  replace("pow(angle,u.fresnelDistortion.z)","powr(angle,u.fresnelDistortion.z)",true);
  replace("float diffuse=pow(max(dot(normal,sunLow),0.0),3.0)*u.diffuseColorFactor.w;",
          "float diffuseBase=max(dot(normal,sunLow),0.0);\n float diffuse=(diffuseBase*u.diffuseColorFactor.w)*diffuseBase*diffuseBase;",true);
  body="float3 WaterNormalizeMVK(float3 x){return x*rsqrt(fma(x.x,x.x,fma(x.y,x.y,x.z*x.z)));}\n"+body;
 }
 for(const auto& field:WaterValues(u))replace(std::string("u.")+field.first,LiteralWaterVec(field.second));
 return defs+source.substr(0,begin)+body+source.substr(end);
}
struct PipelineKey {
	uint32_t flags=0;MTLPixelFormat color=MTLPixelFormatInvalid,depth=MTLPixelFormatInvalid,stencil=MTLPixelFormatInvalid;bool validate=false;unsigned samples=1;
	auto operator<=>(const PipelineKey&) const=default;
};
id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,id<MTLLibrary> library,const char* vs,const char* fs,
	MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool blend,bool constantAlpha,bool validation,unsigned samples=1,id<MTLLibrary> fragmentLibrary=nil) {
	auto d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=samples;auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=library;f.library=fragmentLibrary?fragmentLibrary:library;v.name=String(vs);f.name=String(fs);
	d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
	d.colorAttachments[0].pixelFormat=color; // Metal4 obtains depth/stencil formats from actual attachments.
	(void)depth;(void)stencil;
	if(blend){auto a=d.colorAttachments[0];a.blendingState=MTL4BlendStateEnabled;a.rgbBlendOperation=a.alphaBlendOperation=MTLBlendOperationAdd;
		a.sourceRGBBlendFactor=a.sourceAlphaBlendFactor=constantAlpha?MTLBlendFactorBlendAlpha:MTLBlendFactorSourceAlpha;
		a.destinationRGBBlendFactor=a.destinationAlphaBlendFactor=constantAlpha?MTLBlendFactorOneMinusBlendAlpha:MTLBlendFactorOneMinusSourceAlpha;
	}else d.colorAttachments[0].blendingState=MTL4BlendStateDisabled;
	auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
	NSError* error=nil;auto p=lease.NewRenderPipeline(d,&error);Require(p!=nil,"Cannot compile original water pipeline: "+Error(error));return p;
}
MTL4RenderPassDescriptor* Pass(id<MTLTexture> texture,MTLLoadAction load) {
	auto p=[MTL4RenderPassDescriptor new];p.colorAttachments[0].texture=texture;p.colorAttachments[0].loadAction=load;p.colorAttachments[0].storeAction=MTLStoreActionStore;
	p.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,1);p.renderTargetWidth=texture.width;p.renderTargetHeight=texture.height;return p;
}
static bool WaterReadOnlyEnabled(const char* name){const char* v=std::getenv(name);return v&&std::string_view(v)=="1";}
struct WaterSubmissionState {
 BumpWaterCompletion completion;
 bool coastInitialized=false,encoded=false,completed=false;
 bool queueQualified=false,queuedNormalWriter=false;uint64_t source=0;int64_t simulation=-1;
 std::array<id<MTLTexture>,3> localReads{}; // actual reflection/pre-color/pre-depth read owners
 id<MTLTexture> normal=nil,coast=nil;
};
struct Generation {
 std::weak_ptr<WaterSubmissionState> pending;
	uint64_t generationId=0,normalSequence=0,coastSequence=0;bool coastInitialized=false;
	std::shared_ptr<const NativeBumpWaterMaterial> material;
	std::shared_ptr<const std::vector<WorldVector3>> vertices;
	__strong id<MTLBuffer> vertexBuffer=nil;
	__strong id<MTLTexture> normal=nil,coast=nil;
};
struct Bitmap {
	std::weak_ptr<const WorldTexture> owner;__strong id<MTLTexture> image=nil;std::shared_ptr<bool> mipReady=std::make_shared<bool>(false);
};
}
struct PreparedBumpWater::Impl {
	struct Update {
		__strong MTL4RenderPassDescriptor* pass=nil;__strong id<MTL4ArgumentTable> arguments=nil;
		__strong id<MTLRenderPipelineState> pipeline=nil;uint32_t vertices=0;float alpha=1;bool normal=false,generateMips=false;
	};
	struct Mip {
		__strong MTL4RenderPassDescriptor* pass=nil;__strong id<MTL4ArgumentTable> arguments=nil;
		__strong id<MTLRenderPipelineState> pipeline=nil;
	};
	std::shared_ptr<const NativeBumpWaterFrame> frame;std::shared_ptr<Generation> generation;
	std::vector<Update> updates;std::vector<id<MTLTexture>> bitmapMips;std::vector<std::shared_ptr<bool>> mipCompletions;
	std::vector<Mip> coastMips;
	__strong id<MTL4ArgumentTable> surfaceArguments=nil;
	__strong id<MTLRenderPipelineState> surfacePipeline=nil;
	__strong id<MTLDepthStencilState> updateDepth=nil,surfaceDepth=nil;
	__strong id<MTLTexture> normal=nil,coast=nil;
	BumpWaterCompletion completion;bool clearCoast=false,dependenciesEncoded=false,surfaceEncoded=false;
 unsigned bitmapUploads=0;
 std::shared_ptr<WaterSubmissionState> submission=std::make_shared<WaterSubmissionState>();
};
PreparedBumpWater::PreparedBumpWater(std::unique_ptr<Impl> value):impl(std::move(value)) {}
PreparedBumpWater::~PreparedBumpWater()=default;
id<MTLTexture> PreparedBumpWater::Normal()const{return impl->normal;}
id<MTLTexture> PreparedBumpWater::Coast()const{return impl->coast;}
bool PreparedBumpWater::CanQueueSubmission()const{
 return impl->submission->queueQualified || impl->submission->queuedNormalWriter;
}
uint32_t PreparedBumpWater::DependencyDrawCount()const{
	const auto coastUpdates=std::count_if(impl->updates.begin(),impl->updates.end(),[](const auto& u){return u.generateMips&&!u.normal;});
	const auto clearedEmpty=impl->clearCoast&&impl->completion.coastSequence==0?1:0;
	return uint32_t(impl->updates.size()+impl->coastMips.size()*(coastUpdates+clearedEmpty));
}
void PreparedBumpWater::EncodeDependencies(id<MTL4CommandBuffer> command) {
	Require(!impl->dependenciesEncoded,"Original water dependencies encoded twice");
	auto mip=[&](id<MTLTexture> image){
		if(image==impl->coast){for(const auto& m:impl->coastMips){auto e=[command renderCommandEncoderWithDescriptor:m.pass];Require(e!=nil,"Cannot encode original RGB5 per-level blit");
			[e barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
			[e setViewport:MTLViewport{0,0,double(m.pass.renderTargetWidth),double(m.pass.renderTargetHeight),0,1}];[e setScissorRect:MTLScissorRect{0,0,m.pass.renderTargetWidth,m.pass.renderTargetHeight}];
			[e setCullMode:MTLCullModeNone];[e setTriangleFillMode:MTLTriangleFillModeFill];[e setDepthStencilState:impl->updateDepth];[e setDepthBias:0 slopeScale:0 clamp:0];
			[e setRenderPipelineState:m.pipeline];[e setArgumentTable:m.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];[e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
			[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];}return;}
		auto e=[command computeCommandEncoder];Require(e!=nil,"Cannot encode water mipmaps");
		[e barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[e generateMipmapsForTexture:image];
		[e barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];};
	for(auto image:impl->bitmapMips)mip(image);
	if(impl->clearCoast){auto e=[command renderCommandEncoderWithDescriptor:Pass(impl->coast,MTLLoadActionClear)];Require(e!=nil,"Cannot clear original coast RGB5 storage");
		[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];}
	for(const auto& update:impl->updates){
		auto e=[command renderCommandEncoderWithDescriptor:update.pass];Require(e!=nil,"Cannot encode original water recurrence");
		[e barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
		[e setViewport:MTLViewport{0,0,double(update.pass.renderTargetWidth),double(update.pass.renderTargetHeight),0,1}];
		[e setScissorRect:MTLScissorRect{0,0,update.pass.renderTargetWidth,update.pass.renderTargetHeight}];
		[e setCullMode:MTLCullModeNone];[e setTriangleFillMode:MTLTriangleFillModeFill];[e setDepthStencilState:impl->updateDepth];[e setDepthBias:0 slopeScale:0 clamp:0];
		[e setRenderPipelineState:update.pipeline];[e setArgumentTable:update.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
		if(update.normal)[e setBlendColorRed:1 green:1 blue:1 alpha:update.alpha];
		[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:update.vertices];
		[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];
		if(update.generateMips)mip(update.pass.colorAttachments[0].texture);
	}
	if(impl->clearCoast&&impl->completion.coastSequence==0)mip(impl->coast);
	impl->dependenciesEncoded=true;
}
void PreparedBumpWater::Encode(id<MTL4RenderCommandEncoder> encoder) {
	Require(impl->dependenciesEncoded&&!impl->surfaceEncoded,"Original water surface/dependency order differs");
	[encoder setRenderPipelineState:impl->surfacePipeline];[encoder setDepthStencilState:impl->surfaceDepth];[encoder setCullMode:MTLCullModeNone];
	[encoder setDepthClipMode:MTLDepthClipModeClip];[encoder setTriangleFillMode:impl->frame->wireframe?MTLTriangleFillModeLines:MTLTriangleFillModeFill];
	[encoder setDepthBias:2 slopeScale:0 clamp:0];[encoder setArgumentTable:impl->surfaceArguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:impl->frame->radial?MTLPrimitiveTypeTriangleStrip:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:impl->frame->vertices->size()];
	impl->surfaceEncoded=true;
 impl->submission->completion=impl->completion;impl->submission->coastInitialized=impl->coast!=nil;
 impl->submission->encoded=true;impl->generation->pending=impl->submission;
}
BumpWaterCompletion PreparedBumpWater::DidComplete() {
	Require(impl->dependenciesEncoded&&impl->surfaceEncoded,"Original water cannot retire unencoded dependencies/surface");
	Require(impl->submission->encoded,"Water completion lost its encoded owner");
 if(impl->submission->completed)return impl->completion;
	for(auto& completed:impl->mipCompletions)*completed=true;
	impl->generation->normalSequence=impl->completion.normalSequence;impl->generation->coastSequence=impl->completion.coastSequence;
	if(impl->coast)impl->generation->coastInitialized=true;impl->submission->completed=true;return impl->completion;
}
struct BumpWaterGPU::Impl {
	struct Pipelines {__strong id<MTLLibrary> library=nil;__strong id<MTLRenderPipelineState> surface=nil,normal=nil,coast=nil,atlas=nil,coastMip=nil;};
	std::map<PipelineKey,Pipelines> pipelines;
 std::map<std::tuple<PipelineKey,unsigned,uint32_t,MaterialBits>,id<MTLRenderPipelineState>> fixedSurface;
 id<MTLRenderPipelineState> FixedSurface(RuntimeLease& lease,const PipelineKey& key,const Pipelines& base,const NativeBumpWaterUniforms& u,unsigned profile,bool blend,unsigned samples,double& ms){
  Require(profile>0&&profile<7,"Invalid water surface variant");
  const uint32_t mask=(profile==1||profile>=3)?u.flags[1]:0u;
  const auto bits=profile>=4?WaterBits(u):MaterialBits{};
  auto k=std::tuple{key,profile,mask,bits};
  if(auto it=fixedSurface.find(k);it!=fixedSurface.end()){lease.Retain(it->second);return it->second;}
  auto start=Clock::now();auto ld=[MTL4LibraryDescriptor new];ld.name=@"BumpWater surface variant";ld.source=String(WaterVariantSource(key.flags,u,profile));
  auto opt=[MTLCompileOptions new];opt.mathMode=(profile==2||profile==3||profile==5||profile==6)?MTLMathModeRelaxed:MTLMathModeSafe;
  opt.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;ConfigureInterventionColorMath(opt);ld.options=opt;
  NSError* error=nil;auto fs=lease.NewLibrary(ld,&error);Require(fs,"Cannot compile water surface variant: "+Error(error));
  auto pipeline=Pipeline(lease,base.library,"BumpWaterVertex","BumpWaterFragment",key.color,key.depth,key.stencil,blend,false,key.validate,samples,fs);
  lease.Retain(base.library);lease.Retain(fs);lease.Retain(pipeline);fixedSurface.emplace(k,pipeline);ms+=Milliseconds(start);return pipeline;
 }
 std::map<const WorldTexture*,Bitmap> bitmaps;
	std::shared_ptr<Generation> generation;
	using SamplerKey=std::tuple<unsigned,unsigned,unsigned,unsigned,uint32_t,bool>;
	std::map<SamplerKey,id<MTLSamplerState>> samplers;
	id<MTLSamplerState> SamplerFor(RuntimeLease& lease,MTLSamplerMinMagFilter min,MTLSamplerMinMagFilter mag,MTLSamplerMipFilter mip,MTLSamplerAddressMode wrap,uint32_t anisotropy=1,bool compare=false){
		SamplerKey key{unsigned(min),unsigned(mag),unsigned(mip),unsigned(wrap),anisotropy,compare};auto found=samplers.find(key);
		if(found==samplers.end())found=samplers.emplace(key,Sampler(lease,min,mag,mip,wrap,anisotropy,compare)).first;
		lease.Retain(found->second);return found->second;
	}
	__strong id<MTLDepthStencilState> updateDepth=nil,surfaceDepth=nil;
	id<MTLTexture> BitmapFor(RuntimeLease& lease,std::shared_ptr<const WorldTexture> owner,bool needMips,PreparedBumpWater::Impl& prepared) {
		Require(owner&&!owner->mips.empty(),"Missing actual original water bitmap");auto it=bitmaps.find(owner.get());
		if(it!=bitmaps.end()&&it->second.owner.lock()!=owner){bitmaps.erase(it);it=bitmaps.end();}
		if(it!=bitmaps.end()&&needMips&&!*it->second.mipReady&&lease.PreparingWhilePending())lease.RetirePreviousSubmission();
		if(it==bitmaps.end()){
   ++prepared.bitmapUploads;
			const auto& base=owner->mips.front();const uint32_t levels=owner->generateMipmaps?Levels(base.width,base.height):uint32_t(owner->mips.size());
			const auto decoded=DecodeWorldBCBaseForMipmaps(*owner,lease.Capabilities().maxTextureDimension2D);
			const auto format=decoded?(owner->format==WorldTextureFormat::BC7RGBASRGB?MTLPixelFormatRGBA8Unorm_sRGB:MTLPixelFormatRGBA8Unorm):Format(owner->format);
			Bitmap b;b.owner=owner;b.image=Texture(lease,format,base.width,base.height,levels,MTLStorageModeShared,MTLTextureUsageShaderRead,owner->sourcePath.c_str());
			for(uint32_t i=0;i<owner->mips.size();++i){const auto& level=decoded?*decoded:owner->mips[i];Require(level.width==std::max(1u,base.width>>i)&&level.height==std::max(1u,base.height>>i),"Original water bitmap mip extent differs");
				Require(level.bytesPerRow&&level.bytes.size()>=level.bytesPerRow,"Original water bitmap mip bytes missing");
				[b.image replaceRegion:MTLRegionMake2D(0,0,level.width,level.height) mipmapLevel:i withBytes:level.bytes.data() bytesPerRow:level.bytesPerRow];}
			*b.mipReady=!owner->generateMipmaps;it=bitmaps.emplace(owner.get(),std::move(b)).first;
		}
		lease.AddAllocation(it->second.image);
		if(needMips&&!*it->second.mipReady&&std::find(prepared.bitmapMips.begin(),prepared.bitmapMips.end(),it->second.image)==prepared.bitmapMips.end()){
			prepared.bitmapMips.push_back(it->second.image);prepared.mipCompletions.push_back(it->second.mipReady);}
		return it->second.image;
	}
};
BumpWaterGPU::BumpWaterGPU():impl(std::make_unique<Impl>()) {}
BumpWaterGPU::~BumpWaterGPU()=default;
void BumpWaterGPU::ClearResources(){impl->generation.reset();impl->bitmaps.clear();}
std::unique_ptr<PreparedBumpWater> BumpWaterGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeBumpWaterFrame> frame,
	const BumpWaterSceneImages& images,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& compileMs,unsigned samples,bool queueCandidate)
{
	Require(frame!=nullptr,"Missing original water frame");ValidateNativeBumpWater(*frame);const auto& f=*frame;const auto& m=*f.material;
	Require(NativeBumpWaterVisible(f),"Original world stage does not draw water on this map/frame");
	Require(images.sourceDrawFrame==f.sourceDrawFrame&&images.renderWidth&&images.renderHeight,"Original water scene source generation/extent differs");
	Require(depth!=MTLPixelFormatInvalid,"Original water surface requires the real scene depth attachment");
	uint32_t flips=0;std::array<id<MTLTexture>,12> textures{};
	auto external=[&](const BumpWaterSceneImage& value,uint32_t unit,MTLPixelFormat format,uint32_t w,uint32_t h){
		Require(value.texture&&value.texture.pixelFormat==format&&value.texture.width==w&&value.texture.height==h&&value.texture.sampleCount==1&&(value.texture.usage&MTLTextureUsageShaderRead),"Original water source image format/extent/usage differs");
		textures[unit]=value.texture;if(value.flipV)flips|=1u<<unit;lease.AddAllocation(value.texture);};
	if(!NativeBumpWaterShoreWaves(f))external(images.shading,1,MTLPixelFormatRGBA8Unorm,f.mapSquaresX+1,f.mapSquaresZ+1);
	if(f.settings.reflection)external(images.planarReflection,4,MTLPixelFormatRGBA8Unorm,f.settings.reflectionSize,f.settings.reflectionSize);
	if(f.settings.refraction)external(images.preWaterColor,5,MTLPixelFormatRGBA8Unorm,f.screenTextureWidth,f.screenTextureHeight);
	if(f.settings.depthCopy)external(images.preWaterDepth,7,MTLPixelFormatDepth32Float,f.screenTextureWidth,f.screenTextureHeight);
	if(f.shadows){Require(images.shadowDepth.texture&&images.shadowColor.texture,"Original water shadow depth/color pair missing");
		external(images.shadowDepth,9,MTLPixelFormatDepth32Float,images.shadowDepth.texture.width,images.shadowDepth.texture.height);
		const auto format=images.shadowColor.texture.pixelFormat;
		Require(format==MTLPixelFormatRGBA8Unorm||format==MTLPixelFormatR8Unorm,"Original shadow color must retain RGB8 or R8 storage");
		if(format==MTLPixelFormatR8Unorm){const auto s=images.shadowColor.texture.swizzle;
			Require(s.red==MTLTextureSwizzleRed&&s.green==MTLTextureSwizzleRed&&s.blue==MTLTextureSwizzleRed&&s.alpha==MTLTextureSwizzleOne,"Original gray shadow color requires an actual RRR1 texture view");}
		external(images.shadowColor,11,format,images.shadowDepth.texture.width,images.shadowDepth.texture.height);}
	if(f.infoTexture)external(images.info,10,MTLPixelFormatRGB10A2Unorm,f.paddedSquaresX,f.paddedSquaresZ);
	auto prepared=std::make_unique<PreparedBumpWater::Impl>();prepared->frame=frame;
 const bool enabled=WaterReadOnlyEnabled("BAR_NATIVE_WATER4_READONLY_QUEUE");
 static const bool transition=WaterReadOnlyEnabled("BAR_NATIVE_WATER4_TICK_TRANSITION");
 const bool census=WaterReadOnlyEnabled("BAR_NATIVE_WATER4_READONLY_CENSUS");
 const bool inspect=enabled||census;const bool candidate=inspect&&queueCandidate;
 auto& ticket=*prepared->submission;ticket.source=lease.SourceSubmission();ticket.simulation=f.simulationFrame;
 ticket.localReads={f.settings.reflection?images.planarReflection.texture:nil,f.settings.refraction?images.preWaterColor.texture:nil,f.settings.depthCopy?images.preWaterDepth.texture:nil};
 bool potentialPair=false,orderedPair=false,orderedTransition=false,priorPending=false;unsigned blockers=0;

	if(!impl->generation||impl->generation->generationId!=f.generation){auto next=std::make_shared<Generation>();next->generationId=f.generation;next->material=f.material;
		if(NativeBumpWaterDynamicNormals(f)){const auto& base=m.normal->mips.front();next->normal=Texture(lease,MTLPixelFormatRGBA8Unorm,base.width,base.height,Levels(base.width,base.height),MTLStorageModePrivate,MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead,"Original persistent RGBA8 dynamic normals");}
		if(NativeBumpWaterShoreWaves(f))next->coast=Texture(lease,MTLPixelFormatBGR5A1Unorm,f.mapSquaresX,f.mapSquaresZ,Levels(f.mapSquaresX,f.mapSquaresZ),MTLStorageModePrivate,MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead,"Original RGB5 coast recurrence");
		impl->generation=std::move(next);impl->fixedSurface.clear();
	}
	auto& generation=*impl->generation;Require(generation.material==f.material,"Water material changed without resource generation");prepared->generation=impl->generation;
 auto base=BumpWaterCompletion{f.generation,generation.normalSequence,generation.coastSequence};
 auto noNewUpdates=[&]{return !std::any_of(f.normalUpdates.begin(),f.normalUpdates.end(),[&](const auto& u){return u.sequence>base.normalSequence;})&&!std::any_of(f.coastUpdates.begin(),f.coastUpdates.end(),[&](const auto& u){return u->sequence>base.coastSequence;});};
 auto bitmapReady=[&](const std::shared_ptr<const WorldTexture>& v,bool needMips){auto i=impl->bitmaps.find(v.get());return i!=impl->bitmaps.end()&&i->second.owner.lock()==v&&(!needMips||*i->second.mipReady);};
 bool sourceBitmapsReady=false;
 if(inspect)sourceBitmapsReady=bitmapReady(m.normal,!generation.normal)&&bitmapReady(m.caustics[NativeBumpWaterCausticIndex(f.simulationFrame,m.caustics.size())],true)&&bitmapReady(m.foam,true)&&(!generation.coast||bitmapReady(m.waveRandom,true));

 bool coastInitialized=generation.coastInitialized;
 if(auto prior=generation.pending.lock();prior&&!prior->completed){
  priorPending=true;
  Require(prior->encoded&&prior->completion.generation==f.generation,"Water predecessor lost its original encoded generation");
  if(inspect){
   const bool sameSequences=prior->completion.normalSequence==base.normalSequence&&prior->completion.coastSequence==base.coastSequence;
   const bool sameMaterial=prior->normal==generation.normal&&prior->coast==generation.coast;
   const bool initialized=(!generation.normal||base.normalSequence)&&(!generation.coast||generation.coastInitialized);
   bool distinct=true;for(auto a:ticket.localReads)for(auto p:prior->localReads)if(a&&a==p)distinct=false;
   const bool adjacent=prior->source&&ticket.source>prior->source&&ticket.source-prior->source==1&&ticket.simulation==prior->simulation;
   const uint64_t event=lease.PendingEventForSource(prior->source);
   blockers=(candidate?0u:1u)|(prior->queueQualified?0u:2u)|(sameSequences&&sameMaterial&&initialized?0u:4u)|(noNewUpdates()?0u:8u)|(sourceBitmapsReady?0u:16u)|(distinct?0u:32u)|(adjacent?0u:64u)|(event?0u:128u);
   potentialPair=!blockers;
   if(enabled&&potentialPair){
    // Same completed immutable content; no current recurrence writes. The existing
    // full previous GPU event and FIFO completion/error checks are retained.
    orderedPair=lease.OrderGPUReadAfterSubmission(prior.get(),prior->source,event,[prior]{Require(prior->completed&&prior->queueQualified,"Water read-only predecessor did not retire its actual owner");},NativePriorRead::BumpWater);
   }

   if (enabled && transition && !orderedPair && candidate && sameMaterial &&
       initialized && sourceBitmapsReady && distinct && event && prior->source &&
       ticket.source > prior->source && ticket.source - prior->source == 1) {
    const auto& previous = prior->completion;
    const bool sameCoast = previous.coastSequence == base.coastSequence &&
        std::none_of(f.coastUpdates.begin(), f.coastUpdates.end(), [&](const auto& update) {
         return update->sequence > previous.coastSequence;
        });
    unsigned newNormalUpdates = 0;
    bool nextNormal = false;
    for (const auto& update : f.normalUpdates) {
     if (update.sequence <= previous.normalSequence) continue;
     ++newNormalUpdates;
     nextNormal = !update.initialize && update.sequence - previous.normalSequence == 1;
    }
    const bool normalWriter = sameCoast && sameSequences && prior->queueQualified &&
        ticket.simulation > prior->simulation && ticket.simulation - prior->simulation == 1 &&
        newNormalUpdates == 1 && nextNormal;
    const bool firstReader = sameCoast && prior->queuedNormalWriter && adjacent &&
        previous.normalSequence > base.normalSequence &&
        previous.normalSequence - base.normalSequence == 1 && newNormalUpdates == 0;
    if (normalWriter || firstReader) {
     // SubmitAndDefer orders the entire command buffer after the immediately
     // preceding event. FIFO retirement still checks the actual predecessor
     // before publishing this frame's completions.
     orderedTransition = lease.OrderGPUReadAfterSubmission(
         prior.get(), prior->source, event, [prior, normalWriter] {
          Require(prior->completed && (normalWriter ? prior->queueQualified : prior->queuedNormalWriter),
                  "Water4 normal transition lost its actual predecessor");
         }, NativePriorRead::BumpWater);
     ticket.queuedNormalWriter = orderedTransition && normalWriter;
    }
   }
  }
  if(orderedPair||orderedTransition||lease.OrderAfterPreviousSubmission(prior.get(),[prior]{Require(prior->completed,"Water predecessor did not actually complete before successor submission");},NativePriorRead::BumpWater)){
   base=prior->completion;coastInitialized=prior->coastInitialized;
  }else{
   lease.RetirePreviousSubmission();Require(prior->completed,"Water preparation cannot overwrite an unretired predecessor");
   base={f.generation,generation.normalSequence,generation.coastSequence};coastInitialized=generation.coastInitialized;
  }
 }
 prepared->completion=base;prepared->coast=generation.coast;
 prepared->clearCoast=generation.coast&&!coastInitialized;
	if(generation.normal)lease.AddAllocation(generation.normal);if(generation.coast)lease.AddAllocation(generation.coast);
	for(auto i=impl->bitmaps.begin();i!=impl->bitmaps.end();)if(i->second.owner.expired())i=impl->bitmaps.erase(i);else ++i;
	auto bitmap=[&](const std::shared_ptr<const WorldTexture>& value,uint32_t unit,bool mip=true){textures[unit]=impl->BitmapFor(lease,value,mip,*prepared);if(value->flipV)flips|=1u<<unit;};
	bitmap(m.normal,0,!generation.normal);if(generation.normal){textures[0]=generation.normal;flips&=~1u;}prepared->normal=textures[0];
	bitmap(m.caustics[NativeBumpWaterCausticIndex(f.simulationFrame,m.caustics.size())],2);bitmap(m.foam,3);
	if(generation.coast){textures[6]=generation.coast;bitmap(m.waveRandom,8);}
	const auto uniforms=MakeNativeBumpWaterUniforms(f,images.renderWidth,images.renderHeight,flips);
	PipelineKey key{uniforms.flags[0]&255u,color,depth,stencil,validation,lease.SampleShadingKey(samples)};auto found=impl->pipelines.find(key);
	if(found==impl->pipelines.end()){
		const auto start=Clock::now();auto descriptor=[MTL4LibraryDescriptor new];descriptor.source=String("#define NATIVE_WATER_FLAGS "+std::to_string(key.flags)+"\n"+NATIVE_BUMP_WATER_SHADERS);
		auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;ConfigureInterventionColorMath(options);descriptor.options=options;
		NSError* error=nil;auto library=lease.NewLibrary(descriptor,&error);Require(library!=nil,"Cannot compile original BumpWater shaders: "+Error(error));
		Impl::Pipelines p;p.library=library;p.surface=Pipeline(lease,library,"BumpWaterVertex","BumpWaterFragment",color,depth,stencil,!f.settings.refraction,false,validation,samples);
		p.normal=Pipeline(lease,library,"BumpWaterUpdateVertex","BumpWaterNormalFragment",MTLPixelFormatRGBA8Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,true,true,validation);
		p.coast=Pipeline(lease,library,"BumpWaterUpdateVertex","BumpWaterCoastFragment",MTLPixelFormatBGR5A1Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,false,false,validation);
		p.atlas=Pipeline(lease,library,"BumpWaterUpdateVertex","BumpWaterCoastFragment",MTLPixelFormatRGBA8Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,false,false,validation);
		p.coastMip=Pipeline(lease,library,"BumpWaterMipVertex","BumpWaterMipFragment",MTLPixelFormatBGR5A1Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,false,false,validation);
		found=impl->pipelines.emplace(key,std::move(p)).first;compileMs+=Milliseconds(start);
	}
	const auto& pipelines=found->second;for(auto p:{pipelines.surface,pipelines.normal,pipelines.coast,pipelines.atlas,pipelines.coastMip})lease.Retain(p);prepared->surfacePipeline=pipelines.surface;
 const unsigned requested=WaterVariant();const bool fixedRequested=requested!=0;
 const bool fixedEligible=samples==1&&!InterventionFastMathEnabled()&&(uniforms.flags[1]&~0xfffu)==0;
 if(fixedRequested&&fixedEligible){
  prepared->surfacePipeline=impl->FixedSurface(lease,key,pipelines,uniforms,requested,!f.settings.refraction,samples,compileMs);
  static std::atomic<bool> reported{false};if(!reported.exchange(true))LOG("[BumpWaterSurfaceVariant] active variant=%u mask=%u flags=%u",requested,uniforms.flags[1],key.flags);
 }
	if(!impl->updateDepth){auto d=[MTLDepthStencilDescriptor new];d.depthCompareFunction=MTLCompareFunctionAlways;d.depthWriteEnabled=NO;impl->updateDepth=[lease.Device() newDepthStencilStateWithDescriptor:d];
		d.depthCompareFunction=MTLCompareFunctionLessEqual;impl->surfaceDepth=[lease.Device() newDepthStencilStateWithDescriptor:d];Require(impl->updateDepth&&impl->surfaceDepth,"Cannot create original water depth states");}
	prepared->updateDepth=impl->updateDepth;prepared->surfaceDepth=impl->surfaceDepth;lease.Retain(impl->updateDepth);lease.Retain(impl->surfaceDepth);
	using F=MTLSamplerMinMagFilter;using M=MTLSamplerMipFilter;using W=MTLSamplerAddressMode;
	std::array<id<MTLSamplerState>,12> samplers{};
	Require(f.settings.anisotropy<=16&&f.settings.anisotropy==std::floor(f.settings.anisotropy),"Original fractional/out-of-range water anisotropy needs explicit equivalent sampler");
	samplers[0]=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterLinear,F::MTLSamplerMinMagFilterLinear,m.normal->mipmappedSampling||generation.normal?M::MTLSamplerMipFilterLinear:M::MTLSamplerMipFilterNotMipmapped,W::MTLSamplerAddressModeRepeat,std::max(1u,uint32_t(f.settings.anisotropy)));
	samplers[1]=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterNearest,F::MTLSamplerMinMagFilterLinear,M::MTLSamplerMipFilterNotMipmapped,W::MTLSamplerAddressModeClampToEdge);
	samplers[2]=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterLinear,F::MTLSamplerMinMagFilterLinear,m.caustics[NativeBumpWaterCausticIndex(f.simulationFrame,m.caustics.size())]->mipmappedSampling?M::MTLSamplerMipFilterLinear:M::MTLSamplerMipFilterNotMipmapped,W::MTLSamplerAddressModeRepeat);
	samplers[3]=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterLinear,F::MTLSamplerMinMagFilterLinear,m.foam->mipmappedSampling?M::MTLSamplerMipFilterLinear:M::MTLSamplerMipFilterNotMipmapped,W::MTLSamplerAddressModeRepeat);
	samplers[4]=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterLinear,F::MTLSamplerMinMagFilterLinear,M::MTLSamplerMipFilterNotMipmapped,W::MTLSamplerAddressModeClampToEdge);
	samplers[5]=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterNearest,F::MTLSamplerMinMagFilterNearest,M::MTLSamplerMipFilterNotMipmapped,W::MTLSamplerAddressModeClampToEdge);
	samplers[6]=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterLinear,F::MTLSamplerMinMagFilterLinear,M::MTLSamplerMipFilterNearest,W::MTLSamplerAddressModeClampToEdge);
	samplers[7]=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterNearest,F::MTLSamplerMinMagFilterNearest,M::MTLSamplerMipFilterNotMipmapped,W::MTLSamplerAddressModeRepeat);
	samplers[8]=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterLinear,F::MTLSamplerMinMagFilterLinear,m.waveRandom&&m.waveRandom->mipmappedSampling?M::MTLSamplerMipFilterLinear:M::MTLSamplerMipFilterNotMipmapped,W::MTLSamplerAddressModeRepeat);
	samplers[9]=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterLinear,F::MTLSamplerMinMagFilterLinear,M::MTLSamplerMipFilterNotMipmapped,W::MTLSamplerAddressModeClampToBorderColor,1,true);
	samplers[10]=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterLinear,F::MTLSamplerMinMagFilterLinear,M::MTLSamplerMipFilterNearest,W::MTLSamplerAddressModeClampToEdge);
	samplers[11]=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterLinear,F::MTLSamplerMinMagFilterLinear,M::MTLSamplerMipFilterNotMipmapped,W::MTLSamplerAddressModeClampToBorderColor);
	auto table=Arguments(lease,2,12,12);if(!generation.vertexBuffer){generation.vertexBuffer=Buffer(lease,f.vertices->data(),f.vertices->size()*sizeof(WorldVector3),"Original GenWaterPlaneBuffer vertices");generation.vertices=f.vertices;}
	Require(generation.vertices==f.vertices,"Original water geometry changed without resource reload");lease.AddAllocation(generation.vertexBuffer);auto vertexBuffer=generation.vertexBuffer;auto uniformBuffer=Buffer(lease,&uniforms,sizeof(uniforms),"Original BumpWater material/camera uniforms");
	[table setAddress:vertexBuffer.gpuAddress atIndex:0];[table setAddress:uniformBuffer.gpuAddress atIndex:1];
	for(uint32_t i=0;i<12;++i){[table setTexture:textures[i]?textures[i].gpuResourceID:MTLResourceID{0} atIndex:i];[table setSamplerState:samplers[i].gpuResourceID atIndex:i];}
	prepared->surfaceArguments=table;
	auto nearestRepeat=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterNearest,F::MTLSamplerMinMagFilterNearest,M::MTLSamplerMipFilterNotMipmapped,W::MTLSamplerAddressModeRepeat);
	auto nearestClamp=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterNearest,F::MTLSamplerMinMagFilterNearest,M::MTLSamplerMipFilterNotMipmapped,W::MTLSamplerAddressModeClampToEdge);
	const bool needsCoastMips=prepared->clearCoast||std::any_of(f.coastUpdates.begin(),f.coastUpdates.end(),[&](const auto& u){return u->sequence>base.coastSequence;});
	if(generation.coast&&needsCoastMips){auto filter=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterLinear,F::MTLSamplerMinMagFilterLinear,M::MTLSamplerMipFilterNearest,W::MTLSamplerAddressModeClampToEdge);
		for(uint32_t level=1;level<generation.coast.mipmapLevelCount;++level){auto pass=Pass(generation.coast,MTLLoadActionDontCare);pass.colorAttachments[0].level=level;
			pass.renderTargetWidth=std::max(1u,f.mapSquaresX>>level);pass.renderTargetHeight=std::max(1u,f.mapSquaresZ>>level);const uint32_t sourceLevel=level-1;
			auto b=Buffer(lease,&sourceLevel,sizeof(sourceLevel),"Original RGB5 mip source level");auto arguments=Arguments(lease,1,1,1);[arguments setAddress:b.gpuAddress atIndex:0];
			[arguments setTexture:generation.coast.gpuResourceID atIndex:0];[arguments setSamplerState:filter.gpuResourceID atIndex:0];prepared->coastMips.push_back({pass,arguments,pipelines.coastMip});}
	}
	for(const auto& update:f.normalUpdates){
		if(update.sequence<=base.normalSequence)continue;Require(generation.normal!=nil,"Original normal update without persistent destination");
		if(prepared->completion.normalSequence==0)Require(update.initialize,"First original dynamic-normal update must initialize");
		auto vertices=MakeNativeBumpWaterNormalVertices(m.numTiles,update);auto v=Buffer(lease,vertices.data(),vertices.size()*sizeof(vertices[0]),"Original normal tile geometry");auto args=Arguments(lease,2,1,1);
		[args setAddress:v.gpuAddress atIndex:0];[args setTexture:impl->BitmapFor(lease,m.normal,false,*prepared).gpuResourceID atIndex:0];[args setSamplerState:nearestRepeat.gpuResourceID atIndex:0];
		Require(!m.normal->flipV,"Original dynamic normal bitmap must retain its load-time row order");
		prepared->updates.push_back({Pass(generation.normal,update.initialize?MTLLoadActionDontCare:MTLLoadActionLoad),args,pipelines.normal,uint32_t(vertices.size()),NativeBumpWaterNormalAlpha(update),true,true});
		prepared->completion.normalSequence=update.sequence;
	}
	Require(!generation.normal||prepared->completion.normalSequence,"Original dynamic normals have not initialized");
	for(const auto& owner:f.coastUpdates){
		const auto& update=*owner;
		if(update.sequence<=base.coastSequence)continue;
		auto atlas=Texture(lease,MTLPixelFormatRGBA8Unorm,update.atlasWidth,update.atlasHeight,1,MTLStorageModeShared,MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget,"Original packed water coast-update atlas");
		[atlas replaceRegion:MTLRegionMake2D(0,0,update.atlasWidth,update.atlasHeight) mipmapLevel:0 withBytes:update.rgba.data() bytesPerRow:update.atlasWidth*4];lease.AddAllocation(atlas);
		for(uint32_t radius=0;radius<=10;++radius){auto vertices=MakeNativeBumpWaterCoastVertices(update,radius);if(vertices.empty())continue;
			const std::array<int32_t,4> parameters={int32_t(radius),int32_t(radius&1),0,0};auto v=Buffer(lease,vertices.data(),vertices.size()*sizeof(vertices[0]),"Original coast update rectangles");auto p=Buffer(lease,parameters.data(),sizeof(parameters),"Original coast iteration args");auto args=Arguments(lease,2,2,1);
			[args setAddress:v.gpuAddress atIndex:0];[args setAddress:p.gpuAddress atIndex:1];[args setTexture:generation.coast.gpuResourceID atIndex:0];[args setTexture:atlas.gpuResourceID atIndex:1];[args setSamplerState:nearestClamp.gpuResourceID atIndex:0];
			auto destination=(radius&1)?atlas:generation.coast;prepared->updates.push_back({Pass(destination,MTLLoadActionLoad),args,(radius&1)?pipelines.atlas:pipelines.coast,uint32_t(vertices.size()),1,false,radius==10});
		}prepared->completion.coastSequence=update.sequence;
	}
	if(inspect){
  ticket.normal=generation.normal;ticket.coast=generation.coast;
  const bool actualReadOnly=prepared->updates.empty()&&prepared->bitmapMips.empty()&&prepared->coastMips.empty()&&!prepared->clearCoast&&!prepared->bitmapUploads;
  ticket.queueQualified=candidate&&actualReadOnly&&(!generation.normal||prepared->completion.normalSequence)&&(!generation.coast||coastInitialized);
  // A failed late check must never admit this draw. Any earlier ordering
  // registration remains a real prior check on the original serial path.
  if(orderedPair)Require(ticket.queueQualified&&prepared->completion.normalSequence==base.normalSequence&&prepared->completion.coastSequence==base.coastSequence,"Water read-only queue admission acquired a new producer");

  if (orderedTransition) {
   if (ticket.queuedNormalWriter) {
    Require(prepared->updates.size() == 1 && prepared->updates.front().normal &&
            prepared->bitmapMips.empty() && prepared->coastMips.empty() &&
            !prepared->clearCoast && !prepared->bitmapUploads &&
            prepared->completion.normalSequence > base.normalSequence &&
            prepared->completion.normalSequence - base.normalSequence == 1 &&
            prepared->completion.coastSequence == base.coastSequence,
            "Water4 transition queued a different write set");
   } else {
    Require(ticket.queueQualified && prepared->completion.normalSequence == base.normalSequence &&
            prepared->completion.coastSequence == base.coastSequence,
            "Water4 transition reader acquired a new producer");
   }
   static unsigned long long writers = 0, readers = 0;
   writers += ticket.queuedNormalWriter;
   readers += !ticket.queuedNormalWriter;
   const auto total = writers + readers;
   if (total <= 2 || (census && (total % 120 == 0 || f.simulationFrame == 1739)))
    LOG("[Water4TickTransition] active writers=%llu readers=%llu sim=%lld source=%llu",
        writers, readers, (long long)ticket.simulation, (unsigned long long)ticket.source);
  }
  static unsigned long long calls=0,qualified=0,pending=0,pairs=0,ordered=0,writer=0,bitmaps=0,scene=0;
  static std::array<unsigned long long,8> reasons{};
  if(f.simulationFrame>=540&&f.simulationFrame<1740){++calls;qualified+=ticket.queueQualified;pending+=priorPending;pairs+=potentialPair;ordered+=orderedPair;writer+=!actualReadOnly;bitmaps+=prepared->bitmapMips.size()+prepared->bitmapUploads;scene+=candidate;for(unsigned i=0;i<8;++i)reasons[i]+=priorPending&&bool(blockers&(1u<<i));
   if(calls==1||calls%120==0||f.simulationFrame>=1739)LOG("[Water4ReadonlyQueue] enabled=%u calls=%llu scene=%llu read_only=%llu pending=%llu possible_pairs=%llu ordered_pairs=%llu writes=%llu bitmap_work=%llu blockers=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu source=%llu sim=%lld",enabled,calls,scene,qualified,pending,pairs,ordered,writer,bitmaps,reasons[0],reasons[1],reasons[2],reasons[3],reasons[4],reasons[5],reasons[6],reasons[7],(unsigned long long)ticket.source,(long long)ticket.simulation);
  }
 }
 return std::unique_ptr<PreparedBumpWater>(new PreparedBumpWater(std::move(prepared)));
}
} // namespace rendering::metal4
