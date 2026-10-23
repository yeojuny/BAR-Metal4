/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Actual CBitmap grayscale operation, with independently decoded PNG input.
#include "Rendering/Textures/Bitmap.h"
#include "Rendering/Textures/NativeUITextureQualifiers.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "lib/tracy/profiler/src/stb_image.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks=0;
void Check(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
void TestKnownPixels()
{
	{CBitmap missing;missing.AllocDummy();Check(missing.GetMemSize()==4 && missing.GetRawMem()[0]==255 && missing.GetRawMem()[1]==0 && missing.GetRawMem()[2]==0 && missing.GetRawMem()[3]==255,"original red missing-image fallback was suppressed by HEADLESS");}
	const std::array<std::uint8_t,24> source{0,0,0,19,255,255,255,37,255,0,0,63,0,255,0,127,0,0,255,213,128,128,128,255};
	CBitmap bitmap(source.data(),6,1,4);bitmap.MakeGrayScale();
	// Pinned upstream byte grayscale first normalizes RGB, then converts luma
	// to uint32 before writing channels. Preserve that exact 0/1 quantization,
	// rather than substituting a more conventional 0..255 luma conversion.
	for(unsigned p=0;p<6;++p) {
		const auto expected=p==1?1:0;
		for(unsigned c=0;c<3;++c)Check(bitmap.GetRawMem()[p*4+c]==expected,"upstream byte grayscale quantization differs");
		Check(bitmap.GetRawMem()[p*4+3]==source[p*4+3],"grayscale changed alpha");
	}
}
void TestKnownTint()
{
	const std::array<std::uint8_t,16> source{255,123,50,17,254,127,1,0,1,2,3,255,255,255,255,73};
	const std::array<std::uint8_t,16> expected{76,77,75,17,76,80,1,0,0,1,4,255,76,160,255,73};
	CBitmap bitmap(source.data(),4,1,4);const float tint[3]{.3f,.63f,1.5f};bitmap.Tint(tint);
	for (unsigned n=0;n<expected.size();++n) Check(bitmap.GetRawMem()[n]==expected[n],"actual byte Tint multiply/truncate/clamp/alpha differs");
	const std::array<std::uint8_t,4> white{255,255,255,53};
	CBitmap ordered(white.data(),1,1,4),reversed(white.data(),1,1,4);
	const auto qualifiers=rendering::ui::ParseNativeUITextureQualifiers(":t2,2,2gr2,3:icon.png");
	// Qualifier letter order does not change original NamedTextures operation
	// order: resize, then grayscale, then Tint. Quantize at each original step.
	ordered=ordered.CreateRescaled(qualifiers.resize->first,qualifiers.resize->second);
	ordered.MakeGrayScale();ordered.Tint(qualifiers.tint->data());
	Check(ordered.xsize==2 && ordered.ysize==3,"original resize before color operations lost");
	for (unsigned p=0;p<6;++p) {
		for (unsigned c=0;c<3;++c) Check(ordered.GetRawMem()[p*4+c]==2,"grayscale-before-Tint quantization/order differs");
		Check(ordered.GetRawMem()[p*4+3]==53,"ordered transforms changed alpha");
	}
	reversed.Tint(qualifiers.tint->data());reversed.MakeGrayScale();
	Check(reversed.GetRawMem()[0]==1 && reversed.GetRawMem()[0]!=ordered.GetRawMem()[0],"transform-order negative control is not discriminating");
}
void TestActualGroupTint(const char* path)
{
	int w=0,h=0,channels=0;auto* pixels=stbi_load(path,&w,&h,&channels,4);
	Check(pixels!=nullptr && w>0 && h>0,"actual original group icon decode failed");
	for (const auto factor: {.3f,.63f}) {
		CBitmap bitmap(pixels,w,h,4);const float tint[3]{factor,factor,factor};bitmap.Tint(tint);
		for(int p=0;p<w*h;++p) {
			for(unsigned c=0;c<3;++c) {
				const auto expected=static_cast<std::uint32_t>(float(pixels[p*4+c])*factor);
				Check(bitmap.GetRawMem()[p*4+c]==expected,"actual original group icon Tint byte differs");
			}
			Check(bitmap.GetRawMem()[p*4+3]==pixels[p*4+3],"actual original group icon Tint alpha differs");
		}
	}
	stbi_image_free(pixels);
}
}
int main(int argc,char** argv)
{
	if(argc<2) {std::cerr<<"Usage: native-ui-bitmap <verified actual rgbnoise.png> [verified actual group-icon.png ...]\n";return 1;}
	try {
		CBitmap::InitPool(0);TestKnownPixels();TestKnownTint();int w=0,h=0,channels=0;
		auto* pixels=stbi_load(argv[1],&w,&h,&channels,4);
		Check(pixels!=nullptr && w>0 && h>0,"actual noise PNG decode failed");
		unsigned zeros=0,ones=0;
		{
			CBitmap bitmap(pixels,w,h,4);bitmap.MakeGrayScale();
			for(int p=0;p<w*h;++p) {
				const auto luma=(float(pixels[p*4])/255.f)*.299f+(float(pixels[p*4+1])/255.f)*.587f+(float(pixels[p*4+2])/255.f)*.114f;
				const auto expected=static_cast<unsigned>((256.f/255.f)*luma);
				for(unsigned c=0;c<3;++c)Check(bitmap.GetRawMem()[p*4+c]==expected,"actual noise grayscale pixel differs");
				Check(bitmap.GetRawMem()[p*4+3]==pixels[p*4+3],"actual noise alpha differs");
				zeros+=expected==0;ones+=expected==1;
			}
		}
		stbi_image_free(pixels);
		for(int n=2;n<argc;++n) TestActualGroupTint(argv[n]);
		CBitmap::KillPool();
		std::cout<<"NativeUIBitmap: "<<checks<<" checks passed; actual noise "<<w<<'x'<<h<<"; grayscale zero="<<zeros<<" one="<<ones<<"; original tinted group icons="<<argc-2<<'\n';return 0;
	} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
