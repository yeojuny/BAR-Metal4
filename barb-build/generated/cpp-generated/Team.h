/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* Note: This file is machine generated, do not edit directly! */

#ifndef _CPPWRAPPER_TEAM_H
#define _CPPWRAPPER_TEAM_H

#include <climits> // for INT_MAX (required by unit-command wrapping functions)

#include "IncludesHeaders.h"

namespace springai {

/**
 * Lets C++ Skirmish AIs call back to the Spring engine.
 *
 * @author	AWK wrapper script
 * @version	GENERATED
 */
class Team {

public:
	virtual ~Team(){}
public:
	virtual int GetSkirmishAIId() const = 0;

public:
	virtual int GetTeamId() const = 0;

public:
	virtual bool HasAIController() = 0;

	/**
	 * @return float value of parameter if it's set, defaultValue otherwise.
	 */
public:
	virtual float GetRulesParamFloat(const char* teamRulesParamName, float defaultValue) = 0;

	/**
	 * @return string value of parameter if it's set, defaultValue otherwise.
	 */
public:
	virtual const char* GetRulesParamString(const char* teamRulesParamName, const char* defaultValue) = 0;

}; // class Team

}  // namespace springai

#endif // _CPPWRAPPER_TEAM_H

