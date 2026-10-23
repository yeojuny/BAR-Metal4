/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4OriginalShadowParticles.h"
#include "Metal4NativeEffectState.h"
#include "NativeOriginalShadowParticlesShaders.h"
#import <simd/simd.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <tuple>

namespace rendering::metal4 {
namespace {
struct Uniform {
	simd_float4x4 view,projection;
	simd_float4 alphaControl,sampling;
	simd_uint4 swizzle,image,options;
};
static_assert(sizeof(Uniform)==208);
static_assert(offsetof(Uniform,alphaControl)==128&&offsetof(Uniform,options)==192);
simd_float4x4 Matrix(const std::array<float,16>& values)
{
	simd_float4x4 result;std::memcpy(&result,values.data(),sizeof(result));return result;
}
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* bytes,size_t count,size_t stride)
{
	Require(count&&stride&&count<=lease.Device().maxBufferLength/stride,"Original shadow particle buffer exceeds the actual device bounds");
	auto result=[lease.Device()newBufferWithBytes:bytes length:count*stride options:MTLResourceStorageModeShared];
	Require(result!=nil,"Cannot allocate original shadow particle buffer");lease.AddAllocation(result);return result;
}
MTLBlendOperation Equation(ScreenBlendEquation value)
{
	switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}
	throw std::runtime_error("Original shadow particle blend equation is unsupported");
}
MTLColorWriteMask Mask(const std::array<bool,4>& value)
{
	return MTLColorWriteMask((value[0]?MTLColorWriteMaskRed:0)|(value[1]?MTLColorWriteMaskGreen:0)|(value[2]?MTLColorWriteMaskBlue:0));
}
const NativeTextureSampling& Sampling(const NativeCUSTextureBinding& binding)
{
	Require(binding.knowledge!=NativeCUSBindingKnowledge::Unobserved,"Original shadow particle sampler0 binding is unobserved");
	if(binding.knowledge==NativeCUSBindingKnowledge::DefaultObject)return binding.defaultSampling;
	Require(bool(binding.object),"Original shadow particle sampler0 lost its retained object");return binding.object->sampling;
}
bool Empty(const NativeCUSTextureBinding& binding)
{
	return binding.knowledge==NativeCUSBindingKnowledge::DefaultObject?binding.defaultStorageKnownEmpty:binding.object&&binding.object->storage==NativeTextureStorageKnowledge::Empty;
}
void Target(const NativeOriginalShadowParticlesFrame& source,const CUSSourceTarget& target)
{
	Require(source.sourceDrawFrame&&source.size&&target.width==source.size&&target.height==source.size&&target.logicalWidth==source.size&&target.logicalHeight==source.size,"Original shadow particle target differs from its actual source extent");
	Require(target.samples==1&&target.stencil==MTLPixelFormatInvalid&&(target.depth==MTLPixelFormatDepth16Unorm||target.depth==MTLPixelFormatDepth32Float),"Original shadow particles require the actual shared D16/D32Float depth attachment");
	Require(target.colors[0]==(source.colored?MTLPixelFormatRGBA8Unorm:MTLPixelFormatR8Unorm)&&std::all_of(target.colors.begin()+1,target.colors.end(),[](auto f){return f==MTLPixelFormatInvalid;}),"Original shadow particles require the actual shared RGB/R color attachment");
	Require(std::isfinite(target.shadowPolygonOffsetUnitsScale)&&target.shadowPolygonOffsetUnitsScale>0,"Original shadow particle depth bias has no actual destination scale");
}
void Draw(const NativeOriginalShadowParticlesDraw& draw)
{
	Require(draw.sequence&&draw.compiledOriginal&&!draw.indices.empty()&&draw.indices.size()%3==0,"Original shadow particles lost their actual program or indexed submission");
	Require(draw.surface.viewport.has_value(),"Original shadow particles lost their actual viewport observation");
	Require(draw.world.depthTest&&!draw.world.depthWrite&&draw.surface.blend.enabled&&draw.surface.blend.sourceColor==ScreenBlendFactor::Zero&&draw.surface.blend.destinationColor==ScreenBlendFactor::SourceColor,"Original shadow particle state differs from its real GL::SubState writes");
	// World mask writes are owned by the recorder's world state. Its separate
	// screen draw mask is not a second observation of glColorMask here.
	Require(!draw.world.colorWrite[3],"Original shadow particles cannot synthesize the unwritten vec3 output alpha");
	Require(draw.world.frontPolygonMode==draw.world.backPolygonMode&&draw.world.frontPolygonMode!=NativePolygonMode::Point&&!draw.world.inheritedClipPlane,"Original shadow particle raster/legacy clipping needs its exact consumer");
	if(draw.world.frontPolygonMode==NativePolygonMode::Line)Require(draw.world.lineWidth==1&&!draw.world.lineStipple,"Original shadow particle line raster needs its exact width/stipple consumer");
	Require(std::isfinite(draw.world.offsetFactor)&&std::isfinite(draw.world.offsetUnits),"Original shadow particle polygon offset is nonfinite");
	const auto& alpha=draw.surface.alphaTest;
	Require(!alpha.enabled||alpha.compare==ScreenAlphaCompare::Always||alpha.compare==ScreenAlphaCompare::Never,"Original transparent vec3 output needs its concrete inherited fixed-alpha lowering; no output alpha is assumed");
	for(const auto& m:{draw.view,draw.projection})for(float x:m)Require(std::isfinite(x),"Original shadow particle drawing matrix is nonfinite");
	for(float x:draw.alphaControl)Require(std::isfinite(x),"Original shadow particle alphaCtrl is nonfinite");
	Require(std::isfinite(draw.shadowColorMode),"Original shadow particle color mode is nonfinite");
	for(uint32_t index:draw.indices){
		Require(index<draw.vertices.size(),"Original shadow particle index exceeds its captured vertices");const auto& v=draw.vertices[index];
		for(float x:v.position)Require(std::isfinite(x),"Original shadow particle position is nonfinite");
		for(float x:v.uvw)Require(std::isfinite(x),"Original shadow particle UV/page is nonfinite");
		for(float x:v.uvInfo)Require(std::isfinite(x),"Original shadow particle atlas bounds are nonfinite");
		for(float x:v.animation)Require(std::isfinite(x),"Original shadow particle animation is nonfinite");
	}
}
}

