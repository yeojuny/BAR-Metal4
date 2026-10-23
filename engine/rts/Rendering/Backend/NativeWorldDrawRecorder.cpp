/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeWorldDrawRecorder.h"
#include "NativeEasyFacingProgram.h"
#include "NativeSquadHullProgram.h"
#include "NativeTerraformOverlayProgram.h"
#include "NativeLOSViewProgram.h"
#include "NativeWaterOverlayProgram.h"
#include "NativeSplatPainterProgram.h"
#include "NativeUnitEffectsData.h"
#include "NativeWorldDrawSequence.h"
#include "NativeGenericShadowData.h"
#include "NativeReclaimFieldData.h"
#include "NativeDistortionData.h"
#include "NativeObjectSpotlightData.h"
#include "NativeGrassData.h"
#include "NativeResourceSpotsData.h"
#include "NativeSensorLOSData.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rendering {
namespace {
void Require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
bool Finite(float value){return std::isfinite(value);}
}
void ValidateNativeWorldDrawCommand(const NativeWorldDrawCommand& c)
{
 if(c.surface.splatPainter){
  const auto& input=*c.surface.splatPainter;ValidateNativeSplatPainter(input);
  Require(input.program==NativeSplatPainterProgram::Overlay&&c.stage==NativeWorldDrawStage::World&&
   !c.waterOverlay&&!c.losView&&!c.terraformOverlay&&!c.squadHull&&!c.originalShadowInput&&!c.easyFacing&&!c.unitEffect&&!c.distortion&&!c.objectSpotlight&&!c.grass&&!c.resourceSpots&&!c.sensorLOS&&!c.reclaimField,
   "Splat Painter world overlay command ownership/stage differs");
  Require(c.primitive==NativeDrawPrimitive::Triangles&&c.sourcePrimitive==NativeDrawPrimitive::Triangles&&c.surface.material==ScreenMaterial::RGBA&&c.surface.texture==input.images[0].texture&&!c.surface.secondary.texture&&
   c.world.frontPolygonMode==NativePolygonMode::Fill&&c.world.backPolygonMode==NativePolygonMode::Fill&&!c.world.inheritedClipPlane,
   "Splat Painter original overlay triangles/image/raster differ");
 }

 if(c.waterOverlay){
  ValidateNativeWaterOverlay(*c.waterOverlay);
  Require(c.stage==(c.waterOverlay->fog?NativeWorldDrawStage::World:NativeWorldDrawStage::PreUnit)&&!c.losView&&!c.terraformOverlay&&!c.squadHull&&!c.originalShadowInput&&!c.easyFacing&&!c.unitEffect&&!c.distortion&&!c.objectSpotlight&&!c.grass&&!c.resourceSpots&&!c.sensorLOS&&!c.reclaimField,"Water Overlay original command ownership/stage differs");
  Require(c.primitive==NativeDrawPrimitive::Triangles&&c.sourcePrimitive==NativeDrawPrimitive::Triangles&&c.world.frontPolygonMode==NativePolygonMode::Fill&&c.world.backPolygonMode==NativePolygonMode::Fill&&!c.world.inheritedClipPlane,"Water Overlay original triangle raster contract differs");
 }
 if(c.losView){
  ValidateNativeLOSView(*c.losView);
  Require(c.stage==NativeWorldDrawStage::PreDecals&&!c.terraformOverlay&&!c.squadHull&&!c.originalShadowInput&&!c.easyFacing&&!c.unitEffect&&!c.distortion&&!c.objectSpotlight&&!c.grass&&!c.resourceSpots&&!c.sensorLOS&&!c.reclaimField,"LOS View command owner/callback differs");
  Require(c.primitive==NativeDrawPrimitive::Triangles&&c.sourcePrimitive==NativeDrawPrimitive::Triangles&&c.vertices.size()==6&&c.surface.material==ScreenMaterial::RGBA&&c.surface.texture&&c.surface.secondary.texture&&c.surface.secondary.texture->postprocessVersion,"LOS View original fullscreen/depth/screen-copy contract differs");
 }
 if(c.terraformOverlay){
  ValidateNativeTerraformOverlay(*c.terraformOverlay);
  Require(c.stage==NativeWorldDrawStage::PreUnit&&!c.squadHull&&!c.originalShadowInput&&!c.easyFacing&&!c.unitEffect&&!c.distortion&&!c.objectSpotlight&&!c.grass&&!c.resourceSpots&&!c.sensorLOS&&!c.reclaimField,"Terraform Overlay command owner/callback differs");
  Require(c.primitive==NativeDrawPrimitive::Triangles&&c.sourcePrimitive==NativeDrawPrimitive::Triangles&&c.vertices.size()==6&&c.surface.material==ScreenMaterial::RGBA&&c.surface.texture&&c.surface.secondary.texture,"Terraform Overlay original fullscreen/sampler contract differs");
 }
 if(c.squadHull){
  ValidateNativeSquadHull(*c.squadHull);
  Require(c.stage==NativeWorldDrawStage::PreUnit&&!c.originalShadowInput&&!c.easyFacing&&!c.unitEffect&&!c.distortion&&!c.objectSpotlight&&!c.grass&&!c.resourceSpots&&!c.sensorLOS&&!c.reclaimField,"Squad Hull command owner/callback differs");
  Require((c.sourcePrimitive==NativeDrawPrimitive::TriangleFan&&c.primitive==NativeDrawPrimitive::Triangles)||(c.sourcePrimitive==NativeDrawPrimitive::LineLoop&&c.primitive==NativeDrawPrimitive::Lines),"Squad Hull original fill/border topology differs");
  Require(c.vertices.size()>=3&&c.vertices.size()<=512&&c.surface.material==ScreenMaterial::Solid&&!c.surface.texture&&!c.surface.secondary.texture,"Squad Hull original capacity/sampler contract differs");
 }
	if(c.world.inheritedClipPlane)for(float value:*c.world.inheritedClipPlane)Require(std::isfinite(value),"Non-finite inherited original clip plane");
	Require(c.stage<=NativeWorldDrawStage::TerrainReflection,"Native world draw stage unsupported");
 if(c.unitEffect){ValidateNativeUnitEffect(*c.unitEffect);Require(c.primitive==NativeDrawPrimitive::Triangles&&c.sourcePrimitive==NativeDrawPrimitive::Triangles&&c.vertices.empty()&&c.indices.empty()&&!c.easyFacing&&!c.distortion&&!c.objectSpotlight&&!c.grass&&!c.resourceSpots&&!c.sensorLOS&&!c.reclaimField&&!c.originalShadowInput&&(c.unitEffect->paralyze?c.stage==NativeWorldDrawStage::World:(c.unitEffect->reflectionPass?c.stage==NativeWorldDrawStage::WaterReflection:(c.unitEffect->orb?(c.stage==NativeWorldDrawStage::AlphaAboveWater||c.stage==NativeWorldDrawStage::AlphaAll):c.stage==NativeWorldDrawStage::World))),"Airjets lost original stage/buffer ownership");}
 if(c.easyFacing){ValidateNativeEasyFacing(*c.easyFacing);Require((c.stage==NativeWorldDrawStage::PreUnit||c.stage==NativeWorldDrawStage::World)&&!c.distortion&&!c.objectSpotlight&&!c.grass&&!c.resourceSpots&&!c.sensorLOS&&!c.reclaimField&&!c.originalShadowInput&&c.sourcePrimitive==NativeDrawPrimitive::Quads&&c.primitive==NativeDrawPrimitive::Triangles&&c.vertices.size()==4&&c.indices.size()==6&&c.surface.material==ScreenMaterial::Solid&&!c.surface.texture&&!c.surface.secondary.texture,"Easy Facing original stage/quad ownership differs");}
 if(c.distortion){Require(c.stage==NativeWorldDrawStage::World&&!c.sensorLOS&&!c.resourceSpots&&!c.reclaimField&&!c.originalShadowInput&&c.vertices.empty()&&c.indices.empty()&&c.sequence==c.distortion->sequence,"Distortion typed command lost its actual source stage/ordinal");ValidateNativeDistortionCommand(*c.distortion);return;}

	Require(c.primitive==NativeDrawPrimitive::Triangles || c.primitive==NativeDrawPrimitive::Lines || c.primitive==NativeDrawPrimitive::Points,"Native world topology must be canonical");
	const unsigned group=c.primitive==NativeDrawPrimitive::Triangles?3:c.primitive==NativeDrawPrimitive::Lines?2:1;
 if(c.objectSpotlight){
  ValidateNativeObjectSpotlightInput(*c.objectSpotlight);
  Require(c.stage==NativeWorldDrawStage::World&&!c.reclaimField&&!c.originalShadowInput&&c.primitive==NativeDrawPrimitive::Triangles&&c.sourcePrimitive==NativeDrawPrimitive::TriangleStrip,"Object Spotlight source command stage/topology differs");
 }
 if(c.sensorLOS){
  ValidateNativeSensorLOSDraw(*c.sensorLOS);
  Require(c.stage==NativeWorldDrawStage::World&&!c.sensorLOS->program.defines->stencilPass&&!c.resourceSpots&&!c.grass&&!c.objectSpotlight&&!c.distortion&&!c.reclaimField&&!c.originalShadowInput&&c.primitive==NativeDrawPrimitive::Lines&&c.sourcePrimitive==NativeDrawPrimitive::LineLoop&&c.sequence==c.sensorLOS->sequence,"LOS final outline lost its original callback/sequence/topology");
 }
 if(c.resourceSpots){
  ValidateNativeResourceSpotsInput(*c.resourceSpots);
  const bool metal=c.resourceSpots->kind==NativeResourceSpotKind::Metal;
  Require((c.stage==NativeWorldDrawStage::World||(metal&&c.stage==NativeWorldDrawStage::PreUnit))&&!c.grass&&!c.objectSpotlight&&!c.distortion&&!c.reclaimField&&!c.originalShadowInput&&c.primitive==NativeDrawPrimitive::Triangles&&c.sourcePrimitive==NativeDrawPrimitive::Triangles,"Resource spot source stage/topology/ownership differs");
 }
 if(c.grass){
  ValidateNativeGrassInput(*c.grass);
  Require(c.stage==NativeWorldDrawStage::PreUnit&&!c.originalShadowInput&&!c.reclaimField&&c.primitive==NativeDrawPrimitive::Triangles&&c.sourcePrimitive==NativeDrawPrimitive::Triangles,"Original Grass source stage/topology/ownership differs");
 }
 if(c.reclaimField){
  ValidateNativeReclaimFieldInput(*c.reclaimField);
  Require(c.stage==NativeWorldDrawStage::PreUnit&&!c.originalShadowInput&&c.primitive==c.sourcePrimitive&&c.sourcePrimitive==(c.reclaimField->edges?NativeDrawPrimitive::Lines:NativeDrawPrimitive::Triangles),"Reclaim field source command stage/topology differs");
 }
 if(c.reclaimField&&c.reclaimField->edges)Require(c.vertices.size()==c.reclaimField->count&&c.indices.size()==c.reclaimField->count,"Reclaim outline lost its actual original vertex interval");
 const bool reclaimFill=c.reclaimField&&!c.reclaimField->edges;
 if(reclaimFill||c.objectSpotlight||c.grass||c.resourceSpots||c.sensorLOS||c.unitEffect)Require(c.vertices.empty()&&c.indices.empty(),"Typed original-buffer draw cannot carry replacement geometry");
 else {
	Require(!c.vertices.empty() && c.vertices.size()<=4*1024*1024 && !c.indices.empty() && c.indices.size()%group==0,"Native world geometry extent invalid");
	for(const auto& v:c.vertices) {
		for(float x:v.position)Require(Finite(x),"Native world clip vertex non-finite");
		for(float x:v.color)Require(Finite(x),"Native world color non-finite");
		for(float x:v.uv)Require(Finite(x),"Native world UV non-finite");
	}
	for(auto i:c.indices)Require(i<c.vertices.size(),"Native world index out of range");
 }
	Require(Finite(c.world.lineWidth)&&c.world.lineWidth>0&&Finite(c.world.pointSize)&&c.world.pointSize>0,"Native world raster size invalid");
	Require(Finite(c.world.offsetFactor)&&Finite(c.world.offsetUnits),"Native world polygon offset invalid");
	Require(c.world.frontPolygonMode<=NativePolygonMode::Fill && c.world.backPolygonMode<=NativePolygonMode::Fill,"Native polygon mode invalid");
	Require(c.world.stippleFactor>=1&&c.world.stippleFactor<=256,"Native world stipple factor invalid");
	if(c.originalShadowInput){
		Require(c.stage==NativeWorldDrawStage::ShadowOpaque&&c.originalShadowInput->compiledOriginal&&c.originalShadowInput->sourceDrawFrame&&c.originalShadowInput->contextGeneration&&c.originalShadowInput->vertices.size()==c.vertices.size(),"Native generic shadow command lost its actual raw source input");
	}else{
		Require(c.surface.material==ScreenMaterial::Solid || c.surface.material==ScreenMaterial::RGBA || c.surface.material==ScreenMaterial::FontCoverage,"Native world material unsupported");
		Require(c.surface.material==ScreenMaterial::Solid || c.surface.texture!=nullptr,"Native world texture has no real image");
		ValidateScreenSampler(c.surface.sampler);ValidateScreenSampler(c.surface.secondary.sampler);
		Require(c.surface.secondary.uvMode<=ScreenUVMode::Texel,"Native world secondary UV mode unsupported");
	}
}
NativeWorldDrawRecorder& GetWorldDrawRecorder(){static NativeWorldDrawRecorder value;return value;}
void NativeWorldDrawRecorder::BeginFrame()
{
	Require(!frame,"Native world frame already active");
	Require(serial<std::numeric_limits<std::uint64_t>::max(),"Native world serial exhausted");
	auto next=std::make_shared<NativeWorldDrawFrame>();next->serial=++serial;frame=std::move(next);
}
std::shared_ptr<const NativeWorldDrawFrame> NativeWorldDrawRecorder::EndFrame()
{
	Require(bool(frame),"Native world End without frame");
	return std::exchange(frame,{});
}
std::shared_ptr<const NativeWorldDrawFrame> NativeWorldDrawRecorder::SnapshotPrefix(std::uint64_t cutoff) const
{
	Require(frame && cutoff,"Native world prefix requires its active recorder and original cutoff");
	auto prefix=std::make_shared<NativeWorldDrawFrame>(*frame);
	std::erase_if(prefix->draws,[&](const auto& draw){return draw.sequence>=cutoff;});
	std::erase_if(prefix->clears,[&](const auto& clear){return clear.sequence>=cutoff;});
	return prefix;
}
void NativeWorldDrawRecorder::AbortFrame() noexcept {frame.reset();}
void NativeWorldDrawRecorder::Reset() noexcept {AbortFrame();}
NativeWorldDrawCommand MakeNativeWorldDrawCommand(NativeWorldDrawStage stage,NativeDrawPrimitive mode,
	std::span<const NativeWorldDrawVertex> vertices,const ScreenDrawState& surface,const NativeWorldDrawState& world,
	const std::function<std::shared_ptr<const NativeGenericShadowInput>()>& snapshotShadowInput)
{
	Require(vertices.size()<=4*1024*1024,"Native world bounded geometry capacity exceeded");
	NativeWorldDrawCommand c;c.stage=stage;c.sourcePrimitive=mode;c.surface=surface;c.world=world;
	c.vertices.assign(vertices.begin(),vertices.end());
	const auto append=[&](std::initializer_list<std::uint32_t> values){c.indices.insert(c.indices.end(),values);};
	const auto n=std::uint32_t(vertices.size());
	switch(mode) {
		case NativeDrawPrimitive::Triangles:
			c.primitive=NativeDrawPrimitive::Triangles;for(std::uint32_t i=0;i+2<n;i+=3)append({i,i+1,i+2});break;
		case NativeDrawPrimitive::TriangleStrip:
			c.primitive=NativeDrawPrimitive::Triangles;for(std::uint32_t i=0;i+2<n;++i)if(i&1)append({i+1,i,i+2});else append({i,i+1,i+2});break;
		case NativeDrawPrimitive::TriangleFan:
			c.primitive=NativeDrawPrimitive::Triangles;for(std::uint32_t i=1;i+1<n;++i)append({0,i,i+1});break;
		case NativeDrawPrimitive::Polygon:
			// GL_POLYGON is one convex polygon. The smooth fill is triangulated,
			// while sourcePrimitive retains its single outer boundary for line/point modes.
			c.primitive=NativeDrawPrimitive::Triangles;for(std::uint32_t i=1;i+1<n;++i)append({0,i,i+1});break;
		case NativeDrawPrimitive::Quads:
			c.primitive=NativeDrawPrimitive::Triangles;for(std::uint32_t i=0;i+3<n;i+=4)append({i,i+1,i+3,i+1,i+2,i+3});break;
		case NativeDrawPrimitive::Lines:
			c.primitive=NativeDrawPrimitive::Lines;for(std::uint32_t i=0;i+1<n;i+=2)append({i,i+1});break;
		case NativeDrawPrimitive::LineStrip:
		case NativeDrawPrimitive::LineLoop:
			c.primitive=NativeDrawPrimitive::Lines;for(std::uint32_t i=0;i+1<n;++i)append({i,i+1});if(mode==NativeDrawPrimitive::LineLoop && n>=2)append({n-1,0});break;
		case NativeDrawPrimitive::Points:
			c.primitive=NativeDrawPrimitive::Points;for(std::uint32_t i=0;i<n;++i)append({i});break;
		default:throw std::runtime_error("Native world primitive unsupported");
	}
	// As GL does, incomplete final primitive groups generate no fragments.
	if(!c.indices.empty()){
		if(snapshotShadowInput){c.originalShadowInput=snapshotShadowInput();Require(bool(c.originalShadowInput),"Native generic shadow source returned no actual input");}
		ValidateNativeWorldDrawCommand(c);
	}
	return c;
}
void NativeWorldDrawRecorder::Record(NativeWorldDrawStage stage,NativeDrawPrimitive mode,
	std::span<const NativeWorldDrawVertex> vertices,const ScreenDrawState& surface,const NativeWorldDrawState& world,
	const std::function<std::shared_ptr<const NativeGenericShadowInput>()>& snapshotShadowInput)
{
	Require(bool(frame),"Native world geometry outside frame");
	Require(frame->draws.size()<65536,"Native world bounded geometry capacity exceeded");
	auto c=MakeNativeWorldDrawCommand(stage,mode,vertices,surface,world,snapshotShadowInput);
	if(c.indices.empty())return;
	c.sequence=NextNativeWorldDrawSequence();
	frame->draws.push_back(std::move(c));
}
std::uint64_t NativeWorldDrawRecorder::RecordClear(NativeWorldDrawStage stage,ScreenAttachmentMask mask,
 const ScreenClearValues& values,const NativeWorldDrawState& world,std::optional<ScreenScissor> scissor)
{
 Require(bool(frame),"Native world attachment clear outside frame");
 const bool shadow=stage==NativeWorldDrawStage::ShadowOpaque;
 const bool water=stage==NativeWorldDrawStage::WaterReflection||stage==NativeWorldDrawStage::WaterRefraction;
 const bool terrain=stage==NativeWorldDrawStage::Ground||stage==NativeWorldDrawStage::TerrainReflection;
 Require(shadow||water||terrain,"Native world attachment clear has no selected source consumer for this stage");
 const unsigned bits=static_cast<unsigned>(mask);
 Require(!(bits&~unsigned((water||stage==NativeWorldDrawStage::Ground)?ScreenAttachmentAll:ScreenAttachmentColor|ScreenAttachmentDepth)),
  "Native world attachment clear mask has no actual selected target attachment");
 Require(frame->clears.size()<65536,"Native world bounded attachment clear capacity exceeded");
 for(float value:values.color)Require(Finite(value)&&value>=0&&value<=1,"Native world clear color is not the actual clamped source value");
 Require(Finite(values.depth)&&values.depth>=0&&values.depth<=1,"Native world clear depth is not the actual clamped source value");
 NativeWorldAttachmentClear clear;clear.stage=stage;clear.mask=mask;clear.values=values;
 clear.colorWrite=world.colorWrite;clear.combinedColorWrite=world.combinedColorWrite;clear.depthWrite=world.depthWrite;clear.frontStencilWriteMask=world.frontStencil.writeMask;clear.scissor=scissor;
 // No second allocator at the handler: typed draws and clears share this
 // one source sequence. Masked or zero-coverage clears retain their event.
 clear.sequence=NextNativeWorldDrawSequence();frame->clears.push_back(std::move(clear));
 return frame->clears.back().sequence;
}
void NativeWorldDrawRecorder::RecordDistortion(std::shared_ptr<const NativeDistortionCommand> input)
{
 Require(frame&&input&&frame->draws.size()<65536,"Distortion command is outside the existing common bounded world frame");
 ValidateNativeDistortionCommand(*input);NativeWorldDrawCommand command;command.stage=NativeWorldDrawStage::World;
 command.primitive=command.sourcePrimitive=NativeDrawPrimitive::Triangles;command.sequence=input->sequence;command.surface=input->surface;command.world=input->world;command.distortion=std::move(input);
 ValidateNativeWorldDrawCommand(command);frame->draws.push_back(std::move(command));
}
void NativeWorldDrawRecorder::RecordSensorLOS(std::shared_ptr<const NativeSensorLOSDraw> input)
{
 Require(frame&&input&&frame->draws.size()<65536,"LOS final draw is outside common frame/capacity");
 NativeWorldDrawCommand c;c.stage=NativeWorldDrawStage::World;c.primitive=NativeDrawPrimitive::Lines;c.sourcePrimitive=NativeDrawPrimitive::LineLoop;c.surface=input->surface;c.world=input->raster;c.sequence=input->sequence;
 c.surface.material=ScreenMaterial::Solid;c.surface.texture.reset();c.surface.secondary.texture.reset();c.surface.sampler={};c.surface.secondary.sampler={};
 c.sensorLOS=std::move(input);ValidateNativeWorldDrawCommand(c);frame->draws.push_back(std::move(c));
}
void NativeWorldDrawRecorder::RecordResourceSpots(std::shared_ptr<const NativeResourceSpotsInput> input,NativeWorldDrawStage stage,const ScreenDrawState& surface,const NativeWorldDrawState& world)
{
 Require(frame&&input&&frame->draws.size()<65536,"Resource spot draw is outside its common frame/capacity");
 NativeWorldDrawCommand c;c.stage=stage;c.primitive=c.sourcePrimitive=NativeDrawPrimitive::Triangles;c.surface=surface;c.world=world;
 // The typed input owns the exact height and generated text-atlas versions.
 c.surface.material=ScreenMaterial::Solid;c.surface.texture.reset();c.surface.secondary.texture.reset();c.surface.sampler={};c.surface.secondary.sampler={};
 c.resourceSpots=std::move(input);c.sequence=NextNativeWorldDrawSequence();ValidateNativeWorldDrawCommand(c);frame->draws.push_back(std::move(c));
}
void NativeWorldDrawRecorder::RecordGrass(std::shared_ptr<const NativeGrassInput> input,const ScreenDrawState& surface,const NativeWorldDrawState& world)
{
 Require(frame&&input&&frame->draws.size()<65536,"Original Grass draw is outside its common frame or exceeds existing draw capacity");ValidateNativeGrassInput(*input);
 NativeWorldDrawCommand c;c.stage=NativeWorldDrawStage::PreUnit;c.primitive=c.sourcePrimitive=NativeDrawPrimitive::Triangles;c.surface=surface;c.world=world;
 // The six actual sampled images live in the typed input. The common solid
 // marker prevents an unrelated currently-active texture becoming a second owner.
 c.surface.material=ScreenMaterial::Solid;c.surface.texture.reset();c.surface.secondary.texture.reset();c.surface.sampler={};c.surface.secondary.sampler={};
 c.grass=std::move(input);c.sequence=NextNativeWorldDrawSequence();ValidateNativeWorldDrawCommand(c);frame->draws.push_back(std::move(c));
}
void NativeWorldDrawRecorder::RecordReclaimField(std::shared_ptr<const NativeReclaimFieldInput> input,const ScreenDrawState& surface,const NativeWorldDrawState& world)
{
 Require(frame&&input,"Reclaim field source draw is outside the existing common frame");
 Require(frame->draws.size()<65536,"Reclaim field exceeds existing common draw capacity");ValidateNativeReclaimFieldInput(*input);
 auto effective=surface;effective.material=ScreenMaterial::Solid;effective.texture.reset();effective.secondary.texture.reset();
 effective.sampler={};effective.secondary.sampler={}; // Original family has no sampled inputs.
 NativeWorldDrawCommand c;
 if(input->edges){const auto vertices=EvaluateNativeReclaimFieldOutline(*input);c=MakeNativeWorldDrawCommand(NativeWorldDrawStage::PreUnit,NativeDrawPrimitive::Lines,vertices,effective,world);}
 else {c.stage=NativeWorldDrawStage::PreUnit;c.primitive=c.sourcePrimitive=NativeDrawPrimitive::Triangles;c.surface=effective;c.world=world;}
 c.reclaimField=std::move(input);c.sequence=NextNativeWorldDrawSequence();ValidateNativeWorldDrawCommand(c);frame->draws.push_back(std::move(c));
}
void NativeWorldDrawRecorder::RecordObjectSpotlight(std::shared_ptr<const NativeObjectSpotlightInput> input,const ScreenDrawState& surface,const NativeWorldDrawState& world)
{
 Require(frame&&input,"Object Spotlight source draw is outside the existing common frame");
 Require(frame->draws.size()<65536,"Object Spotlight exceeds existing common draw capacity");ValidateNativeObjectSpotlightInput(*input);
 auto effective=surface;effective.material=ScreenMaterial::Solid;effective.texture.reset();effective.secondary.texture.reset();
 effective.sampler={};effective.secondary.sampler={}; // Original vertex/fragment pair has no sampled inputs.
 NativeWorldDrawCommand c;c.stage=NativeWorldDrawStage::World;c.primitive=NativeDrawPrimitive::Triangles;c.sourcePrimitive=NativeDrawPrimitive::TriangleStrip;
 c.surface=effective;c.world=world;c.objectSpotlight=std::move(input);c.sequence=NextNativeWorldDrawSequence();
 ValidateNativeWorldDrawCommand(c);frame->draws.push_back(std::move(c));
}
} // namespace rendering

