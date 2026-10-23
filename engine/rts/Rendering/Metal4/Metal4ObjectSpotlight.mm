/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4ObjectSpotlight.h"
#include "NativeSampleShadingShaders.h"
#include <simd/simd.h>
#include <cstring>
#include <map>
namespace rendering::metal4 {
namespace {
const char* Source=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Uniform {float4x4 viewProj,viewInv;float4 timeAlpha;uint4 options;};
// Original VBOs use packed float3/60-byte records, not aligned float3/64 bytes.
struct Instance {float radius,height;packed_float4 color;float startTime,expireTime;packed_uint4 instData;packed_float3 worldPosOverride;};
static_assert(sizeof(Instance)==60,"Object Spotlight instance ABI differs");
struct Out {float4 position [[position]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];float3 localPos [[RECOIL_SAMPLE_PERSPECTIVE]];float unitID [[RECOIL_SAMPLE_PERSPECTIVE]];float cameraDistance [[RECOIL_SAMPLE_PERSPECTIVE]],lifetimeElapsed [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex Out ObjectSpotlightVertex(uint id [[vertex_id]],uint instance [[instance_id]],
 device const packed_float3* vertices [[buffer(0)]],device const Instance* instances [[buffer(1)]],
 constant Uniform& u [[buffer(2)]],device const float4* modelDrawPositions [[buffer(3)]]){
 const Instance source=instances[instance];const float3 localPos=float3(vertices[id]);
 // Original31657 VS owns the final position selection. A valid override
 // needs no model value, including for a ground instance with no real row0.
 const float3 worldPosOverride=float3(source.worldPosOverride);
 float3 worldPos;
 if(worldPosOverride.x!=0&&worldPosOverride.y!=0&&worldPosOverride.z!=0)
  worldPos=worldPosOverride;
 else
  worldPos=modelDrawPositions[instance].xyz;
 float cameraDistance=length(worldPos-u.viewInv[3].xyz);
 float effectiveHeight=source.height*clamp(cameraDistance/4500.0f,1.0f,2.5f);
 float2 resultPosXZ=localPos.xz*source.radius+worldPos.xz;
 float3 resultPos=float3(resultPosXZ.x,worldPos.y+localPos.y*effectiveHeight,resultPosXZ.y);
 Out out;out.position=u.viewProj*float4(resultPos,1);
 if(!u.options.x)out.position.z=(out.position.z+out.position.w)*0.5f;
 out.color=float4(source.color);out.localPos=localPos;out.unitID=float(source.instData.y);out.cameraDistance=cameraDistance;
 if(source.startTime!=0&&source.expireTime!=0&&source.startTime!=source.expireTime)
  out.lifetimeElapsed=clamp((u.timeAlpha.x-source.startTime)/(source.expireTime-source.startTime),0.0f,1.0f);
 else out.lifetimeElapsed=0;
 return out;
}
float spotlightNoise(float3 pos,float time){
 float flicker=sin(time*3.0f+pos.y*2.0f+cos(pos.x*4.0f+pos.z*3.0f))*0.5f;
 float wave1=sin(time*2.0f+pos.x*3.0f+pos.z*2.0f)*0.5f;
 float wave2=cos(time*1.5f+pos.y*2.5f-pos.z*3.5f)*0.3f;
 return flicker+wave1+wave2;
}
float3 spotlightRGB(float3 c,float v){if(v>0)return mix(c,float3(1),v);else return mix(c,float3(0),abs(v));}
fragment float4 ObjectSpotlightFragment(Out in [[stage_in]],constant Uniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float effectiveY=in.localPos.y;
 if(effectiveY<0)effectiveY=clamp(abs(effectiveY)*4.0f,0.0f,1.0f);
 float t=0.25f*u.timeAlpha.x;
 float v1=clamp(0.05f*spotlightNoise(in.localPos,t),-0.25f,0.25f);
 float v2=clamp(0.5f*spotlightNoise(in.localPos,t*2.0f),-0.5f,0.5f);
 float4 color=float4(spotlightRGB(in.color.rgb,v1),in.color.a*(1+v2)*pow(1-effectiveY,4.0f)*clamp(in.cameraDistance/4500.0f,0.1f,1.1f)*pow(1-in.lifetimeElapsed,1.0f));
 if(u.options.y){float a=color.a,b=u.timeAlpha.y;bool pass;
  switch(u.options.z){case 0u:pass=false;break;case 1u:pass=a<b;break;case 2u:pass=a==b;break;case 3u:pass=a<=b;break;case 4u:pass=a>b;break;case 5u:pass=a!=b;break;case 6u:pass=a>=b;break;default:pass=true;break;}
  if(!pass)discard_fragment();}
 return color;
}
struct WreckInstance {packed_float3 worldPos;float radius,height;packed_float4 color;};
static_assert(sizeof(WreckInstance)==36,"Original Commander Wreck instance ABI");
struct WreckOut {float4 position [[position]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];float localY [[RECOIL_SAMPLE_PERSPECTIVE]],cameraDistance [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex WreckOut CommanderWreckVertex(uint id [[vertex_id]],uint instance [[instance_id]],device const packed_float3* vertices [[buffer(0)]],device const WreckInstance* instances [[buffer(1)]],constant Uniform& u [[buffer(2)]]){
 const WreckInstance source=instances[instance];float3 localPos=float3(vertices[id]),worldPos=float3(source.worldPos);
 float cameraDistance=length(worldPos-u.viewInv[3].xyz);float effectiveRadius=source.radius*clamp(cameraDistance/4500.0f,1.0f,2.5f);
 float2 resultXZ=localPos.xz*effectiveRadius+worldPos.xz;float3 resultPos=float3(resultXZ.x,worldPos.y+localPos.y*source.height,resultXZ.y);
 WreckOut out;out.position=u.viewProj*float4(resultPos,1);if(!u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.color=float4(source.color);out.localY=localPos.y;out.cameraDistance=cameraDistance;return out;
}
fragment float4 CommanderWreckFragment(WreckOut in [[stage_in]],constant Uniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 color=float4(in.color.rgb,clamp(in.color.a*(1.0f-sqrt(in.localY))*clamp(in.cameraDistance/4500.0f,.25f,1.1f),0.0f,1.0f));
 if(u.options.y){float a=color.a,b=u.timeAlpha.y;bool pass;
  switch(u.options.z){case 0u:pass=false;break;case 1u:pass=a<b;break;case 2u:pass=a==b;break;case 3u:pass=a<=b;break;case 4u:pass=a>b;break;case 5u:pass=a!=b;break;case 6u:pass=a>=b;break;default:pass=true;break;}if(!pass)discard_fragment();}
 return color;
}
)MSL";
struct Uniform {simd_float4x4 viewProj,viewInv;simd_float4 timeAlpha;simd_uint4 options;};
static_assert(sizeof(Uniform)==160);
MTLBlendOperation Equation(ScreenBlendEquation x){switch(x){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Object Spotlight blend equation differs");}
}
struct PreparedObjectSpotlight::Impl {
	struct Draw {uint64_t sequence=0;std::shared_ptr<const NativeObjectSpotlightInput> source;NativeWorldDrawState state;effectstate::Raster raster;
		__strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;__strong id<MTL4ArgumentTable> arguments=nil;bool visible=false;};
	std::vector<Draw> draws;size_t next=0;bool completed=false;
};
PreparedObjectSpotlight::PreparedObjectSpotlight():impl(std::make_unique<Impl>()){}
PreparedObjectSpotlight::~PreparedObjectSpotlight()=default;
size_t PreparedObjectSpotlight::DrawCount()const{return impl->draws.size();}
uint64_t PreparedObjectSpotlight::SequenceAt(size_t n)const{return impl->draws.at(n).sequence;}
void PreparedObjectSpotlight::EncodeDraw(size_t n,id<MTL4RenderCommandEncoder> e){
	Require(e&&n==impl->next&&!impl->completed,"Object Spotlight must consume each actual source ordinal once");const auto& d=impl->draws.at(n);++impl->next;if(!d.visible)return;
	effectstate::RasterState(e,d.state,d.raster);[e setDepthClipMode:d.state.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];
	[e setCullMode:!d.state.cullEnabled?MTLCullModeNone:d.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];
	[e setRenderPipelineState:d.pipeline];[e setDepthStencilState:d.depth];[e setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:d.source->first vertexCount:d.source->count instanceCount:d.source->instanceCount];
}
void PreparedObjectSpotlight::DidComplete(){Require(!impl->completed&&impl->next==impl->draws.size(),"Object Spotlight retirement preceded its actual source draws");impl->completed=true;}
struct ObjectSpotlightGPU::Impl {
	struct Cached {std::weak_ptr<const NativeBufferSnapshot> source;uint64_t revision=0;__strong id<MTLBuffer> buffer=nil;};
	std::map<uint64_t,Cached> buffers;__strong id<MTLLibrary> library=nil;
 std::shared_ptr<const void> compiledFamily;
	std::map<std::array<unsigned,16>,id<MTLRenderPipelineState>> pipelines;
	id<MTLBuffer> Buffer(RuntimeLease& lease,const NativeBufferSnapshotRef& s){
		Require(s&&!s->bytes.empty()&&s->bytes.size()<=lease.Device().maxBufferLength,"Object Spotlight source buffer exceeds actual device bounds");
		auto& b=buffers[s->object];if(b.revision!=s->revision||b.buffer==nil){b.buffer=[lease.Device()newBufferWithBytes:s->bytes.data() length:s->bytes.size() options:MTLResourceStorageModeShared];Require(b.buffer!=nil,"Cannot materialize original Object Spotlight buffer");b.revision=s->revision;b.source=s;}
		lease.AddAllocation(b.buffer);return b.buffer;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,MTLPixelFormat color,const ScreenBlendState& blend,const std::array<bool,4>& mask,bool validation,double& ms,bool wreck,unsigned samples,bool coverage){
		std::array<unsigned,16> key{unsigned(color),unsigned(validation),unsigned(blend.enabled),unsigned(blend.sourceColor),unsigned(blend.destinationColor),unsigned(blend.sourceAlpha),unsigned(blend.destinationAlpha),unsigned(blend.colorEquation),unsigned(blend.alphaEquation),unsigned(mask[0]),unsigned(mask[1]),unsigned(mask[2]),unsigned(mask[3]),unsigned(wreck),lease.SampleShadingKey(samples),unsigned(coverage)};
		if(auto found=pipelines.find(key);found!=pipelines.end())return found->second;
		const auto start=Clock::now();NSError* error=nil;
		if(library==nil){auto d=[MTL4LibraryDescriptor new];d.source=String(Source);d.name=@"Original31657 Object Spotlight";auto o=[MTLCompileOptions new];o.mathMode=MTLMathModeSafe;o.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;d.options=o;library=lease.NewLibrary(d,&error);Require(library!=nil,"Cannot compile original Object Spotlight: "+Error(error));}
		auto d=[MTL4RenderPipelineDescriptor new];auto vs=[MTL4LibraryFunctionDescriptor new],fs=[MTL4LibraryFunctionDescriptor new];vs.library=fs.library=library;vs.name=wreck?@"CommanderWreckVertex":@"ObjectSpotlightVertex";fs.name=wreck?@"CommanderWreckFragment":@"ObjectSpotlightFragment";d.vertexFunctionDescriptor=vs;d.fragmentFunctionDescriptor=fs;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
		auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
		auto p=lease.NewRenderPipeline(d,&error);Require(p!=nil,"Cannot compile Object Spotlight actual fill pipeline: "+Error(error));pipelines.emplace(key,p);ms+=Milliseconds(start);return p;
	}
};
ObjectSpotlightGPU::ObjectSpotlightGPU():impl(std::make_unique<Impl>()){}
ObjectSpotlightGPU::~ObjectSpotlightGPU(){if(NativeObjectSpotlightConsumer()==impl->compiledFamily)SetNativeObjectSpotlightConsumer({});}
void ObjectSpotlightGPU::Initialize(RuntimeLease& l,MTLPixelFormat f,bool validation,double& ms,unsigned samples){
	if(NativeObjectSpotlightConsumer()==impl->compiledFamily)SetNativeObjectSpotlightConsumer({});
	ScreenBlendState blend;blend.enabled=true;blend.sourceColor=blend.sourceAlpha=ScreenBlendFactor::SourceAlpha;blend.destinationColor=blend.destinationAlpha=ScreenBlendFactor::OneMinusSourceAlpha;
	impl->Pipeline(l,f,blend,{true,true,true,true},validation,ms,false,samples,false);impl->Pipeline(l,f,blend,{true,true,true,true},validation,ms,true,samples,false);impl->compiledFamily=std::make_shared<int>(0);SetNativeObjectSpotlightConsumer(impl->compiledFamily);
}
void ObjectSpotlightGPU::ClearResources(){impl->buffers.clear();}
std::shared_ptr<PreparedObjectSpotlight> ObjectSpotlightGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeWorldDrawFrame> frame,uint64_t drawFrame,const WorldCamera& view,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& ms,unsigned samples){
	if(!frame||std::none_of(frame->draws.begin(),frame->draws.end(),[](const auto& c){return bool(c.objectSpotlight);}))return {};std::erase_if(impl->buffers,[](const auto& b){return b.second.source.expired();});
	Require(depth==MTLPixelFormatDepth32Float||depth==MTLPixelFormatDepth32Float_Stencil8,"Object Spotlight requires the actual common world target");
	auto out=std::shared_ptr<PreparedObjectSpotlight>(new PreparedObjectSpotlight);
	for(const auto& c:frame->draws){if(!c.objectSpotlight)continue;ValidateNativeWorldDrawCommand(c);const auto& d=*c.objectSpotlight;
		Require(d.sourceDrawFrame==drawFrame&&c.stage==NativeWorldDrawStage::World,"Object Spotlight belongs to another actual source frame/stage");
		PreparedObjectSpotlight::Impl::Draw ready;ready.sequence=c.sequence;ready.source=c.objectSpotlight;ready.state=c.world;
  Require(d.compiledFamily==impl->compiledFamily,"Object Spotlight source uses another compiled consumer generation");
		Require(!c.world.inheritedClipPlane&&c.world.frontPolygonMode==NativePolygonMode::Fill&&c.world.backPolygonMode==NativePolygonMode::Fill,"Object Spotlight inherited polygon/clip state requires its actual selected profile");
		auto camera=view;if(c.surface.viewport){const auto& v=*c.surface.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
		auto raster=effectstate::Viewport(camera,width,height,logicalWidth,logicalHeight,c.surface.scissor);
		if(!raster||(c.world.cullEnabled&&c.world.cull==NativeWorldCull::FrontAndBack)){out->impl->draws.push_back(std::move(ready));continue;}ready.raster=*raster;ready.visible=true;
		const auto vertices=impl->Buffer(lease,d.vertices),instances=impl->Buffer(lease,d.instances);
		id<MTLBuffer> pb=nil;
  if(!d.commanderWreck){
		const auto modelPositions=ReadNativeObjectSpotlightModelPositions(d);
		Require(!modelPositions.empty()&&modelPositions.size()<=lease.Device().maxBufferLength/sizeof(modelPositions.front()),"Object Spotlight actual model positions exceed device bounds");
		pb=[lease.Device()newBufferWithBytes:modelPositions.data() length:modelPositions.size()*sizeof(modelPositions.front()) options:MTLResourceStorageModeShared];Require(pb!=nil,"Cannot retain actual Object Spotlight model positions");lease.AddAllocation(pb);
  }
		Uniform u{};std::memcpy(&u.viewProj,d.original->matrices->bytes.data()+objectspotlight::ViewProjectionByte,64);std::memcpy(&u.viewInv,d.original->matrices->bytes.data()+objectspotlight::ViewInverseByte,64);
		float drawSeconds;std::memcpy(&drawSeconds,d.original->params->bytes.data()+objectspotlight::DrawSecondsByte,4);u.timeAlpha=simd_make_float4(drawSeconds,c.surface.alphaTest.reference,0,0);
		uint32_t caps;std::memcpy(&caps,d.original->params->bytes.data()+12,4);u.options=simd_make_uint4(caps&1u,uint32_t(c.surface.alphaTest.enabled),uint32_t(c.surface.alphaTest.compare),0);
		auto ub=[lease.Device()newBufferWithBytes:&u length:sizeof(u) options:MTLResourceStorageModeShared];Require(ub!=nil,"Cannot retain original Object Spotlight uniforms");lease.AddAllocation(ub);
		auto desc=[MTL4ArgumentTableDescriptor new];desc.maxBufferBindCount=4;desc.initializeBindings=YES;NSError* error=nil;ready.arguments=[lease.Device()newArgumentTableWithDescriptor:desc error:&error];Require(ready.arguments!=nil,"Cannot create actual Object Spotlight argument table: "+Error(error));
		[ready.arguments setAddress:vertices.gpuAddress atIndex:0];[ready.arguments setAddress:instances.gpuAddress+size_t(d.firstInstance)*(d.commanderWreck?36:objectspotlight::InstanceStride) atIndex:1];[ready.arguments setAddress:ub.gpuAddress atIndex:2];if(pb)[ready.arguments setAddress:pb.gpuAddress atIndex:3];
		ready.pipeline=impl->Pipeline(lease,color,c.surface.blend,c.world.colorWrite,validation,ms,d.commanderWreck,samples,c.world.alphaToCoverage);ready.depth=effectstate::Depth(lease,c.world,stencil);lease.Retain(ready.pipeline);lease.Retain(ready.arguments);lease.Retain(impl->library);out->impl->draws.push_back(std::move(ready));
	}
	return out;
}
}
