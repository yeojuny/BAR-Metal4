/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>
namespace rendering {
// Validate all six BC faces and their supplied mips before pinned nv_dds
// allocates. Face orientation and block/row flipping remain owned by nv_dds.
inline void ValidateNativeSkyDDS(std::span<const uint8_t> bytes)
{
	auto fail=[] { throw std::runtime_error("Unsupported or malformed original sky cube DDS"); };
	auto read=[&](size_t p) { if(p>bytes.size()||bytes.size()-p<4){fail();return uint32_t(0);}return uint32_t(bytes[p])|uint32_t(bytes[p+1])<<8|uint32_t(bytes[p+2])<<16|uint32_t(bytes[p+3])<<24; };
	if(bytes.size()<128||bytes.size()>384u*1024u*1024u||read(0)!=0x20534444||read(4)!=124||read(76)!=32)fail();
	const uint32_t size=read(16),levels=std::max(1u,read(28));
	if(size==0||size>16384||size!=read(12)||read(24)>1||(read(112)&0x00200000u)||!(read(80)&4u))fail();
	size_t offset=128,block=0;
	if(read(84)==0x30315844){
		if(bytes.size()<148||read(132)!=3||!(read(136)&4u)||read(140)!=1)fail();
		switch(read(128)){case 71:case 80:block=8;break;case 74:case 77:case 83:case 98:case 99:block=16;break;default:fail();}
		offset=148;
	}else{
		if((read(112)&0xfe00u)!=0xfe00u)fail();
		switch(read(84)){case 0x31545844:case 0x31495441:case 0x55344342:block=8;break;case 0x33545844:case 0x35545844:case 0x32495441:case 0x55354342:block=16;break;default:fail();}
	}
	uint32_t maximum=1;for(uint32_t n=size;n>1;n/=2)++maximum;if(levels>maximum)fail();
	size_t faceBytes=0;for(uint32_t n=size,i=0;i<levels;++i,n=std::max(1u,n/2))faceBytes+=size_t((n+3)/4)*((n+3)/4)*block;
	if(faceBytes>(bytes.size()-offset)/6||offset+faceBytes*6!=bytes.size())fail();
}
} // namespace rendering
