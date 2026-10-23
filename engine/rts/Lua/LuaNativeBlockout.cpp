/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeBlockout.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaUtils.h"
#include "LuaVBOImpl.h"
#include "LuaNativeUI.h"
#include "LuaNativePostprocess.h"
#include "lib/sol2/sol.hpp"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Models/3DModel.hpp"
#include "Rendering/Models/ModelsMemStorage.h"
#include "Rendering/Backend/NativeBlockoutPrograms.h"
#include "Rendering/Backend/NativeBlueprintProgramIdentity.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativePostprocessRecorder.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/EngineBridge/NativeS3OModelTextureCapture.h"
#include "Sim/Features/FeatureDef.h"
#include "System/FileSystem/FileHandler.h"
#include "System/Sync/SHA512.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>
#include <utility>
namespace LuaNativeBlockout {
namespace {
using namespace rendering;
ModelProvider models;
TargetAllocator allocate;
std::shared_ptr<NativeBlockoutFrame> frame;
void Check(bool x,const char* reason){if(!x)throw std::runtime_error(reason);}
template<class S>struct Boundary;
template<class R,class... A>struct Boundary<R(A...)>{
	template<class F>static auto Wrap(F fn){return[fn=std::move(fn)](A...args)->R{
		try{return fn(std::forward<A>(args)...);}catch(const std::exception& e){throw sol::error(e.what());}
	};}
};
template<class T,class F>void Bind(T& t,const char* name,F fn){t.set_function(name,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(fn)));}
bool Available(){return IsNativeBlockoutConsumerAvailable()&&models&&allocate;}
uint64_t Hash(std::string_view s){uint64_t h=14695981039346656037ULL;for(unsigned char c:s)h=(h^c)*1099511628211ULL;return h;}
std::string Source(const std::string& path){
	const std::map<std::string,std::string> allowed{
		{"LuaUI/Widgets/map_blockout_gl4.lua","742bd977ffebcc63c7892a6f87e6ab5a0683f247dff196ece47fd66c45ef05e0d14c0b6652047656e82053d0e4b5f1cf54f15e3cfc32ca4eee02ed3b1739a5bd"},
		{"modules/graphics/instancevboidtable.lua","2cee58b5961b796df996e44c17239a3756e468d2ac9472069629ba32f616195a9696a349d3c926f3b3309b07b70eb74f6d5ba4337d8a9355c96d83d0b0290732"},
		{"modules/graphics/luashader.lua","81858ab150aceb7d84f324708f3b3c4a5c8101df03c2a3a0597cc64413fea8ff7db558b711b18e930aa577bf7b0c6b96bfde83c342ca94508f72021742372436"}};
	auto pin=allowed.find(path);Check(pin!=allowed.end(),"Unknown original Blockout source");
	CFileHandler file(path,SPRING_VFS_ZIP);std::string text;Check(file.FileSize()>0&&file.FileSize()<512*1024&&file.LoadStringData(text),"Original Blockout source missing");
	sha512::raw_digest digest;sha512::calc_digest(reinterpret_cast<const uint8_t*>(text.data()),text.size(),digest.data());
	Check(sha512::dump_digest(digest)==pin->second,"Original31657 Blockout source identity differs");
	LuaUtils::TracyRemoveAlsoExtras(text.data());return text;
}
void Replace(std::string& text,std::string_view token,const std::string& value){
	size_t p=0;while((p=text.find(token,p))!=std::string::npos){text.replace(p,token.size(),value);p+=value.size();}
}
template<class T>T Read(const NativeBufferSnapshot& b,size_t row,size_t offset){
	Check(row<b.layout.elements&&offset+sizeof(T)<=b.layout.stride,"Blockout original instance range differs");
	T out;std::memcpy(&out,b.bytes.data()+row*b.layout.stride+offset,sizeof(T));return out;
}
struct Buffer;
struct Program;
struct FBO{
	std::shared_ptr<NativePostprocessBinding> color,depth;
	std::shared_ptr<const NativeBlockoutTarget> target;
	bool deleted=false;
	bool Live()const{return !deleted&&target&&target->storage&&color&&!color->deleted&&depth&&!depth->deleted;}
};
struct Context{
	std::weak_ptr<Program> active;
	std::map<std::string,std::weak_ptr<Buffer>> buffers;
	std::shared_ptr<const NativeS3OTexturePair> textures;
	std::shared_ptr<const ScreenTexture> normal;ScreenSampler normalSampler;
	bool clamp=false;
	NativeBlockoutDepthPass* depthPass=nullptr;
	void Abort(){active.reset();textures.reset();normal.reset();clamp=false;depthPass=nullptr;}
};
std::vector<std::weak_ptr<Context>> contexts;
struct Buffer:std::enable_shared_from_this<Buffer>{
	std::shared_ptr<LuaVBOImpl> value;std::weak_ptr<Context> context;GLenum target;bool modelView=false,deleted=false;std::string label;
	Buffer(lua_State* L,GLenum type,bool frequent):value(std::make_shared<LuaVBOImpl>(type,frequent)),target(type){Check(type==GL_ARRAY_BUFFER||type==GL_ELEMENT_ARRAY_BUFFER,"Blockout buffer target differs");CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);}
	void Owned()const{Check(!deleted&&!modelView,"Blockout CPU operation needs live owned buffer");}
	void Define(int n,sol::optional<sol::object> layout){Owned();value->Define(n,layout);}
	size_t Upload(const sol::stack_table& data,sol::optional<int> attr,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Owned();return value->Upload(data,attr,offset,first,last);}
	auto Download(sol::optional<int> attr,sol::optional<int> offset,sol::optional<int> count,sol::optional<bool> force){Owned();return value->Download(attr,offset,count,force);}
	void Delete(){if(!deleted)value->Delete();deleted=true;modelView=false;}
	auto GetBufferSize(){Owned();return value->GetBufferSize();}
	std::string GetID(){Owned();auto id=std::to_string(value->CaptureNativeSnapshot()->object);if(auto c=context.lock())c->buffers[id]=shared_from_this();return id;}
	const std::string& GetLabel()const{return label;}
	void ModelsVBO(){Check(!deleted&&Available(),"Blockout real model provider absent");modelView=true;}
	size_t Definition(int id,int attr,sol::optional<int> team,sol::optional<int> offset){Owned();return value->InstanceDataFromFeatureDefIDs(id,attr,team,offset);}
	size_t Definitions(const sol::stack_table& ids,int attr,sol::optional<int> team,sol::optional<int> offset){Owned();return value->InstanceDataFromFeatureDefIDs(ids,attr,team,offset);}
};
struct Program:std::enable_shared_from_this<Program>{
	std::shared_ptr<Context> context;bool deleted=false,normal=false;
	bool Initialize()const{Check(!deleted&&Available(),"Blockout actual pipelines not ready");return true;}
	void Activate(){Initialize();context->active=shared_from_this();}
	void Deactivate(){if(context->active.lock().get()==this)context->active.reset();}
	void Finalize(){Deactivate();deleted=true;}
	void Uniform(const std::string& name,float x){Check(!deleted&&name=="hasNormalTex"&&(x==0||x==1),"Blockout original uniform differs");normal=x!=0;}
};
const S3DModel* DefinitionModel(int id){
	auto* definition=LuaUtils::SolIdToObject<FeatureDef>(id,__func__);
	Check(definition&&definition->customParams.contains("blockout"),"Blockout source is not a designated FeatureDef");
	auto* source=definition->LoadModel();Check(source,"Blockout designated model missing");return source;
}
struct VertexArray{
	std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,indices,instances;
	struct Item{int id;std::shared_ptr<const WorldModel> model;};std::vector<Item> items;bool deleted=false;
	void Vertex(std::shared_ptr<Buffer> b){Check(!deleted&&b&&!b->deleted&&b->modelView&&b->target==GL_ARRAY_BUFFER,"Blockout model vertex view differs");vertices=std::move(b);}
	void Index(std::shared_ptr<Buffer> b){Check(!deleted&&b&&!b->deleted&&b->modelView&&b->target==GL_ELEMENT_ARRAY_BUFFER,"Blockout model index view differs");indices=std::move(b);}
	void Instance(std::shared_ptr<Buffer> b){Check(!deleted&&b&&!b->deleted&&!b->modelView&&b->target==GL_ARRAY_BUFFER,"Blockout instance view differs");instances=std::move(b);}
	int Add(int id){Check(!deleted&&vertices&&indices&&instances&&items.size()<instances->value->CaptureNativeSnapshot()->layout.elements,"Blockout submission attachments/capacity differ");auto model=models(DefinitionModel(id));Check(bool(model),"Blockout original geometry unavailable");items.push_back({id,std::move(model)});return int(items.size()-1);}
	int AddMany(const sol::stack_table& ids){auto first=items.size();for(size_t n=1;n<=ids.size();++n)Add(ids.raw_get<int>(n));return int(first);}
	void Clear(){Check(!deleted,"Blockout deleted VAO");items.clear();}
	void Remove(int index){Check(!deleted&&index>=0&&size_t(index)<items.size(),"Blockout remove index differs");items[index]=std::move(items.back());items.pop_back();}
	void Delete(){deleted=true;items.clear();vertices.reset();indices.reset();instances.reset();}
	void Submit(){
		Check(Available()&&frame&&!deleted&&context->clamp,"Blockout draw requires actual PreUnit frame and depth clamp");
		Check(ui::GetRecorder().CurrentWorldStage()==NativeWorldDrawStage::PreUnit,"Blockout source callin differs");
		auto program=context->active.lock();Check(program&&!program->deleted&&context->textures&&instances,"Blockout source program/material absent");
		NativeBlockoutDraw draw;draw.sequence=NextNativeWorldDrawSequence();draw.textures=context->textures;draw.hasNormal=program->normal;draw.normal=context->normal;draw.normalSampler=context->normalSampler;
		draw.surface=ui::GetRecorder().CurrentState().draw;draw.world=ui::GetRecorder().CurrentWorldState();
		draw.uniforms=OriginalCUSUniformCapture().Current();Check(draw.uniforms&&draw.uniforms->matrices&&draw.uniforms->params&&draw.uniforms->matrices->sourceDrawFrame==frame->sourceDrawFrame&&draw.uniforms->params->sourceDrawFrame==frame->sourceDrawFrame,"Blockout current actual UBO writers absent");
		std::memcpy(draw.viewProjection.data(),draw.uniforms->matrices->bytes.data()+5*64,64);
		const auto* view=CCameraHandler::GetCamera(CCamera::CAMTYPE_PLAYER);Check(view,"Blockout original camera absent");
		std::array<float,16> control;std::copy_n(view->GetClipControlMatrix().m,16,control.begin());std::array<float,16> identity{};identity[0]=identity[5]=identity[10]=identity[15]=1;
		if(control==identity)draw.clipDepth=ClipDepthConvention::MinusOneToOne;identity[10]=identity[14]=.5f;if(control==identity)draw.clipDepth=ClipDepthConvention::ZeroToOne;
		auto data=instances->value->CaptureNativeSnapshot();Check(data&&data->layout.stride==48,"Blockout instance layout differs");
		for(size_t row=0;row<items.size();++row){
			NativeBlockoutModel out;out.definition=items[row].id;out.model=items[row].model;out.positionYaw=Read<std::array<float,4>>(*data,row,0);out.parameters=Read<std::array<float,4>>(*data,row,16);out.sourceTransformBase=Read<uint32_t>(*data,row,32);
			unsigned maxPiece=0;for(const auto& piece:out.model->pieces)for(const auto& v:piece.vertices)maxPiece=std::max(maxPiece,unsigned(v.boneIds[0]&255));
			Check(out.sourceTransformBase<transformsMemStorage.GetSize()&&maxPiece<transformsMemStorage.GetSize()-out.sourceTransformBase,"Blockout real transform publication out of range");
			for(unsigned n=0;n<=maxPiece;++n){const auto& t=transformsMemStorage[out.sourceTransformBase+n];out.transforms.push_back({{t.r.x,t.r.y,t.r.z,t.r.r},{t.t.x,t.t.y,t.t.z,t.s}});}
			draw.models.push_back(std::move(out));
		}
		ValidateNativeBlockoutDraw(draw);
		if(context->depthPass)context->depthPass->draws.push_back(std::move(draw));else frame->draws.push_back(std::move(draw));
	}
};
sol::table Create(lua_State* L){
	Check(!CLuaHandle::GetHandleSynced(L)&&Available(),"Blockout needs unsynced actual complete model/target consumer");
	sol::state_view lua(L);auto table=lua.create_table();auto c=std::make_shared<Context>();contexts.emplace_back(c);sol::protected_function texture=lua["gl"]["Texture"];
	Bind(table,"GetVBO",[L,c](GLenum type,bool frequent){auto b=std::make_shared<Buffer>(L,type,frequent);b->context=c;return b;});
	Bind(table,"GetVAO",[c](){auto v=std::make_shared<VertexArray>();v->context=c;return v;});
	Bind(table,"CreateProgram",[c](const std::string& vertex,const std::string& fragment,const std::string& engine,const std::string& quaternion,const std::string& defines,const sol::table& floats,const sol::table& ints){
		Check(Hash(engine)==NativeBlueprintEngineDefinitionsHash31657&&Hash(quaternion)==NativeBlueprintQuaternionHash31657&&defines=="#define USEQUATERNIONS 1\n","Blockout engine/quaternion/define contract differs");
		std::string expectedVS(blockoutsource::Vertex),expectedFS(blockoutsource::Fragment);for(auto* text:{&expectedVS,&expectedFS}){Replace(*text,"//__ENGINEUNIFORMBUFFERDEFS__",engine);Replace(*text,"//__QUATERNIONDEFS__",quaternion);Replace(*text,"//__DEFINES__",defines);}
		Check(vertex==expectedVS&&fragment==expectedFS,"Blockout original expanded shader source differs");
		size_t floatCount=0,intCount=0;
		for(const auto& entry:floats){Check(entry.first.as<std::string>()=="hasNormalTex"&&entry.second.as<float>()==0,"Blockout initial float uniform differs");++floatCount;}
		for(const auto& entry:ints){auto key=entry.first.as<std::string>();int unit=entry.second.as<int>();Check((key=="tex1"&&unit==0)||(key=="tex2"&&unit==1)||(key=="normalTex"&&unit==2),"Blockout original sampler differs");++intCount;}
		Check(floatCount==1&&intCount==3,"Blockout required uniform missing");
		auto p=std::make_shared<Program>();p->context=c;return p;
	});
	Bind(table,"DepthClamp",[c](bool enable){c->clamp=enable;});
	Bind(table,"FeatureShapeTextures",[c](sol::optional<int> id,bool enabled){
		if(!enabled){c->textures.reset();return;}Check(id.has_value(),"Blockout feature texture definition absent");
		auto selected=CaptureNativeS3OModelTextures(DefinitionModel(*id));Check(bool(selected),"Blockout actual S3O pair absent");
		auto& actual=OriginalNativeTextureContext();Check(actual.Initialized()&&actual.Registry().ContextGeneration()==selected->contextGeneration,"Blockout texture context differs");
		for(unsigned unit:{1u,0u}){actual.History().ActiveTexture(unit);actual.Registry().Bind(unit,selected->objects[unit]->name,NativeCUSTextureTarget::Image2D);ui::GetRecorder().SyncObservedTextureBinding(unit);}
		c->textures=std::move(selected);
	});
	Bind(table,"Texture",[L,c,texture](unsigned unit,sol::object name){
		Check(unit==2,"Blockout normal sampler unit differs");auto result=texture(unit,name);if(!result.valid()){sol::error error=result;throw std::runtime_error(error.what());}
		if(name.is<bool>()){Check(!name.as<bool>(),"Blockout normal enable lacks a binding");c->normal.reset();return true;}
		auto binding=LuaNativeUI::ResolveTextureBinding(L,name.as<std::string>());Check(binding&&binding->current&&!binding->postprocess,"Blockout normal source image absent");c->normal=binding->current;c->normalSampler=binding->sampler;return true;
	});
	Bind(table,"ObjectLabel",[c](GLenum type,const std::string& id,const std::string& label){Check(type==0x82e0&&label.size()<=1024,"Blockout buffer label differs");auto it=c->buffers.find(id);Check(it!=c->buffers.end(),"Blockout unknown buffer identity");auto b=it->second.lock();Check(bool(b),"Blockout deleted buffer identity");b->label=label;});
	Bind(table,"CreateTexture",[L](uint32_t w,uint32_t h,const sol::table& opts){
		Check(globalRendering&&w&&h&&w<=uint32_t(globalRendering->maxTextureSize)&&h<=uint32_t(globalRendering->maxTextureSize),"Blockout target extent invalid");
		auto format=opts.get<unsigned>("format");Check(format==0x81a6||format==0x8229,"Blockout target format differs");
		Check(!opts.get_or("border",false)&&opts.get<unsigned>("min_filter")==GL_NEAREST&&opts.get<unsigned>("mag_filter")==GL_NEAREST&&opts.get<unsigned>("wrap_s")==GL_CLAMP_TO_EDGE&&opts.get<unsigned>("wrap_t")==GL_CLAMP_TO_EDGE,"Blockout original target sampler differs");
		NativePostprocessSampler sampler;sampler.minFilter=sampler.magFilter=NativePostprocessFilter::Nearest;sampler.wrapS=sampler.wrapT=NativePostprocessWrap::ClampEdge;
		return LuaNativeUI::RegisterPostprocessTexture(L,CreateNativePostprocessBinding(w,h,format==0x81a6?NativePostprocessFormat::Depth24Unorm:NativePostprocessFormat::R8Unorm,sampler,format==0x8229,globalRendering->maxTextureSize));
	});
	Bind(table,"DeleteTexture",[L](sol::object name){if(name.is<std::string>())return LuaNativeUI::DeleteTexture(L,name.as<std::string>());return false;});
	Bind(table,"CreateFBO",[L](const sol::table& opts){
		auto f=std::make_shared<FBO>();f->color=LuaNativeUI::GetPostprocessTextureBinding(L,opts.get<std::string>("color0"));f->depth=LuaNativeUI::GetPostprocessTextureBinding(L,opts.get<std::string>("depth"));
		Check(f->color&&f->depth&&f->color->image->format==NativePostprocessFormat::R8Unorm&&f->depth->image->format==NativePostprocessFormat::Depth24Unorm&&f->color->image->width==f->depth->image->width&&f->color->image->height==f->depth->image->height,"Blockout FBO attachments differ");
		auto draws=opts.get<sol::table>("drawbuffers");Check(draws.size()==1&&draws.get<unsigned>(1)==0x8ce0,"Blockout draw buffer differs");
		f->target=allocate(f->depth->image->width,f->depth->image->height);if(!f->target||!f->target->storage)return std::shared_ptr<FBO>{};return f;
	});
	Bind(table,"DeleteFBO",[](std::shared_ptr<FBO> f){if(f)f->deleted=true;});
	Bind(table,"IsValidFBO",[](std::shared_ptr<FBO> f){return f&&f->Live();});
	Bind(table,"Clear",[c](unsigned mask,float depth){
		Check(c->depthPass&&mask==GL_DEPTH_BUFFER_BIT&&depth==1,"Blockout own target clear differs");
		Check(!ui::GetRecorder().CurrentState().draw.scissor&&ui::GetRecorder().CurrentWorldState().depthWrite,"Blockout own target clear needs actual unscissored depth writer");
		ui::GetRecorder().SetOriginalClearDepth(depth);c->depthPass->clearDepth=depth;c->depthPass->clearObserved=true;
	});
	Bind(table,"ActiveFBO",[c](std::shared_ptr<FBO> f,sol::protected_function callback){
		Check(frame&&f&&f->Live()&&!c->depthPass,"Blockout FBO lifetime or nested target differs");
		NativeBlockoutDepthPass pass;pass.sequence=NextNativeWorldDrawSequence();pass.target=f->target;c->depthPass=&pass;
		try{auto result=callback();c->depthPass=nullptr;if(!result.valid()){sol::error error=result;throw std::runtime_error(error.what());}
			Check(f->Live(),"Blockout attachments deleted in callback");
			LuaNativePostprocess::GetRecorder().PublishExternalOutput(f->depth,NativePostprocessStage::PreUnit,pass.sequence,NativeExternalImageFamily::MapBlockoutDepth,{},
				[&](std::shared_ptr<const NativePostprocessVersion> output){pass.output=std::move(output);ValidateNativeBlockoutDepthPass(pass);frame->depthPasses.push_back(std::move(pass));});
		}catch(...){c->depthPass=nullptr;throw;}
	});
	return table;
}
}
void Configure(ModelProvider provider,TargetAllocator allocator){Check(!frame,"Blockout configuration changed in a frame");models=std::move(provider);allocate=std::move(allocator);}
void BeginWorldFrame(){Check(!frame,"Nested Blockout source frame");frame=std::make_shared<NativeBlockoutFrame>();frame->sourceDrawFrame=globalRendering->drawFrame;}
std::shared_ptr<const NativeBlockoutFrame> EndWorldFrame(){Check(bool(frame),"Blockout End without Begin");return std::exchange(frame,{});}
void AbortWorldFrame()noexcept{frame.reset();for(auto& w:contexts)if(auto c=w.lock())c->Abort();}
void Reset(){AbortWorldFrame();std::erase_if(contexts,[](auto& w){return w.expired();});}
bool PushEntries(lua_State* L){
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();
	family.new_usertype<Buffer>("Buffer",sol::no_constructor,"Define",&Buffer::Define,"Upload",&Buffer::Upload,"Download",&Buffer::Download,"Delete",&Buffer::Delete,"GetBufferSize",&Buffer::GetBufferSize,"GetID",&Buffer::GetID,"GetLabel",&Buffer::GetLabel,"ModelsVBO",&Buffer::ModelsVBO,"InstanceDataFromFeatureDefIDs",sol::overload(&Buffer::Definition,&Buffer::Definitions));
	family.new_usertype<VertexArray>("VAO",sol::no_constructor,"AttachVertexBuffer",&VertexArray::Vertex,"AttachIndexBuffer",&VertexArray::Index,"AttachInstanceBuffer",&VertexArray::Instance,"AddFeatureDefsToSubmission",sol::overload(&VertexArray::Add,&VertexArray::AddMany),"RemoveFromSubmission",&VertexArray::Remove,"ClearSubmission",&VertexArray::Clear,"Delete",&VertexArray::Delete,"Submit",&VertexArray::Submit);
	family.new_usertype<Program>("Program",sol::no_constructor,"Initialize",&Program::Initialize,"Activate",&Program::Activate,"Deactivate",&Program::Deactivate,"Finalize",&Program::Finalize,"SetUniformFloat",&Program::Uniform);
	family.new_usertype<FBO>("FBO",sol::no_constructor);
	family.set("Buffer",sol::lua_nil,"VAO",sol::lua_nil,"Program",sol::lua_nil,"FBO",sol::lua_nil);
	Bind(family,"Available",Available);Bind(family,"CreateContext",[L](){return Create(L);});Bind(family,"LoadArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Blockout source must be unsynced");return Source(path);});gl.set("NativeBlockout",family);return true;
}
}
#else
namespace LuaNativeBlockout{
void Configure(ModelProvider,TargetAllocator){}void BeginWorldFrame(){}
std::shared_ptr<const rendering::NativeBlockoutFrame> EndWorldFrame(){return {};}
void AbortWorldFrame()noexcept{}void Reset(){}bool PushEntries(lua_State*){return true;}
}
#endif
