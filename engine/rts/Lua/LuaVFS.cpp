/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include <cmath>
#include <cstdint>
#include <string_view>
#include <cstdio>
#include <stdexcept>

#include <monocypher-ed25519.h>

#include "LuaVFS.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "NativeWidgetAdapters.h"
#include "NativeLuaMenuDisplayAdapter.h"
#endif
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaHashString.h"
#include "LuaIO.h"
#include "LuaUtils.h"
#include "LuaZip.h"
#include "System/FileSystem/FileHandler.h"
#include "System/FileSystem/ArchiveScanner.h"
#include "System/FileSystem/VFSHandler.h"
#include "System/FileSystem/FileSystem.h"
#include "System/Log/ILog.h"
#include "System/StringUtil.h"
#include "System/TimeProfiler.h"
#include "../tools/pr-downloader/src/pr-downloader.h"
#include "fmt/format.h"

#include "System/Misc/TracyDefs.h"
#include <tracy/TracyLua.hpp>

/*** Unified IO for archive and filesystem
 *
 * The Virtual File System is a unified layer to access (read-only) the
 * different archives used at runtime. So you can access map, game & config
 * files via the same interface.
 * 
 * ## Overview
 * 
 * Although Spring can access the filesystem directly (via os module) it is
 * more common that you would want to access files included with your game or
 * Spring. Trouble is, most of these files are compressed into archives
 * (`.sdz`/`.sd7`) so random access would generally be a difficult procedure.
 * Fortunately, the Spring Lua system automatically provides access to mod and
 * base files via the VFS module.
 *
 * As an additional caveat, synced Lua cannot use the `os` and `io` modules,
 * so using VFS is mandatory there to have any file access at all.
 *
 * The VFS module doesn't simply open archives though. What it does is map
 * your game files, game dependencies and Spring content onto a virtual file
 * tree. All archives start from the 'roots' of the tree and share the same
 * virtual space, meaning that if two or more archives contain the same
 * resource file name the resources overlap and only one of the files will be
 * retrieved. Overlapping directories on the other hand are merged so the
 * resulting virtual directory contains the contents of both. Here is an
 * example of how this works:
 *
 * **Archive 1** (`games/mygame.sd7`)
 *
 * ```
 * textures
 * └── texture1.png
 * models
 * └── model1.mdl
 * ```
 *
 * **Archive 2** (`base/springcontent.sdz`)
 * 
 * ```
 * textures
 * ├── texture1.png
 * ├── texture2.png
 * └── texture3.png
 * ```
 *
 * **VFS**
 * 
 * ```
 * textures
 * ├── texture1.png
 * ├── texture2.png
 * └── texture3.png
 * models
 * └── model1.mdl
 * ```
 *
 * This raises the question: If both archives have a `texture1.png` then which
 * `texture1.png` is retreived via the VFS? The answer depends on the order the
 * archives are loaded and the VFS mode (more on modes below). Generally
 * however, each archive loaded overrides any archives loaded before it. The
 * standard order of loading (from first to last) is:
 * 
 *  1. The automatic dependencies `springcontent.sdz` and `maphelper.sdz`.
 *  2. Dependencies listed in your `modinfo.lua` (or `modinfo.tdf`), in the order listed.
 * Note that they are loaded fully and recursively, i.e. all the deeper dependencies of the 1st base-level dependency are
 * loaded before the 2nd base-level dependency. This breaks the usual "loaded later overrides loaded earlier" priority if
 * a dependency comes from multiple places, since only the first time an archive is loaded counts.
 *  3. Your mod archive.
 *
 * Loose files (not within any archive) in the engine dir are also visible
 * as if under the VFS root if loading under the `VFS.RAW` mode, though you
 * can also use full FS path (i.e. `C:/.../Spring/foo/bar.txt` is visible
 * both as that and as just `foo/bar.txt`). Note that `VFS.RAW` is only
 * accessible to unsynced Lua, all synced states are limited to loaded archives.
 *
 * ## Paths
 * 
 * Spring's VFS is **lowercase only**. Also it is **strongly** recommended to
 * use linux style path separators, e.g. `"foo/bar.txt"` and not `"foo\bar.txt"`.
 * 
 * ## Engine read files
 * 
 * The engine access a few files directly, most of them are lua files which
 * access other files themselves. Here the list of files that must exist in the
 * VFS (some of them don't have to be in the game/map archive cause there are
 * fallback solutions in `springcontent.sdz` & `maphelper.sdz`):
 * 
 * - `./`
 *   - anims/
 *     - `cursornormal.bmp/png`
 *   - gamedata/
 *     - `defs.lua`
 *     - `explosions.lua`
 *     - `explosion_alias.lua`
 *     - `icontypes.lua`
 *     - `messages.lua`
 *     - `modrules.lua`
 *     - `resources.lua`
 *     - `resources_map.lua`
 *     - `sidedata.lua`
 *     - `sounds.lua`
 *   - `luagaia/`
 *     - `main.lua`
 *     - `draw.lua`
 *   - `luarules/`
 *     - `main.lua`
 *     - `draw.lua`
 *   - `luaui/`
 *     - `main.lua`
 *   - `shaders/`
 *     - `?`
 *   - `luaai.lua`
 *   - `mapinfo.lua`
 *   - `mapoptions.lua`
 *   - `modinfo.lua`
 *   - `modoptions.lua`
 *   - `validmaps.lua`
 *
 * @table VFS
 */

