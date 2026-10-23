/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeOriginalShadowParticlesData.h"
#include "NativeSourceProgramBinding.h"
#include <stdexcept>

namespace rendering {
// The real ProjectileDrawer Init/Kill owns definition and retirement. The
// renderer supplies actual compiled variants before the original Enable.
class NativeOriginalShadowParticlesProgramSource {
public:
	void Initialize(unsigned sampler,std::array<float,4> alpha,float colorMode){
		if(defined||sampler!=0)throw std::logic_error("Original ProjFX shadow initialization is repeated or has another sampler");
		defined=true;textureArray=false;alphaControl=alpha;shadowColorMode=colorMode;
		// The original initialization enables, writes these uniforms, then
		// disables the shader. Only its final binding is externally observable.
		OriginalNativeSourceProgramBinding().Unbind();
	}
	void Construct(bool array,std::shared_ptr<const void> compiled){
		if(!defined||!compiled||programs[array])throw std::logic_error("Original ProjFX shadow variant construction is missing/repeated");
		programs[array]=std::move(compiled);
	}
	bool Constructed(bool array)const noexcept{return defined&&bool(programs[array]);}
	void Enable(){
		if(!Constructed(textureArray))throw std::logic_error("Original ProjFX shadow Enable has no actual compiled variant");
		selected=true;OriginalNativeSourceProgramBinding().BindEngine(programs[textureArray]);
	}
	void SetTextureArray(bool array){
		RequireSelected();if(!Constructed(array))throw std::logic_error("Original ProjFX shadow flag has no actual compiled variant");
		textureArray=array;OriginalNativeSourceProgramBinding().BindEngine(programs[array]);
	}
	void SetShadowColorMode(float value){RequireSelected();shadowColorMode=value;}
	void Freeze(NativeOriginalShadowParticlesDraw& draw)const{
		RequireSelected();draw.compiledOriginal=programs[textureArray];draw.textureArray=textureArray;draw.alphaControl=alphaControl;draw.shadowColorMode=shadowColorMode;
	}
	void Disable(){RequireSelected();OriginalNativeSourceProgramBinding().Unbind();selected=false;}
	void Reset()noexcept{
		const auto current=OriginalNativeSourceProgramBinding().Current();
		if(current.owner&&(current.owner==programs[0]||current.owner==programs[1]))OriginalNativeSourceProgramBinding().Unbind();
		programs={};defined=selected=textureArray=false;alphaControl={};shadowColorMode=1;
	}
private:
	void RequireSelected()const{
		if(!defined||!selected||!programs[textureArray]||OriginalNativeSourceProgramBinding().EngineOwner()!=programs[textureArray])throw std::logic_error("Original ProjFX shadow output has another current program");
	}
	std::array<std::shared_ptr<const void>,2> programs;
	std::array<float,4> alphaControl{};
	float shadowColorMode=1;
	bool defined=false,selected=false,textureArray=false;
};
inline NativeOriginalShadowParticlesProgramSource& OriginalNativeShadowParticlesProgramSource(){
	static NativeOriginalShadowParticlesProgramSource source;return source;
}
}
