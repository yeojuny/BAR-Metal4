/*
 * FactoryScript.h
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_FACTORYSCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_FACTORYSCRIPT_H_

#include "script/TaskModuleScript.h"

class CScriptArray;

namespace springai {
	class AIFloat3;
}

namespace circuit {

class CFactoryManager;
class CCircuitDef;

class CFactoryScript final: public ITaskModuleScript {
public:
	CFactoryScript(CScriptManager* scr, CFactoryManager* mgr);
	virtual ~CFactoryScript();

	virtual bool Init() override;

public:
	bool IsSwitchTime(int lastSwitchFrame);
	bool IsSwitchAllowed(CCircuitDef* facDef);
	CCircuitDef* GetFactoryToBuild(const springai::AIFloat3& pos, bool isStart, bool isReset);

private:
	void SetTierWeights(CCircuitDef* facDef, int surfType, int tier, const CScriptArray* array);
	CScriptArray* GetTierWeights(CCircuitDef* facDef, int surfType, int tier) const;
	void SetImportance(CCircuitDef* facDef, const CScriptArray* array);
	CScriptArray* GetImportance(CCircuitDef* facDef) const;

	struct SScriptInfo {
		asIScriptFunction* isSwitchTime = nullptr;
		asIScriptFunction* isSwitchAllowed = nullptr;
		asIScriptFunction* getFactoryToBuild = nullptr;
	} factoryInfo;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_FACTORYSCRIPT_H_
