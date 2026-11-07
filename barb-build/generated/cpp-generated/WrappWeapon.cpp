/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* Note: This file is machine generated, do not edit directly! */

#include "WrappWeapon.h"

#include "IncludesSources.h"

	springai::WrappWeapon::WrappWeapon(int skirmishAIId, int unitId, int weaponId) {

		this->skirmishAIId = skirmishAIId;
		this->unitId = unitId;
		this->weaponId = weaponId;
	}

	springai::WrappWeapon::~WrappWeapon() {

	}

	int springai::WrappWeapon::GetSkirmishAIId() const {

		return skirmishAIId;
	}

	int springai::WrappWeapon::GetUnitId() const {

		return unitId;
	}

	int springai::WrappWeapon::GetWeaponId() const {

		return weaponId;
	}

	springai::WrappWeapon::Weapon* springai::WrappWeapon::GetInstance(int skirmishAIId, int unitId, int weaponId) {

		if (weaponId < 0) {
			return NULL;
		}

		springai::Weapon* internal_ret = NULL;
		internal_ret = new springai::WrappWeapon(skirmishAIId, unitId, weaponId);
		return internal_ret;
	}


	springai::WeaponDef* springai::WrappWeapon::GetDef() {

		WeaponDef* internal_ret_int_out;
		int internal_ret_int;

		internal_ret_int = bridged_Unit_Weapon_getDef(this->GetSkirmishAIId(), this->GetUnitId(), this->GetWeaponId());
		internal_ret_int_out = springai::WrappWeaponDef::GetInstance(skirmishAIId, internal_ret_int);

		return internal_ret_int_out;
	}

	int springai::WrappWeapon::GetReloadFrame() {

		int internal_ret_int;

		internal_ret_int = bridged_Unit_Weapon_getReloadFrame(this->GetSkirmishAIId(), this->GetUnitId(), this->GetWeaponId());
		return internal_ret_int;
	}

	int springai::WrappWeapon::GetReloadTime() {

		int internal_ret_int;

		internal_ret_int = bridged_Unit_Weapon_getReloadTime(this->GetSkirmishAIId(), this->GetUnitId(), this->GetWeaponId());
		return internal_ret_int;
	}

	float springai::WrappWeapon::GetRange() {

		float internal_ret_int;

		internal_ret_int = bridged_Unit_Weapon_getRange(this->GetSkirmishAIId(), this->GetUnitId(), this->GetWeaponId());
		return internal_ret_int;
	}

	bool springai::WrappWeapon::IsShieldEnabled() {

		bool internal_ret_int;

		internal_ret_int = bridged_Unit_Weapon_isShieldEnabled(this->GetSkirmishAIId(), this->GetUnitId(), this->GetWeaponId());
		return internal_ret_int;
	}

	float springai::WrappWeapon::GetShieldPower() {

		float internal_ret_int;

		internal_ret_int = bridged_Unit_Weapon_getShieldPower(this->GetSkirmishAIId(), this->GetUnitId(), this->GetWeaponId());
		return internal_ret_int;
	}
