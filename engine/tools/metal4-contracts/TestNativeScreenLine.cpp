/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeScreenLine.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace rendering;
namespace {
unsigned checks=0;
void Check(bool v,const char* m){++checks;if(!v)throw std::runtime_error(m);}
std::vector<unsigned> Pixels(std::span<const ScreenLine> lines) {
	ScreenDrawList list;ScreenDrawState state;state.blend.enabled=false;AppendNativeScreenLines(list,lines,state,32,32);
	std::vector<unsigned> out(1024);
	for(size_t i=0;i<list.Vertices().size();i+=4){const auto& v=list.Vertices()[i];Check(v.x>=0&&v.y>=0&&v.x<32&&v.y<32,"output exceeds viewport");++out[size_t(v.y)*32+size_t(v.x)];}
	return out;
}
void Run() {
	ScreenLine line{{2.5f,8.5f,0,{1,0,0,1}},{18.5f,8.5f,0,{0,0,1,1}},1};
	for(float width:{1.f,1.5f,2.5f,3.f}){
		line.width=width;const auto pixels=Pixels({&line,1});const int w=int(std::floor(width+.5f));
		for(int y=0;y<32;++y)for(int x=0;x<32;++x){const int lower=8-(w/2);Check(pixels[y*32+x]==unsigned(x>=2&&x<18&&y>=lower&&y<lower+w),"independent horizontal half-open/rounded-width oracle");}
	}
	line.width=1;line.stippled=true;line.pattern=0xa55a;
	for(int factor:{0,1,2,256,500}){line.factor=factor;const auto pixels=Pixels({&line,1});for(int x=2;x<18;++x)Check(pixels[8*32+x]==((line.pattern>>((x-2)/std::clamp(factor,1,256)))&1u),"independent stipple factor/mask oracle");}
	line.stippled=false;line.start={1e6f,8.5f,0,{1,1,1,1}};line.end={-1e6f,8.5f,0,{1,1,1,1}};
	const auto huge=Pixels({&line,1});Check(std::count(huge.begin(),huge.end(),1u)==32,"large finite clipped line did not cover exact32 visible cells");
	line.start=line.end={8.5f,8.5f,0,{1,1,1,1}};auto zero=Pixels({&line,1});Check(std::all_of(zero.begin(),zero.end(),[](unsigned v){return v==0;}),"zero-length line generated a point");
	for(float bad:{0.f,-1.f,65.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){line.width=bad;bool threw=false;try{Pixels({&line,1});}catch(const std::exception&){threw=true;}Check(threw,"invalid width was accepted");}
	line.width=1;line.start.x=std::numeric_limits<float>::quiet_NaN();bool threw=false;try{Pixels({&line,1});}catch(const std::exception&){threw=true;}Check(threw,"nonfinite endpoint was accepted");
	line.start.x=0;ScreenDrawState state;state.material=ScreenMaterial::RGBA;ScreenDrawList list;threw=false;try{AppendNativeScreenLines(list,{&line,1},state,32,32);}catch(const std::exception&){threw=true;}Check(threw&&list.Commands().empty(),"unsupported textured line was not transactional");
}
}
int main(){try{Run();std::cout<<checks<<" native line CPU checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
