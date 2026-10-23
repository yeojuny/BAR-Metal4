/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4WeaponRangeNative.h"
#include "NativeWeaponRangeShaders.h"
#include "Rendering/Metal4/Metal4NativeEffectState.h"
#include <map>
#include <tuple>
#include <unordered_map>
#include <cmath>

namespace rendering::metal4 {
namespace {
id<MTLBuffer> Upload(RuntimeLease& lease,const void* source,size_t size)
{
	Require(source && size && size<=lease.Device().maxBufferLength,"Original weapon range buffer exceeds device capacity");
	auto result=[lease.Device()newBufferWithBytes:source length:size options:MTLResourceStorageModeShared];
	Require(result!=nil,"Cannot allocate original weapon range buffer");lease.AddAllocation(result);return result;
}
MTLBlendOperation Equation(ScreenBlendEquation value)
{
	switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}
	throw std::logic_error("Unknown weapon range blend equation");
}
uint64_t PipelineBits(const ScreenBlendState& b,const std::array<bool,4>& m)
{
	return uint64_t(b.enabled)|uint64_t(b.sourceColor)<<1|uint64_t(b.destinationColor)<<5|uint64_t(b.sourceAlpha)<<9|uint64_t(b.destinationAlpha)<<13|
		uint64_t(m[0])<<17|uint64_t(m[1])<<18|uint64_t(m[2])<<19|uint64_t(m[3])<<20|uint64_t(b.colorEquation)<<21|uint64_t(b.alphaEquation)<<24;
}
}
struct PreparedWeaponRangeDraw::Impl {
	__strong id<MTLRenderPipelineState> pipeline=nil;
	__strong id<MTLDepthStencilState> depth=nil;
	__strong id<MTL4ArgumentTable> arguments=nil;
	NativeWeaponRangeDraw source;
	PreparedStartBoxImage height,coverageMask;
	NativeWorldDrawState rasterState;
	effectstate::Raster raster;
	uint32_t count=0,instances=0;
};
PreparedWeaponRangeDraw::PreparedWeaponRangeDraw():impl(std::make_unique<Impl>()){}
PreparedWeaponRangeDraw::~PreparedWeaponRangeDraw()=default;
bool PreparedWeaponRangeDraw::Empty()const{return !impl->count || !impl->instances;}
void PreparedWeaponRangeDraw::Encode(id<MTL4RenderCommandEncoder> encoder)const
{
	if(Empty())return;
	if(impl->height.requireReadable)impl->height.requireReadable();
	if(impl->coverageMask.requireReadable)impl->coverageMask.requireReadable();
	effectstate::RasterState(encoder,impl->rasterState,impl->raster);
	const auto cull=impl->rasterState.cullEnabled?impl->rasterState.cull:NativeWorldCull::None;
	[encoder setCullMode:cull==NativeWorldCull::Front?MTLCullModeFront:cull==NativeWorldCull::Back?MTLCullModeBack:MTLCullModeNone];
	[encoder setDepthStencilState:impl->depth];[encoder setRenderPipelineState:impl->pipeline];
	[encoder setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:impl->count instanceCount:impl->instances];
}
struct WeaponRangeGPU::Impl {
	using PipelineKey=std::tuple<unsigned,MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,unsigned,bool,uint64_t,bool>;
	__strong id<MTLLibrary> library=nil;
	std::map<PipelineKey,id<MTLRenderPipelineState>> pipelines;
	struct CachedBuffer {std::weak_ptr<const NativeBufferSnapshot> original;__strong id<MTLBuffer> buffer=nil;};
	std::unordered_map<const NativeBufferSnapshot*,CachedBuffer> buffers;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& source)
	{
		auto& entry=buffers[source.get()];
		if(entry.original.lock()!=source)entry={source,Upload(lease,source->bytes.data(),source->bytes.size())};
		lease.AddAllocation(entry.buffer);return entry.buffer;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const NativeWeaponRangeDraw& draw,
		MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& ms)
	{
		const bool line=draw.primitive==NativeDrawPrimitive::LineLoop;
		const unsigned program=draw.program.defines->composite?5:draw.program.defines->maskPass?2:draw.program.maskClip>0.5f?(line?3:4):(line?1:0);
		const PipelineKey key{program,color,depth,stencil,lease.SampleShadingKey(samples),validation,PipelineBits(draw.surface.blend,draw.raster.colorWrite),draw.raster.alphaToCoverage};
		if(auto found=pipelines.find(key);found!=pipelines.end()){lease.Retain(found->second);return found->second;}
		const auto start=Clock::now();NSError* error=nil;
		if(!library){auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Exact31657 original Attack/Defense range coverage and composite";descriptor.source=String(NativeWeaponRangeShaderSource);
			auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;
			library=lease.NewLibrary(descriptor,&error);Require(library!=nil,"Cannot compile original weapon range MSL: "+Error(error));}
		lease.Retain(library);auto descriptor=[MTL4RenderPipelineDescriptor new];
		auto vertex=[MTL4LibraryFunctionDescriptor new],fragment=[MTL4LibraryFunctionDescriptor new];vertex.library=fragment.library=library;
		vertex.name=program==5?@"OriginalWeaponRangeComposite":line?@"OriginalWeaponRangeLine":@"OriginalWeaponRangeFill";
		constexpr const char* fragments[]={"OriginalWeaponRangeFillFragment","OriginalWeaponRangeLineFragment","OriginalWeaponRangeMaskFragment","OriginalWeaponRangeMaskedLineFragment","OriginalWeaponRangeMaskedFillFragment","OriginalWeaponRangeCompositeFragment"};
		fragment.name=String(std::string(fragments[program])+((samples>1&&(program==1||program==3))?"MSAA":""));
		descriptor.vertexFunctionDescriptor=vertex;descriptor.fragmentFunctionDescriptor=fragment;descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;descriptor.rasterSampleCount=samples;descriptor.alphaToCoverageState=draw.raster.alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		const auto& b=draw.surface.blend;const auto& m=draw.raster.colorWrite;auto attachment=descriptor.colorAttachments[0];attachment.pixelFormat=color;
		attachment.blendingState=b.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
		attachment.sourceRGBBlendFactor=effectstate::Factor(b.sourceColor);attachment.destinationRGBBlendFactor=effectstate::Factor(b.destinationColor);
		attachment.sourceAlphaBlendFactor=effectstate::Factor(b.sourceAlpha);attachment.destinationAlphaBlendFactor=effectstate::Factor(b.destinationAlpha);
		attachment.rgbBlendOperation=Equation(b.colorEquation);attachment.alphaBlendOperation=Equation(b.alphaEquation);
		attachment.writeMask=MTLColorWriteMask((m[0]?MTLColorWriteMaskRed:0)|(m[1]?MTLColorWriteMaskGreen:0)|(m[2]?MTLColorWriteMaskBlue:0)|(m[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;
		auto result=lease.NewRenderPipeline(descriptor,&error);Require(result!=nil,"Cannot compile original weapon range pipeline: "+Error(error));
		pipelines.emplace(key,result);lease.Retain(result);ms+=Milliseconds(start);return result;
	}
};
WeaponRangeGPU::WeaponRangeGPU():impl(std::make_unique<Impl>()){}
WeaponRangeGPU::~WeaponRangeGPU()=default;
void WeaponRangeGPU::ClearResources(){impl->buffers.clear();}
std::unique_ptr<PreparedWeaponRangeDraw> WeaponRangeGPU::Prepare(RuntimeLease& lease,const NativeWeaponRangeDraw& draw,
	const StartBoxImageResolver& resolver,MTLViewport viewport,MTLScissorRect scissor,std::array<std::uint32_t,2> targetExtent,
	MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& milliseconds,
	std::optional<NativePostprocessConsumer> consumer)
{
	ValidateNativeWeaponRangeDraw(draw);
	auto result=std::unique_ptr<PreparedWeaponRangeDraw>(new PreparedWeaponRangeDraw);
	const bool line=draw.primitive==NativeDrawPrimitive::LineLoop;
	if(!draw.instanceCount || !scissor.width || !scissor.height || (!line && draw.raster.cullEnabled && draw.raster.cull==NativeWorldCull::FrontAndBack))return result;
	// The native aliased-line transport uses bounded signed fixed-point terms.
	// Its 8-bit subpixel precision remains a reference-pixel acceptance gate.
	for(double value:{viewport.originX,viewport.originY,viewport.width,viewport.height})
		Require(std::isfinite(value) && std::abs(value)<=16384,"Weapon range viewport exceeds validated raster bounds");
	Require(viewport.width>0 && viewport.height>0 && viewport.znear==0 && viewport.zfar==1,"Weapon range requires original full clip-depth viewport");
	Require(targetExtent[0] && targetExtent[1] && targetExtent[0]<=16384 && targetExtent[1]<=16384,"Original weapon range target extent exceeds raster bounds");
	Require(samples&&[lease.Device() supportsTextureSampleCount:samples],"Weapon range output sample count unsupported");
	Require(depth==MTLPixelFormatInvalid || depth==MTLPixelFormatDepth16Unorm || depth==MTLPixelFormatDepth32Float || depth==MTLPixelFormatDepth32Float_Stencil8,"Original weapon range actual depth attachment format differs");
	Require(stencil==MTLPixelFormatInvalid || stencil==MTLPixelFormatStencil8 || stencil==MTLPixelFormatDepth32Float_Stencil8,"Original weapon range actual stencil attachment format differs");
 Require(bool(resolver),"Original31657 range images require actual shared owners");
 const bool composite=draw.program.defines->composite;
 if(draw.program.defines->maskPass)Require(samples==1 && color==MTLPixelFormatR8Unorm && depth==MTLPixelFormatDepth16Unorm && stencil==MTLPixelFormatInvalid,"Original31657 coverage requires its actual R8/D16 single-sample target");
 std::erase_if(impl->buffers,[](const auto& entry){return entry.second.original.expired();});
 auto& out=*result->impl;out.source=draw;out.rasterState=draw.raster;out.raster={viewport,scissor};
 if(!composite){
  auto image=resolver(lease,draw.height,consumer);
  Require(image.kind==StartBoxImageKind::Color && image.original==draw.height.object && image.texture && image.sampler && bool(image.storageOwner),"Weapon range resolver returned different height storage or sampler");
  const auto& allocation=image.original->allocation;
  Require(image.texture.textureType==MTLTextureType2D && image.texture.pixelFormat==MTLPixelFormatR32Float && image.texture.width==allocation.width && image.texture.height==allocation.height && image.texture.mipmapLevelCount==allocation.levels && (image.texture.usage&MTLTextureUsageShaderRead),"Original range requires real R32F height storage");
  Require(image.original->sampling.swizzle[0]==0x1903 && image.texture.swizzle.red==MTLTextureSwizzleRed,"Original range height red-channel mapping differs");
  out.height=std::move(image);
 }
 if(composite || draw.program.maskClip>0.5f){
  Require(draw.program.inMiniMap<0.5f || draw.surface.viewport.has_value(),"Original31657 minimap mask read requires its observed source viewport");
  auto image=resolver(lease,draw.coverageMask,consumer);
  Require(image.kind==StartBoxImageKind::Color && image.original==draw.coverageMask.object && image.texture && bool(image.storageOwner),"Original31657 mask resolver returned different image/version owner");
  Require(image.texture.textureType==MTLTextureType2D && image.texture.pixelFormat==MTLPixelFormatR8Unorm && image.texture.width==image.original->allocation.width && image.texture.height==image.original->allocation.height && image.texture.mipmapLevelCount==1 && (image.texture.usage&MTLTextureUsageShaderRead),"Original31657 coverage must sample its actual readable R8 image");
  Require(image.original->sampling.swizzle[0]==0x1903 && image.texture.swizzle.red==MTLTextureSwizzleRed,"Original31657 coverage red mapping differs");
  out.coverageMask=std::move(image);
 }
	// Original GL tests pass without writes when the actual FBO has no
	// corresponding attachment. Preserve the source capture and derive only
	// effective native state, as the common screen consumer already does.
	if(depth==MTLPixelFormatInvalid){out.rasterState.depthTest=false;out.rasterState.depthWrite=false;}
	if(stencil==MTLPixelFormatInvalid)out.rasterState.stencilTest=false;
	// GL face culling and polygon-offset fill do not apply to LINE_LOOP.
	if(line){out.rasterState.cullEnabled=false;out.rasterState.polygonOffset=false;
		// GL viewport maps and clips the center line, but does not scissor
		// its expanded width. Keep the captured scissor and rasterize the
		// coverage quads in the full attachment viewport.
		out.raster.viewport={0,0,double(targetExtent[0]),double(targetExtent[1]),0,1};}
 auto u=ReadNativeWeaponRangeUniforms(draw);u.options[2]=unsigned(out.height.flipV);u.programOptions[3]=unsigned(out.coverageMask.flipV);
 u.viewport={float(viewport.originX),float(viewport.originY),float(viewport.width),float(viewport.height)};
 u.outputViewport={0,0,float(targetExtent[0]),float(targetExtent[1])};
 u.renderOptions={unsigned(line),unsigned(draw.surface.alphaTest.compare)|(unsigned(draw.surface.alphaTest.enabled)<<8),0,0};
 if(out.coverageMask.texture)Require(u.viewGeometry[0]>0 && u.viewGeometry[1]>0 && out.coverageMask.texture.width==u.viewGeometry[0] && out.coverageMask.texture.height==u.viewGeometry[1],"Original31657 mask extent differs from source world view");
 auto vertices=impl->Buffer(lease,draw.vertices),uniforms=Upload(lease,&u,sizeof(u));
 auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=4;descriptor.maxTextureBindCount=2;descriptor.maxSamplerStateBindCount=1;descriptor.initializeBindings=YES;
 NSError* error=nil;out.arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(out.arguments!=nil,"Cannot allocate original31657 range arguments: "+Error(error));lease.Retain(out.arguments);
 [out.arguments setAddress:vertices.gpuAddress+size_t(draw.firstVertex)*16 atIndex:0];[out.arguments setAddress:uniforms.gpuAddress atIndex:3];
 if(!composite){
  auto instances=impl->Buffer(lease,draw.instances);auto positions=ReadNativeWeaponRangeModelPositions(draw);
  // Static source does not read model rows. This remains an unused ABI binding only.
  std::array<float,4> unused{};auto models=positions.empty()?Upload(lease,unused.data(),sizeof(unused)):Upload(lease,positions.data(),positions.size()*sizeof(positions.front()));
  [out.arguments setAddress:instances.gpuAddress+size_t(draw.firstInstance)*96 atIndex:1];[out.arguments setAddress:models.gpuAddress atIndex:2];
  lease.AddAllocation(out.height.texture);lease.Retain(out.height.sampler);
  [out.arguments setTexture:out.height.texture.gpuResourceID atIndex:0];[out.arguments setSamplerState:out.height.sampler.gpuResourceID atIndex:0];
 }
 if(out.coverageMask.texture){lease.AddAllocation(out.coverageMask.texture);[out.arguments setTexture:out.coverageMask.texture.gpuResourceID atIndex:1];}
	out.pipeline=impl->Pipeline(lease,draw,color,depth,stencil,samples,validation,milliseconds);out.depth=effectstate::Depth(lease,out.rasterState,stencil);
	out.count=composite?draw.vertexCount:line?draw.vertexCount*6:(draw.vertexCount-2)*3;out.instances=draw.instanceCount;return result;
}
}
