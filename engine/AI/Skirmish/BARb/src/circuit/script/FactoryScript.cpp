/*
 * FactoryScript.cpp
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#include "script/FactoryScript.h"
#include "script/ScriptManager.h"
#include "module/FactoryManager.h"
#include "util/ExtAS.h"

#include "angelscript/include/angelscript.h"
#include "angelscript/add_on/scriptarray/scriptarray.h"

namespace circuit {

using namespace springai;

CFactoryScript::CFactoryScript(CScriptManager* scr, CFactoryManager* mgr)
		: ITaskModuleScript(scr, mgr)
{
	asIScriptEngine* engine = script->GetEngine();

	int r = engine->RegisterObjectType("SRecruitTask", sizeof(TaskS::SRecruitTask), asOBJ_VALUE | asOBJ_POD); ASSERT(r >= 0);
	static_assert(sizeof(TaskS::SRecruitTask::type) == sizeof(char), "CRecruitTask::RecruitType is not uint8!");
	r = engine->RegisterObjectProperty("SRecruitTask", "uint8 type", asOFFSET(TaskS::SRecruitTask, type)); ASSERT(r >= 0);  // Task::RecruitType
	static_assert(sizeof(TaskS::SRecruitTask::priority) == sizeof(char), "IBuilderTask::Priority is not uint8!");
	r = engine->RegisterObjectProperty("SRecruitTask", "uint8 priority", asOFFSET(TaskS::SRecruitTask, priority)); ASSERT(r >= 0);  // Task::Priority
	r = engine->RegisterObjectProperty("SRecruitTask", "CCircuitDef@ buildDef", asOFFSET(TaskS::SRecruitTask, buildDef)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SRecruitTask", "AIFloat3 position", asOFFSET(TaskS::SRecruitTask, position)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SRecruitTask", "float radius", asOFFSET(TaskS::SRecruitTask, radius)); ASSERT(r >= 0);

	r = engine->RegisterObjectType("SServSTask", sizeof(TaskS::SServSTask), asOBJ_VALUE | asOBJ_POD); ASSERT(r >= 0);
	static_assert(sizeof(TaskS::SServSTask::type) == sizeof(char), "IBuilderTask::BuildType is not uint8!");
	r = engine->RegisterObjectProperty("SServSTask", "uint8 type", asOFFSET(TaskS::SServSTask, type)); ASSERT(r >= 0);  // Task::RecruitType
	static_assert(sizeof(TaskS::SServSTask::priority) == sizeof(char), "IBuilderTask::Priority is not uint8!");
	r = engine->RegisterObjectProperty("SServSTask", "uint8 priority", asOFFSET(TaskS::SServSTask, priority)); ASSERT(r >= 0);  // Task::Priority
	r = engine->RegisterObjectProperty("SServSTask", "AIFloat3 position", asOFFSET(TaskS::SServSTask, position)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SServSTask", "CCircuitUnit@ target", asOFFSET(TaskS::SServSTask, target)); ASSERT(r >= 0);  // FIXME: CAllyUnit*
	r = engine->RegisterObjectProperty("SServSTask", "float radius", asOFFSET(TaskS::SServSTask, radius)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SServSTask", "bool stop", asOFFSET(TaskS::SServSTask, stop)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SServSTask", "int timeout", asOFFSET(TaskS::SServSTask, timeout)); ASSERT(r >= 0);

	r = engine->RegisterObjectType("CFactoryManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CFactoryManager aiFactoryMgr", manager); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "CCircuitDef@ DefaultGetFactoryToBuild(const AIFloat3& in, bool, bool)", asMETHOD(CFactoryManager, DefaultGetFactoryToBuild), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "IUnitTask@+ DefaultMakeTask(CCircuitUnit@)", asMETHOD(CFactoryManager, DefaultMakeTask), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "IUnitTask@+ Enqueue(const SRecruitTask& in)", asMETHODPR(CFactoryManager, Enqueue, (const TaskS::SRecruitTask&), CRecruitTask*), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "IUnitTask@+ Enqueue(const SServSTask& in)", asMETHODPR(CFactoryManager, Enqueue, (const TaskS::SServSTask&), IUnitTask*), asCALL_THISCALL); ASSERT(r >= 0);
//	r = engine->RegisterObjectMethod("CFactoryManager", "IUnitTask@+ EnqueueRetreat()", asMETHOD(CFactoryManager, EnqueueRetreat), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "CCircuitDef@ GetRoleDef(const CCircuitDef@, Type) const", asMETHOD(CFactoryManager, GetRoleDef), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "int GetFactoryCount() const", asMETHOD(CFactoryManager, GetFactoryCount), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CFactoryManager", "bool isAssistRequired", asOFFSET(CFactoryManager, isAssistRequired)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CFactoryManager", "float buildpowerRatio", asOFFSET(CFactoryManager, bpRatio)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CFactoryManager", "float responseWeight", asOFFSET(CFactoryManager, reWeight)); ASSERT(r >= 0);

	// AS docs / "Registering object methods" / "Composite members"
	r = engine->RegisterObjectMethod("CFactoryManager", "void SetTierWeights(const CCircuitDef@, int, int, array<float>@+)", asMETHOD(CFactoryScript, SetTierWeights), asCALL_THISCALL, 0, asOFFSET(CFactoryManager, script), true); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "array<float>@ GetTierWeights(const CCircuitDef@, int, int) const", asMETHOD(CFactoryScript, GetTierWeights), asCALL_THISCALL, 0, asOFFSET(CFactoryManager, script), true); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "void SetImportance(const CCircuitDef@, array<float>@+)", asMETHOD(CFactoryScript, SetImportance), asCALL_THISCALL, 0, asOFFSET(CFactoryManager, script), true); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "array<float>@ GetImportance(const CCircuitDef@) const", asMETHOD(CFactoryScript, GetImportance), asCALL_THISCALL, 0, asOFFSET(CFactoryManager, script), true); ASSERT(r >= 0);
}

CFactoryScript::~CFactoryScript()
{
}

bool CFactoryScript::Init()
{
	asIScriptModule* mod = script->GetEngine()->GetModule(CScriptManager::mainName.c_str());
	int r = mod->SetDefaultNamespace("Factory"); ASSERT(r >= 0);
	InitModule(mod);
	factoryInfo.isSwitchTime = script->GetFunc(mod, "bool AiIsSwitchTime(int)");
	factoryInfo.isSwitchAllowed = script->GetFunc(mod, "bool AiIsSwitchAllowed(CCircuitDef@)");
	factoryInfo.getFactoryToBuild = script->GetFunc(mod, "CCircuitDef@ AiGetFactoryToBuild(const AIFloat3& in, bool, bool)");
	return true;
}

bool CFactoryScript::IsSwitchTime(int lastSwitchFrame)
{
	if (factoryInfo.isSwitchTime == nullptr) {
		return false;
	}
	asIScriptContext* ctx = script->PrepareContext(factoryInfo.isSwitchTime);
	ctx->SetArgDWord(0, lastSwitchFrame);
	const bool result = script->Exec(ctx) ? ctx->GetReturnByte() : false;
	script->ReturnContext(ctx);
	return result;
}

bool CFactoryScript::IsSwitchAllowed(CCircuitDef* facDef)
{
	if (factoryInfo.isSwitchAllowed == nullptr) {
		return true;
	}
	asIScriptContext* ctx = script->PrepareContext(factoryInfo.isSwitchAllowed);
	ctx->SetArgObject(0, facDef);
	const bool result = script->Exec(ctx) ? ctx->GetReturnByte() : false;
	script->ReturnContext(ctx);
	return result;
}

CCircuitDef* CFactoryScript::GetFactoryToBuild(const AIFloat3& pos, bool isStart, bool isReset)
{
	if (factoryInfo.getFactoryToBuild == nullptr) {
		return static_cast<CFactoryManager*>(manager)->DefaultGetFactoryToBuild(pos, isStart, isReset);
	}
	asIScriptContext* ctx = script->PrepareContext(factoryInfo.getFactoryToBuild);
	ctx->SetArgAddress(0, &const_cast<AIFloat3&>(pos));
	ctx->SetArgByte(1, isStart);
	ctx->SetArgByte(2, isReset);
	CCircuitDef* result = script->Exec(ctx) ? (CCircuitDef*)ctx->GetReturnObject() : nullptr;
	script->ReturnContext(ctx);
	return result;
}

void CFactoryScript::SetTierWeights(CCircuitDef* facDef, int surfType, int tier, const CScriptArray* array)
{
	CFactoryManager* factoryMgr = static_cast<CFactoryManager*>(manager);
	auto weights = const_cast<std::vector<float>*>(factoryMgr->GetTierWeights(facDef, surfType, tier));

	if (weights->size() != array->GetSize()) {
		return;
	}
	for (asUINT i = 0; i < array->GetSize(); ++i) {
		(*weights)[i] = *static_cast<const float*>(array->At(i));
	}
}

CScriptArray* CFactoryScript::GetTierWeights(CCircuitDef* facDef, int surfType, int tier) const
{
	asIScriptEngine* engine = asGetActiveContext()->GetEngine();
	auto cache = static_cast<CScriptManager::STypeInfoCache*>(engine->GetUserData());

	CFactoryManager* factoryMgr = static_cast<CFactoryManager*>(manager);
	const std::vector<float>* weights = factoryMgr->GetTierWeights(facDef, surfType, tier);

	if (weights == nullptr) {
		return nullptr;
	}

	CScriptArray* arr = CScriptArray::Create(cache->floatArray, weights->size());
	asUINT i = 0;
	for (float weight : *weights) {
		arr->SetValue(i++, &weight);
	}
	return arr;
}

void CFactoryScript::SetImportance(CCircuitDef* facDef, const CScriptArray* array)
{
	CFactoryManager* factoryMgr = static_cast<CFactoryManager*>(manager);
	auto sfac = const_cast<CFactoryManager::SFactoryDef*>(factoryMgr->GetFactoryDef(facDef));
	if ((sfac == nullptr) || (array->GetSize() < 2)) {
		return;
	}
	sfac->startImp = *static_cast<const float*>(array->At(0));
	sfac->switchImp = *static_cast<const float*>(array->At(1));
}

CScriptArray* CFactoryScript::GetImportance(CCircuitDef* facDef) const
{
	asIScriptEngine* engine = asGetActiveContext()->GetEngine();
	auto cache = static_cast<CScriptManager::STypeInfoCache*>(engine->GetUserData());

	CFactoryManager* factoryMgr = static_cast<CFactoryManager*>(manager);
	auto sfac = const_cast<CFactoryManager::SFactoryDef*>(factoryMgr->GetFactoryDef(facDef));
	if (sfac == nullptr) {
		return nullptr;
	}

	CScriptArray* arr = CScriptArray::Create(cache->floatArray, 2);
	arr->SetValue(0, &sfac->startImp);
	arr->SetValue(1, &sfac->switchImp);
	return arr;
}

} // namespace circuit
