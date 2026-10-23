/*
 * MilitaryScript.cpp
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#include "script/MilitaryScript.h"
#include "script/ScriptManager.h"
#include "module/MilitaryManager.h"
#include "util/ExtAS.h"
#include "angelscript/include/angelscript.h"

namespace circuit {

using namespace springai;

CMilitaryScript::CMilitaryScript(CScriptManager* scr, CMilitaryManager* mgr)
		: ITaskModuleScript(scr, mgr)
{
	asIScriptEngine* engine = script->GetEngine();

	int r = engine->RegisterObjectType("SFightTask", sizeof(TaskF::SFightTask), asOBJ_VALUE | asOBJ_POD); ASSERT(r >= 0);
	static_assert(sizeof(TaskF::SFightTask::type) == sizeof(char), "IFighterTask::FightType is not uint8!");
	r = engine->RegisterObjectProperty("SFightTask", "uint8 type", asOFFSET(TaskF::SFightTask, type)); ASSERT(r >= 0);  // Task::FightType
	static_assert(sizeof(TaskF::SFightTask::check) == sizeof(char), "IFighterTask::FightType is not uint8!");
	r = engine->RegisterObjectProperty("SFightTask", "uint8 check", asOFFSET(TaskF::SFightTask, check)); ASSERT(r >= 0);  // Task::FightType
	static_assert(sizeof(TaskF::SFightTask::promote) == sizeof(char), "IFighterTask::FightType is not uint8!");
	r = engine->RegisterObjectProperty("SFightTask", "uint8 promote", asOFFSET(TaskF::SFightTask, promote)); ASSERT(r >= 0);  // Task::FightType
	r = engine->RegisterObjectProperty("SFightTask", "float power", asOFFSET(TaskF::SFightTask, power)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SFightTask", "CCircuitUnit@ vip", asOFFSET(TaskF::SFightTask, vip)); ASSERT(r >= 0);

	r = engine->RegisterObjectType("CMilitaryManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CMilitaryManager aiMilitaryMgr", manager); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CMilitaryManager", "IUnitTask@+ DefaultMakeTask(CCircuitUnit@)", asMETHOD(CMilitaryManager, DefaultMakeTask), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CMilitaryManager", "IUnitTask@+ Enqueue(const SFightTask& in)", asMETHODPR(CMilitaryManager, Enqueue, (const TaskF::SFightTask&), IFighterTask*), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CMilitaryManager", "IUnitTask@+ EnqueueRetreat()", asMETHOD(CMilitaryManager, EnqueueRetreat), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CMilitaryManager", "void DefaultMakeDefence(int, const AIFloat3& in)", asMETHOD(CMilitaryManager, DefaultMakeDefence), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CMilitaryManager", "uint GetGuardTaskNum() const", asMETHOD(CMilitaryManager, GetGuardTaskNum), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CMilitaryManager", "const float armyCost", asOFFSET(CMilitaryManager, armyCost)); ASSERT(r >= 0);

	// NOTE: Config's "quota" scattered across CMilitaryManager, CEnemyManager, CThreatMap, CFactoryManager, CSetupManager
	r = engine->RegisterObjectType("SQuotaMilitary", 0, asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CMilitaryManager", "SQuotaMilitary quota", 0); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SQuotaMilitary", "uint scout", asOFFSET(CMilitaryManager, maxScouts)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SQuotaMilitary", "float attack", asOFFSET(CMilitaryManager, minAttackers)); ASSERT(r >= 0);
	r = engine->RegisterObjectType("SRaidQuota", 0, asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SQuotaMilitary", "SRaidQuota raid", 0); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SRaidQuota", "float min", asOFFSET(CMilitaryManager::SRaidQuota, min), asOFFSET(CMilitaryManager, raid), false); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SRaidQuota", "float avg", asOFFSET(CMilitaryManager::SRaidQuota, avg), asOFFSET(CMilitaryManager, raid), false); ASSERT(r >= 0);
	// Alternative "quota" accessors:
//	r = engine->RegisterObjectProperty("CMilitaryManager", "uint quotaScout", asOFFSET(CMilitaryManager, maxScouts)); ASSERT(r >= 0);
//	r = engine->RegisterObjectProperty("CMilitaryManager", "float quotaAttack", asOFFSET(CMilitaryManager, minAttackers)); ASSERT(r >= 0);
//	r = engine->RegisterObjectProperty("CMilitaryManager", "float quotaRaidMin", asOFFSET(CMilitaryManager::SRaidQuota, min), asOFFSET(CMilitaryManager, raid), false); ASSERT(r >= 0);
//	r = engine->RegisterObjectProperty("CMilitaryManager", "float quotaRaidAvg", asOFFSET(CMilitaryManager::SRaidQuota, avg), asOFFSET(CMilitaryManager, raid), false); ASSERT(r >= 0);

	r = engine->RegisterObjectType("SResponseInfo", 0, asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SResponseInfo", "float maxPercent", asOFFSET(CMilitaryManager::SRoleInfo, maxPerc)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SResponseInfo", "float factor", asOFFSET(CMilitaryManager::SRoleInfo, factor)); ASSERT(r >= 0);
	r = engine->RegisterObjectType("SVsInfo", 0, asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SVsInfo", "Type role", asOFFSET(CMilitaryManager::SRoleInfo::SVsInfo, role)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SVsInfo", "float ratio", asOFFSET(CMilitaryManager::SRoleInfo::SVsInfo, ratio)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SVsInfo", "float importance", asOFFSET(CMilitaryManager::SRoleInfo::SVsInfo, importance)); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("SResponseInfo", "SVsInfo@ GetVsInfo(Type) const", asFUNCTION(CMilitaryScript::GetVsInfo), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CMilitaryManager", "SResponseInfo@ GetResponseInfo(Type) const", asMETHOD(CMilitaryManager, GetRoleInfo), asCALL_THISCALL); ASSERT(r >= 0);
}

CMilitaryScript::~CMilitaryScript()
{
}

bool CMilitaryScript::Init()
{
	asIScriptModule* mod = script->GetEngine()->GetModule(CScriptManager::mainName.c_str());
	int r = mod->SetDefaultNamespace("Military"); ASSERT(r >= 0);
	InitModule(mod);
	militaryInfo.makeDefence = script->GetFunc(mod, "void AiMakeDefence(int, const AIFloat3& in)");
	return true;
}

void CMilitaryScript::MakeDefence(int cluster, const AIFloat3& pos)
{
	if (militaryInfo.makeDefence == nullptr) {
		static_cast<CMilitaryManager*>(manager)->DefaultMakeDefence(cluster, pos);
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(militaryInfo.makeDefence);
	ctx->SetArgDWord(0, cluster);
	ctx->SetArgAddress(1, &const_cast<AIFloat3&>(pos));
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

CMilitaryManager::SRoleInfo::SVsInfo* CMilitaryScript::GetVsInfo(CMilitaryManager::SRoleInfo* info, CCircuitDef::RoleT vsType)
{
	for (CMilitaryManager::SRoleInfo::SVsInfo& vs : info->vs) {
		if (vsType == vs.role) {
			return &vs;
		}
	}
	return &info->vs.emplace_back(vsType, 0.f, 0.f);
}

} // namespace circuit
