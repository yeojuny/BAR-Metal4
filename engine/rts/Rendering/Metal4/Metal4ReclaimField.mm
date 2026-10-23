/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4ReclaimField.h"
#include "NativeSampleShadingShaders.h"
#include <simd/simd.h>
#include <cstring>
#include <map>
namespace rendering::metal4 {
namespace {
const char* Source=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Uniform {float4x4 viewProj,viewInv;float4 fillMetal,fillEnergy,edgeMetal,edgeEnergy,togglesOffsetGradient,fadeEnergy;uint4 options;float4 alpha;};
struct Out {float4 position [[position]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex Out ReclaimFieldVertex(uint id [[vertex_id]],device const float4* vertices [[buffer(0)]],device const float4* params [[buffer(1)]],constant Uniform& u [[buffer(2)]]){
	uint chunk=id/u.options.x;float4 p0=params[2*chunk],p1=params[2*chunk+1];float4 posKind=vertices[id];
	bool energy=p1.y>0.5f;float alpha=p1.x*(energy?u.togglesOffsetGradient.y:u.togglesOffsetGradient.x);
	float3 pos=posKind.xyz;pos.xz=p0.xz+(pos.xz-p0.xz)*p0.w;pos.y+=u.togglesOffsetGradient.z;
	float dist=distance(u.viewInv[3].xyz,p0.xyz);float fade=clamp(1.0f-(dist-u.fadeEnergy.x)/(u.fadeEnergy.y-u.fadeEnergy.x),0.0f,1.0f);fade=max(fade,p1.z);
	float4 fill=energy?u.fillEnergy:u.fillMetal,edge=energy?u.edgeEnergy:u.edgeMetal;
	float kind=posKind.w;float baseAlpha=kind<0.5f?fill.a:kind<1.5f?u.togglesOffsetGradient.w:edge.a;
	alpha*=baseAlpha*fade*(energy?u.fadeEnergy.z:1.0f);Out out;out.color=float4(kind<1.5f?fill.rgb:edge.rgb,alpha);
	out.position=alpha<=0.0005f?float4(-2,-2,-2,1):u.viewProj*float4(pos,1);
	if(!u.options.y)out.position.z=(out.position.z+out.position.w)*0.5f;
	return out;
}
fragment float4 ReclaimFieldFragment(Out in [[stage_in]],constant Uniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
	if(u.options.z){float a=in.color.a,b=u.alpha.x;bool pass;
		switch(u.options.w){case 0u:pass=false;break;case 1u:pass=a<b;break;case 2u:pass=a==b;break;case 3u:pass=a<=b;break;case 4u:pass=a>b;break;case 5u:pass=a!=b;break;case 6u:pass=a>=b;break;default:pass=true;break;}
		if(!pass)discard_fragment();}
	return in.color;
}
)MSL";
struct Uniform {simd_float4x4 viewProj,viewInv;simd_float4 fillMetal,fillEnergy,edgeMetal,edgeEnergy,togglesOffsetGradient,fadeEnergy;simd_uint4 options;simd_float4 alpha;};
static_assert(sizeof(Uniform)==256);
MTLBlendOperation Equation(ScreenBlendEquation x){switch(x){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Reclaim field blend equation differs");}
simd_float4 Vec(const std::array<float,4>& v){return simd_make_float4(v[0],v[1],v[2],v[3]);}
}
struct PreparedReclaimField::Impl {
	struct Draw {uint64_t sequence=0;std::shared_ptr<const NativeReclaimFieldInput> source;NativeWorldDrawState state;effectstate::Raster raster;
		__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;__strong id<MTL4ArgumentTable> arguments=nil;bool visible=false;};
	std::vector<Draw> draws;size_t next=0;bool completed=false;
};
PreparedReclaimField::PreparedReclaimField():impl(std::make_unique<Impl>()){}
PreparedReclaimField::~PreparedReclaimField()=default;
size_t PreparedReclaimField::DrawCount()const{return impl->draws.size();}
uint64_t PreparedReclaimField::SequenceAt(size_t n)const{return impl->draws.at(n).sequence;}
void PreparedReclaimField::EncodeDraw(size_t n,id<MTL4RenderCommandEncoder> e){
	Require(e&&n==impl->next&&!impl->completed,"Reclaim field fill must consume each actual source ordinal once");const auto& d=impl->draws.at(n);++impl->next;if(!d.visible)return;
	effectstate::RasterState(e,d.state,d.raster);[e setDepthClipMode:d.state.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];
	[e setCullMode:!d.state.cullEnabled?MTLCullModeNone:d.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];
	[e setRenderPipelineState:d.pipeline];[e setDepthStencilState:d.depth];[e setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:d.source->first vertexCount:d.source->count];
}
void PreparedReclaimField::DidComplete(){Require(!impl->completed&&impl->next==impl->draws.size(),"Reclaim field retirement preceded its actual source draws");impl->completed=true;}
struct ReclaimFieldGPU::Impl {
	struct Cached {std::weak_ptr<const NativeBufferSnapshot> source;uint64_t revision=0;__strong id<MTLBuffer> buffer=nil;};
	std::map<uint64_t,Cached> buffers;__strong id<MTLLibrary> library=nil;
	std::map<std::array<unsigned,15>,id<MTLRenderPipelineState>> pipelines;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& s){
		Require(s&&!s->bytes.empty()&&s->bytes.size()<=lease.Device().maxBufferLength,"Reclaim field source buffer exceeds actual device bounds");
		auto& b=buffers[s->object];if(b.revision!=s->revision||b.buffer==nil){b.buffer=[lease.Device()newBufferWithBytes:s->bytes.data() length:s->bytes.size() options:MTLResourceStorageModeShared];Require(b.buffer!=nil,"Cannot materialize original Reclaim chunk buffer");b.revision=s->revision;b.source=s;}
		lease.AddAllocation(b.buffer);return b.buffer;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,MTLPixelFormat color,const ScreenBlendState& blend,const std::array<bool,4>& mask,bool validation,double& ms,unsigned samples,bool coverage){
		std::array<unsigned,15> key{unsigned(color),unsigned(validation),unsigned(blend.enabled),unsigned(blend.sourceColor),unsigned(blend.destinationColor),unsigned(blend.sourceAlpha),unsigned(blend.destinationAlpha),unsigned(blend.colorEquation),unsigned(blend.alphaEquation),unsigned(mask[0]),unsigned(mask[1]),unsigned(mask[2]),unsigned(mask[3]),lease.SampleShadingKey(samples),unsigned(coverage)};
		if(auto found=pipelines.find(key);found!=pipelines.end())return found->second;
		const auto start=Clock::now();NSError* error=nil;
		if(library==nil){auto d=[MTL4LibraryDescriptor new];d.source=String(Source);d.name=@"Original31657 Reclaim field fill";auto o=[MTLCompileOptions new];o.mathMode=MTLMathModeSafe;o.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;d.options=o;library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile original Reclaim field fill: "+Error(error));}
		auto d=[MTL4RenderPipelineDescriptor new];auto vs=[MTL4LibraryFunctionDescriptor new],fs=[MTL4LibraryFunctionDescriptor new];vs.library=fs.library=library;vs.name=@"ReclaimFieldVertex";fs.name=@"ReclaimFieldFragment";d.vertexFunctionDescriptor=vs;d.fragmentFunctionDescriptor=fs;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
		auto p=lease.NewRenderPipeline(d,&error);Require(p!=nil,"Cannot compile Reclaim field actual fill pipeline: "+Error(error));pipelines.emplace(key,p);ms+=Milliseconds(start);return p;
	}
};
ReclaimFieldGPU::ReclaimFieldGPU():impl(std::make_unique<Impl>()){}
ReclaimFieldGPU::~ReclaimFieldGPU(){SetNativeReclaimFieldConsumerAvailable(false);}
void ReclaimFieldGPU::Initialize(RuntimeLease& l,MTLPixelFormat f,bool validation,double& ms,unsigned samples){
	ScreenBlendState blend;blend.enabled=true;blend.sourceColor=blend.sourceAlpha=ScreenBlendFactor::SourceAlpha;blend.destinationColor=blend.destinationAlpha=ScreenBlendFactor::OneMinusSourceAlpha;
	impl->Pipeline(l,f,blend,{true,true,true,true},validation,ms,samples,false);SetNativeReclaimFieldConsumerAvailable(true);
}
void ReclaimFieldGPU::ClearResources(){impl->buffers.clear();}
std::shared_ptr<PreparedReclaimField> ReclaimFieldGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeWorldDrawFrame> frame,uint64_t drawFrame,const WorldCamera& view,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& ms,unsigned samples){
	if(!frame||std::none_of(frame->draws.begin(),frame->draws.end(),[](const auto& c){return c.reclaimField&&!c.reclaimField->edges;}))return {};std::erase_if(impl->buffers,[](const auto& b){return b.second.source.expired();});
	Require(depth==MTLPixelFormatDepth32Float||depth==MTLPixelFormatDepth32Float_Stencil8,"Reclaim field requires the actual common world target");
	auto out=std::shared_ptr<PreparedReclaimField>(new PreparedReclaimField);
	for(const auto& c:frame->draws){if(!c.reclaimField||c.reclaimField->edges)continue;ValidateNativeWorldDrawCommand(c);const auto& d=*c.reclaimField;
		Require(d.sourceDrawFrame==drawFrame&&c.stage==NativeWorldDrawStage::PreUnit,"Reclaim field fill belongs to another actual source frame/stage");
		PreparedReclaimField::Impl::Draw ready;ready.sequence=c.sequence;ready.source=c.reclaimField;ready.state=c.world;
		Require(!c.world.inheritedClipPlane&&c.world.frontPolygonMode==NativePolygonMode::Fill&&c.world.backPolygonMode==NativePolygonMode::Fill,"Reclaim field inherited polygon/clip state requires its actual selected profile");
		auto camera=view;if(c.surface.viewport){const auto& v=*c.surface.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
		auto raster=effectstate::Viewport(camera,width,height,logicalWidth,logicalHeight,c.surface.scissor);
		if(!raster||(c.world.cullEnabled&&c.world.cull==NativeWorldCull::FrontAndBack)){out->impl->draws.push_back(std::move(ready));continue;}ready.raster=*raster;ready.visible=true;
		const auto vertices=impl->Buffer(lease,d.vertices),params=impl->Buffer(lease,d.params.snapshot);
		Uniform u{};std::memcpy(&u.viewProj,d.original->matrices->bytes.data()+reclaimfield::ViewProjectionByte,64);std::memcpy(&u.viewInv,d.original->matrices->bytes.data()+reclaimfield::ViewInverseByte,64);
		const auto& s=d.uniforms;u.fillMetal=Vec(s.fillMetal);u.fillEnergy=Vec(s.fillEnergy);u.edgeMetal=Vec(s.edgeMetal);u.edgeEnergy=Vec(s.edgeEnergy);u.togglesOffsetGradient=simd_make_float4(s.toggles[0],s.toggles[1],s.yOffset,s.gradientAlpha);u.fadeEnergy=simd_make_float4(s.fadeDist[0],s.fadeDist[1],s.energyOpacity,0);
		uint32_t caps;std::memcpy(&caps,d.original->params->bytes.data()+12,4);u.options=simd_make_uint4(s.vertsPerChunk,caps&1u,uint32_t(c.surface.alphaTest.enabled),uint32_t(c.surface.alphaTest.compare));u.alpha=simd_make_float4(c.surface.alphaTest.reference,0,0,0);
		auto ub=[lease.Device()newBufferWithBytes:&u length:sizeof(u) options:MTLResourceStorageModeShared];Require(ub!=nil,"Cannot retain original Reclaim uniforms");lease.AddAllocation(ub);
		auto desc=[MTL4ArgumentTableDescriptor new];desc.maxBufferBindCount=3;desc.initializeBindings=YES;NSError* error=nil;ready.arguments=[lease.Device()newArgumentTableWithDescriptor:desc error:&error];Require(ready.arguments!=nil,"Cannot create actual Reclaim field argument table: "+Error(error));
		[ready.arguments setAddress:vertices.gpuAddress atIndex:0];[ready.arguments setAddress:params.gpuAddress+d.params.offset atIndex:1];[ready.arguments setAddress:ub.gpuAddress atIndex:2];
		ready.pipeline=impl->Pipeline(lease,color,c.surface.blend,c.world.colorWrite,validation,ms,samples,c.world.alphaToCoverage);ready.depth=effectstate::Depth(lease,c.world,stencil);lease.Retain(ready.pipeline);lease.Retain(ready.arguments);lease.Retain(impl->library);out->impl->draws.push_back(std::move(ready));
	}
	return out;
}
}
