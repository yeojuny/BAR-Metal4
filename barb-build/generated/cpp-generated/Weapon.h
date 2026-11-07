/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* Note: This file is machine generated, do not edit directly! */

#ifndef _CPPWRAPPER_WEAPON_H
#define _CPPWRAPPER_WEAPON_H

#include <climits> // for INT_MAX (required by unit-command wrapping functions)

#include "IncludesHeaders.h"

namespace springai {

/**
 * Lets C++ Skirmish AIs call back to the Spring engine.
 *
 * @author	AWK wrapper script
 * @version	GENERATED
 */
class Weapon {

public:
	virtual ~Weapon(){}
public:
	virtual int GetSkirmishAIId() const = 0;

public:
	virtual int GetUnitId() const = 0;

public:
	virtual int GetWeaponId() const = 0;

public:
	virtual springai::WeaponDef* GetDef() = 0;

	/**
	 * Next tick the weapon can fire again.
	 */
public:
	virtual int GetReloadFrame() = 0;

	/**
	 * Time between succesive fires in ticks.
	 */
public:
	virtual int GetReloadTime() = 0;

public:
	virtual float GetRange() = 0;

public:
	virtual bool IsShieldEnabled() = 0;

public:
	virtual float GetShieldPower() = 0;

}; // class Weapon

}  // namespace springai

#endif // _CPPWRAPPER_WEAPON_H

