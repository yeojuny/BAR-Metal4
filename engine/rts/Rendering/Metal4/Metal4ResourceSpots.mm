/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4ResourceSpots.h"
#include "NativeResourceSpotsShaders.h"
#include <simd/simd.h>
#include <cstring>
#include <map>
#include <set>
#include <tuple>
namespace rendering::metal4 {
namespace {
struct Uniform {
 simd_float4x4 viewProj,viewInv;
 simd_float4 mapSize,timeInfo,values;
 simd_uint4 options,imageFlags[2],swizzle[2];simd_float4 imageLod[2],alpha;
};
static_assert(sizeof(Uniform)==304);
id<MTLBuffer> Upload(RuntimeLease& lease,const void* bytes,size_t n){Require(n&&n<=lease.Device().maxBufferLength,"Original ResourceSpots immutable buffer exceeds actual device bounds");auto b=[lease.Device()newBufferWithBytes:bytes length:n options:MTLResourceStorageModeShared];Require(b!=nil,"Cannot allocate original ResourceSpots source buffer");lease.AddAllocation(b);return b;}
MTLBlendOperation Equation(ScreenBlendEquation x){switch(x){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Original ResourceSpots blend equation differs");}
uint64_t Bits(const ScreenBlendState& b,const std::array<bool,4>& m){return uint64_t(b.enabled)|uint64_t(b.sourceColor)<<1|uint64_t(b.destinationColor)<<5|uint64_t(b.sourceAlpha)<<9|uint64_t(b.destinationAlpha)<<13|uint64_t(m[0])<<17|uint64_t(m[1])<<18|uint64_t(m[2])<<19|uint64_t(m[3])<<20|uint64_t(b.colorEquation)<<21|uint64_t(b.alphaEquation)<<24;}
}
struct PreparedResourceSpots::Impl {
	struct Draw {
		uint64_t sequence=0;NativeWorldDrawStage stage=NativeWorldDrawStage::World;std::shared_ptr<const NativeResourceSpotsInput> source;NativeWorldDrawState state;effectstate::Raster raster;
		__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;__strong id<MTLBuffer> indices=nil;
		std::vector<CUSSourceImage> images;bool visible=false,encoded=false;
	};
	std::vector<Draw> draws;bool dependencies=false,completed=false;
};
PreparedResourceSpots::PreparedResourceSpots():impl(std::make_unique<Impl>()){}
PreparedResourceSpots::~PreparedResourceSpots()=default;
size_t PreparedResourceSpots::DrawCount()const{return impl->draws.size();}
uint64_t PreparedResourceSpots::SequenceAt(size_t n)const{return impl->draws.at(n).sequence;}
NativeWorldDrawStage PreparedResourceSpots::StageAt(size_t n)const{return impl->draws.at(n).stage;}
void PreparedResourceSpots::EncodeDependencies(id<MTL4CommandBuffer> command){
	Require(command&&!impl->dependencies&&!impl->completed,"Original ResourceSpots dependency stage repeated or absent");std::set<const void*> seen;
	for(const auto& d:impl->draws)for(const auto& image:d.images)if(seen.insert(image.owner.get()).second&&image.encodeDependencies)image.encodeDependencies(command);
	impl->dependencies=true;
}
void PreparedResourceSpots::EncodeDraw(size_t n,id<MTL4RenderCommandEncoder> encoder){
	Require(encoder&&impl->dependencies&&!impl->completed,"Original ResourceSpots draw lacks its actual dependency stage");auto& d=impl->draws.at(n);Require(!d.encoded,"Original ResourceSpots indexed draw encoded twice");d.encoded=true;if(!d.visible)return;
	for(const auto& image:d.images)image.requireReadable();effectstate::RasterState(encoder,d.state,d.raster);
	[encoder setDepthClipMode:d.state.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];
	[encoder setCullMode:!d.state.cullEnabled?MTLCullModeNone:d.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];
	[encoder setRenderPipelineState:d.pipeline];[encoder setDepthStencilState:d.depth];[encoder setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	const auto& s=*d.source;const size_t offset=size_t(s.indexOffset)*2;
	[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:s.count indexType:MTLIndexTypeUInt16 indexBuffer:d.indices.gpuAddress+offset indexBufferLength:d.indices.length-offset instanceCount:s.instanceCount baseVertex:s.baseVertex baseInstance:s.baseInstance];
}
void PreparedResourceSpots::DidComplete(){
	Require(impl->dependencies&&!impl->completed&&std::all_of(impl->draws.begin(),impl->draws.end(),[](const auto& d){return d.encoded;}),"Original ResourceSpots retirement preceded its actual indexed draws");
	std::set<const void*> seen;for(const auto& d:impl->draws)for(const auto& image:d.images)if(seen.insert(image.owner.get()).second&&image.didComplete)image.didComplete();impl->completed=true;
}
struct ResourceSpotsGPU::Impl {
	struct Cached {std::weak_ptr<const NativeBufferSnapshot> source;__strong id<MTLBuffer> buffer=nil;};
	std::map<const NativeBufferSnapshot*,Cached> buffers;
	std::map<unsigned,id<MTLLibrary>> libraries;
	using Key=std::tuple<unsigned,unsigned,bool,uint64_t,unsigned,bool>;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& source){
		auto& b=buffers[source.get()];if(b.source.expired()||b.buffer==nil){b.buffer=Upload(lease,source->bytes.data(),source->bytes.size());b.source=source;}else lease.AddAllocation(b.buffer);return b.buffer;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,MTLPixelFormat color,unsigned variant,bool validation,const ScreenBlendState& blend,const std::array<bool,4>& mask,double& ms,unsigned samples,bool coverage){
		const Key key{unsigned(color),variant,validation,Bits(blend,mask),lease.SampleShadingKey(samples),coverage};if(auto found=pipelines.find(key);found!=pipelines.end())return found->second;
		const auto start=Clock::now();NSError* error=nil;auto& library=libraries[variant];
		if(library==nil){auto d=[MTL4LibraryDescriptor new];const std::string prefix="#define METAL_SPOTS "+std::to_string(variant)+"\n";d.source=String(prefix+resourcespotsprogram::Source);d.name=@"Original31657 indexed ResourceSpots";auto o=[MTLCompileOptions new];o.mathMode=MTLMathModeSafe;o.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;d.options=o;library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile original ResourceSpots native source: "+Error(error));}
		auto d=[MTL4RenderPipelineDescriptor new];auto vs=[MTL4LibraryFunctionDescriptor new],fs=[MTL4LibraryFunctionDescriptor new];vs.library=fs.library=library;vs.name=@"ResourceSpotsVertex";fs.name=@"ResourceSpotsFragment";d.vertexFunctionDescriptor=vs;d.fragmentFunctionDescriptor=fs;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto o=[MTL4PipelineOptions new];o.shaderValidation=ShaderValidationMode(validation);d.options=o;auto p=lease.NewRenderPipeline(d,&error);Require(p!=nil,"Cannot compile actual original ResourceSpots pipeline: "+Error(error));pipelines.emplace(key,p);ms+=Milliseconds(start);return p;
	}
};
ResourceSpotsGPU::ResourceSpotsGPU():impl(std::make_shared<Impl>()){}
ResourceSpotsGPU::~ResourceSpotsGPU(){SetNativeResourceSpotsConsumerLifetime({});}
void ResourceSpotsGPU::Initialize(RuntimeLease& lease,MTLPixelFormat color,bool validation,double& ms,unsigned samples){
	ScreenBlendState blend;for(unsigned variant=0;variant<2;++variant)impl->Pipeline(lease,color,variant,validation,blend,{true,true,true,true},ms,samples,false);SetNativeResourceSpotsConsumerLifetime(impl);
}
void ResourceSpotsGPU::ClearResources(){impl->buffers.clear();}
std::shared_ptr<PreparedResourceSpots> ResourceSpotsGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeWorldDrawFrame> frame,uint64_t sourceDrawFrame,const WorldCamera& view,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,const ResourceSpotsImageProvider& images,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& ms,unsigned samples){
	if(!frame||std::none_of(frame->draws.begin(),frame->draws.end(),[](const auto& c){return bool(c.resourceSpots);}))return {};
	Require(images&&(depth==MTLPixelFormatDepth32Float||depth==MTLPixelFormatDepth32Float_Stencil8),"Original ResourceSpots has no shared scene/image consumer");std::erase_if(impl->buffers,[](const auto& b){return b.second.source.expired();});
	auto out=std::shared_ptr<PreparedResourceSpots>(new PreparedResourceSpots);
	for(const auto& c:frame->draws){if(!c.resourceSpots)continue;ValidateNativeWorldDrawCommand(c);const auto& s=*c.resourceSpots;
		Require(s.sourceDrawFrame==sourceDrawFrame,"Original ResourceSpots indexed draw belongs to another source frame/stage");
		Require(!c.world.inheritedClipPlane&&c.world.frontPolygonMode==NativePolygonMode::Fill&&c.world.backPolygonMode==NativePolygonMode::Fill,"Original ResourceSpots inherited polygon/clip state requires its actual shader profile");
		PreparedResourceSpots::Impl::Draw d;d.sequence=c.sequence;d.stage=c.stage;d.source=c.resourceSpots;d.state=c.world;auto camera=view;if(c.surface.viewport){const auto& v=*c.surface.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
		auto scissor=c.surface.scissor;if(scissor&&!scissor->windowCoordinates){scissor->x+=view.viewport[0];scissor->y+=view.viewport[1];scissor->windowCoordinates=true;}
		auto raster=effectstate::Viewport(camera,width,height,logicalWidth,logicalHeight,scissor);
		if(!raster||(c.world.cullEnabled&&c.world.cull==NativeWorldCull::FrontAndBack)){out->impl->draws.push_back(std::move(d));continue;}d.raster=*raster;d.visible=true;
  Uniform u{};std::memcpy(&u.viewProj,s.original->matrices->bytes.data()+5*64,64);std::memcpy(&u.viewInv,s.original->matrices->bytes.data()+7*64,64);
  std::memcpy(&u.mapSize,s.original->params->bytes.data()+3*16,16);std::memcpy(&u.timeInfo,s.original->params->bytes.data()+1*16,16);
  u.values=simd_make_float4(s.drawPass,s.waterLevel,0,0);
		uint32_t caps;std::memcpy(&caps,s.original->params->bytes.data()+12,4);u.options=simd_make_uint4(caps&1u,0,unsigned(c.surface.alphaTest.enabled),unsigned(c.surface.alphaTest.compare));u.alpha=simd_make_float4(c.surface.alphaTest.reference,0,0,0);
		auto desc=[MTL4ArgumentTableDescriptor new];desc.maxBufferBindCount=3;desc.maxTextureBindCount=2;desc.maxSamplerStateBindCount=2;desc.initializeBindings=YES;NSError* error=nil;d.arguments=[lease.Device()newArgumentTableWithDescriptor:desc error:&error];Require(d.arguments!=nil,"Cannot allocate original ResourceSpots arguments: "+Error(error));lease.Retain(d.arguments);
		auto vertices=impl->Buffer(lease,s.vertices),instances=impl->Buffer(lease,s.instances);d.indices=impl->Buffer(lease,s.indices);
		[d.arguments setAddress:vertices.gpuAddress atIndex:0];[d.arguments setAddress:instances.gpuAddress atIndex:1];
  if(s.kind==NativeResourceSpotKind::Metal)for(unsigned slot=0;slot<2;++slot){
   const auto& binding=s.images[slot];const auto& sampling=binding.object->sampling;auto image=images(lease,binding,c.stage,c.sequence);
   Require(image.texture&&image.sampler&&image.owner&&image.requireReadable&&!image.depthImage,"Original resource spot image lacks actual color storage/sampler/producer");
   for(unsigned k=0;k<4;++k){const unsigned sw=sampling.swizzle[k];Require(sw==0||sw==1||(sw>=0x1903&&sw<=0x1906),"Original resource spot texture swizzle differs");u.swizzle[slot][k]=sw;}
   u.imageFlags[slot]=simd_make_uint4(image.flipV,0,0,0);const bool mips=sampling.minFilter>=0x2700&&sampling.minFilter<=0x2703;
   u.imageLod[slot]=simd_make_float4(mips?sampling.minLod:0.f,mips?sampling.maxLod:0.f,sampling.lodBias,0);
   [d.arguments setTexture:image.texture.gpuResourceID atIndex:slot];[d.arguments setSamplerState:image.sampler.gpuResourceID atIndex:slot];lease.AddAllocation(image.texture);lease.Retain(image.sampler);d.images.push_back(std::move(image));
  }
		auto constants=Upload(lease,&u,sizeof(u));[d.arguments setAddress:constants.gpuAddress atIndex:2];
		const unsigned variant=unsigned(s.kind==NativeResourceSpotKind::Metal);d.pipeline=impl->Pipeline(lease,color,variant,validation,c.surface.blend,c.world.colorWrite,ms,samples,c.world.alphaToCoverage);d.depth=effectstate::Depth(lease,c.world,stencil);lease.Retain(d.pipeline);lease.Retain(impl->libraries.at(variant));out->impl->draws.push_back(std::move(d));
	}return out;
}
}
