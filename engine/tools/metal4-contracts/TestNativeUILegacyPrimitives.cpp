/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeUIRecorder.h"
#include <iostream>
#include <stdexcept>

using namespace rendering;
using namespace rendering::ui;
namespace {
unsigned checks=0;
void Check(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
template<class F> void Reject(F action,const char* reason) {bool failed=false;try{action();}catch(const std::exception&){failed=true;}Check(failed,reason);}
void Four(NativeUIRecorder& r) {r.LegacyVertex(0,0);r.LegacyVertex(10,0);r.LegacyVertex(10,10);r.LegacyVertex(0,10);}
}
int main()
{
	try {
		NativeUIRecorder r;r.BeginFrame(800,600);r.BeginScreenStage();
		r.BeginLegacyPrimitive(ScreenPrimitive::Quads);r.LegacyVertex(0,0);
		r.BeginLegacyPrimitive(ScreenPrimitive::Triangles); // ignored, original QUADS still active
		r.Color({.2f,.4f,.6f,.37f});r.LegacyVertex(10,0);r.LegacyVertex(10,10);r.LegacyVertex(0,10);
		r.EndLegacyPrimitive();r.LegacyVertex(500,500);r.EndLegacyPrimitive();
		auto frame=r.EndFrame();Check(frame.drawList->Vertices().size()==4 && frame.drawList->Indices().size()==6,"nested Begin changed mode or extra outside vertex leaked");
		Check(frame.drawList->Vertices()[0].color==ScreenColor{1,1,1,1} && frame.drawList->Vertices()[1].color==ScreenColor{.2f,.4f,.6f,.37f},"per-vertex current color changed across nested Begin");
		const std::array<unsigned,6> diagonal{0,1,2,0,2,3};for(unsigned n=0;n<6;++n)Check(frame.drawList->Indices()[n]==diagonal[n],"outer QUADS topology replaced by nested TRIANGLES");
		Check(r.LegacyInvalidOperationCount()==3 && r.TakeLegacyInvalidOperation() && !r.TakeLegacyInvalidOperation(),"nested/extra End/outside Vertex error state differs");
		r.BeginFrame(800,600);r.BeginScreenStage();r.BeginLegacyPrimitive(ScreenPrimitive::Quads);Four(r);r.LegacyVertex(99,99);r.EndLegacyPrimitive();
		r.BeginLegacyPrimitive(ScreenPrimitive::Triangles);Four(r);r.EndLegacyPrimitive();frame=r.EndFrame();
		Check(frame.drawList->Vertices().size()==7 && frame.drawList->Indices().size()==9,"incomplete final primitive removed complete groups or drew tail");
		Check(!r.TakeLegacyInvalidOperation(),"incomplete final primitive incorrectly generated GL error");
		const auto errorCount=r.LegacyInvalidOperationCount();
		r.BeginList();r.BeginLegacyPrimitive(ScreenPrimitive::Quads);r.BeginLegacyPrimitive(ScreenPrimitive::Triangles);Four(r);r.EndLegacyPrimitive();r.EndLegacyPrimitive();auto list=r.EndList();
		Check(r.LegacyInvalidOperationCount()==errorCount+2 && r.TakeLegacyInvalidOperation(),"invalid compile commands were not reported");
		r.BeginFrame(800,600);r.BeginScreenStage();r.CallList(list);frame=r.EndFrame();
		Check(frame.drawList->Vertices().size()==4 && frame.drawList->Indices().size()==6,"compiled collapsed BeginEnd lost valid geometry");
		Check(r.LegacyInvalidOperationCount()==errorCount+2 && !r.TakeLegacyInvalidOperation(),"invalid compile commands were replayed instead of ignored");
		r.BeginFrame(800,600);r.BeginScreenStage();r.Begin(ScreenPrimitive::Quads);
		Reject([&]{r.Begin(ScreenPrimitive::Quads);},"strict CPU nested Begin invariant weakened");r.AbortPrimitive();
		Reject([&]{r.End();},"strict CPU End invariant weakened");Reject([&]{r.Vertex(0,0);},"strict CPU Vertex invariant weakened");
		r.Begin(ScreenPrimitive::Quads);r.Vertex(0,0);Reject([&]{r.End();},"strict CPU incomplete primitive invariant weakened");r.AbortPrimitive();r.AbortFrame();
		std::cout<<"NativeUILegacyPrimitives: "<<checks<<" checks passed\n";return 0;
	} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
