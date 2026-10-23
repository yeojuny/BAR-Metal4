/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
namespace rendering {
struct NativeMetalDensity {
	std::uint32_t width=0,height=0;
	std::vector<std::uint8_t> values; // original normalized R8 distribution, increasing map Z
};
struct NativeMetalExtraction {
	std::uint32_t width=0,height=0,losWidth=0,losHeight=0;
	std::vector<float> depth; // original R32F extraction; preserve negative/large finite values
	std::vector<std::uint16_t> los; // actual ground LOS at this extraction update
	bool globalLos=false;
};
struct NativeMetalInfoUpdate {
	std::uint64_t firstSerial=0,lastSerial=0;
	std::uint32_t repetitions=0; // exact repeated .25 blends, with RGB10 quantization each time
	bool clear=false; // original SwitchMode black clear, alpha1, before any blend
	float brightness=1;
	std::shared_ptr<const NativeMetalDensity> density;
	std::shared_ptr<const NativeMetalExtraction> extraction;
};
struct NativeMetalInfoFrame {
	std::uint64_t mapIdentity=0,modeGeneration=0,serial=0;
	std::uint32_t squaresX=0,squaresZ=0,paddedSquaresX=0,paddedSquaresZ=0;
	bool active=false;
	std::vector<NativeMetalInfoUpdate> updates;
};
void ValidateNativeMetalDensity(const NativeMetalDensity&);
void ValidateNativeMetalExtraction(const NativeMetalExtraction&);
void ValidateNativeMetalInfoFrame(const NativeMetalInfoFrame&);
void SetNativeMetalInfoGPUReady(bool) noexcept;
void SetNativeMetalInfoMaintenanceReady(bool) noexcept;
bool IsNativeMetalInfoConsumerAvailable() noexcept;
}
