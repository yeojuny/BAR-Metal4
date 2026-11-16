/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Metal4/Metal4GroundDecalUploadPolicyG02.h"
#include <cstdio>
#include <cstring>
#include <limits>

using namespace rendering;
using namespace rendering::metal4;

int main()
{
	unsigned checks=0;
	auto check=[&](bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",why);return false;}return true;};
	// Vary every byte of the actual uniform type, including unusual floating
	// representations; copying the payload must not interpret or alter them.
	for(unsigned seed=0;seed<4096;++seed){
		std::array<unsigned char,sizeof(NativeGroundDecalUniforms)> bytes{};
		for(std::size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<unsigned char>((i*131+seed*17+(seed>>(i%12)))&255);
		NativeGroundDecalUniforms uniforms;
		std::memcpy(&uniforms,bytes.data(),bytes.size());
		const GroundDecalUploadG02 upload{uniforms};
		const auto* raw=reinterpret_cast<const unsigned char*>(&upload);
		if(!check(std::memcmp(raw,bytes.data(),bytes.size())==0,"all 368 uniform bytes unchanged"))return 1;
		if(!check(std::memcmp(raw+GroundDecalIndexOffsetG02,GroundDecalIndicesG01.data(),72)==0,"exact 36 index words at tail"))return 1;
		if(!check(raw+GroundDecalUploadBytesG02==reinterpret_cast<const unsigned char*>(upload.indices.data()+36),"logical upload excludes trailing host padding"))return 1;
	}
	for(std::uint64_t low=0;low<32;++low){
		for(std::uint64_t length=0;length<=456;++length){
			const auto address=0x1000+low;
			const bool expected=low%16==0&&length>=440;
			if(!check(GroundDecalUploadValidG02(address,length)==expected,"alignment and short/odd/oversized allocation boundary"))return 1;
		}
	}
	if(!check(!GroundDecalUploadValidG02(0,440),"null GPU address"))return 1;
	const auto max=std::numeric_limits<std::uint64_t>::max();
	for(std::uint64_t delta=0;delta<=512;++delta){
		const auto address=max-delta;
		const bool expected=address%16==0&&delta>=440;
		if(!check(GroundDecalUploadValidG02(address,440)==expected,"GPU address overflow boundary"))return 1;
	}
	for(bool enabled:{false,true})for(unsigned samples:{1u,2u,4u,8u})for(std::size_t count:{0u,1u,1000000u}){
		if(!check(GroundDecalIndexEligibleG01(enabled,samples,count)==(enabled&&samples==1&&count>0),"G01 eligibility retained"))return 1;
	}
	std::printf("PASS checks=%u payloads=4096 uniformBytes=368 indexOffset=%zu indexBytes=72 logicalUpload=%zu hostSize=%zu; no GPU execution\n",checks,GroundDecalIndexOffsetG02,GroundDecalUploadBytesG02,sizeof(GroundDecalUploadG02));
}
