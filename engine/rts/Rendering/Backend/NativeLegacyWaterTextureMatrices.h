/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace rendering {
// Source-only fixed-function matrix lineage for CAdvWater/CRefractWater.
// This deliberately does not replace the accepted native UI matrix transport.
class NativeLegacyWaterTextureMatrices {
public:
	using Matrix=std::array<float,16>;
	static constexpr Matrix Identity={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
	struct Value { Matrix matrix=Identity; bool observed=false; };
	void InitializeContext(uint64_t generation) {
		if(!generation||context)throw std::logic_error("Texture matrix context invalid or already initialized");
		context=generation;actualDepth=0;
		for(auto& u:units){u.stack={Identity};u.observed=true;}
	}
	void DestroyContext(){context=0;actualDepth=0;for(auto& u:units)u={};}
	void ObserveStackDepth(unsigned depth){
		if(!context||depth<2||(actualDepth&&actualDepth!=depth))throw std::logic_error("Invalid source texture matrix stack capacity");
		actualDepth=depth;
	}
	void Invalidate(std::optional<unsigned> unit={}) noexcept {
		if(unit&&*unit<units.size())units[*unit].observed=false;
		else for(auto& u:units)u.observed=false;
	}
	void Load(std::optional<unsigned> unit,const Matrix& value){if(auto* u=Select(unit))u->stack.back()=value;}
	void Multiply(std::optional<unsigned> unit,const Matrix& value){
		if(auto* u=Select(unit)){
			Matrix product{};const auto old=u->stack.back();
			for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)product[c*4+r]+=old[k*4+r]*value[c*4+k];
			u->stack.back()=product;
		}
	}
	void Push(std::optional<unsigned> unit){
		if(auto* u=Select(unit)){
			// The source guarantees at least two entries. Deeper pushes need the
			// actual driver limit: overflow is a GL no-op, not an unlimited stack.
			if(u->stack.size()>=(actualDepth?actualDepth:2)){
				if(!actualDepth)u->observed=false;
				return;
			}
			u->stack.push_back(u->stack.back());
		}
	}
	void Pop(std::optional<unsigned> unit){if(auto* u=Select(unit))if(u->stack.size()>1)u->stack.pop_back();}
	Value Snapshot(unsigned unit)const{
		if(!context||unit>=units.size())throw std::logic_error("Texture matrix snapshot lacks actual context/unit");
		return {units[unit].stack.back(),units[unit].observed};
	}
	void RequireIdentity(unsigned unit)const{
		const auto v=Snapshot(unit);
		if(!v.observed||v.matrix!=Identity)throw std::runtime_error("Legacy water fixed texture matrix is unobserved or outside identity shader contract");
	}
private:
	struct Unit {std::vector<Matrix> stack;bool observed=false;};
	std::array<Unit,32> units;
	uint64_t context=0;unsigned actualDepth=0;
	Unit* Select(std::optional<unsigned> unit){
		if(!context)return nullptr;
		if(!unit||*unit>=units.size()){Invalidate();return nullptr;}
		return &units[*unit];
	}
};
inline NativeLegacyWaterTextureMatrices& OriginalNativeLegacyWaterTextureMatrices(){static NativeLegacyWaterTextureMatrices state;return state;}
}
