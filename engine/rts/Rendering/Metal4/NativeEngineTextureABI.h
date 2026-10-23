/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

namespace rendering::metal4 {
// Two bits per original image: 0=incomplete/absent, 1=color2D, 2=depth2D,
// 3=cube. ShadowGen's alpha image uses its original slots 0/1; the model
// renderer uses 0/2, 1/3, comparison4, shadow-color5/6 and cubes7/8.
inline std::string NativeEngineTextureShaderDefines(uint32_t key,bool shadowPass=false)
{
	if((key>>12)||(shadowPass&&key>2))throw std::invalid_argument("Original engine image ABI key is invalid");
	std::string source;
	auto define=[&](const std::string& name,const std::string& value){source+="#define "+name+" "+value+"\n";};
	for(unsigned slot=0;slot<6;++slot){
		const unsigned kind=(key>>(slot*2))&3u;
		if((slot<2||slot==3)?kind==3:slot==2?(kind!=0&&kind!=2):(kind!=0&&kind!=3))
			throw std::invalid_argument("Original engine image kind differs from its sampler type");
		const std::string suffix=std::to_string(slot),name="nativeImage"+suffix,state="nativeSampler"+suffix;
		const std::string macro="ENGINE_IMAGE"+suffix;
		if(kind==0){define(macro+"_ARGS","");define(macro+"_PARAMS","");define(macro+"_PASS","");}
		else{
			const unsigned texture=slot<2?(kind==2?(shadowPass?1:slot+2):slot):slot==2?4:slot==3?(kind==2?6:5):slot+3;
			const std::string type=kind==3?"texturecube<float>":kind==2?"depth2d<float>":"texture2d<float>";
			define(macro+"_ARGS",", "+type+" "+name+" [[texture("+std::to_string(texture)+")]], sampler "+state+" [[sampler("+suffix+")]]");
			define(macro+"_PARAMS",", "+type+" "+name+", sampler "+state);
			define(macro+"_PASS",", "+name+", "+state);
		}
		if(slot<2||slot==3){
			const std::string image=slot==3?"u.lightImage":"u.image"+suffix;
			const std::string swizzle=slot==3?"u.lightSwizzle":"u.swizzle"+suffix;
			const std::string bias=slot==3?"u.lightSampler.x":slot==0?"u.samplerParameters.x":"u.samplerParameters.y";
			// Preserve the original early return for a proven incomplete image:
			// it precedes UV conversion, sampling and texture swizzle.
			define("ENGINE_SAMPLE"+suffix+"(UV)",kind?"OriginalSample((UV),"+image+","+swizzle+","+bias+","+name+","+state+")":"float4(0,0,0,1)");
		}else if(slot==2){
			define("ENGINE_SHADOW_SAMPLE(UV,REFERENCE)",kind?name+".sample_compare("+state+",(UV),(REFERENCE))":"0.f");
		}else{
			define(slot==4?"ENGINE_REFLECTION_SAMPLE(DIR)":"ENGINE_SPECULAR_SAMPLE(DIR)",kind?
				name+".sample("+state+",(DIR),bias("+(slot==4?"u.lightSampler.y":"u.lightSampler.z")+")).rgb":"float3(0)");
		}
	}
	return source;
}
} // namespace rendering::metal4
