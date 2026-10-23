/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeLegacyWaterLighting.h"
#include "Rendering/Backend/NativeTerrainShadingState.h"
#include "Rendering/Backend/NativeCUSMaterialControl.h"
#include "LuaNativeCUSMaterial.h"
#include "LuaNativePostprocess.h"
#include "LuaNativeVolumetricClouds.h"
#include "LuaNativeUI.h"
#include "LuaNativeImageReadback.h"
#include "LuaNativeCUSTextures.h"
#include "LuaNativeUnitStencil.h"
#include "LuaNativeUIDrawState.h"
#include "LuaInclude.h"
#include "LuaContextData.h"
#include "LuaOpenGL.h"
#include "LuaHandle.h"
#include "LuaDisplayLists.h"
#include "LuaUtils.h"
#include "Game/Camera.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativePostprocessRecorder.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/Backend/MapEdgeData.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/EngineBridge/NativeWaterGroundSourceCapture.h"
#include "Rendering/Textures/Bitmap.h"
#include "Rendering/Textures/NativeUIDDS.h"
#include "Rendering/Textures/NativeUITextureQualifiers.h"
#include "Rendering/Textures/NativeNamedImagePolicy.h"
#include "Rendering/Textures/nv_dds.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Sim/Units/UnitDefHandler.h"
#include "System/FileSystem/FileHandler.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/LineDrawer.h"
#include "Game/UI/CommandColors.h"
#include "System/Log/ILog.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <atomic>
#include <cmath>
#include <new>
#include <limits>
#include <string_view>
#include <stdexcept>
#include <unordered_map>

using namespace rendering;
using namespace rendering::ui;

