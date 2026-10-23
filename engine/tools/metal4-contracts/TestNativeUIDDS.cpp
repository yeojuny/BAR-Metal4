/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Textures/NativeUIDDS.h"

#include <fstream>
#include <iostream>
#include <iterator>

using namespace rendering::ui;
namespace {
unsigned checks=0;
void Check(bool value, const char* reason) { ++checks; if (!value) throw std::runtime_error(reason); }
void Put(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
	for (unsigned n=0;n<4;++n) bytes[offset+n]=std::uint8_t(value>>(n*8));
}
template<class F> void Reject(F action, const char* reason)
{
	bool failed=false; try { action(); } catch (const std::exception&) { failed=true; } Check(failed,reason);
}
std::vector<std::uint8_t> Read(const char* path)
{
	std::ifstream input(path,std::ios::binary); Check(bool(input),"actual DDS file missing");
	return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
std::vector<std::uint8_t> Synthetic(std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t levels=1)
{
	std::vector<std::uint8_t> bytes(128); Put(bytes,0,0x20534444); Put(bytes,4,124); Put(bytes,8,0x1007);
	Put(bytes,12,height); Put(bytes,16,width); Put(bytes,28,levels); Put(bytes,76,32); Put(bytes,80,4); Put(bytes,84,format); Put(bytes,108,0x1000);
	for (unsigned i=0;i<levels;++i) {
		bytes.resize(bytes.size()+std::size_t((width+3)/4)*((height+3)/4)*(format==0x31545844?8:16));
		width=std::max(1u,width/2);height=std::max(1u,height/2);
	}
	return bytes;
}
std::vector<std::uint8_t> Flat(std::uint32_t bits, std::uint32_t width, std::uint32_t height, std::uint32_t levels=1)
{
	std::vector<std::uint8_t> bytes(128); Put(bytes,0,0x20534444); Put(bytes,4,124); Put(bytes,8,0x100f);
	Put(bytes,12,height); Put(bytes,16,width); Put(bytes,20,width*(bits/8)); Put(bytes,28,levels);
	Put(bytes,76,32); Put(bytes,80,bits==32?0x41:0x40); Put(bytes,88,bits);
	Put(bytes,92,0xff0000); Put(bytes,96,0xff00); Put(bytes,100,0xff); Put(bytes,104,bits==32?0xff000000:0);
	Put(bytes,108,0x1000);
	for (unsigned i=0;i<levels;++i) {
		bytes.resize(bytes.size()+std::size_t(width)*height*(bits/8));
		width=std::max(1u,width/2); height=std::max(1u,height/2);
	}
	return bytes;
}
void FlatCases()
{
	auto bytes=Flat(32,4,3,3); auto layout=ValidateNativeUIDDS(bytes);
	Check(layout.format==NativeDDSFormat::BGRA8 && layout.bitsPerPixel==32 && layout.channelMasks==std::array<std::uint32_t,4>{0xff0000,0xff00,0xff,0xff000000},"BGRA format/mask metadata lost");
	Check(layout.mips.size()==3 && layout.mips[0].bytesPerRow==16 && layout.mips[0].byteCount==48 &&
		layout.mips[1].width==2 && layout.mips[1].height==1 && layout.mips[1].bytesPerRow==8 &&
		layout.mips[2].offset==184 && layout.mips[2].byteCount==4,"BGRA complete tight mip geometry lost");
	Put(bytes,80,0x40); Put(bytes,104,0);
	layout=ValidateNativeUIDDS(bytes); Check(layout.format==NativeDDSFormat::BGRA8 && layout.bitsPerPixel==32,"original RGB32 four-component contract lost");
	bytes=Flat(24,3,2,2); layout=ValidateNativeUIDDS(bytes);
	Check(layout.format==NativeDDSFormat::BGR8 && layout.bitsPerPixel==24 && layout.channelMasks[3]==0,"BGR24 metadata differs");
	Check(layout.mips[0].bytesPerRow==9 && layout.mips[0].byteCount==18 && layout.mips[1].offset==146 && layout.mips[1].byteCount==3,"BGR unaligned rows/tail mip lost");
	const std::vector<std::uint8_t> bgr{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18};
	const std::vector<std::uint8_t> expected{3,2,1,255,6,5,4,255,9,8,7,255,12,11,10,255,15,14,13,255,18,17,16,255};
	Check(ExpandNativeUIBGR8(bgr,3,2)==expected,"BGR conversion changed channels/alpha or flipped rows");
	Check(ExpandNativeUIBGR8(std::span(bgr).subspan(15),1,1)==std::vector<std::uint8_t>({18,17,16,255}),"BGR 1x1 tail conversion differs");
	Reject([&]{ExpandNativeUIBGR8(bgr,0,2);},"zero BGR dimensions accepted");
	Reject([&]{ExpandNativeUIBGR8(bgr,16385,2);},"unbounded BGR conversion accepted");
	Reject([&]{ExpandNativeUIBGR8(std::span(bgr).first(17),3,2);},"truncated BGR conversion accepted");
	bytes=Flat(24,3,2); Put(bytes,20,12); Reject([&]{ValidateNativeUIDDS(bytes);},"padded BGR pitch silently accepted by tight loader");
	bytes=Flat(24,3,2); Put(bytes,8,0x81007); Put(bytes,20,18);
	Check(ValidateNativeUIDDS(bytes).mips[0].byteCount==18,"original LINEARSIZE uncompressed base failed");
	Put(bytes,20,9); Reject([&]{ValidateNativeUIDDS(bytes);},"invalid uncompressed LINEARSIZE accepted");
	bytes=Flat(32,2,2); Put(bytes,92,0xff); Put(bytes,100,0xff0000);
	Reject([&]{ValidateNativeUIDDS(bytes);},"nonoriginal RGBA channel-mask order silently accepted");
	bytes=Flat(32,2,2); Put(bytes,104,0); Reject([&]{ValidateNativeUIDDS(bytes);},"alpha flag without alpha mask accepted");
	bytes=Flat(24,2,2); Put(bytes,104,0xff000000); Reject([&]{ValidateNativeUIDDS(bytes);},"BGR24 alpha mask accepted");
	bytes=Flat(24,2,2); Put(bytes,80,0x41); Reject([&]{ValidateNativeUIDDS(bytes);},"unimplemented BGRA24 accepted");
	bytes=Flat(24,2,2); Put(bytes,84,0x31545844); Reject([&]{ValidateNativeUIDDS(bytes);},"uncompressed flags with FourCC accepted");
	bytes=Flat(32,4,4,3); bytes.pop_back(); Reject([&]{ValidateNativeUIDDS(bytes);},"truncated BGRA tail mip accepted");
	bytes=Flat(32,4,4,3); bytes.push_back(0); Reject([&]{ValidateNativeUIDDS(bytes);},"undeclared flat payload accepted");
}
void ActualFlat(const char* path)
{
	const auto bytes=Read(path); const auto layout=ValidateNativeUIDDS(bytes);
	Check(layout.format==NativeDDSFormat::BGRA8 || layout.format==NativeDDSFormat::BGR8,"actual buildpic is not expected flat BGR/BGRA");
	std::size_t end=128;
	for (const auto& mip: layout.mips) {
		const auto surface=std::span(bytes).subspan(mip.offset,mip.byteCount);
		const auto components=layout.bitsPerPixel/8;
		Check(mip.offset==end && mip.bytesPerRow==std::size_t(mip.width)*components && mip.byteCount==mip.bytesPerRow*mip.height,"actual flat mip offsets/pitch differ");
		if (layout.format==NativeDDSFormat::BGR8) {
			const auto converted=ExpandNativeUIBGR8(surface,mip.width,mip.height);
			bool equal=converted.size()==std::size_t(mip.width)*mip.height*4;
			for (std::size_t i=0;i<surface.size()/3;++i)
				equal=equal && converted[i*4]==surface[i*3+2] && converted[i*4+1]==surface[i*3+1] && converted[i*4+2]==surface[i*3] && converted[i*4+3]==255;
			Check(equal,"actual BGR pixel/channel/orientation conversion differs");
		}
		end+=mip.byteCount;
	}
	Check(end==bytes.size(),"actual flat mip payload lost");
}
}
int main(int argc,char** argv)
{
	try {
		Check(argc>=3,"usage: TestNativeUIDDS actual-indicator.dds actual-glow.dds [actual-flat-buildpic.dds ...]");
		auto indicator=Read(argv[1]); auto layout=ValidateNativeUIDDS(indicator);
		Check(indicator.size()==2432 && layout.format==NativeDDSFormat::BC3 && layout.mips.size()==1,"actual indicator format/mips differ");
		Check(layout.mips[0].width==48 && layout.mips[0].height==48 && layout.mips[0].offset==128 && layout.mips[0].bytesPerRow==192 && layout.mips[0].byteCount==2304,"actual indicator block extent differs");
		auto glow=Read(argv[2]); layout=ValidateNativeUIDDS(glow);
		Check(glow.size()==1152 && layout.format==NativeDDSFormat::BC3 && layout.mips.size()==1 && layout.mips[0].width==32 && layout.mips[0].height==32 && layout.mips[0].byteCount==1024,"actual glow extent differs");
		for (auto code: {0x31545844u,0x33545844u,0x35545844u}) {
			auto bytes=Synthetic(code,5,7,3); layout=ValidateNativeUIDDS(bytes);
			Check(layout.mips.size()==3 && layout.mips[1].width==2 && layout.mips[1].height==3 && layout.mips[2].width==1 && layout.mips[2].height==1,"nonblock tail mip geometry lost");
			Check(layout.mips[2].offset+layout.mips[2].byteCount==bytes.size(),"complete mip payload not retained");
		}
		auto bytes=Synthetic(0x35545844,4,4);
		bytes.pop_back(); Reject([&]{ValidateNativeUIDDS(bytes);},"truncated payload accepted");
		bytes.push_back(0);bytes.push_back(0);Reject([&]{ValidateNativeUIDDS(bytes);},"trailing payload accepted");
		bytes=Synthetic(0x35545844,4,4);Put(bytes,16,0);Reject([&]{ValidateNativeUIDDS(bytes);},"zero width accepted");
		Put(bytes,16,16385);Reject([&]{ValidateNativeUIDDS(bytes);},"unbounded dimension accepted");
		bytes=Synthetic(0x35545844,4,4);Put(bytes,28,4);Reject([&]{ValidateNativeUIDDS(bytes);},"excess mip count accepted");
		bytes=Synthetic(0x35545844,4,4);Put(bytes,112,0x200);Reject([&]{ValidateNativeUIDDS(bytes);},"cubemap accepted");
		bytes=Synthetic(0x35545844,4,4);Put(bytes,8,0x801007);Reject([&]{ValidateNativeUIDDS(bytes);},"volume accepted");
		bytes=Synthetic(0x35545844,4,4);Put(bytes,80,0x41);Reject([&]{ValidateNativeUIDDS(bytes);},"invalid uncompressed format accepted");
		bytes=Synthetic(0x35545844,4,4);Put(bytes,84,0x37434220);Reject([&]{ValidateNativeUIDDS(bytes);},"unimplemented compression accepted");
		bytes=Synthetic(0x35545844,4,4);Put(bytes,4,123);Reject([&]{ValidateNativeUIDDS(bytes);},"invalid header size accepted");
		bytes=Synthetic(0x35545844,4,4);Put(bytes,8,0);Reject([&]{ValidateNativeUIDDS(bytes);},"missing mandatory flags accepted");
		bytes=Synthetic(0x35545844,4,4);Put(bytes,84,0x30315844);bytes.insert(bytes.begin()+128,20,0);
		Put(bytes,128,77);Put(bytes,132,3);Put(bytes,140,1);
		layout=ValidateNativeUIDDS(bytes);Check(layout.format==NativeDDSFormat::BC3 && layout.mips[0].offset==148,"original supported DX10 BC3 failed");
		Put(bytes,140,2);Reject([&]{ValidateNativeUIDDS(bytes);},"DX10 array accepted");
		Put(bytes,140,1);Put(bytes,132,4);Reject([&]{ValidateNativeUIDDS(bytes);},"DX10 3D accepted");
		Put(bytes,132,3);Put(bytes,128,78);Reject([&]{ValidateNativeUIDDS(bytes);},"unsupported DX10 sRGB conversion silently accepted");
		bytes.resize(100);Reject([&]{ValidateNativeUIDDS(bytes);},"truncated header accepted");
		FlatCases();
		for (int n=3;n<argc;++n) ActualFlat(argv[n]);
		std::cout << "NativeUIDDS: " << checks << " checks passed; actual indicator48x48BC3 + glow32x32BC3, " << argc-3 << " original flat buildpics\n";
		return 0;
	} catch (const std::exception& error) { std::cerr << "NativeUIDDS: " << error.what() << '\n'; return 1; }
}
