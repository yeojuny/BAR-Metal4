/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeBlueprint.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/EngineBridge/NativeOriginalShadowCapture.h"
#include "Rendering/EngineBridge/NativeWaterGroundSourceCapture.h"
#include "Rendering/EngineBridge/NativeS3OModelTextureCapture.h"
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Map/ReadMap.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Models/3DModel.hpp"
#include "Rendering/Models/ModelsMemStorage.h"
#include "Rendering/Backend/NativeBlueprintPrograms.h"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeBlueprintProgramIdentity.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawRecorder.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/Team.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Features/FeatureDef.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace LuaNativeBlueprint {
namespace {
using namespace rendering;
NativeBlueprintRecorder recorder;
ModelProvider modelProvider;
std::shared_ptr<const WorldTerrainHeights> heightSnapshot;
void Check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
bool Available() { return IsNativeBlueprintConsumerAvailable() && bool(modelProvider); }
uint64_t SourceHash(std::string_view source) { uint64_t h=14695981039346656037ULL; for (unsigned char b:source) h=(h^b)*1099511628211ULL; return h; }
template<class T> T Read(const NativeBufferSnapshot& buffer,size_t row,size_t offset) {
	Check(row<buffer.layout.elements && offset+sizeof(T)<=buffer.layout.stride,"Original Blueprint selected instance exceeds storage");
	T result{};std::memcpy(&result,buffer.bytes.data()+row*buffer.layout.stride+offset,sizeof(T));return result;
}
NativeBlueprintView View() {
	const auto* playerView=CCameraHandler::GetCamera(CCamera::CAMTYPE_PLAYER);Check(playerView!=nullptr,"Original Blueprint player camera missing");
	NativeBlueprintView view;std::copy_n(playerView->GetViewProjectionMatrix().m,16,view.viewProjection.begin());
	const auto p=playerView->GetPos();view.cameraPosition={p.x,p.y,p.z};
	view.timeInfo={float(gs->frameNum),spring_tomsecs(globalRendering->grTime)*.001f,(gs->GetLuaSimFrame()+globalRendering->timeOffset)/GAME_SPEED,globalRendering->timeOffset};
	std::array<float,16> identity{};identity[0]=identity[5]=identity[10]=identity[15]=1;
	std::array<float,16> control{};std::copy_n(playerView->GetClipControlMatrix().m,16,control.begin());
	if(control==identity)view.clipDepth=ClipDepthConvention::MinusOneToOne;
	identity[10]=identity[14]=.5f;if(control==identity)view.clipDepth=ClipDepthConvention::ZeroToOne;
	return view;
}
NativeWorldDrawStage Stage() {auto stage=ui::GetRecorder().CurrentWorldStage();Check(stage.has_value(),"Original Blueprint draw outside native world stage");return *stage;}
std::shared_ptr<const WorldTerrainHeights> Heights() {
	Check(readMap && mapDims.mapxp1>=3 && mapDims.mapyp1>=3,"Original Blueprint heightmap unavailable");
	const auto* values=readMap->GetCornerHeightMapUnsynced();Check(values!=nullptr,"Original Blueprint unsynced corner heights missing");
	const size_t count=size_t(mapDims.mapxp1)*mapDims.mapyp1;
	if(!heightSnapshot || heightSnapshot->width!=unsigned(mapDims.mapxp1) || heightSnapshot->height!=unsigned(mapDims.mapyp1) || std::memcmp(heightSnapshot->corners.data(),values,count*sizeof(float))) {
		auto snapshot=std::make_shared<WorldTerrainHeights>();snapshot->width=mapDims.mapxp1;snapshot->height=mapDims.mapyp1;snapshot->corners.assign(values,values+count);heightSnapshot=std::move(snapshot);
	}
	return heightSnapshot;
}
struct Program;struct Buffer;
struct Context {
	std::weak_ptr<Program> active;
	std::map<std::string,std::weak_ptr<Buffer>> buffers;
	std::array<std::shared_ptr<const WorldTexture>,2> textures{};
	std::shared_ptr<const NativeS3OTexturePair> modelTextures;
	bool heightBound=false;
	void Reset() {active.reset();textures={};modelTextures.reset();heightBound=false;}
	void BindModelTextures(const S3DModel* model) {
		auto selected=CaptureNativeS3OModelTextures(model);
		if(!selected) {textures=modelProvider(model)->textures;modelTextures.reset();return;}
		auto& shared=OriginalNativeTextureContext();
		Check(shared.Initialized()&&shared.Registry().ContextGeneration()==selected->contextGeneration,
			"Original Blueprint selected texture context differs");
		// Original CModelDrawerHelperS3O::BindOpaqueTex: secondary first,
		// primary last, active unit zero. PushRenderState itself is a no-op.
		for(unsigned unit:{1u,0u}) {
			shared.History().ActiveTexture(unit);
			shared.Registry().Bind(unit,selected->objects[unit]->name,NativeCUSTextureTarget::Image2D);
			ui::GetRecorder().SyncObservedTextureBinding(unit);
		}
		textures={};modelTextures=std::move(selected);
	}
	std::shared_ptr<const NativeS3OTexturePair> CaptureModelTextures() const {
		if(!modelTextures) {
			Check(!OriginalNativeS3OTextures().Attached(),"Original Blueprint model texture pair was not bound");
			return {};
		}
		auto& shared=OriginalNativeTextureContext();
		Check(OriginalNativeS3OTextures().Attached()&&shared.Initialized()&&
			shared.Registry().ContextGeneration()==modelTextures->contextGeneration,
			"Original Blueprint bound textures outlived their actual context");
		auto selected=std::make_shared<NativeS3OTexturePair>();
		selected->contextGeneration=modelTextures->contextGeneration;
		selected->sourceDrawFrame=globalRendering->drawFrame;
		selected->originalTextureBin=modelTextures->originalTextureBin;
		// Resolve the actual bound objects at Submit, so reload/sampler writes
		// affect later draws without changing an already retained draw version.
		for(unsigned unit=0;unit<2;++unit) {
			const auto bound=shared.History().Binding(unit,NativeCUSTextureTarget::Image2D);
			Check(bound.knowledge==NativeCUSBindingKnowledge::BoundObject&&bound.object,
				"Original Blueprint model sampler has no actual bound object");
			selected->objects[unit]=bound.object;
			(void)NativeS3OTexturePixels(*selected,unit);
		}
		return selected;
	}
};
std::vector<std::weak_ptr<Context>> contexts;
struct Buffer:std::enable_shared_from_this<Buffer> {
	std::shared_ptr<LuaVBOImpl> value;
	std::weak_ptr<Context> context;
	GLenum target;
	bool modelView=false,deleted=false;
	std::string label;
	Buffer(lua_State* L,GLenum requested,bool frequent):target(requested) {
		Check(target==GL_ARRAY_BUFFER||target==GL_ELEMENT_ARRAY_BUFFER,"Original Blueprint buffer target differs");
		value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
	}
	void Owned() const {Check(!deleted&&!modelView,"Original Blueprint CPU operation requires an owned live buffer");}
	void Define(int count,sol::optional<sol::object> layout) {Owned();value->Define(count,layout);}
	size_t Upload(const sol::stack_table& data,sol::optional<int> attr,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last) {Owned();return value->Upload(data,attr,offset,first,last);}
	auto Download(sol::optional<int> attr,sol::optional<int> offset,sol::optional<int> count,sol::optional<bool> force) {Owned();return value->Download(attr,offset,count,force);}
	void Delete() {if(deleted)return;value->Delete();deleted=true;modelView=false;}
	auto GetBufferSize() {Owned();return value->GetBufferSize();}
	std::string GetID() {Owned();auto id=std::to_string(value->CaptureNativeSnapshot()->object);if(auto owner=context.lock())owner->buffers[id]=shared_from_this();return id;}
	const std::string& GetLabel() const {return label;}
	// The source ignores ModelsVBO's old GL byte-size return. Bind a real
	// engine-model provider view, with no made-up GL storage/ID/size to expose.
	void ModelsVBO() {Check(!deleted&&Available(),"Original Blueprint ModelsVBO requires the connected model provider");modelView=true;}
	size_t DefinitionInstance(int id,int attr,sol::optional<int> team,sol::optional<int> offset) {Owned();return value->InstanceDataFromUnitDefIDs(id,attr,team,offset);}
	size_t DefinitionInstances(const sol::stack_table& ids,int attr,sol::optional<int> team,sol::optional<int> offset) {Owned();return value->InstanceDataFromUnitDefIDs(ids,attr,team,offset);}
	size_t FeatureDefinitionInstance(int id,int attr,sol::optional<int> team,sol::optional<int> offset) {Owned();return value->InstanceDataFromFeatureDefIDs(id,attr,team,offset);}
	size_t FeatureDefinitionInstances(const sol::stack_table& ids,int attr,sol::optional<int> team,sol::optional<int> offset) {Owned();return value->InstanceDataFromFeatureDefIDs(ids,attr,team,offset);}
	size_t UnitInstance(int id,int attr,sol::optional<int> offset) {Owned();return value->InstanceDataFromUnitIDs(id,attr,offset);}
	size_t UnitInstances(const sol::stack_table& ids,int attr,sol::optional<int> offset) {Owned();return value->InstanceDataFromUnitIDs(ids,attr,offset);}
};
struct Program:std::enable_shared_from_this<Program> {
	std::shared_ptr<Context> context;
	NativeBlueprintProgram kind;
	float iconDistance=1;
	bool deleted=false;
	bool Initialize() const {Check(!deleted&&Available(),"Original Blueprint actual pipelines unavailable");return true;}
	void Activate() {Initialize();context->active=shared_from_this();}
	void Deactivate() {if(context->active.lock().get()==this)context->active.reset();}
	void Finalize() {Deactivate();deleted=true;}
	void SetUniform(const std::string& name,float value) {Check(!deleted&&kind!=NativeBlueprintProgram::Outline&&name=="iconDistance"&&std::isfinite(value),"Original Blueprint unknown or nonfinite uniform");iconDistance=value;}
};
struct Submission {NativeBlueprintObjectKind kind;int id;std::shared_ptr<const WorldModel> model;};
const S3DModel* ResolveSource(NativeBlueprintObjectKind kind,int id) {
	Check(bool(modelProvider),"Original Blueprint model provider unavailable");
	const S3DModel* model=nullptr;
	if(kind==NativeBlueprintObjectKind::UnitDefinition)model=LuaUtils::SolIdToObject<UnitDef>(id,__func__)->LoadModel();
	else if(kind==NativeBlueprintObjectKind::FeatureDefinition)model=LuaUtils::SolIdToObject<FeatureDef>(id,__func__)->LoadModel();
	else model=LuaUtils::SolIdToObject<CUnit>(id,__func__)->model;
	Check(model!=nullptr,"Original Blueprint referenced model unavailable");return model;
}
std::shared_ptr<const WorldModel> Resolve(NativeBlueprintObjectKind kind,int id) {return modelProvider(ResolveSource(kind,id));}
struct VertexArray {
	std::shared_ptr<Context> context;
	std::shared_ptr<Buffer> vertices,indices,instances;
	std::vector<Submission> submissions;
	bool deleted=false;
	void AttachVertexBuffer(std::shared_ptr<Buffer> buffer) {Check(!deleted&&buffer&&!buffer->deleted&&buffer->target==GL_ARRAY_BUFFER,"Original Blueprint vertex attachment differs");vertices=std::move(buffer);}
	void AttachInstanceBuffer(std::shared_ptr<Buffer> buffer) {Check(!deleted&&buffer&&!buffer->deleted&&!buffer->modelView&&buffer->target==GL_ARRAY_BUFFER,"Original Blueprint instance attachment differs");instances=std::move(buffer);}
	void AttachIndexBuffer(std::shared_ptr<Buffer> buffer) {Check(!deleted&&buffer&&!buffer->deleted&&buffer->target==GL_ELEMENT_ARRAY_BUFFER,"Original Blueprint index attachment differs");indices=std::move(buffer);}
	void Delete() {deleted=true;vertices.reset();indices.reset();instances.reset();submissions.clear();}
	int Add(int id,NativeBlueprintObjectKind kind) {
		Check(!deleted&&vertices&&vertices->modelView&&indices&&indices->modelView&&instances,"Original Blueprint model submission requires ModelsVBO attachments");
		Check(submissions.size()<instances->value->CaptureNativeSnapshot()->layout.elements,"Original Blueprint submission exceeds instance capacity");
		submissions.push_back({kind,id,Resolve(kind,id)});return int(submissions.size())-1;
	}
	int AddUnitDef(int id) {return Add(id,NativeBlueprintObjectKind::UnitDefinition);}
	int AddUnitDefs(const sol::stack_table& ids) {
		const auto first=submissions.size(),count=ids.size();
		Check(instances&&first<=instances->value->CaptureNativeSnapshot()->layout.elements&&count<=instances->value->CaptureNativeSnapshot()->layout.elements-first,"Original Blueprint definition array exceeds instance capacity");
		for(size_t i=1;i<=count;++i)Add(ids.raw_get<int>(i),NativeBlueprintObjectKind::UnitDefinition);
		return int(first);
	}
	int AddFeatureDef(int id) {return Add(id,NativeBlueprintObjectKind::FeatureDefinition);}
	int AddFeatureDefs(const sol::stack_table& ids) {
  const auto first=submissions.size(),count=ids.size();
  Check(instances&&first<=instances->value->CaptureNativeSnapshot()->layout.elements&&count<=instances->value->CaptureNativeSnapshot()->layout.elements-first,"FeatureShape definition array exceeds instance capacity");
  for(size_t i=1;i<=count;++i)Add(ids.raw_get<int>(i),NativeBlueprintObjectKind::FeatureDefinition);
  return int(first);
 }
	int AddUnitsToSubmission(int id) {return Add(id,NativeBlueprintObjectKind::Unit);}
	void ClearSubmission() {Check(!deleted,"Deleted original Blueprint VAO");submissions.clear();}
	void RemoveFromSubmission(int index) {Check(!deleted&&index>=0&&size_t(index)<submissions.size(),"Original Blueprint removal index differs");submissions[index]=std::move(submissions.back());submissions.pop_back();}
	void DrawArrays(GLenum primitive,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instancesCount,sol::optional<int> baseInstance) {
		Check(Available()&&recorder.Active()&&!deleted&&vertices&&instances&&!vertices->modelView&&primitive==GL_LINE_STRIP&&context->heightBound,"Original Blueprint outline attachments or original primitive differ");
		auto program=context->active.lock();Check(program&&!program->deleted&&program->kind==NativeBlueprintProgram::Outline,"Original Blueprint outline program missing");
		Check(count.value_or(5)>=0&&first.value_or(0)>=0&&instancesCount.value_or(0)>=0&&baseInstance.value_or(0)>=0,"Original Blueprint negative draw range");
		NativeBlueprintOutlineDraw draw;draw.stage=Stage();draw.vertices=vertices->value->CaptureNativeSnapshot();draw.instances=instances->value->CaptureNativeSnapshot();
		draw.vertexCount=count.value_or(5);draw.firstVertex=first.value_or(0);draw.instanceCount=instancesCount.value_or(0);draw.firstInstance=baseInstance.value_or(0);
		draw.heights=Heights();draw.mapWorldSize={float(mapDims.mapx)*SQUARE_SIZE,float(mapDims.mapy)*SQUARE_SIZE};draw.view=View();
		draw.surface=ui::GetRecorder().CurrentState().draw;draw.world=ui::GetRecorder().CurrentWorldState();recorder.Record(std::move(draw));
	}
	void Submit() {
		Check(Available()&&recorder.Active()&&!deleted&&vertices&&indices&&instances&&vertices->modelView&&indices->modelView,"Original Blueprint ModelsVBO draw unavailable");
		auto program=context->active.lock();Check(program&&!program->deleted&&program->kind!=NativeBlueprintProgram::Outline,"Original Blueprint model program missing");
		NativeBlueprintModelDraw draw;draw.stage=Stage();draw.program=program->kind;draw.instances=instances->value->CaptureNativeSnapshot();draw.textures=context->textures;draw.iconDistance=program->iconDistance;draw.view=View();
		draw.modelTextures=context->CaptureModelTextures();
		draw.surface=ui::GetRecorder().CurrentState().draw;draw.world=ui::GetRecorder().CurrentWorldState();
  const bool featureShape=draw.program==NativeBlueprintProgram::FeatureShape;
  const auto featureUniforms=featureShape?OriginalCUSUniformCapture().Current():nullptr;
  const auto featureTransforms=featureShape?OriginalCUSModelPublication().Transforms():nullptr;
  if(featureShape){
   Check(featureUniforms&&featureTransforms&&featureTransforms->kind==NativeCUSPublishedKind::Transforms&&featureTransforms->stride==32,"FeatureShape actual original shader publications missing");
   ValidateNativeCUSUniformPublication(*featureUniforms);
   Check(featureUniforms->matrices->sourceDrawFrame==uint64_t(globalRendering->drawFrame)&&featureUniforms->params->sourceDrawFrame==uint64_t(globalRendering->drawFrame)&&featureTransforms->publishedDrawFrame<=uint64_t(globalRendering->drawFrame),"FeatureShape shader publications belong to another frame");
   std::memcpy(draw.view.viewProjection.data(),featureUniforms->matrices->bytes.data()+5*64,64);
  }
		for(size_t row=0;row<submissions.size();++row) {
			const auto& source=submissions[row];NativeBlueprintModelSubmission item;item.kind=source.kind;item.objectId=source.id;item.instanceIndex=row;item.model=source.model;
			item.sourceTransformBase=Read<uint32_t>(*draw.instances,row,draw.program==NativeBlueprintProgram::FeatureShape?64:40);
			unsigned maxBone=0;for(const auto& piece:item.model->pieces)for(const auto& vertex:piece.vertices)maxBone=std::max(maxBone,unsigned(vertex.boneIds[0]&255));
			Check(item.sourceTransformBase<transformsMemStorage.GetSize()&&maxBone<transformsMemStorage.GetSize()-item.sourceTransformBase,"Original Blueprint source transform range unavailable");
   for(unsigned bone=0;bone<=maxBone;++bone) {
    if(featureShape){NativeBlueprintTransform transform;ReadNativeCUSPublishedElement(*featureTransforms,size_t(item.sourceTransformBase)+bone,std::span(reinterpret_cast<uint8_t*>(&transform),sizeof(transform)));item.transforms.push_back(transform);}
    else {const auto& t=transformsMemStorage[item.sourceTransformBase+bone];item.transforms.push_back({{t.r.x,t.r.y,t.r.z,t.r.r},{t.t.x,t.t.y,t.t.z,t.s}});}
   }
			const bool feature=draw.program==NativeBlueprintProgram::FeatureShape;const auto overrideTeam=feature?255u:Read<uint32_t>(*draw.instances,row,32);const auto teamId=overrideTeam<255?overrideTeam:(Read<uint32_t>(*draw.instances,row,feature?72:48)&255);
   if(featureShape){const auto color=ReadNativeCUSTeamColor(*featureUniforms,teamId);std::memcpy(item.selectedTeamColor.data(),color.data(),color.size());}
   else {Check(teamHandler.IsValidTeam(teamId),"Original Blueprint source team palette missing");const auto* team=teamHandler.Team(teamId);for(unsigned c=0;c<4;++c)item.selectedTeamColor[c]=team->color[c]/255.f;}
			draw.submissions.push_back(std::move(item));
		}
		recorder.Record(std::move(draw));
	}
};
sol::table CreateContext(lua_State* L,bool game31657=false) {
	Check(!CLuaHandle::GetHandleSynced(L)&&Available(),"Original Blueprint requires unsynced complete native family");
	sol::state_view lua(L);auto table=lua.create_table();auto context=std::make_shared<Context>();contexts.erase(std::remove_if(contexts.begin(),contexts.end(),[](const auto& p){return p.expired();}),contexts.end());contexts.emplace_back(context);
	table.set_function("GetVBO",[L,context](sol::optional<GLenum> target,sol::optional<bool> frequent){auto buffer=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));buffer->context=context;return buffer;});
	table.set_function("GetVAO",[context](){auto array=std::make_shared<VertexArray>();array->context=context;return array;});
	table.set_function("CreateProgram",[context,game31657](const std::string& name,const std::string& vertex,const std::string& fragment,const std::string& engine,const std::string& quaternion,const std::string& defines,const sol::table& floats,const sol::table& ints){
		auto program=std::make_shared<Program>();program->context=context;
		if(name=="Blueprint API")program->kind=NativeBlueprintProgram::Outline;else if(name=="UnitGL4 API")program->kind=NativeBlueprintProgram::Unit;else if(name=="UnitShapeGL4 API")program->kind=NativeBlueprintProgram::UnitShape;else if(name=="FeatureShapeGL4 API")program->kind=NativeBlueprintProgram::FeatureShape;else throw std::runtime_error("Unknown original Blueprint shader name");
		ValidateNativeBlueprintProgram(program->kind,vertex,fragment,engine,quaternion,defines,game31657);
		size_t floatCount=0,intCount=0;
		for(const auto& item:floats){Check(program->kind!=NativeBlueprintProgram::Outline&&item.first.as<std::string>()=="iconDistance"&&item.second.as<float>()==1,"Original Blueprint model float uniform differs");++floatCount;}
		for(const auto& item:ints){const auto key=item.first.as<std::string>();const auto unit=item.second.as<int>();Check(program->kind==NativeBlueprintProgram::Outline?(key=="heightmapTex"&&unit==0):((key=="tex1"&&unit==0)||(key=="tex2"&&unit==1)),"Original Blueprint sampler assignment differs");++intCount;}
		Check(program->kind==NativeBlueprintProgram::Outline?(floatCount==0&&intCount==1):(floatCount==(game31657&&(program->kind==NativeBlueprintProgram::UnitShape||program->kind==NativeBlueprintProgram::FeatureShape)?0u:1u)&&intCount==2),"Original Blueprint required uniforms missing");
		return program;
	});
	table.set_function("Texture",[context](int unit,sol::object value){Check(unit==0,"Original Blueprint height texture unit differs");if(value.is<bool>()){Check(!value.as<bool>(),"Original Blueprint height texture enable lacks binding");context->heightBound=false;}else{Check(value.as<std::string>()=="$heightmap","Original Blueprint height image differs");context->heightBound=true;}return true;});
	table.set_function("UnitShapeTextures",[context](sol::optional<int> definition,bool enable){
		// Original S3O PopRenderState does not unbind either name. The verified
		// wrapper's existing Texture(false) cleanup only changes enable state.
		if(!enable){context->textures={};context->modelTextures.reset();return;}
		Check(definition.has_value(),"Original Blueprint model texture definition missing");
		context->BindModelTextures(ResolveSource(NativeBlueprintObjectKind::UnitDefinition,*definition));
	});
	table.set_function("FeatureShapeTextures",[context](sol::optional<int> definition,bool enable){
  if(!enable){context->textures={};context->modelTextures.reset();return;}
  Check(definition.has_value(),"FeatureShape texture definition missing");
  context->BindModelTextures(ResolveSource(NativeBlueprintObjectKind::FeatureDefinition,*definition));
 });
	table.set_function("ObjectLabel",[context](GLenum type,const std::string& id,const std::string& label){Check(type==0x82e0&&label.size()<=1024,"Invalid original Blueprint buffer label");auto found=context->buffers.find(id);Check(found!=context->buffers.end(),"Unknown original Blueprint buffer identity");auto buffer=found->second.lock();Check(bool(buffer),"Deleted original Blueprint buffer identity");buffer->label=label;});
	return table;
}
} // namespace
void SetModelProvider(ModelProvider provider) {modelProvider=std::move(provider);}
bool HasModelProvider(){return bool(modelProvider);}
std::shared_ptr<const rendering::WorldModel> CaptureUnitModel(int unitID){return Resolve(NativeBlueprintObjectKind::Unit,unitID);}
void BeginWorldFrame() {
	recorder.Begin();
	ui::GetRecorder().SetWorldClearHandler([](unsigned mask,const ScreenClearValues& values,const NativeWorldDrawState& state,std::optional<ScreenScissor> scissor,NativeWorldDrawStage stage){
		if((stage==NativeWorldDrawStage::WaterReflection||stage==NativeWorldDrawStage::WaterRefraction||
   stage==NativeWorldDrawStage::Ground||stage==NativeWorldDrawStage::TerrainReflection)&&CurrentNativeWaterGroundSourceCapture()){
   GetWorldDrawRecorder().RecordClear(stage,static_cast<ScreenAttachmentMask>(mask),values,state,scissor);return;
  }
		if(stage==NativeWorldDrawStage::ShadowOpaque){
   Check(mask&&!(mask&~unsigned(ScreenAttachmentColor|ScreenAttachmentDepth)),"Original shadow target clear requires native color/depth bits and has no stencil attachment");
   auto& source=CurrentOriginalNativeShadowCapture();
   const auto pending=source.ValidateClear(mask,values,state,scissor);
   const auto actualSequence=GetWorldDrawRecorder().RecordClear(stage,static_cast<ScreenAttachmentMask>(mask),values,state,scissor);
   source.CommitClear(pending,actualSequence);return;
  }
		Check(mask==ScreenAttachmentStencil,"Original Blueprint world consumer requires a stencil-only clear");
		NativeWorldStencilClear clear;clear.stage=stage;clear.value=uint8_t(values.stencil);clear.writeMask=uint8_t(state.frontStencil.writeMask);clear.scissor=scissor;
		RecordStencilClear(std::move(clear));
	});
}
std::shared_ptr<const NativeBlueprintFrame> EndWorldFrame() {auto frame=recorder.End();ui::GetRecorder().SetWorldClearHandler({});return frame;}
void AbortWorldFrame() noexcept {recorder.Abort();ui::GetRecorder().SetWorldClearHandler({});for(auto& weak:contexts)if(auto context=weak.lock())context->Reset();}
void RecordStencilClear(NativeWorldStencilClear clear) {Check(Available()&&recorder.Active(),"Original Blueprint world stencil clear consumer unavailable");recorder.Record(std::move(clear));}
void Reset() {AbortWorldFrame();heightSnapshot.reset();}
bool PushEntries(lua_State* L) {
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();
	family.new_usertype<Buffer>("Buffer",sol::no_constructor,"Define",&Buffer::Define,"Upload",&Buffer::Upload,"Download",&Buffer::Download,"Delete",&Buffer::Delete,"GetBufferSize",&Buffer::GetBufferSize,"GetID",&Buffer::GetID,"GetLabel",&Buffer::GetLabel,"ModelsVBO",&Buffer::ModelsVBO,"InstanceDataFromUnitDefIDs",sol::overload(&Buffer::DefinitionInstance,&Buffer::DefinitionInstances),"InstanceDataFromFeatureDefIDs",sol::overload(&Buffer::FeatureDefinitionInstance,&Buffer::FeatureDefinitionInstances),"InstanceDataFromUnitIDs",sol::overload(&Buffer::UnitInstance,&Buffer::UnitInstances));
	family.new_usertype<VertexArray>("VertexArray",sol::no_constructor,"AttachVertexBuffer",&VertexArray::AttachVertexBuffer,"AttachIndexBuffer",&VertexArray::AttachIndexBuffer,"AttachInstanceBuffer",&VertexArray::AttachInstanceBuffer,"DrawArrays",&VertexArray::DrawArrays,"Submit",&VertexArray::Submit,"AddUnitDefsToSubmission",sol::overload(&VertexArray::AddUnitDef,&VertexArray::AddUnitDefs),"AddFeatureDefsToSubmission",sol::overload(&VertexArray::AddFeatureDef,&VertexArray::AddFeatureDefs),"AddUnitsToSubmission",&VertexArray::AddUnitsToSubmission,"RemoveFromSubmission",&VertexArray::RemoveFromSubmission,"ClearSubmission",&VertexArray::ClearSubmission,"Delete",&VertexArray::Delete);
	family.new_usertype<Program>("Program",sol::no_constructor,"Initialize",&Program::Initialize,"Activate",&Program::Activate,"Deactivate",&Program::Deactivate,"Finalize",&Program::Finalize,"SetUniform",&Program::SetUniform);
	family.set("Buffer",sol::lua_nil,"VertexArray",sol::lua_nil,"Program",sol::lua_nil);family.set_function("Available",&Available);family.set_function("CreateContext",[L](){return CreateContext(L);});
	family.set_function("LoadArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Original Blueprint archive scope is unsynced only");Check(path=="LuaUI/Widgets/api_blueprint.lua"||path=="LuaUI/Widgets/gfx_DrawUnitShape_GL4.lua"||path=="modules/graphics/instancevbotable.lua"||path=="modules/graphics/instancevboidtable.lua"||path=="modules/graphics/luashader.lua","Unknown original Blueprint archive path");CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source),"Original Blueprint archive source missing");return source;});
	// Both the wrapper SHA-512 and this template identity precede preprocessing.
	family.set_function("PrepareSource",[](std::string source){const auto hash=SourceHash(source);Check(hash==0x96c8380a3dccd5a3ULL||hash==0xf8b9aeee6713c068ULL||hash==0xa3fc701948f45d37ULL||hash==0x04f5ce9c2e542847ULL||hash==0x1d6a85914e01f693ULL,"Unsupported original Blueprint source identity");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
	family.set_function("CreateContext31657",[L](){return CreateContext(L,true);});
	family.set_function("LoadArchiveSource31657",[L](const std::string& path){
		Check(!CLuaHandle::GetHandleSynced(L),"Pinned31657 Blueprint archive is unsynced only");
		for(const auto& entry:NativeBlueprintSources31657)if(entry.path==path){CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&SourceHash(source)==entry.hash,"Pinned31657 Blueprint archive identity differs");return source;}
		throw std::runtime_error("Unknown pinned31657 Blueprint archive path");
	});
	family.set_function("PrepareSource31657",[](std::string source){const auto hash=SourceHash(source);bool known=false;for(const auto& entry:NativeBlueprintSources31657)known|=hash==entry.hash;Check(known,"Unsupported pinned31657 Blueprint source identity");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
	gl.set("NativeBlueprint",family);return true;
}
} // namespace LuaNativeBlueprint
#else
namespace LuaNativeBlueprint {
void SetModelProvider(ModelProvider){} void BeginWorldFrame(){}
std::shared_ptr<const rendering::NativeBlueprintFrame> EndWorldFrame(){return {};}
void AbortWorldFrame() noexcept{} void RecordStencilClear(rendering::NativeWorldStencilClear){} void Reset(){}
bool PushEntries(lua_State*){return true;}
}
#endif
