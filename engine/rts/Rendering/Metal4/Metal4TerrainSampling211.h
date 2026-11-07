/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace rendering::metal4 {
// A fixed, bounded policy per original program; never a runtime swizzle bitset.
enum class SMFSamplerPolicy211 : std::uint8_t { Generic, Canonical, NormalsOnly };

template<class ImageUniform>
inline bool NativeSMFCanonicalImage211(const ImageUniform& image,bool normals) {
	return image.flags.x==0&&image.flags.y==0&&image.parameters.x==0.0f&&
		image.swizzle.x==0x1903u&&image.swizzle.y==0x1904u&&
		image.swizzle.z==(normals?0x1904u:0x1905u)&&
		image.swizzle.w==(normals?0x1904u:0x1906u);
}

struct SMFSamplerSelection211 {
	const bool supported;
	const int normalSlot;
	bool allCanonical,normalCanonical=false,sawColor=false;
	explicit SMFSamplerSelection211(bool sourceSupported,int namedNormalSlot):
		supported(sourceSupported),normalSlot(namedNormalSlot),allCanonical(sourceSupported) {}
	template<class ImageUniform>
	void ObserveColor(const ImageUniform& image,unsigned slot) {
		if(!supported)return;
		sawColor=true;
		const bool normals=int(slot)==normalSlot;
		const bool canonical=NativeSMFCanonicalImage211(image,normals);
		allCanonical&=canonical;
		if(normals)normalCanonical=canonical;
	}
	SMFSamplerPolicy211 Select() const {
		if(supported&&sawColor&&allCanonical)return SMFSamplerPolicy211::Canonical;
		if(supported&&normalSlot>=0&&normalCanonical)return SMFSamplerPolicy211::NormalsOnly;
		return SMFSamplerPolicy211::Generic;
	}
};

inline constexpr std::string_view NativeSMFColorHelper211=R"SMF211(float4 SMFSample2D(texture2d<float> color,sampler state,
	constant SMFImage& image,float2 uv) {
	if(image.flags.y)uv.y=1-uv.y;
	float4 value=color.sample(state,uv,bias(image.parameters.x));
	return SMFSwizzle(value,image);
}
)SMF211";
inline constexpr std::string_view NativeSMFSampleMacro211=
	"#define SMF_SAMPLE(NAME,UV) SMFSample2D(NAME,NAME##_sampler,u.images[SLOT_##NAME],UV)";
inline constexpr std::string_view NativeSMFNormalConsumer211=
	"#define SMF_NORMAL(UV) float2(SMF_SAMPLE(normalsTex,UV).ra)";
inline bool NativeSMFUnique211(std::string_view source,std::string_view needle) {
	const auto at=source.find(needle);
	return at!=std::string_view::npos&&source.find(needle,at+needle.size())==std::string_view::npos;
}
inline bool NativeSMFSamplerSourceSupported211(std::string_view source) {
	return NativeSMFUnique211(source,NativeSMFColorHelper211)&&
		NativeSMFUnique211(source,NativeSMFSampleMacro211)&&
		NativeSMFUnique211(source,NativeSMFNormalConsumer211)&&
		source.find("NATIVE_SMF_SAMPLING211")==std::string_view::npos;
}
inline constexpr std::string_view NativeSMFSpecializedHelpers211=R"SMF211(
float4 SMFSampleIdentity211(texture2d<float> color,sampler state,
	constant SMFImage& image,float2 uv) {
	return color.sample(state,uv,bias(0.0f));
}
float4 SMFSampleNormals211(texture2d<float> color,sampler state,
	constant SMFImage& image,float2 uv) {
	float4 value=color.sample(state,uv,bias(0.0f));
	return float4(value.r,value.g,value.g,value.g);
}
)SMF211";

// Helpers are bound by immutable source sampler NAME, never source texture unit.
// Generic color/depth, cube/shadow and the original .ra consumer stay intact.
inline std::optional<std::string> NativeSMFSpecializeFragment211(
	std::string source,std::string_view helperDefinitions,SMFSamplerPolicy211 policy) {
	if(policy==SMFSamplerPolicy211::Generic)return source;
	if((policy!=SMFSamplerPolicy211::Canonical&&policy!=SMFSamplerPolicy211::NormalsOnly)||
		!NativeSMFSamplerSourceSupported211(source))return std::nullopt;
	std::string replacement(NativeSMFSpecializedHelpers211);
	replacement+=helperDefinitions;
	replacement+="#define SMF_SAMPLE(NAME,UV) NATIVE_SMF_SAMPLE211_##NAME(NAME,NAME##_sampler,u.images[SLOT_##NAME],UV)";
	source.replace(source.find(NativeSMFSampleMacro211),NativeSMFSampleMacro211.size(),replacement);
	return "#define NATIVE_SMF_SAMPLING211 "+std::to_string(unsigned(policy))+"\n"+source;
}
} // namespace rendering::metal4
