/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeCUSTextures.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaAtlasTextures.h"
#include "LuaHandle.h"
#include "LuaNativePBRSource.h"
#include "LuaNativeUnitStencil.h"
#include "LuaNativeUI.h"
#include "LuaOpenGLUtils.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeNamedVolumeSource.h"
#include "Rendering/Backend/NativeS3OTextureSource.h"
#include "Rendering/Backend/NativeCubeTextureSource.h"
#include "Rendering/Backend/NativeShadowTextureSource.h"
#include "Rendering/Backend/NativeMapTexturePublication.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Rendering/LuaObjectDrawer.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Env/CubeMapHandler.h"
#include "Map/ReadMap.h"
#include "Map/BaseGroundDrawer.h"
#include "Rendering/Textures/NamedTextures.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace LuaNativeCUSTextures {
namespace {
using namespace rendering;
NativeTextureContext& Context(lua_State* L)
{
	if (!L || CLuaHandle::GetHandleSynced(L))
		throw std::runtime_error("CUS texture providers require the original unsynced Lua state");
	auto& context=OriginalNativeTextureContext();
	if (!context.Initialized() || !context.UIAttached())
		throw std::runtime_error("CUS texture providers lack the original shared texture context");
	if (rendering::ui::GetRecorder().TextureObserver()!=&context.UI())
		throw std::runtime_error("CUS resource parser and UI recorder have different texture owners");
	return context;
}
bool SelectedOriginalVolumeName(const std::string& qualified)
{
	std::string_view path=qualified;
	if(!path.empty()&&path.front()==':'){
		const auto end=path.find(':',1);
		if(end==std::string_view::npos)return false;
		path.remove_prefix(end+1);
	}
	return OriginalNativeNamedVolumes().Supports(std::string(path));
}
NativeCUSTextureTarget Target(unsigned original)
{
	switch (original) {
		case 0x0DE1: return NativeCUSTextureTarget::Image2D;
		case 0x806F: return NativeCUSTextureTarget::Image3D;
		case 0x8513: return NativeCUSTextureTarget::Cube;
		case 0x9100: return NativeCUSTextureTarget::Image2DMultisample;
		case 0x8C1A: return NativeCUSTextureTarget::Image2DArray;
	}
	throw std::runtime_error("Original CUS texture target lacks a native owner");
}
}

rendering::NativeTextureLookup ParseTexture(lua_State* L,const std::string& name)
{
	auto& context=Context(L);
	NativeTextureLookup owned;
	if (LuaNativeUnitStencil::Lookup(L,name,owned)) return owned;
	if (LuaNativePBRSource::Lookup(L,name,owned)) return owned;
	if (name.empty()) return {};
	if(SelectedOriginalVolumeName(name)&&ui::GetRecorder().IsCompiling())
		throw std::runtime_error("Original DDS3D list compilation requires its deferred named-source upload consumer");
	if (!SelectedOriginalVolumeName(name) && name.front()!='%' && name.front()!='$' && name.front()!=LuaAtlasTextures::prefix)
		return LuaNativeUI::CaptureTextureLookup(L,name);

	// Reuse the original engine selector for model, shadow, map, cube and
	// geometry names. Parsing/GetTextureID never invokes LuaMatTexture::Bind.
	LuaMatTexture original;
	if (!LuaOpenGLUtils::ParseTextureImage(L,original,name)) return {};
	const auto actualName=original.GetTextureID();
	const bool shadow=original.type==LuaMatTexture::LUATEX_SHADOWMAP;
	if (!actualName) return {true,{},shadow}; // Original parsed ID0 preserves the inherited binding.
	auto object=context.Registry().Find(actualName);
	if (!object)
		throw std::runtime_error("Original engine texture has no actual native source owner: "+name);
	if (object->target!=Target(original.GetTextureTarget()))
		throw std::runtime_error("Original engine texture target differs from its native source owner: "+name);
	return {true,std::move(object),shadow};
}

bool ParseMaterialTexture(lua_State* L,LuaMatTexture& descriptor,const std::string& name)
{
	(void)Context(L);
	if(name.empty())return LuaOpenGLUtils::ParseTextureImage(L,descriptor,name);
	// Native UI/PBR ! allocations and ordinary named2D images currently have
	// different original LuaTextures/CNamedTextures descriptor namespaces.
	// Do not silently manufacture a raw descriptor or normalize their real
	// native owner to0. The direct CUS parser has its separate actual UI path.
	if(name.front()!='%'&&name.front()!='$'&&name.front()!=LuaAtlasTextures::prefix&&!SelectedOriginalVolumeName(name))
		throw std::runtime_error("Original legacy material texture requires its actual typed named/UI alias consumer: "+name);
	return LuaOpenGLUtils::ParseTextureImage(L,descriptor,name);
}

