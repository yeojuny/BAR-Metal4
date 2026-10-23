/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
struct lua_State;class CUnit;struct LocalModelPiece;
namespace LuaNativeUnitGlass {
 bool PushEntries(lua_State*);void ReleaseLuaState(lua_State*);
 bool DrawPiece(const CUnit&,const LocalModelPiece&);
}