namespace {

void Require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
int NativeGLInt(lua_State* L,int index)
{
	const double value=luaL_checknumber(L,index);
	Require(std::isfinite(value)&&value>=double(std::numeric_limits<int>::min())&&value<=double(std::numeric_limits<int>::max()),"GL integer argument outside finite signed range");
	return static_cast<int>(value); // Original Lua integer conversion truncates.
}
std::string LuaPrimitiveOrigin(lua_State* L)
{
	lua_Debug entry{};
	if (!lua_getstack(L,1,&entry) || !lua_getinfo(L,"Sl",&entry)) return "Lua caller unavailable";
	return std::string(entry.short_src)+":"+std::to_string(entry.currentline);
}
int ActiveTexture(lua_State* L,NativeUIRecorder& recorder)
{
	const int args=lua_gettop(L);
	if(args<2||!lua_isnumber(L,1)||!lua_isfunction(L,2))
		return luaL_error(L,"Incorrect arguments to gl.ActiveTexture(number, func, ...)");
	const int unit=NativeGLInt(L,1);
	if(unit<0||unit>=CGlobalRendering::MAX_TEXTURE_UNITS)
		return luaL_error(L,"Bad texture unit(%i) given to gl.ActiveTexture()",unit);
	recorder.LuaActiveTexture(unsigned(unit));
	const int error=lua_pcall(L,args-2,0,0);
	recorder.LuaActiveTexture(0);
	if(error){LOG_L(L_ERROR,"gl.ActiveTexture: error(%i) = %s",error,lua_tostring(L,-1));return lua_error(L);}
	return 0;
}
struct NativeTexture {
	std::shared_ptr<NativeUITextureBinding> binding;
	bool fbo=false;
};
struct NativeTextures {
	std::unordered_map<std::string,NativeTexture> textures;
	std::uint64_t lastCode=0;
	~NativeTextures() {
		for (auto& [name,texture]:textures) {
			if(auto*observer=GetRecorder().TextureObserver())observer->Delete(texture.binding);
			texture.binding->current.reset(); texture.binding->source.reset();
			if (texture.binding->postprocess) { texture.binding->postprocess->deleted=true; texture.binding->postprocess->current.reset(); }
		}
	}
};
char nativeTexturesKey;
NativeTextures& Textures(lua_State* L)
{
	lua_pushlightuserdata(L,&nativeTexturesKey); lua_rawget(L,LUA_REGISTRYINDEX);
	if (lua_isnil(L,-1)) {
		lua_pop(L,1); auto* value=new(lua_newuserdata(L,sizeof(NativeTextures))) NativeTextures();
		lua_newtable(L); lua_pushcfunction(L,[](lua_State* state)->int {
			static_cast<NativeTextures*>(lua_touserdata(state,1))->~NativeTextures(); return 0;
		}); lua_setfield(L,-2,"__gc"); lua_setmetatable(L,-2);
		lua_pushlightuserdata(L,&nativeTexturesKey); lua_pushvalue(L,-2); lua_rawset(L,LUA_REGISTRYINDEX);
		lua_pop(L,1); return *value;
	}
	Require(lua_type(L,-1)==LUA_TUSERDATA,"native texture registry is invalid");
	auto* value=static_cast<NativeTextures*>(lua_touserdata(L,-1)); lua_pop(L,1); return *value;
}
std::unordered_map<std::string,std::weak_ptr<const ScreenTexture>>& NamedImages()
{
	static std::unordered_map<std::string,std::weak_ptr<const ScreenTexture>> images; return images;
}
std::uint64_t NextImageGeneration() { static std::atomic<std::uint64_t> generation{0};return ++generation; }
std::shared_ptr<NativeUITextureBinding> UnitBuildPicture(const std::string& name)
{
	if (name.empty() || name[0]!='#' || unitDefHandler==nullptr) return {};
	// Original LuaOpenGLUtils accepts a decimal ID prefix with strtol.
	char* end=nullptr;errno=0;const char* start=name.c_str()+1;
	const auto value=std::strtol(start,&end,10);
	if (start==end || errno==ERANGE || value<1 || value>std::numeric_limits<int>::max()) return {};
	const auto* unitDef=unitDefHandler->GetUnitDefByID(int(value));
	return unitDef? CUnitDrawer::GetNativeUnitDefImage(unitDef): nullptr;
}
int OptionInt(lua_State* L,int table,const char* key,int fallback)
{
	lua_getfield(L,table,key); const int result=lua_isnumber(L,-1)? luaL_checkint(L,-1): fallback; lua_pop(L,1); return result;
}
bool OptionBool(lua_State* L,int table,const char* key,bool fallback=false)
{
	lua_getfield(L,table,key); const bool result=lua_isboolean(L,-1)? bool(lua_toboolean(L,-1)): fallback; lua_pop(L,1); return result;
}
lua_Number OptionNumber(lua_State* L,int table,const char* key,lua_Number fallback)
{
	lua_getfield(L,table,key); const auto result=lua_isnumber(L,-1)? lua_tonumber(L,-1): fallback; lua_pop(L,1); return result;
}
int GLWrap(ScreenWrap value)
{
	switch(value) {
		case ScreenWrap::Repeat: return GL_REPEAT;
		case ScreenWrap::ClampEdge: return GL_CLAMP_TO_EDGE;
		case ScreenWrap::TransparentBorder: return GL_CLAMP_TO_BORDER;
		case ScreenWrap::LegacyClamp: return GL_CLAMP;
	}
	throw std::runtime_error("invalid native texture wrapping state");
}
ScreenWrap Wrap(int value)
{
	switch(value) {
		case GL_REPEAT: return ScreenWrap::Repeat;
		case GL_CLAMP_TO_EDGE: return ScreenWrap::ClampEdge;
		case GL_CLAMP_TO_BORDER: return ScreenWrap::TransparentBorder;
		case GL_CLAMP: return ScreenWrap::LegacyClamp;
		default: throw std::runtime_error("texture wrapping mode is not implemented");
	}
}
void MinFilter(ScreenSampler& sampler,int value)
{
	switch(value) {
		case GL_NEAREST: sampler.filter=ScreenFilter::Nearest; sampler.mipFilter=ScreenMipFilter::None; break;
		case GL_LINEAR: sampler.filter=ScreenFilter::Linear; sampler.mipFilter=ScreenMipFilter::None; break;
		default: throw std::runtime_error("generated render-target mip filtering is not implemented");
	}
}
ScreenSampler TextureSampler(lua_State* L,int table,ScreenSampler sampler)
{
	const auto previousMag=sampler.magnificationFilter.value_or(sampler.filter);
	MinFilter(sampler,OptionInt(L,table,"min_filter",sampler.filter==ScreenFilter::Nearest? GL_NEAREST: GL_LINEAR));
	const auto mag=OptionInt(L,table,"mag_filter",previousMag==ScreenFilter::Nearest? GL_NEAREST: GL_LINEAR);
	Require(mag==GL_LINEAR || mag==GL_NEAREST,"invalid texture magnification filter");
	sampler.magnificationFilter=mag==GL_NEAREST? ScreenFilter::Nearest: ScreenFilter::Linear;
	sampler.wrapS=Wrap(OptionInt(L,table,"wrap_s",GLWrap(sampler.wrapS))); sampler.wrapT=Wrap(OptionInt(L,table,"wrap_t",GLWrap(sampler.wrapT)));
	Require(OptionInt(L,table,"compareFunc",GL_NONE)==GL_NONE && OptionNumber(L,table,"lodBias",0)==0 && OptionNumber(L,table,"aniso",0)==0,"texture comparison/LOD/aniso options are not implemented");
	ValidateScreenSampler(sampler);
	return sampler;
}
LuaNativeUI::CoverageTargetAllocator coverageTargetAllocator;

int CreateTexture(lua_State* L)
{
	const auto extent=[&](int index) {
		const auto value=luaL_checknumber(L,index);
		Require(std::isfinite(value) && value>=1 && value<=16384,"texture dimension invalid or outside bounds");
		return std::uint32_t(value);
	};
	const auto width=extent(1),height=extent(2);
	Require(!lua_isnumber(L,3),"3D/array/native multisample texture creation is not implemented");
	bool fbo=false;
	int format=GL_RGBA8;
	ScreenSampler sampler{ScreenFilter::Linear,ScreenWrap::Repeat,ScreenWrap::Repeat};
	if (lua_istable(L,3)) {
		Require(OptionInt(L,3,"target",GL_TEXTURE_2D)==GL_TEXTURE_2D,"only original 2D panel textures are implemented");
		format=OptionInt(L,3,"format",GL_RGBA8);
  // Unsized DEPTH_COMPONENT delegates precision to the implementation. Select
  // the existing normalized D24 copy contract explicitly, including its GPU
  // quantization, rather than misclassifying the visualizer copy as RGBA.
  if(format==GL_DEPTH_COMPONENT)format=GL_DEPTH_COMPONENT24;
		Require(format==GL_RGBA8 || format==GL_RGBA || format==GL_RGBA16F || format==GL_R8 || format==GL_R16F || format==GL_R32F ||
			format==GL_RG8 || format==GL_RG16F || format==GL_R11F_G11F_B10F ||
			format==GL_DEPTH_COMPONENT32 || format==GL_DEPTH_COMPONENT24 || format==GL_DEPTH_COMPONENT16,
			"requested original image format has no native producer/consumer");
		Require(!OptionBool(L,3,"border") && OptionNumber(L,3,"border",0)==0 && !OptionBool(L,3,"fboDepth") && OptionInt(L,3,"samples",0)==0,"texture border/depth/multisample storage is not implemented");
		fbo=OptionBool(L,3,"fbo"); sampler=TextureSampler(L,3,sampler);
	}
	Require(format!=GL_DEPTH_COMPONENT16 || !fbo,"Original D16 allocation belongs to the actual paired coverage FBO");
	const bool coverageAllocation=!fbo && (format==GL_R8 || format==GL_DEPTH_COMPONENT16);
	if(coverageAllocation)Require(bool(coverageTargetAllocator),"Original coverage attachments have no actual connected GPU allocation owner");
	if (!coverageAllocation && (format==GL_DEPTH_COMPONENT32 || format==GL_DEPTH_COMPONENT24 || !fbo)) {
		Require(format!=GL_RGBA16F && format!=GL_R8 && format!=GL_R16F && format!=GL_R32F && format!=GL_RG8 && format!=GL_RG16F && format!=GL_R11F_G11F_B10F,"Original scalar images require their actual FBO producer");
		Require(IsNativeScreenCopyConsumerAvailable(),"Original shared color/depth copy consumer has not initialized");
		Require(!fbo,"Depth-attachment RTT is not implemented by color/depth copies");
		NativePostprocessSampler shared;
		shared.minFilter=sampler.filter==ScreenFilter::Nearest?NativePostprocessFilter::Nearest:NativePostprocessFilter::Linear;
		shared.magFilter=sampler.magnificationFilter.value_or(sampler.filter)==ScreenFilter::Nearest?NativePostprocessFilter::Nearest:NativePostprocessFilter::Linear;
		const auto wrap=[](ScreenWrap value) {switch(value){case ScreenWrap::Repeat:return NativePostprocessWrap::Repeat;case ScreenWrap::ClampEdge:return NativePostprocessWrap::ClampEdge;case ScreenWrap::LegacyClamp:return NativePostprocessWrap::LegacyClamp;default:throw std::runtime_error("Copied image border addressing unsupported");}};
		shared.wrapS=wrap(sampler.wrapS);shared.wrapT=wrap(sampler.wrapT);
		auto binding=CreateNativePostprocessBinding(width,height,format==GL_DEPTH_COMPONENT32?NativePostprocessFormat::Depth32:format==GL_DEPTH_COMPONENT24?NativePostprocessFormat::Depth24Unorm:NativePostprocessFormat::RGBA8,
			shared,false,globalRendering?globalRendering->maxTextureSize:0);
		const auto name=LuaNativeUI::RegisterPostprocessTexture(L,std::move(binding));lua_pushstring(L,name.c_str());return 1;
	}
	const auto targetFormat=format==GL_RG8?ScreenTextureFormat::RG8Unorm:
		format==GL_RG16F?ScreenTextureFormat::RG16Float:format==GL_R11F_G11F_B10F?ScreenTextureFormat::RG11B10Float:
		format==GL_R8?ScreenTextureFormat::R8Unorm:
		format==GL_R16F?ScreenTextureFormat::R16Float:format==GL_R32F?ScreenTextureFormat::R32Float:
		format==GL_RGBA16F?ScreenTextureFormat::RGBA16Float:format==GL_DEPTH_COMPONENT16?ScreenTextureFormat::Depth16Unorm:ScreenTextureFormat::RGBA8Unorm;
	auto image=MakeRenderTargetTexture(width,height,{},targetFormat);
	auto binding=std::make_shared<NativeUITextureBinding>(); binding->current=image; binding->sampler=sampler;
	auto& owner=Textures(L); const auto name="!"+std::to_string(++owner.lastCode); image->sourceName=name;
	if(auto*observer=GetRecorder().TextureObserver())observer->Create(binding,name,format);
	owner.textures.emplace(name,NativeTexture{std::move(binding),fbo});
	lua_pushstring(L,name.c_str()); return 1;
}
// The selected original coverage module owns/refcounts these attachments in WG.
// Keep the public FBO userdata alive independently of Lua texture name deletion.
constexpr const char* CoverageFBOMetatable="NativeRangeCoverageFBO";
struct CoverageFBO {
 std::shared_ptr<NativeUITextureBinding> color,depth;
 std::shared_ptr<const void> storageOwner;
 std::shared_ptr<const ScreenRenderTargetIdentity> colorIdentity,depthIdentity;
 bool deleted=false;
 bool Live()const{
  return !deleted && coverageTargetAllocator && storageOwner && color && depth && color->current && depth->current &&
   color->current->renderTarget==colorIdentity && depth->current->renderTarget==depthIdentity &&
   color->current->format==ScreenTextureFormat::R8Unorm && depth->current->format==ScreenTextureFormat::Depth16Unorm &&
   color->current->width==depth->current->width && color->current->height==depth->current->height;
 }
};
CoverageFBO& GetCoverageFBO(lua_State* L){return *static_cast<CoverageFBO*>(luaL_checkudata(L,1,CoverageFBOMetatable));}
int CreateCoverageFBO(lua_State* L)
{
 Require(bool(coverageTargetAllocator) && lua_istable(L,1),"Original coverage FBO has no connected attachment allocator");
 lua_getfield(L,1,"color0");const std::string colorName=luaL_checkstring(L,-1);lua_pop(L,1);
 lua_getfield(L,1,"depth");const std::string depthName=luaL_checkstring(L,-1);lua_pop(L,1);
 auto color=LuaNativeUI::GetTextureBinding(L,colorName),depth=LuaNativeUI::GetTextureBinding(L,depthName);
 Require(color && depth && color->current && depth->current && color->current->renderTarget && depth->current->renderTarget &&
  color->current->format==ScreenTextureFormat::R8Unorm && depth->current->format==ScreenTextureFormat::Depth16Unorm &&
  color->current->width==depth->current->width && color->current->height==depth->current->height,
  "Original coverage FBO requires actual same-size owned R8+D16 attachments");
 lua_getfield(L,1,"drawbuffers");Require(lua_istable(L,-1)&&lua_objlen(L,-1)==1,"Original coverage draw buffer list differs");
 lua_rawgeti(L,-1,1);Require(luaL_checkint(L,-1)==0x8CE0,"Original coverage FBO is not color attachment0");lua_pop(L,2);
 auto storage=coverageTargetAllocator(color->current,depth->current);
 if(!storage){lua_pushnil(L);return 1;} // Actual allocation failure, original caller decides its path.
 if(luaL_newmetatable(L,CoverageFBOMetatable)){
  lua_pushcfunction(L,[](lua_State* state)->int{GetCoverageFBO(state).~CoverageFBO();return 0;});lua_setfield(L,-2,"__gc");
 }lua_pop(L,1);
 auto* fbo=new(lua_newuserdata(L,sizeof(CoverageFBO)))CoverageFBO{color,depth,std::move(storage),color->current->renderTarget,depth->current->renderTarget};
 (void)fbo;luaL_getmetatable(L,CoverageFBOMetatable);lua_setmetatable(L,-2);return 1;
}
int ActiveCoverageFBO(lua_State* L,NativeUIRecorder& recorder)
{
 auto& fbo=GetCoverageFBO(L);if(fbo.deleted)return 0;
 Require(fbo.Live() && recorder.CurrentWorldStage()==NativeWorldDrawStage::World && !recorder.IsCompiling(),
  "Coverage FBO must execute inside its actual World callin with live attachments");
 int callback=2;if(lua_isnumber(L,callback)){Require(NativeGLInt(L,callback)==0x8D40,"Coverage ActiveFBO target differs");++callback;}
 bool identities=false;if(lua_isboolean(L,callback)){identities=lua_toboolean(L,callback);++callback;}
 Require(lua_isfunction(L,callback),"ActiveFBO expects its actual callback");
 int error=0;
 {
  const auto previous=fbo.color->current,previousDepth=fbo.depth->current;
  auto next=std::make_shared<ScreenTexture>(*previous),nextDepth=std::make_shared<ScreenTexture>(*previousDepth);
  Require(next->generation!=std::numeric_limits<std::uint64_t>::max() && nextDepth->generation!=std::numeric_limits<std::uint64_t>::max(),"Coverage attachment version exhausted");
  ++next->generation;++nextDepth->generation;
  recorder.BeginTarget(next,previous->generation?previous:nullptr,nextDepth,previousDepth->generation?previousDepth:nullptr,identities,globalRendering->drawFrame,fbo.color,fbo.depth);
  error=lua_pcall(L,lua_gettop(L)-callback,0,0);
  if(error)recorder.AbortTarget();
  else{
   try{
    Require(fbo.Live(),"Coverage attachments deleted/replaced by callback");
    recorder.EndTarget();
   }catch(...){recorder.AbortTarget();throw;}
  }
 }
 if(error)return lua_error(L); // All temporary C++ owners have already unwound.
 return 0;
}
NativeDrawPrimitive Primitive(int mode)
{
	switch (mode) {
		case GL_QUADS: return NativeDrawPrimitive::Quads;
		case GL_POLYGON: return NativeDrawPrimitive::Polygon;
		case GL_TRIANGLES: return NativeDrawPrimitive::Triangles;
		case GL_TRIANGLE_STRIP: return NativeDrawPrimitive::TriangleStrip;
		case GL_TRIANGLE_FAN: return NativeDrawPrimitive::TriangleFan;
		case GL_LINES: return NativeDrawPrimitive::Lines;
		case GL_LINE_STRIP: return NativeDrawPrimitive::LineStrip;
		case GL_LINE_LOOP: return NativeDrawPrimitive::LineLoop;
		case GL_POINTS: return NativeDrawPrimitive::Points;
		default: throw std::runtime_error("primitive mode is not supported by the planar screen slice");
	}
}

struct ShapeVertexData {
	std::array<float,3> vertex{};
	std::array<float,3> normal{0,1,0};
	std::array<float,2> texCoord{};
	ScreenColor color{1,1,1,1};
	bool hasVertex=false,hasNormal=false,hasTexCoord=false,hasColor=false;
};

void ParseShapeVertex(lua_State* L,ShapeVertexData& value)
{
	// Match the original Shape row parser, including short colors, numeric
	// strings, ignored extra coordinates and lua_next ordering of aliases.
	const int table=lua_gettop(L);
	for (lua_pushnil(L);lua_next(L,table)!=0;lua_pop(L,1)) {
		Require(lua_istable(L,-1) && lua_israwstring(L,-2),"bad vertex data row");
		const std::string_view key=lua_tostring(L,-2);
		if (key=="v" || key=="vertex") {
			value.vertex={0,0,0};
			Require(LuaUtils::ParseFloatArray(L,-1,value.vertex.data(),3)>=2,"bad vertex array");
			value.hasVertex=true;
		} else if (key=="n" || key=="normal") {
			value.normal={0,1,0};
			Require(LuaUtils::ParseFloatArray(L,-1,value.normal.data(),3)==3,"bad normal array");
			value.hasNormal=true;
		} else if (key=="t" || key=="texcoord") {
			value.texCoord={0,0};
			Require(LuaUtils::ParseFloatArray(L,-1,value.texCoord.data(),2)==2,"bad texcoord array");
			value.hasTexCoord=true;
		} else if (key=="c" || key=="color") {
			value.color={1,1,1,1};
			Require(LuaUtils::ParseFloatArray(L,-1,value.color.data(),4)>=0,"bad color array");
			value.hasColor=true;
		}
	}
	Require(value.hasVertex,"bad vertex data");
}

std::array<float,4> Values(lua_State* L, int offset, unsigned count, std::array<float,4> value)
{
	if (lua_istable(L,offset)) {
		for (unsigned k=0; k<count; ++k) {
			lua_rawgeti(L,offset,k+1);
			if (!lua_isnil(L,-1)) value[k]=luaL_checknumber(L,-1);
			else Require(k >= 2,"missing vector coordinate");
			lua_pop(L,1);
		}
	} else {
		for (unsigned k=0; k<count; ++k) value[k]=luaL_optnumber(L,offset+k,value[k]);
	}
	return value;
}

std::shared_ptr<ScreenTexture> DDSTexture(const std::string& name)
{
	CFileHandler file(name);
	Require(file.FileExists() && file.FileSize()>=128 && file.FileSize()<=384*1024*1024,"real VFS DDS file missing or outside bounds");
	std::vector<std::uint8_t> bytes(file.FileSize());
	Require(file.Read(bytes.data(),bytes.size())==int(bytes.size()),"real VFS DDS read was truncated");
	const auto layout=ValidateNativeUIDDS(bytes);
	nv_dds::CDDSImage dds;
	// Preserve the original CBitmap DDS orientation, including its legacy
	// tiny-mip compressed flip behavior, by using the same loader operation.
	Require(dds.load(name,name.find("unitpics")==std::string::npos),"original VFS DDS loader failed");
	Require(dds.get_type()==nv_dds::TextureFlat && dds.get_depth()==1 && dds.get_num_mipmaps()+1==layout.mips.size(),"DDS loader image/mip contract mismatch");
	auto result=std::make_shared<ScreenTexture>();
	switch (layout.format) {
		case NativeDDSFormat::BC1: result->format=ScreenTextureFormat::BC1RGBA; Require(dds.get_format()==GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,"DDS BC1 format mismatch"); break;
		case NativeDDSFormat::BC2: result->format=ScreenTextureFormat::BC2RGBA; Require(dds.get_format()==GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,"DDS BC2 format mismatch"); break;
		case NativeDDSFormat::BC3: result->format=ScreenTextureFormat::BC3RGBA; Require(dds.get_format()==GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,"DDS BC3 format mismatch"); break;
		case NativeDDSFormat::BC4: result->format=ScreenTextureFormat::BC4Unorm; Require(dds.get_format()==0x8dbb,"DDS BC4 format mismatch"); break;
		case NativeDDSFormat::BC5: result->format=ScreenTextureFormat::BC5Unorm; Require(dds.get_format()==0x8dbd,"DDS BC5 format mismatch"); break;
		case NativeDDSFormat::BC7: result->format=ScreenTextureFormat::BC7RGBA; Require(dds.get_format()==0x8e8c,"DDS BC7 format mismatch"); break;
		case NativeDDSFormat::BC7SRGB: result->format=ScreenTextureFormat::BC7RGBASRGB; Require(dds.get_format()==0x8e8d,"DDS BC7SRGB format mismatch"); break;
		case NativeDDSFormat::BGRA8:
			result->format=ScreenTextureFormat::BGRA8Unorm;
			Require(dds.get_format()==GL_BGRA && dds.get_components()==4,"DDS BGRA format/component mismatch");break;
		case NativeDDSFormat::BGR8:
			result->format=ScreenTextureFormat::RGBA8Unorm;
			Require(dds.get_format()==GL_BGR && dds.get_components()==3,"DDS BGR format/component mismatch");break;
	}
	result->width=dds.get_width(); result->height=dds.get_height();
	result->mips.reserve(layout.mips.size());
	for (std::size_t level=0; level<layout.mips.size(); ++level) {
		const auto& expected=layout.mips[level];
		const auto* data=static_cast<unsigned char*>(dds);
		std::uint32_t width=dds.get_width(),height=dds.get_height(),size=dds.get_size();
		if (level>0) {
			const auto& surface=dds.get_mipmap(level-1);
			Require(surface.get_depth()==1,"DDS mip depth mismatch");
			width=surface.get_width(); height=surface.get_height(); size=surface.get_size();
			data=static_cast<unsigned char*>(surface);
		}
		Require(width==expected.width && height==expected.height && size==expected.byteCount,"DDS loader mip extent mismatch");
		ScreenTextureMip mip; mip.width=width; mip.height=height; mip.bytesPerRow=expected.bytesPerRow;
		if (layout.format==NativeDDSFormat::BGR8) {
			mip.bytes=ExpandNativeUIBGR8(std::span<const std::uint8_t>(data,size),width,height);
			mip.bytesPerRow=std::size_t(width)*4;
		} else mip.bytes.assign(data,data+size);
		result->mips.push_back(std::move(mip));
	}
	return result;
}

std::shared_ptr<const ScreenTexture> NamedTexture(const std::string& qualified, ScreenSampler& sampler)
{
	auto& images=NamedImages();
	const auto qualifiers=ParseNativeUITextureQualifiers(qualified);
	const auto& name=qualifiers.name;
	Require(!name.empty() && name[0]!='!' && name[0]!='#' && name[0]!='%' && name[0]!='$',"dynamic/unit/world texture is not implemented in the Ready slice");
	std::string lower=name;
	for (auto& c: lower) c=std::tolower(static_cast<unsigned char>(c));
	const bool isDDS=lower.ends_with(".dds");
	std::uint32_t maximumAnisotropy=1;
	if (qualifiers.anisotropic) {
		Require(globalRendering && std::isfinite(globalRendering->maxTexAnisoLvl) &&
			globalRendering->maxTexAnisoLvl>=1 && globalRendering->maxTexAnisoLvl<=16 &&
			std::floor(globalRendering->maxTexAnisoLvl)==globalRendering->maxTexAnisoLvl,
			"original :a: image requires the actual native maximum anisotropy");
		maximumAnisotropy=std::uint32_t(globalRendering->maxTexAnisoLvl);
	}
	const auto applyPolicy=[&](const std::shared_ptr<const ScreenTexture>& image) {
		const auto extra=isDDS ? std::uint32_t(image->mips.size()-1) : 0;
		sampler=GetNativeNamedImageUploadPolicy(qualifiers,image->format,isDDS,image->width,image->height,extra,maximumAnisotropy).sampler;
	};
	if (auto existing=images[qualified].lock()) {applyPolicy(existing);return existing;}
	// NamedTextures::Load returns false for an absent VFS image. Keep the
	// previous binding; this is distinct from a valid unit with texture zero.
	if (!CFileHandler::FileExists(name,SPRING_VFS_RAW_FIRST)) return {};
	if (isDDS) {
		auto image=DDSTexture(name); image->sourceName=name; image->generation=NextImageGeneration();
		Require(!image->mips.empty(),"original named DDS has no base level");
		const auto policy=GetNativeNamedImageUploadPolicy(qualifiers,image->format,true,image->width,image->height,std::uint32_t(image->mips.size()-1),maximumAnisotropy);

		sampler=policy.sampler;image->generateMipmaps=policy.generateMipmaps;
		// CreateDDSTexture bypasses bitmap resize/invert/gray/tint and the
		// clamp/border branches. Retain every original embedded mip unchanged.
		// Base-only BC retains its original mip request; the native uploader
		// supplies decoded transport to the shared GPU mip producer.
		if(auto*observer=GetRecorder().TextureObserver())observer->Named(qualified,image,sampler,GetRecorder().IsCompiling(),isDDS);
		images[qualified]=image; return image;
	}
	CBitmap bitmap;
	Require(bitmap.Load(name) && !bitmap.compressed && bitmap.channels==4 && bitmap.GetDataTypeSize()==1 && bitmap.xsize>0 && bitmap.ysize>0,"real VFS UI texture decode failed");
	if (qualifiers.resize) bitmap=bitmap.CreateRescaled(qualifiers.resize->first,qualifiers.resize->second);
	if (qualifiers.inverted) bitmap.InvertColors();
	if (qualifiers.greyed) bitmap.MakeGrayScale();
	if (qualifiers.tint) bitmap.Tint(qualifiers.tint->data());
	auto image=std::make_shared<ScreenTexture>();
	image->sourceName=name; image->generation=NextImageGeneration();
	image->width=bitmap.xsize; image->height=bitmap.ysize; image->bytesPerRow=std::size_t(bitmap.xsize)*4;
	image->format=ScreenTextureFormat::RGBA8Unorm;
	image->bytes.assign(bitmap.GetRawMem(),bitmap.GetRawMem()+bitmap.GetMemSize());
	const auto policy=GetNativeNamedImagePolicy(qualifiers,false,image->width,image->height,0,maximumAnisotropy);
	sampler=policy.sampler;image->generateMipmaps=policy.generateMipmaps;
	if(auto*observer=GetRecorder().TextureObserver())observer->Named(qualified,image,sampler,GetRecorder().IsCompiling(),isDDS);
		images[qualified]=image; return image;
}

NativeUILuaTextureLookup LookupUITexture(lua_State* L,const std::string& name)
{
	NativeUILuaTextureLookup result;
	if (name.empty()) return result;
	if (name.starts_with('#')) { result.binding=UnitBuildPicture(name);result.parsed=bool(result.binding); }
	else if (name.starts_with('!')) {auto& textures=Textures(L).textures;const auto found=textures.find(name);if(found!=textures.end()){result.binding=found->second.binding;result.parsed=true;}}
	else {result.image=NamedTexture(name,result.sampler);result.parsed=true;} // Original named TexInfo may have ID0 after a failed load.
	return result;
}

int DispatchImpl(lua_State* L, std::string_view op)
{
	if (op=="LoadOriginalPanelSource" || op=="LoadOriginalRendererSource") {
		Require(lua_gettop(L)==1 && !CLuaHandle::GetHandleSynced(L) && lua_type(L,1)==LUA_TSTRING,"Original panel source requires one unsynced archive path");
		std::size_t pathLength=0;
		const char* pathData=lua_tolstring(L,1,&pathLength);
		const std::string_view path(pathData,pathLength);
		const bool renderer = op=="LoadOriginalRendererSource";
		Require(renderer ? (path=="LuaUI/Widgets/gui_pip.lua" || path=="LuaUI/Widgets/map_edge_extension2.lua") :
			(path=="LuaUI/Widgets/gui_gridmenu.lua" || path=="LuaUI/Widgets/gui_info.lua"),"Unknown original archive source");
		CFileHandler file(std::string(path),SPRING_VFS_ZIP);
		std::string source;
		Require(file.FileExists() && file.LoadStringData(source),"Original panel archive source is missing");
		// Identity is checked against raw ZIP bytes; execute the same source after
		// the ordinary VFS preprocessing, without a second archive lookup.
		lua_pushlstring(L,source.data(),source.size());
		LuaUtils::TracyRemoveAlsoExtras(source.data());
		lua_pushlstring(L,source.data(),source.size());
		return 2;
	}
	if(auto result=LuaNativeUIDrawState::Dispatch(L,GetRecorder(),op))return *result;
	const int args=lua_gettop(L);
	if (op=="NativeCUSMaterialAvailable") {
		Require(args==0,"NativeCUSMaterialAvailable expects no arguments");
		lua_pushboolean(L,IsNativeCUSMaterialConsumerAvailable());return 1;
	}
	if (op=="NativeTerrainShadingAvailable") {
		Require(args==0,"NativeTerrainShadingAvailable expects no arguments");
		lua_pushboolean(L,IsNativeTerrainShadingConsumerAvailable());return 1;
	}
	auto& r=GetRecorder();
	if (op=="NativeMapEdgeSubmit"||op=="NativeMapEdgeSubmitDeferred") {
		const bool deferred=op=="NativeMapEdgeSubmitDeferred";
		Require(args==(deferred?8:7) && lua_istable(L,1) && lua_istable(L,7),"Native map-edge expects original mirrors, settings, mesh and deferred color source");
		const auto count=lua_objlen(L,1);
		Require(count<=32 && count%4==0,"map-edge mirror array must contain at most eight current31657 four-tuples");
		auto number=[&](int index) {
			Require(lua_type(L,index)==LUA_TNUMBER,"map-edge values must be numbers");
			const float value=lua_tonumber(L,index);
			Require(std::isfinite(value),"map-edge values must be finite");
			return value;
		};
		mapedge::Parameters parameters;
		parameters.deferred=deferred;
		parameters.gridSize=number(2);
		parameters.brightness=number(3);
		for (int index=4;index<=6;++index)
			Require(lua_isboolean(L,index),"map-edge curvature, fog and culling settings must be booleans");
		parameters.curvature=lua_toboolean(L,4);
		parameters.fog=lua_toboolean(L,5);
		parameters.cullBackFaces=lua_toboolean(L,6);
		Require(lua_objlen(L,7)==parameters.mesh.size(),"map-edge original mesh tuple arity differs");
		for(std::size_t index=0;index<parameters.mesh.size();++index){
			lua_rawgeti(L,7,index+1);const float value=number(-1);
			Require(value>=1 && value<=16777216 && std::floor(value)==value,"map-edge original mesh integer differs");
			parameters.mesh[index]=uint32_t(value);lua_pop(L,1);
		}
		std::array<mapedge::Mirror,8> mirrors{};
		for (std::size_t index=0;index<count;++index) {
			lua_rawgeti(L,1,index+1);
			mirrors[index/4][index%4]=number(-1);
			lua_pop(L,1);
		}
		const auto selected=std::span<const mapedge::Mirror>(mirrors.data(),count/4);
		if(!deferred){mapedge::Submit(parameters,selected);return 0;}
		mapedge::Validate(parameters,selected);
		auto* source=CurrentNativeWaterGroundSourceCapture();
		Require(source&&!r.IsCompiling()&&!r.HasActiveTarget()&&r.CurrentWorldStage()==NativeWorldDrawStage::Ground&&lua_type(L,8)==LUA_TSTRING,
			"Original deferred map-edge requires its actual Ground source and color name");
		mapedge::DrawPacket draw;draw.parameters=parameters;draw.mirrors.assign(selected.begin(),selected.end());
		auto& textures=OriginalNativeTextureContext();
		const std::array<std::string,3> names{lua_tostring(L,8),"$heightmap","$normals"};
		// Perform the original three Texture operations once. At the draw, retain
		// the actual bound versions, including the native $normals R/G/G/G view.
		for(unsigned unit=0;unit<names.size();++unit){
			const auto lookup=LuaNativeCUSTextures::ParseTexture(L,names[unit]);
			Require(lookup.parsed&&lookup.object,"Original deferred map-edge engine image has no actual source owner");
			textures.Registry().LuaTexture(unit,lookup);
			draw.images[unit]=textures.History().Binding(unit,NativeCUSTextureTarget::Image2D);
			if(unit<2)r.SyncObservedTextureBinding(unit);
		}
		draw.original=OriginalCUSUniformCapture().Current();draw.raster=r.CurrentWorldState();draw.surface=r.CurrentState().draw;
		source->SubmitMapEdge(std::move(draw));
		for(unsigned unit=0;unit<names.size();++unit){textures.UI().LuaEnable(unit,false);if(unit<2)r.SyncObservedTextureBinding(unit);}
		return 0;
	}
	if (op=="CreateList") {
		Require(args>=1 && lua_isfunction(L,1),"CreateList expects a callback");
		const bool previous=LuaOpenGL::IsDrawingEnabled(L);
		r.BeginList(); LuaOpenGL::SetDrawingEnabled(L,true);
		auto& tracker=GetLuaContextData(L)->glMatrixTracker;
		SMatrixStateData previousMatrix=tracker.PushMatrixState(true);
		const int error=lua_pcall(L,args-1,0,0);
		const SMatrixStateData compiledMatrix=tracker.GetMatrixState();
		tracker.PopMatrixState(previousMatrix,false); // Original nested CreateList also restores false.
		LuaOpenGL::SetDrawingEnabled(L,previous);
		if (error) {
			r.AbortList();
			LOG_L(L_ERROR,"[NativeUI] gl.CreateList callback failed (no retained list created): %s",lua_tostring(L,-1));
			// Original CreateList reports compilation failure as handle zero. Do
			// not turn an unsupported rendering list into a fatal gadget LoadCode
			// error, and never publish a successful handle for partial commands.
			lua_pushnumber(L,0); return 1;
		}
		try {
			auto list=r.EndList(); auto& lists=CLuaHandle::GetActiveDisplayLists(L);
			lua_pushnumber(L,lists.NewNativeList(std::move(list),compiledMatrix)); return 1;
		} catch (...) { r.AbortList(); throw; }
	}
	if (op=="DeleteList") {
		if (!lua_isnil(L,1)) CLuaHandle::GetActiveDisplayLists(L).FreeNativeList(luaL_checkint(L,1));
		return 0;
	}
	if (op=="CreateTexture") return CreateTexture(L);
	if (op=="DeleteTexture") {
		if (lua_isnil(L,1)) return 0;
		const std::string name=luaL_checkstring(L,1);
		lua_pushboolean(L,LuaNativeUI::DeleteTexture(L,name)); return 1;
	}
	// FBO deletion, like texture/list deletion, belongs to the original lifecycle.
	if (op=="DeleteFBO") {
		if (lua_isnil(L,1)) return 0;
		auto& fbo=GetCoverageFBO(L);
		fbo.deleted=true;fbo.storageOwner.reset();fbo.color.reset();fbo.depth.reset();return 0;
	}
	if (op=="ChangeTextureParams") {
		if (!lua_isstring(L,1) || !lua_istable(L,2)) return 0;
		auto& textures=Textures(L).textures; const auto found=textures.find(lua_tostring(L,1));
		if (found!=textures.end()) {
   const auto binding=found->second.binding;const auto sampler=TextureSampler(L,2,binding->requestedSampler.value_or(binding->sampler));binding->requestedSampler=sampler;
   r.RecordOperation([binding,sampler](NativeUIRecorder&replay){if(replay.IsCompiling())return;binding->sampler=sampler;if(auto*observer=replay.TextureObserver())observer->Params(binding,sampler);});
  }
		return 0;
	}
	if (op=="TextureInfo") {
		const std::string name=luaL_checkstring(L,1); std::shared_ptr<const ScreenTexture> texture;
		if (name.starts_with('#')) {
			auto binding=UnitBuildPicture(name);if (!binding) return 0;texture=binding->Resolve().image;
		} else if (name.starts_with('!')) {
			auto& textures=Textures(L).textures; const auto found=textures.find(name);
			if (found==textures.end()) return 0;
			if (const auto& shared=found->second.binding->postprocess) {
				lua_createtable(L,0,3);lua_pushnumber(L,shared->image->width);lua_setfield(L,-2,"xsize");lua_pushnumber(L,shared->image->height);lua_setfield(L,-2,"ysize");lua_pushnumber(L,GL_TEXTURE_2D);lua_setfield(L,-2,"target");return 1;
			}
			// Metadata queries describe the allocated image even when a mip
			// filter makes the sampling view incomplete after TexImage.
			texture=found->second.binding->current;
		} else { ScreenSampler sampler; texture=NamedTexture(name,sampler); }
		if (!texture) return 0;
		lua_createtable(L,0,4); lua_pushnumber(L,texture->width); lua_setfield(L,-2,"xsize");
		lua_pushnumber(L,texture->height); lua_setfield(L,-2,"ysize"); lua_pushnumber(L,GL_TEXTURE_2D); lua_setfield(L,-2,"target"); return 1;
	}
	Require(LuaOpenGL::IsDrawingEnabled(L),"drawing is only permitted in original draw call-ins/list compilation");
	if(op=="ActiveTexture")return ActiveTexture(L,r);
	if (op=="CopyToTexture") {
		const std::string name=luaL_checkstring(L,1);Require(name.starts_with('!'),"CopyToTexture can only write to original Lua textures");
		auto binding=LuaNativeUI::GetTextureBinding(L,name);if(!binding)return 0;
		Require(binding->postprocess || (binding->current && ScreenTextureTargetIdentity(*binding->current)),
			"CopyToTexture requires the actual original copy/FBO image");
		Require((lua_isnoneornil(L,8) || NativeGLInt(L,8)==GL_TEXTURE_2D) && (lua_isnoneornil(L,9) || NativeGLInt(L,9)==0),"CopyToTexture target/mip not implemented");
		const int xoff=NativeGLInt(L,2),yoff=NativeGLInt(L,3),x=NativeGLInt(L,4),y=NativeGLInt(L,5),width=NativeGLInt(L,6),height=NativeGLInt(L,7);
		Require(width>=0 && height>=0,"CopyToTexture source extent must be nonnegative");
		r.LuaTextureResource(binding,0);
		r.RecordOperation([binding,xoff,yoff,x,y,width,height](NativeUIRecorder& replay) {
			if(replay.IsCompiling())return;
			if(!width || !height)return; // Original zero-area copy only leaves the image bound/enabled.
			Require(!replay.HasActiveTarget(),"CopyToTexture inside active RTT needs its actual nested framebuffer source");
			auto stage=NativePostprocessStage::Screen;std::uint64_t sequence=0;std::uint32_t offset=0;
			if(auto world=replay.CurrentWorldStage()){stage=*world==NativeWorldDrawStage::PreDecals?NativePostprocessStage::PreDecals:(*world==NativeWorldDrawStage::PreUnit?NativePostprocessStage::PreUnit:NativePostprocessStage::World);sequence=NextNativeWorldDrawSequence();}
			else {Require(replay.IsScreenStage(),"CopyToTexture requires an actual world/screen source stage");offset=replay.RootCommandCount();}
			if(binding->postprocess) {
				LuaNativePostprocess::GetRecorder().Copy(binding->postprocess,IsNativePostprocessDepth(binding->postprocess->image->format)?NativePostprocessOperationKind::CopyDepth:NativePostprocessOperationKind::CopyColor,
					{x,y,std::uint32_t(width),std::uint32_t(height)},{xoff,yoff,std::uint32_t(width),std::uint32_t(height)},offset,stage,sequence);
				return;
			}
			const auto previous=binding->current;
			Require(previous && previous->format==ScreenTextureFormat::RGBA8Unorm &&
				previous->generation<UINT64_MAX-1,"Panel copy requires its live original RGBA8 allocation/version");
			const auto identity=ScreenTextureTargetIdentity(*previous);
			Require(identity && IsNativeScreenCopyConsumerAvailable(),"Panel copy has no actual framebuffer copy consumer");
			if(!binding->framebufferCopies) {
				// This is native contents storage for the SAME Lua image/cell, with
				// no new Lua texture, alias, sampler or producer queue.
				auto copies=CreateNativePostprocessBinding(previous->width,previous->height,NativePostprocessFormat::RGBA8,
					{},false,globalRendering?globalRendering->maxTextureSize:0);
				auto allocation=std::make_shared<NativePostprocessImage>(*copies->image);allocation->originalScreenTarget=identity;
				copies->image=std::move(allocation);binding->framebufferCopies=std::move(copies);
			}
			const auto copies=binding->framebufferCopies;
			Require(copies->image->originalScreenTarget==identity,"Panel copy changed its original FBO identity");
			copies->nextGeneration=std::max(copies->nextGeneration,previous->generation+1);
			LuaNativePostprocess::GetRecorder().Copy(copies,NativePostprocessOperationKind::CopyColor,
				{x,y,std::uint32_t(width),std::uint32_t(height)},{xoff,yoff,std::uint32_t(width),std::uint32_t(height)},offset,stage,sequence,
				previous->generation?previous:nullptr,[&replay,binding,previous](auto version) {
					auto next=std::make_shared<ScreenTexture>(*previous);next->renderTarget.reset();
					next->postprocessVersion=std::move(version);next->generation=next->postprocessVersion->generation;
					replay.CommitCopiedTargetBinding(binding,previous,std::move(next));
				});
			// Copy and RTT publication receipts share the UI's existing LIFO.
		});return 0;
	}
	if(op=="CreateFBO")return CreateCoverageFBO(L);
	if(op=="IsValidFBO"){const bool live=GetCoverageFBO(L).Live();lua_pushboolean(L,live);lua_pushnumber(L,live?0x8CD5:0x8CD6);return 2;}
	if(op=="ActiveFBO")return ActiveCoverageFBO(L,r);
	if (op=="SaveImage") return LuaNativeImageReadback::SaveImage(L);
	if (op=="RenderToTexture" || op=="NativeEditorRenderToTexture") {
		const std::string name=luaL_checkstring(L,1);
		Require(name.starts_with('!') && lua_isfunction(L,2),"RenderToTexture expects an owned FBO texture and callback");
		auto& textures=Textures(L).textures; const auto found=textures.find(name);
		if (found==textures.end() || !found->second.fbo) return 0;
		Require(!found->second.binding->postprocess,"External typed targets require their real paired program context");
		auto binding=found->second.binding; auto previous=binding->current;
		Require(previous && previous->generation!=UINT64_MAX,"RenderToTexture has no live unexhausted original image");
		auto next=std::make_shared<ScreenTexture>(*previous); ++next->generation;
		next->renderTarget=ScreenTextureTargetIdentity(*previous);next->postprocessVersion.reset();
		const bool editor=op=="NativeEditorRenderToTexture";
		Require(!editor || globalRendering,"Editor RenderToTexture has no actual draw frame");
		r.BeginTarget(next,previous->generation? previous: nullptr,{},{},true,
			editor?std::uint64_t(globalRendering->drawFrame):0,binding,{},editor);
		const int error=lua_pcall(L,args-2,0,0);
		if (error) { r.AbortTarget(); return lua_error(L); }
		try {
			r.EndTarget();
		} catch (...) { r.AbortTarget(); throw; }
		return 0;
	}
	if (op=="Clear") {
		const unsigned bits=luaL_checkint(L,1);
		Require(!(bits & ~(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT)),"unsupported native attachment clear mask");
		std::optional<ScreenColor> color;
		std::optional<float> depth;
		std::optional<std::int32_t> stencil;
		// Original LuaOpenGL::Clear updates a value only for its exact single
		// attachment bit. Combined masks retain all existing clear values.
		if(args==5){ScreenColor value{};for(unsigned n=0;n<4;++n)value[n]=luaL_checknumber(L,n+2);if(bits==GL_COLOR_BUFFER_BIT)color=value;}
		else if(args==2){const auto value=luaL_checknumber(L,2);if(bits==GL_DEPTH_BUFFER_BIT)depth=value;else if(bits==GL_STENCIL_BUFFER_BIT)stencil=std::int32_t(value);}
		unsigned mask=0;
		if(bits&GL_COLOR_BUFFER_BIT)mask|=ScreenAttachmentColor;
		if(bits&GL_DEPTH_BUFFER_BIT)mask|=ScreenAttachmentDepth;
		if(bits&GL_STENCIL_BUFFER_BIT)mask|=ScreenAttachmentStencil;
		r.ClearAttachments(mask,color,depth,stencil);return 0;
	}
	if (op=="CallList") {
		const unsigned index=luaL_checkint(L,1);
		auto& lists=CLuaHandle::GetActiveDisplayLists(L);
		auto list=lists.GetNativeList(index);
		if(list){
			auto matrix=lists.GetMatrixState(index);
			const int error=GetLuaContextData(L)->glMatrixTracker.ApplyMatrixState(matrix);
			Require(error==0,error>0?"Matrix stack overflow in gl.CallList":"Matrix stack underflow in gl.CallList");
			r.CallList(std::move(list));
		}
		return 0;
	}
	if (op=="Shape") {
		Require(lua_istable(L,2),"expected Shape(type, elements[])");
		const auto primitive=Primitive(luaL_checkint(L,1));
		r.BeginLegacyPrimitive(primitive,LuaPrimitiveOrigin(L));
		for (int index=1;;++index) {
			lua_rawgeti(L,2,index);
			if (lua_isnil(L,-1)) {lua_pop(L,1);break;}
			Require(lua_istable(L,-1),"bad vertex data, not a table");
			ShapeVertexData value;
			ParseShapeVertex(L,value);
			if (value.hasColor) r.Color(value.color);
			if (value.hasTexCoord) r.TexCoord(value.texCoord[0],value.texCoord[1]);
			if (value.hasNormal) r.Normal(value.normal[0],value.normal[1],value.normal[2]);
			r.LegacyVertex(value.vertex[0],value.vertex[1],value.vertex[2]);
			lua_pop(L,1);
		}
		r.EndLegacyPrimitive();
		return 0;
	}
	if (op=="BeginEnd") {
		Require(args>=2 && lua_isfunction(L,2),"BeginEnd expects primitive and callback");
		const auto previousErrors=r.LegacyInvalidOperationCount();
		r.BeginLegacyPrimitive(Primitive(luaL_checkint(L,1)),LuaPrimitiveOrigin(L));
		const int error=lua_pcall(L,args-2,0,0);
		if (error) { r.AbortPrimitive(); return lua_error(L); }
		try { r.EndLegacyPrimitive(); } catch (...) { r.AbortPrimitive(); throw; }
		static bool reportedLegacyError=false;
		if (!reportedLegacyError && r.LegacyInvalidOperationCount()!=previousErrors) {
			reportedLegacyError=true;
			LOG_L(L_WARNING,"[NativeUI] Original GL invalid-operation semantics recorded at %s (invalid Begin/End/Vertex or raster-state command ignored)",LuaPrimitiveOrigin(L).c_str());
		}
		return 0;
	}
	if (op=="Vertex") { auto v=Values(L,1,4,{0,0,0,1}); r.LegacyVertex(v[0],v[1],v[2],v[3]); return 0; }
	if (op=="Color") { auto v=Values(L,1,4,{1,1,1,1}); r.Color(v); return 0; }
	if (op=="Normal") {
		std::array<float,3> value{};
		if (args==1) Require(LuaUtils::ParseFloatArray(L,1,value.data(),3)==3,"bad normal array");
		else for (int index=0;index<3;++index) value[index]=luaL_checknumber(L,index+1);
		r.Normal(value[0],value[1],value[2]);
		return 0;
	}
	if (op=="TexCoord" || op=="MultiTexCoord") {
		unsigned unit=0;
		if(op=="MultiTexCoord") {const float value=luaL_checknumber(L,1);Require(std::isfinite(value)&&std::trunc(value)>=0&&std::trunc(value)<2,"only native planar texture units zero and one are implemented");unit=unsigned(std::trunc(value));}
		const int first=op=="MultiTexCoord"?2:1,count=args-first+1;
		std::array<float,4> v{0,0,0,1};
		if(count==1 && lua_istable(L,first)) {
			for(int index=0;index<4;++index){lua_rawgeti(L,first,index+1);if(!lua_isnumber(L,-1)){lua_pop(L,1);Require(index>0,"bad texture coordinate array");break;}v[index]=lua_tonumber(L,-1);lua_pop(L,1);}
		} else {
			Require(count>=1 && count<=4,"incorrect texture coordinate arguments");
			for(int index=0;index<count;++index)v[index]=luaL_checknumber(L,first+index);
		}
		Require(v[2]==0 && v[3]==1,"projective or non-planar texture coordinates require their original interpolation");
		r.MultiTexCoord(unit,v[0],v[1]);return 0;
	}
	if (op=="Rect" || op=="TexRect") {
		float x1=luaL_checknumber(L,1),y1=luaL_checknumber(L,2),x2=luaL_checknumber(L,3),y2=luaL_checknumber(L,4);
		float s1=0,t1=1,s2=1,t2=0;
		if (op=="TexRect" && args>6) { s1=luaL_checknumber(L,5); t1=luaL_checknumber(L,6); s2=luaL_checknumber(L,7); t2=luaL_checknumber(L,8); }
		else if (op=="TexRect") { if (lua_toboolean(L,5)) std::swap(s1,s2); if (lua_toboolean(L,6)) std::swap(t1,t2); }
		r.Begin(ScreenPrimitive::Quads,LuaPrimitiveOrigin(L));
		if (op=="TexRect") r.TexCoord(s1,t1); r.Vertex(x1,y1);
		if (op=="TexRect") r.TexCoord(s2,t1); r.Vertex(x2,y1);
		if (op=="TexRect") r.TexCoord(s2,t2); r.Vertex(x2,y2);
		if (op=="TexRect") r.TexCoord(s1,t2); r.Vertex(x1,y2);
		r.End(); return 0;
	}
	if (op=="Texture") {
		// Family-owned logical names retain their actual shared object, also in
		// display lists. Replay resolves current contents through the common
		// registry; one real LuaBind event is mirrored into recorder state.
		const int selectedIndex=lua_isnumber(L,1)?2:1;
		if(lua_type(L,selectedIndex)==LUA_TSTRING){
			NativeTextureLookup selected;
			if(LuaNativeUnitStencil::Lookup(L,lua_tostring(L,selectedIndex),selected)){
				unsigned requested=0;
				if(selectedIndex==2){const double n=luaL_checknumber(L,1);Require(std::isfinite(n)&&std::trunc(n)>=0&&std::trunc(n)<32,"Unit Stencil original texture unit out of range");requested=unsigned(std::trunc(n));}
				r.RecordOperation([requested,object=selected.object](NativeUIRecorder& recorder){
					auto& context=OriginalNativeTextureContext();Require(context.Initialized(),"Unit Stencil binding lost actual shared context");
					const auto actual=context.UI().LuaBind(requested,object);recorder.SyncObservedTextureBinding(actual);
				});lua_pushboolean(L,true);return 1;
			}
		}
		int index=1;unsigned unit=0;
		if (lua_isnumber(L,1)) {const float value=luaL_checknumber(L,1);Require(std::isfinite(value)&&std::trunc(value)>=0&&std::trunc(value)<2,"only native texture units zero and one are implemented");unit=unsigned(std::trunc(value));index=2;}
		if (lua_isboolean(L,index)) {
			r.LuaTextureEnabled(lua_toboolean(L,index),unit); lua_pushboolean(L,true); return 1;
		}
		const std::string name=luaL_checkstring(L,index);
		const bool parsed=r.LuaTextureLookup(unit,[&] { return LookupUITexture(L,name); });
		lua_pushboolean(L,parsed);return 1;
	}
	if (op=="Viewport") {
		Require(r.IsActive(),"Viewport requires the actual drawing stage or list");
		const int x=luaL_checkint(L,1),y=luaL_checkint(L,2),w=luaL_checkint(L,3),h=luaL_checkint(L,4);
		if(w<0)return luaL_argerror(L,3,"<width> must be greater than or equal zero!");
		if(h<0)return luaL_argerror(L,4,"<height> must be greater than or equal zero!");
		r.SetViewport({x,y,std::uint32_t(w),std::uint32_t(h)});return 0;
	}
	if (op=="PushAttrib") {
		unsigned mask=luaL_optint(L,1,GL_ALL_ATTRIB_BITS),native=0;
		if (mask==GL_ALL_ATTRIB_BITS) native=NativeUIAttribute::All;
		else {
			constexpr unsigned supported=GL_CURRENT_BIT|GL_COLOR_BUFFER_BIT|GL_SCISSOR_BIT|GL_TEXTURE_BIT|GL_ENABLE_BIT|GL_DEPTH_BUFFER_BIT|GL_POLYGON_BIT|GL_STENCIL_BUFFER_BIT|GL_LINE_BIT|GL_POINT_BIT|GL_VIEWPORT_BIT|GL_MULTISAMPLE_BIT;
			if(mask & ~supported)OriginalNativeLegacyWaterLighting().Invalidate();
			Require((mask & ~supported)==0,"attribute mask includes unsupported state");
			if(mask&GL_CURRENT_BIT) native|=Current;
			if(mask&GL_COLOR_BUFFER_BIT) native|=NativeUIAttribute::Blend|Alpha;
			if(mask&GL_SCISSOR_BIT) native|=Scissor;
			if(mask&GL_TEXTURE_BIT) native|=Texture;
			if(mask&GL_ENABLE_BIT) native|=NativeUIAttribute::Enable;
			if(mask&GL_DEPTH_BUFFER_BIT)native|=NativeUIAttribute::Depth;
			if(mask&GL_POLYGON_BIT)native|=NativeUIAttribute::Polygon;
			if(mask&GL_STENCIL_BUFFER_BIT)native|=NativeUIAttribute::Stencil;
			if(mask&GL_LINE_BIT)native|=NativeUIAttribute::Line;
			if(mask&GL_POINT_BIT)native|=NativeUIAttribute::Point;
			if(mask&GL_VIEWPORT_BIT)native|=NativeUIAttribute::Viewport;
			if(mask&GL_MULTISAMPLE_BIT)native|=NativeUIAttribute::Multisample;
			if(mask&GL_COLOR_BUFFER_BIT)native|=NativeUIAttribute::ColorMask;
		}
		r.PushAttrib(native); return 0;
	}
	if (op=="PopAttrib") { r.PopAttrib(); return 0; }
	if (op=="Scissor") {
		if (args==1) {
			Require(lua_isboolean(L,1),"Scissor enable must be a boolean");r.SetScissorEnabled(lua_toboolean(L,1));return 0;
		}
		Require(args==4,"Scissor expects four coordinates"); int w=luaL_checkint(L,3),h=luaL_checkint(L,4); Require(w>=0 && h>=0,"negative scissor extent");
		const std::int64_t x=std::int64_t(luaL_checkint(L,1))+globalRendering->viewPosX,y=std::int64_t(luaL_checkint(L,2))+globalRendering->viewPosY;
		Require(x>=INT32_MIN && x<=INT32_MAX && y>=INT32_MIN && y<=INT32_MAX,"Scissor window coordinate overflow");
		r.SetScissor(ScreenScissor{std::int32_t(x),std::int32_t(y),unsigned(w),unsigned(h),true}); return 0;
	}
	if (op=="AlphaToCoverage") {
		const bool force=luaL_optboolean(L,2,false);
		if(!force && globalRendering->msaaLevel<4)return 0;
		Require(r.IsActive()||r.IsCompiling(),"AlphaToCoverage outside a drawing/list stage");
		const bool enabled=luaL_checkboolean(L,1);
		r.UpdateWorldState([enabled](NativeWorldDrawState& state){state.alphaToCoverage=enabled;},false,true);
		return 0;
	}
	if (op=="AlphaTest") {
		auto test=r.CurrentState().draw.alphaTest;
		if (lua_isboolean(L,1)) test.enabled=lua_toboolean(L,1);
		else { int compare=luaL_checkint(L,1); Require(compare>=GL_NEVER && compare<=GL_ALWAYS,"invalid alpha comparison"); test.enabled=true; test.compare=ScreenAlphaCompare(compare-GL_NEVER); test.reference=luaL_checknumber(L,2); }
		r.SetAlphaTest(test); return 0;
	}
	if(op=="Billboard") {Require(camera!=nullptr,"original Billboard requires an actual camera");r.MultMatrix(std::to_array(camera->GetBillBoardMatrix().m));return 0;}
	if(op=="PolygonMode") {
		Require(r.IsActive()||r.IsCompiling(),"PolygonMode outside a drawing/list stage");
		const int face=NativeGLInt(L,1),mode=NativeGLInt(L,2);
		Require(face==GL_FRONT||face==GL_BACK||face==GL_FRONT_AND_BACK,"invalid polygon face");
		Require(mode==GL_POINT||mode==GL_LINE||mode==GL_FILL,"invalid polygon mode");
		const auto value=mode==GL_POINT?NativePolygonMode::Point:mode==GL_LINE?NativePolygonMode::Line:NativePolygonMode::Fill;
		r.UpdateWorldState([face,value](NativeWorldDrawState& state){if(face!=GL_BACK)state.frontPolygonMode=value;if(face!=GL_FRONT)state.backPolygonMode=value;},false,true);return 0;
	}
	if(op=="LineStipple") {
		Require(r.IsActive()||r.IsCompiling(),"LineStipple outside a drawing/list stage");
		if(args==1 && lua_isboolean(L,1)) {
			const bool enabled=lua_toboolean(L,1);r.UpdateWorldState([enabled](NativeWorldDrawState& state){state.lineStipple=enabled;},false,true);
		} else if(args==1 && lua_isstring(L,1)) {
			const unsigned pattern=0xffff & cmdColors.StipplePattern();const bool enabled=pattern!=0&&pattern!=0xffff;
			if(enabled) {
				// Preserve SetupLineStipple's original CPU queue-policy side effect,
				// then retain the exact current GL pattern/phase as GL_COMPILE does.
				lineDrawer.SetupLineStipple();const auto [factor,current]=lineDrawer.GetNativeStippleSettings();
				r.UpdateWorldState([factor,current](NativeWorldDrawState& state){state.lineStipple=true;state.stippleFactor=std::clamp(factor,1,256);state.stipplePattern=current;},false,true);
			} else r.UpdateWorldState([](NativeWorldDrawState& state){state.lineStipple=false;},false,true);
		} else {
			Require(args>=2,"LineStipple expects enable, original command pattern, or factor,pattern[,shift]");
			const int factor=std::clamp(NativeGLInt(L,1),1,256);unsigned pattern=std::uint16_t(NativeGLInt(L,2));
			if(args>=3 && lua_isnumber(L,3)){const int shift=(NativeGLInt(L,3)%16+16)%16;pattern=(pattern|(pattern<<16))>>shift;}
			r.UpdateWorldState([factor,pattern](NativeWorldDrawState& state){state.lineStipple=true;state.stippleFactor=factor;state.stipplePattern=std::uint16_t(pattern);},false,true);
		}
		return 0;
	}
	if(op=="PointSprite") {
		Require(lua_isboolean(L,1),"PointSprite expects an enable boolean");
		// Coordinate replacement is per active texture unit, and the original
		// third argument uses a nonstandard TexEnv origin operation. Neither
		// contract is silently invented by this fixed-function subset.
		Require(!(args>=2 && lua_isboolean(L,2) && lua_toboolean(L,2)),"Generic point sprite coordinate replacement is unsupported; requires its original texture-unit contract");
		Require(!(args>=3 && lua_isboolean(L,3)),"Generic PointSprite origin argument requires its original TexEnv contract");
		const bool enabled=lua_toboolean(L,1);
		r.UpdateWorldState([enabled](NativeWorldDrawState& state){state.point.sprite=enabled;},false,true);return 0;
	}
	if(op=="PointParameter") {
		std::array<float,3> attenuation{};for(unsigned n=0;n<3;++n)attenuation[n]=luaL_checknumber(L,int(n+1));
		std::array<std::optional<float>,3> limits{};for(unsigned n=0;n<3;++n)if(args>=int(n+4))limits[n]=luaL_checknumber(L,int(n+4));
		r.UpdateWorldState([attenuation,limits](NativeWorldDrawState& state){
			state.point.attenuation=attenuation;
			if(limits[0])state.point.sizeMin=*limits[0];if(limits[1])state.point.sizeMax=*limits[1];if(limits[2])state.point.fadeThreshold=*limits[2];
		},false,true);return 0;
	}
	if(op=="LineWidth" || op=="PointSize") {
		const float value=luaL_checknumber(L,1);Require(std::isfinite(value)&&value>0,"raster size must be finite and positive");
		r.UpdateWorldState([value,line=op=="LineWidth"](NativeWorldDrawState& state){if(line)state.lineWidth=value;else state.pointSize=value;},false,true);return 0;
	}
	if(op=="Culling") {
		Require(r.IsActive()||r.IsCompiling(),"Culling outside a drawing/list stage");
		Require(args==1,"Culling expects one enable boolean or face");
		if(lua_isboolean(L,1)){const bool value=lua_toboolean(L,1);r.UpdateWorldState([value](NativeWorldDrawState& state){state.cullEnabled=value;},false,true);}
		else {const int face=luaL_checkint(L,1);Require(face==GL_FRONT||face==GL_BACK||face==GL_FRONT_AND_BACK,"invalid cull face");const auto value=face==GL_FRONT?NativeWorldCull::Front:face==GL_BACK?NativeWorldCull::Back:NativeWorldCull::FrontAndBack;r.UpdateWorldState([value](NativeWorldDrawState& state){state.cullEnabled=true;state.cull=value;},false,true);}return 0;
	}
	if(op=="PolygonOffset") {
		if(args==1){Require(lua_isboolean(L,1),"PolygonOffset expects boolean");const bool value=lua_toboolean(L,1);r.UpdateWorldState([value](NativeWorldDrawState& state){state.polygonOffset=value;});}
		else {Require(args==2,"PolygonOffset expects enable or factor,units");const float factor=luaL_checknumber(L,1),units=luaL_checknumber(L,2);r.UpdateWorldState([factor,units](NativeWorldDrawState& state){state.polygonOffset=true;state.offsetFactor=factor;state.offsetUnits=units;});}return 0;
	}
	if(op=="ColorMask") {
		Require(args==1||args==4,"ColorMask expects one or four booleans");std::array<bool,4> value{};for(unsigned i=0;i<4;++i){const int index=args==1?1:int(i+1);Require(lua_isboolean(L,index),"ColorMask expects booleans");value[i]=lua_toboolean(L,index);}r.UpdateWorldState([value](NativeWorldDrawState& state){state.colorWrite=value;state.combinedColorWrite=value;},false,true);return 0;
	}
	if(op=="StencilTest"){Require(lua_isboolean(L,1),"StencilTest expects boolean");const bool value=lua_toboolean(L,1);r.UpdateWorldState([value](NativeWorldDrawState& state){state.stencilTest=value;},false,true);return 0;}
	if(op=="StencilMask" || op=="StencilFunc" || op=="StencilOp") {
		if(op=="StencilMask"){const auto value=std::uint32_t(luaL_checknumber(L,1));r.UpdateWorldState([value](NativeWorldDrawState& state){state.frontStencil.writeMask=state.backStencil.writeMask=value;},false,true);}
		else if(op=="StencilFunc"){const int compare=luaL_checkint(L,1),reference=luaL_checkint(L,2);Require(compare>=GL_NEVER&&compare<=GL_ALWAYS,"invalid stencil compare");const auto mask=std::uint32_t(luaL_checknumber(L,3));r.UpdateWorldState([compare,reference,mask](NativeWorldDrawState& state){for(auto* face:{&state.frontStencil,&state.backStencil}){face->compare=ScreenAlphaCompare(compare-GL_NEVER);face->reference=reference;face->readMask=mask;}},false,true);}
		else {const auto parse=[&](int index){switch(luaL_checkint(L,index)){case GL_KEEP:return NativeWorldStencilOp::Keep;case GL_ZERO:return NativeWorldStencilOp::Zero;case GL_REPLACE:return NativeWorldStencilOp::Replace;case GL_INCR:return NativeWorldStencilOp::IncrementClamp;case GL_DECR:return NativeWorldStencilOp::DecrementClamp;case GL_INVERT:return NativeWorldStencilOp::Invert;case GL_INCR_WRAP:return NativeWorldStencilOp::IncrementWrap;case GL_DECR_WRAP:return NativeWorldStencilOp::DecrementWrap;default:throw std::runtime_error("invalid stencil operation");}};const auto fail=parse(1),depthFail=parse(2),pass=parse(3);r.UpdateWorldState([fail,depthFail,pass](NativeWorldDrawState& state){for(auto* face:{&state.frontStencil,&state.backStencil}){face->fail=fail;face->depthFail=depthFail;face->pass=pass;}},false,true);}return 0;
	}
	if (op=="Lighting") {
		Require(lua_isboolean(L,1),"screen lighting enable must be boolean");
		if(lua_toboolean(L,1)){OriginalNativeLegacyWaterLighting().Invalidate();throw std::runtime_error("screen pipeline cannot enable lighting");}
		r.ObserveOriginalLighting(false);return 0;
	}
	if (op=="ResetState") { r.ResetDrawingState(); return 0; }
	if (op=="ResetMatrices") { Require(args==0,"ResetMatrices takes no arguments"); r.ResetDrawingMatrices(); return 0; }
	throw std::runtime_error("operation is not implemented by the original Ready UI slice");
}

int Dispatch(lua_State* L)
{
	const char* name=lua_tostring(L,lua_upvalueindex(1));
	char message[1536];
	try { return DispatchImpl(L,name); }
	catch (const std::exception& error) {
		static bool nestedReported=false;
		if (!nestedReported && std::strstr(error.what(),"nested primitive Begin")) {
			nestedReported=true;LOG_L(L_ERROR,"[NativeUI] First nested primitive: gl.%s: %s",name,error.what());
			for (int level=0;level<12;++level) {
				lua_Debug entry{};if (!lua_getstack(L,level,&entry)) break;
				if (lua_getinfo(L,"Sln",&entry)) LOG_L(L_ERROR,"[NativeUI] nested trace %d: %s:%d %s",level,entry.short_src,entry.currentline,entry.name?entry.name:"?");
			}
		}
		std::snprintf(message,sizeof(message),"Native UI gl.%s: %s",name,error.what());
		GetRecorder().AbortPrimitive();
	}
	// Lua error recovery must occur after the C++ exception has been destroyed.
	return luaL_error(L,"%s",message);
}

} // namespace