bool LuaVFS::PushCommon(lua_State* L)
{

	/*** @field VFS.RAW "r" Only select uncompressed files. */
	LuaPushNamedString(L, "RAW",       SPRING_VFS_RAW);
	/*** @field VFS.GAME "M" */
	LuaPushNamedString(L, "GAME",      SPRING_VFS_MOD); // synonym to MOD
	/*** @field VFS.MAP "m" */
	LuaPushNamedString(L, "MAP",       SPRING_VFS_MAP);
	/*** @field VFS.BASE "b" */
	LuaPushNamedString(L, "BASE",      SPRING_VFS_BASE);
	/*** @field VFS.MENU "e" */
	LuaPushNamedString(L, "MENU",      SPRING_VFS_MENU);
	/*** @field VFS.ZIP "Mmeb" Only select compressed files (`.sdz`, `.sd7`). */
	LuaPushNamedString(L, "ZIP",       SPRING_VFS_ZIP);
	/*** @field VFS.RAW_FIRST "rMmeb" Try uncompressed files first, then compressed. */
	LuaPushNamedString(L, "RAW_FIRST", SPRING_VFS_RAW_FIRST);
	/*** @field VFS.ZIP_FIRST "Mmebr" Try compressed files first, then uncompressed. */
	LuaPushNamedString(L, "ZIP_FIRST", SPRING_VFS_ZIP_FIRST);

	/***
	 * @deprecated
	 * @field VFS.MOD "M" Older spelling for `VFS.GAME`
	 */
	LuaPushNamedString(L, "MOD",       SPRING_VFS_MOD);
	/***
	 * @deprecated
	 * @field VFS.RAW_ONLY "r"
	 */
	LuaPushNamedString(L, "RAW_ONLY",  SPRING_VFS_RAW); // backwards compatibility
	/***
	 * @deprecated
	 * @field VFS.ZIP_ONLY "Mmeb"
	 */
	LuaPushNamedString(L, "ZIP_ONLY",  SPRING_VFS_ZIP); // backwards compatibility

	LuaPushNamedCFunc(L, "PackU8",    PackU8);
	LuaPushNamedCFunc(L, "PackU16",   PackU16);
	LuaPushNamedCFunc(L, "PackU32",   PackU32);
	LuaPushNamedCFunc(L, "PackS8",    PackS8);
	LuaPushNamedCFunc(L, "PackS16",   PackS16);
	LuaPushNamedCFunc(L, "PackS32",   PackS32);
	LuaPushNamedCFunc(L, "PackF32",   PackF32);
	LuaPushNamedCFunc(L, "UnpackU8",  UnpackU8);
	LuaPushNamedCFunc(L, "UnpackU16", UnpackU16);
	LuaPushNamedCFunc(L, "UnpackU32", UnpackU32);
	LuaPushNamedCFunc(L, "UnpackS8",  UnpackS8);
	LuaPushNamedCFunc(L, "UnpackS16", UnpackS16);
	LuaPushNamedCFunc(L, "UnpackS32", UnpackS32);
	LuaPushNamedCFunc(L, "UnpackF32", UnpackF32);

	// compression should be safe in synced context
	LuaPushNamedCFunc(L, "ZlibCompress", ZlibCompress);
	LuaPushNamedCFunc(L, "ZlibDecompress", ZlibDecompress);
	LuaPushNamedCFunc(L, "CalculateHash", CalculateHash);
	LuaPushNamedCFunc(L, "VerifyEd25519", VerifyEd25519);

	return true;
}


bool LuaVFS::PushSynced(lua_State* L)
{
	PushCommon(L);

	LuaPushNamedCFunc(L, "Include",    SyncInclude);
	LuaPushNamedCFunc(L, "LoadFile",   SyncLoadFile);
	LuaPushNamedCFunc(L, "FileExists", SyncFileExists);
	LuaPushNamedCFunc(L, "DirList",    SyncDirList);
	LuaPushNamedCFunc(L, "SubDirs",    SyncSubDirs);

	return true;
}


bool LuaVFS::PushUnsynced(lua_State* L)
{
	PushCommon(L);

	LuaPushNamedCFunc(L, "Include",             UnsyncInclude);
	LuaPushNamedCFunc(L, "LoadFile",            UnsyncLoadFile);
	LuaPushNamedCFunc(L, "FileExists",          UnsyncFileExists);
	LuaPushNamedCFunc(L, "DirList",             UnsyncDirList);
	LuaPushNamedCFunc(L, "SubDirs",             UnsyncSubDirs);

	LuaPushNamedCFunc(L, "GetFileAbsolutePath",      GetFileAbsolutePath);
	LuaPushNamedCFunc(L, "GetArchiveContainingFile", GetArchiveContainingFile);

	LuaPushNamedCFunc(L, "UseArchive",     UseArchive);
	LuaPushNamedCFunc(L, "CompressFolder", CompressFolder);

	// Removed due to sync unsafety, see commit 0ee88788931f9f0b195eb5f895f1092fde4211c0
	// LuaPushNamedCFunc(L, "MapArchive",     MapArchive);
	// LuaPushNamedCFunc(L, "UnmapArchive",   UnmapArchive);

	return true;
}


/******************************************************************************/
/******************************************************************************/

const string LuaVFS::GetModes(lua_State* L, int index, bool synced)
{
	const bool vfsOnly = (synced && !CLuaHandle::GetDevMode());

	const char* defModes = vfsOnly? SPRING_VFS_ZIP : SPRING_VFS_RAW_FIRST;
	const char* badModes = vfsOnly? SPRING_VFS_RAW SPRING_VFS_MENU : "";

	return CFileHandler::ForbidModes(luaL_optstring(L, index, defModes), badModes);
}


/******************************************************************************/

static int LoadFileWithModes(const std::string& fileName, std::string& data, const std::string& vfsModes)
{
	CFileHandler fh(fileName, vfsModes);

	if (!fh.FileExists())
		return (fh.LoadCode());

	return (fh.LoadStringData(data));
}


/******************************************************************************/
/******************************************************************************/


