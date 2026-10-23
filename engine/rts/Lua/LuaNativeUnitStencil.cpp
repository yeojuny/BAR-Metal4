/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeUnitStencil.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeSourceProgramBinding.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/GlobalRendering.h"
#include "System/FileSystem/FileHandler.h"
#include "System/Sync/SHA512.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <utility>
namespace LuaNativeUnitStencil {
namespace {
using namespace rendering;
Services services;
std::shared_ptr<NativeUnitStencilFrame> frame;
void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<typename S>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<class F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,A...a)->R{
	try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();
};}};
template<class T,class F>void Bind(T& t,const char* name,F f){t.set_function(name,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
std::string Hash(std::string_view bytes){sha512::raw_digest d;sha512::calc_digest(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size(),d.data());return sha512::dump_digest(d);}
struct Context;
struct Program {
	std::weak_ptr<Context> context;
	std::shared_ptr<const NativeUnitStencilProgramConsumer> consumer;
	std::map<std::string,std::vector<float>> floats;
	std::string label;
	bool deleted=false;
};
struct Buffer {
	std::weak_ptr<Context> context;
	std::shared_ptr<LuaVBOImpl> value;
	unsigned target=0;
	bool deleted=false;
	void Live()const{Check(!deleted&&bool(value),"Unit Stencil actual original VBO was deleted");}
	void Delete(){if(!deleted)value->Delete();deleted=true;}
};
struct Cell {
	std::shared_ptr<const NativeUnitStencilImage> image;
	std::shared_ptr<const NativeUnitStencilVersion> current;
	uint64_t name=0,nextGeneration=1;
	bool deleted=false;
};
struct Location {std::shared_ptr<Program> program;NativeCUSUniformDescription description;};
struct Context:std::enable_shared_from_this<Context> {
	lua_State* lua=nullptr;
	uint64_t generation=0;
	std::vector<std::shared_ptr<Program>> programs;
	std::vector<Location> locations;
	std::vector<std::weak_ptr<Buffer>> buffers;
	std::map<std::string,std::shared_ptr<Cell>,std::less<>> images;
	std::optional<NativeUnitStencilPass> target;
	~Context(){auto& c=OriginalNativeTextureContext();if(c.Initialized()&&c.Registry().ContextGeneration()==generation)for(auto&[_,cell]:images)if(!cell->deleted)c.Registry().Delete(cell->name);}
	NativeTextureContext& Actual()const{
		auto& c=OriginalNativeTextureContext();Check(c.Initialized()&&c.UIAttached()&&c.Registry().ContextGeneration()==generation,"Unit Stencil crossed actual texture context lifetime");return c;
	}
	std::shared_ptr<Program> Get(unsigned handle)const{
		Check(handle&&handle<=programs.size()&&!programs[handle-1]->deleted,"Unknown/deleted original Unit Stencil native program");return programs[handle-1];
	}
	std::shared_ptr<Program> Active()const{
		const auto selected=OriginalNativeSourceProgramBinding().Current();if(selected.kind!=NativeSourceProgramBinding::Kind::UnitStencil||!selected.owner)return{};
		auto p=std::const_pointer_cast<Program>(std::static_pointer_cast<const Program>(selected.owner));return !p->deleted&&p->context.lock().get()==this?p:nullptr;
	}
	unsigned LocationOf(unsigned handle,const std::string& name){
		auto p=Get(handle);for(unsigned i=0;i<locations.size();++i)if(locations[i].program==p&&locations[i].description.name==name)return i+1;
		for(const auto& u:p->consumer->uniforms)if(u.name==name){locations.push_back({p,u});return unsigned(locations.size());}return 0;
	}
	void Uniform(unsigned handle,unsigned location,const std::string& kind,sol::variadic_args values){
		auto p=Get(handle);Check(Active()==p&&OriginalNativeSourceProgramBinding().IsUnitStencil(p),"Unit Stencil uniform is not owned by current actual source program");
		Check(location&&location<=locations.size()&&locations[location-1].program==p&&kind=="float"&&!locations[location-1].description.integer&&values.size()==1,
			"Unit Stencil original reflected scalar location/type/arity differs");
		float value=values[0].as<float>();Check(std::isfinite(value),"Unit Stencil original uniform is nonfinite");p->floats[locations[location-1].description.name]={value};
	}
	void Clear(unsigned mask,std::array<float,4> color){
		Check(frame&&target&&mask==0x4000,"Unit Stencil clear requires its original R8 RTT scope");NativeUnitStencilCommand c;c.clear=true;c.sequence=NextNativeWorldDrawSequence();c.clearColor=color;
		c.surface=ui::GetRecorder().CurrentState().draw;c.world=ui::GetRecorder().CurrentWorldState();target->commands.push_back(std::move(c));
	}
	std::string CreateTexture(double width,double height,const sol::table& options){
		Check(std::isfinite(width)&&std::isfinite(height)&&width>=1&&height>=1&&globalRendering&&width<double(INT32_MAX)&&height<double(INT32_MAX)&&std::trunc(width)<=globalRendering->maxTextureSize&&std::trunc(height)<=globalRendering->maxTextureSize,
			"Unit Stencil original quarter R8 extent is invalid");
		for(const auto& e:options){const auto k=e.first.as<std::string>();Check(k=="format"||k=="fbo"||k=="min_filter"||k=="mag_filter"||k=="wrap_s"||k=="wrap_t","Unknown Unit Stencil original texture option");}
		Check(options.get<unsigned>("format")==0x8229&&options.get<bool>("fbo")&&options.get<unsigned>("min_filter")==0x2600&&options.get<unsigned>("mag_filter")==0x2600&&
			options.get<unsigned>("wrap_s")==0x812f&&options.get<unsigned>("wrap_t")==0x812f,"Unit Stencil original R8 nearest/clamp target differs");
		static std::atomic<uint64_t> serial{1};const auto identity=serial.fetch_add(1);Check(identity&&identity<UINT64_MAX,"Unit Stencil image identity exhausted");
		auto image=std::make_shared<NativeUnitStencilImage>();image->identity=identity;image->width=uint32_t(std::trunc(width));image->height=uint32_t(std::trunc(height));
		image->sampling.minFilter=image->sampling.magFilter=0x2600;image->sampling.wrapS=image->sampling.wrapT=0x812f;
		auto version=std::make_shared<NativeUnitStencilVersion>();version->image=image;version->producer->allocationOnly=true;
		auto cell=std::make_shared<Cell>();cell->image=image;cell->current=version;
		// Allocation is the first immutable content version, even before a draw.
		// Consume its generation so the first RTT publication has a distinct owner version.
		version->generation=cell->nextGeneration++;
		auto& registry=Actual().Registry();auto object=registry.Create(NativeCUSTextureTarget::Image2D,"OriginalUnitStencilR8");cell->name=object->name;
		try{
			NativeTextureAllocation allocation;allocation.width=image->width;allocation.height=image->height;allocation.depth=allocation.levels=allocation.samples=1;allocation.originalInternalFormat=0x8229;
			registry.Allocate(cell->name,allocation);registry.SetSampling(cell->name,image->sampling);
			auto backing=std::make_shared<NativeUnitStencilBacking>();backing->version=version;backing->storageGeneration=image->identity;backing->contentVersion=version->generation;backing->content=NativeTextureContent::AllocationOnly;
			registry.PublishContent(cell->name,{},backing);
			const auto name="!nativeUnitStencil"+std::to_string(identity);images.emplace(name,cell);return name;
		}catch(...){registry.Delete(cell->name);throw;}
	}
	void DeleteTexture(const std::string& name){auto i=images.find(name);if(i==images.end())return;Check(!target||target->output->image!=i->second->image,"Unit Stencil deletes its active original RTT target");Actual().Registry().Delete(i->second->name);i->second->deleted=true;images.erase(i);}
	void RenderToTexture(const std::string& name,sol::protected_function callback){
		Check(frame&&!target&&callback.valid()&&!ui::GetRecorder().IsWorldStage()&&!ui::GetRecorder().IsScreenStage()&&ui::GetRecorder().IsActive(),
			"Unit Stencil producer must run inside original DrawGenesis source frame");
		auto i=images.find(name);Check(i!=images.end()&&!i->second->deleted,"Unit Stencil original RTT name missing/deleted");auto cell=i->second;
		Check(cell->nextGeneration<UINT64_MAX,"Unit Stencil image generation exhausted");
		auto next=std::make_shared<NativeUnitStencilVersion>();next->image=cell->image;next->generation=cell->nextGeneration;next->sourceDrawFrame=frame->sourceDrawFrame;
		NativeUnitStencilPass pass;pass.sequence=NextNativeWorldDrawSequence();pass.output=next;pass.previous=cell->current;target=std::move(pass);
		try{ui::GetRecorder().RunFramebufferState(cell->image->width,cell->image->height,true,[&]{auto result=callback();if(!result.valid()){sol::error e=result;throw std::runtime_error(e.what());}});}
		catch(...){target.reset();throw;}
		pass=std::move(*target);target.reset();
		Check(!pass.commands.empty()&&pass.commands.front().clear,"Unit Stencil original demand callback must clear even an empty scene");
		auto& registry=Actual().Registry();auto priorObject=registry.Find(cell->name);auto previous=cell->current;
		auto expected=std::make_shared<std::shared_ptr<const NativeCUSTextureObject>>();std::weak_ptr<Context> weak=shared_from_this();
		std::function<void()> cancel=[weak,cell,next,previous,expected,priorObject]()noexcept{
			if(auto context=weak.lock()){auto& c=OriginalNativeTextureContext();if(c.Initialized()&&c.Registry().ContextGeneration()==context->generation)c.Registry().CancelContentPublication(*expected,priorObject);}
			if(cell->current==next)cell->current=previous;
		};
		frame->passes.push_back(std::move(pass));try{frame->rollback.push_back(cancel);}catch(...){frame->passes.pop_back();throw;}
		try{auto backing=std::make_shared<NativeUnitStencilBacking>();backing->version=next;backing->storageGeneration=cell->image->identity;backing->contentVersion=next->generation;
			backing->sourceDrawFrame=next->sourceDrawFrame;backing->content=NativeTextureContent::ProducerVersion;registry.PublishContent(cell->name,{},backing);*expected=registry.Find(cell->name);cell->current=next;++cell->nextGeneration;
		}catch(...){cancel();frame->rollback.pop_back();frame->passes.pop_back();throw;}
	}
};
std::vector<std::weak_ptr<Context>> contexts;
struct VertexArray {
	std::shared_ptr<Context> context;
	std::shared_ptr<Buffer> vertices,indices,instances;
	bool deleted=false;
	void Attach(std::shared_ptr<Buffer>& destination,std::shared_ptr<Buffer> value,unsigned target){Check(!deleted&&value&&!value->deleted&&value->context.lock()==context&&value->target==target,"Unit Stencil original VAO attachment target/owner differs");destination=std::move(value);}
	void Draw(unsigned mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instanceCount,sol::optional<int> firstInstance){
		Check(frame&&context->target&&!deleted&&vertices&&mode==0&&instanceCount.value_or(0)==0&&firstInstance.value_or(0)==0,"Unit Stencil original successful GS path must draw actual point records in Genesis RTT");
		vertices->Live();auto program=context->Active();Check(bool(program)&&OriginalNativeSourceProgramBinding().IsUnitStencil(program),"Unit Stencil draw has no genuine active source program");
		NativeUnitStencilDraw draw;draw.records=vertices->value->CaptureNativeSnapshot();draw.program=program->consumer;
		const int start=std::max(first.value_or(0),0);const auto capacity=draw.records->layout.elements;
		Check(capacity<=size_t(INT32_MAX)&&size_t(start)<=capacity,"Unit Stencil original point offset exceeds actual VBO");const int number=count.value_or(int(capacity)-start);
		Check(number>0&&size_t(number)<=capacity-size_t(start),"Unit Stencil original point count exceeds actual VBO");
		draw.first=uint32_t(start);draw.count=uint32_t(number);draw.addRadius=program->floats.at("addRadius").at(0);draw.stencilColor=program->floats.at("stencilColor").at(0);
		draw.engineUniforms=OriginalCUSUniformCapture().Current();draw.modelUniforms=OriginalCUSModelPublication().ModelUniforms();draw.surface=ui::GetRecorder().CurrentState().draw;draw.world=ui::GetRecorder().CurrentWorldState();
		draw.sequence=NextNativeWorldDrawSequence();ValidateNativeUnitStencilDraw(draw);
		NativeUnitStencilCommand command;command.sequence=draw.sequence;command.draw=std::move(draw);context->target->commands.push_back(std::move(command));
	}
	void Delete(){deleted=true;vertices.reset();indices.reset();instances.reset();}
};
std::map<std::string,std::vector<float>> FloatTable(sol::object source){
	std::map<std::string,std::vector<float>> result;if(!source.valid()||source.get_type()==sol::type::lua_nil)return result;
	for(const auto& item:source.as<sol::table>()){std::vector<float> values;if(item.second.is<sol::table>()){auto t=item.second.as<sol::table>();for(size_t i=1;i<=t.size();++i)values.push_back(t.get<float>(i));}else values.push_back(item.second.as<float>());result.emplace(item.first.as<std::string>(),std::move(values));}return result;
}
sol::table CreateContext(lua_State* L){
	Check(!CLuaHandle::GetHandleSynced(L)&&Available(),"Unit Stencil requires original unsynced native compiler/image services");
	sol::state_view lua(L);auto table=lua.create_table();auto context=std::make_shared<Context>();context->lua=L;context->generation=OriginalNativeTextureContext().Registry().ContextGeneration();
	std::erase_if(contexts,[](const auto& w){return w.expired();});contexts.emplace_back(context);
	Bind(table,"GetVBO",[L,context](sol::optional<unsigned> target,sol::optional<bool> frequent){
		context->Actual();auto b=std::make_shared<Buffer>();b->context=context;b->target=target.value_or(GL_ARRAY_BUFFER);
		Check(b->target==GL_ARRAY_BUFFER||b->target==GL_ELEMENT_ARRAY_BUFFER,"Unit Stencil requested another original VBO target");b->value=std::make_shared<LuaVBOImpl>(b->target,frequent.value_or(false));
		CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(b->value);context->buffers.emplace_back(b);return b;
	});
	Bind(table,"GetVAO",[context]{auto a=std::make_shared<VertexArray>();a->context=context;return a;});
	Bind(table,"CreateShader",[context](const sol::table& params){
		context->Actual();Check(Available(),"Unit Stencil genuine compiler unavailable");NativeUnitStencilProgramSource source;
		source.vertex=params.get<std::string>("vertex");source.geometry=params.get<std::string>("geometry");source.fragment=params.get<std::string>("fragment");
		for(const char* stage:{"tcs","tes","compute"})Check(!params[stage].valid(),"Unit Stencil selected original program has another stage");
		if(params["uniformInt"].valid())Check(params.get<sol::table>("uniformInt").size()==0,"Unit Stencil original program has no integer uniforms");
		source.initialFloat=FloatTable(params["uniformFloat"]);source.vertexSHA512=Hash(source.vertex);source.geometrySHA512=Hash(source.geometry);source.fragmentSHA512=Hash(source.fragment);
		ValidateNativeUnitStencilProgram(source);auto actual=services.compile(source);
		Check(actual&&actual->pipelineLifetime&&actual->vertexSHA512==source.vertexSHA512&&actual->geometrySHA512==source.geometrySHA512&&actual->fragmentSHA512==source.fragmentSHA512,
			"Unit Stencil exact original program has no genuinely compiled native GS-equivalent owner");
		auto p=std::make_shared<Program>();p->context=context;p->consumer=std::move(actual);p->floats=p->consumer->defaultFloat;
		for(auto&[name,value]:source.initialFloat)p->floats.insert_or_assign(name,std::move(value));context->programs.push_back(p);return unsigned(context->programs.size());
	});
	Bind(table,"UseShader",[context](unsigned handle){if(handle)OriginalNativeSourceProgramBinding().BindUnitStencil(context->Get(handle));else OriginalNativeSourceProgramBinding().Unbind();});
	Bind(table,"ActiveShader",[context](unsigned handle,sol::protected_function callback){
		auto& binding=OriginalNativeSourceProgramBinding();auto previous=binding.Current();binding.BindUnitStencil(context->Get(handle));
		try{auto result=callback();if(!result.valid()){sol::error e=result;throw std::runtime_error(e.what());}}catch(...){binding.Restore(std::move(previous));throw;}binding.Restore(std::move(previous));
	});
	Bind(table,"DeleteShader",[context](unsigned handle){auto p=context->Get(handle);if(OriginalNativeSourceProgramBinding().IsUnitStencil(p))OriginalNativeSourceProgramBinding().Unbind();p->deleted=true;p->consumer.reset();p->floats.clear();});
	Bind(table,"GetProgramLog",[context](unsigned handle){return context->Get(handle)->consumer->log;});
	Bind(table,"GetActiveUniforms",[context,L](unsigned handle){sol::state_view lua(L);auto result=lua.create_table();unsigned i=1;for(const auto& u:context->Get(handle)->consumer->uniforms){auto entry=lua.create_table();entry["name"]=u.name;entry["type"]=u.glType;entry["size"]=u.size;result[i++]=entry;}return result;});
	Bind(table,"GetUniformLocation",[context,L](unsigned handle,const std::string& name){const auto location=context->LocationOf(handle,name);return location?sol::make_object(L,location):sol::make_object(L,sol::lua_nil);});
	Bind(table,"UniformLocation",[context](unsigned handle,unsigned location,const std::string& kind,sol::variadic_args values){context->Uniform(handle,location,kind,values);});
	Bind(table,"CreateTexture",[context](double w,double h,const sol::table& options){return context->CreateTexture(w,h,options);});
	Bind(table,"DeleteTexture",[context](sol::object name){if(name.get_type()==sol::type::string)context->DeleteTexture(name.as<std::string>());});
	Bind(table,"RenderToTexture",[context](const std::string& name,sol::protected_function callback){context->RenderToTexture(name,std::move(callback));});
	Bind(table,"Clear",[context](unsigned mask,float r,float g,float b,float a){context->Clear(mask,{r,g,b,a});});
	sol::protected_function originalLabel=lua["gl"]["ObjectLabel"];
	Bind(table,"ObjectLabel",[context,originalLabel](unsigned type,unsigned identity,const std::string& label){
		if(type==0x82e2){context->Get(identity)->label=label;return;}Check(type==0x82e0,"Unit Stencil original object label namespace differs");bool owned=false;
		for(const auto& weak:context->buffers)if(auto b=weak.lock();b&&!b->deleted&&b->value->GetID()==identity){owned=true;break;}
		Check(owned&&originalLabel.valid(),"Unit Stencil buffer annotation has no actual original GL owner");auto result=originalLabel(type,identity,label);if(!result.valid()){sol::error e=result;throw std::runtime_error(e.what());}
	});
	return table;
}
const std::map<std::string,std::string,std::less<>>& Identities(){
	static const std::map<std::string,std::string,std::less<>> values{
		{"luaui/widgets/gfx_unit_stencil_gl4.lua","191af0888221e3b5806e7efd99ccf1ce76dd07dd2704856bfc7fbfa6895bbe247f2aa8df252d3baac6cee481585ce8cb8122b155610814658cb1b32d87d8055a"},
		{"modules/graphics/luashader.lua","81858ab150aceb7d84f324708f3b3c4a5c8101df03c2a3a0597cc64413fea8ff7db558b711b18e930aa577bf7b0c6b96bfde83c342ca94508f72021742372436"},
		{"modules/graphics/instancevbotable.lua","bf128f904541628d4f89c1699c94461f604dcbbc9fa48164bc4ac6febdeba29c4c92786e9d1d0c054e3fd8fc55104766e0b0d0e70f2a6fbd3bd3149b0ea39256"},
	};return values;
}
}
void Configure(Services value){Check(!frame,"Unit Stencil cannot reconfigure native compiler during Genesis capture");services=std::move(value);}
bool Available(){auto& c=OriginalNativeTextureContext();return services.ready&&services.compile&&services.ready()&&c.Initialized()&&c.UIAttached()&&c.Registry().ContextGeneration()&&ui::GetRecorder().TextureObserver()==&c.UI();}
void BeginGenesisFrame(){Check(!frame&&globalRendering,"Unit Stencil Genesis frame nested or has no original draw owner");frame=std::make_shared<NativeUnitStencilFrame>();frame->sourceDrawFrame=uint64_t(globalRendering->drawFrame);}
std::shared_ptr<const NativeUnitStencilFrame> EndGenesisFrame(){Check(bool(frame),"Unit Stencil Genesis frame is not active");for(const auto& weak:contexts)if(auto c=weak.lock())Check(!c->target,"Unit Stencil Genesis ended during a target callback");return std::exchange(frame,{});}
void AbortGenesisFrame()noexcept{for(const auto& weak:contexts)if(auto c=weak.lock())c->target.reset();frame.reset();}
void Reset(){
	AbortGenesisFrame();auto& actual=OriginalNativeTextureContext();auto& binding=OriginalNativeSourceProgramBinding();
	for(const auto& weak:contexts)if(auto context=weak.lock()){
		for(auto&[_,cell]:context->images){if(!cell->deleted&&actual.Initialized()&&actual.Registry().ContextGeneration()==context->generation)actual.Registry().Delete(cell->name);cell->deleted=true;}
		context->images.clear();
		for(auto& p:context->programs){if(binding.IsUnitStencil(p))binding.Unbind();p->deleted=true;p->consumer.reset();p->floats.clear();}
	}
	contexts.clear();
}
bool Lookup(lua_State* L,const std::string& name,NativeTextureLookup& result){
	if(!L||CLuaHandle::GetHandleSynced(L))return false;auto& actual=OriginalNativeTextureContext();if(!actual.Initialized())return false;
	for(const auto& weak:contexts)if(auto c=weak.lock();c&&c->lua==L&&c->generation==actual.Registry().ContextGeneration())if(auto i=c->images.find(name);i!=c->images.end()&&!i->second->deleted){result={true,actual.Registry().Find(i->second->name),false};Check(bool(result.object),"Unit Stencil actual logical name lost its typed R8 owner");return true;}return false;
}
bool PushEntries(lua_State* L){
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto f=lua.create_table();auto b=f.new_usertype<Buffer>("Buffer",sol::no_constructor);
	Bind(b,"Define",[](Buffer& b,int count,sol::optional<sol::object> definition){b.Live();b.value->Define(count,definition);});
	Bind(b,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){b.Live();return b.value->Upload(data,attribute,offset,first,last);});
	Bind(b,"Download",[](Buffer& b,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> count,sol::optional<bool> forceGPU){b.Live();return b.value->Download(attribute,offset,count,forceGPU);});
	Bind(b,"InstanceDataFromUnitIDs",[](Buffer& b,sol::stack_object ids,int attribute,sol::optional<int> offset){b.Live();Check(attribute==2,"Unit Stencil original unit instData attribute differs");if(ids.is<int>())return b.value->InstanceDataFromUnitIDs(ids.as<int>(),attribute,offset);return b.value->InstanceDataFromUnitIDs(ids.as<sol::stack_table>(),attribute,offset);});
	Bind(b,"InstanceDataFromFeatureIDs",[](Buffer& b,sol::stack_object ids,int attribute,sol::optional<int> offset){b.Live();Check(attribute==2,"Unit Stencil original feature instData attribute differs");if(ids.is<int>())return b.value->InstanceDataFromFeatureIDs(ids.as<int>(),attribute,offset);return b.value->InstanceDataFromFeatureIDs(ids.as<sol::stack_table>(),attribute,offset);});
	Bind(b,"Delete",[](Buffer& b){b.Delete();});Bind(b,"GetID",[](Buffer& b){b.Live();return b.value->GetID();});Bind(b,"GetBufferSize",[](Buffer& b){b.Live();return b.value->GetBufferSize();});
	auto a=f.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
	Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){a.Attach(a.vertices,std::move(b),GL_ARRAY_BUFFER);});
	Bind(a,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){a.Attach(a.indices,std::move(b),GL_ELEMENT_ARRAY_BUFFER);});
	Bind(a,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){a.Attach(a.instances,std::move(b),GL_ARRAY_BUFFER);});
	Bind(a,"DrawArrays",[](VertexArray& a,unsigned mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> firstInstance){a.Draw(mode,count,first,instances,firstInstance);});
	Bind(a,"Delete",[](VertexArray& a){a.Delete();});f.set("Buffer",sol::lua_nil,"VertexArray",sol::lua_nil);
	Bind(f,"Available",&Available);Bind(f,"CreateContext",[L]{return CreateContext(L);});
	Bind(f,"LoadArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Unit Stencil original archive source is unsynced only");const auto id=Identities().find(path);Check(id!=Identities().end(),"Unknown Unit Stencil original archive source");CFileHandler file(path,SPRING_VFS_ZIP);std::string bytes;Check(file.FileExists()&&file.LoadStringData(bytes)&&Hash(bytes)==id->second,"Unit Stencil original source missing or identity changed");return bytes;});
	Bind(f,"PrepareSource",[](std::string bytes){const auto hash=Hash(bytes);Check(std::any_of(Identities().begin(),Identities().end(),[&](const auto& e){return e.second==hash;}),"Unit Stencil private Lua source identity differs");return bytes;});gl.set("NativeUnitStencil",f);return true;
}
}
#else
namespace LuaNativeUnitStencil {
void Configure(Services){}bool Available(){return false;}void BeginGenesisFrame(){}std::shared_ptr<const rendering::NativeUnitStencilFrame> EndGenesisFrame(){return{};}void AbortGenesisFrame()noexcept{}void Reset(){}bool PushEntries(lua_State*){return false;}bool Lookup(lua_State*,const std::string&,rendering::NativeTextureLookup&){return false;}
}
#endif
