/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace rendering {
// Bound the original nv_dds flat loader before it allocates. This does not
// decode/reinterpret blocks, masks, orientation, mip data or create pixels.
inline void ValidateNativeWorldDDS(std::span<const uint8_t> bytes)
{
	auto fail = [] { throw std::runtime_error("Unsupported or malformed original native-world DDS layout"); };
	auto read = [&](size_t p) -> uint32_t {
		if (p > bytes.size() || bytes.size()-p < 4) { fail(); return 0; }
		return uint32_t(bytes[p]) | uint32_t(bytes[p+1])<<8 | uint32_t(bytes[p+2])<<16 | uint32_t(bytes[p+3])<<24;
	};
	if (bytes.size() < 128 || bytes.size() > size_t(384)*1024*1024 || read(0)!=0x20534444 || read(4)!=124 || read(76)!=32) fail();
	uint32_t width=read(16),height=read(12),levels=std::max(1u,read(28));
	if (!width || !height || width>32768 || height>32768 || (read(112)&0x0020fe00u) || read(24)>1) fail();
	uint32_t maximum=1; for(uint32_t size=std::max(width,height);size>1;size/=2)++maximum;
	if(levels>maximum)fail();
	size_t cursor=128,block=0,components=0;
	const uint32_t flags=read(80),fourcc=read(84),bits=read(88);
	if(flags&4u){
		switch(fourcc){
			case 0x31545844: case 0x31495441: case 0x55344342: block=8;break; // DXT1 / ATI1 / BC4U
			case 0x33545844: case 0x35545844: case 0x32495441: case 0x55354342: block=16;break;
			case 0x30315844: // DX10: exactly the formats accepted by pinned nv_dds.
				if(bytes.size()<148 || read(132)!=3 || (read(136)&4u) || read(140)!=1)fail();
				switch(read(128)){
					case 71: case 80:block=8;break;
					case 74: case 77: case 83: case 98: case 99:block=16;break;
					default:fail();
				}cursor=148;break;
			default:fail();
		}
	}else if((flags==0x41u || flags==0x40u) && bits==32){
		if(read(92)!=0x00ff0000 || read(96)!=0x0000ff00 || read(100)!=0x000000ff || (flags==0x41u && read(104)!=0xff000000))fail();
		components=4;
	}else if(flags==0x40u && bits==24){
		if(read(92)!=0x00ff0000 || read(96)!=0x0000ff00 || read(100)!=0x000000ff)fail();components=3;
	}else if(bits==8){components=1;}else fail();
	for(uint32_t i=0;i<levels;++i){
		const size_t row=block?size_t((width+3)/4)*block:size_t(width)*components;
		const size_t count=row*(block?(height+3)/4:height);
		if(cursor>bytes.size() || count>bytes.size()-cursor)fail();cursor+=count;
		width=std::max(1u,width/2);height=std::max(1u,height/2);
	}
}
} // namespace rendering
