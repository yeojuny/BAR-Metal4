/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeDistortion.h"
#include "Rendering/Backend/NativeUnitUniformCopy.h"

#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaVBOImpl.h"
#include "Lua/LuaNativeCUSTextures.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeDistortionPrograms.h"
#include "Rendering/Backend/NativeSourceProgramBinding.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/Backend/NativeWorldDrawRecorder.h"
#include "Rendering/GlobalRendering.h"
#include "System/FileSystem/FileHandler.h"
#include "System/Sync/SHA512.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace LuaNativeDistortion {
namespace {
using namespace rendering;
Services services;
uint64_t nextImageIdentity=1;
struct ImageCell {
 std::shared_ptr<const NativeDistortionImage> image;
 std::shared_ptr<const NativeDistortionVersion> current;
 uint64_t nextGeneration=1;
 bool deleted=false;
};
void Check(bool value, const char* message)
{
	if (!value) throw std::runtime_error(message);
}
template<typename Signature> struct Boundary;
template<typename R, typename... Args> struct Boundary<R(Args...)> {
	template<typename F> static auto Wrap(F function)
	{
		return [function = std::move(function)](sol::this_state state, Args... args) -> R {
			try { return function(std::forward<Args>(args)...); }
			catch (const std::exception& error) { lua_pushstring(state, error.what()); }
			lua_error(state);
			std::terminate();
		};
	}
};
template<typename T, typename F> void Bind(T& table, const char* name, F function)
{
	table.set_function(name, Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(function)));
}
std::string Hash(std::string_view text)
{
	sha512::raw_digest digest;
	sha512::calc_digest(reinterpret_cast<const uint8_t*>(text.data()), text.size(), digest.data());
	return sha512::dump_digest(digest);
}
uint64_t TemplateHash(std::string_view text)
{
	uint64_t hash = 14695981039346656037ULL;
	for (unsigned char character: text) hash = (hash ^ character) * 1099511628211ULL;
	return hash;
}
std::string Lower(std::string text)
{
	for (char& character: text) character = char(std::tolower(static_cast<unsigned char>(character)));
	return text;
}
std::map<std::string, std::vector<float>> UniformTable(const sol::table& table, bool integer)
{
	std::map<std::string, std::vector<float>> result;
	for (const auto& entry: table) {
		std::vector<float> values;
		if (entry.second.is<sol::table>()) {
			const auto data = entry.second.as<sol::table>();
			for (size_t i = 1; i <= data.size(); ++i) values.push_back(data.get<float>(i));
		} else values.push_back(entry.second.as<float>());
		for (float value: values)
			Check(std::isfinite(value) && (!integer || value == std::trunc(value)), "Invalid original distortion initial uniform scalar");
		result.emplace(entry.first.as<std::string>(), std::move(values));
	}
	return result;
}
struct Context;
std::vector<std::weak_ptr<Context>> contexts;
struct Program {
	std::weak_ptr<Context> context;
	std::shared_ptr<const NativeDistortionProgramConsumer> consumer;
	std::map<std::string, std::vector<float>> floats, ints;
	std::string label;
	bool deleted = false;
};
struct Buffer {
	std::weak_ptr<Context> context;
	std::shared_ptr<LuaVBOImpl> value;
	unsigned target = 0;
	bool deleted = false;
	void Live() const { Check(!deleted && bool(value), "Original distortion VBO was deleted"); }
	void Delete() { if (!deleted) value->Delete(); deleted = true; }
};
struct Location { std::shared_ptr<Program> program; NativeCUSUniformDescription description; };
struct Context {
 const void* luaHandle=nullptr;
	uint64_t textureContext = 0;
 std::map<std::string,std::shared_ptr<ImageCell>,std::less<>> images;
 std::array<std::shared_ptr<ImageCell>,2> localBindings;
 std::shared_ptr<ImageCell> target;
 std::shared_ptr<ImageCell> Image(const std::string& name) const {
  const auto it=images.find(name);Check(it!=images.end()&&!it->second->deleted,"Unknown or deleted original Distortion private image");return it->second;
 }
 NativeDistortionCommand Command(NativeDistortionOperation kind) const {
  auto& recorder=ui::GetRecorder();
  Check(globalRendering&&recorder.CurrentWorldStage()==NativeWorldDrawStage::World&&!recorder.IsCompiling()&&!recorder.HasActiveTarget(),"Distortion requires its original main DrawWorld stage");
  NativeDistortionCommand command;command.sequence=NextNativeWorldDrawSequence();command.sourceDrawFrame=uint64_t(globalRendering->drawFrame);command.kind=kind;
  command.surface=recorder.CurrentState().draw;command.world=recorder.CurrentWorldState();for(const auto& program:programs)if(!program->deleted&&program->consumer->kind==NativeDistortionProgram::Volume){command.transferProgram=program->consumer;break;}return command;
 }
 std::shared_ptr<const NativeDistortionVersion> WriteVersion(const std::shared_ptr<ImageCell>& cell,const NativeDistortionCommand& command) const {
  Check(cell&&!cell->deleted,"Original Distortion write target was deleted");
  auto version=std::make_shared<NativeDistortionVersion>();version->image=cell->image;version->generation=cell->nextGeneration;version->sourceDrawFrame=command.sourceDrawFrame;version->producerSequence=command.sequence;return version;
 }
 void Commit(NativeDistortionCommand command,const std::shared_ptr<ImageCell>& output={}) {
  const auto version=command.output;ValidateNativeDistortionCommand(command);
  GetWorldDrawRecorder().RecordDistortion(std::make_shared<const NativeDistortionCommand>(std::move(command)));
  if(output){output->current=version;++output->nextGeneration;}
 }