#ifdef RECOIL_METAL4_LIVE_SCENE
namespace {
// Run only AFTER the original loader selected bytes using its actual VFS mode.
// AddonRevs.ParseAddon passes the real addon environment through VFS.Include;
// no staged menu, archive replacement or LuaUI route broadening is involved.
int AdaptNativeLuaMenuDisplayText(lua_State* L, bool synced, const std::string& filename,
                                 std::string& data, char* reason, size_t capacity)
{
    if (synced) return 0;
    const auto* handle = CLuaHandle::GetHandle(L);
    if (handle == nullptr || handle->GetName() != "LuaMenu" ||
        StringToLower(filename) != NativeLuaMenuDisplayAdapter::path) return 0;
    try {
        return NativeLuaMenuDisplayAdapter::Adapt(data) ? 1 : 0;
    } catch (const std::exception& error) {
        std::snprintf(reason, capacity, "%s", error.what());
        return -1;
    }
}

// Return bytes to the existing original loader, preserving its environment,
// mod/user classification and selection. Never add loose user-widget entries.
int LoadNativeWidgetText(lua_State* L, bool synced, const std::string& filename,
                        const std::string& mode, bool library, std::string& data,
                        char* reason, size_t capacity)
{
    if (synced || mode != SPRING_VFS_ZIP)
        return 0;
    const auto* handle = CLuaHandle::GetHandle(L);
    if (handle == nullptr || handle->GetName() != "LuaUI")
        return 0;

    const std::string lowerName = StringToLower(filename);
    const NativeWidgetAdapters::Entry* selected = nullptr;
    for (const auto& entry: NativeWidgetAdapters::entries) {
        if (lowerName == entry.originalPath) {
            selected = &entry;
            break;
        }
    }
    if (selected == nullptr)
        return 0;

    int callerLineDefined = 0;
    int callerLastLineDefined = 0;
    if (library) {
        // Original PiP entries require the same library in their own environment
        // (pipNumber 0/2). Keep the Include load/setfenv/pcall/return path intact.
        if (lowerName != "luaui/widgets/gui_pip.lua" || !lua_istable(L, 2))
            return 0;
    } else {
        lua_Debug caller{};
        if (!lua_getstack(L, 1, &caller) || !lua_getinfo(L, "S", &caller) || !caller.source)
            return 0;
        callerLineDefined = caller.linedefined;
        callerLastLineDefined = caller.lastlinedefined;
        std::string_view source(caller.source);
        if (source.starts_with('@'))
            source.remove_prefix(1);
        if (StringToLower(std::string(source)) != "luaui/barwidgets.lua"
            || !((callerLineDefined == 1238 && callerLastLineDefined == 1412)
                 || (callerLineDefined == 1269 && callerLastLineDefined == 1443)))
            return 0;
    }

    try {
        std::string original;
        if (LoadFileWithModes("LuaUI/barwidgets.lua", original, SPRING_VFS_ZIP) != 1)
            throw std::runtime_error("Original LuaUI widget handler is missing");
        if (original.empty() || original.size() > 524288)
            throw std::runtime_error("Original LuaUI widget handler has invalid extent");
        sha512::raw_digest raw;
        sha512::hex_digest hex{};
        sha512::calc_digest({original.data(), original.data() + original.size()}, raw);
        sha512::dump_digest(raw, hex);
        const auto handlerIdentity = std::string_view(hex.data());
        const bool reviewed31657 = handlerIdentity == NativeWidgetAdapters::handlerSHA512
            && (library || (callerLineDefined == 1238 && callerLastLineDefined == 1412));
        const bool reviewed31691 = handlerIdentity == "738dd12f4df4d17fcb9c2079e0a071121896fbc975d9f9e5c5435721950a7e6436ee26ac88850fe8b9793663f585502eb0620c7610f43d7a26ba57fa6e5067cb"
            && (library || (callerLineDefined == 1269 && callerLastLineDefined == 1443));
        if (!reviewed31657 && !reviewed31691)
            throw std::runtime_error("Native widget routing requires review: original handler identity changed");

        if (LoadFileWithModes(selected->stagedPath, data, SPRING_VFS_RAW) != 1)
            throw std::runtime_error(std::string("Staged native widget adapter is missing: ") + selected->stagedPath);
        if (data.empty() || data.size() > 65536)
            throw std::runtime_error("Staged native widget adapter has invalid extent");
        sha512::calc_digest({data.data(), data.data() + data.size()}, raw);
        sha512::dump_digest(raw, hex);
        if (std::string_view(hex.data()) != selected->sha512)
            throw std::runtime_error(std::string("Staged native widget adapter identity changed: ") + selected->stagedPath);
        return 1;
    } catch (const std::exception& error) {
        std::snprintf(reason, capacity, "%s", error.what());
        return -1;
    }
}
}
#endif


/***
 * Loads and runs lua code from a file in the VFS.
 * 
 * @function VFS.Include
 * 
 * The path is relative to the main Spring directory, e.g.
 * 
 * ```lua
 * VFS.Include('LuaUI/includes/filename.lua', nil, vfsmode)
 * ```
 * 
 * @param filename string
 * 
 * Path to file, lowercase only. Use linux style path separators, e.g.
 * `"foo/bar.txt"`.
 * 
 * @param environment table? (Default: the current function environment)
 * 
 * The environment arg sets the global environment (see generic lua refs). In
 * almost all cases, this should be left `nil` to preserve the current env.
 *  
 * If the provided, any non-local variables and functions defined in
 * `filename.lua` are then accessable via env. Vise-versa, any variables
 * defined in env prior to passing to `VFS.Include` are available to code in the
 * included file. Code running in `filename.lua` will see the contents of env in
 * place of the normal global environment.
 * 
 * @param mode string?
 * 
 * VFS modes are single char strings and can be concatenated;
 * doing specifies an order of preference for the mode (i.e. location) from
 * which to include files.
 * 
 * @return any module The return value of the included file.
 */
int LuaVFS::Include(lua_State* L, bool synced)
{
	const std::string fileName = luaL_checkstring(L, 1);
	      std::string fileData;

	#if 0
	ScopedOnceTimer timer("LuaVFS::Include(" + fileName + ")");
	#endif

	// the path may point to a file or dir outside of any data-dir
	// if (!LuaIO::IsSimplePath(fileName)) return 0;

	// note: this check must happen before luaL_loadbuffer gets called
	// it pushes new values on the stack and if only index 1 was given
	// to Include those by luaL_loadbuffer are pushed to index 2,3,...
	const bool hasCustomEnv = !lua_isnoneornil(L, 2);

	if (hasCustomEnv)
		luaL_checktype(L, 2, LUA_TTABLE);

	int loadCode = 0;
	int luaError = 0;

	const auto mode = GetModes(L, 3, synced);
#ifdef RECOIL_METAL4_LIVE_SCENE
	char routeReason[512]{};
	loadCode = LoadNativeWidgetText(L, synced, fileName, mode, true, fileData, routeReason, sizeof(routeReason));
	if (loadCode < 0)
		return luaL_error(L, "[NativeWidgets VFS] %s", routeReason);
	if (loadCode == 0)
#endif
		loadCode = LoadFileWithModes(fileName, fileData, mode);
	if (loadCode != 1) {
		std::string_view hint {""};
		if (loadCode == -1) // magic value from VFSHandler
			hint = "File not seen by VFS (missing or in different VFS mode)";

		const auto buf = fmt::format("[LuaVFS::{}(synced={})][loadvfs] file={} status={} cenv={} vfsmode={} {}", __func__, synced, fileName, loadCode, hasCustomEnv, mode, hint);
		lua_pushlstring(L, buf.c_str(), buf.size());
 		lua_error(L);
	}

#ifdef RECOIL_METAL4_LIVE_SCENE
	if (AdaptNativeLuaMenuDisplayText(L, synced, fileName, fileData, routeReason, sizeof(routeReason)) < 0)
		return luaL_error(L, "[NativeLuaMenu VFS] %s", routeReason);
#endif
	LuaUtils::TracyRemoveAlsoExtras(fileData.data());
	if ((luaError = luaL_loadbuffer(L, fileData.c_str(), fileData.size(), fileName.c_str())) != 0) {
		const auto buf = fmt::format("[LuaVFS::{}(synced={})][loadbuf] file={} error={} ({}) cenv={} vfsmode={}", __func__, synced, fileName, luaError, lua_tostring(L, -1), hasCustomEnv, mode);
		lua_pushlstring(L, buf.c_str(), buf.size());
		lua_error(L);
	}


	// set the chunk's fenv to the current fenv, or a user table
	if (hasCustomEnv) {
		luaL_checktype(L, 2, LUA_TTABLE);
		lua_pushvalue(L, 2);
	} else {
		LuaUtils::PushCurrentFuncEnv(L, __func__);
		luaL_checktype(L, -1, LUA_TTABLE);
	}

	// set the include fenv to the current function's fenv
	if (lua_setfenv(L, -2) == 0)
		luaL_error(L, "[LuaVFS::%s(synced=%d)][setfenv] file=%s type=%d cenv=%d", __func__, synced, fileName.c_str(), lua_type(L, -2), hasCustomEnv);


	const int paramTop = lua_gettop(L) - 1;

	if ((luaError = lua_pcall(L, 0, LUA_MULTRET, 0)) != 0) {
		const auto buf = fmt::format("[LuaVFS::{}(synced={})][pcall] file={} error={} ({}) ptop={} cenv={} vfsmode={}", __func__, synced, fileName, luaError, lua_tostring(L, -1), paramTop, hasCustomEnv, mode);
		lua_pushlstring(L, buf.c_str(), buf.size());
		lua_error(L);
	}

	// FIXME -- adjust stack?
	return lua_gettop(L) - paramTop;
}


