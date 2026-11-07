/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* Note: This file is machine generated, do not edit directly! */

#include "StubWeapon.h"

#include "IncludesSources.h"

	springai::StubWeapon::~StubWeapon() {}
	void springai::StubWeapon::SetSkirmishAIId(int skirmishAIId) {
		this->skirmishAIId = skirmishAIId;
	}
	int springai::StubWeapon::GetSkirmishAIId() const {
		return skirmishAIId;
	}

	void springai::StubWeapon::SetUnitId(int unitId) {
		this->unitId = unitId;
	}
	int springai::StubWeapon::GetUnitId() const {
		return unitId;
	}

	void springai::StubWeapon::SetWeaponId(int weaponId) {
		this->weaponId = weaponId;
	}
	int springai::StubWeapon::GetWeaponId() const {
		return weaponId;
	}

	void springai::StubWeapon::SetDef(springai::WeaponDef* def) {
		this->def = def;
	}
	springai::WeaponDef* springai::StubWeapon::GetDef() {
		return def;
	}

	void springai::StubWeapon::SetReloadFrame(int reloadFrame) {
		this->reloadFrame = reloadFrame;
	}
	int springai::StubWeapon::GetReloadFrame() {
		return reloadFrame;
	}

	void springai::StubWeapon::SetReloadTime(int reloadTime) {
		this->reloadTime = reloadTime;
	}
	int springai::StubWeapon::GetReloadTime() {
		return reloadTime;
	}

	void springai::StubWeapon::SetRange(float range) {
		this->range = range;
	}
	float springai::StubWeapon::GetRange() {
		return range;
	}

	void springai::StubWeapon::SetShieldEnabled(bool isShieldEnabled) {
		this->isShieldEnabled = isShieldEnabled;
	}
	bool springai::StubWeapon::IsShieldEnabled() {
		return isShieldEnabled;
	}

	void springai::StubWeapon::SetShieldPower(float shieldPower) {
		this->shieldPower = shieldPower;
	}
	float springai::StubWeapon::GetShieldPower() {
		return shieldPower;
	}

