/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBumpWaterData.h"
#include <functional>

namespace rendering {
// Original CBumpWater CPU producer. Only the normal unsynced engine thread may
// initialize/update it. The TextureProvider shares WorldFrameCapture's immutable
// decoded bitmap cache. No GL objects or GPU work are created here.
class NativeBumpWaterCapture {
public:
	using TextureProvider=std::function<std::shared_ptr<const WorldTexture>(const std::string&)>;
	NativeBumpWaterCapture();~NativeBumpWaterCapture();
	NativeBumpWaterCapture(const NativeBumpWaterCapture&)=delete;
	NativeBumpWaterCapture& operator=(const NativeBumpWaterCapture&)=delete;
	// This is the original InitResources boundary (map/water mode/reload).
	// The caller supplies effective graphical settings, not HEADLESS defaults.
	void Initialize(const NativeBumpWaterSettings&,TextureProvider);
	void Reset();
	// Call at original CWorldDrawer::Update's newSimFrame water stage only.
	// Replaying a captured frame never calls this or advances the actual guRNG.
	void Update();
	std::shared_ptr<const NativeBumpWaterFrame> Capture(const WorldFrame&,bool shadows,bool infoTexture,bool wireframe)const;
	// Ack only after the enclosing complete world submission retires. Obsolete
	// generations are ignored; future/mismatched sequence acknowledgments fail.
	void DidComplete(uint64_t generation,uint64_t normalSequence,uint64_t coastSequence);
	bool Initialized()const;
private:
	struct State;std::unique_ptr<State> state;
};
} // namespace rendering
