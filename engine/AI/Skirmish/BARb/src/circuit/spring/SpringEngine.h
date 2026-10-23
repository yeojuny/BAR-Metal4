/*
 * SpringEngine.h
 *
 *  Created on: Nov 8, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SPRING_SPRINGENGINE_H_
#define SRC_CIRCUIT_SPRING_SPRINGENGINE_H_

struct SSkirmishAICallback;

namespace circuit {

class CEngine final {
public:
	CEngine(const struct SSkirmishAICallback* clb, int sAIId);
	~CEngine();

	const char* GetVersionMajor() const;

private:
	const struct SSkirmishAICallback* sAICallback;
	int skirmishAIId;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SPRING_SPRINGENGINE_H_
