/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4MapEdgeRenderer.h"
#include "NativeSampleShadingShaders.h"
#include <tuple>
#include "Metal4NativeEffectState.h"
#include "Rendering/Backend/NativeCUSDeferredData.h"
#import <simd/simd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace rendering::metal4 {
namespace {
const char* SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct EdgeUniform {
	float4x4 viewProjection,view;
	float4 mapSize,parameters,sun,fogColor,fogParameters;
	uint4 options;
	uint4 mesh;
	float4 reserved;
};
struct EdgeVarying {float4 position [[position]];float2 alphaFog [[RECOIL_SAMPLE_PERSPECTIVE]],uv [[RECOIL_SAMPLE_PERSPECTIVE]],normalSign [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex EdgeVarying MapEdgeVertex(uint vertexId [[vertex_id]],uint instanceId [[instance_id]],
	constant EdgeUniform& u [[buffer(0)]],device const float4* instances [[buffer(1)]],texture2d<float> heightMap [[texture(1)]]) {
	const float4 aMirrorParams=instances[instanceId];
	const float2 flip=aMirrorParams.xy,offset=aMirrorParams.zw;
	const int cols=int(flip.x>0.5f?u.mesh.z:u.mesh.x),rows=int(flip.y>0.5f?u.mesh.w:u.mesh.y);
	const int rev=int((flip.x>0.5f&&offset.x<0.0f)!=(flip.y>0.5f&&offset.y<0.0f));
	const int mapRev=int((flip.x>0.5f&&offset.x>0.0f)!=(flip.y>0.5f&&offset.y>0.0f));
	const bool alongZ=rev==mapRev;const int swap=alongZ?1-rev:rev;
	const int outer=alongZ?cols:rows,inner=alongZ?rows:cols,perLine=2*(inner+1)+2;
	const int line=int(vertexId)/perLine,k=int(vertexId)-line*perLine;
	int i=k>>1,o=line+((k&1)^swap);
	if(k==perLine-2){i=inner;o=line+(1^swap);}
	else if(k==perLine-1){i=0;o=line+1+swap;}
	if(line>=outer){i=0;o=outer+swap;}
	const int c=alongZ?o:i,r=alongZ?i:o;
	const float2 t=float2(float(c),float(r))*u.parameters.x;
	const bool2 mirrored=flip>float2(0.5f);
	const float2 edge=step(float2(0.0f),offset)*u.mapSize.xy,dir=sign(offset);
	const float2 mapPos=select(t,edge-dir*t,mirrored);
	const float2 worldXZ=select(t,edge+dir*t,mirrored); // Forward EDGE_TIGHTENING is exactly 0.
	const float2 adjusted=mapPos-float2(8)*(mapPos/u.mapSize.xy)+float2(4);
	const float2 heightUV=clamp(adjusted,float2(8),u.mapSize.xy-float2(8))/u.mapSize.xy;
	constexpr sampler heightSampler(coord::normalized,address::clamp_to_edge,filter::linear);
	float height=heightMap.sample(heightSampler,heightUV,level(0)).r;
	float alpha=1.0f;
	if(u.parameters.z==1.0f){
		const float2 dist=flip*t,bend=dist/150.0f; height-=dot(bend,bend);
		const float2 fade=dist/u.mapSize.xy;alpha=clamp(1.0f+6.0f*(0.18f-dot(fade,fade)),0.0f,1.0f);
	}
	const float4 worldPos=float4(worldXZ.x,height,worldXZ.y,1.0f);
	float fog=1.0f;
	if(u.parameters.w==1.0f){const float distance=length((u.view*worldPos).xyz);fog=clamp((u.fogParameters.y-distance)*u.fogParameters.w,0.0f,1.0f);}
	EdgeVarying output;output.alphaFog=float2(alpha,fog);output.uv=mapPos/u.mapSize.xy;
	output.normalSign=1.0f-2.0f*flip;output.position=u.viewProjection*worldPos;
	if(u.options.x==1)output.position.z=(output.position.z+output.position.w)*0.5f;
	return output;
}
fragment float4 MapEdgeFragment(EdgeVarying input [[stage_in]],constant EdgeUniform& u [[buffer(0)]],
	texture2d<float> color [[texture(0)]],texture2d<float> normals [[texture(2)]],
	sampler colorSampler [[sampler(0)]],sampler normalSampler [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	const float halfTexel=.5f/1024.f;
	float2 colorUV=clamp(input.uv,float2(halfTexel),float2(1-halfTexel));
	float2 normalUV=clamp(input.uv,float2(halfTexel/4),float2(1-halfTexel/4));
	if(u.options.y)colorUV.y=1-colorUV.y;
	if(u.options.z)normalUV.y=1-normalUV.y;
	const float3 source=color.sample(colorSampler,colorUV).rgb;
	const float3 raw=normals.sample(normalSampler,normalUV).rbg*2-1;
	if(input.alphaFog.x<0.05f)discard_fragment();
	const float luma=dot(source,float3(.2126f,.7152f,.0722f));
	float3 result=source+(clamp(luma*u.parameters.y,0.0f,1.0f)-luma);
	const float normalLength=length(raw);float3 normal=normalLength>.001f?raw/normalLength:float3(0,1,0);
	normal.xz*=input.normalSign;
	result=clamp(result*(dot(normal,u.sun.xyz)*.5f+1),float3(0),float3(1));
	return float4(mix(u.fogColor.rgb,result,input.alphaFog.y),input.alphaFog.x);
}
)MSL";
struct Uniform {
	simd_float4x4 viewProjection,view;
	simd_float4 mapSize,parameters,sun,fogColor,fogParameters;
	simd_uint4 options;
	simd_uint4 mesh;
	simd_float4 reserved;
};
static_assert(sizeof(Uniform)==256);
simd_float4x4 Matrix(const WorldMatrix& source){simd_float4x4 value;std::memcpy(&value,source.data(),sizeof(value));return value;}
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* bytes,size_t length)
{
	Require(length&&length<=lease.Device().maxBufferLength,"Map-edge GPU buffer exceeds device bounds");
	auto value=[lease.Device() newBufferWithBytes:bytes length:length options:MTLResourceStorageModeShared];Require(value!=nil,"Cannot allocate map-edge GPU buffer");lease.AddAllocation(value);return value;
}

}
struct PreparedMapEdge::Impl {
	__strong id<MTLRenderPipelineState> pipeline=nil;
	__strong id<MTLDepthStencilState> depth=nil;
	__strong id<MTL4ArgumentTable> arguments=nil;
	NSUInteger vertices=0,instances=0;
	bool cull=false;
	bool deferred=false;
	mutable bool encoded=false;
	NativeWorldDrawState raster;
	effectstate::Raster viewport;
	std::shared_ptr<const mapedge::DrawPacket> source;
	std::vector<CUSSourceImage> images;
};
PreparedMapEdge::PreparedMapEdge():impl(std::make_unique<Impl>()){}
PreparedMapEdge::~PreparedMapEdge()=default;
uint32_t PreparedMapEdge::MirrorInstances()const{return uint32_t(impl->instances);}
void PreparedMapEdge::EncodeDependencies(id<MTL4CommandBuffer> command)const
{
	for(const auto& image:impl->images)if(image.encodeDependencies)image.encodeDependencies(command);
}
void PreparedMapEdge::FinishDeferredDraw()const
{
	Require(!impl->deferred||impl->encoded,"Original deferred map-edge source command was not encoded at its ordinal");
}
void PreparedMapEdge::DidComplete()const
{
	FinishDeferredDraw();
	for(const auto& image:impl->images)if(image.didComplete)image.didComplete();
}
void PreparedMapEdge::Encode(id<MTL4RenderCommandEncoder> encoder)const
{
	Require(!impl->deferred||!impl->encoded,"Original deferred map-edge source command encoded twice");
	if(!impl->vertices||!impl->instances){if(impl->deferred)impl->encoded=true;return;}
	for(const auto& image:impl->images)if(image.requireReadable)image.requireReadable();
	if(impl->deferred)effectstate::RasterState(encoder,impl->raster,impl->viewport);
	[encoder setRenderPipelineState:impl->pipeline];[encoder setDepthStencilState:impl->depth];[encoder setDepthClipMode:MTLDepthClipModeClip];
	[encoder setCullMode:impl->cull?MTLCullModeBack:MTLCullModeNone];[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
	[encoder setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:impl->vertices instanceCount:impl->instances];
	if(impl->deferred)impl->encoded=true;
}
struct MapEdgeGPU::Impl {
	std::shared_ptr<const WorldTerrainHeights> heightOwner;
	__strong id<MTLTexture> height=nil;
	__strong id<MTLLibrary> library=nil;
	__strong id<MTLRenderPipelineState> pipeline=nil;
	MTLPixelFormat format=MTLPixelFormatInvalid;bool validation=false;
	using ForwardProgram=std::pair<id<MTLLibrary>,id<MTLRenderPipelineState>>;
	std::map<std::tuple<MTLPixelFormat,bool,unsigned>,ForwardProgram> forwardPrograms;
	__strong id<MTLLibrary> deferredLibrary=nil;
	std::map<std::tuple<uint64_t,bool,MTLPixelFormat>,id<MTLRenderPipelineState>> deferredPipelines;
};
MapEdgeGPU::MapEdgeGPU():impl(std::make_unique<Impl>()){}
MapEdgeGPU::~MapEdgeGPU()=default;
void MapEdgeGPU::ClearResources(){impl=std::make_unique<Impl>();}
std::unique_ptr<PreparedMapEdge> MapEdgeGPU::Prepare(RuntimeLease& lease,const WorldFrame& frame,MTLPixelFormat format,bool validation,double& shaderMilliseconds,
	const MapEdgeImageProvider& images,unsigned outputSamples)
{
	if(!frame.mapEdge||!frame.mapEdge->draw||frame.mapEdge->draw->mirrors.empty())return {};
	const auto& edge=*frame.mapEdge;const auto& terrain=frame.terrain;const auto& p=edge.draw->parameters;
	Require(edge.resources&&edge.resources->color&&edge.resources->normal&&terrain.heights,"Map-edge missing original owned image/height resources");
	mapedge::Validate(p,edge.draw->mirrors);
	Require(frame.viewCamera.clipDepth!=ClipDepthConvention::Unsupported,"Map-edge requires actual camera clip-depth convention");
	Require(terrain.squareSize==8&&terrain.squaresX&&terrain.squaresZ&&terrain.squaresX<=8192&&terrain.squaresZ<=8192&&terrain.squaresX%4==0&&terrain.squaresZ%4==0,"Map-edge original SMF/grid dimensions invalid");
	const auto& heights=*terrain.heights;
	Require(heights.width==terrain.squaresX+1&&heights.height==terrain.squaresZ+1&&heights.corners.size()==size_t(heights.width)*heights.height,"Map-edge height snapshot extent mismatch");
	const bool updateHeights=impl->heightOwner!=terrain.heights||impl->height==nil||impl->height.width!=heights.width||impl->height.height!=heights.height;
	if(updateHeights)for(float value:heights.corners)Require(std::isfinite(value),"Map-edge nonfinite actual unsynced height");
	Require(bool(images),"Map-edge requires its original shared image producer");
	auto color=images(lease,edge.resources->color),normal=images(lease,edge.resources->normal);
	Require(color.texture&&color.sampler&&color.owner&&normal.texture&&normal.sampler&&normal.owner,
		"Map-edge source image or original sampling state is missing");
	if(updateHeights){
		if(impl->height==nil||impl->height.width!=heights.width||impl->height.height!=heights.height){auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float width:heights.width height:heights.height mipmapped:NO];d.storageMode=MTLStorageModeShared;d.usage=MTLTextureUsageShaderRead;impl->height=[lease.Device() newTextureWithDescriptor:d];Require(impl->height!=nil,"Cannot allocate actual map-edge corner-height texture");}
		[impl->height replaceRegion:MTLRegionMake2D(0,0,heights.width,heights.height) mipmapLevel:0 withBytes:heights.corners.data() bytesPerRow:size_t(heights.width)*sizeof(float)];
		// Publication follows validation and upload; the retained immutable owner
		// also prevents allocator address reuse from accepting stale height bytes.
		impl->heightOwner=terrain.heights;
	}
	const auto key=std::make_tuple(format,validation,lease.SampleShadingKey(outputSamples));
	if(auto found=impl->forwardPrograms.find(key);found!=impl->forwardPrograms.end()) {
		std::tie(impl->library,impl->pipeline)=found->second;
		impl->format=format;impl->validation=validation;
	}else{const auto start=Clock::now();NSError* error=nil;
		auto d=[MTL4LibraryDescriptor new];d.source=String(SHADER);d.name=@"Original current31657 forward mirrored map-edge strip";
		auto library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile original map-edge MSL: "+Error(error));
		auto vertex=[MTL4LibraryFunctionDescriptor new];vertex.library=library;vertex.name=@"MapEdgeVertex";auto fragment=[MTL4LibraryFunctionDescriptor new];fragment.library=library;fragment.name=@"MapEdgeFragment";
		auto pipeline=[MTL4RenderPipelineDescriptor new];pipeline.rasterSampleCount=outputSamples;pipeline.vertexFunctionDescriptor=vertex;pipeline.fragmentFunctionDescriptor=fragment;pipeline.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
		auto attachment=pipeline.colorAttachments[0];attachment.pixelFormat=format;attachment.blendingState=MTL4BlendStateEnabled;attachment.sourceRGBBlendFactor=attachment.sourceAlphaBlendFactor=MTLBlendFactorSourceAlpha;attachment.destinationRGBBlendFactor=attachment.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);pipeline.options=options;
		auto state=lease.NewRenderPipeline(pipeline,&error);Require(state!=nil,"Cannot compile original map-edge Metal4 pipeline: "+Error(error));
		impl->library=library;impl->pipeline=state;impl->format=format;impl->validation=validation;impl->forwardPrograms.emplace(key,Impl::ForwardProgram{library,state});shaderMilliseconds+=Milliseconds(start);}
	Uniform u{};u.view=Matrix(frame.viewCamera.view);u.viewProjection=simd_mul(Matrix(frame.viewCamera.projection),u.view);
	u.mapSize=simd_make_float4(float(terrain.squaresX)*8,float(terrain.squaresZ)*8,0,0);u.parameters=simd_make_float4(p.gridSize,p.brightness,p.curvature?1.f:0.f,p.fog?1.f:0.f);
	std::memcpy(&u.sun,edge.sunDirection.data(),sizeof(u.sun));std::memcpy(&u.fogColor,edge.fogColor.data(),sizeof(u.fogColor));std::memcpy(&u.fogParameters,edge.fogParameters.data(),sizeof(u.fogParameters));
	const auto& mesh=p.mesh;
	const uint32_t cols=(terrain.squaresX*8+31)/32,rows=(terrain.squaresZ*8+31)/32;
	const double visibleFraction=std::sqrt(0.18+0.95/6.0);
	const uint32_t trimCols=p.curvature?std::min(cols,uint32_t(std::ceil(visibleFraction*cols))):cols;
	const uint32_t trimRows=p.curvature?std::min(rows,uint32_t(std::ceil(visibleFraction*rows))):rows;
	Require(mesh[0]==cols&&mesh[1]==rows&&mesh[2]==trimCols&&mesh[3]==trimRows,"Map-edge original mesh tuple differs from its actual map/settings");
	uint32_t count=0;
	for(const auto& mirror:edge.draw->mirrors){
		const uint32_t vertices=mirror[0]&&mirror[1]?trimCols*(2*(trimRows+1)+2):mirror[0]?rows*(2*(trimCols+1)+2):trimRows*(2*(cols+1)+2);
		count=std::max(count,vertices);
	}
	Require(mesh[4]==count,"Map-edge strip invocation count differs from actual source mirrors");
	u.mesh=simd_make_uint4(mesh[0],mesh[1],mesh[2],mesh[3]);
	u.options=simd_make_uint4(frame.viewCamera.clipDepth==ClipDepthConvention::MinusOneToOne?1:0,color.flipV,normal.flipV,0);
	for(const auto* values:{frame.viewCamera.view.data(),frame.viewCamera.projection.data()})for(size_t i=0;i<16;++i)Require(std::isfinite(values[i]),"Map-edge nonfinite camera matrix");
	for(const auto* values:{edge.sunDirection.data(),edge.fogColor.data(),edge.fogParameters.data()})for(size_t i=0;i<4;++i)Require(std::isfinite(values[i]),"Map-edge nonfinite actual sun/fog");
	for(const auto& column:u.viewProjection.columns)for(size_t i=0;i<4;++i)Require(std::isfinite(column[i]),"Map-edge camera matrix product overflow");
	auto uniforms=Buffer(lease,&u,sizeof(u));auto instances=Buffer(lease,edge.draw->mirrors.data(),edge.draw->mirrors.size()*sizeof(mapedge::Mirror));
	lease.AddAllocation(color.texture);lease.AddAllocation(normal.texture);lease.Retain(color.sampler);lease.Retain(normal.sampler);lease.AddAllocation(impl->height);lease.Retain(impl->library);lease.Retain(impl->pipeline);
	auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=2;descriptor.maxTextureBindCount=3;descriptor.maxSamplerStateBindCount=2;descriptor.initializeBindings=YES;NSError* error=nil;
	auto arguments=[lease.Device() newArgumentTableWithDescriptor:descriptor error:&error];Require(arguments!=nil,"Cannot allocate original map-edge arguments: "+Error(error));lease.Retain(arguments);
	[arguments setAddress:uniforms.gpuAddress atIndex:0];[arguments setAddress:instances.gpuAddress atIndex:1];[arguments setTexture:color.texture.gpuResourceID atIndex:0];[arguments setTexture:impl->height.gpuResourceID atIndex:1];[arguments setTexture:normal.texture.gpuResourceID atIndex:2];
	[arguments setSamplerState:color.sampler.gpuResourceID atIndex:0];[arguments setSamplerState:normal.sampler.gpuResourceID atIndex:1];
	auto depth=[MTLDepthStencilDescriptor new];depth.depthCompareFunction=MTLCompareFunctionLessEqual;depth.depthWriteEnabled=YES;
	auto depthState=[lease.Device() newDepthStencilStateWithDescriptor:depth];Require(depthState!=nil,"Cannot create map-edge original LEQUAL/write depth state");lease.Retain(depthState);
	auto result=std::unique_ptr<PreparedMapEdge>(new PreparedMapEdge);result->impl->pipeline=impl->pipeline;result->impl->depth=depthState;result->impl->arguments=arguments;
	result->impl->vertices=mesh[4];result->impl->instances=edge.draw->mirrors.size();result->impl->cull=p.cullBackFaces;
	result->impl->images={std::move(color),std::move(normal)};
	return result;
}
#include "Metal4MapEdgeDeferredNative.inc"
} // namespace rendering::metal4
