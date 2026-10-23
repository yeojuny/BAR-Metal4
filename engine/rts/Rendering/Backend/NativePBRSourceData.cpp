/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativePBRSourceData.h"
#include "NativePBRSourceIdentity.h"
#include <charconv>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int32_t OriginalMacro(std::string_view original,std::string_view actual,std::string_view marker)
{
	const auto at=original.find(marker);Check(at!=std::string_view::npos,"Original PBR macro absent");
	const auto prefix=original.substr(0,at),suffix=original.substr(at+marker.size());
	Check(actual.size()>prefix.size()+suffix.size()&&actual.starts_with(prefix)&&actual.ends_with(suffix),"PBR full original fragment differs");
	const auto number=actual.substr(prefix.size(),actual.size()-prefix.size()-suffix.size());
	int32_t value=0;const auto result=std::from_chars(number.data(),number.data()+number.size(),value);
	Check(result.ec==std::errc{}&&result.ptr==number.data()+number.size(),"PBR original numeric macro invalid");
	return value;
}
}
NativePBRProgram ValidateNativePBRProgram(const NativePBRProgramSource& source)
{
	NativePBRProgram result;
	if(source.vertex==pbrsource::BRDFVS){
		Check(OriginalMacro(pbrsource::BRDFFS,source.fragment,"###G_OPTION###")==1,"Selected30770 BRDF G_OPTION differs");
		Check(source.initialInt.size()==1&&source.initialInt.contains("texSize"),"Original BRDF Initialize uniforms differ");
		const auto& size=source.initialInt.at("texSize");Check(size==std::vector<float>{512,512},"Selected30770 BRDF extent differs");
		result.kind=NativePBRKind::BRDF;
	}else{
		Check(source.vertex==pbrsource::EnvironmentVS,"PBR full original vertex differs");
		result.samples=OriginalMacro(pbrsource::EnvironmentFS,source.fragment,"###ENV_SMPL_NUM###");
		Check(source.initialInt.size()==1&&source.initialInt.contains("reflectTex")&&source.initialInt.at("reflectTex")==std::vector<float>{7},"Original environment Initialize sampler differs");
		result.kind=NativePBRKind::Environment;
	}
	return result;
}
void ValidateNativePBRAllocation(const NativePBRAllocation& a)
{
	Check((a.originalFormat==0x822F&&a.width==512&&a.height==512)||(a.originalFormat==0x881B&&a.width==3&&a.height==3),"Original PBR texture format/extent differs");
	const unsigned filter=a.originalFormat==0x822F?0x2601:0x2600;
	Check(a.sampling.minFilter==filter&&a.sampling.magFilter==filter&&a.sampling.wrapS==0x812F&&a.sampling.wrapT==0x812F&&a.sampling.compareMode==0,"Original PBR sampler differs");
}
std::shared_ptr<const NativePBRImage> GetNativePBRImage(const NativeCUSTextureBinding& binding)
{
	if(binding.knowledge!=NativeCUSBindingKnowledge::BoundObject||!binding.object)return {};
	return std::dynamic_pointer_cast<const NativePBRImage>(binding.object->backing);
}
} // namespace rendering
