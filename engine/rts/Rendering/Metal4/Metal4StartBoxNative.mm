/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4StartBoxNative.h"
#include "NativeStartBoxShaders.h"
#include "Rendering/Metal4/Metal4NativeEffectState.h"
#include <map>
#include <tuple>
#include <unordered_map>

namespace rendering::metal4 {
namespace {
id<MTLBuffer> Upload(RuntimeLease& lease,const void* source,size_t size)
{
	Require(source && size && size<=lease.Device().maxBufferLength,"Original StartBox buffer exceeds the actual device capacity");
	auto result=[lease.Device() newBufferWithBytes:source length:size options:MTLResourceStorageModeShared];
	Require(result!=nil,"Cannot allocate original StartBox buffer");lease.AddAllocation(result);return result;
}
MTLBlendOperation Equation(ScreenBlendEquation value)
{
	switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}
	throw std::logic_error("Unknown original StartBox blend equation");
}
uint64_t PipelineBits(const ScreenBlendState& b,const std::array<bool,4>& m)
{
	return uint64_t(b.enabled)|uint64_t(b.sourceColor)<<1|uint64_t(b.destinationColor)<<5|uint64_t(b.sourceAlpha)<<9|uint64_t(b.destinationAlpha)<<13|
		uint64_t(m[0])<<17|uint64_t(m[1])<<18|uint64_t(m[2])<<19|uint64_t(m[3])<<20|uint64_t(b.colorEquation)<<21|uint64_t(b.alphaEquation)<<24;
}
MTLTextureSwizzle Channel(unsigned value)
{
	switch(value){case 0:return MTLTextureSwizzleZero;case 1:return MTLTextureSwizzleOne;case 0x1903:return MTLTextureSwizzleRed;case 0x1904:return MTLTextureSwizzleGreen;case 0x1905:return MTLTextureSwizzleBlue;case 0x1906:return MTLTextureSwizzleAlpha;}
	throw std::logic_error("Unknown original StartBox channel swizzle");
}
bool Identity(MTLTextureSwizzleChannels v)
{
	return v.red==MTLTextureSwizzleRed && v.green==MTLTextureSwizzleGreen && v.blue==MTLTextureSwizzleBlue && v.alpha==MTLTextureSwizzleAlpha;
}
void Replace(std::string& text,std::string_view token,const std::string& replacement)
{
	const auto offset=text.find(token);Require(offset!=std::string::npos && text.find(token,offset+token.size())==std::string::npos,"Original StartBox MSL insertion identity differs");
	text.replace(offset,token.size(),replacement);
}
}

