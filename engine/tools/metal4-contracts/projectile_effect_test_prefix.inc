/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>
#include "lib/sol2/sol.hpp"
#include "Rendering/GL/myGL.h"
#include "Rendering/Backend/NativeProjectileEffectRecorder.h"
#include "Rendering/Backend/NativeProjectileEffectPrograms.h"

#include <fmt/format.h>
#include <fmt/printf.h>
namespace recoil {void* aligned_alloc(size_t alignment,size_t size){void* ptr=nullptr;if(posix_memalign(&ptr,alignment,size))return nullptr;return ptr;}void aligned_free(void* ptr){free(ptr);}}
namespace spring {template<class T>void SafeDelete(T*& ptr){delete ptr;ptr=nullptr;}}
struct LuaUtils {static void TracyRemoveAlsoExtras(char*);template<class... A>static void SolLuaError(const char* message,A&&... args){throw std::runtime_error(fmt::sprintf(message,std::forward<A>(args)...));}};
template<class T,class U>auto AlignUp(T value,U alignment){return (value+alignment-1)/alignment*alignment;}
class VBO {public:explicit VBO(GLenum,bool){}size_t GetSize()const{return bytes.size();}GLuint GetIdRaw()const{return 0;}void Bind(){}void Unbind(){}void New(size_t count,GLenum){bytes.assign(count,0);}void SetBufferSubData(size_t offset,size_t count,const void* data){if(offset>bytes.size()||count>bytes.size()-offset)throw std::runtime_error("CPU GL transport overrun");memcpy(bytes.data()+offset,data,count);}std::vector<uint8_t> bytes;};
class LuaVBOImpl;
struct FixtureVBOs {std::vector<std::weak_ptr<LuaVBOImpl>> luaVBOs;};
struct CLuaHandle {static bool GetHandleSynced(lua_State*){return false;}static FixtureVBOs& GetActiveVBOs(lua_State*) {static FixtureVBOs values;return values;}};
namespace {void VBOExistenceCheck(const VBO* vbo,const char* name){if(!vbo)LuaUtils::SolLuaError("Undefined buffer %s",name);}}
#include "Lua/LuaMemPool.h"
size_t LuaMemPool::GetPoolCount(){return 0;}
void LuaMemPool::Free(void*,size_t){throw std::runtime_error("fixture has no engine mempool");}
void* LuaMemPool::Realloc(void*,size_t,size_t){throw std::runtime_error("fixture has no engine mempool");}

#undef LOG_L
#define LOG_L(...) ((void)0)

#include "tracy/TracyLua.hpp"
#include "System/StringUtil.h"
#include "System/Sync/SHA512.hpp"
std::string fixtureWork;
struct CFileHandler {std::string name;CFileHandler(const std::string& path,const char*):name(path){}bool FileExists()const{return true;}bool LoadStringData(std::string& data){std::string low=StringToLower(name);std::ifstream file(fixtureWork+"/exact/"+low);if(!file)return false;std::ostringstream out;out<<file.rdbuf();data=out.str();return true;}};
#define SPRING_VFS_ZIP "Mmeb"
#define SPRING_VFS_RAW "r"


struct CBitmap {
 bool compressed=false;unsigned channels=4;int xsize=0,ysize=0;std::vector<uint8_t> bytes;
 unsigned GetDataTypeSize()const{return 1;}const uint8_t* GetRawMem()const{return bytes.data();}size_t GetMemSize()const{return bytes.size();}
 bool Load(const std::string& name,float alpha,unsigned count,unsigned type){if(alpha!=1||count!=4||type!=0)throw std::runtime_error("Original named texture decode arguments differ");const std::map<std::string,int> sizes{{"largebeam",64},{"flare2",512},{"plasmaball",256},{"glow2",256}};auto base=name.substr(name.find_last_of('/')+1);base.resize(base.size()-4);xsize=ysize=sizes.at(base);std::ifstream f(fixtureWork+"/exact/bitmaps/projectiletextures/"+base+".rgba",std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});return bool(f);}
};
void lua_pushsstring(lua_State* L,const std::string& value){lua_pushlstring(L,value.data(),value.size());}
namespace {int checks=0;void CheckTest(bool b,const char* name){++checks;if(!b)throw std::runtime_error(name);}std::string Read(const std::string& p){std::ifstream f(p,std::ios::binary);std::ostringstream o;o<<f.rdbuf();return o.str();}void Run(lua_State* L,const std::string& source,const std::string& label){if(luaL_loadbuffer(L,source.data(),source.size(),label.c_str())||lua_pcall(L,0,0,0)){const char* raw=lua_tostring(L,-1);auto msg=std::string(raw?raw:lua_typename(L,lua_type(L,-1)));lua_pop(L,1);throw std::runtime_error(label+": "+msg);}}}
#include "System/Sync/SHA512.hpp"
std::string fixtureAdapter;bool fixtureMissing=false,fixtureStale=false;std::vector<std::pair<std::string,std::string>> fixtureLoads;
using std::string;
int LoadFileWithModes(const std::string& name,std::string& data,const std::string& modes){fixtureLoads.emplace_back(name,modes);if(modes=="r"){if(fixtureMissing)return 0;std::ifstream f(fixtureAdapter+"/"+name.substr(name.find_last_of("/")+1));if(!f)return 0;std::ostringstream out;out<<f.rdbuf();data=out.str();return 1;}CFileHandler file(name,modes.c_str());if(!file.LoadStringData(data))return 0;if(fixtureStale&&StringToLower(name)=="luarules/gadgets.lua")data+="-- stale";return 1;}
struct LuaVFS {static int LoadFile(lua_State*,bool);static std::string GetModes(lua_State* L,int n,bool synced){const char* p=lua_tostring(L,n);return p?p:(synced?"Mmeb":"rMmeb");}};