int LuaVFS::SyncInclude(lua_State* L)
{
	return Include(L, true);
}


int LuaVFS::UnsyncInclude(lua_State* L)
{
	return Include(L, false);
}


/******************************************************************************/

/***
 * Load raw text data from the VFS.
 * 
 * @function VFS.LoadFile
 * 
 * Returns file contents as a string. Unlike `VFS.Include` the file will not be
 * executed. This lets you pre-process the code. Use `loadstring` afterwards.
 *
 * @param filename string
 * 
 * Path to file, lowercase only. Use linux style path separators, e.g.
 * `"foo/bar.txt"`.
 *
 * @param mode string?
 * 
 * VFS modes are single char strings and can be concatenated;
 * doing specifies an order of preference for the mode (i.e. location) from
 * which to include files.
 * 
 * @return string? data The contents of the file.
 */
#ifdef RECOIL_METAL4_LIVE_SCENE
namespace {
// The original LuaRules manager loads ZIP_ONLY, so a loose native family
// adapter would otherwise never execute. Route these verified unsynced
// loader call; Include, other sources and the adapter's archive reads retain
// their original VFS policy. There is no process-wide recursion/DevLua flag.
int LoadNativeGadgetAdapter(lua_State* L, bool synced, char* reason, size_t capacity)
{
	if(synced)return 0;
	const char* filename=luaL_checkstring(L,1);
	const char* mode=lua_tostring(L,2);
	if(!mode || std::string_view(mode)!=SPRING_VFS_ZIP)return 0;
	const std::string lowerName=StringToLower(filename);
	const char* adapterPath=nullptr;
	for(const char* path:{"LuaRules/Gadgets/gfx_nano_particles_gl4.lua", "LuaRules/Gadgets/gfx_beam_laser_gl4.lua", "LuaRules/Gadgets/gfx_plasma_cannon_gl4.lua", "LuaRules/Gadgets/cus_gl4.lua", "LuaRules/Gadgets/api_pbr_enabler.lua", "LuaRules/Gadgets/gfx_energy_explosion_particles_gl4.lua", "LuaRules/Gadgets/gfx_fire_smoke_gl4.lua", "LuaRules/Gadgets/gfx_missile_thruster_gl4.lua", "LuaRules/Gadgets/gfx_flamethrower_gl4.lua", "LuaRules/Gadgets/gfx_fire_gl4.lua", "LuaRules/Gadgets/gfx_unit_shield_effects.lua", "LuaRules/Gadgets/gfx_lightning_cannon_gl4.lua", "LuaRules/Gadgets/gfx_environmental_lightning_gl4.lua"}) {
		if(lowerName==StringToLower(path)) {adapterPath=path;break;}
	}
	if(adapterPath==nullptr)return 0;
	lua_Debug caller{};
	if(!lua_getstack(L,1,&caller)||!lua_getinfo(L,"S",&caller)||!caller.source)return 0;
	std::string_view source(caller.source);if(source.starts_with('@'))source.remove_prefix(1);
	if(source!="LuaRules/gadgets.lua" || caller.linedefined!=604 || caller.lastlinedefined!=709)return 0;
	try {
		std::string original;
		if(LoadFileWithModes("LuaRules/gadgets.lua",original,SPRING_VFS_ZIP)!=1)throw std::runtime_error("Original LuaRules gadget handler is missing");
		sha512::raw_digest raw;sha512::hex_digest hex{};
		sha512::calc_digest({original.data(),original.data()+original.size()},raw);sha512::dump_digest(raw,hex);
		if(std::string_view(hex.data())!="fc85d9c197122104ed6b7c73539b45ae2b9b406056062eb060aa2461d2a3194402adf1a345c8ff24e9905277348064ec0e11d4c03adba74500b796f5a08c65e6")
			throw std::runtime_error("Native effect routing requires review: original LuaRules handler identity changed");
		std::string adapter;
		if(LoadFileWithModes(adapterPath,adapter,SPRING_VFS_RAW)!=1)
			throw std::runtime_error(std::string("Staged native effect adapter is missing: ")+adapterPath);
		if(adapter.empty()||adapter.size()>65536)throw std::runtime_error("Staged native effect adapter has invalid extent");
		LuaUtils::TracyRemoveAlsoExtras(adapter.data());lua_pushsstring(L,adapter);return 1;
	} catch(const std::exception& error) {std::snprintf(reason,capacity,"%s",error.what());}
	return -1;
}
}
#endif

