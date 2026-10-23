/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4ModelDepthNative.h"
#include "Metal4WorldShaderSource.h"
#include "Metal4WorldViewport.h"
#import <simd/simd.h>
#include <atomic>
#include <cstring>
#include <map>
namespace rendering::metal4 {
namespace {
// Appended to the exact current world source so WorldUniform/WorldVarying
// cannot silently drift. Only the original depth-writing alpha predicates
// are evaluated; color attachments and lighting do not affect GBuffer depth.
constexpr const char* DEPTH_SHADER=R"MSL(
fragment void OriginalModelGBufferDepth(WorldVarying in [[stage_in]],constant WorldUniform& u [[buffer(1)]],constant uint4& alpha [[buffer(2)]],texture2d<float> other [[texture(1)]]) {
 // CUS non-HASALPHASHADOWS writes depth even alphaBin0. Legacy deferred
 // ModelFragProg writes normal attachment0 alpha=1; its fixed alpha test
 // therefore passes even tex2.a=0 (full original 5-MRT selected6085 proof).
 if(alpha.x==0u||alpha.x==3u)return;
 constexpr sampler image(coord::normalized,address::repeat,filter::linear,mip_filter::linear);
 float2 uv=float2(in.uv.x,(u.options.z&1u)?1-in.uv.y:in.uv.y);
 float a;
 if(alpha.x==1u){
  a=(u.options.z&2u)?other.sample(image,uv,level(0)).a:other.sample(image,uv,bias(-.5f)).a;
  if(a<.5f)discard_fragment(); // CUS deferred HASALPHASHADOWS, equality survives.
 }else if(alpha.x==2u){
  a=SourceTextureSample(other,image,uv,bool(u.options.z&2u)).a;
  if(u.team.a*float(a>=.5f)<=u.parameters.z)discard_fragment();
 }
}
)MSL";
struct DepthUniform {simd_float4x4 viewProjection,world,piece;simd_float4 team;simd_uint4 terrain,options;simd_float4 parameters;};
static_assert(sizeof(DepthUniform)==256&&sizeof(WorldModelVertex)==76);
struct Target {__strong id<MTLTexture> texture=nil;};
struct Epoch {std::shared_ptr<Target> target;};
void ValidateDraw(const ModelDepthDraw& draw,const WorldCamera& camera,RuntimeLease& lease)
{
	Require(draw.opaque&&!draw.projectile,"Original model GBuffer excludes alpha/fading objects and opaque projectiles");
	Require(draw.vertices!=nil&&draw.indices!=nil&&draw.uniforms!=nil&&draw.scene!=nil&&draw.other!=nil,"Original model GBuffer needs actual shared model geometry/uniform/other-texture bindings");
	Require(draw.indexCount>0&&draw.indexCount%3==0&&draw.indexCount<=draw.indices.length/sizeof(std::uint32_t),"Original model GBuffer index range differs");
	Require(draw.vertices.length%sizeof(WorldModelVertex)==0,"Original model GBuffer vertex ABI differs");
	Require(draw.uniformOffset<=draw.uniforms.length&&sizeof(DepthUniform)<=draw.uniforms.length-draw.uniformOffset&&draw.scene.length>=384,"Original model GBuffer uniform span differs");
	Require(draw.uniforms.contents!=nullptr,"Original model GBuffer must validate its current shared WorldUniform camera");
	DepthUniform uniform;std::memcpy(&uniform,static_cast<const char*>(draw.uniforms.contents)+draw.uniformOffset,sizeof(uniform));
	simd_float4x4 view,projection;std::memcpy(&view,camera.view.data(),64);std::memcpy(&projection,camera.projection.data(),64);const auto expected=simd_mul(projection,view);
	Require(std::memcmp(&uniform.viewProjection,&expected,64)==0&&uniform.options.x==unsigned(camera.clipDepth==ClipDepthConvention::MinusOneToOne),"Original model GBuffer cannot use shadow/reflection/stale-camera uniforms");
	Require(unsigned(draw.alpha)<=unsigned(ModelDepthAlpha::EngineLegacy),"Original model GBuffer alpha family is invalid");
	Require(draw.other.device.registryID==lease.Device().registryID,"Original model GBuffer texture belongs to another device");
}
}
struct PreparedModelDepth::Impl {
	struct Draw {ModelDepthDraw source;__strong id<MTL4ArgumentTable> arguments=nil;};
	ShieldDepthImage image;std::vector<Draw> draws;std::vector<ModelDepthPhase> phases;
	std::shared_ptr<ShieldDepthProducerState> producer;
	__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;
	bool requiresEncode=true,encoded=false,completed=false;
};
PreparedModelDepth::PreparedModelDepth():impl(std::make_unique<Impl>()){} PreparedModelDepth::~PreparedModelDepth()=default;
const ShieldDepthImage& PreparedModelDepth::Image()const{return impl->image;}
std::size_t PreparedModelDepth::PieceCount()const{return impl->draws.size();}
bool PreparedModelDepth::RequiresEncode()const{return impl->requiresEncode;}
void PreparedModelDepth::Encode(id<MTL4CommandBuffer> command)
{
	Require(impl->requiresEncode&&!impl->encoded&&!impl->completed,"Original model GBuffer has no new pass or was already encoded");
	id<MTL4RenderCommandEncoder> encoder=nil;
	for(const auto& phase:impl->phases){
		if(encoder==nil||phase.clearBefore){
			if(encoder!=nil){[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];}
			auto pass=[MTL4RenderPassDescriptor new];pass.depthAttachment.texture=impl->image.texture;pass.depthAttachment.loadAction=phase.clearBefore?MTLLoadActionClear:MTLLoadActionLoad;pass.depthAttachment.storeAction=MTLStoreActionStore;pass.depthAttachment.clearDepth=1;pass.renderTargetWidth=impl->image.texture.width;pass.renderTargetHeight=impl->image.texture.height;
			encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"Cannot encode original model GBuffer source phase");
			[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
			[encoder setViewport:MTLViewport{0,0,double(pass.renderTargetWidth),double(pass.renderTargetHeight),0,1}];[encoder setScissorRect:MTLScissorRect{0,0,pass.renderTargetWidth,pass.renderTargetHeight}];
			[encoder setDepthClipMode:MTLDepthClipModeClip];[encoder setCullMode:MTLCullModeBack];[encoder setFrontFacingWinding:MTLWindingCounterClockwise];[encoder setTriangleFillMode:MTLTriangleFillModeFill];[encoder setDepthBias:0 slopeScale:0 clamp:0];
			[encoder setDepthStencilState:impl->depth];[encoder setRenderPipelineState:impl->pipeline];
		}
		for(std::size_t i=phase.firstDraw;i<phase.firstDraw+phase.drawCount;++i){const auto&draw=impl->draws[i];[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.source.indexCount indexType:MTLIndexTypeUInt32 indexBuffer:draw.source.indices.gpuAddress indexBufferLength:draw.source.indices.length];}
	}
	[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
	[encoder endEncoding];impl->encoded=true;impl->producer->encoded=true;
}
void PreparedModelDepth::DidComplete(){Require((!impl->requiresEncode||impl->encoded)&&!impl->completed,"Original model GBuffer cannot retire without its real producer");impl->completed=true;if(impl->producer)impl->producer->completed=true;}
struct ModelDepthGPU::Impl {
	__strong id<MTLLibrary> fragments=nil;
	__strong id<MTLLibrary> world=nil;
	__strong id<MTLRenderPipelineState> pipeline=nil;
	__strong id<MTLDepthStencilState> depth=nil;
	bool validation=false;
	std::shared_ptr<Target> cache;std::weak_ptr<const void> outstanding;
};
ModelDepthGPU::ModelDepthGPU():impl(std::make_unique<Impl>()){} ModelDepthGPU::~ModelDepthGPU()=default;
void ModelDepthGPU::ClearResources(){impl->cache.reset();impl->outstanding.reset();}
void ModelDepthGPU::Initialize(RuntimeLease& lease,id<MTLLibrary> world,bool validation,double& milliseconds)
{
	Require(world!=nil,"Original model GBuffer actual world shader is unavailable");
	if(impl->pipeline==nil||impl->world!=world||impl->validation!=validation){
		const auto start=Clock::now();auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Actual WorldShader geometry with original deferred alpha depth predicates";descriptor.source=String(std::string(WORLD_SHADER)+DEPTH_SHADER);auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;
		NSError* error=nil;auto library=lease.NewLibrary(descriptor,&error);Require(library!=nil,"Cannot compile original model GBuffer depth predicates: "+Error(error));
		auto vertex=[MTL4LibraryFunctionDescriptor new];vertex.library=world;vertex.name=@"UnitVertexMain";auto fragment=[MTL4LibraryFunctionDescriptor new];fragment.library=library;fragment.name=@"OriginalModelGBufferDepth";
		auto pipelineDescriptor=[MTL4RenderPipelineDescriptor new];pipelineDescriptor.vertexFunctionDescriptor=vertex;pipelineDescriptor.fragmentFunctionDescriptor=fragment;pipelineDescriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;auto pipelineOptions=[MTL4PipelineOptions new];pipelineOptions.shaderValidation=ShaderValidationMode(validation);pipelineDescriptor.options=pipelineOptions;
		auto pipeline=lease.NewRenderPipeline(pipelineDescriptor,&error);Require(pipeline!=nil,"Cannot compile genuine models-only depth PSO: "+Error(error));
		auto depthDescriptor=[MTLDepthStencilDescriptor new];depthDescriptor.depthCompareFunction=MTLCompareFunctionLessEqual;depthDescriptor.depthWriteEnabled=YES;auto depth=[lease.Device()newDepthStencilStateWithDescriptor:depthDescriptor];Require(depth!=nil,"Cannot create original model GBuffer LEQUAL depth writes");
		impl->world=world;impl->fragments=library;impl->pipeline=pipeline;impl->depth=depth;impl->validation=validation;milliseconds+=Milliseconds(start);
	}
}
std::unique_ptr<PreparedModelDepth> ModelDepthGPU::Prepare(RuntimeLease& lease,std::uint64_t frame,const WorldCamera& camera,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,std::span<const ModelDepthDraw> draws,id<MTLLibrary> world,bool validation,double& milliseconds,const ModelDepthSource* actual,std::uint64_t submission)
{
	Require(world!=nil&&camera.clipDepth!=ClipDepthConvention::Unsupported,"Original model GBuffer needs the actual main-camera world vertex program");
	// Original GBuffer storage is independent of the main drawable scissor.
	// Render full source NDC into its actual allocation; never truncate a
	// fractional main backing viewport into a different source resolution.
	Require(camera.viewport[2]>0&&camera.viewport[3]>0,"Original model GBuffer camera viewport is empty");
	const auto phases=actual?actual->phases:std::span<const ModelDepthPhase>{};
	const bool raster=!actual||!phases.empty();
	const auto w=actual?NSUInteger(actual->storageWidth):NSUInteger(camera.viewport[2]),h=actual?NSUInteger(actual->storageHeight):NSUInteger(camera.viewport[3]);
	if(actual&&!actual->namedDepthStorageExists){Require(!raster&&draws.empty()&&!actual->prior,"Original model deferred pass has no actual named storage");return{};}
	if(!w||!h){Require(!raster&&draws.empty(),"Original zero-size model GBuffer cannot render/clear a valid framebuffer");return{};}
	Require(w<=lease.Capabilities().maxTextureDimension2D&&h<=lease.Capabilities().maxTextureDimension2D,"Original model GBuffer extent exceeds actual device limit");
	if(!raster){
		Require(draws.empty(),"Original disabled model deferred phases cannot consume geometry");
		if(actual->prior){
			const auto& prior=*actual->prior;Require(prior.role==ShieldDepthRole::ModelsOnly&&prior.texture&&prior.storageOwner&&prior.producer&&prior.texture.width==w&&prior.texture.height==h,"Original retained model GBuffer differs from the actual storage lifetime");
			auto prepared=std::unique_ptr<PreparedModelDepth>(new PreparedModelDepth);prepared->impl->image=prior;prepared->impl->requiresEncode=false;lease.AddAllocation(prior.texture);return prepared;
		}
	}
	for(const auto& draw:draws)ValidateDraw(draw,camera,lease);
	if(!phases.empty()){
		Require(phases.front().clearBefore,"Original first selected model deferred phase must clear one");
		Require(phases.size()<=2,"Original model deferred producer has more than its unit/feature phases");
		for(const auto& phase:phases)Require(phase.feature.has_value(),"Original selected model phase must identify unit or feature ownership");
		if(phases.size()==2)Require(!*phases[0].feature&&*phases[1].feature,"Original model deferred source orders unit before feature");
		std::size_t next=0;for(const auto&phase:phases){Require(phase.firstDraw==next&&phase.drawCount<=draws.size()-next,"Original model deferred source phase range differs");if(phase.feature)for(std::size_t i=next;i<next+phase.drawCount;++i)Require(draws[i].feature==*phase.feature,"Original unit/feature deferred phase order differs");next+=phase.drawCount;}
		Require(next==draws.size(),"Original model deferred phase leaves actual pieces unconsumed");
	}
	if(raster)Initialize(lease,world,validation,milliseconds);
	auto target=impl->cache;
	if(!target||!impl->outstanding.expired()||target->texture.width!=w||target->texture.height!=h){auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:w height:h mipmapped:NO];descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;auto texture=[lease.Device()newTextureWithDescriptor:descriptor];Require(texture!=nil,"Cannot allocate actual models-only D32F GBuffer");target=std::make_shared<Target>();target->texture=texture;}
	auto prepared=std::unique_ptr<PreparedModelDepth>(new PreparedModelDepth);prepared->impl->requiresEncode=raster;if(!actual)prepared->impl->phases.push_back({{},0,draws.size(),true});else prepared->impl->phases.assign(phases.begin(),phases.end());prepared->impl->pipeline=impl->pipeline;prepared->impl->depth=impl->depth;
	auto epoch=std::make_shared<Epoch>();epoch->target=target;prepared->impl->producer=CreateShieldDepthProducer(ShieldDepthRole::ModelsOnly,frame);
	const auto identity=prepared->impl->producer->identity;
	prepared->impl->producer->submissionIdentity=submission;
	prepared->impl->producer->allocationVerified=true;if(!raster)prepared->impl->producer->operation=ShieldDepthOperation::AllocationOnly;
	prepared->impl->image={target->texture,ShieldDepthRole::ModelsOnly,frame,identity,camera.view,camera.projection,camera.viewport,epoch,prepared->impl->producer};
	lease.AddAllocation(target->texture);if(raster){lease.Retain(impl->world);lease.Retain(impl->fragments);lease.Retain(impl->pipeline);lease.Retain(impl->depth);}
	for(const auto& draw:draws){
		simd_uint4 alpha=simd_make_uint4(unsigned(draw.alpha),0,0,0);auto constants=[lease.Device()newBufferWithBytes:&alpha length:16 options:MTLResourceStorageModeShared];Require(constants!=nil,"Cannot allocate original model GBuffer alpha selector");lease.AddAllocation(constants);
		auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=4;descriptor.maxTextureBindCount=2;descriptor.initializeBindings=YES;NSError* error=nil;auto arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(arguments!=nil,"Cannot allocate original model GBuffer shared arguments: "+Error(error));lease.Retain(arguments);
		for(auto buffer:{draw.vertices,draw.indices,draw.uniforms,draw.scene})lease.AddAllocation(buffer);lease.AddAllocation(draw.other);
		[arguments setAddress:draw.vertices.gpuAddress atIndex:0];[arguments setAddress:draw.uniforms.gpuAddress+draw.uniformOffset atIndex:1];[arguments setAddress:constants.gpuAddress atIndex:2];[arguments setAddress:draw.scene.gpuAddress atIndex:3];[arguments setTexture:draw.other.gpuResourceID atIndex:1];prepared->impl->draws.push_back({draw,arguments});
	}
	impl->cache=target;impl->outstanding=epoch;return prepared;
}
}