struct PreparedOriginalShadowParticles::Impl {
	struct Draw {
		uint64_t sequence=0;
		NativeWorldDrawState state;
		std::optional<effectstate::Raster> raster;
		CUSSourceImage image;
		__strong id<MTLRenderPipelineState> pipeline=nil;
		__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;
		__strong id<MTLBuffer> indices=nil;
		NSUInteger indexCount=0;
	};
	std::shared_ptr<const NativeOriginalShadowParticlesFrame> source;
	std::vector<Draw> draws;
	float shadowPolygonOffsetUnitsScale=1.f;
	size_t next=0;
	bool dependencies=false,finished=false,completed=false;
};
PreparedOriginalShadowParticles::PreparedOriginalShadowParticles():impl(std::make_unique<Impl>()){}
PreparedOriginalShadowParticles::~PreparedOriginalShadowParticles()=default;
size_t PreparedOriginalShadowParticles::DrawCount()const{return impl->draws.size();}
uint64_t PreparedOriginalShadowParticles::SequenceAt(size_t index)const
{
	Require(index<DrawCount(),"Original shadow particle sequence index exceeds the source");return impl->draws[index].sequence;
}
void PreparedOriginalShadowParticles::EncodeDependencies(id<MTL4CommandBuffer> command)
{
	Require(command&&!impl->dependencies&&!impl->finished&&!impl->completed,"Original shadow particle dependencies are missing or repeated");
	std::set<const void*> seen;
	for(const auto& draw:impl->draws)if(draw.image.owner&&seen.insert(draw.image.owner.get()).second&&draw.image.encodeDependencies)draw.image.encodeDependencies(command);
	impl->dependencies=true;
}
void PreparedOriginalShadowParticles::EncodeDraw(size_t index,id<MTL4RenderCommandEncoder> encoder)
{
	Require(encoder&&impl->dependencies&&!impl->finished&&!impl->completed&&index==impl->next&&index<DrawCount(),"Original shadow particle draw skipped/repeated or has no dependencies");
	const auto& draw=impl->draws[index];
	if(draw.raster&&(!draw.state.cullEnabled||draw.state.cull!=NativeWorldCull::FrontAndBack)){
		effectstate::RasterState(encoder,draw.state,*draw.raster);
		[encoder setDepthClipMode:draw.state.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];
		[encoder setCullMode:!draw.state.cullEnabled||draw.state.cull==NativeWorldCull::None?MTLCullModeNone:draw.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];
		[encoder setTriangleFillMode:draw.state.frontPolygonMode==NativePolygonMode::Line?MTLTriangleFillModeLines:MTLTriangleFillModeFill];
		if(draw.state.polygonOffset)[encoder setDepthBias:draw.state.offsetUnits*impl->shadowPolygonOffsetUnitsScale slopeScale:draw.state.offsetFactor clamp:0];
		if(draw.image.requireReadable)draw.image.requireReadable();
		[encoder setDepthStencilState:draw.depth];[encoder setRenderPipelineState:draw.pipeline];
		[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.indexCount indexType:MTLIndexTypeUInt32 indexBuffer:draw.indices.gpuAddress indexBufferLength:draw.indices.length];
	}
	++impl->next;
}
void PreparedOriginalShadowParticles::Finish()
{
	Require(impl->dependencies&&!impl->finished&&impl->next==DrawCount(),"Original shadow particles did not finish their actual ordered draws");impl->finished=true;
}
void PreparedOriginalShadowParticles::DidComplete()
{
	Require(impl->finished&&!impl->completed,"Original shadow particle retirement preceded real execution or was repeated");
	std::set<const void*> seen;
	for(const auto& draw:impl->draws)if(draw.image.owner&&seen.insert(draw.image.owner.get()).second&&draw.image.didComplete)draw.image.didComplete();
	impl->completed=true;
}

