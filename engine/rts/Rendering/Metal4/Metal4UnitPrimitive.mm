/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4UnitPrimitive.h"
#include "NativeUnitPrimitiveShaders.h"
#include "Rendering/Metal4/Metal4NativeEffectState.h"
#import <simd/simd.h>
#include <cstring>
#include <map>
#include <tuple>
#include <unordered_map>

namespace rendering::metal4 {
namespace {
struct Uniform {
	simd_float4x4 viewProjection,viewInverse;
	simd_float4 time,parameters;
	simd_uint4 options,alpha;
	simd_float4 ally,selected;
};
static_assert(sizeof(Uniform)==224);
id<MTLBuffer> Upload(RuntimeLease& lease,const void* bytes,std::size_t size)
{
	Require(size && size<=lease.Device().maxBufferLength,"Original unit primitive buffer exceeds actual device capacity");
	auto buffer=[lease.Device()newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared];
	Require(buffer!=nil,"Cannot allocate original unit primitive immutable buffer");lease.AddAllocation(buffer);return buffer;
}
MTLBlendOperation Equation(ScreenBlendEquation value)
{
	switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}
	throw std::runtime_error("Unknown original unit primitive blend equation");
}
std::uint64_t PipelineBits(const ScreenBlendState& blend,const std::array<bool,4>& mask)
{
	return uint64_t(blend.enabled)|uint64_t(blend.sourceColor)<<1|uint64_t(blend.destinationColor)<<5|
		uint64_t(blend.sourceAlpha)<<9|uint64_t(blend.destinationAlpha)<<13|uint64_t(mask[0])<<17|
		uint64_t(mask[1])<<18|uint64_t(mask[2])<<19|uint64_t(mask[3])<<20|uint64_t(blend.colorEquation)<<21|uint64_t(blend.alphaEquation)<<24;
}
}
struct PreparedUnitPrimitive::Impl {
	struct Draw {
		std::uint64_t sequence=0;
		__strong id<MTLRenderPipelineState> pipeline=nil;
		__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;
		std::uint32_t vertexCount=0,instances=0;
		effectstate::Raster raster;
		NativeWorldDrawState state;
  std::vector<PreparedScreenExternalImage> externalImages;
	};
	std::vector<Draw> draws;
	std::shared_ptr<const NativeUnitPrimitiveFrame> owner;
};
PreparedUnitPrimitive::PreparedUnitPrimitive():impl(std::make_unique<Impl>()){}
PreparedUnitPrimitive::~PreparedUnitPrimitive()=default;
std::size_t PreparedUnitPrimitive::DrawCount()const{return impl->draws.size();}
std::uint64_t PreparedUnitPrimitive::SequenceAt(std::size_t index)const{return impl->draws.at(index).sequence;}
void PreparedUnitPrimitive::EncodeDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder)const
{
	const auto& draw=impl->draws.at(index);for(const auto& image:draw.externalImages)image.requireReadable();effectstate::RasterState(encoder,draw.state,draw.raster);
	const auto cull=draw.state.cullEnabled?draw.state.cull:NativeWorldCull::None;
	[encoder setCullMode:cull==NativeWorldCull::Front?MTLCullModeFront:cull==NativeWorldCull::Back?MTLCullModeBack:MTLCullModeNone];
	[encoder setRenderPipelineState:draw.pipeline];[encoder setDepthStencilState:draw.depth];
	[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:draw.vertexCount instanceCount:draw.instances];
}
struct UnitPrimitiveGPU::Impl {
	__strong id<MTLLibrary> library=nil;
	using Key=std::tuple<unsigned,MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,unsigned,bool,std::uint64_t,bool,bool>;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	struct CachedBuffer{std::weak_ptr<const NativeBufferSnapshot> owner;__strong id<MTLBuffer> buffer=nil;};
	std::unordered_map<const NativeBufferSnapshot*,CachedBuffer> buffers;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& source,std::size_t used)
	{
		Require(used && used<=source->bytes.size(),"Original unit primitive GPU read exceeds immutable buffer");
		auto& entry=buffers[source.get()];
		if(entry.owner.expired() || entry.buffer.length<used){auto next=Upload(lease,source->bytes.data(),used);entry={source,next};}
		else lease.AddAllocation(entry.buffer);
		return entry.buffer;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,NativeUnitPrimitiveKind program,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,const ScreenBlendState& blend,const std::array<bool,4>& mask,double& ms,bool coverage=false,bool selected31657=false)
	{
		const Key key{unsigned(program),color,depth,stencil,lease.SampleShadingKey(samples),validation,PipelineBits(blend,mask),coverage,selected31657};if(auto it=pipelines.find(key);it!=pipelines.end())return it->second;
		const auto start=Clock::now();NSError* error=nil;
		if(!library){
			auto d=[MTL4LibraryDescriptor new];d.name=@"Original DrawPrimitiveAtUnit paired program family";d.source=String(NativeUnitPrimitiveShaderSource);
			auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;d.options=options;
			library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile original unit primitive program family: "+Error(error));
		}
		auto d=[MTL4RenderPipelineDescriptor new];auto vertex=[MTL4LibraryFunctionDescriptor new],fragment=[MTL4LibraryFunctionDescriptor new];vertex.library=fragment.library=library;
		vertex.name=@"UnitPrimitiveVertex";fragment.name=selected31657?@"UnitPrimitiveSelected31657Fragment":(program==NativeUnitPrimitiveKind::UnitTrackerDebug || program==NativeUnitPrimitiveKind::EnemySpotter)?@"UnitPrimitiveTrackerFragment":program==NativeUnitPrimitiveKind::ResurrectionHalo?@"UnitPrimitiveHaloFragment":IsNativeUnitIcon(program)?@"UnitPrimitiveIconFragment":program==NativeUnitPrimitiveKind::AllySelected?@"UnitPrimitiveAllyFragment":IsNativeSelectedUnits(program)?@"UnitPrimitiveSelectedFragment":program==NativeUnitPrimitiveKind::TeamPlatter?@"UnitPrimitiveTeamFragment":@"UnitPrimitiveAtlasFragment";
		d.vertexFunctionDescriptor=vertex;d.fragmentFunctionDescriptor=fragment;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
		a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);
		a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);
		a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
		auto pipeline=lease.NewRenderPipeline(d,&error);Require(pipeline!=nil,"Cannot compile original unit primitive actual render pipeline: "+Error(error));
		pipelines.emplace(key,pipeline);ms+=Milliseconds(start);return pipeline;
	}
};
UnitPrimitiveGPU::UnitPrimitiveGPU():impl(std::make_unique<Impl>()){}
UnitPrimitiveGPU::~UnitPrimitiveGPU()=default;
void UnitPrimitiveGPU::ClearResources(){impl->buffers.clear();}
void UnitPrimitiveGPU::Initialize(RuntimeLease& lease,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& milliseconds)
{
	ScreenBlendState blend;
	for(auto program:{NativeUnitPrimitiveKind::BuildingAO,NativeUnitPrimitiveKind::FeatureAO,NativeUnitPrimitiveKind::TeamPlatter,NativeUnitPrimitiveKind::SelectedGround,NativeUnitPrimitiveKind::SelectedUnfinished,NativeUnitPrimitiveKind::SelectedWater,NativeUnitPrimitiveKind::AllySelected,NativeUnitPrimitiveKind::EnergyIcon,NativeUnitPrimitiveKind::FireStateIcon,NativeUnitPrimitiveKind::GroupNumber,NativeUnitPrimitiveKind::IdleBuilderIcon,NativeUnitPrimitiveKind::RepeatIcon,NativeUnitPrimitiveKind::WaitIcon,NativeUnitPrimitiveKind::RankIcon,NativeUnitPrimitiveKind::ResurrectionHalo,NativeUnitPrimitiveKind::UnitTrackerDebug,NativeUnitPrimitiveKind::EnemySpotter})
		impl->Pipeline(lease,program,color,depth,stencil,samples,validation,blend,{true,true,true,true},milliseconds);
}
std::unique_ptr<PreparedUnitPrimitive> UnitPrimitiveGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeUnitPrimitiveFrame> frame,
	const WorldCamera& originalCamera,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,ScreenGPU& sharedImages,const PreparedScreenFrame* producers,
	MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& milliseconds,std::optional<NativePostprocessConsumer> consumer)
{
	if(!frame || frame->draws.empty())return {};
	const auto uniforms=ReadNativeUnitPrimitiveUniforms(frame->uniforms);
	Require(depth==MTLPixelFormatDepth32Float || depth==MTLPixelFormatDepth32Float_Stencil8,"Original unit primitive requires the actual main depth attachment");
	Initialize(lease,color,depth,stencil,samples,validation,milliseconds);lease.Retain(impl->library);
	std::erase_if(impl->buffers,[](const auto& entry){return entry.second.owner.expired();});
	auto result=std::unique_ptr<PreparedUnitPrimitive>(new PreparedUnitPrimitive);result->impl->owner=frame;
	for(const auto& draw:frame->draws){
		ValidateNativeUnitPrimitiveDraw(draw);if(!draw.count || (draw.world.cullEnabled && draw.world.cull==NativeWorldCull::FrontAndBack))continue;
		Require(draw.world.frontPolygonMode==NativePolygonMode::Fill && draw.world.backPolygonMode==NativePolygonMode::Fill,"Original unit primitive non-fill raster consumer is unavailable");
		Require(!draw.world.inheritedClipPlane,"Original unit primitive main stage cannot inherit a water clipping plane");
		WorldCamera camera=originalCamera;if(draw.surface.viewport){const auto& v=*draw.surface.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
		auto scissor=draw.surface.scissor;if(scissor && !scissor->windowCoordinates){scissor->x+=originalCamera.viewport[0];scissor->y+=originalCamera.viewport[1];scissor->windowCoordinates=true;}
		auto raster=effectstate::Viewport(camera,width,height,logicalWidth,logicalHeight,scissor);if(!raster)continue;
		auto vertices=impl->Buffer(lease,draw.vertices,std::size_t(draw.first+draw.count)*72);
		const auto objectValues=ReadNativeUnitPrimitiveObjects(draw,frame->uniforms);auto objects=Upload(lease,objectValues.data(),objectValues.size()*sizeof(NativeUnitPrimitiveObject));
		Uniform u{};std::memcpy(&u.viewProjection,uniforms.viewProjection.data(),64);std::memcpy(&u.viewInverse,uniforms.viewInverse.data(),64);std::memcpy(&u.time,uniforms.time.data(),16);
		u.parameters=simd_make_float4(draw.addRadius,draw.iconDistance,draw.opacity,draw.surface.alphaTest.reference);
		u.options=simd_make_uint4(unsigned(draw.kind),uniforms.clipDepth==ClipDepthConvention::MinusOneToOne,draw.first,draw.atlas?draw.atlas->flipV:false);
		u.alpha=simd_make_uint4(draw.surface.alphaTest.enabled,unsigned(draw.surface.alphaTest.compare),draw.disableQuads,0);
		u.ally=simd_make_float4(draw.lineOpacity,draw.maxVertices,0,0);
		u.selected=simd_make_float4(draw.initialSize,draw.whiteMix,0,0);
		auto constants=Upload(lease,&u,sizeof(u));auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=3;descriptor.maxTextureBindCount=1;descriptor.maxSamplerStateBindCount=1;descriptor.initializeBindings=YES;
		NSError* error=nil;auto arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(arguments!=nil,"Cannot allocate original unit primitive argument table: "+Error(error));lease.Retain(arguments);
		[arguments setAddress:vertices.gpuAddress atIndex:0];[arguments setAddress:objects.gpuAddress atIndex:1];[arguments setAddress:constants.gpuAddress atIndex:2];
		std::vector<PreparedScreenExternalImage> external;
		if(draw.atlas){auto image=sharedImages.PrepareImage(lease,draw.atlas,draw.sampler,producers,consumer,[&](PreparedScreenExternalImage image){external.push_back(std::move(image));});[arguments setTexture:image.texture.gpuResourceID atIndex:0];[arguments setSamplerState:image.sampler.gpuResourceID atIndex:0];}
		auto pipeline=impl->Pipeline(lease,draw.kind,color,depth,stencil,samples,validation,draw.surface.blend,draw.world.colorWrite,milliseconds,draw.world.alphaToCoverage,draw.selected31657);lease.Retain(pipeline);
		PreparedUnitPrimitive::Impl::Draw packet;packet.sequence=draw.sequence;packet.pipeline=pipeline;packet.depth=effectstate::Depth(lease,draw.world,stencil);packet.arguments=arguments;
		packet.vertexCount=IsNativeUnitIcon(draw.kind)?6:draw.kind==NativeUnitPrimitiveKind::TeamPlatter || IsNativeSelectedUnits(draw.kind) || draw.kind==NativeUnitPrimitiveKind::AllySelected || draw.kind==NativeUnitPrimitiveKind::UnitTrackerDebug?186:6;packet.instances=draw.count;packet.raster=*raster;packet.state=draw.world;packet.externalImages=std::move(external);result->impl->draws.push_back(std::move(packet));
	}
	return result;
}
}
