/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* Note: This file is machine generated, do not edit directly! */

#ifndef _CPPWRAPPER_FEATURE_H
#define _CPPWRAPPER_FEATURE_H

#include <climits> // for INT_MAX (required by unit-command wrapping functions)

#include "IncludesHeaders.h"

namespace springai {

/**
 * Lets C++ Skirmish AIs call back to the Spring engine.
 *
 * @author	AWK wrapper script
 * @version	GENERATED
 */
class Feature {

public:
	virtual ~Feature(){}
public:
	virtual int GetSkirmishAIId() const = 0;

public:
	virtual int GetFeatureId() const = 0;

public:
	virtual springai::FeatureDef* GetDef() = 0;

public:
	virtual float GetHealth() = 0;

public:
	virtual float GetReclaimLeft() = 0;

public:
	virtual springai::AIFloat3 GetPosition() = 0;

	/**
	 * @return float value of parameter if it's set, defaultValue otherwise.
	 */
public:
	virtual float GetRulesParamFloat(const char* featureRulesParamName, float defaultValue) = 0;

	/**
	 * @return string value of parameter if it's set, defaultValue otherwise.
	 */
public:
	virtual const char* GetRulesParamString(const char* featureRulesParamName, const char* defaultValue) = 0;

public:
	virtual springai::UnitDef* GetResurrectDef() = 0;

public:
	virtual short GetBuildingFacing() = 0;

}; // class Feature

}  // namespace springai

#endif // _CPPWRAPPER_FEATURE_H

