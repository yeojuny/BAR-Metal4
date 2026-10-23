/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/MapEdgeData.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace rendering::mapedge;
namespace {
unsigned checks=0;
void Check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void Reject(F action,const char* reason){bool failed=false;try{action();}catch(const std::exception&){failed=true;}Check(failed,reason);}
}
int main()
{
	try {
		const std::array<Mirror,2> left{{{1,0,-1,0},{0,0,0,0}}};
		Reject([&]{Submit({},left);},"submission outside original stage accepted");
		BeginFrame();Check(!Snapshot(),"new frame retained old draw");
		{WorldPreUnitScope stage;Reject([&]{WorldPreUnitScope nested;},"nested world stage accepted");Reject([&]{Snapshot();},"unclosed stage snapshot accepted");
			Reject([&]{BeginFrame();},"active stage frame reset accepted");Submit({},left);Reject([&]{Submit({},left);},"duplicate widget submission accepted");}
		auto retained=Snapshot();Check(retained&&retained->mirrors==std::vector<Mirror>(left.begin(),left.end()),"original ordered mirror tuples changed");
		BeginFrame();Check(!Snapshot()&&retained->mirrors.size()==2,"frame reset mutated retained immutable command");
		try{WorldPreUnitScope stage;throw std::runtime_error("callback");}catch(const std::exception&){}
		{WorldPreUnitScope stage;Submit({},{});}Check(Snapshot()&&Snapshot()->mirrors.empty(),"empty visible mirror set lost enabled packet");
		for(float brightness:{-2.f,0.f,.3f,2.f,128.f}){Parameters p;p.brightness=brightness;Validate(p,left);Check(true,"finite original brightness rejected");}
		for(float brightness:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){Parameters p;p.brightness=brightness;Reject([&]{Validate(p,left);},"nonfinite brightness accepted");}
		Parameters bad;bad.gridSize=64;Reject([&]{Validate(bad,left);},"non-original forward grid accepted");
		for(auto mirrors:{std::vector<Mirror>{{1,0,-1,0}},std::vector<Mirror>{{0,0,0,0}},std::vector<Mirror>{{1,0,-1,0},{1,0,-1,0},{0,0,0,0}},std::vector<Mirror>{{0,0,-1,0},{0,0,0,0}},std::vector<Mirror>{{1,0,2,0},{0,0,0,0}}})
			Reject([&]{Validate({},mirrors);},"invalid mirror/seam packet accepted");
		std::vector<Mirror> all;for(float z:{-1.f,0.f,1.f})for(float x:{-1.f,0.f,1.f})if(x!=0||z!=0)all.push_back({float(x!=0),float(z!=0),x,z});all.push_back({});Validate({},all);Check(all.size()==9,"eight original neighbors and seam rejected");
		BeginFrame();{WorldPreUnitScope stage;auto invalid=all;invalid.front()[0]=std::numeric_limits<float>::infinity();Reject([&]{Submit({},invalid);},"nonfinite mirror accepted");Submit({},all);}
		Check(Snapshot()->mirrors==all,"invalid submit damaged later valid transaction");
		const auto priorSequence=Snapshot()->sequence;
		const auto beforeOtherFamily=rendering::NextNativeWorldDrawSequence();
		BeginFrame();{WorldPreUnitScope stage;Submit({},left);}
		const auto afterOtherFamily=rendering::NextNativeWorldDrawSequence();
		Check(priorSequence>0 && priorSequence<beforeOtherFamily && beforeOtherFamily<Snapshot()->sequence && Snapshot()->sequence<afterOtherFamily,
			"map-edge original PreUnit order lost across other native families or frame reset");
		std::cout<<"MapEdgeData: "<<checks<<" checks passed\n";return 0;
	}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
