/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* Note: This file is machine generated, do not edit directly! */

#ifndef _CPPWRAPPER_STUBWEAPON_H
#define _CPPWRAPPER_STUBWEAPON_H

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
class StubWeapon : public Weapon {

protected:
	virtual ~StubWeapon();
private:
	int skirmishAIId;/* = 0;*/ // TODO: FIXME: put this into a constructor
public:
	virtual void SetSkirmishAIId(int skirmishAIId);
	// @Override
	virtual int GetSkirmishAIId() const;
private:
	int unitId;/* = 0;*/ // TODO: FIXME: put this into a constructor
public:
	virtual void SetUnitId(int unitId);
	// @Override
	virtual int GetUnitId() const;
private:
	int weaponId;/* = 0;*/ // TODO: FIXME: put this into a constructor
public:
	virtual void SetWeaponId(int weaponId);
	// @Override
	virtual int GetWeaponId() const;
private:
	springai::WeaponDef* def;/* = 0;*/ // TODO: FIXME: put this into a constructor
public:
	virtual void SetDef(springai::WeaponDef* def);
	// @Override
	virtual springai::WeaponDef* GetDef();
	/**
	 * Next tick the weapon can fire again.
	 */
private:
	int reloadFrame;/* = 0;*/ // TODO: FIXME: put this into a constructor
public:
	virtual void SetReloadFrame(int reloadFrame);
	// @Override
	virtual int GetReloadFrame();
	/**
	 * Time between succesive fires in ticks.
	 */
private:
	int reloadTime;/* = 0;*/ // TODO: FIXME: put this into a constructor
public:
	virtual void SetReloadTime(int reloadTime);
	// @Override
	virtual int GetReloadTime();
private:
	float range;/* = 0;*/ // TODO: FIXME: put this into a constructor
public:
	virtual void SetRange(float range);
	// @Override
	virtual float GetRange();
private:
	bool isShieldEnabled;/* = false;*/ // TODO: FIXME: put this into a constructor
public:
	virtual void SetShieldEnabled(bool isShieldEnabled);
	// @Override
	virtual bool IsShieldEnabled();
private:
	float shieldPower;/* = 0;*/ // TODO: FIXME: put this into a constructor
public:
	virtual void SetShieldPower(float shieldPower);
	// @Override
	virtual float GetShieldPower();
}; // class StubWeapon

}  // namespace springai

#endif // _CPPWRAPPER_STUBWEAPON_H

