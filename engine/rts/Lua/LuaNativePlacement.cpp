/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativePlacement.h"
#include "LuaNativeBuildSquare.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/Backend/NativeModelLightCapture.h"
#include "Rendering/EngineBridge/NativeS3OModelTextureCapture.h"
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Game/GameHelper.h"
#include "Map/ReadMap.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/Backend/NativePlacementIdentity.h"
#include "Rendering/Common/ModelDrawerState.hpp"
#include "Rendering/GlobalRendering.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/Misc/Team.h"
#include "Sim/Units/BuildInfo.h"
#include "Sim/Units/UnitDef.h"
#include "System/Color.h"
#include "System/Config/ConfigHandler.h"
#include "System/FileSystem/FileHandler.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace LuaNativePlacement {
namespace {
using namespace rendering;
ModelProvider modelProvider;
std::shared_ptr<NativePlacementFrame> frame;
std::shared_ptr<const WorldTerrainHeights> heightSnapshot;
bool engineCapture=false;
NativeWorldDrawState engineState;
NativeWorldDrawStage engineStage=NativeWorldDrawStage::World;
ScreenColor engineColor;
ScreenDrawState engineSurface;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<typename Signature> struct LuaBoundary;
template<typename R,typename... Args> struct LuaBoundary<R(Args...)> {
 template<typename F> static auto Wrap(F function){
  return [function=std::move(function)](sol::this_state lua,Args... args)->R {
   // Pinned sol2's capturing-lambda path has no C++ exception trampoline.
   // Preserve a useful Lua string without changing global/vendor bindings.
   try{return function(std::forward<Args>(args)...);}
   catch(const std::exception& error){lua_pushstring(lua,error.what());}
   lua_error(lua);std::terminate();
  };
 }
};
template<typename F> void BindFunction(sol::table& table,const char* name,F function){table.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(function)));}
bool Available(){return IsNativePlacementAvailable()&&bool(modelProvider);}
uint64_t Hash(std::string_view s){uint64_t h=14695981039346656037ULL;for(unsigned char c:s)h=(h^c)*1099511628211ULL;return h;}
NativeBlueprintView View(){
	const auto* c=CCameraHandler::GetCamera(CCamera::CAMTYPE_PLAYER);Check(c,"Original placement camera missing");NativeBlueprintView v;
	std::copy_n(c->GetViewProjectionMatrix().m,16,v.viewProjection.begin());const auto& inverseView=c->GetViewMatrixInverse();v.cameraPosition={inverseView[12],inverseView[13],inverseView[14]};
	std::array<float,16> actual{},control{};std::copy_n(c->GetClipControlMatrix().m,16,actual.begin());control[0]=control[5]=control[10]=control[15]=1;
	if(actual==control)v.clipDepth=ClipDepthConvention::MinusOneToOne;control[10]=control[14]=.5f;if(actual==control)v.clipDepth=ClipDepthConvention::ZeroToOne;return v;
}
std::shared_ptr<const WorldTerrainHeights> Heights(){
	Check(readMap&&mapDims.mapxp1>=3&&mapDims.mapyp1>=3,"Original Building Grid heightmap missing");auto* values=readMap->GetCornerHeightMapUnsynced();size_t count=size_t(mapDims.mapxp1)*mapDims.mapyp1;
	if(!heightSnapshot||heightSnapshot->width!=unsigned(mapDims.mapxp1)||heightSnapshot->height!=unsigned(mapDims.mapyp1)||std::memcmp(heightSnapshot->corners.data(),values,count*sizeof(float))){auto h=std::make_shared<WorldTerrainHeights>();h->width=mapDims.mapxp1;h->height=mapDims.mapyp1;h->corners.assign(values,values+count);heightSnapshot=std::move(h);}return heightSnapshot;
}
ScreenBlendFactor Blend(unsigned value){switch(value){case GL_ZERO:return ScreenBlendFactor::Zero;case GL_ONE:return ScreenBlendFactor::One;case GL_SRC_ALPHA:return ScreenBlendFactor::SourceAlpha;case GL_ONE_MINUS_SRC_ALPHA:return ScreenBlendFactor::OneMinusSourceAlpha;case GL_DST_ALPHA:return ScreenBlendFactor::DestinationAlpha;case GL_ONE_MINUS_DST_ALPHA:return ScreenBlendFactor::OneMinusDestinationAlpha;case GL_SRC_COLOR:return ScreenBlendFactor::SourceColor;case GL_ONE_MINUS_SRC_COLOR:return ScreenBlendFactor::OneMinusSourceColor;case GL_DST_COLOR:return ScreenBlendFactor::DestinationColor;case GL_ONE_MINUS_DST_COLOR:return ScreenBlendFactor::OneMinusDestinationColor;default:throw std::runtime_error("Unknown original placement blend factor");}}
void RecordEngine(std::span<const NativePlacementVertex> source,NativeDrawPrimitive primitive){
	Check(engineCapture&&frame,"Original placement engine draw outside capture");auto view=View();Check(view.clipDepth!=ClipDepthConvention::Unsupported,"Original placement clip convention unsupported");std::vector<NativeWorldDrawVertex> vertices;vertices.reserve(source.size());
	for(const auto& p:source){NativeWorldDrawVertex v{};v.color=p.color;std::array<float,4> world{p.position[0],p.position[1],p.position[2],1};for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)v.position[r]+=view.viewProjection[c*4+r]*world[c];if(view.clipDepth==ClipDepthConvention::ZeroToOne)v.position[2]=2*v.position[2]-v.position[3];vertices.push_back(v);}
	GetWorldDrawRecorder().Record(engineStage,primitive,vertices,engineSurface,engineState);
}
struct Program;
struct Context{std::weak_ptr<Program> active;bool heightBound=false;};
struct Buffer{
	std::shared_ptr<LuaVBOImpl> value;unsigned target;bool deleted=false;
	Buffer(lua_State* L,unsigned requested,bool frequent):target(requested){Check(target==GL_ARRAY_BUFFER||target==GL_ELEMENT_ARRAY_BUFFER,"Original Building Grid buffer target differs");value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);}
	void Define(int count,sol::optional<sol::object> layout){Check(!deleted,"Deleted Building Grid buffer");value->Define(count,layout);}
	size_t Upload(const sol::stack_table& data,sol::optional<int> attr,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Check(!deleted,"Deleted Building Grid buffer");return value->Upload(data,attr,offset,first,last);}
	void Delete(){if(!deleted)value->Delete();deleted=true;}
};
struct Program:std::enable_shared_from_this<Program>{
	std::shared_ptr<Context> context;std::array<float,3> mouse{},lineParameters{};float cameraFade=0,waterLevel=0;int waterMode=0;bool game31657=false,deleted=false;
	bool Initialize(){Check(!deleted&&Available(),"Original Building Grid real consumer unavailable");return true;}
	void Activate(){Initialize();context->active=shared_from_this();}
	void Deactivate(){if(context->active.lock().get()==this)context->active.reset();}
	void Finalize(){Deactivate();deleted=true;}
	std::shared_ptr<Program> ShaderObject(){return shared_from_this();}
};
enum class UniformKind {Mouse,Water,MouseFade,LineParameters};
struct Uniform{std::weak_ptr<Program> program;UniformKind kind;};
struct VertexArray{
	std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,indices,instances;bool deleted=false;
	void AttachVertexBuffer(std::shared_ptr<Buffer> b){Check(!deleted&&b&&!b->deleted&&b->target==GL_ARRAY_BUFFER,"Original Building Grid vertex attachment invalid");vertices=std::move(b);}
	void AttachIndexBuffer(std::shared_ptr<Buffer> b){Check(!deleted&&b&&!b->deleted&&b->target==GL_ELEMENT_ARRAY_BUFFER,"Original Building Grid index attachment invalid");indices=std::move(b);}
	void AttachInstanceBuffer(std::shared_ptr<Buffer> b){Check(!deleted&&b&&!b->deleted&&b->target==GL_ARRAY_BUFFER,"Original Building Grid instance attachment invalid");instances=std::move(b);}
	void Delete(){deleted=true;vertices.reset();indices.reset();instances.reset();}
	void DrawArrays(unsigned primitive,sol::optional<int> count,sol::optional<int> first){
		Check(!deleted&&frame&&Available()&&vertices&&!vertices->deleted&&primitive==GL_LINES&&context->heightBound,"Original Building Grid draw attachments differ");auto p=context->active.lock();Check(p&&!p->deleted,"Original Building Grid active program missing");
		auto stage=ui::GetRecorder().CurrentWorldStage();Check(stage&&*stage==NativeWorldDrawStage::PreUnit,"Original Building Grid source stage differs");
		NativeBuildingGridDraw d;d.vertices=vertices->value->CaptureNativeSnapshot();Check(first.value_or(0)>=0&&count.value_or(int(d.vertices->layout.elements))>=0,"Original Building Grid negative range");d.firstVertex=first.value_or(0);d.vertexCount=count.value_or(int(d.vertices->layout.elements));d.view=View();d.heights=Heights();d.mapWorldSize={float(mapDims.mapx*8),float(mapDims.mapy*8)};d.mousePosition=p->mouse;d.waterLevel=p->waterLevel;d.waterSurfaceMode=p->waterMode;
		d.generatedPatch31657=p->game31657;d.cameraFade=p->cameraFade;d.lineParameters=p->lineParameters;
		if(p->game31657){Check(indices&&!indices->deleted&&instances&&!instances->deleted,"Pinned31657 Building Grid unused source attachments missing");d.unusedIndices=indices->value->CaptureNativeSnapshot();d.unusedInstances=instances->value->CaptureNativeSnapshot();}d.surface=ui::GetRecorder().CurrentState().draw;d.world=ui::GetRecorder().CurrentWorldState();ValidateNativeBuildingGrid(d);d.sequence=NextNativeWorldDrawSequence();frame->grids.push_back(std::move(d));
	}
};
sol::table CreateContext(lua_State* L,bool game31657=false){
	Check(!CLuaHandle::GetHandleSynced(L)&&Available(),"Original Building Grid needs unsynced connected family");sol::state_view lua(L);auto t=lua.create_table();auto c=std::make_shared<Context>();
	BindFunction(t,"GetVBO",[L](unsigned target,bool frequent){return std::make_shared<Buffer>(L,target,frequent);});
	BindFunction(t,"GetVAO",[c](){auto v=std::make_shared<VertexArray>();v->context=c;return v;});
	BindFunction(t,"CreateProgram",[c,game31657](const std::string& name,const std::string& vs,const std::string& fs,const std::string& engine,const std::string& defines,const sol::table& floats,const sol::table& ints){
		Check(name=="gridShader","Unknown original Building Grid program");ValidateNativeBuildingGridProgram(vs,fs,engine,defines,game31657,{float(mapDims.mapx*8),float(mapDims.mapy*8)});auto p=std::make_shared<Program>();p->context=c;p->game31657=game31657;size_t nf=0,ni=0;
		for(const auto& item:floats){auto key=item.first.as<std::string>();
			if(game31657){Check(key=="mouseFade"||key=="lineParams","Unknown pinned31657 Building Grid uniform");auto a=item.second.as<sol::table>();const unsigned count=key=="mouseFade"?4:3;Check(a.size()==count,"Pinned31657 Building Grid uniform dimensions differ");for(unsigned i=1;i<=count;++i)Check(a.get<float>(i)==0,"Pinned31657 Building Grid initial uniform differs");}
			else if(key=="waterLevel"){p->waterLevel=item.second.as<float>();Check(std::isfinite(p->waterLevel),"Nonfinite Building Grid water level");}else{Check(key=="mousePos","Unknown original Building Grid uniform");auto a=item.second.as<sol::table>();Check(a.size()==3,"Original Building Grid mouse dimensions differ");for(unsigned i=0;i<3;++i){p->mouse[i]=a[i+1];Check(p->mouse[i]==0,"Original Building Grid initial mouse differs");}}++nf;}
		for(const auto& item:ints){auto key=item.first.as<std::string>();Check((key=="heightmapTex"||(!game31657&&key=="waterSurfaceMode"))&&item.second.as<int>()==0,"Original Building Grid integer uniform differs");++ni;}Check(nf==2&&ni==(game31657?1u:2u),"Original Building Grid required uniforms missing");return p;
	});
	BindFunction(t,"GetUniformLocation",[](std::shared_ptr<Program> p,const std::string& name){
		Check(p&&!p->deleted,"Deleted original Building Grid uniform program");
		if(p->game31657){Check(name=="mouseFade"||name=="lineParams","Unknown pinned31657 Building Grid uniform location");return Uniform{p,name=="mouseFade"?UniformKind::MouseFade:UniformKind::LineParameters};}
		Check(name=="mousePos"||name=="waterSurfaceMode","Unknown original Building Grid uniform location");return Uniform{p,name=="waterSurfaceMode"?UniformKind::Water:UniformKind::Mouse};
	});
	BindFunction(t,"Uniform",[](const Uniform& u,float x,float y,float z,sol::optional<float> w){
		auto p=u.program.lock();Check(p&&!p->deleted&&u.kind!=UniformKind::Water&&p->context->active.lock()==p&&std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z),"Original Building Grid float uniform binding differs");
		if(u.kind==UniformKind::MouseFade){Check(w.has_value()&&std::isfinite(*w),"Pinned31657 Building Grid mouseFade needs four finite components");p->mouse={x,y,z};p->cameraFade=*w;}
		else{Check(!w.has_value(),"Original Building Grid three-component uniform has extra value");if(u.kind==UniformKind::LineParameters)p->lineParameters={x,y,z};else p->mouse={x,y,z};}
	});
	BindFunction(t,"UniformInt",[](const Uniform& u,int value){auto p=u.program.lock();Check(p&&!p->deleted&&u.kind==UniformKind::Water&&p->context->active.lock()==p&&(value==0||value==1),"Original Building Grid integer uniform binding differs");p->waterMode=value;});
	BindFunction(t,"Texture",[c](int unit,sol::object value){Check(unit==0,"Original Building Grid height unit differs");if(value.is<bool>()){Check(!value.as<bool>(),"Original Building Grid texture enable lacks binding");c->heightBound=false;}else{Check(value.as<std::string>()=="$heightmap","Original Building Grid texture differs");c->heightBound=true;}return true;});return t;
}
}
void SetModelProvider(ModelProvider p){modelProvider=std::move(p);}
void BeginWorldFrame(){Check(!frame,"Nested placement frame");frame=std::make_shared<NativePlacementFrame>();LuaNativeBuildSquare::BeginWorldFrame();}
std::shared_ptr<const rendering::NativePlacementFrame> EndWorldFrame(){Check(frame&&!engineCapture,"Placement frame/scope unbalanced");frame->buildSquares=LuaNativeBuildSquare::EndWorldFrame();return std::exchange(frame,{});}
void AbortWorldFrame()noexcept{LuaNativeBuildSquare::AbortWorldFrame();frame.reset();engineCapture=false;}
void Reset(){AbortWorldFrame();heightSnapshot.reset();}
EngineScope::EngineScope(float lineWidth,unsigned source,unsigned destination,NativeWorldDrawStage stage,bool requirePlacementResources){
	Check(stage==NativeWorldDrawStage::World||stage==NativeWorldDrawStage::PreUnit,"Original placement engine stage differs");
	// Line-only engine ranges use RecordEngine, which needs the open frame and
	// world recorder, but neither the building-grid provider nor model provider.
	// Validate before touching an existing scope's state (including failed nesting).
	Check(frame&&!engineCapture&&(!requirePlacementResources||Available()),"Original placement capture unavailable or nested");
	engineStage=stage;engineColor={1,1,1,1};engineState={};engineSurface={};
	engineState.depthTest=true;engineState.depthWrite=false;engineState.lineWidth=lineWidth;
	EngineBlend(source,destination);engineCapture=true;
}
EngineScope::~EngineScope(){engineCapture=false;}
bool CapturingEngine()noexcept{return engineCapture;}
void EngineDepthTest(bool value){if(engineCapture)engineState.depthTest=value;}
void EngineBlend(unsigned source,unsigned destination){engineSurface.blend.sourceColor=engineSurface.blend.sourceAlpha=Blend(source);engineSurface.blend.destinationColor=engineSurface.blend.destinationAlpha=Blend(destination);}
void RecordEngineLineLoop(std::span<const float3> points,const SColor& color){const float4 c=color;std::vector<rendering::NativePlacementVertex> v;for(const auto& p:points)v.push_back({{p.x,p.y,p.z},{c.x,c.y,c.z,c.w}});RecordEngine(v,rendering::NativeDrawPrimitive::LineLoop);}
void RecordEngineLines(std::span<const float3> points,rendering::ScreenColor color)
{
	std::vector<NativePlacementVertex> vertices; vertices.reserve(points.size());
	for (const auto& p: points) vertices.push_back({{p.x,p.y,p.z},color});
	RecordEngine(vertices,NativeDrawPrimitive::Lines);
}
void RecordEngineCulledTriangles(std::span<const float3> points,rendering::ScreenColor color,bool cullFront)
{
	std::vector<NativePlacementVertex> vertices; vertices.reserve(points.size());
	for (const auto& p: points) vertices.push_back({{p.x,p.y,p.z},color});
	// Keep per-draw culling local, including a failed projection or recorder call.
	const auto previous = engineState;
	engineState.cullEnabled = true;
	engineState.cull = cullFront ? NativeWorldCull::Front : NativeWorldCull::Back;
	try { RecordEngine(vertices,NativeDrawPrimitive::Triangles); }
	catch (...) { engineState = previous; throw; }
	engineState = previous;
}
void RecordBuildSquare(const BuildInfo& bi,float h,bool canBuild,std::span<const uint8_t> status){
	EngineDepthTest(false);EngineBlend(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);auto v=rendering::NativeBuildSquareVertices({bi.pos.x,bi.pos.y,bi.pos.z},bi.GetXSize(),bi.GetZSize(),h,canBuild,status,CGameHelper::BUILDSQUARE_OPEN,CGameHelper::BUILDSQUARE_OCCUPIED,CGameHelper::BUILDSQUARE_RECLAIMABLE);RecordEngine(v,rendering::NativeDrawPrimitive::Lines);EngineDepthTest(true);
}
void EngineColor(ScreenColor value){if(engineCapture)engineColor=value;}
void RecordQueuedBuildSites(std::span<const float> quads,std::span<const float> waterQuads,
	std::span<const float> lines,std::span<const float> colors)
{
	Check(engineCapture&&engineStage==NativeWorldDrawStage::PreUnit&&quads.size()%12==0&&waterQuads.size()%12==0&&lines.size()%6==0&&colors.size()==lines.size()/3*4,"Original queued build-site array shape differs");
	auto loops=[&](std::span<const float> data,ScreenColor color){
		for(size_t first=0;first<data.size();first+=12){std::array<NativePlacementVertex,4> v;
			for(unsigned n=0;n<4;++n)v[n]={{data[first+n*3],data[first+n*3+1],data[first+n*3+2]},color};
			RecordEngine(v,NativeDrawPrimitive::LineLoop);}
	};
	loops(quads,engineColor);loops(waterQuads,{0,.5f,1,1});
	if(!lines.empty()){std::vector<NativePlacementVertex> v;v.reserve(lines.size()/3);
		for(size_t n=0;n<lines.size()/3;++n)v.push_back({{lines[n*3],lines[n*3+1],lines[n*3+2]},{colors[n*4],colors[n*4+1],colors[n*4+2],colors[n*4+3]}});
		RecordEngine(v,NativeDrawPrimitive::Lines);}
}
void RecordBuildingSample(const BuildInfo& bi,int team){
	Check(engineCapture&&frame&&modelProvider&&teamHandler.IsValidTeam(team),"Original building sample capture unavailable");auto* model=bi.def->LoadModel();if(!model)return;
	NativeBuildingSample d;d.model=modelProvider(model);d.modelTextures=CaptureNativeS3OModelTextures(model);CMatrix44f m;m.Translate(bi.pos);m.RotateY(-bi.buildFacing*math::HALFPI);std::copy_n(m.m,16,d.world.begin());auto* t=teamHandler.Team(team);std::copy_n(t->color,4,d.teamColor.begin());d.alpha=IModelDrawerState::alphaValues.x;
	const auto* playerCamera=CCameraHandler::GetCamera(CCamera::CAMTYPE_PLAYER);auto p=playerCamera->GetViewProjectionMatrixInverse()*float4(0,0,0,1);Check(p.w!=0,"Original building sample inverse projection singular");d.legacyCameraPosition={p.x/p.w,p.y/p.w,p.z/p.w};d.requestedDynamicLights=configHandler->GetInt("MaxDynamicModelLights");d.modelLights=CaptureNativeModelLights();ValidateNativeBuildingSample(d);d.sequence=NextNativeWorldDrawSequence();frame->samples.push_back(std::move(d));
}
bool PushEntries(lua_State* L){
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();family.new_usertype<Buffer>("Buffer",sol::no_constructor,"Define",&Buffer::Define,"Upload",&Buffer::Upload,"Delete",&Buffer::Delete);family.new_usertype<VertexArray>("VertexArray",sol::no_constructor,"AttachVertexBuffer",&VertexArray::AttachVertexBuffer,"AttachIndexBuffer",&VertexArray::AttachIndexBuffer,"AttachInstanceBuffer",&VertexArray::AttachInstanceBuffer,"DrawArrays",&VertexArray::DrawArrays,"Delete",&VertexArray::Delete);family.new_usertype<Uniform>("Uniform",sol::no_constructor);family.new_usertype<Program>("Program",sol::no_constructor,"Initialize",&Program::Initialize,"Activate",&Program::Activate,"Deactivate",&Program::Deactivate,"Finalize",&Program::Finalize,"shaderObj",sol::property(&Program::ShaderObject));family.set("Buffer",sol::lua_nil);family.set("VertexArray",sol::lua_nil);family.set("Uniform",sol::lua_nil);family.set("Program",sol::lua_nil);
	family.set_function("Available",Available);BindFunction(family,"CreateContext",[L](){return CreateContext(L);});
	BindFunction(family,"LoadArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L)&&(path=="LuaUI/Widgets/gui_building_grid_gl4.lua"||path=="modules/graphics/luashader.lua"),"Unknown original Building Grid unsynced archive path");CFileHandler f(path,SPRING_VFS_ZIP);std::string s;Check(f.FileExists()&&f.LoadStringData(s),"Original Building Grid archive source missing");return s;});
	BindFunction(family,"PrepareSource",[](std::string s){auto h=Hash(s);Check(h==NativeBuildingGridWidgetHash||h==NativeBuildingGridHelperHash,"Original Building Grid source identity differs");LuaUtils::TracyRemoveAlsoExtras(s.data());return s;});
	BindFunction(family,"CreateContext31657",[L](){return CreateContext(L,true);});
	BindFunction(family,"LoadArchiveSource31657",[L](const std::string& path){
		Check(!CLuaHandle::GetHandleSynced(L),"Pinned31657 Building Grid archive is unsynced only");
		for(const auto& entry:NativeBuildingGridSources31657)if(entry.path==path){CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&Hash(source)==entry.hash,"Pinned31657 Building Grid archive identity differs");return source;}
		throw std::runtime_error("Unknown pinned31657 Building Grid archive path");
	});
	BindFunction(family,"PrepareSource31657",[](std::string source){const auto hash=Hash(source);bool known=false;for(const auto& entry:NativeBuildingGridSources31657)known|=hash==entry.hash;Check(known,"Unsupported pinned31657 Building Grid source identity");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
	gl.set("NativePlacement",family);return true;
}
}
#else
namespace LuaNativePlacement {
void SetModelProvider(ModelProvider){}void BeginWorldFrame(){}std::shared_ptr<const rendering::NativePlacementFrame> EndWorldFrame(){return {};}
void AbortWorldFrame()noexcept{}void Reset(){}bool PushEntries(lua_State*){return true;}
EngineScope::EngineScope(float,unsigned,unsigned,rendering::NativeWorldDrawStage,bool){}EngineScope::~EngineScope(){}bool CapturingEngine()noexcept{return false;}
void EngineDepthTest(bool){}void EngineBlend(unsigned,unsigned){}void RecordEngineLineLoop(std::span<const float3>,const SColor&){}
void RecordEngineLines(std::span<const float3>,rendering::ScreenColor){}
void RecordEngineCulledTriangles(std::span<const float3>,rendering::ScreenColor,bool){}
void RecordBuildSquare(const BuildInfo&,float,bool,std::span<const uint8_t>){}void EngineColor(rendering::ScreenColor){}
void RecordQueuedBuildSites(std::span<const float>,std::span<const float>,std::span<const float>,std::span<const float>){}
void RecordBuildingSample(const BuildInfo&,int){}
}
#endif
