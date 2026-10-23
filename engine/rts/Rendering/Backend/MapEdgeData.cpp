/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "MapEdgeData.h"
#include "NativeWorldDrawSequence.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rendering::mapedge {
namespace {
std::shared_ptr<const DrawPacket> packet;
bool recording=false;
bool frameStarted=false;
void Require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
}
void BeginFrame()
{
	Require(!recording,"Native map-edge frame reset during DrawWorldPreUnit");
	packet.reset();frameStarted=true;
}
WorldPreUnitScope::WorldPreUnitScope()
{
	Require(frameStarted&&!recording,"Native map-edge invalid/nested world recording stage");recording=true;
}
WorldPreUnitScope::~WorldPreUnitScope(){recording=false;}
void Validate(Parameters parameters,std::span<const Mirror> mirrors)
{
	Require(parameters.gridSize==(parameters.deferred?64:32),"Native map-edge grid differs from its original forward/deferred caller");
	Require(std::isfinite(parameters.brightness),"Native map-edge nonfinite effective brightness");
	Require(mirrors.size()<=8,"Native map-edge mirror count outside current31657 bounds");
	const auto& mesh=parameters.mesh;
	Require(mesh[0] && mesh[0]<=2048 && mesh[1] && mesh[1]<=2048 && mesh[2] && mesh[2]<=mesh[0] &&
		mesh[3] && mesh[3]<=mesh[1] && mesh[4] && uint64_t(mesh[4])<=2ull*mesh[0]*mesh[1]+4ull*std::max(mesh[0],mesh[1]),
		"Native map-edge lacks its actual current31657 strip mesh tuple");
	for(size_t i=0;i<mirrors.size();++i) {
		const auto& m=mirrors[i];
		for(float value:m)Require(std::isfinite(value),"Native map-edge nonfinite mirror parameter");
		Require((m[2]==-1||m[2]==0||m[2]==1)&&(m[3]==-1||m[3]==0||m[3]==1)&&
			(m[2]!=0||m[3]!=0)&&m[0]==float(m[2]!=0)&&m[1]==float(m[3]!=0),"Native map-edge invalid original mirror tuple");
		const auto previous=mirrors.first(i);
		Require(std::find(previous.begin(),previous.end(),m)==previous.end(),"Native map-edge duplicate mirror tuple");
	}
}
void Submit(Parameters parameters,std::span<const Mirror> mirrors)
{
	Require(recording,"Native map-edge submission requires original DrawWorldPreUnit stage");
	Require(!parameters.deferred,"Deferred map-edge must belong to its actual Ground Map MRT callback");
	Require(!packet,"Native map-edge duplicate submission in one frame");
	Validate(parameters,mirrors);
	auto next=std::make_shared<DrawPacket>();next->parameters=parameters;next->mirrors.assign(mirrors.begin(),mirrors.end());
	next->sequence=NextNativeWorldDrawSequence();packet=std::move(next);
}
std::shared_ptr<const DrawPacket> Snapshot()
{
	Require(!recording,"Native map-edge snapshot requested before world stage closes");return packet;
}
} // namespace rendering::mapedge
