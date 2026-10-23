/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4PBRSource.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Lua/LuaNativePBRSource.h"
namespace rendering::metal4 {
// The concrete source/GPU owner. Its lifetime is the renderer's actual context;
// gadget reload only creates/deletes original Lua-owned images/programs.
class PBRSourceService {
public:
	using Parser=std::function<NativeTextureLookup(lua_State*,const std::string&)>;
	void Initialize(NativeTextureContext&,Parser,std::function<bool()> actualResourcesReady,
		std::function<uint64_t()> drawFrame,std::function<void()> validateOriginalRaster,bool validation);
	void Shutdown(); // After ReleaseLuaState for each actual Lua handle.
	bool Ready()const;
	bool HasPending(NativePBRKind)const;
	std::unique_ptr<PreparedPBRSource> Prepare(RuntimeLease&,NativePBRKind,const CUSSourceImageProvider&);
	void PruneCompleted();
	void ResetWorld(); // All common submissions retired; world teardown precedes Lua Shutdown.
	CUSSourceImage Image(RuntimeLease&,const NativeCUSTextureBinding&,uint64_t actualSubmission)const;
	double CompileMilliseconds()const{return compileMilliseconds;}
private:
	PBRSourceGPU gpu;
	NativeTextureContext* context=nullptr;
	std::function<bool()> resourcesReady;
	std::vector<std::shared_ptr<const NativePBRImage>> pending;
	double compileMilliseconds=0;
	bool validation=true;
};
}
