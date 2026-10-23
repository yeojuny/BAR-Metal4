/*
 * WaitTask.cpp
 *
 *  Created on: Jul 24, 2016
 *      Author: rlcevg
 */

#include "task/common/WaitTask.h"
#include "module/TaskModule.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"

#include "spring/SpringUnit.h"

#include "AISCommands.h"

namespace circuit {

IWaitTask::IWaitTask(ITaskModule* mgr, bool stop, int timeout)
		: IUnitTask(mgr, Priority::NORMAL, Type::WAIT, timeout)
		, isStop(stop)
{
}

IWaitTask::~IWaitTask()
{
}

void IWaitTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	lastTouched = manager->GetCircuit()->GetLastFrame();
}

void IWaitTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);

	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void IWaitTask::Start(CCircuitUnit* unit)
{
	if (!isStop) {
		return;
	}
	CCircuitAI* circuit = manager->GetCircuit();
	CUnitAPI* unitAPI = circuit->GetUnitAPI();
	int cmdSize = unitAPI->GetCMDQueueSize(unit->GetId());
	if (cmdSize == 0) {
		return;
	}
	std::vector<float> params;
	params.reserve(cmdSize);
	for (int commandIdx = 0; commandIdx < cmdSize; ++commandIdx) {
		params.push_back(unitAPI->GetCMD(unit->GetId(), commandIdx));
	}
	TRY_UNIT(circuit, unit,
		unit->CmdRemove(std::move(params), UNIT_COMMAND_OPTION_ALT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY);
	)
}

void IWaitTask::Update()
{
}

void IWaitTask::OnUnitIdle(CCircuitUnit* unit)
{
}

void IWaitTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	RemoveAssignee(unit);
}

} // namespace circuit
