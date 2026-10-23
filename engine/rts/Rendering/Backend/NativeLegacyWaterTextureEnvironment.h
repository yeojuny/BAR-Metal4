/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>

namespace rendering {
// Only the fixed-function texture environment mode actually consumed by the
// AdvWater bump producer. Combine operands/colors are irrelevant in MODULATE.
class NativeLegacyWaterTextureEnvironment {
public:
	static constexpr unsigned Units=32, Modulate=0x2100;
	struct Mode { unsigned value=Modulate; bool observed=false; };
	struct Values { uint64_t context=0; std::array<Mode,Units> modes{}; };
	void InitializeContext(uint64_t context) {
		if(!context || values.context) throw std::logic_error("Texture environment context already owned or invalid");
		values={};values.context=context;
		for(auto& mode:values.modes)mode={Modulate,true}; // Defined fresh-context initial state.
	}
	void DestroyContext() noexcept {values={};}
	void Observe(unsigned unit,unsigned target,unsigned pname,float first) {
		RequireContext();RequireUnit(unit);
		if(target!=0x2300 || pname!=0x2200)return; // TEXTURE_ENV / TEXTURE_ENV_MODE only.
		// Exact enum values are portable. Fractional/out-of-range/extension-only
		// inputs require source conversion/capability evidence, never a guess.
		for(unsigned mode:{0x2100u,0x2101u,0x0BE2u,0x1E01u,0x0104u,0x8570u})
			if(first==float(mode)){values.modes[unit]={mode,true};return;}
		values.modes[unit].observed=false;
	}
	void ResetModulate(unsigned actualUnit) {Observe(actualUnit,0x2300,0x2200,float(Modulate));}
	void InvalidateAll() noexcept {for(auto& mode:values.modes)mode.observed=false;}
	Mode Snapshot(unsigned unit) const {RequireContext();RequireUnit(unit);return values.modes[unit];}
	Values SaveTextureAttribute() const {RequireContext();return values;}
	void RestoreTextureAttribute(const Values& saved) {
		RequireContext();if(saved.context!=values.context)throw std::logic_error("Texture environment attribute scope crossed contexts");values=saved;
	}
private:
	void RequireContext() const {if(!values.context)throw std::logic_error("Texture environment lacks actual source context");}
	static void RequireUnit(unsigned unit) {if(unit>=Units)throw std::out_of_range("Texture environment unit outside original engine range");}
	Values values;
};
inline NativeLegacyWaterTextureEnvironment& OriginalNativeLegacyWaterTextureEnvironment() {
	static NativeLegacyWaterTextureEnvironment state;return state;
}
}
