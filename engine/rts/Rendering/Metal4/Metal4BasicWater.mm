/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4BasicWater.h"
#include "Metal4WorldBCDecode.h"
#include "NativeBasicWaterShaders.h"
#include <algorithm>
#include <bit>
#include <map>
#include <tuple>

namespace rendering::metal4 {
namespace {
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* bytes,size_t size,const char* name)
{
	Require(bytes&&size,"Original BasicWater buffer empty");
	auto b=[lease.Device() newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared];
	Require(b!=nil,"Cannot allocate original BasicWater buffer");b.label=String(name);lease.AddAllocation(b);return b;
}
MTLPixelFormat Format(WorldTextureFormat f)
{
	switch(f){case WorldTextureFormat::RGBA8Unorm:return MTLPixelFormatRGBA8Unorm;case WorldTextureFormat::BGRA8Unorm:return MTLPixelFormatBGRA8Unorm;
	case WorldTextureFormat::BC1RGBA:return MTLPixelFormatBC1_RGBA;case WorldTextureFormat::BC2RGBA:return MTLPixelFormatBC2_RGBA;case WorldTextureFormat::BC3RGBA:return MTLPixelFormatBC3_RGBA;
	case WorldTextureFormat::BC4Unorm:return MTLPixelFormatBC4_RUnorm;case WorldTextureFormat::BC5Unorm:return MTLPixelFormatBC5_RGUnorm;
	case WorldTextureFormat::BC7RGBA:return MTLPixelFormatBC7_RGBAUnorm;case WorldTextureFormat::BC7RGBASRGB:return MTLPixelFormatBC7_RGBAUnorm_sRGB;}
	throw std::invalid_argument("Original BasicWater image format unknown");
}
struct Resources {
	std::shared_ptr<const NativeBasicWaterMaterial> material;
	__strong id<MTLBuffer> vertices=nil,indices=nil;
	__strong id<MTLTexture> texture=nil;
	bool mipsReady=false;
};
}
struct PreparedBasicWater::Impl {
	std::shared_ptr<const NativeBasicWaterFrame> frame;
	std::shared_ptr<Resources> resources;
	__strong id<MTL4ArgumentTable> arguments=nil;
	__strong id<MTLRenderPipelineState> pipeline=nil;
	__strong id<MTLDepthStencilState> depth=nil;
	bool generateMips=false,dependenciesEncoded=false,encoded=false;
};
PreparedBasicWater::PreparedBasicWater(std::unique_ptr<Impl> value):impl(std::move(value)){}
PreparedBasicWater::~PreparedBasicWater()=default;
id<MTLTexture> PreparedBasicWater::Texture()const{return impl->resources->texture;}
void PreparedBasicWater::EncodeDependencies(id<MTL4CommandBuffer> command)
{
	Require(!impl->dependenciesEncoded,"Original BasicWater dependencies encoded twice");
	if(impl->generateMips){auto e=[command computeCommandEncoder];Require(e!=nil,"Cannot encode original BasicWater mipmaps");
		[e generateMipmapsForTexture:impl->resources->texture];
		[e barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];}
	impl->dependenciesEncoded=true;
}
void PreparedBasicWater::Encode(id<MTL4RenderCommandEncoder> e)
{
	Require(impl->dependenciesEncoded&&!impl->encoded,"Original BasicWater dependency/surface order differs");
	[e setRenderPipelineState:impl->pipeline];[e setDepthStencilState:impl->depth];
	[e setCullMode:MTLCullModeNone];[e setDepthClipMode:MTLDepthClipModeClip];[e setDepthBias:0 slopeScale:0 clamp:0];
	[e setTriangleFillMode:impl->frame->wireframe?MTLTriangleFillModeLines:MTLTriangleFillModeFill];
	[e setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:impl->frame->material->mesh->indices.size() indexType:MTLIndexTypeUInt32 indexBuffer:impl->resources->indices.gpuAddress indexBufferLength:impl->resources->indices.length];
	impl->encoded=true;
}
void PreparedBasicWater::DidComplete()
{
	Require(impl->dependenciesEncoded&&impl->encoded,"Original BasicWater cannot retire an unencoded frame");impl->resources->mipsReady=true;
}
struct BasicWaterGPU::Impl {
	using Key=std::tuple<MTLPixelFormat,bool,unsigned>;
	std::map<Key,id<MTLRenderPipelineState>> pipelines;
	std::array<id<MTLSamplerState>,2> samplers{};
	__strong id<MTLDepthStencilState> depth=nil;
	std::shared_ptr<Resources> resources;
};
BasicWaterGPU::BasicWaterGPU():impl(std::make_unique<Impl>()){}
BasicWaterGPU::~BasicWaterGPU()=default;
void BasicWaterGPU::ClearResources(){impl->resources.reset();}
std::unique_ptr<PreparedBasicWater> BasicWaterGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeBasicWaterFrame> frame,
	MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& compileMs,unsigned samples)
{
	Require(frame!=nullptr,"Missing original BasicWater frame");ValidateNativeBasicWater(*frame);
	Require(NativeBasicWaterVisible(*frame),"Original world stage does not draw Low water on this map/frame");
	Require(depth!=MTLPixelFormatInvalid,"Original BasicWater requires actual scene depth");(void)stencil;
	auto prepared=std::make_unique<PreparedBasicWater::Impl>();prepared->frame=frame;
	const auto key=Impl::Key{color,validation,lease.SampleShadingKey(samples)};auto it=impl->pipelines.find(key);
	if(it==impl->pipelines.end()){
		const auto start=Clock::now();auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
		NSError* error=nil;auto lib=lease.NewLibraryWithSource(String(NativeBasicWaterShaders),options,&error);Require(lib!=nil,"Cannot compile original BasicWater shader: "+Error(error));
		auto d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=samples;auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];
		v.library=f.library=lib;v.name=@"BasicWaterVertex";f.name=@"BasicWaterFragment";d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
		auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=MTL4BlendStateEnabled;a.rgbBlendOperation=a.alphaBlendOperation=MTLBlendOperationAdd;
		a.sourceRGBBlendFactor=a.sourceAlphaBlendFactor=MTLBlendFactorSourceAlpha;a.destinationRGBBlendFactor=a.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
		auto po=[MTL4PipelineOptions new];po.shaderValidation=ShaderValidationMode(validation);d.options=po;
		auto pipeline=lease.NewRenderPipeline(d,&error);Require(pipeline!=nil,"Cannot compile original BasicWater pipeline: "+Error(error));
		it=impl->pipelines.emplace(key,pipeline).first;compileMs+=Milliseconds(start);
	}
	prepared->pipeline=it->second;lease.Retain(it->second);
	if(!impl->depth){auto d=[MTLDepthStencilDescriptor new];d.depthCompareFunction=MTLCompareFunctionLessEqual;d.depthWriteEnabled=NO;impl->depth=[lease.Device() newDepthStencilStateWithDescriptor:d];Require(impl->depth!=nil,"Cannot create original BasicWater depth state");}
	prepared->depth=impl->depth;lease.Retain(impl->depth);
	if(!impl->resources||impl->resources->material->generation!=frame->material->generation){
		ValidateNativeBasicWaterMaterial(*frame->material);
		auto r=std::make_shared<Resources>();r->material=frame->material;const auto& m=*r->material->mesh;const auto& t=*r->material->texture;const auto& base=t.mips.front();
		r->vertices=Buffer(lease,m.vertices.data(),m.vertices.size()*sizeof(NativeBasicWaterVertex),"Original Low water immutable16x16 vertices");
		r->indices=Buffer(lease,m.indices.data(),m.indices.size()*sizeof(uint32_t),"Original Low water AddQuadTriangles indices");
		const uint32_t levels=t.generateMipmaps?std::bit_width(std::max(base.width,base.height)):uint32_t(t.mips.size());
		const auto decoded=DecodeWorldBCBaseForMipmaps(t,lease.Capabilities().maxTextureDimension2D);
		const auto format=decoded?(t.format==WorldTextureFormat::BC7RGBASRGB?MTLPixelFormatRGBA8Unorm_sRGB:MTLPixelFormatRGBA8Unorm):Format(t.format);
		auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:base.width height:base.height mipmapped:levels>1];d.mipmapLevelCount=levels;d.storageMode=MTLStorageModeShared;d.usage=MTLTextureUsageShaderRead;
		r->texture=[lease.Device() newTextureWithDescriptor:d];Require(r->texture!=nil,"Cannot allocate original BasicWater image");r->texture.label=String(t.sourcePath);
		for(uint32_t i=0;i<t.mips.size();++i){const auto& mip=decoded?*decoded:t.mips[i];[r->texture replaceRegion:MTLRegionMake2D(0,0,mip.width,mip.height) mipmapLevel:i withBytes:mip.bytes.data() bytesPerRow:mip.bytesPerRow];}
		r->mipsReady=!t.generateMipmaps||levels==1;impl->resources=std::move(r);
	}
	Require(impl->resources->material==frame->material,"Original BasicWater immutable resources changed without reload");
	prepared->resources=impl->resources;prepared->generateMips=!impl->resources->mipsReady;
	lease.AddAllocation(impl->resources->vertices);lease.AddAllocation(impl->resources->indices);lease.AddAllocation(impl->resources->texture);
	const auto& texture=*frame->material->texture;
	const bool mips=texture.mipmappedSampling&&impl->resources->texture.mipmapLevelCount>1;
	if(!impl->samplers[mips]){auto d=[MTLSamplerDescriptor new];d.supportArgumentBuffers=YES;d.minFilter=d.magFilter=MTLSamplerMinMagFilterLinear;
		d.mipFilter=mips?MTLSamplerMipFilterLinear:MTLSamplerMipFilterNotMipmapped;d.sAddressMode=d.tAddressMode=MTLSamplerAddressModeRepeat;
		impl->samplers[mips]=[lease.Device() newSamplerStateWithDescriptor:d];Require(impl->samplers[mips]!=nil,"Cannot create original BasicWater sampler");}
	auto sampler=impl->samplers[mips];lease.Retain(sampler);
	const auto u=MakeNativeBasicWaterUniforms(*frame);auto uniforms=Buffer(lease,&u,sizeof(u),"Original BasicWater camera uniforms");
	auto d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=2;d.maxTextureBindCount=1;d.maxSamplerStateBindCount=1;d.initializeBindings=YES;
	prepared->arguments=[lease.Device() newArgumentTableWithDescriptor:d error:nil];Require(prepared->arguments!=nil,"Cannot allocate original BasicWater arguments");lease.Retain(prepared->arguments);
	[prepared->arguments setAddress:impl->resources->vertices.gpuAddress atIndex:0];[prepared->arguments setAddress:uniforms.gpuAddress atIndex:1];
	[prepared->arguments setTexture:impl->resources->texture.gpuResourceID atIndex:0];[prepared->arguments setSamplerState:sampler.gpuResourceID atIndex:0];
	return std::unique_ptr<PreparedBasicWater>(new PreparedBasicWater(std::move(prepared)));
}
} // namespace rendering::metal4
