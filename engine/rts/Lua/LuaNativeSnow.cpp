/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeSnow.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaUtils.h"
#include "LuaNativeUI.h"
#include "lib/squish/squish.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeSnowData.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <utility>
namespace LuaNativeSnow {
namespace {
using namespace rendering;
NativeSnowRecorder recorder;
void Check(bool valid,const char* why) { if(!valid) throw std::runtime_error(why); }
std::uint64_t Hash(std::string_view text)
{
	std::uint64_t hash=14695981039346656037ULL;
	for(unsigned char byte:text) hash=(hash^byte)*1099511628211ULL;
	return hash;
}
template<typename Signature> struct LuaBoundary;
template<typename R,typename... Args> struct LuaBoundary<R(Args...)> {
	template<typename F> static auto Wrap(F function) { return [function=std::move(function)](sol::this_state lua,Args... args)->R {
		try { return function(std::forward<Args>(args)...); }
		catch(const std::exception& error) { lua_pushstring(lua,error.what()); }
		lua_error(lua); std::terminate();
	}; }
};
template<typename T,typename F> void Bind(T& table,const char* name,F function)
{
	table.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(function)));
}
struct Program;
struct Context;
struct PointList {
	std::weak_ptr<Context> owner;
	std::shared_ptr<const NativeSnowPoints> points;
	bool deleted=false;
	void Delete() noexcept { deleted=true;points.reset(); }
};
struct Context {
	std::weak_ptr<Program> active;
	std::vector<std::weak_ptr<PointList>> lists;
	std::vector<std::weak_ptr<Program>> programs;
	NativeSnowPointState points;
	std::shared_ptr<const NativeCUSTextureObject> originalTexture;
	std::shared_ptr<const ScreenTexture> decodedTexture;
	bool closed=false;
	void Close();
};
std::vector<std::weak_ptr<Context>> contexts;
unsigned Arity(const std::string& name)
{
	return name=="time"||name=="scale"||name=="sizeMult"?1:name=="speed"||name=="camPos"?3:name=="snowColor"?4:0;
}
struct Program:std::enable_shared_from_this<Program> {
	std::shared_ptr<Context> context;
	NativeSnowUniforms uniforms;
	bool deleted=false;
	void Activate() { Check(!deleted&&!context->closed,"Deleted/closed original Snow program");context->active=shared_from_this(); }
	void Deactivate() { if(context->active.lock().get()==this) context->active.reset(); }
	void Finalize() { Deactivate();deleted=true; }
	void Set(const std::string& name,const std::vector<float>& values)
	{
		Check(!deleted&&!context->closed,"Deleted/closed original Snow uniform target");
		const auto count=Arity(name);Check(count&&values.size()==count,"Original Snow uniform name/arity differs");
		for(float value:values) Check(std::isfinite(value),"Original Snow uniform is nonfinite");
		if(name=="time") uniforms.time=values[0];
		else if(name=="scale") uniforms.scale=values[0];
		else if(name=="sizeMult") uniforms.sizeMult=values[0];
		else if(name=="speed") std::copy_n(values.begin(),3,uniforms.speed.begin());
		else if(name=="camPos") std::copy_n(values.begin(),3,uniforms.camPos.begin());
		else std::copy_n(values.begin(),4,uniforms.snowColor.begin());
	}
	void SetUniform(const std::string& name,sol::variadic_args args)
	{
		std::vector<float> values;values.reserve(args.size());
		for(const auto& arg:args) values.push_back(arg.as<float>());
		Set(name,values);
	}
};
void Context::Close()
{
	active.reset();closed=true;
	for(auto& weak:lists) if(auto list=weak.lock()) list->Delete();
	for(auto& weak:programs) if(auto program=weak.lock()) program->Finalize();
	lists.clear();programs.clear();points={};originalTexture.reset();decodedTexture.reset();
}
void Draw(const std::shared_ptr<Context>& context,const std::shared_ptr<PointList>& list)
{
	Check(!context->closed&&IsNativeSnowConsumerAvailable()&&recorder.Active(),"Original Snow consumer/frame is unavailable");
	Check(list&&list->owner.lock()==context&&!list->deleted&&list->points,"Original Snow list is deleted/from another context");
	auto program=context->active.lock();Check(program&&!program->deleted,"Original Snow active program is missing");
	auto& common=ui::GetRecorder();
	Check(common.CurrentWorldStage()==NativeWorldDrawStage::World&&!common.HasActiveTarget()&&!common.IsCompiling(),"Original Snow requires its actual immediate DrawWorld callin");
	auto* observer=common.TextureObserver();Check(observer,"Original Snow has no actual shared sampler0 image owner");
	// Shader sampler0 reads its bound object independently of fixed texture enables.
	const auto sampled=observer->BoundImage(0);
	const auto actual=OriginalNativeTextureContext().History().Binding(0,NativeCUSTextureTarget::Image2D);
	Check(context->originalTexture&&actual.object==context->originalTexture&&context->decodedTexture,"Original Snow sampler0 no longer names its actual source object");
	NativeSnowDraw draw;draw.points=list->points;draw.uniforms=program->uniforms;draw.pointState=context->points;
	draw.modelView=common.GetMatrix(ui::NativeUIMatrixMode::ModelView);draw.projection=common.GetMatrix(ui::NativeUIMatrixMode::Projection);
	draw.texture=context->decodedTexture;draw.originalTexture=context->originalTexture;draw.sampler=sampled.sampler;draw.surface=common.CurrentState().draw;draw.world=common.CurrentWorldState();
	recorder.Record(std::move(draw));
}
sol::table CreateContext(lua_State* L)
{
	Check(!CLuaHandle::GetHandleSynced(L)&&IsNativeSnowConsumerAvailable(),"Original Snow needs its actual unsynced native consumer");
	sol::state_view lua(L);auto table=lua.create_table();auto context=std::make_shared<Context>();
	std::erase_if(contexts,[](const auto& weak){return weak.expired();});contexts.emplace_back(context);
	Bind(table,"CreatePointList",[context](const sol::table& source) {
		Check(!context->closed,"Closed original Snow context");
		auto points=std::make_shared<NativeSnowPoints>();points->reserve(source.size());
		for(std::size_t i=1;i<=source.size();++i) {
			const auto row=source.get<sol::table>(i);Check(row.size()==4,"Original Snow point tuple arity differs");
			std::array<float,4> point;
			for(unsigned k=0;k<4;++k){point[k]=row.get<float>(k+1);Check(std::isfinite(point[k]),"Original Snow point is nonfinite");}
			Check(point[0]>=0&&point[0]<=1&&point[1]>=0&&point[1]<=1&&point[2]>=0&&point[2]<=1&&point[3]==1,"Original Snow normalized point source differs");
			points->push_back(point);
		}
		auto list=std::make_shared<PointList>();list->owner=context;list->points=std::move(points);
		std::erase_if(context->lists,[](const auto& weak){return weak.expired();});context->lists.emplace_back(list);return list;
	});
	Bind(table,"DeletePointList",[context](const std::shared_ptr<PointList>& list) {
		Check(list&&list->owner.lock()==context,"Original Snow list owner differs");list->Delete();
	});
	Bind(table,"DrawPointList",[context](const std::shared_ptr<PointList>& list){Draw(context,list);});
	Bind(table,"SetPointSprite",[context](sol::variadic_args args) {
		Check(!context->closed&&args.size()>=1&&args.size()<=3,"Original Snow point-sprite arguments differ");
		context->points.sprite=args[0].as<bool>();
		if(args.size()>=2) context->points.coordReplace=args[1].as<bool>();
		if(args.size()>=3) context->points.upperLeft=args[2].as<bool>();
	});
	Bind(table,"SetPointParameter",[context](sol::variadic_args args) {
		Check(!context->closed&&args.size()>=3&&args.size()<=6,"Original Snow point-parameter arity differs");
		std::array<float,6> values{};
		for(unsigned i=0;i<args.size();++i){values[i]=args[i].as<float>();Check(std::isfinite(values[i]),"Original Snow point parameter is nonfinite");}
		std::copy_n(values.begin(),3,context->points.attenuation.begin());
		if(args.size()>=4){Check(values[3]>=0,"Negative original point size minimum");context->points.sizeMin=values[3];}
		if(args.size()>=5){Check(values[4]>=0,"Negative original point size maximum");context->points.sizeMax=values[4];}
		if(args.size()>=6){Check(values[5]>=0,"Negative original point fade threshold");context->points.fadeThreshold=values[5];}
	});
	Bind(table,"ResetPointState",[context](){context->points={};});
	Bind(table,"PrepareTexture",[context,L](const std::string& name) {
		Check(!context->closed&&name=="LuaUI/Images/snow.dds","Original Snow texture identity differs");
		const auto lookup=LuaNativeUI::CaptureTextureLookup(L,name);
		Check(lookup.parsed&&lookup.object,"Original Snow named texture has no actual source object");
		if(context->originalTexture==lookup.object&&context->decodedTexture) return;
		const auto binding=LuaNativeUI::ResolveTextureBinding(L,name);
		Check(binding&&binding->current,"Original Snow image loader has no source bytes");
		const auto& source=*binding->current;
		Check(source.format==ScreenTextureFormat::BC3RGBA&&source.width==64&&source.height==64&&source.mips.size()==1&&source.mips[0].bytes.size()==4096,
			"Original31657 Snow must be its actual64x64 base-only DXT5 source");
		auto decoded=std::make_shared<ScreenTexture>();
		decoded->sourceName=source.sourceName;decoded->generation=source.generation;decoded->format=ScreenTextureFormat::RGBA8Unorm;
		decoded->width=source.width;decoded->height=source.height;decoded->bytesPerRow=source.width*4;decoded->flipV=source.flipV;
		decoded->bytes.resize(std::size_t(source.width)*source.height*4);
		// Reuse the engine's linked BC decoder on the original loader's already
		// flipped bytes. ScreenGPU generates the complete linear-filtered chain
		// on the same submission; no source artwork or sampler quality changes.
		squish::DecompressImage(decoded->bytes.data(),source.width,source.height,source.mips[0].bytes.data(),squish::kDxt5);
		decoded->generateMipmaps=true;
		context->originalTexture=lookup.object;context->decodedTexture=std::move(decoded);
	});
	Bind(table,"Close",[context](){context->Close();});
	Bind(table,"CreateProgram",[context](const std::string& name,const std::string& vertex,const std::string& fragment,const sol::table& floats,const sol::table& integers) {
		Check(!context->closed&&name=="Snow Shader"&&Hash(vertex)==1667757316263175468ULL&&Hash(fragment)==14947375303480434960ULL,"Original Snow program identity differs");
		Check(integers.empty(),"Original Snow sampler must retain its actual link-default tex0=0");
		auto program=std::make_shared<Program>();program->context=context;unsigned mask=0;
		for(const auto& entry:floats) {
			const auto name=entry.first.as<std::string>();const unsigned count=Arity(name);Check(count,"Unknown initial original Snow uniform");
			std::vector<float> values;
			if(count==1) values.push_back(entry.second.as<float>());
			else {const auto row=entry.second.as<sol::table>();Check(row.size()==count,"Original Snow initial vector arity differs");for(unsigned i=1;i<=count;++i) values.push_back(row.get<float>(i));}
			program->Set(name,values);
			mask|=name=="time"?1:name=="scale"?2:name=="speed"?4:name=="camPos"?8:name=="snowColor"?16:32;
		}
		Check(mask==63,"Original Snow initial uniform is missing");
		std::erase_if(context->programs,[](const auto& weak){return weak.expired();});context->programs.emplace_back(program);return program;
	});
	return table;
}
}
void BeginWorldFrame() { recorder.Begin(globalRendering->drawFrame,OriginalCUSUniformCapture().Current()); }
std::shared_ptr<const NativeSnowFrame> EndWorldFrame() { return recorder.End(); }
void AbortWorldFrame() noexcept
{
	recorder.Abort();for(auto& weak:contexts) if(auto context=weak.lock()) context->active.reset();
}
void Reset()
{
	AbortWorldFrame();for(auto& weak:contexts) if(auto context=weak.lock()) context->Close();contexts.clear();
}
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();
	auto program=family.new_usertype<Program>("Program",sol::no_constructor);
	Bind(program,"Activate",[](Program& self){self.Activate();});Bind(program,"Deactivate",[](Program& self){self.Deactivate();});
	Bind(program,"Finalize",[](Program& self){self.Finalize();});
	Bind(program,"SetUniform",[](Program& self,const std::string& name,sol::variadic_args args){self.SetUniform(name,args);});
	family.new_usertype<PointList>("PointList",sol::no_constructor);family.set("Program",sol::lua_nil);family.set("PointList",sol::lua_nil);
	Bind(family,"Available",&IsNativeSnowConsumerAvailable);Bind(family,"CreateContext",[L](){return CreateContext(L);});
	Bind(family,"LoadArchiveSource",[L](const std::string& path) {
		Check(!CLuaHandle::GetHandleSynced(L)&&path=="LuaUI/Widgets/gfx_snow.lua","Unknown/synced original Snow source request");
		CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source),"Original Snow archive source is missing");
		Check(Hash(source)==11791695488788498155ULL,"Original31657 Snow widget identity differs");return source;
	});
	Bind(family,"PrepareSource",[](std::string source) {
		Check(Hash(source)==11791695488788498155ULL,"Original Snow source identity differs");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;
	});
	gl.set("NativeSnow",family);return true;
}
}
#else
namespace LuaNativeSnow {
void BeginWorldFrame() {} std::shared_ptr<const rendering::NativeSnowFrame> EndWorldFrame(){return {};}
void AbortWorldFrame() noexcept {} void Reset() {} bool PushEntries(lua_State*) {return true;}
}
#endif
