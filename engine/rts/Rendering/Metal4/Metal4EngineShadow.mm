/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4EngineShadow.h"
#include "Rendering/Backend/NativeUITextureHistory.h"
#include "Metal4EngineModelState.h"
#include "Metal4NativeEffectState.h"
#include "NativeEngineDeferredShaders.h"
#include "NativeEngineTextureABI.h"
#include "NativeEngineShadowShaders.h"
#include "NativeGenericShadowShaders.h"
#include "Rendering/Backend/NativeGenericShadowData.h"
#include <map>
#include <set>
#include <algorithm>
#include <cstring>
#include <climits>
#include <cmath>
#include <tuple>
#include <string>
namespace rendering::metal4 {
namespace {
using engineoriginal::Uniform;using engineoriginal::Matrix;using engineoriginal::Vec;
struct TerrainShadowUniform {
 simd_float4x4 modelView,projection;
 simd_float4 mapSize,parameters;
 simd_float4 clips[3];
 simd_uint4 flags;
};
static_assert(sizeof(TerrainShadowUniform)==224&&offsetof(TerrainShadowUniform,clips)==160&&offsetof(TerrainShadowUniform,flags)==208);
static_assert(sizeof(std::array<float,3>)==12);
// Same production allocation as WorldRenderer; callers retain each allocation.
id<MTLBuffer> Buffer(id<MTLDevice> device, const void* bytes, size_t length, NSString* label)
{
	Require(length > 0 && length <= device.maxBufferLength, "World GPU buffer length exceeds device bounds");
	id<MTLBuffer> result = bytes ? [device newBufferWithBytes:bytes length:length options:MTLResourceStorageModeShared] :
		[device newBufferWithLength:length options:MTLResourceStorageModeShared];
	Require(result != nil, "Cannot allocate world GPU buffer");
	result.label = label;
	return result;
}
struct GenericShadowUniform {
	simd_float4x4 modelView,projection;
	simd_float4 clips[3];
	simd_uint4 flags;
};
static_assert(sizeof(GenericShadowUniform)==192&&offsetof(GenericShadowUniform,clips)==128&&offsetof(GenericShadowUniform,flags)==176);
id<MTLBuffer> GenericBuffer(RuntimeLease& lease,const void* data,size_t count,size_t stride,NSString* label)
{
	Require(data&&count&&stride&&count<=lease.Device().maxBufferLength/stride,"Original generic shadow buffer exceeds the actual device bounds");
	auto result=Buffer(lease.Device(),data,count*stride,label);lease.AddAllocation(result);return result;
}
void Target(const CUSSourceTarget& target){
 Require((target.depth==MTLPixelFormatDepth16Unorm||target.depth==MTLPixelFormatDepth32Float)&&target.stencil==MTLPixelFormatInvalid&&target.samples==1,
  "Original shadow depth target requires actual D16/D32Float storage");
 Require((target.colors[0]==MTLPixelFormatInvalid||target.colors[0]==MTLPixelFormatRGBA8Unorm||target.colors[0]==MTLPixelFormatR8Unorm)&&
  std::all_of(target.colors.begin()+1,target.colors.end(),[](auto f){return f==MTLPixelFormatInvalid;}),"Original shadow source selected another color attachment set");
}
bool Empty(const NativeCUSTextureBinding& b){return ui::NativeUITextureHistory::SamplingIncomplete(b);}
const NativeTextureSampling& Sampling(const NativeCUSTextureBinding& b){Require(b.knowledge!=NativeCUSBindingKnowledge::Unobserved,"Original shadow alpha sampler is unobserved");if(b.knowledge==NativeCUSBindingKnowledge::DefaultObject)return b.defaultSampling;Require(bool(b.object),"Original shadow alpha sampler lost its actual object");return b.object->sampling;}
void Rigid(const NativeEngineShadowDraw& draw){
 Require(draw.object&&draw.object->model&&draw.object->modelType==1&&draw.object->pieces.size()==draw.object->model->pieces.size(),"Original shadow model format/pose has no actual shared rigid consumer");
	for (size_t p = 0; p < draw.object->model->pieces.size(); ++p) {
		const auto& piece = draw.object->model->pieces[p];
		// Match original HasGeometryData/PostProcessGeometry and the shared
		// geometry uploader. Emitter-only vertices retain their unassigned bone
		// IDs and CPU/pose slots, but never enter a vertex shader invocation.
		if (piece.indices.size() < 3) continue;
		for (size_t vertex = 0; vertex < piece.vertices.size(); ++vertex) {
			const auto& v = piece.vertices[vertex];
			if (v.boneIds[0] == p && v.boneWeights == std::array<uint8_t, 4>{255, 0, 0, 0}) continue;
			throw std::runtime_error("Original drawable weighted/mixed shadow bones require their actual skinning consumer: " +
				draw.object->model->sourceName + "#" + piece.name + " owner=" + std::to_string(unsigned(draw.object->owner)) +
				" id=" + std::to_string(draw.object->id) + " piece=" + std::to_string(p) + " vertex=" + std::to_string(vertex) +
				" indices=" + std::to_string(piece.indices.size()) + " bones=" + std::to_string(v.boneIds[0]) + "," +
				std::to_string(v.boneIds[1]) + "," + std::to_string(v.boneIds[2]) + "," + std::to_string(v.boneIds[3]) +
				" weights=" + std::to_string(v.boneWeights[0]) + "," + std::to_string(v.boneWeights[1]) + "," +
				std::to_string(v.boneWeights[2]) + "," + std::to_string(v.boneWeights[3]));
		}
	}
}
Uniform Snapshot(const NativeEngineShadowFrame& frame,const NativeEngineShadowDraw& draw,const CUSSourceTarget& target){
 Uniform u{};u.world=Matrix(draw.modelView);u.viewProjection=Matrix(draw.projection);u.alphaControl=Vec(draw.alphaControl);
 u.flags.x=unsigned(frame.camera.clipDepth==ClipDepthConvention::MinusOneToOne)|(unsigned(target.invertClipY)<<1);u.flags.y=draw.modern;
 u.clip0=draw.clipEnabled[0]?Vec(draw.clipPlanes[0]):simd_float4{0,0,0,1};u.clip1=draw.clipEnabled[1]?Vec(draw.clipPlanes[1]):simd_float4{0,0,0,1};
 if(draw.modern){
  const auto& pose=draw.pose;Require(!pose.staticModel&&pose.transforms&&pose.engineUniforms,"Original live GL4 shadow has no immutable original transform/UBO owner");ValidateNativeCUSUniformPublication(*pose.engineUniforms);
  UniformMatricesBuffer matrices;UniformParamsBuffer params;
  std::memcpy(&matrices,pose.engineUniforms->matrices->bytes.data(),sizeof(matrices));std::memcpy(&params,pose.engineUniforms->params->bytes.data(),NativeCUSParamsPrefixBytes);
  u.world=Matrix(matrices.shadowView.m);u.viewProjection=Matrix(matrices.shadowProj.m);u.flags.x=unsigned(!(params.renderCaps&1u))|(unsigned(target.invertClipY)<<1);u.samplerParameters.z=params.timeInfo.w;
  engineoriginal::Transform(*pose.transforms,pose.instance[0],u.root0q,u.root0t);engineoriginal::Transform(*pose.transforms,size_t(pose.instance[0])+1,u.root1q,u.root1t);
 }
 return u;
}
}
struct PreparedEngineShadow::Impl {
 struct Piece{EngineDeferredPieceBuffers geometry;__strong id<MTL4ArgumentTable> arguments=nil;uint32_t firstIndex=0;bool indexed=true;};
 struct Draw{uint64_t sequence=0;NativeWorldDrawState state;std::optional<effectstate::Raster> raster;std::vector<Piece> pieces;CUSSourceImage image;float shadowPolygonOffsetUnitsScale=1.f;__strong id<MTLDepthStencilState> depth=nil;__strong id<MTLRenderPipelineState> pipeline=nil;};
 std::shared_ptr<const NativeEngineShadowFrame> source;std::vector<Draw> draws;
 std::shared_ptr<const NativeTerrainShadowFrame> terrainSource;
 bool dependencies=false,finished=false,completed=false;size_t next=0;
};
PreparedEngineShadow::PreparedEngineShadow():impl(std::make_unique<Impl>()){}
PreparedEngineShadow::~PreparedEngineShadow()=default;
size_t PreparedEngineShadow::DrawCount()const{return impl->draws.size();}
uint64_t PreparedEngineShadow::SequenceAt(size_t i)const{Require(i<DrawCount(),"Original shadow draw index exceeds retained source");return impl->draws[i].sequence;}
void PreparedEngineShadow::EncodeDependencies(id<MTL4CommandBuffer> command){
 Require(command&&!impl->dependencies&&!impl->completed,"Original shadow model dependencies are missing/repeated");std::set<const void*> seen;
 for(const auto& draw:impl->draws)if(draw.image.owner&&seen.insert(draw.image.owner.get()).second&&draw.image.encodeDependencies)draw.image.encodeDependencies(command);impl->dependencies=true;
}
void PreparedEngineShadow::EncodeDraw(size_t i,id<MTL4RenderCommandEncoder> encoder){
 Require(encoder&&impl->dependencies&&!impl->finished&&i==impl->next&&i<DrawCount(),"Original shadow model draw skipped/repeated or has no actual dependencies");const auto& draw=impl->draws[i];
 if(draw.raster&&(!draw.state.cullEnabled||draw.state.cull!=NativeWorldCull::FrontAndBack)){
  effectstate::RasterState(encoder,draw.state,*draw.raster);[encoder setDepthStencilState:draw.depth];[encoder setRenderPipelineState:draw.pipeline];
  if(draw.state.polygonOffset)[encoder setDepthBias:draw.state.offsetUnits*draw.shadowPolygonOffsetUnitsScale slopeScale:draw.state.offsetFactor clamp:0];
  [encoder setCullMode:!draw.state.cullEnabled||draw.state.cull==NativeWorldCull::None?MTLCullModeNone:draw.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];
  [encoder setTriangleFillMode:draw.state.frontPolygonMode==NativePolygonMode::Line?MTLTriangleFillModeLines:MTLTriangleFillModeFill];if(draw.image.requireReadable)draw.image.requireReadable();
  for(const auto& piece:draw.pieces){
   [encoder setArgumentTable:piece.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
   if(piece.indexed){const NSUInteger offset=NSUInteger(piece.firstIndex)*sizeof(uint32_t);[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:piece.geometry.indexCount indexType:MTLIndexTypeUInt32 indexBuffer:piece.geometry.indices.gpuAddress+offset indexBufferLength:piece.geometry.indices.length-offset];}
   else [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:piece.geometry.indexCount];
  }
 }++impl->next;
}
void PreparedEngineShadow::Finish(){Require(impl->dependencies&&!impl->finished&&impl->next==DrawCount(),"Original shadow model source did not finish exactly once");impl->finished=true;}
void PreparedEngineShadow::DidComplete(){Require(impl->finished&&!impl->completed,"Original shadow model retirement preceded its real execution");std::set<const void*> seen;for(const auto& draw:impl->draws)if(draw.image.owner&&seen.insert(draw.image.owner.get()).second&&draw.image.didComplete)draw.image.didComplete();impl->completed=true;}
struct PreparedGenericShadow::Impl {
	struct Draw {
		uint64_t sequence=0;NSUInteger indexCount=0;
		NativeWorldDrawState state;std::optional<effectstate::Raster> raster;
		float shadowPolygonOffsetUnitsScale=1.f;
		__strong id<MTLBuffer> vertices=nil,indices=nil,uniforms=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;
		__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTLRenderPipelineState> pipeline=nil;
	};
	std::shared_ptr<const NativeWorldDrawFrame> source;
	std::vector<Draw> draws;size_t next=0;
	bool dependencies=false,finished=false,completed=false;
};
PreparedGenericShadow::PreparedGenericShadow():impl(std::make_unique<Impl>()){}
PreparedGenericShadow::~PreparedGenericShadow()=default;
size_t PreparedGenericShadow::DrawCount()const{return impl->draws.size();}
uint64_t PreparedGenericShadow::SequenceAt(size_t index)const
{
	Require(index<DrawCount(),"Original generic shadow index exceeds the retained common command");return impl->draws[index].sequence;
}
void PreparedGenericShadow::EncodeDependencies(id<MTL4CommandBuffer> command)
{
	// The selected legacy fragment reads no image. Buffer residency already
	// belongs to this lease; no synthetic texture producer is introduced.
	Require(command&&!impl->dependencies&&!impl->finished&&!impl->completed,"Original generic shadow dependencies are missing or repeated");impl->dependencies=true;
}
void PreparedGenericShadow::EncodeDraw(size_t index,id<MTL4RenderCommandEncoder> encoder)
{
	Require(encoder&&impl->dependencies&&!impl->finished&&index==impl->next&&index<DrawCount(),"Original generic shadow command was skipped/repeated or has no dependencies");
	// The common dispatcher splits this same named target at original clears.
	// Consume its current segment; the common command/submission and ordinals
	// remain owned by PreparedOriginalShadowSource across those boundaries.
	const auto& draw=impl->draws[index];
	if(draw.raster&&(!draw.state.cullEnabled||draw.state.cull!=NativeWorldCull::FrontAndBack)){
		effectstate::RasterState(encoder,draw.state,*draw.raster);
		[encoder setDepthClipMode:draw.state.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];
		[encoder setCullMode:!draw.state.cullEnabled||draw.state.cull==NativeWorldCull::None?MTLCullModeNone:draw.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];
		if(draw.state.polygonOffset)[encoder setDepthBias:draw.state.offsetUnits*draw.shadowPolygonOffsetUnitsScale slopeScale:draw.state.offsetFactor clamp:0];
		[encoder setDepthStencilState:draw.depth];[encoder setRenderPipelineState:draw.pipeline];
		[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.indexCount indexType:MTLIndexTypeUInt32 indexBuffer:draw.indices.gpuAddress indexBufferLength:draw.indices.length];
	}
	++impl->next;
}
void PreparedGenericShadow::Finish()
{
	Require(impl->dependencies&&!impl->finished&&impl->next==DrawCount(),"Original generic shadow common commands did not finish exactly once");impl->finished=true;
}
void PreparedGenericShadow::DidComplete()
{
	Require(impl->finished&&!impl->completed,"Original generic shadow retirement preceded actual execution or repeated");impl->completed=true;
}
struct EngineShadowGPU::Impl {
 struct Original{bool modern=false,validation=false;NativeEngineShadowProfile profile=NativeEngineShadowProfile::LegacyModel;CUSSourceTarget target;__strong id<MTLRenderPipelineState> pipeline=nil;};
 std::map<const void*,std::shared_ptr<const Original>> programs;__strong id<MTLLibrary> library=nil;
 std::map<uint32_t,id<MTLLibrary>> imageLibraries;
 using ImageKey=std::tuple<std::array<MTLPixelFormat,5>,MTLPixelFormat,MTLPixelFormat,unsigned,uint32_t,bool>;
 std::map<ImageKey,id<MTLRenderPipelineState>> imagePipelines;
 id<MTLRenderPipelineState> ModelPipeline(RuntimeLease& lease,const Original& original,uint32_t imageKey,bool validation,double& ms){
  Require(original.modern&&library&&library.device==lease.Device(),"Original shadow image variant lacks its genuine modern program/device");
  Require(imageKey<=2,"Original shadow alpha image has another sampler type");
  if(imageKey==0&&validation==original.validation){lease.Retain(library);lease.Retain(original.pipeline);return original.pipeline;}
  const auto& target=original.target;
  const ImageKey key{target.colors,target.depth,target.stencil,target.samples,imageKey,validation};
  if(auto found=imagePipelines.find(key);found!=imagePipelines.end()){
   lease.Retain(imageKey?imageLibraries.at(imageKey):library);lease.Retain(found->second);return found->second;
  }
  const auto start=Clock::now();NSError* error=nil;id<MTLLibrary> selected=library;
  if(imageKey){
   auto& variant=imageLibraries[imageKey];
   if(!variant){auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
    variant=lease.NewLibraryWithSource(String(NativeEngineTextureShaderDefines(imageKey,true)+NativeEngineDeferredShaders+NativeEngineShadowShaders+NativeGenericShadowShaders),options,&error);Require(variant,"Cannot compile original ShadowGen image-type library: "+Error(error));}
   selected=variant;
  }
  auto d=[MTL4RenderPipelineDescriptor new];auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=selected;
  v.name=@"OriginalShadowVertex";f.name=@"OriginalShadowGL4";d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=target.samples;
  for(unsigned i=0;i<5;++i){d.colorAttachments[i].pixelFormat=target.colors[i];d.colorAttachments[i].writeMask=MTLColorWriteMaskNone;d.colorAttachments[i].blendingState=MTL4BlendStateDisabled;}
  auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
  auto pipeline=lease.NewRenderPipeline(d,&error);Require(pipeline,"Cannot compile original ShadowGen image-type PSO: "+Error(error));
  imagePipelines.emplace(key,pipeline);lease.Retain(selected);lease.Retain(pipeline);ms+=Milliseconds(start);return pipeline;
 }
 // The actual upload publication is immutable and shared across frames. Keep
 // its one GPU copy with this existing renderer owner, replacing it only when
 // the original uploader publishes new bytes. Prepared draws and their lease
 // retain older buffers if the source publication changes before retirement.
 struct ProjectileGeometry {
  std::shared_ptr<const NativeShadowPieceGeometry> owner;
  EngineDeferredPieceBuffers buffers;
 } projectileGeometry;
	using GenericKey=std::tuple<std::array<MTLPixelFormat,5>,MTLPixelFormat,MTLPixelFormat,unsigned,bool>;
	std::map<GenericKey,id<MTLRenderPipelineState>> genericPipelines;
	id<MTLRenderPipelineState> GenericPipeline(RuntimeLease& lease,const CUSSourceTarget& target,bool validation,double& milliseconds)
	{
		Require(library&&library.device==lease.Device(),"Original generic shadow has no actual selected compiler library/device");
		const GenericKey key{target.colors,target.depth,target.stencil,target.samples,validation};
		if(auto found=genericPipelines.find(key);found!=genericPipelines.end()){
			Require(found->second.device==lease.Device(),"Original generic shadow pipeline belongs to another device");return found->second;
		}
		const auto start=Clock::now();auto descriptor=[MTL4RenderPipelineDescriptor new];
		auto vertex=[MTL4LibraryFunctionDescriptor new],fragment=[MTL4LibraryFunctionDescriptor new];vertex.library=fragment.library=library;
		vertex.name=@"OriginalGenericShadowVertex";fragment.name=@"OriginalGenericShadowFragment";
		descriptor.vertexFunctionDescriptor=vertex;descriptor.fragmentFunctionDescriptor=fragment;
		descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;descriptor.rasterSampleCount=target.samples;
		for(unsigned i=0;i<target.colors.size();++i){auto color=descriptor.colorAttachments[i];color.pixelFormat=target.colors[i];color.writeMask=MTLColorWriteMaskNone;color.blendingState=MTL4BlendStateDisabled;}
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;
		NSError* error=nil;auto pipeline=lease.NewRenderPipeline(descriptor,&error);
		Require(pipeline!=nil,"Cannot compile original generic shadow input pipeline: "+Error(error));
		genericPipelines.emplace(key,pipeline);milliseconds+=Milliseconds(start);return pipeline;
	}
};
EngineShadowGPU::EngineShadowGPU():impl(std::make_unique<Impl>()){}
EngineShadowGPU::~EngineShadowGPU()=default;
std::shared_ptr<const void> EngineShadowGPU::CompileOriginalProgram(RuntimeLease& lease,bool modern,const CUSSourceTarget& target,bool validation,double& ms){
 return CompileOriginalProgram(lease,modern?NativeEngineShadowProfile::ModernModel:NativeEngineShadowProfile::LegacyModel,target,validation,ms);
}
std::shared_ptr<const void> EngineShadowGPU::CompileOriginalProgram(RuntimeLease& lease,NativeEngineShadowProfile profile,const CUSSourceTarget& target,bool validation,double& ms){
 const bool modern=profile==NativeEngineShadowProfile::ModernModel;
 Target(target);for(const auto& [key,old]:impl->programs)if(old->profile==profile&&old->validation==validation&&old->target.colors==target.colors&&old->target.depth==target.depth&&old->target.stencil==target.stencil&&old->target.samples==target.samples)return old;
 const auto start=Clock::now();NSError* error=nil;
 if(!impl->library){auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
  impl->library=lease.NewLibraryWithSource(String(NativeEngineTextureShaderDefines(0,true)+NativeEngineDeferredShaders+NativeEngineShadowShaders+NativeGenericShadowShaders),options,&error);Require(impl->library,"Cannot compile actual original ShadowGen stages: "+Error(error));}
 auto d=[MTL4RenderPipelineDescriptor new];auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=impl->library;v.name=profile==NativeEngineShadowProfile::Terrain?@"OriginalTerrainShadowVertex":@"OriginalShadowVertex";f.name=modern?@"OriginalShadowGL4":@"OriginalShadowLegacy";d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=target.samples;
 for(unsigned i=0;i<5;++i){d.colorAttachments[i].pixelFormat=target.colors[i];d.colorAttachments[i].writeMask=MTLColorWriteMaskNone;d.colorAttachments[i].blendingState=MTL4BlendStateDisabled;}
 auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
 auto token=std::make_shared<Impl::Original>();token->modern=modern;token->validation=validation;token->profile=profile;token->target=target;token->pipeline=lease.NewRenderPipeline(d,&error);Require(token->pipeline,"Cannot compile original selected ShadowGen PSO: "+Error(error));lease.Retain(token->pipeline);impl->programs.emplace(token.get(),token);ms+=Milliseconds(start);return token;
}
std::shared_ptr<PreparedEngineShadow> EngineShadowGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeEngineShadowFrame> source,const CUSSourceTarget& target,const EngineDeferredGeometryProvider& geometry,const CUSSourceImageProvider& images,bool validation,double& ms){
 Target(target);Require(source&&geometry&&images&&source->camera.clipDepth!=ClipDepthConvention::Unsupported&&target.width&&target.height&&target.logicalWidth&&target.logicalHeight,"Original shadow source/shared target/resources are unconnected");
 auto result=std::shared_ptr<PreparedEngineShadow>(new PreparedEngineShadow);result->impl->source=std::move(source);std::map<const WorldModel*,std::vector<EngineDeferredPieceBuffers>> meshes;
 for(const auto& d:result->impl->source->draws){
  if(d.pieceGeometry)Require(result->impl->source->projectile&&!d.modern&&!d.object&&d.profile==NativeEngineShadowProfile::Projectile,"Original projectile piece was relabeled as another model source");else Rigid(d);
  Require(d.world.frontPolygonMode==d.world.backPolygonMode&&d.world.frontPolygonMode!=NativePolygonMode::Point&&(!d.world.lineStipple||d.world.frontPolygonMode==NativePolygonMode::Fill)&&(d.world.lineWidth==1||d.world.frontPolygonMode==NativePolygonMode::Fill)&&!d.world.inheritedClipPlane,"Original shadow source raster/additional legacy clipping requires its exact unsupported consumer");
  for(const auto& m:{d.modelView,d.projection})for(float x:m)Require(std::isfinite(x),"Original shadow matrix is not finite");
  for(const auto& plane:d.clipPlanes)for(float x:plane)Require(std::isfinite(x),"Original shadow clip equation is not finite");
  for(float x:d.alphaControl)Require(std::isfinite(x),"Original shadow alpha-control uniform is not finite");
  const auto expected=d.pieceGeometry?NativeEngineShadowProfile::Projectile:(d.modern?NativeEngineShadowProfile::ModernModel:NativeEngineShadowProfile::LegacyModel);
  const auto profile=impl->programs.find(d.compiledOriginal.get());Require(profile!=impl->programs.end()&&profile->second->profile==expected&&profile->second->target.colors==target.colors&&profile->second->target.depth==target.depth&&profile->second->target.samples==target.samples,"Original shadow source program does not belong to this real compiler/target");
  // These exact original ShadowGen stages store no fragment color. The
  // accepted Mesa source lowers fixed alpha-test only at color store_output
  // instructions, so it emits no alpha-test epilogue for these stages. Keep
  // the captured enable/reference intact; do not invent or sample an alpha.
  // GL4's explicit alphaControl discard still executes in its own stage.
  Require(d.modern||std::none_of(d.world.colorWrite.begin(),d.world.colorWrite.end(),[](bool v){return v;}),"Original legacy ShadowGen has no defined fragment color for enabled color writes");Require(d.raster.observed&&d.raster.viewport,"Original shadow draw lost its actual viewport/scissor");
  PreparedEngineShadow::Impl::Draw draw;draw.sequence=d.sequence;draw.state=d.world;draw.pipeline=profile->second->pipeline;lease.Retain(draw.pipeline);draw.depth=effectstate::Depth(lease,d.world,target.stencil);
  draw.shadowPolygonOffsetUnitsScale=target.shadowPolygonOffsetUnitsScale;
  auto camera=target.camera;const auto& viewport=*d.raster.viewport;Require(viewport.width<=INT32_MAX&&viewport.height<=INT32_MAX,"Original shadow viewport exceeds source GLsizei");camera.viewport={viewport.x,viewport.y,int32_t(viewport.width),int32_t(viewport.height)};
  if(viewport.width&&viewport.height)draw.raster=effectstate::Viewport(camera,target.width,target.height,target.logicalWidth,target.logicalHeight,d.raster.scissor);
  auto common=Snapshot(*result->impl->source,d,target);
  if(d.pieceGeometry){
   const auto& source=*d.pieceGeometry;
   Require(d.firstIndex<=source.indices.size()&&d.indexCount<=source.indices.size()-d.firstIndex&&d.indexCount&&d.indexCount%3==0,"Original projectile shadow index range differs from its immutable geometry");
   for(size_t n=d.firstIndex;n<size_t(d.firstIndex)+d.indexCount;++n)Require(source.indices[n]<source.vertices.size(),"Original projectile shadow index exceeds actual vertices");
   WorldMatrix identity{};identity[0]=identity[5]=identity[10]=identity[15]=1;common.piece=Matrix(identity);
   auto& cached=impl->projectileGeometry;
   if(cached.owner!=d.pieceGeometry){
    EngineDeferredPieceBuffers next;
    next.vertices=Buffer(lease.Device(),source.vertices.data(),source.vertices.size()*sizeof(WorldModelVertex),@"Original projectile shadow vertices");
    next.indices=Buffer(lease.Device(),source.indices.data(),source.indices.size()*sizeof(uint32_t),@"Original projectile/shatter shadow indices");
    cached={d.pieceGeometry,std::move(next)};
   }
   Require(cached.buffers.vertices.device==lease.Device()&&cached.buffers.indices.device==lease.Device(),"Original projectile geometry belongs to another actual device");
   auto mesh=cached.buffers;mesh.indexCount=d.indexCount;
   auto bytes=Buffer(lease.Device(),&common,sizeof(common),@"Original projectile shadow draw uniforms");lease.AddAllocation(bytes);lease.AddAllocation(mesh.vertices);lease.AddAllocation(mesh.indices);
   auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=2;descriptor.initializeBindings=YES;NSError* error=nil;
   auto table=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(table,"Cannot create original projectile shadow argument table: "+Error(error));lease.Retain(table);
   [table setAddress:mesh.vertices.gpuAddress atIndex:0];[table setAddress:bytes.gpuAddress atIndex:1];draw.pieces.push_back({mesh,table,d.firstIndex});result->impl->draws.push_back(std::move(draw));continue;
  }
  if(d.modern){const auto& sampling=Sampling(d.alphaTexture);common.swizzle0=Vec(sampling.swizzle);common.samplerParameters.x=sampling.lodBias;common.image0.z=sampling.depthMode;
   if(!Empty(d.alphaTexture)){draw.image=images(lease,d.alphaTexture,0);Require(draw.image.texture&&draw.image.sampler&&draw.image.owner&&draw.image.requireReadable,"Original shadow alpha mask has no actual image/sampler/producer");Require(draw.image.texture.textureType==MTLTextureType2D&&(!draw.image.depthImage||sampling.compareMode==0),"Original shadow sampler2D alpha mask target/comparison differs");common.image0.x=draw.image.depthImage?2:1;common.image0.y=draw.image.flipV;lease.AddAllocation(draw.image.texture);lease.Retain(draw.image.sampler);}
  }
  if(d.modern)draw.pipeline=impl->ModelPipeline(lease,*profile->second,common.image0.x,validation,ms);
  auto [entry,created]=meshes.try_emplace(d.object->model.get());if(created)entry->second=geometry(lease,d.object->model);Require(entry->second.size()==d.object->model->pieces.size(),"Original shadow shared geometry piece count differs");
  for(size_t p=0;p<entry->second.size();++p){if(!d.modern&&!d.object->pieces[p].visible)continue;const auto& mesh=entry->second[p];if(!mesh.indexCount)continue;Require(mesh.vertices&&mesh.indices,"Original shadow shared geometry backing missing");auto uniform=common;
   if(d.modern){const auto index=size_t(d.pose.instance[0])+2*(1+p);engineoriginal::Transform(*d.pose.transforms,index,uniform.piece0q,uniform.piece0t);engineoriginal::Transform(*d.pose.transforms,index+1,uniform.piece1q,uniform.piece1t);}else uniform.piece=Matrix(d.object->pieces[p].modelSpace);
   auto bytes=Buffer(lease.Device(),&uniform,sizeof(uniform),@"Original immutable ShadowGen uniforms");lease.AddAllocation(bytes);lease.AddAllocation(mesh.vertices);lease.AddAllocation(mesh.indices);
   auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=2;descriptor.maxTextureBindCount=2;descriptor.maxSamplerStateBindCount=1;descriptor.initializeBindings=YES;NSError* error=nil;auto table=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(table,"Cannot create original shadow argument table: "+Error(error));lease.Retain(table);[table setAddress:mesh.vertices.gpuAddress atIndex:0];[table setAddress:bytes.gpuAddress atIndex:1];
   if(draw.image.texture){[table setTexture:draw.image.texture.gpuResourceID atIndex:draw.image.depthImage?1:0];[table setSamplerState:draw.image.sampler.gpuResourceID atIndex:0];}draw.pieces.push_back({mesh,table});
  }result->impl->draws.push_back(std::move(draw));
 }return result;
}
std::shared_ptr<PreparedEngineShadow> EngineShadowGPU::PrepareTerrain(RuntimeLease& lease,
 std::shared_ptr<const NativeTerrainShadowFrame> source,const CUSSourceTarget& target,const CUSSourceImageProvider& images)
{
 Target(target);
 Require(source&&source->drawInvoked&&source->contextGeneration&&source->sourceDrawFrame&&images&&
  source->camera.clipDepth!=ClipDepthConvention::Unsupported&&target.width&&target.height&&target.logicalWidth&&target.logicalHeight,
  "Original MAP shadow has no actual source/context/target/image service");
 Require(source->drawEntered||source->draws.empty(),"Original MAP shadow early return emitted geometry");
 auto result=std::shared_ptr<PreparedEngineShadow>(new PreparedEngineShadow);result->impl->terrainSource=std::move(source);
 uint64_t previous=0;
 for(const auto& d:result->impl->terrainSource->draws){
  const auto program=impl->programs.find(d.compiledOriginal.get());
  Require(program!=impl->programs.end()&&program->second->profile==NativeEngineShadowProfile::Terrain&&
   program->second->target.colors==target.colors&&program->second->target.depth==target.depth&&
   program->second->target.stencil==target.stencil&&program->second->target.samples==target.samples&&
   program->second->pipeline.device==lease.Device(),
   "Original MAP shadow lost its actual compiled program/target owner");
  Require(d.sequence>previous,"Original MAP shadow submissions are repeated or reordered");previous=d.sequence;
  const size_t count=d.indexed?d.indices.size():d.vertices.size();
  Require(count&&count%3==0&&count<=UINT32_MAX&&!d.vertices.empty()&&(d.indexed||d.indices.empty()),
   "Original MAP shadow has a different triangle invocation");
  for(uint32_t index:d.indices)Require(index<d.vertices.size(),"Original MAP shadow retained index exceeds its source vertices");
  for(const auto& vertex:d.vertices)for(float value:vertex)Require(std::isfinite(value),"Original MAP shadow position is nonfinite");
  Require(d.world.frontPolygonMode==d.world.backPolygonMode&&d.world.frontPolygonMode!=NativePolygonMode::Point&&
   !d.world.depthClamp&&!d.world.lineStipple&&d.world.lineWidth==1&&!d.world.inheritedClipPlane&&d.surface.viewport,
   "Original MAP shadow raster state has no matching consumer");
  Require(std::none_of(d.world.colorWrite.begin(),d.world.colorWrite.end(),[](bool value){return value;}),
   "Original MAP ShadowGen has no defined fragment color for enabled writes");
  for(const auto& matrix:{d.modelView,d.projection})for(float x:matrix)Require(std::isfinite(x),"Original MAP shadow matrix is nonfinite");
  for(const auto& plane:d.clipPlanes)for(float x:plane)Require(std::isfinite(x),"Original MAP shadow clip equation is nonfinite");
  Require(d.heightTexture.knowledge==NativeCUSBindingKnowledge::BoundObject&&d.heightTexture.object&&
   d.heightTexture.object->contextGeneration==result->impl->terrainSource->contextGeneration&&
   d.heightTexture.object->target==NativeCUSTextureTarget::Image2D,"Original MAP shadow height image lost its captured source object");
  const auto& sampling=d.heightTexture.object->sampling;
  // The actual SMF height allocation is level0. Preserve its explicit textureLod
  // semantics; do not silently ignore another source base/clamped level.
  Require(sampling.compareMode==0&&sampling.baseLevel==0&&sampling.maxLevel>=0&&sampling.minLod<=0&&sampling.maxLod>=0&&sampling.lodBias==0,
   "Original MAP shadow height requires its exact nonzero-base/LOD/comparison consumer");
  // The shared image provider uses NativeUITextureHistory::DescribeSampler.
  // Reject source parameters that it would otherwise truncate or quantize.
  Require(std::isfinite(sampling.anisotropy)&&sampling.anisotropy>=1&&sampling.anisotropy<=16&&
   sampling.anisotropy==std::floor(sampling.anisotropy),"Original MAP shadow height anisotropy has no exact shared sampler");
  const bool usesBorder=sampling.wrapS==0x812D||sampling.wrapS==0x2900||sampling.wrapT==0x812D||sampling.wrapT==0x2900;
  Require(!usesBorder||sampling.borderColor==std::array<float,4>{0,0,0,0}||sampling.borderColor==std::array<float,4>{1,1,1,1},
   "Original MAP shadow height border color has no exact shared sampler");
  const auto channel=sampling.swizzle[0];Require(channel==0||channel==1||(channel>=0x1903&&channel<=0x1906),"Original MAP shadow height swizzle is invalid");
  PreparedEngineShadow::Impl::Draw draw;draw.sequence=d.sequence;draw.state=d.world;
  draw.shadowPolygonOffsetUnitsScale=target.shadowPolygonOffsetUnitsScale;
  draw.pipeline=program->second->pipeline;lease.Retain(draw.pipeline);draw.depth=effectstate::Depth(lease,d.world,target.stencil);
  auto camera=target.camera;const auto& viewport=*d.surface.viewport;
  Require(viewport.width<=INT32_MAX&&viewport.height<=INT32_MAX,"Original MAP shadow viewport exceeds GLsizei");
  camera.viewport={viewport.x,viewport.y,int32_t(viewport.width),int32_t(viewport.height)};
  if(viewport.width&&viewport.height)draw.raster=effectstate::Viewport(camera,target.width,target.height,target.logicalWidth,target.logicalHeight,d.surface.scissor);
  draw.image=images(lease,d.heightTexture,1);
  Require(draw.image.texture&&draw.image.sampler&&draw.image.owner&&draw.image.requireReadable&&
   draw.image.texture.device==lease.Device()&&draw.image.sampler.device==lease.Device()&&draw.image.texture.sampleCount==1&&
   draw.image.texture.textureType==MTLTextureType2D&&!draw.image.depthImage,
   "Original MAP shadow height has no actual retained color image/sampler/producer");
  lease.AddAllocation(draw.image.texture);lease.Retain(draw.image.sampler);
  TerrainShadowUniform uniform{};uniform.modelView=Matrix(d.modelView);uniform.projection=Matrix(d.projection);uniform.mapSize=Vec(d.mapSize);
  uniform.parameters={float(d.texSquare[0]),float(d.texSquare[1]),d.borderMinHeight,0};
  for(size_t plane=0;plane<3;++plane)uniform.clips[plane]=d.clipEnabled[plane]?Vec(d.clipPlanes[plane]):simd_float4{0,0,0,1};
  uniform.flags={uint32_t(result->impl->terrainSource->camera.clipDepth==ClipDepthConvention::MinusOneToOne),uint32_t(target.invertClipY),uint32_t(draw.image.flipV),channel};
  EngineDeferredPieceBuffers mesh;mesh.vertices=GenericBuffer(lease,d.vertices.data(),d.vertices.size(),sizeof(d.vertices[0]),@"Original MAP shadow vertices");
  if(d.indexed)mesh.indices=GenericBuffer(lease,d.indices.data(),d.indices.size(),sizeof(uint32_t),@"Original MAP shadow indices");
  mesh.indexCount=uint32_t(count);auto bytes=GenericBuffer(lease,&uniform,1,sizeof(uniform),@"Original immutable MAP ShadowGen uniforms");
  auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=2;descriptor.maxTextureBindCount=1;descriptor.maxSamplerStateBindCount=1;descriptor.initializeBindings=YES;NSError* error=nil;
  auto table=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(table,"Cannot create original MAP shadow arguments: "+Error(error));lease.Retain(table);
  [table setAddress:mesh.vertices.gpuAddress atIndex:0];[table setAddress:bytes.gpuAddress atIndex:1];
  [table setTexture:draw.image.texture.gpuResourceID atIndex:0];[table setSamplerState:draw.image.sampler.gpuResourceID atIndex:0];
  draw.pieces.push_back({mesh,table,0,d.indexed});result->impl->draws.push_back(std::move(draw));
 }
 return result;
}
std::shared_ptr<PreparedGenericShadow> EngineShadowGPU::PrepareGeneric(RuntimeLease& lease,std::shared_ptr<const NativeWorldDrawFrame> source,uint64_t frame,uint64_t context,const CUSSourceTarget& target,bool validation,double& milliseconds)
{
	Require(source&&source->serial&&frame&&context,"Original generic shadow lost its common source frame/context");
	if(std::none_of(source->draws.begin(),source->draws.end(),[](const auto& draw){return draw.stage==NativeWorldDrawStage::ShadowOpaque;}))return {};
	Target(target);Require(target.width&&target.height&&target.logicalWidth&&target.logicalHeight,"Original generic shadow target lost its actual extent");
	Require(std::isfinite(target.shadowPolygonOffsetUnitsScale)&&target.shadowPolygonOffsetUnitsScale>0,"Original generic shadow bias lost its actual destination scale");
	auto result=std::shared_ptr<PreparedGenericShadow>(new PreparedGenericShadow);result->impl->source=std::move(source);uint64_t previous=0;
	for(const auto& command:result->impl->source->draws){
		if(command.stage!=NativeWorldDrawStage::ShadowOpaque)continue;
		ValidateNativeWorldDrawCommand(command);
		Require(command.sequence>previous&&command.originalShadowInput,"Original generic shadow lost its existing command ordinal/raw input");previous=command.sequence;
		const auto& input=*command.originalShadowInput;
		Require(input.sourceDrawFrame==frame&&input.contextGeneration==context&&input.vertices.size()==command.vertices.size(),"Original generic shadow raw input belongs to another actual frame/context/stream");
		const auto selected=impl->programs.find(input.compiledOriginal.get());
		Require(selected!=impl->programs.end()&&!selected->second->modern&&(selected->second->profile==NativeEngineShadowProfile::LegacyModel||selected->second->profile==NativeEngineShadowProfile::Projectile),"Original generic shadow selected another program or input ABI");
		const auto& program=*selected->second;
		Require(program.target.colors==target.colors&&program.target.depth==target.depth&&program.target.stencil==target.stencil&&program.target.samples==target.samples&&program.pipeline.device==lease.Device(),"Original generic shadow selected program differs from its actual compiler/target");
		Require(command.primitive==NativeDrawPrimitive::Triangles&&command.world.frontPolygonMode==NativePolygonMode::Fill&&command.world.backPolygonMode==NativePolygonMode::Fill&&!command.world.inheritedClipPlane,"Original generic shadow topology/raster/extra clipping requires its exact consumer");
		Require(command.world.cull<=NativeWorldCull::FrontAndBack,"Original generic shadow cull mode is unsupported");
		Require(!command.surface.alphaTest.enabled,"Original generic shadow fixed alpha requires its actual selected-program lowering");
		Require(std::none_of(command.world.colorWrite.begin(),command.world.colorWrite.end(),[](bool value){return value;}),"Original generic ShadowGen has no defined fragment color for enabled color writes");
		Require(!command.world.stencilTest&&command.surface.viewport.has_value(),"Original generic shadow has no stencil attachment or actual viewport observation");
		for(const auto& matrix:{input.modelView,input.projection})for(float value:matrix)Require(std::isfinite(value),"Original generic shadow matrix is nonfinite");
		for(const auto& plane:input.clipPlanes)for(float value:plane)Require(std::isfinite(value),"Original generic shadow clip equation is nonfinite");
		for(const auto& vertex:input.vertices)for(const auto& component:{vertex.position,vertex.normal,vertex.color,vertex.uv})for(float value:component)Require(std::isfinite(value),"Original generic shadow vertex attribute is nonfinite");
		PreparedGenericShadow::Impl::Draw draw;draw.sequence=command.sequence;draw.state=command.world;draw.shadowPolygonOffsetUnitsScale=target.shadowPolygonOffsetUnitsScale;
		const auto& viewport=*command.surface.viewport;Require(viewport.width<=INT32_MAX&&viewport.height<=INT32_MAX,"Original generic shadow viewport exceeds source GLsizei");
		auto camera=target.camera;camera.viewport={viewport.x,viewport.y,int32_t(viewport.width),int32_t(viewport.height)};
		if(viewport.width&&viewport.height)draw.raster=effectstate::Viewport(camera,target.width,target.height,target.logicalWidth,target.logicalHeight,command.surface.scissor);
		GenericShadowUniform uniform{};uniform.modelView=Matrix(input.modelView);uniform.projection=Matrix(input.projection);
		uniform.flags.x=unsigned(!input.zeroToOneDepth)|(unsigned(target.invertClipY)<<1);
		for(unsigned plane=0;plane<input.clipPlanes.size();++plane){uniform.clips[plane]=Vec(input.clipPlanes[plane]);if(input.clipEnabled[plane])uniform.flags.y|=1u<<plane;}
		draw.pipeline=impl->GenericPipeline(lease,target,validation,milliseconds);lease.Retain(draw.pipeline);lease.Retain(impl->library);
		draw.depth=effectstate::Depth(lease,command.world,target.stencil);
		draw.vertices=GenericBuffer(lease,input.vertices.data(),input.vertices.size(),sizeof(NativeGenericShadowVertex),@"Original generic shadow raw vertices");
		draw.indices=GenericBuffer(lease,command.indices.data(),command.indices.size(),sizeof(uint32_t),@"Original generic shadow canonical indices");
		draw.uniforms=GenericBuffer(lease,&uniform,1,sizeof(uniform),@"Original generic shadow source uniforms");draw.indexCount=command.indices.size();
		auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=2;descriptor.initializeBindings=YES;NSError* error=nil;
		draw.arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(draw.arguments!=nil,"Cannot create original generic shadow argument table: "+Error(error));lease.Retain(draw.arguments);
		[draw.arguments setAddress:draw.vertices.gpuAddress atIndex:0];[draw.arguments setAddress:draw.uniforms.gpuAddress atIndex:1];
		result->impl->draws.push_back(std::move(draw));
	}
	return result;
}
} // namespace rendering::metal4
