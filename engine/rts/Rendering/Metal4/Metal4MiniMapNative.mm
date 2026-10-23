/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4MiniMapNative.h"
#include "Metal4MetalInfo.h"
#include "Metal4WorldInfo.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <simd/simd.h>

namespace rendering::metal4 {
namespace {
const char* SHADERS=R"MSL(
#include <metal_stdlib>
using namespace metal;
struct ShadeUniform { float4 ambient,diffuse,light,waterBase,waterAbsorb,waterMinimum,parameters;uint4 writtenRect; };
float3 TerrainVertex(int2 point,texture2d<float,access::read> heights) {
	// The legacy shader clamps to the dimension inclusive, then texelFetches
	// outside it on the far neighbor. Robust GL reads return zero there.
	// Keep that position/zero-height boundary, without an unsafe Metal read.
	point=clamp(point,int2(0),int2(heights.get_width(),heights.get_height()));
	float h=all(point<int2(heights.get_width(),heights.get_height()))?heights.read(uint2(point)).r:0;
	return float3(point.x*8,h,point.y*8);
}
kernel void MiniMapShade(uint2 point [[thread_position_in_grid]],constant ShadeUniform& u [[buffer(0)]],
	texture2d<float,access::read> heights [[texture(0)]],texture2d<float,access::write> result [[texture(1)]]) {
	point+=u.writtenRect.xy;
	if(any(point>=u.writtenRect.zw)||point.x>=result.get_width()||point.y>=result.get_height())return;
	const int2 p=int2(point);const float3 center=TerrainVertex(p,heights);
	const float3 bl=TerrainVertex(p+int2(-1,-1),heights)-center,bm=TerrainVertex(p+int2(0,-1),heights)-center;
	const float3 br=TerrainVertex(p+int2(1,-1),heights)-center,ml=TerrainVertex(p+int2(-1,0),heights)-center;
	const float3 mr=TerrainVertex(p+int2(1,0),heights)-center,tl=TerrainVertex(p+int2(-1,1),heights)-center;
	const float3 tm=TerrainVertex(p+int2(0,1),heights)-center,tr=TerrainVertex(p+int2(1,1),heights)-center;
	const float3 n=normalize(cross(tr,mr)+cross(mr,br)+cross(br,bm)+cross(bm,bl)+cross(bl,ml)+cross(ml,tl)+cross(tl,tm)+cross(tm,tr));
	const float ndl=max(dot(n,u.light.xyz),0.f),h=center.y;
	const float3 lighting=min((u.ambient.rgb+u.diffuse.rgb*ndl)*(210.f/255.f),float3(1));
	float4 output=float4(lighting,1);
	if(h<u.parameters.x) {
		const float depth=u.parameters.x-h;
		const float3 water=clamp(u.waterBase.rgb-u.waterAbsorb.rgb*clamp(depth,0.f,1024.f),u.waterMinimum.rgb,float3(1));
		const float intensity=min((ndl+.2f)*2.f,1.f);
		output.rgb=h>u.parameters.x-10.f?water*intensity*(depth*.1f)+lighting*(1-depth*.1f):water*intensity;
		output.a=clamp((255-10*depth)/255,0.f,1.f);
	}
	result.write(output,point);
}
struct CoverageUniform { uint4 options; };
kernel void MiniMapCoverage(uint2 p [[thread_position_in_grid]],constant CoverageUniform& u [[buffer(0)]],
	texture2d<uint,access::read> raw [[texture(0)]],texture2d<float,access::write> output [[texture(1)]]) {
	if(p.x>=output.get_width()||p.y>=output.get_height())return;
	output.write(float4(float(u.options.x!=0 || raw.read(p).r>0)),p);
}
kernel void MiniMapRadar(uint2 p [[thread_position_in_grid]],constant CoverageUniform& u [[buffer(0)]],
	texture2d<uint,access::read> rawRadar [[texture(0)]],texture2d<uint,access::read> rawJammer [[texture(1)]],
	texture2d<float> los [[texture(2)]],texture2d<float,access::write> output [[texture(3)]]) {
	if(p.x>=output.get_width()||p.y>=output.get_height())return;
	constexpr sampler losSampler(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::nearest);
	float2 uv=(float2(p)+.5f)/float2(output.get_width(),output.get_height());
	const float lod=max(log2(max(float(los.get_width())/output.get_width(),float(los.get_height())/output.get_height())),0.f);
	float2 value=u.options.x!=0?float2(1,0):float2(float(rawRadar.read(p).r>0),los.sample(losSampler,uv,level(round(lod))).r*float(rawJammer.read(p).r>0));
	output.write(float4(value,0,1),p);
}
struct InfoUniform { float4 alwaysColor,losColor,radarColor,jammerColor,radarColor2,parameters; };
struct InfoVarying { float4 position [[position]];float2 uv; };
vertex InfoVarying MiniMapInfoVertex(uint vid [[vertex_id]]) {
	float2 uv=vid==0?float2(0,0):(vid==1?float2(2,0):float2(0,2));
	InfoVarying value;value.position=float4(uv.x*2-1,1-uv.y*2,0,1);value.uv=uv;return value;
}
float Random(float2 p){float3 p3=fract(float3(p.x,p.y,p.x)*443.8975f);p3+=dot(p3,p3.yzx+19.19f);return fract((p3.x+p3.y)*p3.z);}
float4 InfoSample(texture2d<float> tex,sampler filter,float2 p,constant InfoUniform& u) {
	if(u.parameters.y==0)return tex.sample(filter,p);
	float lod=max(tex.calculate_clamped_lod(filter,p),0.f);
	uint levelIndex=uint(lod);float2 size=float2(tex.get_width(levelIndex),tex.get_height(levelIndex));
	float2 off=float2(u.parameters.x);float4 color=0;
	for(int i=0;i<4;++i){off=(float2(Random(p+off),Random(p.yx-off.yx))*2-1)/size;color+=tex.sample(filter,p+off);}
	return smoothstep(.5f,1.f,color*.25f);
}
fragment float4 MiniMapInfoFragment(InfoVarying in [[stage_in]],constant InfoUniform& u [[buffer(0)]],
	texture2d<float> los [[texture(0)]],texture2d<float> air [[texture(1)]],texture2d<float> radar [[texture(2)]]) {
	constexpr sampler ls(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::nearest);
	constexpr sampler rs(coord::normalized,address::clamp_to_edge,min_filter::nearest,mag_filter::linear);
	const float ground=InfoSample(los,ls,in.uv,u).r,airLos=InfoSample(air,ls,in.uv,u).r;
	const float2 rj=InfoSample(radar,rs,in.uv,u).rg;
	float3 result=u.losColor.rgb*(ground*.9f+airLos*.1f)+u.jammerColor.rgb*rj.y;
	result+=fract(u.radarColor2.rgb)*step(1.f,rj.x)*(1-ground);
	result+=u.radarColor2.rgb*fract(step(.8f,rj.x)*rj.x);
	result+=u.radarColor.rgb*step(.2f,fract(1-rj.x));result+=u.alwaysColor.rgb;
	return float4(result,.05f);
}
// Exact selected test30770 infoLOS.lua, separate from engine fallback above.
float GameInfoRandom(float2 p){return fract(sin(dot(p,float2(12.9898f,78.233f)))*43758.5453f);}
float4 GameInfoSample(texture2d<float> tex,sampler filter,float2 p,constant InfoUniform& u){
 if(u.parameters.y==0)return tex.sample(filter,p);
 const float2 size=float2(tex.get_width(0),tex.get_height(0));float4 c=0;
 for(int i=0;i<4;++i){float2 off=float2(u.parameters.x+float(i)*.234567f);off=(float2(GameInfoRandom(p+off),GameInfoRandom(p.yx-off.yx))*2-1)/size;c+=tex.sample(filter,p+off);}
 return smoothstep(.5f,1.f,c*.25f);
}
fragment float4 MiniMapGameInfoFragment(InfoVarying in [[stage_in]],constant InfoUniform& u [[buffer(0)]],texture2d<float> los [[texture(0)]],texture2d<float> air [[texture(1)]],texture2d<float> radar [[texture(2)]]){
 constexpr sampler ls(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::nearest);
 constexpr sampler rs(coord::normalized,address::clamp_to_edge,min_filter::nearest,mag_filter::linear);
 float4 color=u.alwaysColor;const float4 rj=GameInfoSample(radar,rs,in.uv,u);color=max(color,u.radarColor2*rj.r);
 const float ground=GameInfoSample(los,ls,in.uv,u).r,airLos=GameInfoSample(air,ls,in.uv,u).r;
 color=max(color,u.losColor*mix(ground,airLos,.2f));color+=u.jammerColor*rj.g;color.a=.03f;return color;
}

)MSL";
struct ShadeUniform {simd_float4 ambient,diffuse,light,waterBase,waterAbsorb,waterMinimum,parameters;simd_uint4 writtenRect;};
struct InfoUniform {simd_float4 alwaysColor,losColor,radarColor,jammerColor,radarColor2,parameters;};
simd_float4 Vec(const std::array<float,4>& v) {return {v[0],v[1],v[2],v[3]};}
simd_float4 Vec(const std::array<float,3>& v) {return {v[0],v[1],v[2],0};}
id<MTLBuffer> Upload(RuntimeLease& lease,const void* data,size_t size) {
	Require(size && size<=lease.Device().maxBufferLength,"Native minimap buffer size exceeds device bounds");
	auto buffer=[lease.Device() newBufferWithBytes:data length:size options:MTLResourceStorageModeShared];
	Require(buffer!=nil,"Cannot allocate native minimap uniform");lease.AddAllocation(buffer);return buffer;
}
id<MTLTexture> Texture(RuntimeLease& lease,MTLPixelFormat format,uint32_t width,uint32_t height,bool mips,MTLTextureUsage usage,MTLStorageMode storage) {
	Require(width && height && width<=16384 && height<=16384,"Native minimap texture extent exceeds device bounds");
	auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:width height:height mipmapped:mips];
	d.usage=usage;d.storageMode=storage;auto image=[lease.Device() newTextureWithDescriptor:d];
	Require(image!=nil,"Cannot allocate original native minimap resource");lease.AddAllocation(image);return image;
}
id<MTL4ArgumentTable> Arguments(RuntimeLease& lease,uint32_t buffers,uint32_t textures) {
	MTL4ArgumentTableDescriptor* d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=buffers;d.maxTextureBindCount=textures;d.initializeBindings=YES;
	NSError* error=nil;auto args=[lease.Device() newArgumentTableWithDescriptor:d error:&error];Require(args!=nil,"Cannot allocate native minimap arguments: "+Error(error));lease.Retain(args);return args;
}
}
const char* MiniMapScreenShaderSource() {return R"MSL(
struct MiniMapUniform {float4 parameters;};
fragment float4 MiniMapScreenFragment(ScreenVarying input [[stage_in]],constant ScreenUniform& screen [[buffer(2)]],
	constant MiniMapUniform& u [[buffer(3)]],texture2d<float> base [[texture(0)]],texture2d<float> shade [[texture(1)]],texture2d<float> info [[texture(2)]]) {
	constexpr sampler baseSampler(coord::normalized,address::repeat,filter::linear,mip_filter::linear);
	constexpr sampler shadeSampler(coord::normalized,address::clamp_to_edge,min_filter::nearest,mag_filter::linear);
	constexpr sampler infoSampler(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::nearest);
	float2 baseUV=input.uv;if(screen.options.y)baseUV.y=1-baseUV.y;
	return shade.sample(shadeSampler,input.uv)*base.sample(baseSampler,baseUV,bias(-2.f))+
		(info.sample(infoSampler,input.uv*u.parameters.xy,bias(-2.f))-.5f)*u.parameters.z;
}
)MSL";}
struct PreparedMiniMap::Impl {
	struct Compute {__strong id<MTLComputePipelineState> pipeline=nil;__strong id<MTL4ArgumentTable> args=nil;__strong id<MTLTexture> output=nil;bool mips=false;uint32_t width=0,height=0;};
	std::vector<Compute> before;
	__strong NSMutableArray* infoAllocations=nil;
	std::shared_ptr<PreparedMetalInfo> metal;
	std::shared_ptr<PreparedWorldInfo> globalInfo;
	__strong id<MTLTexture> shade=nil,info=nil,height=nil,groundLos=nil,radar=nil;
	__strong id<MTLBuffer> uniforms=nil;
	__strong MTL4RenderPassDescriptor* infoPass=nil;
	__strong id<MTLRenderPipelineState> infoPipeline=nil;
	__strong id<MTL4ArgumentTable> infoArgs=nil;
	std::vector<std::shared_ptr<bool>> initialized;
	uint32_t infoWidth=0,infoHeight=0;
};
PreparedMiniMap::PreparedMiniMap():impl(std::make_unique<Impl>()){}
PreparedMiniMap::~PreparedMiniMap()=default;
id<MTLTexture> PreparedMiniMap::Shading()const{return impl->shade;}
id<MTLTexture> PreparedMiniMap::Height()const{return impl->height;}
id<MTLTexture> PreparedMiniMap::GroundLOS()const{return impl->groundLos;}
id<MTLTexture> PreparedMiniMap::Radar()const{return impl->radar;}
id<MTLTexture> PreparedMiniMap::Info()const{return impl->info;}
id<MTLBuffer> PreparedMiniMap::Uniforms()const{return impl->uniforms;}
void PreparedMiniMap::EncodeDependencies(id<MTL4CommandBuffer> command)const {
	if(impl->metal)impl->metal->EncodeDependencies(command);
	if(impl->globalInfo)impl->globalInfo->EncodeDependencies(command);
	for(const auto& work:impl->before) {
		auto encoder=[command computeCommandEncoder];Require(encoder!=nil,"Cannot create native minimap compute encoder");
		[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];
		[encoder setComputePipelineState:work.pipeline];[encoder setArgumentTable:work.args];
		[encoder dispatchThreads:MTLSizeMake(work.width?work.width:work.output.width,work.height?work.height:work.output.height,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];
		[encoder barrierAfterStages:MTLStageDispatch beforeQueueStages:MTLStageVertex|MTLStageDispatch|MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		[encoder endEncoding];
		if(work.mips) {auto mip=[command computeCommandEncoder];[mip generateMipmapsForTexture:work.output];[mip barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageDispatch|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[mip endEncoding];}
	}
	if(impl->infoPass!=nil) {
		auto encoder=[command renderCommandEncoderWithDescriptor:impl->infoPass];Require(encoder!=nil,"Cannot create native minimap info encoder");
		[encoder barrierAfterQueueStages:MTLStageDispatch|MTLStageBlit|MTLStageFragment beforeStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
		if(impl->infoPipeline!=nil){
			[encoder setRenderPipelineState:impl->infoPipeline];[encoder setArgumentTable:impl->infoArgs atStages:MTLRenderStageFragment];
			[encoder setDepthStencilState:nil];[encoder setCullMode:MTLCullModeNone];
			[encoder setViewport:MTLViewport{0,0,double(impl->infoWidth),double(impl->infoHeight),0,1}];
			[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
		}
		[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageBlit|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];
		auto mip=[command computeCommandEncoder];[mip generateMipmapsForTexture:impl->info];[mip barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[mip endEncoding];
	}
}
void PreparedMiniMap::RegisterInfoResources(RuntimeLease& lease)const {
	for(id allocation in impl->infoAllocations)lease.AddAllocation((id<MTLAllocation>)allocation);
	for(const auto& work:impl->before){lease.Retain(work.pipeline);lease.Retain(work.args);}
	if(impl->infoPipeline!=nil)lease.Retain(impl->infoPipeline);if(impl->infoArgs!=nil)lease.Retain(impl->infoArgs);
}
void PreparedMiniMap::DidComplete()const {if(impl->globalInfo)impl->globalInfo->DidComplete();if(impl->metal)impl->metal->DidComplete();for(const auto& ready:impl->initialized)*ready=true;}
struct MiniMapGPU::Impl {
	__strong id<MTLLibrary> library=nil;bool validation=false;
	__strong id<MTLComputePipelineState> shadePipeline=nil,coveragePipeline=nil,radarPipeline=nil;
	__strong id<MTLRenderPipelineState> infoPipeline=nil,gameInfoPipeline=nil;
	std::shared_ptr<const WorldTerrainHeights> heightOwner;
	std::shared_ptr<const PreparedMiniMapHeight> heightResource;
	NativeMiniMapLighting lighting{};
	__strong id<MTLTexture> shade=nil,height=nil,info=nil;
	std::shared_ptr<bool> shadeReady,infoReady;
	std::weak_ptr<const NativeMiniMapInfo> lastInfo;
	uint64_t infoModeGeneration=0;
	id<MTLComputePipelineState> Compute(RuntimeLease& lease,NSString* name) {
		MTL4ComputePipelineDescriptor* d=[MTL4ComputePipelineDescriptor new];MTL4LibraryFunctionDescriptor* f=[MTL4LibraryFunctionDescriptor new];f.library=library;f.name=name;d.computeFunctionDescriptor=f;
		MTL4PipelineOptions* options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
		NSError* error=nil;auto pipeline=[lease.Compiler() newComputePipelineStateWithDescriptor:d compilerTaskOptions:nil error:&error];Require(pipeline!=nil,"Cannot compile native minimap compute pipeline: "+Error(error));return pipeline;
	}
	void Pipelines(RuntimeLease& lease,bool validate,double& shaderMs) {
		if(library!=nil && shadePipeline!=nil && coveragePipeline!=nil && radarPipeline!=nil && infoPipeline!=nil && gameInfoPipeline!=nil && validation==validate)return;
		const auto start=Clock::now();Impl candidate;candidate.validation=validate;NSError* error=nil;
		MTL4LibraryDescriptor* lib=[MTL4LibraryDescriptor new];lib.source=String(SHADERS);lib.name=@"Original SMF minimap shading and LOS";
		candidate.library=lease.NewLibrary(lib,&error);Require(candidate.library!=nil,"Cannot compile original native minimap shader: "+Error(error));
		candidate.shadePipeline=candidate.Compute(lease,@"MiniMapShade");candidate.coveragePipeline=candidate.Compute(lease,@"MiniMapCoverage");candidate.radarPipeline=candidate.Compute(lease,@"MiniMapRadar");
		MTL4RenderPipelineDescriptor* d=[MTL4RenderPipelineDescriptor new];
		MTL4LibraryFunctionDescriptor* vs=[MTL4LibraryFunctionDescriptor new];vs.library=candidate.library;vs.name=@"MiniMapInfoVertex";d.vertexFunctionDescriptor=vs;
		MTL4LibraryFunctionDescriptor* fs=[MTL4LibraryFunctionDescriptor new];fs.library=candidate.library;fs.name=@"MiniMapInfoFragment";d.fragmentFunctionDescriptor=fs;
		MTL4PipelineOptions* options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validate);d.options=options;
		auto* color=d.colorAttachments[0];color.pixelFormat=MTLPixelFormatRGB10A2Unorm;color.blendingState=MTL4BlendStateEnabled;
		color.sourceRGBBlendFactor=MTLBlendFactorSourceAlpha;color.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
		color.writeMask=MTLColorWriteMaskRed|MTLColorWriteMaskGreen|MTLColorWriteMaskBlue;
		d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
		candidate.infoPipeline=lease.NewRenderPipeline(d,&error);Require(candidate.infoPipeline!=nil,"Cannot compile native minimap info blend pipeline: "+Error(error));
		fs=[MTL4LibraryFunctionDescriptor new];fs.library=candidate.library;fs.name=@"MiniMapGameInfoFragment";d.fragmentFunctionDescriptor=fs;
		candidate.gameInfoPipeline=lease.NewRenderPipeline(d,&error);Require(candidate.gameInfoPipeline!=nil,"Cannot compile original selected game LOS combiner: "+Error(error));library=candidate.library;shadePipeline=candidate.shadePipeline;coveragePipeline=candidate.coveragePipeline;radarPipeline=candidate.radarPipeline;infoPipeline=candidate.infoPipeline;gameInfoPipeline=candidate.gameInfoPipeline;validation=validate;shaderMs+=Milliseconds(start);
	}
};
MiniMapGPU::MiniMapGPU():impl(std::make_unique<Impl>()){}
MiniMapGPU::~MiniMapGPU()=default;
void MiniMapGPU::Initialize(RuntimeLease& lease,bool validation,double& shaderMs) {
	// Do not publish a partial program set after a compiler/setup exception.
	auto initialized=std::make_unique<Impl>();
	initialized->Pipelines(lease,validation,shaderMs);
	impl=std::move(initialized);
}
void MiniMapGPU::ClearResources() {
	auto cleared=std::make_unique<Impl>();
	cleared->library=impl->library;cleared->validation=impl->validation;
	cleared->shadePipeline=impl->shadePipeline;cleared->coveragePipeline=impl->coveragePipeline;
	cleared->radarPipeline=impl->radarPipeline;cleared->infoPipeline=impl->infoPipeline;cleared->gameInfoPipeline=impl->gameInfoPipeline;
	impl=std::move(cleared);
}
std::shared_ptr<const PreparedMiniMapHeight> MiniMapGPU::PrepareHeight(RuntimeLease& lease,std::shared_ptr<const WorldTerrainHeights> source) {
	Require(source && source->width && source->height && source->corners.size()==size_t(source->width)*source->height,"Original map corner height owner/extent differs");
	if(!impl->heightResource || impl->heightResource->source!=source) {
		for(float value:source->corners)Require(std::isfinite(value),"Nonfinite minimap height");
		auto next=std::make_shared<PreparedMiniMapHeight>();next->source=source;
		// Shared map-height consumers retain this storage; generic sampling also
		// needs a view for the original RRRR channels without duplicating pixels.
		next->texture=Texture(lease,MTLPixelFormatR32Float,source->width,source->height,false,MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView,MTLStorageModeShared);
		[next->texture replaceRegion:MTLRegionMake2D(0,0,source->width,source->height) mipmapLevel:0 withBytes:source->corners.data() bytesPerRow:size_t(source->width)*4];
		impl->height=next->texture;impl->heightResource=std::move(next);
	}
	lease.AddAllocation(impl->heightResource->texture);return impl->heightResource;
}
std::shared_ptr<PreparedMiniMap> MiniMapGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeMiniMapBackground> source,bool validation,double& shaderMs,bool individualImagesOnly) {
	Require(source && source->base && source->heights,"Native minimap background lacks its original owned source");
	Require(source->squaresX && source->squaresZ && source->heights->width==source->squaresX+1 && source->heights->height==source->squaresZ+1 &&
		source->heights->corners.size()==size_t(source->heights->width)*source->heights->height,"Native minimap corner geometry mismatch");
	Require(source->paddedSquaresX>=source->squaresX && source->paddedSquaresZ>=source->squaresZ,"Native minimap padded map bounds mismatch");
	for(const auto& values:{source->lighting.groundAmbient,source->lighting.groundDiffuse,source->lighting.lightDirection})
		for(float v:values)Require(std::isfinite(v),"Nonfinite minimap lighting input");
	for(const auto& values:{source->lighting.waterBase,source->lighting.waterAbsorb,source->lighting.waterMinimum})
		for(float v:values)Require(std::isfinite(v),"Nonfinite minimap water input");
	Require(std::isfinite(source->lighting.waterLevel),"Nonfinite minimap water plane");
	impl->Pipelines(lease,validation,shaderMs);lease.Retain(impl->library);lease.Retain(impl->shadePipeline);lease.Retain(impl->coveragePipeline);lease.Retain(impl->radarPipeline);lease.Retain(impl->infoPipeline);
	auto result=std::shared_ptr<PreparedMiniMap>(new PreparedMiniMap());
	const auto& heights=*source->heights;
	const bool heightChanged=impl->heightOwner!=source->heights;
	(void)PrepareHeight(lease,source->heights);
	if(heightChanged || impl->lighting!=source->lighting || !impl->shadeReady || !*impl->shadeReady) {
		impl->shade=Texture(lease,MTLPixelFormatRGBA8Unorm,heights.width,heights.height,false,MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite,MTLStorageModePrivate);
		const auto& l=source->lighting;
		ShadeUniform uniform{Vec(l.groundAmbient),Vec(l.groundDiffuse),Vec(l.lightDirection),Vec(l.waterBase),Vec(l.waterAbsorb),Vec(l.waterMinimum),{l.waterLevel,0,0,0},{0,0,heights.width,heights.height}};
		const auto buffer=Upload(lease,&uniform,sizeof(uniform));auto args=Arguments(lease,1,2);
		[args setAddress:buffer.gpuAddress atIndex:0];[args setTexture:impl->height.gpuResourceID atIndex:0];[args setTexture:impl->shade.gpuResourceID atIndex:1];
		result->impl->before.push_back({impl->shadePipeline,args,impl->shade,false});
		impl->heightOwner=source->heights;impl->lighting=l;impl->shadeReady=std::make_shared<bool>(false);result->impl->initialized.push_back(impl->shadeReady);
	}
	lease.AddAllocation(impl->height);lease.AddAllocation(impl->shade);result->impl->shade=impl->shade;result->impl->height=impl->height;
	result->impl->info=impl->shade;
	if(source->info) {
		const auto& info=*source->info;
		if(info.worldInfo&&!individualImagesOnly){
			result->impl->globalInfo=SharedWorldInfoGPU().Prepare(lease,info.worldInfo,validation,shaderMs);result->impl->info=result->impl->globalInfo->Info();
		}else if(info.mode==NativeMiniMapInfoMode::Metal){
			result->impl->metal=SharedMetalInfoGPU().Prepare(lease,info.metal,validation,shaderMs);result->impl->info=result->impl->metal->Info();
			impl->info=nil;impl->infoReady.reset();impl->lastInfo.reset();
		}else{
		Require(info.mode==NativeMiniMapInfoMode::Los,"Native minimap requested info producer is not implemented");
		const auto coverage=[&](const std::shared_ptr<const NativeMiniMapCoverage>& data){
			Require(data && data->width && data->height && data->counts.size()==size_t(data->width)*data->height,"Original native minimap coverage bounds mismatch");
			auto raw=Texture(lease,MTLPixelFormatR16Uint,data->width,data->height,false,MTLTextureUsageShaderRead,MTLStorageModeShared);
			[raw replaceRegion:MTLRegionMake2D(0,0,data->width,data->height) mipmapLevel:0 withBytes:data->counts.data() bytesPerRow:size_t(data->width)*2];return raw;
		};
		std::array<uint32_t,4> options{uint32_t(info.globalLos),0,0,0};auto optionBuffer=Upload(lease,options.data(),sizeof(options));
		const auto visibility=[&](const auto& data) {
			auto raw=coverage(data);auto output=Texture(lease,MTLPixelFormatR8Unorm,data->width,data->height,true,MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite,MTLStorageModePrivate);
			auto args=Arguments(lease,1,2);[args setAddress:optionBuffer.gpuAddress atIndex:0];[args setTexture:raw.gpuResourceID atIndex:0];[args setTexture:output.gpuResourceID atIndex:1];
			result->impl->before.push_back({impl->coveragePipeline,args,output,true});return output;
		};
		auto los=visibility(info.groundLos);
		id<MTLTexture> air=individualImagesOnly?nil:visibility(info.airLos);
		auto radar=coverage(info.radar),jammer=coverage(info.jammer);
		Require(info.radar->width==info.jammer->width && info.radar->height==info.jammer->height,"Original radar/jammer extents differ");
		auto radarOutput=Texture(lease,MTLPixelFormatRG8Unorm,info.radar->width,info.radar->height,true,MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite,MTLStorageModePrivate);
		auto args=Arguments(lease,1,4);[args setAddress:optionBuffer.gpuAddress atIndex:0];[args setTexture:radar.gpuResourceID atIndex:0];[args setTexture:jammer.gpuResourceID atIndex:1];[args setTexture:los.gpuResourceID atIndex:2];[args setTexture:radarOutput.gpuResourceID atIndex:3];
		result->impl->before.push_back({impl->radarPipeline,args,radarOutput,true});
		result->impl->groundLos=los;result->impl->radar=radarOutput;
		// PiP consumes these original images directly, not the global LOS overlay.
		if(individualImagesOnly)return result;
		const bool reset=impl->infoModeGeneration!=info.modeGeneration || impl->info==nil || impl->info.width!=source->paddedSquaresX || impl->info.height!=source->paddedSquaresZ || !impl->infoReady || !*impl->infoReady;
		if(reset) {impl->info=Texture(lease,MTLPixelFormatRGB10A2Unorm,source->paddedSquaresX,source->paddedSquaresZ,true,MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget,MTLStorageModePrivate);impl->infoReady=std::make_shared<bool>(false);}
		lease.AddAllocation(impl->info);result->impl->info=impl->info;
		if(impl->lastInfo.lock()!=source->info || reset) {
			InfoUniform u{Vec(info.alwaysColor),Vec(info.losColor),Vec(info.radarColor),Vec(info.jammerColor),Vec(info.radarColor2),{info.time,float(info.highQuality),0,0}};
			auto buffer=Upload(lease,&u,sizeof(u));auto table=Arguments(lease,1,3);[table setAddress:buffer.gpuAddress atIndex:0];[table setTexture:los.gpuResourceID atIndex:0];[table setTexture:air.gpuResourceID atIndex:1];[table setTexture:radarOutput.gpuResourceID atIndex:2];
			result->impl->infoArgs=table;result->impl->infoPipeline=impl->infoPipeline;
			auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=impl->info;pass.colorAttachments[0].loadAction=reset?MTLLoadActionClear:MTLLoadActionLoad;
			pass.colorAttachments[0].clearColor=MTLClearColorMake(.5,.5,.5,1);pass.colorAttachments[0].storeAction=MTLStoreActionStore;
			pass.renderTargetWidth=source->paddedSquaresX;pass.renderTargetHeight=source->paddedSquaresZ;
			result->impl->infoPass=pass;result->impl->infoWidth=source->squaresX;result->impl->infoHeight=source->squaresZ;result->impl->initialized.push_back(impl->infoReady);impl->lastInfo=source->info;impl->infoModeGeneration=info.modeGeneration;
		}
		}
	} else {impl->info=nil;impl->infoReady.reset();impl->lastInfo.reset();}
	const simd_float4 dimensions{float(source->squaresX)/source->paddedSquaresX,float(source->squaresZ)/source->paddedSquaresZ,float(source->info!=nullptr),0};
	result->impl->uniforms=Upload(lease,&dimensions,sizeof(dimensions));return result;
}
std::shared_ptr<PreparedMiniMap> MiniMapGPU::PrepareShadingImage(RuntimeLease& lease,
	std::shared_ptr<const NativeMiniMapBackground> source,id<MTLTexture> destination,
	std::array<int32_t,4> rect,bool validation,double& shaderMs)
{
	Require(source&&source->heights&&destination,"Original partial SMF shading lacks its actual source/output owner");
	const auto& heights=*source->heights;
	Require(source->squaresX&&source->squaresZ&&heights.width==source->squaresX+1&&heights.height==source->squaresZ+1&&
		destination.textureType==MTLTextureType2D&&destination.pixelFormat==MTLPixelFormatRGBA8Unorm&&
		destination.width==heights.width&&destination.height==heights.height&&destination.mipmapLevelCount==1,
		"Original partial SMF shading output differs from its actual RGBA8 allocation");
	Require(rect[0]>=0&&rect[1]>=0&&rect[0]<=rect[2]&&rect[1]<=rect[3]&&
		uint32_t(rect[2])<=heights.width&&uint32_t(rect[3])<=heights.height,
		"Original partial SMF shading rectangle exceeds its actual output");
	for(const auto& values:{source->lighting.groundAmbient,source->lighting.groundDiffuse,source->lighting.lightDirection})
		for(float v:values)Require(std::isfinite(v),"Nonfinite original partial shading lighting");
	for(const auto& values:{source->lighting.waterBase,source->lighting.waterAbsorb,source->lighting.waterMinimum})
		for(float v:values)Require(std::isfinite(v),"Nonfinite original partial shading water input");
	Require(std::isfinite(source->lighting.waterLevel),"Nonfinite original partial shading water plane");
	impl->Pipelines(lease,validation,shaderMs);lease.Retain(impl->library);lease.Retain(impl->shadePipeline);
	const auto height=PrepareHeight(lease,source->heights);
	auto result=std::shared_ptr<PreparedMiniMap>(new PreparedMiniMap());
	result->impl->height=height->texture;result->impl->shade=destination;
	// Reuse the existing deferred-plan resource registration on later leases;
	// this plan has no independent shading cache or base/info producer.
	result->impl->infoAllocations=[NSMutableArray new];
	[result->impl->infoAllocations addObject:height->texture];
	[result->impl->infoAllocations addObject:destination];lease.AddAllocation(destination);
	if(rect[0]!=rect[2]&&rect[1]!=rect[3]){
		const auto& l=source->lighting;
		ShadeUniform uniform{Vec(l.groundAmbient),Vec(l.groundDiffuse),Vec(l.lightDirection),Vec(l.waterBase),Vec(l.waterAbsorb),Vec(l.waterMinimum),
			{l.waterLevel,0,0,0},{uint32_t(rect[0]),uint32_t(rect[1]),uint32_t(rect[2]),uint32_t(rect[3])}};
		const auto buffer=Upload(lease,&uniform,sizeof(uniform));
		[result->impl->infoAllocations addObject:buffer];
		auto args=Arguments(lease,1,2);[args setAddress:buffer.gpuAddress atIndex:0];
		[args setTexture:height->texture.gpuResourceID atIndex:0];[args setTexture:destination.gpuResourceID atIndex:1];
		result->impl->before.push_back({impl->shadePipeline,args,destination,false,uint32_t(rect[2]-rect[0]),uint32_t(rect[3]-rect[1])});
	}
	return result;
}

std::shared_ptr<PreparedMiniMap> MiniMapGPU::PrepareInfoImage(RuntimeLease& lease,
	std::shared_ptr<const NativeMiniMapInfo> source,std::shared_ptr<const NativeMiniMapCoverage> radarGround,bool radarGroundGlobal,
	id<MTLTexture> target,uint32_t squaresX,uint32_t squaresZ,bool clear,bool validation,double& shaderMs)
{
	Require(target!=nil&&target.pixelFormat==MTLPixelFormatRGB10A2Unorm&&squaresX&&squaresZ&&target.width>=squaresX&&target.height>=squaresZ,"Original global info requires actual padded RGB10A2 storage");
	impl->Pipelines(lease,validation,shaderMs);lease.Retain(impl->library);lease.Retain(impl->coveragePipeline);lease.Retain(impl->radarPipeline);lease.Retain(impl->gameInfoPipeline);lease.AddAllocation(target);
	auto result=std::shared_ptr<PreparedMiniMap>(new PreparedMiniMap);result->impl->info=target;result->impl->infoAllocations=[NSMutableArray array];[result->impl->infoAllocations addObject:target];
	const auto keep=[&](id<MTLAllocation> allocation){[result->impl->infoAllocations addObject:allocation];};
	auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=clear?MTLLoadActionClear:MTLLoadActionLoad;
	pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(.5,.5,.5,1);pass.renderTargetWidth=target.width;pass.renderTargetHeight=target.height;
	result->impl->infoPass=pass;result->impl->infoWidth=squaresX;result->impl->infoHeight=squaresZ;
	if(!source){Require(clear,"Original global info empty source must be an actual clear");return result;}
	Require(source->mode==NativeMiniMapInfoMode::Los,"Original global info source requires the actual LOS shader");const auto& info=*source;
	const auto coverage=[&](const std::shared_ptr<const NativeMiniMapCoverage>& data){
		Require(data&&data->width&&data->height&&data->counts.size()==size_t(data->width)*data->height,"Original global info coverage bounds mismatch");
		auto raw=Texture(lease,MTLPixelFormatR16Uint,data->width,data->height,false,MTLTextureUsageShaderRead,MTLStorageModeShared);
		[raw replaceRegion:MTLRegionMake2D(0,0,data->width,data->height) mipmapLevel:0 withBytes:data->counts.data() bytesPerRow:size_t(data->width)*2];keep(raw);return raw;
	};
	std::array<uint32_t,4> options{uint32_t(info.globalLos),0,0,0};auto optionBuffer=Upload(lease,options.data(),sizeof(options));keep(optionBuffer);
	const auto visibility=[&](const auto& data,bool global){auto raw=coverage(data);auto output=Texture(lease,MTLPixelFormatR8Unorm,data->width,data->height,true,MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite,MTLStorageModePrivate);auto a=Arguments(lease,1,2);auto opts=optionBuffer;if(global!=info.globalLos){std::array<uint32_t,4> alternate{uint32_t(global),0,0,0};opts=Upload(lease,alternate.data(),sizeof(alternate));keep(opts);}[a setAddress:opts.gpuAddress atIndex:0];[a setTexture:raw.gpuResourceID atIndex:0];[a setTexture:output.gpuResourceID atIndex:1];keep(output);result->impl->before.push_back({impl->coveragePipeline,a,output,true});return output;};
	auto los=visibility(info.groundLos,info.globalLos),air=visibility(info.airLos,info.globalLos);
	auto radarLos=radarGround==info.groundLos&&radarGroundGlobal==info.globalLos?los:visibility(radarGround,radarGroundGlobal);
	auto radar=coverage(info.radar),jammer=coverage(info.jammer);
	Require(info.radar->width==info.jammer->width&&info.radar->height==info.jammer->height,"Original global info radar/jammer extents differ");
	auto radarOutput=Texture(lease,MTLPixelFormatRG8Unorm,info.radar->width,info.radar->height,true,MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite,MTLStorageModePrivate);
	keep(radarOutput);auto a=Arguments(lease,1,4);[a setAddress:optionBuffer.gpuAddress atIndex:0];[a setTexture:radar.gpuResourceID atIndex:0];[a setTexture:jammer.gpuResourceID atIndex:1];[a setTexture:radarLos.gpuResourceID atIndex:2];[a setTexture:radarOutput.gpuResourceID atIndex:3];result->impl->before.push_back({impl->radarPipeline,a,radarOutput,true});
	InfoUniform u{Vec(info.alwaysColor),Vec(info.losColor),Vec(info.radarColor),Vec(info.jammerColor),Vec(info.radarColor2),{info.time,float(info.highQuality),0,0}};
	auto uniform=Upload(lease,&u,sizeof(u));keep(uniform);auto table=Arguments(lease,1,3);[table setAddress:uniform.gpuAddress atIndex:0];[table setTexture:los.gpuResourceID atIndex:0];[table setTexture:air.gpuResourceID atIndex:1];[table setTexture:radarOutput.gpuResourceID atIndex:2];
	result->impl->infoArgs=table;result->impl->infoPipeline=impl->gameInfoPipeline;result->impl->groundLos=los;result->impl->radar=radarOutput;return result;
}

}
