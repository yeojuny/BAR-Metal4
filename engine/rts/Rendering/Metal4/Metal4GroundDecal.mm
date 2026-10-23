/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4GroundDecal.h"
#include "NativeGroundDecalShaders.h"
#include <map>
#include <tuple>
#include <utility>

namespace rendering::metal4 {
namespace {
id<MTLBuffer> Buffer(RuntimeLease& l,const void* p,size_t n,const char* label){Require(p&&n,"Original decal buffer empty");auto b=[l.Device() newBufferWithBytes:p length:n options:MTLResourceStorageModeShared];Require(b!=nil,"Cannot allocate original decal buffer");b.label=String(label);l.AddAllocation(b);return b;}
MTLPixelFormat Format(WorldTextureFormat f){switch(f){case WorldTextureFormat::RGBA8Unorm:return MTLPixelFormatRGBA8Unorm;case WorldTextureFormat::BGRA8Unorm:return MTLPixelFormatBGRA8Unorm;case WorldTextureFormat::BC1RGBA:return MTLPixelFormatBC1_RGBA;case WorldTextureFormat::BC2RGBA:return MTLPixelFormatBC2_RGBA;case WorldTextureFormat::BC3RGBA:return MTLPixelFormatBC3_RGBA;case WorldTextureFormat::BC4Unorm:return MTLPixelFormatBC4_RUnorm;case WorldTextureFormat::BC5Unorm:return MTLPixelFormatBC5_RGUnorm;case WorldTextureFormat::BC7RGBA:return MTLPixelFormatBC7_RGBAUnorm;case WorldTextureFormat::BC7RGBASRGB:return MTLPixelFormatBC7_RGBAUnorm_sRGB;}throw std::invalid_argument("Original decal source format unknown");}
id<MTL4ArgumentTable> Arguments(RuntimeLease& l,uint32_t buffers,uint32_t textures,uint32_t samplers){auto d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=buffers;d.maxTextureBindCount=textures;d.maxSamplerStateBindCount=samplers;d.initializeBindings=YES;auto a=[l.Device() newArgumentTableWithDescriptor:d error:nil];Require(a!=nil,"Cannot allocate original decal argument table");l.Retain(a);return a;}
struct AtlasResources {
	std::shared_ptr<const NativeGroundDecalAtlas> source;
	__strong id<MTLTexture> texture=nil;
	std::vector<id<MTLTexture>> inputs;
	__strong id<MTLBuffer> vertices=nil;
	std::vector<NativeGroundDecalAtlasDraw> draws;
	bool ready=false;
};
struct InstanceResources {std::shared_ptr<const NativeGroundDecalInstances> source;__strong id<MTLBuffer> buffer=nil;};
struct AtlasPassDraw {uint32_t page=0,level=0;__strong id<MTL4ArgumentTable> arguments=nil;};
struct alignas(16) AtlasUniforms {std::array<float,4> clamp{};std::array<uint32_t,4> flags{};};
}
struct PreparedGroundDecal::Impl {
	std::shared_ptr<const NativeGroundDecalFrame> frame;
	std::shared_ptr<AtlasResources> atlas;
	std::shared_ptr<InstanceResources> instances;
	__strong id<MTLRenderPipelineState> surfacePipeline=nil,atlasPipeline=nil;
	__strong id<MTLDepthStencilState> depth=nil;
	__strong id<MTL4ArgumentTable> arguments=nil;
	std::vector<AtlasPassDraw> atlasDraws;
	bool dependenciesEncoded=false,encoded=false;
};
PreparedGroundDecal::PreparedGroundDecal(std::unique_ptr<Impl> v):impl(std::move(v)){}
PreparedGroundDecal::~PreparedGroundDecal()=default;
id<MTLTexture> PreparedGroundDecal::Atlas()const{return impl->atlas->texture;}
uint32_t PreparedGroundDecal::AtlasDrawCount()const{return uint32_t(impl->atlasDraws.size());}
void PreparedGroundDecal::EncodeDependencies(id<MTL4CommandBuffer> command)
{
	Require(!impl->dependenciesEncoded,"Original decal atlas encoded twice");
	id<MTL4RenderCommandEncoder> e=nil;uint32_t page=~0u,level=~0u;
	for(const auto& d:impl->atlasDraws){
		if(d.page!=page||d.level!=level){if(e){[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];}page=d.page;level=d.level;
			auto pass=[MTL4RenderPassDescriptor new];auto a=pass.colorAttachments[0];a.texture=impl->atlas->texture;a.slice=page;a.level=level;a.loadAction=MTLLoadActionDontCare;a.storeAction=MTLStoreActionStore;
			e=[command renderCommandEncoderWithDescriptor:pass];Require(e!=nil,"Cannot encode original decal atlas page/mip");
			const auto w=std::max(1u,impl->atlas->source->width>>level),h=std::max(1u,impl->atlas->source->height>>level);
			[e setViewport:MTLViewport{0,0,double(w),double(h),0,1}];[e setScissorRect:MTLScissorRect{0,0,w,h}];
			[e setRenderPipelineState:impl->atlasPipeline];[e setDepthStencilState:impl->depth];[e setCullMode:MTLCullModeNone];[e setTriangleFillMode:MTLTriangleFillModeFill];[e setDepthBias:0 slopeScale:0 clamp:0];
		}
		[e setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
	}
	if(e){[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];}
	impl->dependenciesEncoded=true;
}
void PreparedGroundDecal::Encode(id<MTL4RenderCommandEncoder> e)
{
	Require(impl->dependenciesEncoded&&!impl->encoded,"Original ground decal pass order differs");
	[e setRenderPipelineState:impl->surfacePipeline];[e setDepthStencilState:impl->depth];[e setFrontFacingWinding:MTLWindingCounterClockwise];[e setCullMode:MTLCullModeBack];[e setDepthClipMode:MTLDepthClipModeClip];[e setTriangleFillMode:MTLTriangleFillModeFill];[e setDepthBias:0 slopeScale:0 clamp:0];
	[e setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	if(!impl->frame->instances->values.empty())[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:36 instanceCount:impl->frame->instances->values.size()];impl->encoded=true;
}
void PreparedGroundDecal::DidComplete(){Require(impl->dependenciesEncoded&&impl->encoded,"Original ground decal cannot retire unencoded resources");impl->atlas->ready=true;}
struct GroundDecalGPU::Impl {
	using Key=std::tuple<MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,bool,bool,uint32_t,bool,bool>;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	std::map<std::pair<bool,bool>,id<MTLLibrary>> libraries;
	std::array<id<MTLSamplerState>,6> samplers{};
	__strong id<MTLDepthStencilState> depth=nil;
	std::shared_ptr<AtlasResources> atlas;
	std::shared_ptr<InstanceResources> instances;
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& l,MTLPixelFormat color,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,bool atlas,bool high,uint32_t samples,bool validation,double& milliseconds,bool shadows=false){
		const std::pair<bool,bool> libraryKey{high,shadows};
		const Key key{color,depthFormat,stencilFormat,atlas,high,l.SampleShadingKey(samples),validation,shadows};if(auto it=pipelines.find(key);it!=pipelines.end()){l.Retain(libraries.at(libraryKey));return it->second;}
		const auto start=Clock::now();auto it=libraries.find(libraryKey);NSError* error=nil;
		if(it==libraries.end()){auto o=[MTLCompileOptions new];o.mathMode=MTLMathModeSafe;o.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
			auto lib=l.NewLibraryWithSource(String(NativeGroundDecalShaderSource(high,shadows)),o,&error);Require(lib!=nil,"Cannot compile original ground decal family: "+Error(error));it=libraries.emplace(libraryKey,lib).first;}
		l.Retain(it->second);
		auto d=[MTL4RenderPipelineDescriptor new];auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=it->second;v.name=atlas?@"GroundDecalAtlasVertex":@"GroundDecalVertex";f.name=atlas?@"GroundDecalAtlasFragment":@"GroundDecalFragment";
		// Metal4 retains depth/stencil formats on the real render pass. The
		// cache still records the consumer contract; no legacy MTL descriptor.
		d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=samples;
		auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=atlas?MTL4BlendStateDisabled:MTL4BlendStateEnabled;
		a.rgbBlendOperation=a.alphaBlendOperation=MTLBlendOperationAdd;a.sourceRGBBlendFactor=a.sourceAlphaBlendFactor=MTLBlendFactorSourceAlpha;a.destinationRGBBlendFactor=a.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
		auto pipeline=l.NewRenderPipeline(d,&error);Require(pipeline!=nil,"Cannot compile original ground decal pipeline: "+Error(error));pipelines[key]=pipeline;milliseconds+=Milliseconds(start);return pipeline;
	}
};
GroundDecalGPU::GroundDecalGPU():impl(std::make_unique<Impl>()){}
GroundDecalGPU::~GroundDecalGPU()=default;
void GroundDecalGPU::ClearResources(){impl->atlas.reset();impl->instances.reset();}
void GroundDecalGPU::Initialize(RuntimeLease& l,MTLPixelFormat color,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,uint32_t samples,bool validation,double& milliseconds)
{
 Require(samples&&samples<=32&&(samples&(samples-1))==0&&[l.Device() supportsTextureSampleCount:samples],"Original ground decal initialization sample count invalid");
 impl->Pipeline(l,MTLPixelFormatRGBA8Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,true,false,1,validation,milliseconds);
 impl->Pipeline(l,color,depthFormat,stencilFormat,false,false,samples,validation,milliseconds);
 if(samples>1)impl->Pipeline(l,color,depthFormat,stencilFormat,false,true,samples,validation,milliseconds);
}
std::unique_ptr<PreparedGroundDecal> GroundDecalGPU::Prepare(RuntimeLease& l,std::shared_ptr<const NativeGroundDecalFrame> frame,
	const GroundDecalSceneImages& images,MTLPixelFormat color,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,bool validation,double& milliseconds,GroundDecalTextureProvider textureProvider)
{
	Require(frame!=nullptr,"Original ground decal frame absent");ValidateNativeGroundDecalFrame(*frame);
	Require(images.sourceDrawFrame==frame->sourceDrawFrame,"Original ground decal framebuffer is from another draw");
	const auto& h=images.height;const auto& n=images.normals;const auto& m=images.minimap;const auto& d=images.terrainDepth;
	Require(h.texture&&h.texture.pixelFormat==MTLPixelFormatR32Float&&n.texture&&n.texture.pixelFormat==MTLPixelFormatRG16Float&&m.texture,"Original ground decal terrain images are incomplete");
	Require(d.texture&&d.texture.pixelFormat==MTLPixelFormatDepth32Float&&d.texture.width==frame->viewport[2]&&d.texture.height==frame->viewport[3],"Original ground decal requires unresampled original viewport depth extent");
	Require(d.texture.sampleCount==(frame->highQuality?frame->sampleCount:1),"Original ground decal depth sampling differs from source");
	Require(!frame->shadows||(images.shadowDepth.texture&&images.shadowColor.texture),"Original ground decal shadow producer incomplete");
	auto p=std::make_unique<PreparedGroundDecal::Impl>();p->frame=frame;
	p->surfacePipeline=impl->Pipeline(l,color,depthFormat,stencilFormat,false,frame->highQuality,frame->sampleCount,validation,milliseconds,frame->shadows);l.Retain(p->surfacePipeline);
	if(!impl->depth){auto d=[MTLDepthStencilDescriptor new];d.depthCompareFunction=MTLCompareFunctionAlways;d.depthWriteEnabled=NO;impl->depth=[l.Device() newDepthStencilStateWithDescriptor:d];Require(impl->depth!=nil,"Cannot create original ground decal raster state");}p->depth=impl->depth;l.Retain(p->depth);
	for(size_t i=0;i<impl->samplers.size();++i){if(!impl->samplers[i]){auto s=[MTLSamplerDescriptor new];s.supportArgumentBuffers=YES;s.minFilter=(i<2)?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;s.magFilter=MTLSamplerMinMagFilterLinear;s.mipFilter=i==5?MTLSamplerMipFilterLinear:MTLSamplerMipFilterNotMipmapped;
			s.sAddressMode=s.tAddressMode=(i==3||i==4)?MTLSamplerAddressModeClampToBorderColor:MTLSamplerAddressModeClampToEdge;s.borderColor=MTLSamplerBorderColorOpaqueWhite;s.compareFunction=i==3?MTLCompareFunctionLessEqual:MTLCompareFunctionNever;
			impl->samplers[i]=[l.Device() newSamplerStateWithDescriptor:s];Require(impl->samplers[i]!=nil,"Cannot create original ground decal sampler");}l.Retain(impl->samplers[i]);}
	if(!impl->atlas||impl->atlas->source->generation!=frame->atlas->generation){ValidateNativeGroundDecalAtlas(*frame->atlas);auto a=std::make_shared<AtlasResources>();a->source=frame->atlas;a->draws=MakeNativeGroundDecalAtlasDraws(*a->source);
		auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:a->source->width height:a->source->height mipmapped:a->source->levels>1];td.textureType=MTLTextureType2DArray;td.arrayLength=a->source->pages;td.mipmapLevelCount=a->source->levels;td.storageMode=MTLStorageModePrivate;td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
		a->texture=[l.Device() newTextureWithDescriptor:td];Require(a->texture!=nil,"Cannot allocate original multipage ground decal atlas");
		for(const auto& source:a->source->sources){id<MTLTexture> image=nil;if(textureProvider)image=textureProvider(l,source);else{const auto& b=source->mips.front();auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Format(source->format) width:b.width height:b.height mipmapped:NO];td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead;image=[l.Device() newTextureWithDescriptor:td];Require(image!=nil,"Cannot allocate original decal atlas source");[image replaceRegion:MTLRegionMake2D(0,0,b.width,b.height) mipmapLevel:0 withBytes:b.bytes.data() bytesPerRow:b.bytesPerRow];}
			Require(image!=nil,"Original decal shared image provider returned no image");a->inputs.push_back(image);}
		std::vector<NativeGroundDecalAtlasVertex> vertices;for(const auto& draw:a->draws)vertices.insert(vertices.end(),draw.vertices.begin(),draw.vertices.end());a->vertices=Buffer(l,vertices.data(),vertices.size()*sizeof(vertices[0]),"Original decal atlas snapped per-mip geometry");impl->atlas=a;
	}
	Require(impl->atlas->source==frame->atlas,"Original decal atlas changed within one generation");p->atlas=impl->atlas;l.AddAllocation(p->atlas->texture);l.AddAllocation(p->atlas->vertices);for(auto image:p->atlas->inputs)l.AddAllocation(image);
	if(!p->atlas->ready){p->atlasPipeline=impl->Pipeline(l,MTLPixelFormatRGBA8Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,true,false,1,validation,milliseconds);l.Retain(p->atlasPipeline);
		for(size_t i=0;i<p->atlas->draws.size();++i){const auto& draw=p->atlas->draws[i];const auto& src=*p->atlas->source->sources[draw.source];AtlasUniforms u{draw.sourceClamp,{src.flipV,0,0,0}};
			auto b=Buffer(l,&u,sizeof(u),"Original decal atlas source clamp");auto args=Arguments(l,2,1,1);[args setAddress:p->atlas->vertices.gpuAddress+i*6*sizeof(NativeGroundDecalAtlasVertex) atIndex:0];[args setAddress:b.gpuAddress atIndex:1];[args setTexture:p->atlas->inputs[draw.source].gpuResourceID atIndex:0];[args setSamplerState:impl->samplers[2].gpuResourceID atIndex:0];p->atlasDraws.push_back({draw.page,draw.level,args});}
	}
	if(!impl->instances||impl->instances->source!=frame->instances){auto r=std::make_shared<InstanceResources>();r->source=frame->instances;NativeGroundDecalInstance empty{};const auto& v=frame->instances->values;r->buffer=Buffer(l,v.empty()?&empty:v.data(),v.empty()?sizeof(empty):v.size()*sizeof(v[0]),"Original ground decal immutable instance revision");impl->instances=r;}
	p->instances=impl->instances;l.AddAllocation(p->instances->buffer);
	const std::array<GroundDecalSceneImage,6> sources={h,n,m,images.shadowDepth,images.shadowColor,d};uint32_t flips=0;
	for(size_t i=0;i<sources.size();++i){flips|=uint32_t(sources[i].flipV)<<i;if(sources[i].texture)l.AddAllocation(sources[i].texture);}
	const auto uniforms=MakeNativeGroundDecalUniforms(*frame,images.fragmentToLogical,flips);auto ub=Buffer(l,&uniforms,sizeof(uniforms),"Original ground decal matrices and material");p->arguments=Arguments(l,2,7,6);[p->arguments setAddress:p->instances->buffer.gpuAddress atIndex:0];[p->arguments setAddress:ub.gpuAddress atIndex:1];
	for(size_t i=0;i<sources.size();++i)if(sources[i].texture)[p->arguments setTexture:sources[i].texture.gpuResourceID atIndex:i];[p->arguments setTexture:p->atlas->texture.gpuResourceID atIndex:6];for(size_t i=0;i<impl->samplers.size();++i)[p->arguments setSamplerState:impl->samplers[i].gpuResourceID atIndex:i];
	return std::unique_ptr<PreparedGroundDecal>(new PreparedGroundDecal(std::move(p)));
}
}
