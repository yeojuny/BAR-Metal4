/*
 * SpringUnit.h
 *
 *  Created on: Jul 20, 2026
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SPRING_SPRINGUNIT_H_
#define SRC_CIRCUIT_SPRING_SPRINGUNIT_H_

struct SSkirmishAICallback;

namespace circuit {

class CUnitAPI final {
public:
	CUnitAPI(const struct SSkirmishAICallback* clb, int sAIId);
	~CUnitAPI();

	int GetCMDQueueSize(int unitId);
	int GetCMD(int unitId, int commandIdx = 0);

private:
	const struct SSkirmishAICallback* sAICallback;
	int skirmishAIId;
};

} /* namespace circuit */

#endif /* SRC_CIRCUIT_SPRING_SPRINGUNIT_H_ */