void LuaNativeUI::SetCoverageTargetAllocator(CoverageTargetAllocator allocator){coverageTargetAllocator=std::move(allocator);}

void LuaNativeUI::Install(lua_State* L)
{
	for (const char* name: {"CreateList","CallList","DeleteList","BeginEnd","Vertex","Color","Normal","TexCoord","MultiTexCoord","Rect","TexRect","Texture","TextureInfo",
		"Translate","Scale","Rotate","MatrixMode","LoadIdentity","LoadMatrix","MultMatrix","Ortho","PushMatrix","PopMatrix","PushAttrib","PopAttrib",
		"Blending","BlendFunc","BlendFuncSeparate","BlendEquation","BlendEquationSeparate","Scissor","AlphaTest","AlphaToCoverage","DepthTest","Lighting","Culling","ResetState","ResetMatrices",
		"Billboard","PolygonMode","LineStipple","LineWidth","PointSize","PointSprite","PointParameter","PolygonOffset","StencilTest","StencilMask","StencilFunc","StencilOp",
		"Shape","CreateFBO","IsValidFBO","DeleteFBO","ActiveFBO","CreateTexture","DeleteTexture","ChangeTextureParams","RenderToTexture","NativeEditorRenderToTexture","SaveImage","Clear","CopyToTexture","ActiveTexture","Viewport","ColorMask","DepthMask","PushPopMatrix","NativeMapEdgeSubmit","NativeMapEdgeSubmitDeferred","NativeTerrainShadingAvailable","NativeCUSMaterialAvailable","LoadOriginalPanelSource","LoadOriginalRendererSource"}) {
		lua_pushstring(L,name); lua_pushcclosure(L,Dispatch,1); lua_setfield(L,-2,name);
	}
	LuaNativeCUSMaterial::PushActionEntries(L);
	LuaNativePostprocess::PushEntries(L);
	LuaNativeVolumetricClouds::PushEntries(L);
}

