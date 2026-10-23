/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
namespace rendering {
// Only enable lineage: the bounded native bump/surface profile does not consume
// inherited generated coordinates. It rejects them, never guesses a plane/mode.
class NativeLegacyWaterTexGen {
public:
	struct Coordinate {bool enabled=false,observed=false;};
	using Unit=std::array<Coordinate,4>;
	struct Values {uint64_t context=0;std::array<Unit,32> units;};
	void InitializeContext(uint64_t generation){
		if(!generation||values.context)throw std::logic_error("Texgen context invalid or already initialized");
		values={};values.context=generation;
		for(auto& unit:values.units)for(auto& c:unit)c={false,true};
	}
	void DestroyContext() noexcept{values={};}
	void Enable(unsigned unit,unsigned coordinate,bool enabled){
		Require(unit);if(coordinate<0x2000||coordinate>0x2003)return; // Original SetTexGenState ignores invalid S/T/R/Q.
		values.units[unit][coordinate-0x2000]={enabled,true};
	}
	void Reset(unsigned unit){Require(unit);for(auto& c:values.units[unit])c={false,true};}
	void Invalidate()noexcept{for(auto& unit:values.units)for(auto& c:unit)c.observed=false;}
	Values Save()const{if(!values.context)throw std::logic_error("Texgen attribute save without context");return values;}
	void Restore(const Values& saved){if(!values.context||values.context!=saved.context)throw std::logic_error("Texgen attribute scope crossed contexts");values=saved;}
	Unit Snapshot(unsigned unit)const{Require(unit);return values.units[unit];}
	void RequireDisabled(unsigned unit,unsigned mask=15)const{
		Require(unit);for(unsigned i=0;i<4;++i)if((mask&(1u<<i))&&(!values.units[unit][i].observed||values.units[unit][i].enabled))throw std::runtime_error("Legacy water inherited texgen outside explicit-coordinate profile");
	}
private:
	void Require(unsigned unit)const{if(!values.context||unit>=32)throw std::logic_error("Texgen lacks actual context/unit");}
	Values values{};
};
inline NativeLegacyWaterTexGen& OriginalNativeLegacyWaterTexGen(){static NativeLegacyWaterTexGen state;return state;}
}
