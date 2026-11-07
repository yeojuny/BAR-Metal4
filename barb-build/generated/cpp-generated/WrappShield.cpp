/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* Note: This file is machine generated, do not edit directly! */

#include "WrappShield.h"

#include "IncludesSources.h"

	springai::WrappShield::WrappShield(int skirmishAIId, int weaponDefId) {

		this->skirmishAIId = skirmishAIId;
		this->weaponDefId = weaponDefId;
	}

	springai::WrappShield::~WrappShield() {

	}

	int springai::WrappShield::GetSkirmishAIId() const {

		return skirmishAIId;
	}

	int springai::WrappShield::GetWeaponDefId() const {

		return weaponDefId;
	}

	springai::WrappShield::Shield* springai::WrappShield::GetInstance(int skirmishAIId, int weaponDefId) {

		if (weaponDefId < 0) {
			return NULL;
		}

		springai::Shield* internal_ret = NULL;
		internal_ret = new springai::WrappShield(skirmishAIId, weaponDefId);
		return internal_ret;
	}


	float springai::WrappShield::GetResourceUse(Resource* resource) {

		float internal_ret_int;

		int resourceId = resource->GetResourceId();

		internal_ret_int = bridged_WeaponDef_Shield_getResourceUse(this->GetSkirmishAIId(), this->GetWeaponDefId(), resourceId);
		return internal_ret_int;
	}

	float springai::WrappShield::GetRadius() {

		float internal_ret_int;

		internal_ret_int = bridged_WeaponDef_Shield_getRadius(this->GetSkirmishAIId(), this->GetWeaponDefId());
		return internal_ret_int;
	}

	float springai::WrappShield::GetForce() {

		float internal_ret_int;

		internal_ret_int = bridged_WeaponDef_Shield_getForce(this->GetSkirmishAIId(), this->GetWeaponDefId());
		return internal_ret_int;
	}

	float springai::WrappShield::GetMaxSpeed() {

		float internal_ret_int;

		internal_ret_int = bridged_WeaponDef_Shield_getMaxSpeed(this->GetSkirmishAIId(), this->GetWeaponDefId());
		return internal_ret_int;
	}

	float springai::WrappShield::GetPower() {

		float internal_ret_int;

		internal_ret_int = bridged_WeaponDef_Shield_getPower(this->GetSkirmishAIId(), this->GetWeaponDefId());
		return internal_ret_int;
	}

	float springai::WrappShield::GetPowerRegen() {

		float internal_ret_int;

		internal_ret_int = bridged_WeaponDef_Shield_getPowerRegen(this->GetSkirmishAIId(), this->GetWeaponDefId());
		return internal_ret_int;
	}

	float springai::WrappShield::GetPowerRegenResource(Resource* resource) {

		float internal_ret_int;

		int resourceId = resource->GetResourceId();

		internal_ret_int = bridged_WeaponDef_Shield_getPowerRegenResource(this->GetSkirmishAIId(), this->GetWeaponDefId(), resourceId);
		return internal_ret_int;
	}

	float springai::WrappShield::GetStartingPower() {

		float internal_ret_int;

		internal_ret_int = bridged_WeaponDef_Shield_getStartingPower(this->GetSkirmishAIId(), this->GetWeaponDefId());
		return internal_ret_int;
	}

	int springai::WrappShield::GetRechargeDelay() {

		int internal_ret_int;

		internal_ret_int = bridged_WeaponDef_Shield_getRechargeDelay(this->GetSkirmishAIId(), this->GetWeaponDefId());
		return internal_ret_int;
	}

	int springai::WrappShield::GetInterceptType() {

		int internal_ret_int;

		internal_ret_int = bridged_WeaponDef_Shield_getInterceptType(this->GetSkirmishAIId(), this->GetWeaponDefId());
		return internal_ret_int;
	}
