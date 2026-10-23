/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeMetalInfoData.h"
namespace rendering {
// Serialized original engine info-update timeline. Does not read or alter game state.
// Capture wraps this in a transaction; failed GPU work cannot acknowledge updates.
class NativeMetalInfoRecorder {
public:
	NativeMetalInfoRecorder();
	void Reset();
	void SetMode(bool active,float brightness,std::uint32_t x,std::uint32_t z,std::uint32_t paddedX,std::uint32_t paddedZ);
	bool NeedsExtractionUpdate() const;
	void Update(std::shared_ptr<const NativeMetalDensity>,std::shared_ptr<const NativeMetalExtraction>);
	std::shared_ptr<const NativeMetalInfoFrame> Capture() const;
	void Retire(std::uint64_t completedSerial);
private:
	NativeMetalInfoFrame frame;
	std::shared_ptr<const NativeMetalDensity> priorDensity;
	std::shared_ptr<const NativeMetalExtraction> priorExtraction;
	std::uint32_t extractionUpdateCounter=0;
	bool firstUpdate=true;
	float brightness=1;
	std::uint64_t NextSerial();
};
}
