/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* Note: This file is machine generated, do not edit directly! */

#include "WrappTeam.h"

#include "IncludesSources.h"

	springai::WrappTeam::WrappTeam(int skirmishAIId, int teamId) {

		this->skirmishAIId = skirmishAIId;
		this->teamId = teamId;
	}

	springai::WrappTeam::~WrappTeam() {

	}

	int springai::WrappTeam::GetSkirmishAIId() const {

		return skirmishAIId;
	}

	int springai::WrappTeam::GetTeamId() const {

		return teamId;
	}

	springai::WrappTeam::Team* springai::WrappTeam::GetInstance(int skirmishAIId, int teamId) {

		if (teamId < 0) {
			return NULL;
		}

		springai::Team* internal_ret = NULL;
		internal_ret = new springai::WrappTeam(skirmishAIId, teamId);
		return internal_ret;
	}


	bool springai::WrappTeam::HasAIController() {

		bool internal_ret_int;

		internal_ret_int = bridged_Team_hasAIController(this->GetSkirmishAIId(), this->GetTeamId());
		return internal_ret_int;
	}

	float springai::WrappTeam::GetRulesParamFloat(const char* teamRulesParamName, float defaultValue) {

		float internal_ret_int;

		internal_ret_int = bridged_Team_getRulesParamFloat(this->GetSkirmishAIId(), this->GetTeamId(), teamRulesParamName, defaultValue);
		return internal_ret_int;
	}

	const char* springai::WrappTeam::GetRulesParamString(const char* teamRulesParamName, const char* defaultValue) {

		const char* internal_ret_int;

		internal_ret_int = bridged_Team_getRulesParamString(this->GetSkirmishAIId(), this->GetTeamId(), teamRulesParamName, defaultValue);
		return internal_ret_int;
	}
