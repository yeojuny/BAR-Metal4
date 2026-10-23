/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4DynamicWater.h"
#include "NativeDynamicWaterShaders.h"
#include "Metal4WorldBCDecode.h"
#include "Rendering/Backend/NativeDynamicWaterTransaction.h"
#include "Rendering/Backend/NativeDynamicWaterTexture.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
#include <tuple>

namespace rendering::metal4 {
namespace {
struct alignas(16) Uniforms {
	WorldMatrix mvp{}, shadow{};
	std::array<std::array<float,4>,19> ve{}, fe{};
	std::array<float,4> fogColor{}, fogParameters{}, source{};
	std::array<uint32_t,4> flags{};
	std::array<std::array<uint32_t,4>,8> channels{};
};
static_assert(sizeof(Uniforms) == 928);
static_assert(sizeof(DynamicWaterVertex) == 32);
struct Sampled { id<MTLTexture> texture = nil; id<MTLSamplerState> sampler = nil; bool flip = false; DynamicWaterTextureChannels channels{}; };
struct MaterialGPU {
	std::shared_ptr<const NativeDynamicWaterMaterial> source;
	id<MTLTexture> raw = nil, splash = nil, boat = nil, foam = nil, zero = nil, up = nil;
	bool boatMips = false, foamMips = false;
};
struct Recurrence {
	std::shared_ptr<MaterialGPU> material;
	std::array<id<MTLTexture>,3> wave{};
	id<MTLTexture> detail = nil, height = nil;
	uint64_t sequence = 0, sourceDrawFrame = 0;
};
struct Owner {
	std::shared_ptr<Recurrence> committed;
	std::weak_ptr<DynamicWaterTransaction> active;
};
struct Draw {
	DynamicWaterPass pass;
	id<MTLTexture> target = nil;
	id<MTL4ArgumentTable> arguments = nil;
	id<MTLRenderPipelineState> pipeline = nil;
	id<MTLBuffer> indices = nil;
	uint32_t count = 0;
	bool mips = false;
};
id<MTLTexture> Texture(RuntimeLease& lease, MTLPixelFormat format, uint32_t width, uint32_t height,
	uint32_t levels, MTLStorageMode storage, MTLTextureUsage usage)
{
	auto d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:width height:height mipmapped:levels > 1];
	d.mipmapLevelCount = levels; d.storageMode = storage; d.usage = usage;
	auto t = [lease.Device() newTextureWithDescriptor:d];
	Require(t != nil, "Cannot allocate source dynamic-water texture"); lease.AddAllocation(t); return t;
}
id<MTLBuffer> Buffer(RuntimeLease& lease, const void* bytes, size_t size)
{
	Require(bytes && size, "Dynamic-water buffer is empty");
	auto b = [lease.Device() newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared];
	Require(b != nil, "Cannot allocate source dynamic-water buffer"); lease.AddAllocation(b); return b;
}
MTLPixelFormat Format(WorldTextureFormat f)
{
	switch (f) {
		case WorldTextureFormat::RGBA8Unorm: return MTLPixelFormatRGBA8Unorm;
		case WorldTextureFormat::BGRA8Unorm: return MTLPixelFormatBGRA8Unorm;
		case WorldTextureFormat::BC1RGBA: return MTLPixelFormatBC1_RGBA;
		case WorldTextureFormat::BC2RGBA: return MTLPixelFormatBC2_RGBA;
		case WorldTextureFormat::BC3RGBA: return MTLPixelFormatBC3_RGBA;
		case WorldTextureFormat::BC4Unorm: return MTLPixelFormatBC4_RUnorm;
		case WorldTextureFormat::BC5Unorm: return MTLPixelFormatBC5_RGUnorm;
		case WorldTextureFormat::BC7RGBA: return MTLPixelFormatBC7_RGBAUnorm;
		case WorldTextureFormat::BC7RGBASRGB: return MTLPixelFormatBC7_RGBAUnorm_sRGB;
	}
	throw std::invalid_argument("Dynamic-water source bitmap format unknown");
}
id<MTLTexture> Bitmap(RuntimeLease& lease, const WorldTexture& t, bool& mips)
{
	Require(!t.mips.empty(), "Dynamic-water bitmap has no actual bytes");
	const auto& b = t.mips.front();
	const uint32_t levels = t.generateMipmaps ? std::bit_width(std::max(b.width,b.height)) : uint32_t(t.mips.size());
	const auto decoded = DecodeWorldBCBaseForMipmaps(t, lease.Capabilities().maxTextureDimension2D);
	const auto format = decoded ? (t.format == WorldTextureFormat::BC7RGBASRGB ? MTLPixelFormatRGBA8Unorm_sRGB : MTLPixelFormatRGBA8Unorm) : Format(t.format);
	auto image = Texture(lease,format,b.width,b.height,levels,MTLStorageModeShared,MTLTextureUsageShaderRead);
	for (uint32_t i = 0; i < t.mips.size(); ++i) {
		const auto& mip = decoded ? *decoded : t.mips[i];
		[image replaceRegion:MTLRegionMake2D(0,0,mip.width,mip.height) mipmapLevel:i withBytes:mip.bytes.data() bytesPerRow:mip.bytesPerRow];
	}
	mips = t.generateMipmaps && levels > 1; return image;
}
id<MTLTexture> FloatImage(RuntimeLease& lease, const std::array<float,64*64*4>& data)
{
	std::array<uint16_t,64*64*4> half;
	std::transform(data.begin(),data.end(),half.begin(),DynamicWaterHalf);
	auto image = Texture(lease,MTLPixelFormatRGBA16Float,64,64,1,MTLStorageModeShared,MTLTextureUsageShaderRead);
	[image replaceRegion:MTLRegionMake2D(0,0,64,64) mipmapLevel:0 withBytes:half.data() bytesPerRow:64*8]; return image;
}
id<MTLSamplerState> Sampler(RuntimeLease& lease, bool linear, bool clamp, bool mips = false, bool interpolateMips = false)
{
	auto d = [MTLSamplerDescriptor new]; d.supportArgumentBuffers = YES;
	d.minFilter = d.magFilter = linear ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
	d.mipFilter = mips ? (interpolateMips ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest) : MTLSamplerMipFilterNotMipmapped;
	d.sAddressMode = d.tAddressMode = clamp ? MTLSamplerAddressModeClampToEdge : MTLSamplerAddressModeRepeat;
	auto s = [lease.Device() newSamplerStateWithDescriptor:d]; Require(s != nil,"Cannot allocate dynamic-water sampler"); lease.Retain(s); return s;
}
void ValidateProfile(const DynamicWaterSourceProfile& p, bool surface)
{
	Require(p.fragmentClampObserved && p.inheritedRasterObserved,
		"Dynamic-water float clamp/raster source profile is unobserved");
	if (surface) Require(p.surfaceProgram != DynamicWaterSurfaceProgram::Unobserved && p.shadowProgramStateObserved && p.surfaceFragmentClampObserved,
		"Dynamic-water surface program/shadow/clamp source profile is unobserved");
}
void ValidateDraw(const DynamicWaterDraw& d, const DynamicWaterSourceProfile& p)
{
	if (d.pass == DynamicWaterPass::OuterSurface) Require(p.outerQuadDiagonalObserved, "Dynamic-water outer nonplanar quad diagonal is unobserved");
	if (d.pass == DynamicWaterPass::BoatWake || d.pass == DynamicWaterPass::HoverWake || d.pass == DynamicWaterPass::Explosion)
		Require(p.impulseFogObserved && std::isfinite(p.impulseFogCoordinate), "Dynamic-water splash VP fog output has no reference profile");
	if (d.pass == DynamicWaterPass::DetailNormal)
		Require(p.detailFogObserved && std::isfinite(p.detailFogCoordinate), "Dynamic-water detail VP fog output has no reference profile");
	Require(d.indices.size() % 3 == 0 && d.indices.size() <= UINT32_MAX,"Dynamic-water index stream is invalid");
	for (auto i: d.indices) Require(i < d.vertices.size(),"Dynamic-water index escapes immutable source vertices");
	for (const auto& row: d.vertexEnv) for (float v: row) Require(std::isfinite(v),"Dynamic-water vertex parameter is not finite");
	for (const auto& row: d.fragmentEnv) for (float v: row) Require(std::isfinite(v),"Dynamic-water fragment parameter is not finite");
	Require((d.pass == DynamicWaterPass::Flow && (d.zeroTextureMask == 0 || d.zeroTextureMask == 63)) ||
		(d.pass == DynamicWaterPass::HeightFoam && (d.zeroTextureMask == 0 || d.zeroTextureMask == 1)) ||
		(d.pass != DynamicWaterPass::Flow && d.pass != DynamicWaterPass::HeightFoam && d.zeroTextureMask == 0),"Dynamic-water boundary reset mask differs from source");
}
std::pair<const char*,const char*> Functions(DynamicWaterPass p, DynamicWaterSurfaceProgram variant)
{
	switch (p) {
		case DynamicWaterPass::BoatWake: case DynamicWaterPass::HoverWake: case DynamicWaterPass::Explosion: return {"dwAddSplashVertex","dwAddSplashFragment"};
		case DynamicWaterPass::DetailNormal: return {"dwDetailNormalVertex","dwDetailNormalFragment"};
		case DynamicWaterPass::Flow: return {"waterDynWave2Vertex","waterDynWave2Fragment"};
		case DynamicWaterPass::HeightFoam: return {"waterDynWaveVertex","waterDynWaveFragment"};
		case DynamicWaterPass::Normal: return {"waterDynNormalVertex","waterDynNormalFragment"};
		case DynamicWaterPass::DerivedHeight: return {"waterDynWave3Vertex","waterDynWave3Fragment"};
		case DynamicWaterPass::Surface: case DynamicWaterPass::OuterSurface:
			return {variant == DynamicWaterSurfaceProgram::VertexTexture ? "waterDynVertex" : "waterDynNTVertex","waterDynFragment"};
	}
	throw std::invalid_argument("Unknown source dynamic-water pass");
}
MTL4RenderPassDescriptor* Pass(id<MTLTexture> image, MTLLoadAction load, MTLClearColor clear = MTLClearColorMake(0,0,0,0))
{
	auto d = [MTL4RenderPassDescriptor new]; d.colorAttachments[0].texture=image;
	d.colorAttachments[0].loadAction=load; d.colorAttachments[0].storeAction=MTLStoreActionStore;d.colorAttachments[0].clearColor=clear;
	d.renderTargetWidth=image.width;d.renderTargetHeight=image.height;return d;
}
}

