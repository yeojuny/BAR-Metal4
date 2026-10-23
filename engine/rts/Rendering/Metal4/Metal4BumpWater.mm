/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4BumpWater.h"
#include "Metal4WorldBCDecode.h"
#include "NativeBumpWaterShaders.h"
#include <algorithm>
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
struct PipelineKey {
	uint32_t flags=0;MTLPixelFormat color=MTLPixelFormatInvalid,depth=MTLPixelFormatInvalid,stencil=MTLPixelFormatInvalid;bool validate=false;unsigned samples=1;
	auto operator<=>(const PipelineKey&) const=default;
};
id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,id<MTLLibrary> library,const char* vs,const char* fs,
	MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool blend,bool constantAlpha,bool validation,unsigned samples=1) {
	auto d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=samples;auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=library;v.name=String(vs);f.name=String(fs);
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
struct Generation {
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
};
PreparedBumpWater::PreparedBumpWater(std::unique_ptr<Impl> value):impl(std::move(value)) {}
PreparedBumpWater::~PreparedBumpWater()=default;
id<MTLTexture> PreparedBumpWater::Normal()const{return impl->normal;}
id<MTLTexture> PreparedBumpWater::Coast()const{return impl->coast;}
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
}
BumpWaterCompletion PreparedBumpWater::DidComplete() {
	Require(impl->dependenciesEncoded&&impl->surfaceEncoded,"Original water cannot retire unencoded dependencies/surface");
	for(auto& completed:impl->mipCompletions)*completed=true;
	impl->generation->normalSequence=impl->completion.normalSequence;impl->generation->coastSequence=impl->completion.coastSequence;
	if(impl->coast)impl->generation->coastInitialized=true;return impl->completion;
}
struct BumpWaterGPU::Impl {
	struct Pipelines {__strong id<MTLRenderPipelineState> surface=nil,normal=nil,coast=nil,atlas=nil,coastMip=nil;};
	std::map<PipelineKey,Pipelines> pipelines;std::map<const WorldTexture*,Bitmap> bitmaps;
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
		if(it==bitmaps.end()){
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
	const BumpWaterSceneImages& images,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& compileMs,unsigned samples)
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
	if(!impl->generation||impl->generation->generationId!=f.generation){auto next=std::make_shared<Generation>();next->generationId=f.generation;next->material=f.material;
		if(NativeBumpWaterDynamicNormals(f)){const auto& base=m.normal->mips.front();next->normal=Texture(lease,MTLPixelFormatRGBA8Unorm,base.width,base.height,Levels(base.width,base.height),MTLStorageModePrivate,MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead,"Original persistent RGBA8 dynamic normals");}
		if(NativeBumpWaterShoreWaves(f))next->coast=Texture(lease,MTLPixelFormatBGR5A1Unorm,f.mapSquaresX,f.mapSquaresZ,Levels(f.mapSquaresX,f.mapSquaresZ),MTLStorageModePrivate,MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead,"Original RGB5 coast recurrence");
		impl->generation=std::move(next);
	}
	auto& generation=*impl->generation;Require(generation.material==f.material,"Water material changed without resource generation");prepared->generation=impl->generation;
	prepared->completion={f.generation,generation.normalSequence,generation.coastSequence};prepared->coast=generation.coast;
	prepared->clearCoast=generation.coast&&!generation.coastInitialized;
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
		auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;
		NSError* error=nil;auto library=lease.NewLibrary(descriptor,&error);Require(library!=nil,"Cannot compile original BumpWater shaders: "+Error(error));
		Impl::Pipelines p;p.surface=Pipeline(lease,library,"BumpWaterVertex","BumpWaterFragment",color,depth,stencil,!f.settings.refraction,false,validation,samples);
		p.normal=Pipeline(lease,library,"BumpWaterUpdateVertex","BumpWaterNormalFragment",MTLPixelFormatRGBA8Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,true,true,validation);
		p.coast=Pipeline(lease,library,"BumpWaterUpdateVertex","BumpWaterCoastFragment",MTLPixelFormatBGR5A1Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,false,false,validation);
		p.atlas=Pipeline(lease,library,"BumpWaterUpdateVertex","BumpWaterCoastFragment",MTLPixelFormatRGBA8Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,false,false,validation);
		p.coastMip=Pipeline(lease,library,"BumpWaterMipVertex","BumpWaterMipFragment",MTLPixelFormatBGR5A1Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,false,false,validation);
		found=impl->pipelines.emplace(key,std::move(p)).first;compileMs+=Milliseconds(start);
	}
	const auto& pipelines=found->second;for(auto p:{pipelines.surface,pipelines.normal,pipelines.coast,pipelines.atlas,pipelines.coastMip})lease.Retain(p);prepared->surfacePipeline=pipelines.surface;
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
	const bool needsCoastMips=prepared->clearCoast||std::any_of(f.coastUpdates.begin(),f.coastUpdates.end(),[&](const auto& u){return u->sequence>generation.coastSequence;});
	if(generation.coast&&needsCoastMips){auto filter=impl->SamplerFor(lease,F::MTLSamplerMinMagFilterLinear,F::MTLSamplerMinMagFilterLinear,M::MTLSamplerMipFilterNearest,W::MTLSamplerAddressModeClampToEdge);
		for(uint32_t level=1;level<generation.coast.mipmapLevelCount;++level){auto pass=Pass(generation.coast,MTLLoadActionDontCare);pass.colorAttachments[0].level=level;
			pass.renderTargetWidth=std::max(1u,f.mapSquaresX>>level);pass.renderTargetHeight=std::max(1u,f.mapSquaresZ>>level);const uint32_t sourceLevel=level-1;
			auto b=Buffer(lease,&sourceLevel,sizeof(sourceLevel),"Original RGB5 mip source level");auto arguments=Arguments(lease,1,1,1);[arguments setAddress:b.gpuAddress atIndex:0];
			[arguments setTexture:generation.coast.gpuResourceID atIndex:0];[arguments setSamplerState:filter.gpuResourceID atIndex:0];prepared->coastMips.push_back({pass,arguments,pipelines.coastMip});}
	}
	for(const auto& update:f.normalUpdates){
		if(update.sequence<=generation.normalSequence)continue;Require(generation.normal!=nil,"Original normal update without persistent destination");
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
		if(update.sequence<=generation.coastSequence)continue;
		auto atlas=Texture(lease,MTLPixelFormatRGBA8Unorm,update.atlasWidth,update.atlasHeight,1,MTLStorageModeShared,MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget,"Original packed water coast-update atlas");
		[atlas replaceRegion:MTLRegionMake2D(0,0,update.atlasWidth,update.atlasHeight) mipmapLevel:0 withBytes:update.rgba.data() bytesPerRow:update.atlasWidth*4];lease.AddAllocation(atlas);
		for(uint32_t radius=0;radius<=10;++radius){auto vertices=MakeNativeBumpWaterCoastVertices(update,radius);if(vertices.empty())continue;
			const std::array<int32_t,4> parameters={int32_t(radius),int32_t(radius&1),0,0};auto v=Buffer(lease,vertices.data(),vertices.size()*sizeof(vertices[0]),"Original coast update rectangles");auto p=Buffer(lease,parameters.data(),sizeof(parameters),"Original coast iteration args");auto args=Arguments(lease,2,2,1);
			[args setAddress:v.gpuAddress atIndex:0];[args setAddress:p.gpuAddress atIndex:1];[args setTexture:generation.coast.gpuResourceID atIndex:0];[args setTexture:atlas.gpuResourceID atIndex:1];[args setSamplerState:nearestClamp.gpuResourceID atIndex:0];
			auto destination=(radius&1)?atlas:generation.coast;prepared->updates.push_back({Pass(destination,MTLLoadActionLoad),args,(radius&1)?pipelines.atlas:pipelines.coast,uint32_t(vertices.size()),1,false,radius==10});
		}prepared->completion.coastSequence=update.sequence;
	}
	return std::unique_ptr<PreparedBumpWater>(new PreparedBumpWater(std::move(prepared)));
}
} // namespace rendering::metal4
