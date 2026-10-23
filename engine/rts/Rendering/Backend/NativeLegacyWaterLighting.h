/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <stdexcept>

namespace rendering {
// Only global fixed-function GL_LIGHTING, consumed by the unlit AdvWater bump
// producer and fixed-function legacy surface vertices. GL_LIGHT0..7 and material/light parameters are not global enables.
class NativeLegacyWaterLighting {
public:
	struct Value { uint64_t context=0; bool enabled=false, observed=false; };
	void InitializeContext(uint64_t generation) {
		if(!generation||value.context)throw std::logic_error("Lighting context invalid or already initialized");
		value={generation,false,true}; // Defined fresh-context GL_LIGHTING default.
	}
	void DestroyContext() noexcept {value={};}
	void Observe(bool enabled) {RequireContext();value.enabled=enabled;value.observed=true;}
	void Invalidate() noexcept {value.observed=false;}
	Value Snapshot() const {RequireContext();return value;}
	void RequireDisabled()const{RequireContext();if(!value.observed||value.enabled)throw std::runtime_error("Legacy water requires observed disabled fixed-function lighting");}
	void Restore(const Value& saved) {
		RequireContext();if(saved.context!=value.context){Invalidate();throw std::logic_error("Lighting attribute crossed source contexts");}
		value=saved;
	}
private:
	void RequireContext() const {if(!value.context)throw std::logic_error("Lighting lacks actual source context");}
	Value value;
};
inline NativeLegacyWaterLighting& OriginalNativeLegacyWaterLighting(){static NativeLegacyWaterLighting state;return state;}
}
