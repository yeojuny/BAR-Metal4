/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "ScreenDrawList.h"
#include "NativeCUSTextureHistory.h"
#include "NativeTextureRegistry.h"
#include <memory>
#include <string>
namespace rendering::ui {
struct NativeUITextureBinding;
struct NativeUITexturePublication {
 std::shared_ptr<const NativeCUSTextureObject> previous,published;
};
// Public Lua operations use these typed events exactly once at execution or
// display-list replay. Internal typed-family state synchronization does not
// impersonate another original GL operation.
struct NativeUITextureObserver {
 virtual ~NativeUITextureObserver()=default;
 virtual void ActiveTexture(unsigned actualUnit)=0;
 virtual unsigned LuaEnable(unsigned requestedUnit,bool)=0;
 virtual std::shared_ptr<const NativeCUSTextureObject> CaptureBind(const std::shared_ptr<NativeUITextureBinding>&,const std::shared_ptr<const ScreenTexture>&)=0;
 virtual unsigned LuaBind(unsigned requestedUnit,std::shared_ptr<const NativeCUSTextureObject>)=0;
 virtual unsigned LuaLookup(unsigned requestedUnit,const std::function<NativeTextureLookup()>& actualParser)=0;
 struct ObservedImage{std::shared_ptr<const ScreenTexture> image;ScreenSampler sampler;};
 virtual ObservedImage BoundImage(unsigned actualUnit)=0;
 virtual bool Enabled(unsigned actualUnit)const=0;
 virtual unsigned LuaNoBind(unsigned requestedUnit,bool parsed)=0;
 virtual void Push(bool texture,bool enables)=0;
 virtual void Pop()=0;
 virtual void Reset2D()=0;
 virtual void Create(const std::shared_ptr<NativeUITextureBinding>&,const std::string& source,unsigned originalFormat)=0;
 virtual void Delete(const std::shared_ptr<NativeUITextureBinding>&)=0;
 virtual void Params(const std::shared_ptr<NativeUITextureBinding>&,ScreenSampler)=0;
 virtual NativeUITexturePublication Publish(const std::shared_ptr<NativeUITextureBinding>&)=0;
 virtual void CancelPublication(const NativeUITexturePublication&)noexcept=0;
 // Actual named image loading owns registration, rather than first draw.
 virtual void Named(const std::string&,const std::shared_ptr<const ScreenTexture>&,ScreenSampler,bool compiling,bool flatDDS=false)=0;
 virtual void DeleteNamed(const std::string&)=0;
 virtual void UpdateNamed()=0;
};
} // namespace rendering::ui