struct PreparedDynamicWater::Impl {
	std::shared_ptr<Owner> owner;
	std::shared_ptr<DynamicWaterTransaction> transaction;
	std::shared_ptr<const NativeDynamicWaterFrame> frame;
	std::shared_ptr<Recurrence> previous, staging;
	std::array<id<MTLTexture>,3> initialWaves{};
	std::vector<Draw> updates, derived, surface;
	std::vector<CUSSourceImage> externals;
	std::vector<id<MTLTexture>> initialBitmapMips;
	id<MTLDepthStencilState> updateDepth=nil,surfaceDepth=nil;
	id<MTL4CommandBuffer> command=nil;
	void EncodeDraws(id<MTL4CommandBuffer> command, const std::vector<Draw>& draws) {
		for (const auto& draw: draws) {
			if (!draw.count) continue;
			auto e=[command renderCommandEncoderWithDescriptor:Pass(draw.target,MTLLoadActionLoad)];Require(e!=nil,"Cannot encode dynamic-water pass");
			[e barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
			[e setViewport:MTLViewport{0,0,double(draw.target.width),double(draw.target.height),0,1}];
			[e setScissorRect:MTLScissorRect{0,0,draw.target.width,draw.target.height}];
			[e setCullMode:MTLCullModeNone];[e setTriangleFillMode:MTLTriangleFillModeFill];[e setDepthStencilState:updateDepth];[e setDepthBias:0 slopeScale:0 clamp:0];
			[e setRenderPipelineState:draw.pipeline];[e setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
			[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.count indexType:MTLIndexTypeUInt32 indexBuffer:draw.indices.gpuAddress indexBufferLength:draw.indices.length];
			[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];
			if (draw.mips) GenerateMips(command,draw.target);
		}
	}
	static void GenerateMips(id<MTL4CommandBuffer> command,id<MTLTexture> image) {
		auto e=[command computeCommandEncoder];Require(e!=nil,"Cannot encode dynamic-water derived mip chain");
		[e barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		[e generateMipmapsForTexture:image];
		[e barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];
	}
};
struct DynamicWaterGPU::Impl {
	std::shared_ptr<Owner> owner=std::make_shared<Owner>();
	using Key=std::tuple<DynamicWaterPass,DynamicWaterSurfaceProgram,MTLPixelFormat,unsigned,bool>;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	std::map<bool,id<MTLLibrary>> libraries;
};
PreparedDynamicWater::PreparedDynamicWater(std::unique_ptr<Impl> p):impl(std::move(p)) {}
PreparedDynamicWater::~PreparedDynamicWater()=default;
DynamicWaterGPU::DynamicWaterGPU():impl(std::make_unique<Impl>()) {}
DynamicWaterGPU::~DynamicWaterGPU()=default;
void DynamicWaterGPU::ClearResources() {
	Require(impl->owner->active.expired(),"Cannot reset dynamic water with a live prepared submission");impl->owner->committed.reset();
}
void PreparedDynamicWater::EncodeUpdates(id<MTL4CommandBuffer> command)
{
	impl->transaction->BeginUpdates();
	try {
		Require(command!=nil,"Dynamic-water shared command buffer is absent");impl->command=command;
		for (auto& image:impl->externals) { if(image.encodeDependencies)image.encodeDependencies(command);if(image.requireReadable)image.requireReadable(); }
		if (impl->previous) {
			auto e=[command computeCommandEncoder];Require(e!=nil,"Cannot stage committed dynamic-water recurrence");
			[e barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
			auto copy=[&](id<MTLTexture> src,id<MTLTexture> dst){
				for(NSUInteger level=0;level<src.mipmapLevelCount;++level)
					[e copyFromTexture:src sourceSlice:0 sourceLevel:level sourceOrigin:MTLOriginMake(0,0,0)
					 sourceSize:MTLSizeMake(std::max<NSUInteger>(1,src.width>>level),std::max<NSUInteger>(1,src.height>>level),1)
					 toTexture:dst destinationSlice:0 destinationLevel:level destinationOrigin:MTLOriginMake(0,0,0)];};
			for(unsigned i=0;i<3;++i)copy(impl->previous->wave[i],impl->initialWaves[i]);
			copy(impl->previous->detail,impl->staging->detail);copy(impl->previous->height,impl->staging->height);
			[e barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];
		} else {
			std::vector<std::pair<id<MTLTexture>,MTLClearColor>> clears;
			for(unsigned i=0;i<3;++i)clears.emplace_back(impl->initialWaves[i],MTLClearColorMake(0,i==2?1:0,0,0));
			clears.emplace_back(impl->staging->height,MTLClearColorMake(0,0,0,0));
			// detailNormalTex's source initial allocation is undefined. Its full
			// original first-draw/update write is required before sampling; no
			// synthetic clear establishes detail readiness.
			for(auto [image,color]:clears){auto e=[command renderCommandEncoderWithDescriptor:Pass(image,MTLLoadActionClear,color)];Require(e!=nil,"Cannot initialize dynamic-water source images");
				[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];}
		}
		for(auto image:impl->initialBitmapMips)Impl::GenerateMips(command,image);
		impl->EncodeDraws(command,impl->updates);impl->transaction->EndUpdates();
	} catch (...) { impl->transaction->Fail();throw; }
}
void PreparedDynamicWater::EncodeDerived(id<MTL4CommandBuffer> command)
{
	impl->transaction->BeginDerived();
	try { Require(command==impl->command,"Dynamic-water derived pass moved to a different submission");impl->EncodeDraws(command,impl->derived);impl->transaction->EndDerived(); }
	catch (...) {impl->transaction->Fail();throw;}
}
void PreparedDynamicWater::EncodeSurface(id<MTL4RenderCommandEncoder> e)
{
	impl->transaction->BeginSurface();
	try {
		Require(e!=nil,"Dynamic-water surface encoder absent");
		const auto& r=impl->frame->raster;
		[e setCullMode:r.cullEnabled?(r.cull==NativeWorldCull::Front?MTLCullModeFront:r.cull==NativeWorldCull::Back?MTLCullModeBack:MTLCullModeNone):MTLCullModeNone];
		[e setFrontFacingWinding:MTLWindingCounterClockwise];[e setDepthClipMode:r.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];
		[e setTriangleFillMode:impl->frame->wireframe?MTLTriangleFillModeLines:MTLTriangleFillModeFill];[e setDepthBias:0 slopeScale:0 clamp:0];[e setDepthStencilState:impl->surfaceDepth];
		for(const auto& draw:impl->surface){if(!draw.count||(r.cullEnabled&&r.cull==NativeWorldCull::FrontAndBack))continue;
			[e setRenderPipelineState:draw.pipeline];[e setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
			[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.count indexType:MTLIndexTypeUInt32 indexBuffer:draw.indices.gpuAddress indexBufferLength:draw.indices.length];}
		impl->transaction->EndSurface();
	} catch (...) {impl->transaction->Fail();throw;}
}
DynamicWaterCompletion PreparedDynamicWater::DidComplete()
{
	impl->transaction->Retire();
	// External retirement must finish before committing the recurrence head.
	for(auto& image:impl->externals)if(image.didComplete)image.didComplete();
	impl->owner->committed=impl->staging;
	return {impl->frame->material->generation,impl->frame->throughUpdate,impl->frame->sourceDrawFrame};
}

std::unique_ptr<PreparedDynamicWater> DynamicWaterGPU::Prepare(RuntimeLease& lease,
	std::shared_ptr<const NativeDynamicWaterFrame> frame,const DynamicWaterSceneImages& scene,
	const std::function<CUSSourceImage(const NativeCUSTextureBinding&)>& resolve,
	MTLPixelFormat color,unsigned samples,bool validation,double& compileMs)
{
	Require(frame && frame->material && frame->material->generation && resolve,"Dynamic water has no actual source material/provider");
	Require(impl->owner->active.expired(),"Dynamic-water previous prepared submission has not retired or been discarded");
	ValidateProfile(frame->profile,true);
	Require(scene.reflection && scene.refraction && scene.reflection.width==512 && scene.reflection.height==512 &&
		scene.refraction.width==frame->material->refractionExtent && scene.refraction.height==frame->material->refractionExtent,
		"Dynamic water requires its actual512 reflection and original-sized refraction");
	Require(scene.reflection.pixelFormat==MTLPixelFormatRGBA8Unorm && scene.refraction.pixelFormat==MTLPixelFormatRGBA8Unorm,
		"Dynamic-water scene images lost original RGBA8 storage");
	const auto& raster=frame->raster;const auto& blend=frame->blend;
	Require(!raster.stencilTest && !raster.alphaToCoverage && !raster.polygonOffset && !raster.inheritedClipPlane &&
		raster.colorWrite==std::array<bool,4>{true,true,true,true} && !raster.lineStipple && raster.lineWidth==1 &&
		raster.frontPolygonMode==NativePolygonMode::Fill && raster.backPolygonMode==NativePolygonMode::Fill,"Dynamic-water surface raster state is outside its observed profile");
	Require(blend.enabled && blend.sourceColor==ScreenBlendFactor::SourceAlpha && blend.sourceAlpha==ScreenBlendFactor::SourceAlpha &&
		blend.destinationColor==ScreenBlendFactor::OneMinusSourceAlpha && blend.destinationAlpha==ScreenBlendFactor::OneMinusSourceAlpha &&
		blend.colorEquation==ScreenBlendEquation::Add && blend.alphaEquation==ScreenBlendEquation::Add,
		"Dynamic-water surface blend state differs from the observed source contract");
	auto p=std::make_unique<PreparedDynamicWater::Impl>();p->owner=impl->owner;p->frame=frame;
	p->transaction=std::make_shared<DynamicWaterTransaction>();
	const auto committed=impl->owner->committed;
	if(committed){
		Require(frame->material->generation>=committed->material->source->generation,"Dynamic-water material generation went backwards");
		if(frame->material->generation==committed->material->source->generation){Require(frame->material==committed->material->source,"Dynamic-water immutable material changed within a generation");p->previous=committed;}
	}
	if(p->previous) Require(frame->sourceDrawFrame>p->previous->sourceDrawFrame,"Dynamic-water retired draw reservation was replayed");
	const uint64_t completed=p->previous?p->previous->sequence:0;
	std::vector<uint64_t> sequences;
	for(const auto& update:frame->updates){Require(update && update->materialGeneration==frame->material->generation,"Dynamic-water queued update belongs to another generation");sequences.push_back(update->sequence);}
	ValidateDynamicWaterSequences(completed,frame->throughUpdate,sequences);
	p->staging=std::make_shared<Recurrence>();p->staging->sequence=frame->throughUpdate;p->staging->sourceDrawFrame=frame->sourceDrawFrame;
	if(p->previous)p->staging->material=p->previous->material;
	else {
		Require(frame->material->boat && frame->material->foam,"Dynamic-water original bitmap owners are missing");
		auto m=std::make_shared<MaterialGPU>();m->source=frame->material;
		m->raw=FloatImage(lease,frame->material->rawBump);m->splash=FloatImage(lease,frame->material->splash);
		m->boat=Bitmap(lease,*frame->material->boat,m->boatMips);m->foam=Bitmap(lease,*frame->material->foam,m->foamMips);
		m->zero=Texture(lease,MTLPixelFormatRGBA8Unorm,1,1,1,MTLStorageModeShared,MTLTextureUsageShaderRead);
		m->up=Texture(lease,MTLPixelFormatRGBA8Unorm,1,1,1,MTLStorageModeShared,MTLTextureUsageShaderRead);
		const uint8_t zero[4]={0,0,0,0},up[4]={0,255,0,0};
		[m->zero replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:zero bytesPerRow:4];
		[m->up replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:up bytesPerRow:4];
		if(m->boatMips)p->initialBitmapMips.push_back(m->boat);if(m->foamMips)p->initialBitmapMips.push_back(m->foam);
		p->staging->material=std::move(m);
	}
	const auto usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;
	for(auto& image:p->staging->wave)image=Texture(lease,MTLPixelFormatRGBA16Float,1024,1024,1,MTLStorageModePrivate,usage);
	p->initialWaves=p->staging->wave;
	p->staging->height=Texture(lease,MTLPixelFormatRGBA32Float,256,256,1,MTLStorageModePrivate,usage);
	p->staging->detail=Texture(lease,MTLPixelFormatRGBA16Float,256,256,9,MTLStorageModePrivate,usage);
	const auto& material=*p->staging->material;
	for(auto t:{material.raw,material.splash,material.boat,material.foam,material.zero,material.up})lease.AddAllocation(t);
	if(p->previous){for(auto t:p->previous->wave)lease.AddAllocation(t);lease.AddAllocation(p->previous->height);lease.AddAllocation(p->previous->detail);}
	lease.AddAllocation(scene.reflection);lease.AddAllocation(scene.refraction);
	const auto nearest=Sampler(lease,false,false),linear=Sampler(lease,true,false),linearClamp=Sampler(lease,true,true),nearestClamp=Sampler(lease,false,true),mip=Sampler(lease,true,false,true),bitmapMip=Sampler(lease,true,false,true,true);
	auto depth=[MTLDepthStencilDescriptor new];depth.depthCompareFunction=MTLCompareFunctionAlways;depth.depthWriteEnabled=NO;
	p->updateDepth=[lease.Device() newDepthStencilStateWithDescriptor:depth];Require(p->updateDepth!=nil,"Dynamic-water update depth state missing");lease.Retain(p->updateDepth);
	const auto compare=[](ScreenAlphaCompare c){switch(c){
		case ScreenAlphaCompare::Never:return MTLCompareFunctionNever;case ScreenAlphaCompare::Less:return MTLCompareFunctionLess;
		case ScreenAlphaCompare::Equal:return MTLCompareFunctionEqual;case ScreenAlphaCompare::LessEqual:return MTLCompareFunctionLessEqual;
		case ScreenAlphaCompare::Greater:return MTLCompareFunctionGreater;case ScreenAlphaCompare::NotEqual:return MTLCompareFunctionNotEqual;
		case ScreenAlphaCompare::GreaterEqual:return MTLCompareFunctionGreaterEqual;case ScreenAlphaCompare::Always:return MTLCompareFunctionAlways;}
		throw std::invalid_argument("Dynamic-water source depth comparison unknown");};
	depth=[MTLDepthStencilDescriptor new];depth.depthCompareFunction=raster.depthTest?compare(raster.depthCompare):MTLCompareFunctionAlways;depth.depthWriteEnabled=raster.depthWrite;
	p->surfaceDepth=[lease.Device() newDepthStencilStateWithDescriptor:depth];Require(p->surfaceDepth!=nil,"Dynamic-water surface depth state missing");lease.Retain(p->surfaceDepth);
	std::map<const void*,size_t> externalCache;
	auto external=[&](const NativeCUSTextureBinding& binding,bool depthRequired=false)->std::pair<Sampled,bool>{
		if(binding.knowledge==NativeCUSBindingKnowledge::DefaultObject){
			const auto channels=DynamicWaterExternalChannels(binding,depthRequired,false);
			// Bind an already-owned resource for Metal argument validity only.
			// The generated TEX branch returns the specified incomplete constant
			// without sampling it; no invented object-zero pixels or owner.
			return {{material.zero,nearest,false,channels},false};
		}
		Require(binding.object!=nullptr,"Dynamic-water source image has no immutable readable owner");
		auto [it,inserted]=externalCache.emplace(binding.object.get(),p->externals.size());
		if(inserted){auto image=resolve(binding);Require(image.texture && image.sampler && image.owner && image.requireReadable,
			"Dynamic-water external image lost actual texture/sampler/storage/readability owner");
			lease.AddAllocation(image.texture);lease.Retain(image.sampler);p->externals.push_back(std::move(image));}
		const auto& image=p->externals[it->second];
		const auto channels=DynamicWaterExternalChannels(binding,depthRequired,image.depthImage);
		return {{image.texture,image.sampler,image.flipV,channels},image.clampDepthCompare};
	};
	auto pipeline=[&](DynamicWaterPass pass,MTLPixelFormat format,unsigned count)->id<MTLRenderPipelineState>{
		const bool surface=pass==DynamicWaterPass::Surface||pass==DynamicWaterPass::OuterSurface;
		const auto variant=surface?frame->profile.surfaceProgram:DynamicWaterSurfaceProgram::Unobserved;
		const Impl::Key key{pass,variant,format,lease.SampleShadingKey(count),validation};
		auto found=impl->pipelines.find(key);if(found!=impl->pipelines.end()){lease.Retain(found->second);return found->second;}
		const auto start=Clock::now();auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
		NSError* error=nil;
		// RuntimeLease injects the same startup-effective sample-shading macros
		// used by other native scene shaders. Key tracks its effective mode.
		auto library=lease.NewLibraryWithSource(String(NativeDynamicWaterShaders),options,&error);Require(library!=nil,"Dynamic-water shader compilation failed: "+Error(error));
		auto d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=count;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
		auto [vs,fs]=Functions(pass,variant);auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];
		v.library=f.library=library;v.name=String(vs);f.name=String(fs);d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;
		auto a=d.colorAttachments[0];a.pixelFormat=format;
		const bool impulse=pass==DynamicWaterPass::BoatWake||pass==DynamicWaterPass::HoverWake||pass==DynamicWaterPass::Explosion;
		a.blendingState=surface||impulse?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
		if(surface||impulse){a.rgbBlendOperation=a.alphaBlendOperation=MTLBlendOperationAdd;
			a.sourceRGBBlendFactor=a.sourceAlphaBlendFactor=impulse?MTLBlendFactorOne:MTLBlendFactorSourceAlpha;
			a.destinationRGBBlendFactor=a.destinationAlphaBlendFactor=impulse?MTLBlendFactorOne:MTLBlendFactorOneMinusSourceAlpha;}
		auto po=[MTL4PipelineOptions new];po.shaderValidation=ShaderValidationMode(validation);d.options=po;
		auto result=lease.NewRenderPipeline(d,&error);Require(result!=nil,"Dynamic-water pipeline compilation failed: "+Error(error));
		impl->pipelines.emplace(key,result);lease.Retain(result);compileMs+=Milliseconds(start);return result;
	};
	bool detailReady=bool(p->previous);
	auto prepareDraw=[&](const DynamicWaterDraw& source,const DynamicWaterSourceProfile& profile,
		const std::array<float,4>& fogColor,const std::array<float,4>& fogParams,
		const NativeCUSTextureBinding& shadingBinding,const NativeCUSTextureBinding& hoverBinding)->Draw{
		ValidateDraw(source,profile);Draw draw{};draw.pass=source.pass;draw.count=uint32_t(source.indices.size());
		if(!draw.count)return draw;
		std::array<Sampled,8> textures{};const auto& wave=p->staging->wave;
		const auto sample=[&](id<MTLTexture> t,id<MTLSamplerState> s,bool flip=true){return Sampled{t,s,flip};};
		bool surface=false,shadowClamp=false;
		switch(source.pass){
			case DynamicWaterPass::BoatWake:draw.target=wave[0];textures[0]=sample(material.boat,material.source->boat->mipmappedSampling?bitmapMip:linear,false);break;
			case DynamicWaterPass::HoverWake:draw.target=wave[0];textures[0]=external(hoverBinding).first;break;
			case DynamicWaterPass::Explosion:draw.target=wave[0];textures[0]=sample(material.splash,linearClamp,false);break;
			case DynamicWaterPass::DetailNormal:draw.target=p->staging->detail;for(auto& t:textures)t=sample(material.raw,linear,false);draw.mips=true;detailReady=true;break;
			case DynamicWaterPass::Flow:draw.target=wave[2];textures[0]=sample(wave[1],nearest);for(unsigned i=1;i<=5;++i)textures[i]=sample(wave[0],nearest);textures[6]=external(shadingBinding).first;break;
			case DynamicWaterPass::HeightFoam:Require(detailReady,"Dynamic-water height consumes uninitialized detail normals");draw.target=wave[1];textures[0]=sample(wave[0],nearest);for(unsigned i=1;i<=5;++i)textures[i]=sample(wave[2],nearest);textures[6]=sample(p->staging->detail,mip);break;
			case DynamicWaterPass::Normal:draw.target=wave[0];for(unsigned i=0;i<4;++i)textures[i]=sample(wave[1],linear);break;
			case DynamicWaterPass::DerivedHeight:draw.target=p->staging->height;textures[0]=sample(wave[0],linear);break;
			case DynamicWaterPass::Surface:case DynamicWaterPass::OuterSurface:
				Require(detailReady,"Dynamic-water surface consumes uninitialized detail normals");surface=true;
				textures[0]=source.pass==DynamicWaterPass::OuterSurface?sample(material.up,linear,false):sample(wave[2],linear);
				textures[1]=sample(scene.reflection,linearClamp,scene.reflectionFlipV);textures[2]=sample(p->staging->height,nearestClamp);
				textures[3]=sample(scene.refraction,linearClamp,scene.refractionFlipV);textures[4]=external(shadingBinding).first;
				textures[5]=sample(material.foam,material.source->foam->mipmappedSampling?bitmapMip:linear,false);textures[6]=sample(p->staging->detail,mip);
				{auto image=external(frame->shadow,true);textures[7]=image.first;shadowClamp=image.second;}break;
		}
		for(unsigned i=0;i<8;++i)if(source.zeroTextureMask&(1u<<i))textures[i]=sample(material.zero,linear,false);
		Uniforms u{};u.ve=source.vertexEnv;u.fe=source.fragmentEnv;u.fogColor=fogColor;u.fogParameters=fogParams;
		if(surface){u.mvp=frame->viewProjection;u.shadow=frame->shadowMatrix;u.source[1]=float(frame->captureCamera.clipDepth==ClipDepthConvention::MinusOneToOne);}
		else {u.mvp={2,0,0,0,0,2,0,0,0,0,-1,0,-1,-1,0,1};u.source[1]=1;}
		if(source.pass==DynamicWaterPass::DetailNormal)u.source[0]=profile.detailFogCoordinate;
		if(source.pass==DynamicWaterPass::BoatWake||source.pass==DynamicWaterPass::HoverWake||source.pass==DynamicWaterPass::Explosion)u.source[0]=profile.impulseFogCoordinate;
		// Validate the source-owned floating target independently of the resolved
		// clamp profile. Source-context identity is not inferred from configuration.
		if(!surface)Require(draw.target.pixelFormat==(source.pass==DynamicWaterPass::DerivedHeight?MTLPixelFormatRGBA32Float:MTLPixelFormatRGBA16Float),
			"Dynamic-water float pass target differs from its source allocation");
		u.flags[1]=surface?profile.surfaceFragmentClamp:profile.fragmentClamp;u.flags[2]=shadowClamp;
		for(unsigned i=0;i<8;++i){
			u.channels[i]=textures[i].channels.channels;
			if(textures[i].flip)u.flags[0]|=1u<<i;
			if(textures[i].channels.incompleteDefault)u.flags[3]|=1u<<i;
		}
		draw.pipeline=pipeline(source.pass,surface?color:draw.target.pixelFormat,surface?samples:1);
		auto vertices=Buffer(lease,source.vertices.data(),source.vertices.size()*sizeof(DynamicWaterVertex));
		draw.indices=Buffer(lease,source.indices.data(),source.indices.size()*sizeof(uint32_t));auto uniform=Buffer(lease,&u,sizeof(u));
		auto ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=2;ad.maxTextureBindCount=8;ad.maxSamplerStateBindCount=8;ad.initializeBindings=YES;
		draw.arguments=[lease.Device() newArgumentTableWithDescriptor:ad error:nil];Require(draw.arguments!=nil,"Dynamic-water argument allocation failed");lease.Retain(draw.arguments);
		[draw.arguments setAddress:vertices.gpuAddress atIndex:0];[draw.arguments setAddress:uniform.gpuAddress atIndex:1];
		for(unsigned i=0;i<8;++i)if(textures[i].texture){[draw.arguments setTexture:textures[i].texture.gpuResourceID atIndex:i];[draw.arguments setSamplerState:textures[i].sampler.gpuResourceID atIndex:i];}
		return draw;
	};
	for(const auto& update:frame->updates){
		if(update->sequence<=completed)continue;
		ValidateProfile(update->profile,false);unsigned last=0;bool detail=false,flow=false,height=false,normal=false;
		for(const auto& d:update->draws){const auto rank=unsigned(d.pass);Require(rank<=unsigned(DynamicWaterPass::Normal)&&rank>=last,"Dynamic-water simulation pass order differs from source");last=rank;
			detail|=d.pass==DynamicWaterPass::DetailNormal;flow|=d.pass==DynamicWaterPass::Flow;height|=d.pass==DynamicWaterPass::HeightFoam;normal|=d.pass==DynamicWaterPass::Normal;
			p->updates.push_back(prepareDraw(d,update->profile,update->fogColor,update->fogParameters,update->shading,update->hoverDefault));}
		Require(detail&&flow&&height&&normal,"Dynamic-water update omits a required recurrence stage");
		const auto old=p->staging->wave;p->staging->wave={old[1],old[2],old[0]};
	}
	Require(!frame->derived.empty() && frame->derived.front().pass==DynamicWaterPass::DerivedHeight,"Dynamic-water draw lost its source derived-height pass");
	for(size_t i=0;i<frame->derived.size();++i){const auto& d=frame->derived[i];Require(i==0?d.pass==DynamicWaterPass::DerivedHeight:d.pass==DynamicWaterPass::DetailNormal,"Dynamic-water derived pass order differs from source");
		p->derived.push_back(prepareDraw(d,frame->profile,frame->fogColor,frame->fogParameters,frame->shading,frame->material->hoverDefault));}
	Require(detailReady,"Dynamic-water first draw omitted original detail initialization");
	for(size_t i=0;i<frame->surface.size();++i){const auto& d=frame->surface[i];Require(i==0?d.pass==DynamicWaterPass::Surface:d.pass==DynamicWaterPass::OuterSurface,"Dynamic-water surface order differs from source");
		p->surface.push_back(prepareDraw(d,frame->profile,frame->fogColor,frame->fogParameters,frame->shading,frame->material->hoverDefault));}
	Require(!frame->surface.empty(),"Dynamic-water source surface stream absent");
	impl->owner->active=p->transaction;
	return std::unique_ptr<PreparedDynamicWater>(new PreparedDynamicWater(std::move(p)));
}
}