struct OriginalShadowParticlesGPU::Impl {
	struct Program {bool textureArray=false;std::array<__strong id<MTLLibrary>,3> libraries{};};
	std::array<std::shared_ptr<const Program>,2> programs;
	using Key=std::tuple<MTLPixelFormat,MTLPixelFormat,unsigned,bool,unsigned,bool,ScreenBlendEquation,ScreenBlendEquation,ScreenBlendFactor,ScreenBlendFactor,unsigned>;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const NativeOriginalShadowParticlesDraw& draw,const CUSSourceTarget& target,unsigned imageKind,bool validation,double& milliseconds)
	{
		const auto& blend=draw.surface.blend;const auto mask=Mask(draw.world.colorWrite);
		const Key key{target.colors[0],target.depth,target.samples,draw.textureArray,imageKind,validation,blend.colorEquation,blend.alphaEquation,blend.sourceAlpha,blend.destinationAlpha,unsigned(mask)};
		if(auto found=pipelines.find(key);found!=pipelines.end()){Require(found->second.device==lease.Device(),"Original shadow particle pipeline belongs to another device");return found->second;}
		const auto start=Clock::now();auto d=[MTL4RenderPipelineDescriptor new];
		auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=programs[draw.textureArray]->libraries[imageKind];
		v.name=@"OriginalColorVertexMain";f.name=@"OriginalColorFragmentMain";
		d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=target.samples;
		auto a=d.colorAttachments[0];a.pixelFormat=target.colors[0];a.writeMask=mask;a.blendingState=MTL4BlendStateEnabled;
		a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.rgbBlendOperation=Equation(blend.colorEquation);
		a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);a.alphaBlendOperation=Equation(blend.alphaEquation);
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;NSError* error=nil;
		auto pipeline=lease.NewRenderPipeline(d,&error);Require(pipeline!=nil,"Cannot compile original shadow particle target pipeline: "+Error(error));
		pipelines.emplace(key,pipeline);milliseconds+=Milliseconds(start);return pipeline;
	}
};
OriginalShadowParticlesGPU::OriginalShadowParticlesGPU():impl(std::make_unique<Impl>()){}
OriginalShadowParticlesGPU::~OriginalShadowParticlesGPU()=default;
std::shared_ptr<const void> OriginalShadowParticlesGPU::CompileOriginalProgram(RuntimeLease& lease,bool textureArray,double& milliseconds)
{
	if(const auto& old=impl->programs[textureArray]){Require(old->libraries[0].device==lease.Device(),"Original shadow particle program belongs to another device");return old;}
	const auto start=Clock::now();
	auto program=std::make_shared<Impl::Program>();program->textureArray=textureArray;
	// Compile the original program's supported resource contracts. Each draw
	// selects exactly one library by its observed presence/type; no dummy image.
	for(unsigned imageKind=0;imageKind<(textureArray?2u:3u);++imageKind){
		auto d=[MTL4LibraryDescriptor new];d.name=@"Original ProjFX shadow stages";
		const auto source=std::string("#define ORIGINAL_TEXTURE_ARRAY ")+std::to_string(unsigned(textureArray))+"\n#define ORIGINAL_IMAGE_KIND "+std::to_string(imageKind)+"\n"+NativeOriginalShadowParticlesShaders;
		d.source=String(source);
		auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;d.options=options;NSError* error=nil;
		auto library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile original ProjFX shadow stages: "+Error(error));
		Require([library newFunctionWithName:@"OriginalColorVertexMain"]!=nil&&[library newFunctionWithName:@"OriginalColorFragmentMain"]!=nil,"Original ProjFX shadow stage entrypoint is absent");
		program->libraries[imageKind]=library;lease.Retain(library);
	}
	impl->programs[textureArray]=program;milliseconds+=Milliseconds(start);return program;
}
std::shared_ptr<PreparedOriginalShadowParticles> OriginalShadowParticlesGPU::PrepareOriginalShadowParticles(RuntimeLease& lease,std::shared_ptr<const NativeOriginalShadowParticlesFrame> source,const CUSSourceTarget& target,const CUSSourceImageProvider& images,bool validation,double& milliseconds)
{
	Require(bool(source),"Original shadow particle source is unconnected");Target(*source,target);
	if(source->draws.empty())return {};
	Require(bool(images),"Original shadow particle image provider is unconnected");
	auto result=std::shared_ptr<PreparedOriginalShadowParticles>(new PreparedOriginalShadowParticles);result->impl->source=std::move(source);result->impl->shadowPolygonOffsetUnitsScale=target.shadowPolygonOffsetUnitsScale;
	uint64_t previous=0;
	for(const auto& draw:result->impl->source->draws){
		Draw(draw);Require(draw.sequence>previous,"Original shadow particle source sequence repeats or goes backwards");previous=draw.sequence;
		const auto& program=impl->programs[draw.textureArray];
		Require(program&&draw.compiledOriginal.get()==program.get()&&program->textureArray==draw.textureArray&&program->libraries[0].device==lease.Device(),"Original shadow particle source program is not this consumer's actual selected variant");
		PreparedOriginalShadowParticles::Impl::Draw prepared;prepared.sequence=draw.sequence;prepared.state=draw.world;
		auto camera=target.camera;const auto& viewport=*draw.surface.viewport;Require(viewport.width<=INT32_MAX&&viewport.height<=INT32_MAX,"Original shadow particle viewport exceeds GLsizei");
		camera.viewport={viewport.x,viewport.y,int32_t(viewport.width),int32_t(viewport.height)};
		if(viewport.width&&viewport.height)prepared.raster=effectstate::Viewport(camera,target.width,target.height,target.logicalWidth,target.logicalHeight,draw.surface.scissor);
		const auto& sampling=Sampling(draw.texture);Require(std::isfinite(sampling.lodBias),"Original shadow particle texture LOD bias is nonfinite");
		for(unsigned channel:sampling.swizzle)Require(channel==0||channel==1||(channel>=0x1903&&channel<=0x1906),"Original shadow particle texture swizzle is unsupported");
		Uniform u{};u.view=Matrix(draw.view);u.projection=Matrix(draw.projection);
		std::memcpy(&u.alphaControl,draw.alphaControl.data(),sizeof(u.alphaControl));std::memcpy(&u.swizzle,sampling.swizzle.data(),sizeof(u.swizzle));
		u.sampling={sampling.lodBias,draw.shadowColorMode,0,0};u.image.z=sampling.depthMode;
		u.options={unsigned(draw.zeroToOneDepth),unsigned(draw.surface.alphaTest.enabled&&draw.surface.alphaTest.compare==ScreenAlphaCompare::Never),unsigned(target.invertClipY),0};
		if(!Empty(draw.texture)){
			prepared.image=images(lease,draw.texture,0);const auto& image=prepared.image;
			Require(image.texture&&image.sampler&&image.owner&&image.requireReadable,"Original shadow particle sampler0 has no actual image/sampler/producer");
			Require(image.texture.device==lease.Device()&&image.sampler.device==lease.Device()&&image.texture.sampleCount==1,"Original shadow particle image belongs to another device or sample count");
			Require(image.texture.textureType==(draw.textureArray?MTLTextureType2DArray:MTLTextureType2D)&&(!draw.textureArray||!image.depthImage),"Original shadow particle texture type differs from its selected sampler");
			if(image.depthImage)Require(sampling.compareMode==0&&(sampling.depthMode==0x1909||sampling.depthMode==0x8049||sampling.depthMode==0x1906),"Original shadow particle raw depth sampler has another comparison/depth mode");
			u.image.x=image.depthImage?2:1;u.image.y=image.flipV;lease.AddAllocation(image.texture);lease.Retain(image.sampler);
		}
		const unsigned imageKind=u.image.x; // 0 empty, 1 color, 2 raw depth; paired with binding below.
		Require(program->libraries[imageKind]&&program->libraries[imageKind].device==lease.Device(),"Original shadow particle image variant has no library on this device");
		lease.Retain(program->libraries[imageKind]);
		prepared.pipeline=impl->Pipeline(lease,draw,target,imageKind,validation,milliseconds);lease.Retain(prepared.pipeline);prepared.depth=effectstate::Depth(lease,draw.world,target.stencil);
		auto vertices=Buffer(lease,draw.vertices.data(),draw.vertices.size(),sizeof(NativeWorldParticleVertex));prepared.indices=Buffer(lease,draw.indices.data(),draw.indices.size(),sizeof(uint32_t));prepared.indexCount=draw.indices.size();
		auto uniform=Buffer(lease,&u,1,sizeof(u));auto d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=2;d.maxTextureBindCount=2;d.maxSamplerStateBindCount=1;d.initializeBindings=YES;NSError* error=nil;
		auto arguments=[lease.Device()newArgumentTableWithDescriptor:d error:&error];Require(arguments!=nil,"Cannot create original shadow particle argument table: "+Error(error));lease.Retain(arguments);
		[arguments setAddress:vertices.gpuAddress atIndex:0];[arguments setAddress:uniform.gpuAddress atIndex:1];
		if(prepared.image.texture){[arguments setTexture:prepared.image.texture.gpuResourceID atIndex:prepared.image.depthImage?1:0];[arguments setSamplerState:prepared.image.sampler.gpuResourceID atIndex:0];}
		prepared.arguments=arguments;result->impl->draws.push_back(std::move(prepared));
	}
	return result;
}
} // namespace rendering::metal4
