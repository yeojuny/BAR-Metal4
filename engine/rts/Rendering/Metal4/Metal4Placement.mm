/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4Placement.h"
#include "NativePlacementShaders.h"
#include "Rendering/Metal4/Metal4NativeEffectState.h"
#include <map>
namespace rendering::metal4 {
namespace {
struct Uniform {std::array<float,4> mouse,camera,alphaTest;};
static_assert(sizeof(Uniform)==48); // Three float4 fields, paired with PlacementGridUniform.
id<MTLBuffer> Buffer(RuntimeLease& lease,const void* bytes,size_t count){Require(count&&count<=lease.Device().maxBufferLength,"Original Building Grid GPU extent invalid");auto b=[lease.Device()newBufferWithBytes:bytes length:count options:MTLResourceStorageModeShared];Require(b!=nil,"Cannot allocate original Building Grid geometry");lease.AddAllocation(b);return b;}
MTLBlendOperation Equation(ScreenBlendEquation e){switch(e){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Unknown original Building Grid blend equation");}
}
struct PreparedPlacementGrid::Impl {
	struct Draw {uint64_t sequence;__strong id<MTLRenderPipelineState> pipeline;__strong id<MTLDepthStencilState> depth;__strong id<MTL4ArgumentTable> arguments;__strong id<MTLBuffer> indices;NSUInteger count;NativeWorldDrawState world;effectstate::Raster raster;};
	std::vector<Draw> draws;
};
PreparedPlacementGrid::PreparedPlacementGrid():impl(std::make_unique<Impl>()){}
PreparedPlacementGrid::~PreparedPlacementGrid()=default;
size_t PreparedPlacementGrid::DrawCount()const{return impl->draws.size();}
uint64_t PreparedPlacementGrid::SequenceAt(size_t i)const{return impl->draws.at(i).sequence;}
void PreparedPlacementGrid::EncodeDraw(size_t i,id<MTL4RenderCommandEncoder> encoder)const{
	const auto& d=impl->draws.at(i);[encoder setRenderPipelineState:d.pipeline];[encoder setDepthStencilState:d.depth];effectstate::RasterState(encoder,d.world,d.raster);[encoder setCullMode:MTLCullModeNone];[encoder setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:d.count indexType:MTLIndexTypeUInt32 indexBuffer:d.indices.gpuAddress indexBufferLength:d.indices.length];
}
struct PlacementGPU::Impl {
	__strong id<MTLLibrary> library=nil;MTLPixelFormat format=MTLPixelFormatInvalid;bool validation=false;std::map<uint64_t,id<MTLRenderPipelineState>> pipelines;
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const ScreenBlendState& blend,const std::array<bool,4>& mask,double& ms,unsigned samples,bool coverage){
		uint64_t key=(uint64_t(samples)<<32)|(uint64_t(lease.SampleShadingEnabled(samples))<<39)|(uint64_t(coverage)<<38)|uint64_t(blend.enabled)|uint64_t(blend.sourceColor)<<1|uint64_t(blend.destinationColor)<<5|uint64_t(blend.sourceAlpha)<<9|uint64_t(blend.destinationAlpha)<<13|uint64_t(mask[0])<<17|uint64_t(mask[1])<<18|uint64_t(mask[2])<<19|uint64_t(mask[3])<<20|uint64_t(blend.colorEquation)<<21|uint64_t(blend.alphaEquation)<<24;
		if(auto p=pipelines.find(key);p!=pipelines.end())return p->second;auto start=Clock::now();auto d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;auto v=[MTL4LibraryFunctionDescriptor new];v.library=library;v.name=@"PlacementGridVertex";auto f=[MTL4LibraryFunctionDescriptor new];f.library=library;f.name=@"PlacementGridFragment";d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
		auto a=d.colorAttachments[0];a.pixelFormat=format;a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
		NSError* error=nil;auto p=lease.NewRenderPipeline(d,&error);Require(p!=nil,"Cannot compile original Building Grid pipeline: "+Error(error));pipelines.emplace(key,p);ms+=Milliseconds(start);return p;
	}
};
PlacementGPU::PlacementGPU():impl(std::make_unique<Impl>()){}
PlacementGPU::~PlacementGPU(){if(impl->library!=nil)SetNativePlacementGridAvailable(false);}
void PlacementGPU::Initialize(RuntimeLease& lease,MTLPixelFormat format,bool validation,double& ms,unsigned samples){
	if(impl->library!=nil&&impl->format==format&&impl->validation==validation)return;SetNativePlacementGridAvailable(false);auto start=Clock::now();auto d=[MTL4LibraryDescriptor new];d.source=String(NativePlacementShaderSource);d.name=@"Original source-paired Building Grid";auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;d.options=options;NSError* error=nil;impl->library=lease.NewLibrary(d,&error);Require(impl->library!=nil,"Cannot compile original Building Grid Metal library: "+Error(error));impl->format=format;impl->validation=validation;impl->pipelines.clear();ms+=Milliseconds(start);impl->Pipeline(lease,ScreenBlendState{},{true,true,true,true},ms,samples,false);SetNativePlacementGridAvailable(true);
}
std::unique_ptr<PreparedPlacementGrid> PlacementGPU::Prepare(RuntimeLease& lease,const NativePlacementFrame& frame,const WorldCamera& camera,uint32_t width,uint32_t height,uint32_t logicalWidth,uint32_t logicalHeight,MTLPixelFormat format,MTLPixelFormat stencil,bool validation,double& ms,unsigned samples){
	Initialize(lease,format,validation,ms,samples);lease.Retain(impl->library);auto result=std::unique_ptr<PreparedPlacementGrid>(new PreparedPlacementGrid);
	for(const auto& source:frame.grids){
		ValidateNativeBuildingGrid(source);auto raster=effectstate::Viewport(camera,width,height,logicalWidth,logicalHeight,source.surface.scissor);if(!raster)continue;std::array<double,4> viewport{raster->viewport.originX,raster->viewport.originY,raster->viewport.width,raster->viewport.height};auto mesh=BuildNativeBuildingGrid(source,viewport,width,height,samples);if(mesh.indices.empty())continue;if(mesh.fullViewport)raster->viewport={0,0,double(width),double(height),0,1};
		auto vertices=Buffer(lease,mesh.vertices.data(),mesh.vertices.size()*sizeof(NativeWorldDrawVertex));auto indices=Buffer(lease,mesh.indices.data(),mesh.indices.size()*sizeof(uint32_t));Uniform u{{source.mousePosition[0],source.mousePosition[1],source.mousePosition[2],source.cameraFade},{source.view.cameraPosition[0],source.view.cameraPosition[1],source.view.cameraPosition[2],float(source.generatedPatch31657)},{float(source.surface.alphaTest.enabled),float(source.surface.alphaTest.compare),source.surface.alphaTest.reference,0}};auto uniform=Buffer(lease,&u,sizeof(u));auto desc=[MTL4ArgumentTableDescriptor new];desc.maxBufferBindCount=2;desc.initializeBindings=YES;NSError* error=nil;auto arguments=[lease.Device()newArgumentTableWithDescriptor:desc error:&error];Require(arguments!=nil,"Cannot allocate original Building Grid argument table: "+Error(error));[arguments setAddress:vertices.gpuAddress atIndex:0];[arguments setAddress:uniform.gpuAddress atIndex:1];lease.Retain(arguments);
		auto pipeline=impl->Pipeline(lease,source.surface.blend,source.world.colorWrite,ms,samples,source.world.alphaToCoverage);lease.Retain(pipeline);result->impl->draws.push_back({source.sequence,pipeline,effectstate::Depth(lease,source.world,stencil),arguments,indices,mesh.indices.size(),source.world,*raster});
	}return result;
}
}