int LuaVFS::LoadFile(lua_State* L, bool synced)
{
#ifdef RECOIL_METAL4_LIVE_SCENE
	char reason[512]{};
	const int nativeRoute=LoadNativeGadgetAdapter(L,synced,reason,sizeof(reason));
	if(nativeRoute<0)return luaL_error(L,"[NativeEffects VFS] %s",reason);
	if(nativeRoute>0)return 1;
#endif
	const string filename = luaL_checkstring(L, 1);
	// the path may point to a file or dir outside of any data-dir
	// if (!LuaIO::IsSimplePath(filename)) return 0;

	string data;
	const auto mode = GetModes(L, 2, synced);
	int loadCode = 0;
#ifdef RECOIL_METAL4_LIVE_SCENE
	loadCode = LoadNativeWidgetText(L, synced, filename, mode, false, data, reason, sizeof(reason));
	if (loadCode < 0)
		return luaL_error(L, "[NativeWidgets VFS] %s", reason);
#endif
	if (loadCode == 0)
		loadCode = LoadFileWithModes(filename, data, mode);
	if (loadCode == 1) {
#ifdef RECOIL_METAL4_LIVE_SCENE
		if (AdaptNativeLuaMenuDisplayText(L, synced, filename, data, reason, sizeof(reason)) < 0)
			return luaL_error(L, "[NativeLuaMenu VFS] %s", reason);
#endif
		LuaUtils::TracyRemoveAlsoExtras(data.data());
		lua_pushsstring(L, data);
		return 1;
	}
	return 0;
}


int LuaVFS::SyncLoadFile(lua_State* L)
{
	return LoadFile(L, true);
}


int LuaVFS::UnsyncLoadFile(lua_State* L)
{
	return LoadFile(L, false);
}


/******************************************************************************/

/***
 * Check if file exists in VFS.
 * 
 * @function VFS.FileExists
 * 
 * Example usage:
 * 
 * ```lua
 * if VFS.FileExists("mapconfig/custom_lava_config.lua", VFS.MAP) then
 *   # ...
 * end
 * ```
 * 
 * @param filename string
 * 
 * Path to file, lowercase only. Use linux style path separators, e.g.
 * `"foo/bar.txt"`.
 *
 * @param mode string?
 * 
 * VFS modes are single char strings and can be concatenated;
 * doing specifies an order of preference for the mode (i.e. location) from
 * which to include files.
 * 
 * @return boolean exists `true` if the file exists, otherwise `false`.
 */
int LuaVFS::FileExists(lua_State* L, bool synced)
{
	const std::string& filename = luaL_checkstring(L, 1);
	const std::string& vfsModes = GetModes(L, 2, synced);

	// FIXME: return 0, keep searches within the Spring directory
	// the path may point to a file or dir outside of any data-dir
	// if (!LuaIO::IsSimplePath(filename)) return 0;

	lua_pushboolean(L, CFileHandler::FileExists(filename, vfsModes));
	return 1;
}


int LuaVFS::  SyncFileExists(lua_State* L) { return FileExists(L,  true); }
int LuaVFS::UnsyncFileExists(lua_State* L) { return FileExists(L, false); }


/******************************************************************************/

/***
 * List files in a directory.
 * 
 * @function VFS.DirList
 * 
 * Example usage:
 * 
 * ```lua
 * local luaFiles = VFS.DirList('units/', '*.lua', nil, true)
 * ```
 * 
 * @param directory string
 * 
 * Path to directory, lowercase only. Use linux style path separators, e.g.
 * `"foo/bar/"`.
 *
 * @param pattern string? (Default: `"*"`)
 * 
 * @param mode string?
 * 
 * VFS modes are single char strings and can be concatenated;
 * doing specifies an order of preference for the mode (i.e. location) from
 * which to include files.
 * 
 * @param recursive boolean? (Default: `false`)
 * 
 * @return string[] filenames
 */
int LuaVFS::DirList(lua_State* L, bool synced)
{
	const std::string& dir = luaL_checkstring(L, 1);

	// FIXME: return 0, keep searches within the Spring directory
	// the path may point to a file or dir outside of any data-dir
	// if (!LuaIO::IsSimplePath(dir)) return 0;

	const std::string& pattern = luaL_optstring(L, 2, "*");
	const std::string& modes = GetModes(L, 3, synced);
	const bool recursive = luaL_optboolean(L, 4, false);

	LuaUtils::PushStringVector(L, CFileHandler::DirList(dir, pattern, modes, recursive));
	return 1;
}


int LuaVFS::SyncDirList(lua_State* L)
{
	return DirList(L, true);
}


int LuaVFS::UnsyncDirList(lua_State* L)
{
	return DirList(L, false);
}


/******************************************************************************/

/***
 * List sub-directories in a directory.
 * 
 * @function VFS.SubDirs
 * 
 * Example usage:
 * 
 * ```lua
 * local files = VFS.SubDirs('sounds/voice/' .. language, '*')
 * for _, file in ipairs(files) do
 * 	# ...
 * end
 * ```
 * 
 * @param directory string
 * 
 * Path to directory, lowercase only. Use linux style path separators, e.g.
 * `"foo/bar/"`.
 *
 * @param pattern string? (Default: `"*"`)
 * 
 * @param mode string?
 * 
 * VFS modes are single char strings and can be concatenated;
 * doing specifies an order of preference for the mode (i.e. location) from
 * which to include files.
 * 
 * @param recursive boolean? (Default: `false`)
 * 
 * @return string[] dirnames
 */
int LuaVFS::SubDirs(lua_State* L, bool synced)
{
	const std::string& dir = luaL_checkstring(L, 1);

	// FIXME: return 0, keep searches within the Spring directory
	// the path may point to a file or dir outside of any data-dir
	// if (!LuaIO::IsSimplePath(dir)) return 0;

	const std::string& pattern = luaL_optstring(L, 2, "*");
	const std::string& modes = GetModes(L, 3, synced);
	const bool recursive = luaL_optboolean(L, 4, false);

	LuaUtils::PushStringVector(L, CFileHandler::SubDirs(dir, pattern, modes, recursive));
	return 1;
}


int LuaVFS::SyncSubDirs(lua_State* L)
{
	return SubDirs(L, true);
}

int LuaVFS::UnsyncSubDirs(lua_State* L)
{
	return SubDirs(L, false);
}

/***
 * @function VFS.GetFileAbsolutePath
 *
 * @param filename string
 * 
 * Path to file, lowercase only. Use linux style path separators, e.g.
 * `"foo/bar.txt"`.
 *
 * @param mode string?
 * 
 * VFS modes are single char strings and can be concatenated;
 * doing specifies an order of preference for the mode (i.e. location) from
 * which to include files.
 * 
 * @return string? absolutePath
 */