namespace rendering {
void NativeWorldDrawRecorder::RecordWaterOverlay(std::span<const NativeWorldDrawVertex> vertices,const ScreenDrawState& surface,const NativeWorldDrawState& world,std::shared_ptr<const NativeWaterOverlayInput> input){
 Require(frame&&input&&frame->draws.size()<65536,"Water Overlay outside original world frame");
 auto c=MakeNativeWorldDrawCommand(input->fog?NativeWorldDrawStage::World:NativeWorldDrawStage::PreUnit,NativeDrawPrimitive::Triangles,vertices,surface,world);
 c.waterOverlay=std::move(input);ValidateNativeWorldDrawCommand(c);c.sequence=NextNativeWorldDrawSequence();frame->draws.push_back(std::move(c));
}
void NativeWorldDrawRecorder::RecordLOSView(std::span<const NativeWorldDrawVertex> vertices,const ScreenDrawState& surface,const NativeWorldDrawState& world,std::shared_ptr<const NativeLOSViewInput> input){
 Require(frame&&input&&frame->draws.size()<65536,"LOS View outside common world frame");
 auto command=MakeNativeWorldDrawCommand(NativeWorldDrawStage::PreDecals,NativeDrawPrimitive::Triangles,vertices,surface,world);
 command.losView=std::move(input);ValidateNativeWorldDrawCommand(command);command.sequence=NextNativeWorldDrawSequence();frame->draws.push_back(std::move(command));
}
void NativeWorldDrawRecorder::RecordTerraformOverlay(std::span<const NativeWorldDrawVertex> vertices,const ScreenDrawState& surface,const NativeWorldDrawState& world,std::shared_ptr<const NativeTerraformOverlayInput> input){
 Require(frame&&input&&frame->draws.size()<65536,"Terraform Overlay outside common world frame");
 auto command=MakeNativeWorldDrawCommand(NativeWorldDrawStage::PreUnit,NativeDrawPrimitive::Triangles,vertices,surface,world);
 command.terraformOverlay=std::move(input);ValidateNativeWorldDrawCommand(command);command.sequence=NextNativeWorldDrawSequence();frame->draws.push_back(std::move(command));
}
void NativeWorldDrawRecorder::RecordSquadHull(NativeDrawPrimitive primitive,std::span<const NativeWorldDrawVertex> vertices,const ScreenDrawState& surface,const NativeWorldDrawState& world,std::shared_ptr<const NativeSquadHullInput> input){
 Require(frame&&input&&frame->draws.size()<65536,"Squad Hull outside common world frame");
 auto command=MakeNativeWorldDrawCommand(NativeWorldDrawStage::PreUnit,primitive,vertices,surface,world);
 command.squadHull=std::move(input);ValidateNativeWorldDrawCommand(command);command.sequence=NextNativeWorldDrawSequence();frame->draws.push_back(std::move(command));
}
void NativeWorldDrawRecorder::RecordEasyFacing(NativeWorldDrawStage stage,std::span<const NativeWorldDrawVertex> vertices,const ScreenDrawState& surface,const NativeWorldDrawState& world,std::shared_ptr<const NativeEasyFacingInput> input){
 Require(frame&&input&&frame->draws.size()<65536,"Easy Facing outside common world frame");
 auto command=MakeNativeWorldDrawCommand(stage,NativeDrawPrimitive::Quads,vertices,surface,world);
 command.easyFacing=std::move(input);ValidateNativeWorldDrawCommand(command);command.sequence=NextNativeWorldDrawSequence();frame->draws.push_back(std::move(command));
}
}

namespace rendering {
void NativeWorldDrawRecorder::RecordUnitEffect(NativeWorldDrawStage stage,const ScreenDrawState& surface,const NativeWorldDrawState& world,std::shared_ptr<const NativeUnitEffectInput> input){
 Require(frame&&input&&frame->draws.size()<65536,"Airjets outside original common world frame");NativeWorldDrawCommand c;c.stage=stage;c.primitive=c.sourcePrimitive=NativeDrawPrimitive::Triangles;c.surface=surface;c.surface.material=ScreenMaterial::Solid;c.surface.texture.reset();c.surface.secondary={};c.world=world;c.unitEffect=std::move(input);ValidateNativeWorldDrawCommand(c);c.sequence=NextNativeWorldDrawSequence();frame->draws.push_back(std::move(c));
}
}
