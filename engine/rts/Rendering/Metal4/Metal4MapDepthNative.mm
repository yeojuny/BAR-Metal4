/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4MapDepthNative.h"
#include "Metal4WorldViewport.h"
#import <simd/simd.h>
#include <cstring>
namespace rendering::metal4 {
namespace {
struct Uniform {simd_float4x4 viewProjection,world,piece;simd_float4 team;simd_uint4 terrain,options;simd_float4 parameters;};
static_assert(sizeof(Uniform)==256);
struct Target {__strong id<MTLTexture> texture=nil;};
struct Epoch {std::shared_ptr<Target> target;};
void ValidateDraw(const MapDepthDraw& d,const WorldCamera& camera)
{
	Require(d.heights&&d.tiles&&d.uniforms&&d.scene,"Original map GBuffer requires actual shared terrain buffers");
	Require(d.uniforms.contents&&d.uniformOffset<=d.uniforms.length&&sizeof(Uniform)<=d.uniforms.length-d.uniformOffset&&d.scene.length>=384,"Original map GBuffer uniform span differs");
	Require(d.tileCount>0&&d.tileCount<=d.tiles.length/16,"Original map GBuffer tile instance span differs");
	Uniform u;std::memcpy(&u,static_cast<const char*>(d.uniforms.contents)+d.uniformOffset,sizeof(u));
	simd_float4x4 view,projection;std::memcpy(&view,camera.view.data(),64);std::memcpy(&projection,camera.projection.data(),64);auto expected=simd_mul(projection,view);
	Require(std::memcmp(&u.viewProjection,&expected,64)==0&&u.options.x==unsigned(camera.clipDepth==ClipDepthConvention::MinusOneToOne),"Original map GBuffer needs current main-camera terrain uniforms");
	Require(u.terrain.x&&u.terrain.y&&std::uint64_t(u.terrain.x)*u.terrain.y<=d.heights.length/sizeof(float),"Original map GBuffer height extent differs");
}
}
struct PreparedMapDepth::Impl {
	ShieldDepthImage image;std::shared_ptr<ShieldDepthProducerState> producer;
	NativeMapDepthAction action=NativeMapDepthAction::Retain;
	std::vector<MapDepthDraw> draws;std::vector<id<MTL4ArgumentTable>> arguments;
	__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;
	bool requiresEncode=false,encoded=false,completed=false;
};
PreparedMapDepth::PreparedMapDepth():impl(std::make_unique<Impl>()){} PreparedMapDepth::~PreparedMapDepth()=default;
const ShieldDepthImage& PreparedMapDepth::Image()const{return impl->image;}
bool PreparedMapDepth::RequiresEncode()const{return impl->requiresEncode;}
NativeMapDepthAction PreparedMapDepth::Action()const{return impl->action;}
void PreparedMapDepth::Encode(id<MTL4CommandBuffer> command)
{
	Require(impl->requiresEncode&&!impl->encoded&&!impl->completed,"Original map GBuffer has no new pass or was already encoded");
	auto pass=[MTL4RenderPassDescriptor new];pass.depthAttachment.texture=impl->image.texture;pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.storeAction=MTLStoreActionStore;pass.depthAttachment.clearDepth=1;pass.renderTargetWidth=impl->image.texture.width;pass.renderTargetHeight=impl->image.texture.height;
	auto e=[command renderCommandEncoderWithDescriptor:pass];Require(e!=nil,"Cannot encode original map GBuffer draw/clear-one pass");
	if(impl->action==NativeMapDepthAction::DrawTerrain){
		[e barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageVertex visibilityOptions:MTL4VisibilityOptionDevice];
		[e setViewport:MTLViewport{0,0,double(pass.renderTargetWidth),double(pass.renderTargetHeight),0,1}];[e setScissorRect:MTLScissorRect{0,0,pass.renderTargetWidth,pass.renderTargetHeight}];
		[e setDepthClipMode:MTLDepthClipModeClip];[e setCullMode:MTLCullModeBack];[e setFrontFacingWinding:MTLWindingCounterClockwise];[e setTriangleFillMode:MTLTriangleFillModeFill];[e setDepthBias:0 slopeScale:0 clamp:0];
		[e setDepthStencilState:impl->depth];[e setRenderPipelineState:impl->pipeline];
		// Exact current main terrain mesh. Original deferred normal attachment0
		// alpha=1: forward void-ground/void-water alpha cutout is not reused.
		for(size_t i=0;i<impl->draws.size();++i){[e setArgumentTable:impl->arguments[i] atStages:MTLRenderStageVertex|MTLRenderStageFragment];[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:16*6 instanceCount:impl->draws[i].tileCount];}
	}
	[e barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];impl->encoded=true;impl->producer->encoded=true;
}
void PreparedMapDepth::DidComplete()
{
	Require(!impl->completed&&(!impl->requiresEncode||impl->encoded),"Original map GBuffer cannot retire an unencoded draw/clear");
	impl->completed=true;if(impl->producer)impl->producer->completed=true;
}
struct MapDepthGPU::Impl {
	__strong id<MTLLibrary> library=nil;__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;
	bool validation=false;std::shared_ptr<Target> cache;std::weak_ptr<const void> outstanding;
};
MapDepthGPU::MapDepthGPU():impl(std::make_unique<Impl>()){} MapDepthGPU::~MapDepthGPU()=default;
void MapDepthGPU::ClearResources(){impl->cache.reset();impl->outstanding.reset();}
void MapDepthGPU::Initialize(RuntimeLease&lease,id<MTLLibrary> library,bool validation,double&ms)
{
	Require(library!=nil,"Original map GBuffer needs actual current terrain shader");
	if(impl->pipeline!=nil&&impl->library==library&&impl->validation==validation)return;
	const auto start=Clock::now();auto vertex=[MTL4LibraryFunctionDescriptor new];vertex.library=library;vertex.name=@"TerrainVertexMain";auto fragment=[MTL4LibraryFunctionDescriptor new];fragment.library=library;fragment.name=@"ShadowTerrainFragment";
	auto descriptor=[MTL4RenderPipelineDescriptor new];descriptor.vertexFunctionDescriptor=vertex;descriptor.fragmentFunctionDescriptor=fragment;descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;
	NSError*error=nil;auto pipeline=lease.NewRenderPipeline(descriptor,&error);Require(pipeline!=nil,"Cannot compile genuine terrain-only GBuffer depth PSO: "+Error(error));auto d=[MTLDepthStencilDescriptor new];d.depthCompareFunction=MTLCompareFunctionLessEqual;d.depthWriteEnabled=YES;auto depth=[lease.Device()newDepthStencilStateWithDescriptor:d];Require(depth!=nil,"Cannot create original map GBuffer depth writes");
	impl->library=library;impl->pipeline=pipeline;impl->depth=depth;impl->validation=validation;ms+=Milliseconds(start);
}
std::unique_ptr<PreparedMapDepth> MapDepthGPU::Prepare(RuntimeLease&lease,std::uint64_t frame,const WorldCamera&camera,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,const NativeMapDepthSelection&selection,const MapDepthDraw*draw,const ShieldDepthImage*prior,id<MTLLibrary>library,bool validation,double&ms,std::uint64_t submission)
{
	return PrepareBatches(lease,frame,camera,width,height,logicalWidth,logicalHeight,selection,draw?std::span<const MapDepthDraw>(draw,1):std::span<const MapDepthDraw>{},prior,library,validation,ms,submission);
}
std::unique_ptr<PreparedMapDepth> MapDepthGPU::PrepareBatches(RuntimeLease&lease,std::uint64_t frame,const WorldCamera&camera,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,const NativeMapDepthSelection&selection,std::span<const MapDepthDraw> draws,const ShieldDepthImage*prior,id<MTLLibrary>library,bool validation,double&ms,std::uint64_t submission)
{
	Require(camera.clipDepth!=ClipDepthConvention::Unsupported,"Original map GBuffer camera clip convention differs");
	const auto action=SelectNativeMapDepthAction(selection);
	if(action==NativeMapDepthAction::Retain&&!prior&&!selection.namedDepthStorageExists)return{};
	Require(selection.namedDepthStorageExists,"Original map GBuffer has no observed named depth storage creation");
	// The named GBuffer is rasterized over its own full allocation, as in
	// GeometryBuffer::Bind. Main drawable clipping and fractional backing
	// scale never crop this source. Consumers use projected normalized UV.
	Require(camera.viewport[2]>0&&camera.viewport[3]>0,"Original map GBuffer camera viewport is empty");
	if(!selection.storageWidth||!selection.storageHeight){Require(action==NativeMapDepthAction::Retain&&!selection.geometryBufferValid,"Original zero-size map GBuffer cannot render/clear a valid framebuffer");return{};}
	const auto w=NSUInteger(selection.storageWidth),h=NSUInteger(selection.storageHeight);Require(w<=lease.Capabilities().maxTextureDimension2D&&h<=lease.Capabilities().maxTextureDimension2D,"Original map GBuffer extent exceeds actual device limit");auto p=std::unique_ptr<PreparedMapDepth>(new PreparedMapDepth);p->impl->action=action;
	if(action==NativeMapDepthAction::Retain&&prior){
		Require(prior->role==ShieldDepthRole::TerrainOnly&&prior->texture&&prior->storageOwner&&prior->producer,"Original map GBuffer retained version has no actual producer/allocation owner");
		Require(prior->texture.width==w&&prior->texture.height==h,"Original GBuffer resize must delete/recreate logical objects before retaining storage");
		p->impl->image=*prior;lease.AddAllocation(prior->texture);return p;
	}
	if(action==NativeMapDepthAction::DrawTerrain){Require(!draws.empty(),"Original map GBuffer draw has no actual terrain inputs");for(const auto& draw:draws)ValidateDraw(draw,camera);Initialize(lease,library,validation,ms);}
	auto target=impl->cache;if(!target||!impl->outstanding.expired()||target->texture.width!=w||target->texture.height!=h){auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:w height:h mipmapped:NO];d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;auto t=[lease.Device()newTextureWithDescriptor:d];Require(t!=nil,"Cannot allocate original map GBuffer D32F storage");target=std::make_shared<Target>();target->texture=t;}
	auto epoch=std::make_shared<Epoch>();epoch->target=target;p->impl->producer=CreateShieldDepthProducer(ShieldDepthRole::TerrainOnly,frame);p->impl->producer->operation=action==NativeMapDepthAction::Retain?ShieldDepthOperation::AllocationOnly:action==NativeMapDepthAction::ClearOne?ShieldDepthOperation::ClearOne:ShieldDepthOperation::Raster;
	p->impl->producer->submissionIdentity=submission;
	p->impl->producer->allocationVerified=true;p->impl->requiresEncode=action!=NativeMapDepthAction::Retain;
	p->impl->image={target->texture,ShieldDepthRole::TerrainOnly,frame,p->impl->producer->identity,camera.view,camera.projection,camera.viewport,epoch,p->impl->producer};lease.AddAllocation(target->texture);
	if(action==NativeMapDepthAction::DrawTerrain){p->impl->draws.assign(draws.begin(),draws.end());p->impl->pipeline=impl->pipeline;p->impl->depth=impl->depth;lease.Retain(impl->library);lease.Retain(impl->pipeline);lease.Retain(impl->depth);
		for(const auto& draw:draws){
		auto d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=4;d.initializeBindings=YES;NSError*error=nil;auto a=[lease.Device()newArgumentTableWithDescriptor:d error:&error];Require(a!=nil,"Cannot allocate original map GBuffer argument table: "+Error(error));
		for(auto b:{draw.heights,draw.tiles,draw.uniforms,draw.scene})lease.AddAllocation(b);[a setAddress:draw.heights.gpuAddress atIndex:0];[a setAddress:draw.uniforms.gpuAddress+draw.uniformOffset atIndex:1];[a setAddress:draw.tiles.gpuAddress atIndex:2];[a setAddress:draw.scene.gpuAddress atIndex:3];lease.Retain(a);p->impl->arguments.push_back(a);
		}
	}
	impl->cache=target;impl->outstanding=epoch;return p;
}
}