int LuaVFS::GetFileAbsolutePath(lua_State* L)
{
	const std::string filename = luaL_checkstring(L, 1);

	// FIXME: return 0, keep searches within the Spring directory
	// the path may point to a file or dir outside of any data-dir
	// if (!LuaIO::IsSimplePath(filename)) return 0;

	if (!CFileHandler::FileExists(filename, GetModes(L, 2, false)))
		return 0;

	const std::string& absolutePath = CFileHandler::GetFileAbsolutePath(filename, GetModes(L, 2, false));

	if (absolutePath.empty())
		return 0;

	lua_pushsstring(L, absolutePath);
	return 1;
}

/******************************************************************************/
/******************************************************************************/

/***
 * @function VFS.GetArchiveContainingFile
 *
 * @param filename string
 * 
 * Path to file, lowercase only. Use linux style path separators, e.g.
 * `"foo/bar.txt"`.
 *
 * @param mode string?
 * 
 * VFS modes are single char strings and can be concatenated;
 * doing specifies an order of preference for the mode (i.e. location) from
 * which to include files.
 * 
 * @return string? archiveName
 */
int LuaVFS::GetArchiveContainingFile(lua_State* L)
{
	const std::string filename = luaL_checkstring(L, 1);

	// FIXME: return 0, keep searches within the Spring directory
	// the path may point to a file or dir outside of any data-dir
	// if (!LuaIO::IsSimplePath(filename)) return 0;

	if (!CFileHandler::FileExists(filename, GetModes(L, 2, false)))
		return 0;

	const std::string& archiveName = CFileHandler::GetArchiveContainingFile(filename, GetModes(L, 2, false));

	if (archiveName.empty())
		return 0;

	lua_pushsstring(L, archiveName);
	return 1;
}


/******************************************************************************/
/******************************************************************************/

/***
 * Temporarily load an archive from the VFS and run the given function,
 * which can make usage of the files in the archive.
 * 
 * @function VFS.UseArchive
 * @param archiveName string
 * @param fun(...) func
 * @return any ... Results of the given function
 */
int LuaVFS::UseArchive(lua_State* L)
{
	// only from unsynced
	if (CLuaHandle::GetHandleSynced(L))
		return 0;

	const std::string& archiveName = luaL_checkstring(L, 1);
	const CArchiveScanner::ArchiveData& archiveData = archiveScanner->GetArchiveData(archiveName);
	if (archiveData.IsEmpty())
		luaL_error(L, "[VFS::%s] archive \"%s\" not found", __func__, archiveName.c_str());

	constexpr int funcIndex = 2;
	if (!lua_isfunction(L, funcIndex))
		luaL_error(L, "[VFS::%s] second argument should be a function", __func__);

	if (vfsHandler->HasArchive(archiveName))
		luaL_error(L, "[VFS::%s] archive \"%s\" already loaded", __func__, archiveName.c_str());

	// block other threads from getting the global until we are done
	vfsHandler->GrabLock();
	vfsHandler->SetName("LuaVFS");
	vfsHandler->UnMapArchives(false);

	// could be mod,map,etc
	vfsHandler->AddArchive(archiveName, false);

	const int callError = lua_pcall(L, lua_gettop(L) - funcIndex, LUA_MULTRET, 0);

	vfsHandler->RemoveArchive(archiveName);
	vfsHandler->ReMapArchives(false);
	vfsHandler->SetName("SpringVFS");
	vfsHandler->FreeLock();

	if (callError != 0)
		lua_error(L);

	return (lua_gettop(L) - funcIndex + 1);
}

/** -- Not exported.
 * 
 * Permanently loads an archive into the VFS (to load zipped music collections
 * etc.).
 * 
 * Does nothing if the archive is already loaded in the VFS (won't reload even
 * if there are changes made to the archive). If checksum is given it checks if
 * the to be loaded file is correct, if not then it won't load it and return
 * false.
 * 
 * @function VFS.MapArchive
 * @param archiveName string
 * @param checksum string?
 * @return boolean
 */
int LuaVFS::MapArchive(lua_State* L)
{
	// only from unsynced
	if (CLuaHandle::GetHandleSynced(L))
		return 0;

	const int args = lua_gettop(L); // number of arguments

	const std::string& archiveName = luaL_checkstring(L, 1);
	const CArchiveScanner::ArchiveData& archiveData = archiveScanner->GetArchiveData(archiveName);
	if (archiveData.IsEmpty())
		luaL_error(L, "[VFS::%s] archive not found: %s", __func__, archiveName.c_str());

	if (args >= 2) {
		sha512::hex_digest argChecksum;
		sha512::hex_digest hexChecksum;

		std::fill(argChecksum.begin(), argChecksum.end(), 0);
		std::memcpy(argChecksum.data(), lua_tostring(L, 2), std::min(argChecksum.size() - 1, strlen(lua_tostring(L, 2))));
		sha512::dump_digest(archiveScanner->GetArchiveSingleChecksumBytes(archiveName), hexChecksum);

		if (argChecksum != hexChecksum)
			luaL_error(L, "[VFS::%s] incorrect checksum for archive: %s (got: %s, expected: %s)",
				__func__, archiveName.c_str(), argChecksum.data(), hexChecksum.data());
	}

	if (!vfsHandler->AddArchive(archiveName, false))
		luaL_error(L, "[VFS::%s] failed to load archive: %s", archiveName.c_str());

	lua_pushboolean(L, true);
	return 1;
}

/** -- Not exported.
 * 
 * Removes an already loaded archive (see `VFS.MapArchive`).
 * 
 * @function VFS.UnmapArchive
 * @param archiveName string
 * @return boolean
 */
int LuaVFS::UnmapArchive(lua_State* L)
{
	// only from unsynced
	if (CLuaHandle::GetHandleSynced(L))
		return 0;

	const std::string& archiveName = luaL_checkstring(L, 1);
	const CArchiveScanner::ArchiveData& archiveData = archiveScanner->GetArchiveData(archiveName);
	if (archiveData.IsEmpty())
		luaL_error(L, "[VFS::%s] archive not found: %s", __func__, archiveName.c_str());

	LOG("[LuaVFS::%s] archive=%s", __func__, archiveName.c_str());

	if (!vfsHandler->RemoveArchive(archiveName))
		luaL_error(L, "[VFS::%s] failed to remove archive: %s", __func__, archiveName.c_str());

	lua_pushboolean(L, true);
	return 1;
}



/******************************************************************************/

/***
 * Compresses the specified folder.
 * @function VFS.CompressFolder
 * @param folderPath string
 * @param archiveType string? (Default: `"zip"`)The compression type (can
 * currently be only `"zip"`).
 * @param compressedFilePath string? (Default: `folderPath .. ".sdz"`)
 * @param includeFolder boolean? (Default: `false`) Whether the archive should
 * have the specified folder as root.
 * @param mode string?
 */
