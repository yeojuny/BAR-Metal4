/*
 * SpringUnit.cpp
 *
 *  Created on: Jul 20, 2026
 *      Author: rlcevg
 */

#include "spring/SpringUnit.h"

#include "SSkirmishAICallback.h"	// "direct" C API

namespace circuit {

CUnitAPI::CUnitAPI(const struct SSkirmishAICallback* clb, int sAIId)
		: sAICallback(clb)
		, skirmishAIId(sAIId)
{
}

CUnitAPI::~CUnitAPI()
{
}

int CUnitAPI::GetCMDQueueSize(int unitId)
{
	return sAICallback->Unit_getCurrentCommands(skirmishAIId, unitId);
}

int CUnitAPI::GetCMD(int unitId, int commandIdx)
{
	return sAICallback->Unit_CurrentCommand_getId(skirmishAIId, unitId, commandIdx);
}

} /* namespace circuit */
