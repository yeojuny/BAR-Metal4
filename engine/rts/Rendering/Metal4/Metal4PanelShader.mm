/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4PanelShader.h"
#include "NativePanelShaders.h"

#include <cstddef>
#include <map>
#include <tuple>

namespace rendering::metal4 {
namespace {
MTLBlendFactor Factor(ScreenBlendFactor value)
{
	switch(value) {
		case ScreenBlendFactor::Zero:return MTLBlendFactorZero;
		case ScreenBlendFactor::One:return MTLBlendFactorOne;
		case ScreenBlendFactor::SourceAlpha:return MTLBlendFactorSourceAlpha;
		case ScreenBlendFactor::OneMinusSourceAlpha:return MTLBlendFactorOneMinusSourceAlpha;
		case ScreenBlendFactor::DestinationAlpha:return MTLBlendFactorDestinationAlpha;
		case ScreenBlendFactor::OneMinusDestinationAlpha:return MTLBlendFactorOneMinusDestinationAlpha;
		case ScreenBlendFactor::SourceColor:return MTLBlendFactorSourceColor;
		case ScreenBlendFactor::OneMinusSourceColor:return MTLBlendFactorOneMinusSourceColor;
		case ScreenBlendFactor::DestinationColor:return MTLBlendFactorDestinationColor;
		case ScreenBlendFactor::OneMinusDestinationColor:return MTLBlendFactorOneMinusDestinationColor;
		case ScreenBlendFactor::SourceAlphaSaturate:return MTLBlendFactorSourceAlphaSaturated;
	}
	throw std::runtime_error("Invalid original panel blend factor");
}
MTLBlendOperation Equation(ScreenBlendEquation value)
{
	switch(value) {
		case ScreenBlendEquation::Add:return MTLBlendOperationAdd;
		case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;
		case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;
		case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;
		case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;
	}
	throw std::runtime_error("Invalid original panel blend equation");
}
std::uint32_t BlendKey(const ScreenBlendState& s)
{
	return std::uint32_t(s.enabled)|(std::uint32_t(s.sourceColor)<<1)|(std::uint32_t(s.destinationColor)<<5)|
		(std::uint32_t(s.sourceAlpha)<<9)|(std::uint32_t(s.destinationAlpha)<<13)|
		(std::uint32_t(s.colorEquation)<<17)|(std::uint32_t(s.alphaEquation)<<20);
}
id<MTLBuffer> Upload(RuntimeLease& lease,const void* bytes,std::size_t length)
{
	Require(length && length<=lease.Device().maxBufferLength,"Original panel buffer exceeds actual device limit");
	auto buffer=[lease.Device() newBufferWithBytes:bytes length:length options:MTLResourceStorageModeShared];
	Require(buffer!=nil,"Cannot allocate original panel immutable buffer");lease.AddAllocation(buffer);return buffer;
}
struct alignas(16) PanelUniform {
	std::array<float,16> mvp,textureMatrix;
	std::array<float,4> rect,uv,blur,view,effect,tint;
	std::array<std::uint32_t,4> mode,alphaTest;
	std::array<float,4> alphaValue;
	std::array<std::array<std::uint32_t,4>,3> image;
};
static_assert(offsetof(PanelUniform,rect)==128 && offsetof(PanelUniform,mode)==224);
static_assert(offsetof(PanelUniform,image)==272 && sizeof(PanelUniform)==320);
}

struct PreparedPanelShaderDraw::Impl {
	__strong id<MTLRenderPipelineState> pipeline=nil;
	__strong id<MTL4ArgumentTable> arguments=nil;
	__strong id<MTLBuffer> indices=nil;
	std::uint32_t instances=0;
};
PreparedPanelShaderDraw::PreparedPanelShaderDraw():impl(std::make_unique<Impl>()){}
PreparedPanelShaderDraw::~PreparedPanelShaderDraw()=default;
void PreparedPanelShaderDraw::Encode(id<MTL4RenderCommandEncoder> encoder)const
{
	if(!impl->instances)return;
	[encoder setRenderPipelineState:impl->pipeline];
	[encoder setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:6 indexType:MTLIndexTypeUInt32
		indexBuffer:impl->indices.gpuAddress indexBufferLength:impl->indices.length instanceCount:impl->instances];
}

struct PanelGPU::Impl {
	__strong id<MTLLibrary> library=nil;
	using PipelineKey=std::tuple<NSUInteger,std::uint32_t,unsigned,bool,std::uint32_t,bool>;
	std::map<PipelineKey,id<MTLRenderPipelineState>> pipelines;
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const NativePanelShaderDraw& source,
		MTLPixelFormat format,bool validation,double& shaderMs,std::uint32_t samples)
	{
		NSError* error=nil;
		if(library==nil) {
			const auto start=Clock::now();
			auto descriptor=[MTL4LibraryDescriptor new];
			descriptor.name=@"Original BAR GUI Shader and Pause Screen";
			descriptor.source=String(NativePanelMetalPrograms);
			auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;
			options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;
			library=lease.NewLibrary(descriptor,&error);
			Require(library!=nil,"Cannot compile original panel shaders: "+Error(error));
			shaderMs+=Milliseconds(start);
		}
		unsigned mask=0;for(unsigned i=0;i<4;++i)if(source.surface.colorWrite[i])mask|=1u<<i;
		const auto& blend=source.surface.blend;
		auto& pipeline=pipelines[{format,BlendKey(blend),mask,validation,lease.SampleShadingKey(samples),source.surface.alphaToCoverage}];
		if(pipeline==nil) {
			const auto start=Clock::now();
			auto descriptor=[MTL4RenderPipelineDescriptor new];
			descriptor.rasterSampleCount=samples;
			descriptor.alphaToCoverageState=source.surface.alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
			auto vertex=[MTL4LibraryFunctionDescriptor new];vertex.library=library;vertex.name=@"PanelVertex";
			auto fragment=[MTL4LibraryFunctionDescriptor new];fragment.library=library;fragment.name=@"PanelFragment";
			descriptor.vertexFunctionDescriptor=vertex;descriptor.fragmentFunctionDescriptor=fragment;
			descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
			auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);
			descriptor.options=options;
			auto color=descriptor.colorAttachments[0];color.pixelFormat=format;
			color.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
			color.sourceRGBBlendFactor=Factor(blend.sourceColor);color.destinationRGBBlendFactor=Factor(blend.destinationColor);
			color.sourceAlphaBlendFactor=Factor(blend.sourceAlpha);color.destinationAlphaBlendFactor=Factor(blend.destinationAlpha);
			color.rgbBlendOperation=Equation(blend.colorEquation);color.alphaBlendOperation=Equation(blend.alphaEquation);
			color.writeMask=MTLColorWriteMask((mask&1?MTLColorWriteMaskRed:0)|(mask&2?MTLColorWriteMaskGreen:0)|
				(mask&4?MTLColorWriteMaskBlue:0)|(mask&8?MTLColorWriteMaskAlpha:0));
			// Metal4 gets depth/stencil formats from the original render pass.
			pipeline=lease.NewRenderPipeline(descriptor,&error);
			Require(pipeline!=nil,"Cannot create original panel pipeline: "+Error(error));
			shaderMs+=Milliseconds(start);
		}
		lease.Retain(library);lease.Retain(pipeline);return pipeline;
	}
};
PanelGPU::PanelGPU():impl(std::make_unique<Impl>()){}
PanelGPU::~PanelGPU()=default;