int LuaVFS::CompressFolder(lua_State* L)
{
	const std::string& folderPath = luaL_checkstring(L, 1);
	const std::string& archiveType = luaL_optstring(L, 2, "zip");

	if (archiveType != "zip" && archiveType != "7z") //TODO: add 7z support
		luaL_error(L, ("Unsupported archive type " + archiveType).c_str());

	 // "sdz" is the default type if not specified
	const std::string& compressedFilePath = luaL_optstring(L, 3, (folderPath + ".sdz").c_str());
	const std::string& modes = GetModes(L, 5, false);

	const bool includeFolder = luaL_optboolean(L, 4, false);

	if (CFileHandler::FileExists(compressedFilePath, modes))
		luaL_error(L, ("File already exists " + compressedFilePath).c_str());

	if (archiveType == "zip") {
		LuaZipFolder::ZipFolder(L, folderPath, compressedFilePath, includeFolder, modes);
	} else if (archiveType == "7z") {
		SevenZipFolder(L, folderPath, compressedFilePath, includeFolder, modes);
	}

	return 0;
}

/******************************************************************************/

int LuaVFS::SevenZipFolder(lua_State* L, const string& folderPath, const string& zipFilePath, bool includeFolder, const string& modes)
{
	luaL_error(L, "7z-compression is not implemented yet.");
	return 0;
}


/***
 * @function VFS.ZlibCompress
 * @param uncompressed string Data to compress.
 * @return string? compressed Compressed data, or `nil` on error.
 */
int LuaVFS::ZlibCompress(lua_State* L)
{
	size_t inSize = 0;
	const std::uint8_t* inData = reinterpret_cast<const std::uint8_t*>(luaL_checklstring(L, 1, &inSize));

	const std::vector<std::uint8_t> compressed = zlib::deflate(inData, inSize);

	if (!compressed.empty()) {
		lua_pushlstring(L, reinterpret_cast<const char*>(compressed.data()), compressed.size());
		return 1;
	}

	return luaL_error(L, "Error while compressing");
}

/***
 * @function VFS.ZlibDecompress
 * @param compressed string Data to decompress.
 * @return string? uncompressed Uncompressed data, or `nil` on error.
 */
int LuaVFS::ZlibDecompress(lua_State* L)
{
	size_t inSize = 0;
	const std::uint8_t* inData = reinterpret_cast<const std::uint8_t*>(luaL_checklstring(L, 1, &inSize));

	const std::vector<std::uint8_t> uncompressed = zlib::inflate(inData, inSize);

	if (!uncompressed.empty()) {
		lua_pushlstring(L, reinterpret_cast<const char*>(uncompressed.data()), uncompressed.size());
		return 1;
	}

	return luaL_error(L, "Error while decompressing");
}


/***
 * @alias HashType
 * | 0 # MD5
 * | 1 # SHA512
 */

/***
 * Calculates hash of a given string.
 *
 * - MD5 gets base64 encoded.
 * - SHA512 gets hex encoded.
 * 
 * @function VFS.CalculateHash
 * @param input string
 * @param hashType HashType Hash type.
 * @return string? hash
 */
int LuaVFS::CalculateHash(lua_State* L)
{
	size_t slen = 0;

	const char* sstr = luaL_checklstring(L, 1, &slen);
	const char* hash = "";

	enum {
		HASHTYPE_MD5 = 0,
		HASHTYPE_SHA = 1,
	};

	switch (luaL_checkint(L, 2)) {
		case HASHTYPE_MD5: {
			// base64(MD5); pr-downloader only accepts type=0
			lua_pushstring(L, hash = CalcHash(sstr, slen, HASHTYPE_MD5));
			free((char*) hash);  // CalcHash in pr-downloader uses system malloc, cannot use recoil::free((char*) hash);
		} break;
		case HASHTYPE_SHA: {
			sha512::hex_digest hexHash;
			sha512::raw_digest rawHash;

			hexHash.fill(0);
			rawHash.fill(0);

			sha512::calc_digest({sstr, sstr + slen}, rawHash);
			sha512::dump_digest(rawHash, hexHash);

			lua_pushstring(L, hexHash.data());
		} break;
		default: {
			luaL_error(L, "[VFS::%s] unsupported hash type", __func__);
		} break;
	}

	return 1;
}

/***
 * Verifies an Ed25519 signature.
 *
 * All arguments are binary strings. The signature must be 64 bytes and the
 * public key must be 32 bytes.
 *
 * @function VFS.VerifyEd25519
 * @param message string
 * @param signature string
 * @param publicKey string
 * @return boolean valid
 */
int LuaVFS::VerifyEd25519(lua_State* L)
{
	static constexpr size_t ED25519_SIGNATURE_SIZE = 64;
	static constexpr size_t ED25519_PUBLIC_KEY_SIZE = 32;

	size_t messageSize = 0;
	size_t signatureSize = 0;
	size_t publicKeySize = 0;
	const auto* message = reinterpret_cast<const std::uint8_t*>(luaL_checklstring(L, 1, &messageSize));
	const auto* signature = reinterpret_cast<const std::uint8_t*>(luaL_checklstring(L, 2, &signatureSize));
	const auto* publicKey = reinterpret_cast<const std::uint8_t*>(luaL_checklstring(L, 3, &publicKeySize));

	if (signatureSize != ED25519_SIGNATURE_SIZE || publicKeySize != ED25519_PUBLIC_KEY_SIZE) {
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, crypto_ed25519_check(signature, publicKey, message, messageSize) == 0);
	return 1;
}

/******************************************************************************/
/******************************************************************************/
//
//  NOTE: Endianess should be handled
//

template <typename T>
int PackType(lua_State* L)
{
	std::vector<T> vals;
	std::vector<char> buf;

	if (lua_istable(L, 1)) {
		vals.reserve(lua_objlen(L, 1));

		for (int i = 1; lua_rawgeti(L, 1, i), lua_isnumber(L, -1); lua_pop(L, 1), i++) {
			vals.push_back(static_cast<T>(lua_tonumber(L, -1)));
		}
	} else {
		vals.resize(lua_gettop(L));

		for (size_t i = 0; i < vals.size(); i++) {
			if (!lua_isnumber(L, i + 1))
				break;

			vals[i] = static_cast<T>(lua_tonumber(L, i + 1));
		}
	}

	if (vals.empty())
		return 0;

	buf.resize(sizeof(T) * vals.size(), 0);

	for (size_t i = 0; i < vals.size(); i++) {
		memcpy(buf.data() + (i * sizeof(T)), &vals[i], sizeof(T));
	}

	lua_pushlstring(L, buf.data(), buf.size());
	return 1;
}


