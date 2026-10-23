/*
 * GuardAction.cpp
 *
 *  Created on: Jul 3, 2016
 *      Author: rlcevg
 */

#include "unit/action/SupportAction.h"
#include "unit/CircuitUnit.h"
#include "task/fighter/SquadTask.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "Command.h"
#include "Sim/Units/CommandAI/Command.h"

namespace circuit {

using namespace springai;

CSupportAction::CSupportAction(CCircuitUnit* owner)
		: IUnitAction(owner, Type::SUPPORT)
		, updCount(0)
{
	isBlocking = true;
	CCircuitDef* cdef = owner->GetCircuitDef();
	isLowUpdate = cdef->IsAttrMelee() || (cdef->GetReloadTime() >= FRAMES_PER_SEC * 5);
}

CSupportAction::~CSupportAction()
{
}

void CSupportAction::Update(CCircuitAI* circuit)
{
	if ((updCount++ % 2 != 0) ||
		(isLowUpdate && (updCount % 4 != 1)))
	{
		return;
	}

	// TODO: Instead of polling order Guard command on leader change event.
	//       Though REGROUP interrupts
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	if (unit->GetCurrentCommand()->GetId() == CMD_GUARD) {
		return;
	}
	CCircuitUnit* leader = static_cast<ISquadTask*>(unit->GetTask())->GetLeader();
	TRY_UNIT(circuit, unit,
		unit->GetUnit()->Guard(leader->GetUnit());
	)
}

} // namespace circuit
