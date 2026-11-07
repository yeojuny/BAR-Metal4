/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* Note: This file is machine generated, do not edit directly! */

#include "AbstractWeapon.h"

#include "IncludesSources.h"

	springai::AbstractWeapon::~AbstractWeapon() {}
	int springai::AbstractWeapon::CompareTo(const Weapon& other) {
		static const int EQUAL  =  0;
		static const int BEFORE = -1;
		static const int AFTER  =  1;

		if ((Weapon*)this == &other) return EQUAL;

		if (this->GetSkirmishAIId() < other.GetSkirmishAIId()) return BEFORE;
		if (this->GetSkirmishAIId() > other.GetSkirmishAIId()) return AFTER;
		if (this->GetUnitId() < other.GetUnitId()) return BEFORE;
		if (this->GetUnitId() > other.GetUnitId()) return AFTER;
		if (this->GetWeaponId() < other.GetWeaponId()) return BEFORE;
		if (this->GetWeaponId() > other.GetWeaponId()) return AFTER;
		return EQUAL;
	}

bool springai::AbstractWeapon::Equals(const Weapon& other) {

	if (this->GetSkirmishAIId() != other.GetSkirmishAIId()) return false;
	if (this->GetUnitId() != other.GetUnitId()) return false;
	if (this->GetWeaponId() != other.GetWeaponId()) return false;
	return true;
}

int springai::AbstractWeapon::HashCode() {

	int internal_ret = 23;

	internal_ret += this->GetSkirmishAIId() * (int) (10E6);
	internal_ret += this->GetUnitId() * (int) (10E5);
	internal_ret += this->GetWeaponId() * (int) (10E4);

	return internal_ret;
}

std::string springai::AbstractWeapon::ToString() {

	std::string internal_ret = "Weapon";

	char _buff[64];
	sprintf(_buff, "skirmishAIId=%i, ", this->GetSkirmishAIId());
	internal_ret = internal_ret + _buff;
	sprintf(_buff, "unitId=%i, ", this->GetUnitId());
	internal_ret = internal_ret + _buff;
	sprintf(_buff, "weaponId=%i, ", this->GetWeaponId());
	internal_ret = internal_ret + _buff;
	internal_ret = internal_ret + ")";

	return internal_ret;
}