	std::vector<std::shared_ptr<Program>> programs;
	std::vector<Location> locations;
	std::vector<std::weak_ptr<Buffer>> buffers;
 void Release() {
  const auto active=Active();if(active)OriginalNativeSourceProgramBinding().Unbind();
  for(const auto& weak:buffers)if(auto buffer=weak.lock())buffer->Delete();
  for(auto& program:programs)program->deleted=true;
  programs.clear();locations.clear();buffers.clear();localBindings={};target.reset();
  for(auto& [name,image]:images){image->deleted=true;image->current.reset();}images.clear();
 }
	NativeTextureContext& Actual() const
	{
		auto& actual = OriginalNativeTextureContext();
		Check(actual.Initialized() && actual.UIAttached() && actual.Registry().ContextGeneration() == textureContext,
			"Distortion context crossed its actual native texture lifetime");
		return actual;
	}
	std::shared_ptr<Program> Get(unsigned handle) const
	{
		Check(handle && handle <= programs.size() && !programs[handle - 1]->deleted,
			"Unknown or deleted original distortion program handle");
		return programs[handle - 1];
	}
	std::shared_ptr<Program> Active() const
	{
		const auto selected = OriginalNativeSourceProgramBinding().Current();
		if (selected.kind != NativeSourceProgramBinding::Kind::Distortion || !selected.owner) return {};
		auto program = std::const_pointer_cast<Program>(std::static_pointer_cast<const Program>(selected.owner));
		if (program->context.lock().get() != this || program->deleted) return {};
		return program;
	}
	unsigned GetLocation(unsigned handle, const std::string& name)
	{
		auto program = Get(handle);
		for (unsigned i = 0; i < locations.size(); ++i)
			if (locations[i].program == program && locations[i].description.name == name) return i + 1;
		for (const auto& uniform: program->consumer->uniforms) {
			if (uniform.name == name) {
				locations.push_back({program, uniform});
				return unsigned(locations.size());
			}
		}
		return 0; // Genuine source compiler reflection reported this uniform inactive.
	}
	void Uniform(unsigned handle, const std::string& name, const std::string& kind, sol::variadic_args input)
	{
		auto program = Get(handle);
		Check(Active() == program && OriginalNativeSourceProgramBinding().IsDistortion(program),
			"Distortion uniform does not own the actual current source program");
		Check(kind == "int" || kind == "float", "Selected original distortion program has no matrix uniform setters");
		const bool integer = kind == "int";
		const auto location = GetLocation(handle, name);
		if (!location) return; // Original inactive location writes have no effect.
		Check(locations[location - 1].description.integer == integer, "Distortion uniform scalar type differs from compiled reflection");
		std::vector<float> values;
		if (input.size() == 1 && input[0].is<sol::table>()) {
			const auto table = input[0].as<sol::table>();
			for (size_t i = 1; i <= table.size(); ++i) values.push_back(table.get<float>(i));
		} else for (auto item: input) values.push_back(item.as<float>());
		Check(!values.empty() && values.size() <= 4, "Distortion scalar/vector uniform arity differs");
		for (float value: values)
			Check(std::isfinite(value) && (!integer || value == std::trunc(value)), "Invalid original distortion uniform scalar");
		(integer ? program->ints : program->floats).insert_or_assign(name, std::move(values));
	}
};
struct VertexArray {
	std::shared_ptr<Context> context;
	std::shared_ptr<Buffer> vertices, indices, instances;
	bool deleted = false;
	void Attach(std::shared_ptr<Buffer>& destination, std::shared_ptr<Buffer> buffer, unsigned target)
	{
		Check(!deleted && buffer && !buffer->deleted && buffer->target == target && buffer->context.lock() == context,
			"Distortion VAO attachment has a different original target or context");
		destination = std::move(buffer);
	}
	void Delete() { deleted = true; vertices.reset(); indices.reset(); instances.reset(); }
	void Draw(bool indexed, unsigned mode, sol::optional<int> count, sol::optional<int> first,
		sol::optional<int> instanceCount, sol::optional<int> baseVertex, sol::optional<int> firstInstance)
	{
  Check(Available()&&!deleted&&vertices,"Distortion draw lacks actual VAO vertices");vertices->Live();
  auto program=context->Active();Check(program&&OriginalNativeSourceProgramBinding().IsDistortion(program),"Distortion draw has no actual active compiled source");
  const bool combine=program->consumer->kind==NativeDistortionProgram::Combine;
  Check(combine?!context->target:bool(context->target),"Distortion program was used on a different original target");
  if(!combine){Check(instances&&(!indexed||indices),"Distortion volume lost original instance/index backing");instances->Live();if(indexed)indices->Live();}
  auto command=context->Command(combine?NativeDistortionOperation::Combine:NativeDistortionOperation::Volume);
  auto draw=std::make_shared<NativeDistortionDraw>();draw->sequence=command.sequence;draw->program=program->consumer;
  draw->vertices=vertices->value->CaptureNativeSnapshot();if(!combine){draw->instances=instances->value->CaptureNativeSnapshot();if(indexed)draw->indices=indices->value->CaptureNativeSnapshot();}
  const size_t capacity=indexed?draw->indices->layout.elements:draw->vertices->layout.elements;
  Check(capacity<=size_t(std::numeric_limits<int>::max()),"Distortion original GLsizei capacity overflows");
  const int firstElement=first.value_or(0);Check(firstElement>=0,"Negative original Distortion first element");
  int drawCount=count.value_or(int(capacity));if(!count||(indexed&&*count<=0))drawCount-=firstElement;
  Check(drawCount>=0&&size_t(firstElement)<=capacity&&size_t(drawCount)<=capacity-size_t(firstElement),"Distortion draw exceeds actual source buffer");
  if(drawCount==0||(!combine&&instanceCount.value_or(0)==0))return;
  Check(instanceCount.value_or(0)>=0&&baseVertex.value_or(0)>=0&&firstInstance.value_or(0)>=0,"Negative Distortion instance/base interval");
  draw->indexed=indexed;draw->mode=mode;draw->count=uint32_t(drawCount);draw->firstIndex=indexed?uint32_t(firstElement):0;draw->firstVertex=indexed?0:uint32_t(firstElement);
  draw->baseVertex=baseVertex.value_or(0);draw->instanceCount=combine?1:uint32_t(instanceCount.value_or(0));draw->firstInstance=uint32_t(firstInstance.value_or(0));
  if(indexed){const auto scalar=draw->indices->layout.attributes.front().scalarType;draw->indexType=scalar==NativeScalarType::UInt8?0x1401:scalar==NativeScalarType::UInt16?0x1403:0x1405;}
  draw->floatUniforms=program->floats;draw->intUniforms=program->ints;
  auto& actual=context->Actual();
  for(unsigned i=0;i<3;++i)draw->textures[i]=actual.History().Binding(i,i==2?NativeCUSTextureTarget::Image3D:NativeCUSTextureTarget::Image2D);
  draw->engineUniforms=OriginalCUSUniformCapture().Current();draw->transforms=OriginalCUSModelPublication().Transforms();draw->modelUniforms=OriginalCUSModelPublication().ModelUniforms();
  if(!combine&&draw->program->uniformBufferCopy)draw->uniformBufferCopy=CaptureNativeUnitCopyBinding(context->luaHandle);
  draw->surface=command.surface;draw->world=command.world;
  command.draw=std::move(draw);
  if(combine){Check(!indexed&&context->localBindings[0]&&context->localBindings[1],"Distortion combine lost local source bindings");for(unsigned i=0;i<2;++i)command.inputs[i]=context->localBindings[i]->current;context->Commit(std::move(command));}
  else {command.previous=context->target->current;command.output=context->WriteVersion(context->target,command);context->Commit(std::move(command),context->target);}

	}
};
sol::table CreateContext(lua_State* state)
{
	Check(!CLuaHandle::GetHandleSynced(state) && Available(), "Distortions require genuine unsynced original source services");
	sol::state_view lua(state);
	auto table = lua.create_table();
	auto context = std::make_shared<Context>();
 context->luaHandle=CLuaHandle::GetHandle(state);
	context->textureContext = OriginalNativeTextureContext().Registry().ContextGeneration();
 std::erase_if(contexts,[](const auto& value){return value.expired();});contexts.emplace_back(context);
 Bind(table,"Release",[context](){context->Release();});
	Bind(table, "GetVBO", [state, context](sol::optional<unsigned> target, sol::optional<bool> frequent) {
		context->Actual();
		auto buffer = std::make_shared<Buffer>();
		buffer->target = target.value_or(GL_ARRAY_BUFFER);
		Check(buffer->target == GL_ARRAY_BUFFER || buffer->target == GL_ELEMENT_ARRAY_BUFFER,
			"Selected distortion source requested an unsupported original VBO target");
		buffer->context = context;
		buffer->value = std::make_shared<LuaVBOImpl>(buffer->target, frequent.value_or(false));
		CLuaHandle::GetActiveVBOs(state).luaVBOs.emplace_back(buffer->value);
		context->buffers.emplace_back(buffer);
		return buffer;
	});
	Bind(table, "GetVAO", [context]() {
		context->Actual();
		auto array = std::make_shared<VertexArray>(); array->context = context; return array;
	});
	Bind(table, "CreateProgram", [context](const std::string& name, const std::string& vertex, const std::string& fragment,
		const sol::table& defines, const sol::table& floats, const sol::table& integers, sol::optional<sol::table> metadata) {
		context->Actual();
		Check(Available() && metadata, "Distortion program has no actual native compiler or original helper assembly");
		Check((TemplateHash(vertex)==distortionProgram::VolumeVertexHash&&TemplateHash(fragment)==distortionProgram::VolumeFragmentHash)||(TemplateHash(vertex)==distortionProgram::CombineVertexHash&&TemplateHash(fragment)==distortionProgram::CombineFragmentHash),
			"Original selected distortion GLSL source identity differs");
		NativeDistortionProgramSource source;
		source.name = name; source.vertex = vertex; source.fragment = fragment;
		for (const auto& entry: defines) {
			const auto key = entry.first.as<std::string>();
			if (entry.second.get_type() == sol::type::string) source.defines.emplace(key, entry.second.as<std::string>());
			else source.defines.emplace(key, entry.second.as<float>());
		}
		source.initialFloat = UniformTable(floats, false); source.initialInt = UniformTable(integers, true);
		source.assembledVertex = metadata->get<std::string>("assembledVS");
		source.assembledFragment = metadata->get<std::string>("assembledFS");
		source.engineDefinitions = metadata->get<std::string>("engineDefs");
		source.quaternionDefinitions = metadata->get<std::string>("quaternionDefs");
		source.definitions = metadata->get<std::string>("defines");
		const auto paths = metadata->get<sol::table>("paths");
		source.vertexPath = paths.get<std::string>("vertex"); source.fragmentPath = paths.get<std::string>("fragment");
  const bool volume=name=="Deferred Distortions GL4";
  Check(Lower(source.vertexPath)==(volume?"luaui/shaders/distortion_gl4.vert.glsl":"luaui/shaders/screen_distortion_combine_gl4.vert.glsl")&&Lower(source.fragmentPath)==(volume?"luaui/shaders/distortion_gl4.frag.glsl":"luaui/shaders/screen_distortion_combine_gl4.frag.glsl"),"Original Distortion source-cache paths differ");
		source.vertexSHA512 = Hash(vertex); source.fragmentSHA512 = Hash(fragment);
		ValidateNativeDistortionProgramSource(source);
		auto consumer = services.compile(source);
		Check(consumer && consumer->pipelineLifetime && consumer->vertexSHA512 == source.vertexSHA512 &&
			consumer->fragmentSHA512 == source.fragmentSHA512, "Distortion original source has no matching compiled native program");
		auto program = std::make_shared<Program>(); program->context = context; program->consumer = std::move(consumer);
		program->floats = program->consumer->defaultFloat; program->ints = program->consumer->defaultInt;
		for (auto& [key, values]: source.initialFloat) program->floats.insert_or_assign(key, std::move(values));
		for (auto& [key, values]: source.initialInt) program->ints.insert_or_assign(key, std::move(values));
		context->programs.push_back(program);
		return unsigned(context->programs.size());
	});
	Bind(table, "Activate", [context](unsigned handle) { context->Actual(); OriginalNativeSourceProgramBinding().BindDistortion(context->Get(handle)); });
	Bind(table, "Deactivate", [context](unsigned handle) {
		auto program = context->Get(handle);
		Check(OriginalNativeSourceProgramBinding().IsDistortion(program), "Distortion Deactivate does not own the actual current source program");
		OriginalNativeSourceProgramBinding().Unbind();
	});
	Bind(table, "Finalize", [context](unsigned handle) {
		auto program = context->Get(handle);
		if (OriginalNativeSourceProgramBinding().IsDistortion(program)) OriginalNativeSourceProgramBinding().Unbind();
		program->deleted = true;
	});
	Bind(table, "GetProgramLog", [context](unsigned handle) { return context->Get(handle)->consumer->log; });
	Bind(table, "GetActiveUniforms", [state, context](unsigned handle) {
		sol::state_view lua(state); auto result = lua.create_table(); unsigned index = 1;
		for (const auto& uniform: context->Get(handle)->consumer->uniforms) {
			auto entry = lua.create_table(); entry["name"] = uniform.name; entry["type"] = uniform.glType; entry["size"] = uniform.size;
			result[index++] = entry;
		}
		return result;
	});
	Bind(table, "GetUniformLocation", [state, context](unsigned handle, const std::string& name) {
		const auto location = context->GetLocation(handle, name);
		return location ? sol::make_object(state, location) : sol::make_object(state, sol::lua_nil);
	});
	Bind(table, "Uniform", [context](unsigned handle, const std::string& name, const std::string& kind, sol::variadic_args input) {
		context->Uniform(handle, name, kind, input);
	});
	Bind(table, "UniformLocation", [context](unsigned handle, unsigned location, const std::string& kind, sol::variadic_args input) {
		Check(location && location <= context->locations.size() && context->locations[location - 1].program == context->Get(handle),
			"Distortion private reflected location belongs to another compiled source program");
		context->Uniform(handle, context->locations[location - 1].description.name, kind, input);
	});
	Bind(table, "Texture", [state, context](sol::object first, sol::optional<sol::object> second) {
		unsigned unit = 0; auto value = first;
		if (second) {
			const auto number = first.as<double>();
			Check(std::isfinite(number) && number == std::trunc(number) && number >= 0 && number < NativeCUSTextureHistory::Units,
				"Original distortion texture unit is out of range");
			unit = unsigned(number); value = *second;
		}
		auto& actual = context->Actual(); auto& recorder = ui::GetRecorder();
		if (value.is<bool>()) {
			if(unit<2&&context->localBindings[unit]){if(!value.as<bool>())context->localBindings[unit].reset();const auto observed=actual.UI().LuaEnable(unit,value.as<bool>());recorder.SyncObservedTextureBinding(observed);return true;}
   const unsigned observed = actual.UI().LuaEnable(unit, value.as<bool>());
			recorder.SyncObservedTextureBinding(observed); return true;
		}
		const auto name = value.as<std::string>(); const auto key = Lower(name);
  if(const auto local=context->images.find(name);local!=context->images.end()){
   Check(unit<2&&!local->second->deleted,"Original Distortion local sampler unit differs");context->localBindings[unit]=local->second;return true;
  }
  if(unit<2)context->localBindings[unit].reset();
		NativeTextureLookup lookup;
		const std::map<std::string, std::pair<NativeGeometryTextureRole, unsigned>, std::less<>> geometry{
			{"$map_gbuffer_normtex", {NativeGeometryTextureRole::Map, 0}}, {"$model_gbuffer_normtex", {NativeGeometryTextureRole::Models, 0}},
			{"$map_gbuffer_difftex", {NativeGeometryTextureRole::Map, 1}}, {"$model_gbuffer_difftex", {NativeGeometryTextureRole::Models, 1}},
			{"$map_gbuffer_zvaltex", {NativeGeometryTextureRole::Map, 5}}, {"$model_gbuffer_zvaltex", {NativeGeometryTextureRole::Models, 5}}
		};
		const unsigned observed = actual.UI().LuaLookup(unit, [&]() {
			if (const auto selected = geometry.find(key); selected != geometry.end()) {
				const auto logicalName = actual.Geometry(selected->second.first).Name(selected->second.second);
				lookup = {true, logicalName ? actual.Registry().Find(logicalName) : nullptr, false};
			} else lookup = LuaNativeCUSTextures::ParseTexture(state, name);
			return lookup;
		});
		recorder.SyncObservedTextureBinding(observed);
		return lookup.parsed;
	});
 Bind(table,"CreateTexture",[context](uint32_t width,uint32_t height,const sol::table& options){
  context->Actual();Check(width&&height&&width<=32768&&height<=32768,"Invalid original Distortion texture dimensions");
  auto image=std::make_shared<NativeDistortionImage>();image->identity=nextImageIdentity++;image->width=width;image->height=height;
  const auto format=options.get_or<unsigned>("format",0x1908);Check(format==0x1908||format==0x881a,"Unselected Distortion texture format");image->format=format==0x881a?NativeDistortionFormat::RGBA16Float:NativeDistortionFormat::RGBA8;
  Check(options.get_or<unsigned>("wrap_s",GL_CLAMP)==GL_CLAMP&&options.get_or<unsigned>("wrap_t",GL_CLAMP)==GL_CLAMP,"Unselected original Distortion wrap mode");image->sampling.wrapS=image->sampling.wrapT=GL_CLAMP;
  const auto linear=options.get_or<unsigned>("min_filter",GL_LINEAR);Check(linear==GL_LINEAR&&options.get_or<unsigned>("mag_filter",GL_LINEAR)==GL_LINEAR,"Distortion selected original sampler differs");image->sampling.minFilter=image->sampling.magFilter=GL_LINEAR;
  auto cell=std::make_shared<ImageCell>();cell->image=image;const auto name="__native_distortion_"+std::to_string(image->identity);context->images.emplace(name,cell);return name;
 });
 Bind(table,"DeleteTexture",[context](const std::string& name){auto cell=context->Image(name);Check(context->target!=cell,"Deleting active original Distortion target");cell->deleted=true;cell->current.reset();for(auto& binding:context->localBindings)if(binding==cell)binding.reset();context->images.erase(name);});
 Bind(table,"RenderToTexture",[context](const std::string& name,const sol::protected_function& callback){
  Check(!context->target,"Unexpected nested original Distortion target");auto image=context->Image(name);Check(image->image->format==NativeDistortionFormat::RGBA16Float,"Original Distortion RTT format differs");context->target=image;
  auto result=callback();context->target.reset();if(!result.valid()){sol::error error=result;throw std::runtime_error(error.what());}
 });
 Bind(table,"Clear",[context](unsigned mask,float red,float green,float blue,float alpha){
  Check(mask==GL_COLOR_BUFFER_BIT&&context->target,"Original Distortion clear is not its color RTT operation");auto command=context->Command(NativeDistortionOperation::Clear);command.output=context->WriteVersion(context->target,command);command.clearColor={red,green,blue,alpha};context->Commit(std::move(command),context->target);
 });
 Bind(table,"CopyToTexture",[context](const std::string& name,int destinationX,int destinationY,int sourceX,int sourceY,int width,int height){
  Check(!context->target,"Original Distortion screen copy occurred inside RTT");auto image=context->Image(name);Check(image->image->format==NativeDistortionFormat::RGBA8,"Original Distortion screen-copy format differs");auto command=context->Command(NativeDistortionOperation::CopyScreen);command.output=context->WriteVersion(image,command);command.copySource={sourceX,sourceY,width,height};command.copyDestination={destinationX,destinationY,width,height};context->Commit(std::move(command),image);
 });
	sol::protected_function originalLabel = lua["gl"]["ObjectLabel"];
	Bind(table, "ObjectLabel", [context, originalLabel](unsigned kind, unsigned identity, const std::string& label) {
		if (kind == 0x82e2) { context->Get(identity)->label = label; return; }
		Check(kind == 0x82e0, "Selected distortion annotation requested an unsupported object namespace");
		bool owned = false;
		for (const auto& weak: context->buffers)
			if (auto buffer = weak.lock(); buffer && !buffer->deleted && buffer->value->GetID() == identity) { owned = true; break; }
		Check(owned && originalLabel.valid(), "Distortion buffer annotation lacks an actual original GL buffer owner");
		auto result = originalLabel(kind, identity, label);
		if (!result.valid()) { sol::error error = result; throw std::runtime_error(error.what()); }
	});
	return table;
}
const std::map<std::string, std::string, std::less<>>& SourceIdentities()
{
 static const std::map<std::string,std::string,std::less<>> identities{
  {"luaui/widgets/gfx_distortion_gl4.lua","263fe13116363c8d0285db0e296d095501d434069253ecd0f25d00a0df311327e7b05de3c2f8a3563b47bc414e5d6e993078488a87d149152b5d5bd9ab891bb4"},
  {"luaui/shaders/distortion_gl4.vert.glsl","92340b9de6d00e4284ac4d82324b27bf49cd8366ed9b016cad113c60de69e895c9057af4c2e5de5ae88ae934dede8471f6eb2f0218880200db21551e41e0303a"},
  {"luaui/shaders/distortion_gl4.frag.glsl","ecd77671e5a9e8b2eae9cd339a47d3bda29ddda1f23ee2a7c64fc62600147e9df53548039882dbacb815ed26f7dcfd2358ab83e087890e368a2e064d6309aa9b"},
  {"luaui/shaders/screen_distortion_combine_gl4.vert.glsl","3923f4092a3cabce6f18c2887af45f5c7ce630f89655985dd76edb1ed1d4613eeb341be5d80ee200e5dc58f683eed07895704d6a481fbdeb9e02a1e069f5ee41"},
  {"luaui/shaders/screen_distortion_combine_gl4.frag.glsl","9c294e64623db684a708e2150606ef278ed75ef591d4345c83e51e2222778c06ef89448885ef3a64fddce050a636a41f82174fc835e1f21df844857b02b60b5e"},
  {"modules/graphics/instancevbotable.lua","bf128f904541628d4f89c1699c94461f604dcbbc9fa48164bc4ac6febdeba29c4c92786e9d1d0c054e3fd8fc55104766e0b0d0e70f2a6fbd3bd3149b0ea39256"},
  {"modules/graphics/luashader.lua","81858ab150aceb7d84f324708f3b3c4a5c8101df03c2a3a0597cc64413fea8ff7db558b711b18e930aa577bf7b0c6b96bfde83c342ca94508f72021742372436"},
  {"luaui/include/instancevbotable.lua","560d78e197982d7aabf9f7f13723ed14737c1556db9c25aeb6eb6a37a10b6eb9d9887672e368b54a83b97c855fc134b25eef931a6ae859ada955fcc2ea9e9dfe"},
  {"luaui/include/luashader.lua","182d1627951e914c5da9fa8495613e9148edbf378d4ade162b59fcbde678c6439c5805f754493e7d537921f40e4af093c93df1708748ed687da3828eb67d1e5c"},
  {"luaui/configs/distortiongl4config.lua","b5220213a02da5dcd6de6ff6e49317152c37ba69daabf5282489ad0ddd7d13ad99ad940452c119606245e4af9018e0a56a3ed70d4ca2d60e3417f78572b9acd4"},
  {"luaui/configs/distortiongl4weaponsconfig.lua","6d4b40e69277286c3e318dfd6a5f85b441a71f666f8e8c5b47235af808ba5d38b08fc47dcd81ace1293abff95549950aada8f983f50f9fddb4e0ac0245f3f13d"},
 };return identities;
}
}
void Configure(Services value){services=std::move(value);}
bool Available()
{
 auto& actual=OriginalNativeTextureContext();
 return services.ready&&services.compile&&services.ready()&&actual.Initialized()&&actual.UIAttached()&&actual.Registry().ContextGeneration()&&ui::GetRecorder().TextureObserver()==&actual.UI();
}
void Reset(){for(const auto& weak:contexts)if(auto context=weak.lock())context->Release();contexts.clear();if(OriginalNativeSourceProgramBinding().Current().kind==NativeSourceProgramBinding::Kind::Distortion)OriginalNativeSourceProgramBinding().Unbind();}
bool PushEntries(lua_State* state)
{
	sol::state_view lua(state); auto gl = sol::stack::get<sol::table>(state, -1); auto family = lua.create_table();
	auto buffer = family.new_usertype<Buffer>("Buffer", sol::no_constructor);
	Bind(buffer, "Define", [](Buffer& value, int count, sol::optional<sol::object> layout) { value.Live(); value.value->Define(count, layout); });
	Bind(buffer, "Upload", [](Buffer& value, const sol::stack_table& data, sol::optional<int> attribute, sol::optional<int> offset, sol::optional<int> first, sol::optional<int> last) {
		value.Live(); return value.value->Upload(data, attribute, offset, first, last);
	});
	Bind(buffer, "Download", [](Buffer& value, sol::optional<int> attribute, sol::optional<int> offset, sol::optional<int> count, sol::optional<bool> forceGPU) {
		value.Live(); return value.value->Download(attribute, offset, count, forceGPU);
	});
	Bind(buffer, "InstanceDataFromUnitIDs", [](Buffer& value, sol::stack_object ids, int attribute, sol::optional<int> offset) {
		value.Live(); Check(attribute == 10, "Original distortion unit attachment attribute differs");
		if (ids.is<int>()) return value.value->InstanceDataFromUnitIDs(ids.as<int>(), attribute, offset);
		return value.value->InstanceDataFromUnitIDs(ids.as<sol::stack_table>(), attribute, offset);
	});
	Bind(buffer, "Delete", [](Buffer& value) { value.Delete(); });
	Bind(buffer, "GetID", [](Buffer& value) { value.Live(); return value.value->GetID(); });
	Bind(buffer, "GetBufferSize", [](Buffer& value) { value.Live(); return value.value->GetBufferSize(); });
	Bind(buffer, "Clear", [](Buffer& value) { value.Live(); value.value->Clear(); });
	Bind(buffer, "DumpDefinition", [](Buffer& value) { value.Live(); value.value->DumpDefinition(); });
	auto array = family.new_usertype<VertexArray>("VertexArray", sol::no_constructor);
	Bind(array, "AttachVertexBuffer", [](VertexArray& value, std::shared_ptr<Buffer> buffer) { value.Attach(value.vertices, std::move(buffer), GL_ARRAY_BUFFER); });
	Bind(array, "AttachIndexBuffer", [](VertexArray& value, std::shared_ptr<Buffer> buffer) { value.Attach(value.indices, std::move(buffer), GL_ELEMENT_ARRAY_BUFFER); });
	Bind(array, "AttachInstanceBuffer", [](VertexArray& value, std::shared_ptr<Buffer> buffer) { value.Attach(value.instances, std::move(buffer), GL_ARRAY_BUFFER); });
	Bind(array, "Delete", [](VertexArray& value) { value.Delete(); });
	Bind(array, "DrawElements", [](VertexArray& value, unsigned mode, sol::optional<int> count, sol::optional<int> first, sol::optional<int> instances, sol::optional<int> baseVertex, sol::optional<int> firstInstance) {
		value.Draw(true, mode, count, first, instances, baseVertex, firstInstance);
	});
	Bind(array, "DrawArrays", [](VertexArray& value, unsigned mode, sol::optional<int> count, sol::optional<int> first, sol::optional<int> instances, sol::optional<int> firstInstance) {
		value.Draw(false, mode, count, first, instances, sol::optional<int>{}, firstInstance);
	});
	family.set("Buffer", sol::lua_nil, "VertexArray", sol::lua_nil);
	Bind(family, "Available", &Available);
	Bind(family, "CreateContext", [state]() { return CreateContext(state); });
	Bind(family, "LoadArchiveSource", [state](const std::string& path) {
		Check(!CLuaHandle::GetHandleSynced(state), "Original distortion archive source is unsynced only");
		const auto identity = SourceIdentities().find(Lower(path));
		Check(identity != SourceIdentities().end(), "Unknown selected original distortion archive source");
		CFileHandler file(path, SPRING_VFS_ZIP); std::string source;
		Check(file.FileExists() && file.LoadStringData(source), "Selected original distortion archive source is missing");
		Check(Hash(source) == identity->second, "Selected original distortion archive source identity changed");
		return source;
	});
	Bind(family, "PrepareSource", [](std::string source) {
		const auto hash = Hash(source);
		Check(std::any_of(SourceIdentities().begin(), SourceIdentities().end(), [&hash](const auto& item) { return item.second == hash; }),
			"Original distortion Lua source identity differs before private adaptation");
		return source;
	});
	gl.set("NativeDistortion", family); return true;
}
} // namespace LuaNativeDistortion
#else
namespace LuaNativeDistortion {
void Configure(Services) {}
bool Available() { return false; }
void Reset() {}
bool PushEntries(lua_State*) { return false; }
}
#endif