struct PreparedStartBoxDraw::Impl {
	__strong id<MTLRenderPipelineState> pipeline=nil;
	__strong id<MTLDepthStencilState> depth=nil;
	__strong id<MTL4ArgumentTable> arguments=nil;
	std::vector<PreparedStartBoxImage> images;
	std::shared_ptr<const NativeCUSUniformPublication> uniforms;
	NativeWorldDrawState state;
	effectstate::Raster raster;
	uint32_t first=0,count=0,instances=0;
};
PreparedStartBoxDraw::PreparedStartBoxDraw():impl(std::make_unique<Impl>()){}
PreparedStartBoxDraw::~PreparedStartBoxDraw()=default;
bool PreparedStartBoxDraw::Empty()const{return !impl->count || !impl->instances;}
void PreparedStartBoxDraw::Encode(id<MTL4RenderCommandEncoder> encoder)const
{
	if(Empty())return;
	for(const auto& image:impl->images)if(image.requireReadable)image.requireReadable();
	effectstate::RasterState(encoder,impl->state,impl->raster);
	const auto cull=impl->state.cullEnabled?impl->state.cull:NativeWorldCull::None;
	[encoder setCullMode:cull==NativeWorldCull::Front?MTLCullModeFront:cull==NativeWorldCull::Back?MTLCullModeBack:MTLCullModeNone];
	[encoder setDepthStencilState:impl->depth];[encoder setRenderPipelineState:impl->pipeline];
	[encoder setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:impl->first vertexCount:impl->count instanceCount:impl->instances];
}
struct StartBoxGPU::Impl {
	using LibraryKey=std::tuple<NativeStartBoxProgram,std::string,std::array<StartBoxImageKind,6>>;
	using PipelineKey=std::tuple<LibraryKey,NativeStartBoxProgram,MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,unsigned,bool,uint64_t,bool>;
	std::map<LibraryKey,id<MTLLibrary>> libraries;
	std::map<PipelineKey,id<MTLRenderPipelineState>> pipelines;
	struct CachedBuffer {std::weak_ptr<const NativeBufferSnapshot> original;__strong id<MTLBuffer> buffer=nil;};
	std::unordered_map<const NativeBufferSnapshot*,CachedBuffer> buffers;
	struct CachedView {std::weak_ptr<const NativeCUSTextureObject> original;__strong id<MTLTexture> storage=nil,view=nil;};
	std::unordered_map<const NativeCUSTextureObject*,CachedView> views;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& source)
	{
		Require(bool(source),"Original StartBox buffer snapshot is missing");auto& entry=buffers[source.get()];
		if(entry.original.lock()!=source){auto value=Upload(lease,source->bytes.data(),source->bytes.size());entry={source,value};}
		lease.AddAllocation(entry.buffer);return entry.buffer;
	}
	id<MTLTexture> View(RuntimeLease& lease,const PreparedStartBoxImage& image)
	{
		Require(image.texture && image.original && Identity(image.texture.swizzle),"StartBox resolver must return actual identity-swizzle storage");
		const auto& source=image.original->sampling.swizzle;
		if(source==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906})return image.texture;
		Require(image.kind==StartBoxImageKind::Color && (image.texture.usage&MTLTextureUsagePixelFormatView),"StartBox source swizzle requires its real color texture view");
		auto& entry=views[image.original.get()];
		if(entry.original.lock()!=image.original || entry.storage!=image.texture){
			const MTLTextureSwizzleChannels swizzle={Channel(source[0]),Channel(source[1]),Channel(source[2]),Channel(source[3])};
			auto view=[image.texture newTextureViewWithPixelFormat:image.texture.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(0,image.texture.mipmapLevelCount) slices:NSMakeRange(0,1) swizzle:swizzle];
			Require(view!=nil,"Cannot create the observed original StartBox channel view");entry={image.original,image.texture,view};
		}
		lease.AddAllocation(entry.view);return entry.view;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const NativeStartBoxDraw& draw,const std::array<StartBoxImageKind,6>& kinds,
		MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& ms)
	{
		const auto& definition=*draw.program.defines;
		const LibraryKey libraryKey{draw.program.program,definition.originalText,kinds};
		const PipelineKey key{libraryKey,draw.program.program,color,depth,stencil,lease.SampleShadingKey(samples),validation,PipelineBits(draw.surface.blend,draw.raster.colorWrite),draw.raster.alphaToCoverage};
		if(auto it=pipelines.find(key);it!=pipelines.end()){lease.Retain(it->second);return it->second;}
		const auto start=Clock::now();NSError* error=nil;auto& library=libraries[libraryKey];
		if(!library){
			std::string source=NativeStartBoxShaderSource;Replace(source,"/*__ORIGINAL_STARTBOX_DEFINES__*/",definition.originalText);
			std::string types=std::string("#define STARTBOX_POLYGON ")+(draw.program.program==NativeStartBoxProgram::Polygon?"1\n":"0\n");
			types+=std::string("#define STARTBOX_SDF_BAKE ")+(draw.program.program==NativeStartBoxProgram::SDFBake?"1\n":"0\n");
			for(unsigned i:{0u,1u,3u,4u,5u}){types+="#define STARTBOX_IMAGE_"+std::to_string(i)+" "+std::to_string(unsigned(kinds[i]))+"\n";
				if(kinds[i]!=StartBoxImageKind::Empty)types+="#define STARTBOX_TYPE_"+std::to_string(i)+(kinds[i]==StartBoxImageKind::Depth?" depth2d<float>\n":" texture2d<float>\n");}
			Replace(source,"/*__ORIGINAL_STARTBOX_IMAGE_TYPES__*/",types);
			auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Exact31657 original StartBox polygon, cone and SDF bake programs";descriptor.source=String(source);
			auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;
			library=lease.NewLibrary(descriptor,&error);Require(library!=nil,"Cannot compile original StartBox actual image program: "+Error(error));
		}
		lease.Retain(library);auto descriptor=[MTL4RenderPipelineDescriptor new];
		auto vertex=[MTL4LibraryFunctionDescriptor new],fragment=[MTL4LibraryFunctionDescriptor new];vertex.library=fragment.library=library;
		const bool polygon=draw.program.program==NativeStartBoxProgram::Polygon;
		const bool bake=draw.program.program==NativeStartBoxProgram::SDFBake;
		vertex.name=polygon||bake?@"OriginalStartPolygonVertex":@"OriginalStartConeVertex";
		fragment.name=polygon?@"OriginalStartPolygonFragment":bake?@"OriginalStartSDFBakeFragment":@"OriginalStartConeFragment";
		descriptor.vertexFunctionDescriptor=vertex;descriptor.fragmentFunctionDescriptor=fragment;descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;descriptor.rasterSampleCount=samples;descriptor.alphaToCoverageState=draw.raster.alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		const auto& blend=draw.surface.blend;const auto& mask=draw.raster.colorWrite;auto attachment=descriptor.colorAttachments[0];attachment.pixelFormat=color;
		attachment.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;attachment.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);attachment.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);
		attachment.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);attachment.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);attachment.rgbBlendOperation=Equation(blend.colorEquation);attachment.alphaBlendOperation=Equation(blend.alphaEquation);
		attachment.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;
		auto result=lease.NewRenderPipeline(descriptor,&error);Require(result!=nil,"Cannot compile original StartBox actual pipeline: "+Error(error));
		pipelines.emplace(key,result);lease.Retain(result);ms+=Milliseconds(start);return result;
	}
};
StartBoxGPU::StartBoxGPU():impl(std::make_unique<Impl>()){}
StartBoxGPU::~StartBoxGPU()=default;
void StartBoxGPU::ClearResources(){impl->buffers.clear();impl->views.clear();}
std::unique_ptr<PreparedStartBoxDraw> StartBoxGPU::Prepare(RuntimeLease& lease,const NativeStartBoxDraw& draw,
	const StartBoxImageResolver& resolver,MTLViewport viewport,MTLScissorRect scissor,
	MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& milliseconds,
	std::optional<NativePostprocessConsumer> consumer)
{
	ValidateNativeStartBoxDraw(draw);
	if(draw.program.program==NativeStartBoxProgram::SDFBake)
		Require(color==MTLPixelFormatRGBA16Float && samples==1 && depth==MTLPixelFormatInvalid && stencil==MTLPixelFormatInvalid,
			"Original StartBox bake needs the actual RGBA16F color target without depth/stencil");
	auto result=std::unique_ptr<PreparedStartBoxDraw>(new PreparedStartBoxDraw);
	if(!draw.vertexCount || !draw.instanceCount || !scissor.width || !scissor.height || (draw.raster.cullEnabled && draw.raster.cull==NativeWorldCull::FrontAndBack))return result;
	Require(draw.raster.frontPolygonMode==NativePolygonMode::Fill && draw.raster.backPolygonMode==NativePolygonMode::Fill && !draw.raster.inheritedClipPlane,"Original StartBox selected callback requires its original filled main/minimap raster state");
	Require(!draw.raster.depthTest || depth!=MTLPixelFormatInvalid,"Original StartBox depth test needs an actual depth attachment");
	std::erase_if(impl->buffers,[](const auto& p){return p.second.original.expired();});std::erase_if(impl->views,[](const auto& p){return p.second.original.expired();});
	auto& out=*result->impl;out.uniforms=draw.uniforms;out.state=draw.raster;out.raster={viewport,scissor};
	auto uniform=ReadNativeStartBoxUniforms(draw);uniform.options[2]=unsigned(draw.surface.alphaTest.compare)|(unsigned(draw.surface.alphaTest.enabled)<<8);uniform.values[2]=draw.surface.alphaTest.reference;
	std::array<StartBoxImageKind,6> kinds{};std::array<id<MTLTexture>,6> images{};std::array<id<MTLSamplerState>,6> samplers{};
	const auto active=NativeStartBoxActiveSamplers(draw.program);
	for(unsigned i=0;i<active.size();++i){
		if(!active[i])continue;kinds[i]=ClassifyNativeStartBoxImage(draw.images[i]);
		if(kinds[i]==StartBoxImageKind::Empty)continue;
		Require(bool(resolver),"Original StartBox sampled images require the shared actual-resource resolver");auto prepared=resolver(lease,draw.images[i],consumer);
		Require(prepared.kind==kinds[i] && prepared.original==draw.images[i].object && prepared.texture && prepared.sampler && bool(prepared.storageOwner),"StartBox resolver returned different source storage or sampler ownership");
		if(i==5)Require(prepared.kind==StartBoxImageKind::Color && prepared.texture.pixelFormat==MTLPixelFormatRGBA16Float,
			"Original StartBox SDF must retain its actual half-float channels and integer flags");
		const auto& allocation=prepared.original->allocation;
		Require(prepared.texture.textureType==MTLTextureType2D && prepared.texture.width==allocation.width && prepared.texture.height==allocation.height && prepared.texture.mipmapLevelCount==allocation.levels && (prepared.texture.usage&MTLTextureUsageShaderRead),"StartBox actual image dimensions/lifetime differ");
		const bool isDepth=prepared.texture.pixelFormat==MTLPixelFormatDepth32Float || prepared.texture.pixelFormat==MTLPixelFormatDepth32Float_Stencil8 || prepared.texture.pixelFormat==MTLPixelFormatDepth16Unorm;
		Require(isDepth==(kinds[i]==StartBoxImageKind::Depth),"StartBox actual image sampled type differs");
		lease.AddAllocation(prepared.texture);lease.Retain(prepared.sampler);images[i]=impl->View(lease,prepared);samplers[i]=prepared.sampler;uniform.options[0]|=unsigned(prepared.flipV)<<i;out.images.push_back(std::move(prepared));
	}
	auto vertices=impl->Buffer(lease,draw.vertices);auto constants=Upload(lease,&uniform,sizeof(uniform));
	auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=5;descriptor.maxTextureBindCount=6;descriptor.maxSamplerStateBindCount=6;descriptor.initializeBindings=YES;
	NSError* error=nil;out.arguments=[lease.Device() newArgumentTableWithDescriptor:descriptor error:&error];Require(out.arguments!=nil,"Cannot allocate original StartBox arguments: "+Error(error));lease.Retain(out.arguments);
	[out.arguments setAddress:vertices.gpuAddress atIndex:0];[out.arguments setAddress:constants.gpuAddress atIndex:1];
	if(draw.program.program==NativeStartBoxProgram::SDFBake){auto storage=impl->Buffer(lease,draw.polygons.snapshot);[out.arguments setAddress:storage.gpuAddress+draw.polygons.offset atIndex:4];}
	else if(draw.program.program==NativeStartBoxProgram::Cone){auto instances=impl->Buffer(lease,draw.instances);[out.arguments setAddress:instances.gpuAddress+size_t(draw.firstInstance)*32 atIndex:2];}
	for(unsigned i=0;i<images.size();++i)if(images[i]){[out.arguments setTexture:images[i].gpuResourceID atIndex:i];[out.arguments setSamplerState:samplers[i].gpuResourceID atIndex:i];}
	out.pipeline=impl->Pipeline(lease,draw,kinds,color,depth,stencil,samples,validation,milliseconds);out.depth=effectstate::Depth(lease,draw.raster,stencil);
	out.first=draw.firstVertex;out.count=draw.vertexCount;out.instances=draw.instanceCount;return result;
}
}
