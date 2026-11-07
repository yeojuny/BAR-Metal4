/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* Note: This file is machine generated, do not edit directly! */

#ifndef _CPPWRAPPER_WRAPPWEAPON_H
#define _CPPWRAPPER_WRAPPWEAPON_H

#include <climits> // for INT_MAX (required by unit-command wrapping functions)

#include "IncludesHeaders.h"
#include "Weapon.h"

namespace springai {

/**
 * Lets C++ Skirmish AIs call back to the Spring engine.
 *
 * @author	AWK wrapper script
 * @version	GENERATED
 */
class WrappWeapon : public Weapon {

private:
	int skirmishAIId;
	int unitId;
	int weaponId;

	WrappWeapon(int skirmishAIId, int unitId, int weaponId);
	virtual ~WrappWeapon();
public:
public:
	// @Override
	virtual int GetSkirmishAIId() const;
public:
	// @Override
	virtual int GetUnitId() const;
public:
	// @Override
	virtual int GetWeaponId() const;
public:
	static Weapon* GetInstance(int skirmishAIId, int unitId, int weaponId);

public:
	// @Override
	virtual springai::WeaponDef* GetDef();

	/**
	 * Next tick the weapon can fire again.
	 */
public:
	// @Override
	virtual int GetReloadFrame();

	/**
	 * Time between succesive fires in ticks.
	 */
public:
	// @Override
	virtual int GetReloadTime();

public:
	// @Override
	virtual float GetRange();

public:
	// @Override
	virtual bool IsShieldEnabled();

public:
	// @Override
	virtual float GetShieldPower();
}; // class WrappWeapon

}  // namespace springai

#endif // _CPPWRAPPER_WRAPPWEAPON_H