std::unique_ptr<PreparedPanelShaderDraw> PanelGPU::Prepare(RuntimeLease& lease,const NativePanelShaderDraw& source,
	const std::array<id<MTLTexture>,3>& images,const std::array<id<MTLSamplerState>,3>& samplers,
	std::uint32_t actualTargetHeight,MTLPixelFormat colorFormat,bool validation,double& shaderMs,std::uint32_t samples)
{
	ValidateNativePanelShaderDraw(source);Require(actualTargetHeight!=0,"Panel shader lacks actual framebuffer height");
	for(unsigned i=0;i<3;++i)Require(images[i]!=nil && samplers[i]!=nil,"Panel shader lacks resolved texture/sampler arguments");
	auto prepared=std::unique_ptr<PreparedPanelShaderDraw>(new PreparedPanelShaderDraw);
	prepared->impl->instances=source.instanceCount;
	prepared->impl->pipeline=impl->Pipeline(lease,source,colorFormat,validation,shaderMs,samples);
	PanelUniform u{};u.mvp=source.modelViewProjection;u.textureMatrix=source.textureMatrix;
	u.rect=source.rect;u.uv=source.uv;u.blur={source.ivsx,source.ivsy,source.blurScale,source.tileSize};
	u.view={source.viewSize[0],source.viewSize[1],float(actualTargetHeight),0};
	u.effect={source.threshold,source.alpha,source.vignette,0};u.tint={source.tint[0],source.tint[1],source.tint[2],1};
	u.mode={std::uint32_t(source.kind),std::uint32_t(source.toScreen),std::uint32_t(source.blockSize),
		std::uint32_t(source.intelGPU)|(std::uint32_t(source.intelNormalized)<<1)|(std::uint32_t(source.perceivedLuma)<<2)|
		(std::uint32_t(source.smoothTone)<<3)|(std::uint32_t(source.tintEnabled)<<4)|(std::uint32_t(source.fineDerivatives)<<5)};
	u.alphaTest={std::uint32_t(source.surface.alphaTest.enabled),std::uint32_t(source.surface.alphaTest.compare),0,0};
	u.alphaValue={source.surface.alphaTest.reference,0,0,0};
	for(unsigned i=0;i<3;++i) {
		const auto& image=source.images[i];const auto& sampler=source.samplers[i];
		u.image[i]={std::uint32_t(image && image->flipV),std::uint32_t(sampler.wrapS==ScreenWrap::LegacyClamp),
			std::uint32_t(sampler.wrapT==ScreenWrap::LegacyClamp),std::uint32_t(image && image->incomplete)};
		lease.AddAllocation(images[i]);lease.Retain(samplers[i]);
	}
	// TexRect's original 0,1,2,0,2,3 fan addressed through the VAO's
	// 00,10,01,11 corner order. Preserve its winding even when y0 > y1.
	const std::array<std::uint32_t,6> pauseIndices{0,1,3,0,3,2};
	const auto& indices=source.kind==NativePanelShaderKind::Pause?pauseIndices:source.indices;
	prepared->impl->indices=Upload(lease,indices.data(),sizeof(indices));
	auto corners=Upload(lease,source.corners.data(),sizeof(source.corners));
	const std::array<float,2> unusedTile{0,0};
	auto tiles=source.kind==NativePanelShaderKind::GUIBlur && !source.tiles.empty()?
		Upload(lease,source.tiles.data(),source.tiles.size()*sizeof(source.tiles[0])):
		Upload(lease,unusedTile.data(),sizeof(unusedTile));
	auto uniforms=Upload(lease,&u,sizeof(u));
	auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=3;
	descriptor.maxTextureBindCount=3;descriptor.maxSamplerStateBindCount=3;descriptor.initializeBindings=YES;
	NSError* error=nil;auto args=[lease.Device() newArgumentTableWithDescriptor:descriptor error:&error];
	Require(args!=nil,"Cannot allocate original panel argument table: "+Error(error));
	[args setAddress:corners.gpuAddress atIndex:0];[args setAddress:tiles.gpuAddress atIndex:1];[args setAddress:uniforms.gpuAddress atIndex:2];
	for(unsigned i=0;i<3;++i){[args setTexture:images[i].gpuResourceID atIndex:i];[args setSamplerState:samplers[i].gpuResourceID atIndex:i];}
	lease.Retain(args);prepared->impl->arguments=args;return prepared;
}
} // namespace rendering::metal4
