/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeUIRecorder.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace rendering;
using namespace rendering::ui;
namespace {
unsigned checks=0;
void Check(bool value){++checks;if(!value)throw std::runtime_error("world contract check "+std::to_string(checks));}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected);}
void VertexTriangle(NativeUIRecorder& r){r.BeginLegacyPrimitive(NativeDrawPrimitive::Triangles);r.LegacyVertex(1,0,-2);r.LegacyVertex(0,1,-2);r.LegacyVertex(0,0,-2);r.EndLegacyPrimitive();}
}
int main()
{
	NativeUIRecorder r;
	auto& world=GetWorldDrawRecorder();world.Reset();
	// Compilation runs without any frame/target and retains relative commands.
	r.BeginList();r.MultMatrix(TranslationMatrix(2,0,0));VertexTriangle(r);auto translated=r.EndList();
	r.BeginList();r.UpdateWorldState([](auto& s){s.depthWrite=false;});r.BeginLegacyPrimitive(NativeDrawPrimitive::LineLoop);for(int i=0;i<4;++i)r.LegacyVertex(float(i),0,-2);r.EndLegacyPrimitive();auto outline=r.EndList();
	Check(!world.IsActive());
	auto projection=IdentityMatrix();projection[10]=-1;projection[11]=-1;projection[14]=-1;projection[15]=0;
	r.BeginFrame(100,100);world.BeginFrame();
	r.BeginWorldStage(NativeWorldDrawStage::PreUnit,IdentityMatrix(),projection,IdentityMatrix(),true);
	r.UpdateWorldState([](auto& s){s.depthTest=true;s.depthCompare=ScreenAlphaCompare::Greater;s.depthWrite=true;s.lineWidth=3;});
	r.PushMatrix();r.CallList(translated);r.PopMatrix();
	r.CallList(outline);
	// The list's one-field update must preserve the real caller comparison.
	Check(r.CurrentWorldState().depthTest);Check(r.CurrentWorldState().depthCompare==ScreenAlphaCompare::Greater);Check(!r.CurrentWorldState().depthWrite);
	r.Color({.2f,.4f,.6f,.8f});
	r.PushAttrib(Depth|Polygon|Stencil|Line|Point|ColorMask);
	r.UpdateWorldState([](auto& s){s.depthWrite=true;s.depthCompare=ScreenAlphaCompare::Never;s.cullEnabled=true;s.frontStencil.reference=64;s.lineWidth=2;s.pointSize=5;s.colorWrite[0]=false;});
	r.PopAttrib();Check(!r.CurrentWorldState().depthWrite);Check(r.CurrentWorldState().lineWidth==3);Check(r.CurrentWorldState().pointSize==1);Check(r.CurrentWorldState().colorWrite[0]);Check(!r.CurrentWorldState().cullEnabled);Check(r.CurrentWorldState().frontStencil.reference==0);
	r.EndWorldStage();Check(r.CurrentState().color==ScreenColor({.2f,.4f,.6f,.8f}));
	// Same retained triangle command can execute in a distinct original stage.
	r.BeginWorldStage(NativeWorldDrawStage::World,IdentityMatrix(),projection,IdentityMatrix(),true);r.PushMatrix();r.CallList(translated);r.PopMatrix();
	// Failed replay restores state and keeps previously committed valid draws.
	r.BeginList();r.Color({.9f,0,0,1});r.PopMatrix();r.PopMatrix();auto invalid=r.EndList();
	const auto before=r.CurrentState().color;Reject([&]{r.CallList(invalid);});Check(r.CurrentState().color==before);
	// GL primitive invalid-operation semantics: ignored nested Begin, inner End
	// closes the real primitive, subsequent outside Vertex and End are ignored.
	r.BeginLegacyPrimitive(NativeDrawPrimitive::Triangles);r.LegacyVertex(0,0,-2);r.BeginLegacyPrimitive(NativeDrawPrimitive::Lines);r.LegacyVertex(1,0,-2);r.LegacyVertex(0,1,-2);r.EndLegacyPrimitive();r.LegacyVertex(2,0,-2);r.EndLegacyPrimitive();Check(r.TakeLegacyInvalidOperation());
	r.EndWorldStage();
	auto packet=world.EndFrame();r.EndFrame();
	Check(packet->draws.size()==4);Check(packet->draws[0].stage==NativeWorldDrawStage::PreUnit);Check(packet->draws[2].stage==NativeWorldDrawStage::World);
	const auto& v=packet->draws[0].vertices[0];Check(v.position==std::array<float,4>({3,0,1,2})); // w is deliberately not divided.
	Check(packet->draws[0].indices==std::vector<std::uint32_t>({0,1,2}));
	Check(packet->draws[1].indices==std::vector<std::uint32_t>({0,1,1,2,2,3,3,0}));
	Check(packet->draws[1].world.depthCompare==ScreenAlphaCompare::Greater);Check(!packet->draws[1].world.depthWrite);Check(packet->draws[1].world.lineWidth==3);
	for(size_t i=1;i<packet->draws.size();++i)Check(packet->draws[i].sequence>packet->draws[i-1].sequence);
	// Original screen projection and strict perspective refusal remain intact.
	r.BeginFrame(100,100);r.BeginScreenStage();VertexTriangle(r);r.BeginLegacyPrimitive(NativeDrawPrimitive::Lines);r.LegacyVertex(1,1);r.LegacyVertex(10,10);r.EndLegacyPrimitive();
	r.MatrixMode(NativeUIMatrixMode::Projection);r.LoadMatrix(projection);r.MatrixMode(NativeUIMatrixMode::ModelView);Reject([&]{VertexTriangle(r);});r.AbortPrimitive();
	r.EndScreenStage();auto screen=r.EndFrame();Check(screen.drawList->Commands().size()==2);
	// Invalid native draws are rejected before append; active frame survives.
	world.BeginFrame();NativeWorldDrawVertex bad{{0,0,0,1},{1,1,1,1},{0,0}};bad.position[0]=std::numeric_limits<float>::quiet_NaN();Reject([&]{world.Record(NativeWorldDrawStage::World,NativeDrawPrimitive::Points,{&bad,1},{},{});});Check(world.EndFrame()->draws.empty());
	world.BeginFrame();Reject([&]{world.BeginFrame();});world.AbortFrame();Check(!world.IsActive());Reject([&]{world.EndFrame();});
	// Published geometry is owned and survives the next recorder frame.
	Check(packet->draws[0].vertices[0].position[0]==3);
	std::cout<<checks<<" native world CPU checks passed\n";
}