rendering::NativeTextureLookup ResolveMaterialTexture(const LuaMatTexture& descriptor)
{
	auto& context=OriginalNativeTextureContext();
	if(!context.Initialized()||!context.UIAttached()||ui::GetRecorder().TextureObserver()!=&context.UI())return {};
	bool sourceObserved=false,nameObserved=false,zeroObserved=false;
	uint32_t actualName=0;
	switch(descriptor.type){
		case LuaMatTexture::LUATEX_NONE:return {true,{},false};
		case LuaMatTexture::LUATEX_UNITTEXTURE1:
		case LuaMatTexture::LUATEX_UNITTEXTURE2:sourceObserved=OriginalNativeS3OTextures().Attached();break;
		case LuaMatTexture::LUATEX_MAP_REFLECTION:
		case LuaMatTexture::LUATEX_SKY_REFLECTION:
		case LuaMatTexture::LUATEX_SPECULAR:
			sourceObserved=OriginalNativeCubeTextures().Attached()&&cubeMapHandler.HasNativeTextureOwner();zeroObserved=sourceObserved;break;
		case LuaMatTexture::LUATEX_SHADOWMAP:
		case LuaMatTexture::LUATEX_SHADOWCOLOR:
			sourceObserved=OriginalNativeShadowTextures().Attached()&&shadowHandler.HasNativeTextureOwner();zeroObserved=sourceObserved;break;
		case LuaMatTexture::LUATEX_HEIGHTMAP:
		case LuaMatTexture::LUATEX_SMF_NORMALS:sourceObserved=OriginalNativeMapTextures().HasOriginalAllocation();break;
		case LuaMatTexture::LUATEX_MAP_GBUFFER_NORM:
		case LuaMatTexture::LUATEX_MAP_GBUFFER_DIFF:
		case LuaMatTexture::LUATEX_MAP_GBUFFER_SPEC:
		case LuaMatTexture::LUATEX_MAP_GBUFFER_EMIT:
		case LuaMatTexture::LUATEX_MAP_GBUFFER_MISC:
		case LuaMatTexture::LUATEX_MAP_GBUFFER_ZVAL:
			sourceObserved=readMap&&readMap->GetGroundDrawer()&&context.OwnsGeometry(readMap->GetGroundDrawer()->GetGeometryBuffer());zeroObserved=sourceObserved;break;
		case LuaMatTexture::LUATEX_MODEL_GBUFFER_NORM:
		case LuaMatTexture::LUATEX_MODEL_GBUFFER_DIFF:
		case LuaMatTexture::LUATEX_MODEL_GBUFFER_SPEC:
		case LuaMatTexture::LUATEX_MODEL_GBUFFER_EMIT:
		case LuaMatTexture::LUATEX_MODEL_GBUFFER_MISC:
		case LuaMatTexture::LUATEX_MODEL_GBUFFER_ZVAL:
			sourceObserved=unitDrawer&&context.OwnsGeometry(unitDrawer->GetGeometryBuffer());zeroObserved=sourceObserved;break;
		case LuaMatTexture::LUATEX_NAMED:{
			actualName=descriptor.GetTextureID();nameObserved=true;
			sourceObserved=actualName&&OriginalNativeNamedVolumes().Owns(actualName);break;
		}
		default:return {}; // Unconnected atlas/font/Lua/name owner is not observed0.
	}
	if(!sourceObserved)return {};
	const auto name=nameObserved?actualName:descriptor.GetTextureID();const auto originalTarget=descriptor.GetTextureTarget();
	const bool shadow=descriptor.type==LuaMatTexture::LUATEX_SHADOWMAP;
	if(!name)return zeroObserved?NativeTextureLookup{true,{},shadow}:NativeTextureLookup{};
	// Attachment alone cannot prove an earlier original constructor chose the
	// native writer. Nonzero names must resolve in this actual context too.
	auto object=context.Registry().Find(name);
	if(!object||object->target!=Target(originalTarget))return {};
	return {true,std::move(object),shadow};
}

bool DeleteTexture(lua_State* L,const std::string& name)
{
	auto& history=Context(L).History();
	const std::array before{history.Binding(0,NativeCUSTextureTarget::Image2D),
		history.Binding(1,NativeCUSTextureTarget::Image2D)};
	const bool freed=SelectedOriginalVolumeName(name)?CNamedTextures::Free(name):
		(LuaNativePBRSource::DeleteTexture(L,name) || LuaNativeUI::DeleteTexture(L,name));
	// The owning Delete records the one real deletion. Reflect alias removal
	// only in affected generic UI units: mirroring an unrelated/missing name
	// would itself change their observed material state.
	auto& recorder=rendering::ui::GetRecorder();
	// Deletion is immediate during list compilation. Existing observed slots
	// resolve history at draw; Emit must not add a later deletion-side mirror
	// to the compiled command list.
	if (recorder.IsCompiling()) return freed;
	for (unsigned unit=0; unit<before.size(); ++unit) {
		const auto after=history.Binding(unit,NativeCUSTextureTarget::Image2D);
		if (before[unit].knowledge!=after.knowledge ||
			bool(before[unit].object)!=bool(after.object) ||
			(before[unit].object && after.object && before[unit].object->identity!=after.object->identity))
			recorder.SyncObservedTextureBinding(unit);
	}
	return freed;
}
}
#endif
