/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* Note: This file is machine generated, do not edit directly! */

#ifndef _CPPWRAPPER_STUBTEAM_H
#define _CPPWRAPPER_STUBTEAM_H

#include <climits> // for INT_MAX (required by unit-command wrapping functions)

#include "IncludesHeaders.h"
#include "Team.h"

namespace springai {

/**
 * Lets C++ Skirmish AIs call back to the Spring engine.
 *
 * @author	AWK wrapper script
 * @version	GENERATED
 */
class StubTeam : public Team {

protected:
	virtual ~StubTeam();
private:
	int skirmishAIId;/* = 0;*/ // TODO: FIXME: put this into a constructor
public:
	virtual void SetSkirmishAIId(int skirmishAIId);
	// @Override
	virtual int GetSkirmishAIId() const;
private:
	int teamId;/* = 0;*/ // TODO: FIXME: put this into a constructor
public:
	virtual void SetTeamId(int teamId);
	// @Override
	virtual int GetTeamId() const;
	// @Override
	virtual bool HasAIController();
	/**
	 * @return float value of parameter if it's set, defaultValue otherwise.
	 */
	// @Override
	virtual float GetRulesParamFloat(const char* teamRulesParamName, float defaultValue);
	/**
	 * @return string value of parameter if it's set, defaultValue otherwise.
	 */
	// @Override
	virtual const char* GetRulesParamString(const char* teamRulesParamName, const char* defaultValue);
}; // class StubTeam

}  // namespace springai

#endif // _CPPWRAPPER_STUBTEAM_H