std::shared_ptr<const ScreenTexture> LuaNativeUI::LoadBuildPicture(const std::string& fileName)
{
	Require(!fileName.empty(),"Native unit build picture filename is empty");
	std::string lower=fileName;for (auto& c:lower)c=std::tolower(static_cast<unsigned char>(c));
	std::shared_ptr<ScreenTexture> image;
	if (lower.ends_with(".dds")) {
		if (!CFileHandler::FileExists(fileName,SPRING_VFS_RAW_FIRST)) {
			// CBitmap keeps TextureNone/0x0 after a missing DDS; CreateDDSTexture
			// returns texture zero. Do not turn that result into a Lua exception
			// or substitute the separate missing-default red bitmap.
			image=std::make_shared<ScreenTexture>();image->incomplete=true;
			LOG_L(L_WARNING,"[NativeUI] Missing unit build picture \"%s\"; preserving original incomplete texture",fileName.c_str());
		} else image=DDSTexture(fileName);
	}
	else {
		CBitmap bitmap;
		// Original SetUnitDefImage ignores Load's boolean and keeps the original
		// red AllocDummy on an unreadable non-DDS override. Validate its actual
		// resulting storage instead; never route DDS through HEADLESS's dummy.
		bitmap.Load(fileName);
		Require(!bitmap.compressed && bitmap.channels==4 && bitmap.GetDataTypeSize()==1 && bitmap.xsize>0 && bitmap.ysize>0,"real VFS unit build picture storage is invalid");
		image=std::make_shared<ScreenTexture>();image->width=bitmap.xsize;image->height=bitmap.ysize;
		image->bytesPerRow=std::size_t(bitmap.xsize)*4;image->format=ScreenTextureFormat::RGBA8Unorm;
		image->bytes.assign(bitmap.GetRawMem(),bitmap.GetRawMem()+bitmap.GetMemSize());
	}
	image->sourceName=fileName;image->generation=NextImageGeneration();return image;
}

