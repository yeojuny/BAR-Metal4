/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include <IL/il.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
int main(int argc,char** argv)
{
	if(argc!=2)return 2;
	const std::string base=std::string(argv[1])+"/exact/bitmaps/projectiletextures/flare2";
	std::ifstream source(base+".tga",std::ios::binary),oracle(base+".rgba",std::ios::binary);
	const std::vector<unsigned char> input(std::istreambuf_iterator<char>(source),{}),expected(std::istreambuf_iterator<char>(oracle),{});
	ilInit();ilOriginFunc(IL_ORIGIN_UPPER_LEFT);ilEnable(IL_ORIGIN_SET);ILuint id=0;ilGenImages(1,&id);ilBindImage(id);
	if(!ilLoadL(IL_TYPE_UNKNOWN,input.data(),input.size())||!ilConvertImage(IL_RGBA,IL_UNSIGNED_BYTE))return 1;
	if(ilGetInteger(IL_IMAGE_WIDTH)!=512||ilGetInteger(IL_IMAGE_HEIGHT)!=512||expected.size()!=512*512*4)return 1;
	const auto* actual=ilGetData();unsigned checks=0;for(std::size_t i=0;i<expected.size();++i){++checks;if(actual[i]!=expected[i]){std::cerr<<"FAIL original flare channel "<<i<<'\n';return 1;}}
	ilDisable(IL_ORIGIN_SET);ilDeleteImages(1,&id);ilShutDown();std::cout<<"PASS "<<checks<<" original flare2 DevIL upper-origin RGBA byte checks\n";
}