/***
 * Convert unsigned 8-bit integer(s) to binary string.
 * @function VFS.PackU8 
 * @param ... integer Numbers to pack.
 * @return string
 */
/***
 * Convert unsigned 8-bit integer(s) to binary string.
 * @function VFS.PackU8 
 * @param numbers integer[] Numbers to pack.
 * @return string
 */
int LuaVFS::PackU8(lua_State* L) { return PackType<std::uint8_t >(L); }

/***
 * Convert unsigned 16-bit integer(s) to binary string.
 * @function VFS.PackU16 
 * @param ... integer Numbers to pack.
 * @return string
 */
/***
 * Convert unsigned 16-bit integer(s) to binary string.
 * @function VFS.PackU16 
 * @param numbers integer[] Numbers to pack.
 * @return string
 */
int LuaVFS::PackU16(lua_State* L) { return PackType<std::uint16_t>(L); }

/***
 * Convert unsigned 32-bit integer(s) to binary string.
 * @function VFS.PackU32 
 * @param ... integer Numbers to pack.
 * @return string
 */
/***
 * Convert unsigned 32-bit integer(s) to binary string.
 * @function VFS.PackU32 
 * @param numbers integer[] Numbers to pack.
 * @return string
 */
int LuaVFS::PackU32(lua_State* L) { return PackType<std::uint32_t>(L); }

/***
 * Convert signed 8-bit integer(s) to binary string.
 * @function VFS.PackS8 
 * @param ... integer Numbers to pack.
 * @return string
 */
/***
 * Convert signed 8-bit integer(s) to binary string.
 * @function VFS.PackS8 
 * @param numbers integer[] Numbers to pack.
 * @return string
 */
int LuaVFS::PackS8(lua_State* L) { return PackType<std::int8_t>(L); }

/***
 * Convert signed 16-bit integer(s) to binary string.
 * @function VFS.PackS16 
 * @param ... integer Numbers to pack.
 * @return string
 */
/***
 * Convert signed 16-bit integer(s) to binary string.
 * @function VFS.PackS16 
 * @param numbers integer[] Numbers to pack.
 * @return string
 */
int LuaVFS::PackS16(lua_State* L) { return PackType<std::int16_t>(L); }

/***
 * Convert signed 32-bit integer(s) to binary string.
 * @function VFS.PackS32 
 * @param ... integer Numbers to pack.
 * @return string
 */
/***
 * Convert signed 32-bit integer(s) to binary string.
 * @function VFS.PackS32 
 * @param numbers integer[] Numbers to pack.
 * @return string
 */
int LuaVFS::PackS32(lua_State* L) { return PackType<std::int32_t>(L); }

/***
 * Convert signed 32-bit float(s) to binary string.
 * @function VFS.PackF32
 * @param ... number Numbers to pack.
 * @return string?
 */
/***
 * Convert signed 32-bit float(s) to binary string.
 * @function VFS.PackF32
 * @param numbers number[] Numbers to pack.
 * @return string?
 */
int LuaVFS::PackF32(lua_State* L) { return PackType<float>(L); }


/******************************************************************************/

template <typename T>
int UnpackType(lua_State* L)
{
	if (!lua_isstring(L, 1))
		return 0;

	size_t len;
	const char* str = lua_tolstring(L, 1, &len);

	if (lua_isnumber(L, 2)) {
		const int pos = lua_toint(L, 2);
		if ((pos < 1) || ((size_t)pos >= len))
			return 0;

		const int offset = (pos - 1);
		str += offset;
		len -= offset;
	}

	const size_t eSize = sizeof(T);
	if (len < eSize)
		return 0;

	if (!lua_isnumber(L, 3)) {
		lua_pushnumber(L, *(reinterpret_cast<const T*>(str)));
		return 1;
	}

	const size_t maxCount = len / eSize;
	int tableCount = lua_toint(L, 3);
	if (tableCount < 0)
		tableCount = maxCount;

	lua_createtable(L, tableCount = std::min((int)maxCount, tableCount), 0);
	for (int i = 0; i < tableCount; i++) {
		lua_pushnumber(L, *(reinterpret_cast<const T*>(str) + i));
		lua_rawseti(L, -2, (i + 1));
	}
	return 1;
}


/***
 * Convert a binary string to an unsigned 8-bit integer.
 * @function VFS.UnpackU8
 * @param str string Binary string.
 * @param pos integer? Byte offset.
 * @return integer
 */
int LuaVFS::UnpackU8(lua_State* L) { return UnpackType<std::uint8_t>(L); }

/***
 * Convert a binary string to an unsigned 16-bit integer.
 * @function VFS.UnpackU16
 * @param str string Binary string.
 * @param pos integer? Byte offset.
 * @return integer
 */
int LuaVFS::UnpackU16(lua_State* L) { return UnpackType<std::uint16_t>(L); }

/***
 * Convert a binary string to an unsigned 32-bit integer.
 * @function VFS.UnpackU32
 * @param str string Binary string.
 * @param pos integer? Byte offset.
 * @return integer
 */
int LuaVFS::UnpackU32(lua_State* L) { return UnpackType<std::uint32_t>(L); }

/***
 * Convert a binary string to a signed 8-bit integer.
 * @function VFS.UnpackS8
 * @param str string Binary string.
 * @param pos integer? Byte offset.
 * @return integer
 */
int LuaVFS::UnpackS8(lua_State* L) { return UnpackType<std::int8_t>(L); }

/***
 * Convert a binary string to a signed 16-bit integer.
 * @function VFS.UnpackS16
 * @param str string Binary string.
 * @param pos integer? Byte offset.
 * @return integer
 */
int LuaVFS::UnpackS16(lua_State* L) { return UnpackType<std::int16_t>(L); }

/***
 * Convert a binary string to a signed 32-bit integer.
 * @function VFS.UnpackS32
 * @param str string Binary string.
 * @param pos integer? Byte offset.
 * @return integer
 */
int LuaVFS::UnpackS32(lua_State* L) { return UnpackType<std::int32_t>(L); }

/***
 * Convert a binary string to a signed 32-bit float.
 * @function VFS.UnpackF32
 * @param str string Binary string.
 * @param pos integer? Byte offset.
 * @return integer
 */
int LuaVFS::UnpackF32(lua_State* L) { return UnpackType<float>(L); }


/******************************************************************************/
/******************************************************************************/
