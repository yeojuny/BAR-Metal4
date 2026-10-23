/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeWaterGroundSourceCapture.h"
#include <stdexcept>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include "Rendering/Backend/NativeOriginalModelContext.h"
#include "Rendering/Backend/NativeSourceProgramBinding.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/Backend/NativeWorldDrawRecorder.h"
#include "Rendering/Backend/NativeCUSDeferredData.h"
#include "Rendering/Backend/NativeTextureContext.h"

namespace rendering {
namespace {
thread_local NativeWaterGroundSourceCapture* active=nullptr;
bool GroundStage(NativeWorldDrawStage stage)
{
	return stage==NativeWorldDrawStage::WaterReflection || stage==NativeWorldDrawStage::WaterRefraction ||
		stage==NativeWorldDrawStage::Ground || stage==NativeWorldDrawStage::TerrainReflection;
}
}

NativeWaterGroundSourceCapture* CurrentNativeWaterGroundSourceCapture() noexcept {return active;}

NativeWaterGroundSourceCapture::NativeWaterGroundSourceCapture(NativeTextureRegistry& actualRegistry,
	NativeCUSTextureHistory& actualHistory,ui::NativeUIRecorder& actualRecorder,NativeSourceProgramBinding& actualPrograms)
	:registry(actualRegistry),history(actualHistory),recorder(actualRecorder),programs(actualPrograms)
{}

void NativeWaterGroundSourceCapture::RequireActive() const
{
	if(active!=this || !frame || recorder.CurrentWorldStage()!=frame->stage ||
		registry.ContextGeneration()!=frame->contextGeneration || !registry.MatchesHistory(history))
		throw std::logic_error("Original ground writer lost its actual enclosing source capture");
}

std::shared_ptr<const NativeWaterGroundSourceFrame> NativeWaterGroundSourceCapture::Capture(uint64_t drawFrame,
	NativeWorldDrawStage stage,const std::function<void()>& originalDraw,const WorldCamera* actualView)
{
	if(active || frame || !originalDraw || !GroundStage(stage) || recorder.CurrentWorldStage()!=stage || recorder.HasActiveTarget() ||
		!registry.ContextGeneration() || !registry.MatchesHistory(history))
		throw std::logic_error("Original water ground capture has a missing or nested source scope");
	frame=std::make_shared<NativeWaterGroundSourceFrame>();
	frame->contextGeneration=registry.ContextGeneration();frame->sourceDrawFrame=drawFrame;frame->stage=stage;
	if(actualView)frame->sourceView=*actualView;
 frame->source.begin=NextNativeWorldDrawSequence();drawInvoked=false;active=this;
	try {
		originalDraw();
		RequireActive();
		if(!drawInvoked)throw std::logic_error("Original water ground Draw source hook was not invoked");
		if(inDeferred||(mapPhase&&!mapPhase->lua))throw std::logic_error("Original map MRT capture did not finish its actual target/callbacks");
  if(callback)throw std::logic_error("Original water ground callback did not close");
		if(frame->combined!=frame->compositeEntered||(frame->combined&&frame->preForward))throw std::logic_error("Combined terrain omitted its original composite or replayed PreForward");
		frame->source.end=NextNativeWorldDrawSequence();
		auto output=std::move(frame);active=nullptr;square.reset();drawInvoked=false;mapPhase.reset();
		return output;
	} catch(...) {
		// Preserve actual source-context mutations. Only the unsubmitted frame
		// is abandoned; there is no GL-state rollback or callback replay.
		if(mapPhase)CancelNativeCUSDeferredPhase(registry,*mapPhase);
  mapPhase.reset();inDeferred=false;active=nullptr;frame.reset();callback.reset();square.reset();drawInvoked=false;throw;
	}
}

void NativeWaterGroundSourceCapture::BeginDraw()
{
	RequireActive();
	if(drawInvoked)throw std::logic_error("Original water ground Draw was invoked twice");
	drawInvoked=true;
}

void NativeWaterGroundSourceCapture::EnterDraw()
{
	RequireActive();
	if(!drawInvoked || frame->drawEntered)throw std::logic_error("Original water ground Draw body is missing or repeated");
	frame->drawEntered=true;
}

void NativeWaterGroundSourceCapture::BeginDeferred(bool clearOnly,bool combined)
{
 RequireActive();
 if(frame->stage!=NativeWorldDrawStage::Ground||!frame->drawEntered||frame->deferredEntered||!frame->sourceView||callback)
  throw std::logic_error("Original selected map MRT has no actual Normal source view or is repeated");
 frame->deferredEntered=true;frame->deferredClearOnly=clearOnly;frame->combined=combined;
 if(combined&&clearOnly)throw std::logic_error("Combined ground requires an actual selected mesh program");
 frame->deferred=NativeGroundSourceSpan{NextNativeWorldDrawSequence(),0};
 mapPhase=std::make_shared<NativeCUSDeferredPhase>(BeginNativeMapDeferredPhase(registry,
  OriginalNativeTextureContext().Geometry(NativeGeometryTextureRole::Map),frame->sourceDrawFrame,*frame->sourceView));
 if(combined&&!mapPhase->combinedColor.Changed())throw std::logic_error("Combined ground has no original optional color publication");
 mapPhase->clearOnly=clearOnly;frame->mapMRT=mapPhase;inDeferred=true;square.reset();
 const auto& m=mapPhase->depth.backing->Metadata();
 if(!clearOnly)recorder.SetViewport({0,0,m.width,m.height});
}
void NativeWaterGroundSourceCapture::RecordDeferredClear()
{
 RequireActive();if(!inDeferred)throw std::logic_error("Original map clear is outside the actual MRT phase");
 recorder.SetOriginalClearColor({0,0,0,0});recorder.SetOriginalClearDepth(1);
 recorder.UpdateWorldState([](auto& state){state.depthCompare=ScreenAlphaCompare::LessEqual;},false);
 GetWorldDrawRecorder().RecordClear(frame->stage,static_cast<ScreenAttachmentMask>(ScreenAttachmentColor|ScreenAttachmentDepth),
  recorder.CurrentClearValues(),recorder.CurrentWorldState(),recorder.CurrentState().draw.scissor);
}
void NativeWaterGroundSourceCapture::EndDeferred()
{
 RequireActive();if(!inDeferred||!frame->deferred||callback)throw std::logic_error("Original map MRT target end is missing or nested");
 recorder.SetOriginalClearDepth(1);recorder.UpdateWorldState([](auto& state){state.depthCompare=ScreenAlphaCompare::LessEqual;},false);
 frame->deferred->end=NextNativeWorldDrawSequence();inDeferred=false;square.reset();
 if(!frame->deferredClearOnly){const auto& view=*frame->sourceView;
  recorder.SetViewport({view.viewport[0],view.viewport[1],uint32_t(view.viewport[2]),uint32_t(view.viewport[3])});}
}
void NativeWaterGroundSourceCapture::FinishDeferredCallbacks(std::shared_ptr<const NativeCUSSourceFrame> whole)
{
 RequireActive();if(!mapPhase)return;
 if(inDeferred||!whole||whole->sourceDrawFrame!=frame->sourceDrawFrame)throw std::logic_error("Original map callback finalization lost its actual completed source");
 auto slice=std::make_shared<NativeCUSSourceFrame>();slice->sourceDrawFrame=whole->sourceDrawFrame;slice->sourceLoads=whole->sourceLoads;
 for(const auto& draw:whole->draws)if(frame->deferred->Contains(draw.sequence))slice->draws.push_back(draw);
 FinishNativeMapDeferredPhase(*mapPhase,std::move(slice));
}

void NativeWaterGroundSourceCapture::EnterForward()
{
	RequireActive();
	if(!frame->drawEntered || frame->forwardEntered)
		throw std::logic_error("Original water ground forward selection is missing or repeated");
	frame->forwardEntered=true;
}

void NativeWaterGroundSourceCapture::SubmitComposite(NativeTerrainProgramSnapshot program)
{
	RequireActive();
	if(!frame->combined||frame->stage!=NativeWorldDrawStage::Ground||inDeferred||callback||frame->compositeEntered||frame->forwardEntered||!frame->mapMRT||!frame->mapMRT->combinedColor.Changed())
		throw std::logic_error("Original combined composite has no completed same-invocation Map color/depth source");
	frame->compositeEntered=true;square=std::pair{0u,0u};
	// The four original GL_QUADS vertices, with their unchanged diagonal and winding.
	const std::array<std::array<float,3>,4> positions{{{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}}};
	const std::array<uint32_t,6> indices{0,1,2,0,2,3};
	SubmitSquare(NativeTerrainMeshSource::Composite,positions.data(),positions.size(),sizeof(positions[0]),indices,true,std::move(program));
	square.reset();
}

void NativeWaterGroundSourceCapture::EnterBorder()
{
	RequireActive();
	if(!frame->drawEntered || frame->borderEntered || callback || frame->stage!=NativeWorldDrawStage::Ground)
		throw std::logic_error("Original ground border is outside its actual Normal draw or repeated");
	frame->borderEntered=true;square.reset();
}

void NativeWaterGroundSourceCapture::Callback(NativeGroundCallback kind,const std::function<void()>& originalCallback)
{
	RequireActive();
	if(callback || !originalCallback)
		throw std::logic_error("Original water ground callback has a missing or nested forward scope");
	std::optional<NativeGroundSourceSpan>* span=nullptr;
	switch(kind) {
		case NativeGroundCallback::PreForward:span=&frame->preForward;break;
		case NativeGroundCallback::PostForward:span=&frame->postForward;break;
  case NativeGroundCallback::PreDeferred:span=&frame->preDeferred;break;
  case NativeGroundCallback::Deferred:span=&frame->deferredCallback;break;
  case NativeGroundCallback::PostDeferred:span=&frame->postDeferred;break;
		default:throw std::logic_error("Unknown original ground callback");
	}
	const bool deferredCallback=kind==NativeGroundCallback::PreDeferred||kind==NativeGroundCallback::Deferred;
 if(deferredCallback?!inDeferred:(kind==NativeGroundCallback::PostDeferred?(!frame->deferredEntered||inDeferred):!(frame->forwardEntered||(kind==NativeGroundCallback::PostForward&&frame->compositeEntered))))
  throw std::logic_error("Original ground callback belongs to another actual target phase");
 if(*span)throw std::logic_error("Original water ground callback was replayed");
	span->emplace();(*span)->begin=NextNativeWorldDrawSequence();callback=kind;
	try {
		if(frame->combined&&kind==NativeGroundCallback::Deferred)recorder.UpdateWorldState([](auto& state){state.combinedColorWrite={false,false,false,false};},false);
		originalCallback();RequireActive();
		if(frame->combined&&kind==NativeGroundCallback::Deferred)recorder.UpdateWorldState([](auto& state){state.combinedColorWrite={true,true,true,true};},false);
		(*span)->end=NextNativeWorldDrawSequence();callback.reset();
	} catch(...) {callback.reset();throw;}
}

void NativeWaterGroundSourceCapture::SelectSquare(uint32_t x,uint32_t z)
{
	RequireActive();
	if((!frame->forwardEntered&&!frame->borderEntered&&!inDeferred) || callback)
		throw std::logic_error("Original terrain square setup is outside actual mesh traversal");
	square=std::pair{x,z};
}

void NativeWaterGroundSourceCapture::SubmitMapEdge(mapedge::DrawPacket draw)
{
	RequireActive();
	if(!inDeferred||callback!=NativeGroundCallback::Deferred||!mapPhase||mapPhase->clearOnly||
		mapPhase->role!=NativeGeometryTextureRole::Map||frame->stage!=NativeWorldDrawStage::Ground||!draw.parameters.deferred)
		throw std::logic_error("Original deferred map-edge lost its actual selected Ground callback/Map MRT");
	mapedge::Validate(draw.parameters,draw.mirrors);
	if(draw.mirrors.empty())throw std::logic_error("Original deferred map-edge submitted after its empty-mirror return");
	if(!draw.original)throw std::logic_error("Original deferred map-edge lacks its actual engine uniforms");
	ValidateNativeCUSUniformPublication(*draw.original);
	if(draw.original->matrices->sourceDrawFrame>frame->sourceDrawFrame||
		draw.original->params->sourceDrawFrame>frame->sourceDrawFrame)
		throw std::logic_error("Original deferred map-edge uniforms are from a future source frame");
	for(const auto& image:draw.images)
		if(image.knowledge!=NativeCUSBindingKnowledge::BoundObject||!image.object||
			image.object->contextGeneration!=frame->contextGeneration||image.object->target!=NativeCUSTextureTarget::Image2D||
			image.object->storage!=NativeTextureStorageKnowledge::Published)
			throw std::logic_error("Original deferred map-edge image has no actual published binding");
	draw.sourceDrawFrame=frame->sourceDrawFrame;draw.mapMRT=mapPhase;
	draw.sequence=NextNativeWorldDrawSequence();
	frame->mapEdges.push_back(std::make_shared<const mapedge::DrawPacket>(std::move(draw)));
}

void NativeWaterGroundSourceCapture::SubmitSquare(NativeTerrainMeshSource mesh,const void* positions,size_t vertexCount,size_t vertexStride,
	std::span<const uint32_t> indices,bool indexed,NativeTerrainProgramSnapshot program,
	const void* colors,size_t colorStride,bool border)
{
	RequireActive();
	const bool composite=mesh==NativeTerrainMeshSource::Composite;
	if((composite?!frame->compositeEntered:border?!frame->borderEntered:(!frame->forwardEntered&&!inDeferred)) || callback || !square)
		throw std::logic_error("Original terrain submit has no actual selected source square");
	const size_t count=indexed?indices.size():vertexCount;
	if(!count)return;
	if(!positions || !vertexCount || vertexStride<sizeof(std::array<float,3>) ||
		vertexCount>std::numeric_limits<size_t>::max()/vertexStride || count%3 || count>size_t(std::numeric_limits<int32_t>::max()) ||
		(!indexed&&!indices.empty()) || (border&&(!colors||colorStride<4||vertexCount>std::numeric_limits<size_t>::max()/colorStride)))
		throw std::runtime_error("Original terrain actual triangle positions/indices/colors have an invalid source extent");
	if(recorder.HasActiveTarget())
		throw std::runtime_error("Original terrain submit targets a nested framebuffer without an actual terrain consumer");
	if(mesh!=NativeTerrainMeshSource::Basic && mesh!=NativeTerrainMeshSource::ROAM && !composite)
		throw std::logic_error("Original terrain submission has an unknown source mesh");
	if(programs.TerrainOwner()!=program.consumer)
		throw std::runtime_error("Original ground callback changed the actual current terrain program");
	ValidateNativeTerrainProgram(program.identity,program.consumer,frame->contextGeneration);
	if(border!=(program.identity.selection==NativeTerrainProgramSelection::Border)||composite!=(program.identity.selection==NativeTerrainProgramSelection::Composite))
		throw std::runtime_error("Original terrain border draw has another actual program family");
	if(!program.uniforms || program.uniforms->contextGeneration!=frame->contextGeneration ||
		program.uniforms->originalProgramGeneration!=program.identity.originalProgramGeneration || !program.uniforms->revision)
		throw std::runtime_error("Original terrain draw lacks its actual current program uniform publication");
	const auto& fixed=OriginalNativeModelContext();
	if(!fixed.clips.observed || fixed.clips.contextGeneration!=frame->contextGeneration || !fixed.fog.observed)
		throw std::runtime_error("Original terrain draw lacks its actual shared fog/clip source");
	NativeTerrainSquareDraw draw;
	draw.squareX=square->first;draw.squareZ=square->second;draw.mesh=mesh;draw.originalIndexCount=indices.size();draw.border=border;draw.deferred=inDeferred;draw.composite=composite;
	draw.geometry.indexed=indexed;draw.geometry.positions.resize(vertexCount);
	const auto* positionBytes=static_cast<const std::byte*>(positions);
	for(size_t n=0;n<vertexCount;++n) {
		std::memcpy(draw.geometry.positions[n].data(),positionBytes+n*vertexStride,sizeof(draw.geometry.positions[n]));
		for(float value:draw.geometry.positions[n])if(!std::isfinite(value))throw std::runtime_error("Original terrain vertex is nonfinite");
	}
	for(uint32_t index:indices)if(index>=vertexCount)throw std::runtime_error("Original terrain index exceeds its actual source vertices");
	draw.geometry.indices.assign(indices.begin(),indices.end());
	if(colors) {
		if(colorStride<4||vertexCount>std::numeric_limits<size_t>::max()/colorStride)throw std::runtime_error("Original terrain color source stride is invalid");
		draw.geometry.colors.resize(vertexCount);const auto* colorBytes=static_cast<const std::byte*>(colors);
		for(size_t n=0;n<vertexCount;++n)std::memcpy(draw.geometry.colors[n].data(),colorBytes+n*colorStride,4);
	}
	draw.program=std::move(program);draw.images=CaptureNativeTerrainImages(registry,history,frame->sourceDrawFrame);
	draw.world=recorder.CurrentWorldState();draw.surface=recorder.CurrentState().draw;
	draw.modelView=recorder.GetMatrix(ui::NativeUIMatrixMode::ModelView);
	draw.projection=recorder.GetMatrix(ui::NativeUIMatrixMode::Projection);
	draw.clipEnabled=fixed.clips.enabled;draw.clipEyeEquations=fixed.clips.eyeEquations;
	draw.fogEnabled=fixed.fogEnabled;draw.fogColor=fixed.fog.color;draw.fogParameters=fixed.fog.parameters;
	draw.sequence=NextNativeWorldDrawSequence();frame->squares.push_back(std::move(draw));
}
} // namespace rendering
