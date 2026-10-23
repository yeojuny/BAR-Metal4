/*
 * SetupScript.cpp
 *
 *  Created on: Jan 18, 2025
 *      Author: rlcevg
 */

#include "script/SetupScript.h"
#include "script/ScriptManager.h"
#include "setup/SetupManager.h"

#include "angelscript/add_on/scriptdictionary/scriptdictionary.h"

namespace circuit {

CSetupScript::CSetupScript(CScriptManager* scr, CSetupManager* mgr)
		: IScript(scr)
		, manager(mgr)
		, moDict(nullptr)
{
	asIScriptEngine* engine = script->GetEngine();

	int r = engine->RegisterObjectType("CSetupManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CSetupManager aiSetupMgr", manager); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CSetupManager", "void SetWaterHarmful(bool)", asMETHOD(CSetupManager, SetWaterHarmful), asCALL_THISCALL); ASSERT(r >= 0);
	// AS docs / "Registering object methods" / "Composite members"
	r = engine->RegisterObjectMethod("CSetupManager", "dictionary@+ GetModOptions()", asMETHOD(CSetupScript, GetModOptions), asCALL_THISCALL, 0, asOFFSET(CSetupManager, script), true); ASSERT(r >= 0);

	// NOTE: Methods with references to types registered in CInitScript.RegisterCore()
	//       are registered in CInitScript.RegisterMgr()
}

CSetupScript::~CSetupScript()
{
	Release();
}

void CSetupScript::Release()
{
	if (moDict != nullptr) {
		moDict->Release();
		moDict = nullptr;
	}
}

CScriptDictionary* CSetupScript::GetModOptions()
{
	/*
	 * dictionary@ mo = aiSetupMgr.GetModOptions();
	 * if (mo.exists("chicken_queendifficulty"))
	 *     AiLog(string(mo["chicken_queendifficulty"]));
	 */
	if (moDict != nullptr) {
		return moDict;
	}
	moDict = CScriptDictionary::Create(script->GetEngine());
	int typeId = script->GetEngine()->GetTypeIdByDecl("string");
	const CSetupData::ModOptions& modoptions = manager->GetModOptions();
	for (const auto& kv : modoptions) {
		moDict->Set(kv.first, (void*)&kv.second, typeId);
	}
	// Lazy-getter. Starting from ref=1.
	// Return-auto-handle "@+" adds ref, while
	// dict->Release() is in script's scope.
	return moDict;
}

} /* namespace circuit */
