/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* Note: This file is machine generated, do not edit directly! */

#ifndef _CPPWRAPPER_WRAPPTEAM_H
#define _CPPWRAPPER_WRAPPTEAM_H

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
class WrappTeam : public Team {

private:
	int skirmishAIId;
	int teamId;

	WrappTeam(int skirmishAIId, int teamId);
	virtual ~WrappTeam();
public:
public:
	// @Override
	virtual int GetSkirmishAIId() const;
public:
	// @Override
	virtual int GetTeamId() const;
public:
	static Team* GetInstance(int skirmishAIId, int teamId);

public:
	// @Override
	virtual bool HasAIController();

	/**
	 * @return float value of parameter if it's set, defaultValue otherwise.
	 */
public:
	// @Override
	virtual float GetRulesParamFloat(const char* teamRulesParamName, float defaultValue);

	/**
	 * @return string value of parameter if it's set, defaultValue otherwise.
	 */
public:
	// @Override
	virtual const char* GetRulesParamString(const char* teamRulesParamName, const char* defaultValue);
}; // class WrappTeam

}  // namespace springai

#endif // _CPPWRAPPER_WRAPPTEAM_H