std::shared_ptr<NativeUITextureBinding> LuaNativeUI::GetTextureBinding(lua_State* L,const std::string& name)
{
	if (name.empty() || name[0]!='!') return {};
	auto& textures=Textures(L).textures;const auto found=textures.find(name);
	return found==textures.end()? nullptr:found->second.binding;
}

NativeTextureLookup LuaNativeUI::CaptureTextureLookup(lua_State* L,const std::string& name)
{
	NativeTextureLookup selected;if(LuaNativeUnitStencil::Lookup(L,name,selected))return selected;
	auto* observer=GetRecorder().TextureObserver();
	Require(observer,"CUS image lookup lacks the actual UI texture owner");
	const auto result=LookupUITexture(L,name);
	return {result.parsed,(result.binding||result.image)?observer->CaptureBind(result.binding,result.image):nullptr,false};
}

bool LuaNativeUI::DeleteTexture(lua_State* L,const std::string& name)
{
	auto& r=GetRecorder();
	bool freed=false;
	if (name.starts_with('!')) {
		auto& textures=Textures(L).textures;const auto found=textures.find(name);
		if (found!=textures.end()) {
			if(auto*observer=r.TextureObserver())observer->Delete(found->second.binding);
			if (found->second.binding->postprocess) {found->second.binding->postprocess->deleted=true;found->second.binding->postprocess->current.reset();}
			if (found->second.binding->framebufferCopies) {found->second.binding->framebufferCopies->deleted=true;found->second.binding->framebufferCopies->current.reset();}
			found->second.binding->current.reset();found->second.binding->source.reset();textures.erase(found);freed=true;
		}
	} else {if(auto*observer=r.TextureObserver())observer->DeleteNamed(name);freed=NamedImages().erase(name)!=0;}
	return freed;
}

std::shared_ptr<NativeUITextureBinding> LuaNativeUI::ResolveTextureBinding(lua_State* L,const std::string& name)
{
	if(name.empty())return {};
	if(name.starts_with('!'))return GetTextureBinding(L,name);
	if(name.starts_with('#'))return UnitBuildPicture(name);
	ScreenSampler sampler;
	auto image=NamedTexture(name,sampler);
	if(!image)return {};
	auto binding=std::make_shared<NativeUITextureBinding>();
	binding->current=std::move(image);binding->sampler=sampler;return binding;
}

std::shared_ptr<NativePostprocessBinding> LuaNativeUI::GetPostprocessTextureBinding(lua_State* L,const std::string& name)
{
	auto binding=GetTextureBinding(L,name);return binding?binding->postprocess:nullptr;
}

std::string LuaNativeUI::RegisterPostprocessTexture(lua_State* L,std::shared_ptr<NativePostprocessBinding> shared)
{
	Require(shared && shared->image && !shared->deleted,"Cannot register missing/deleted shared native image");
	ValidateNativePostprocessImage(*shared->image);
	auto binding=std::make_shared<NativeUITextureBinding>();binding->postprocess=std::move(shared);
	// Existing original UnitDefImage delegation queries allocation dimensions
	// before recording a draw. This metadata is never sampled: Resolve above
	// always requires the real current postprocess version and makes a snapshot.
	auto metadata=std::make_shared<ScreenTexture>();metadata->width=binding->postprocess->image->width;
	metadata->height=binding->postprocess->image->height;metadata->sourceName="shared native allocation metadata";
	binding->current=std::move(metadata);
	const auto& sampler=binding->postprocess->image->sampler;
	binding->sampler.filter=sampler.minFilter==NativePostprocessFilter::Nearest?ScreenFilter::Nearest:ScreenFilter::Linear;
	binding->sampler.magnificationFilter=sampler.magFilter==NativePostprocessFilter::Nearest?ScreenFilter::Nearest:ScreenFilter::Linear;
	const auto wrap=[](NativePostprocessWrap value){switch(value){case NativePostprocessWrap::Repeat:return ScreenWrap::Repeat;case NativePostprocessWrap::ClampEdge:return ScreenWrap::ClampEdge;case NativePostprocessWrap::LegacyClamp:return ScreenWrap::LegacyClamp;}throw std::runtime_error("Invalid shared native image addressing");};
	binding->sampler.wrapS=wrap(sampler.wrapS);binding->sampler.wrapT=wrap(sampler.wrapT);
	auto& owner=Textures(L);Require(owner.lastCode!=std::numeric_limits<std::uint64_t>::max() && owner.textures.size()<4096,"Native Lua image registry exhausted");
	const auto name="!"+std::to_string(owner.lastCode+1);
	if(auto*observer=GetRecorder().TextureObserver()){
  unsigned format=0;switch(binding->postprocess->image->format){case NativePostprocessFormat::RGBA8:format=GL_RGBA8;break;case NativePostprocessFormat::RG8:format=0x822B;break;case NativePostprocessFormat::R8Unorm:format=0x8229;break;case NativePostprocessFormat::RGBA16Float:format=GL_RGBA16F;break;case NativePostprocessFormat::Depth32:format=GL_DEPTH_COMPONENT32;break;case NativePostprocessFormat::Depth24Unorm:format=GL_DEPTH_COMPONENT24;break;case NativePostprocessFormat::BC1RGBA:format=0x83F1;break;}
  observer->Create(binding,name,format);
 }
	owner.textures.emplace(name,NativeTexture{binding,binding->postprocess->image->renderToTexture});++owner.lastCode;return name;
}
