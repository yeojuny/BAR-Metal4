/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeUIRecorder.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace rendering;
using namespace rendering::ui;
namespace {
unsigned checks=0;
void Check(bool value,const char* reason) { ++checks; if (!value) throw std::runtime_error(reason); }
bool Near(float a,float b) { return std::abs(a-b)<0.0001f; }
template<class F> void Reject(F action,const char* reason)
{
	bool failed=false;try { action(); } catch(const std::exception&) { failed=true; } Check(failed,reason);
}
std::shared_ptr<const ScreenTexture> Initial(std::uint64_t id,unsigned w,unsigned h)
{
	auto identity=std::make_shared<ScreenRenderTargetIdentity>(); identity->resourceId=id;identity->width=w;identity->height=h;
	auto texture=std::make_shared<ScreenTexture>();texture->renderTarget=identity;texture->width=w;texture->height=h;texture->flipV=true;
	return texture;
}
std::shared_ptr<const ScreenTexture> Next(std::shared_ptr<const ScreenTexture> previous)
{
	auto texture=std::make_shared<ScreenTexture>(*previous);++texture->generation;return texture;
}
void Quad(NativeUIRecorder& r)
{
	r.Begin(ScreenPrimitive::Quads);r.Vertex(0,0);r.Vertex(10,0);r.Vertex(10,20);r.Vertex(0,20);r.End();
}
void Pixels(NativeUIRecorder& r,float width,float height)
{
	r.MultMatrix(TranslationMatrix(-1,-1,0));r.MultMatrix(ScaleMatrix(2/width,2/height,0));
}
}
int main()
{
	try {
		NativeUIRecorder r;auto a=std::make_shared<NativeUITextureBinding>();auto b=std::make_shared<NativeUITextureBinding>();
		a->current=Initial(11,100,50);b->current=Initial(12,40,20);
		r.BeginList();r.SetTextureResource(a);Quad(r);auto cached=r.EndList();
		r.BeginFrame(800,600);r.BeginScreenStage();r.MultMatrix(TranslationMatrix(7,9,0));
		const auto callerMatrix=r.GetMatrix(NativeUIMatrixMode::ModelView);
		auto a1=Next(a->current),b1=Next(b->current);
		r.BeginTarget(a1);r.ClearTarget(ScreenColor{0,0,0,0});Pixels(r,100,50);r.Color({1,0,0,0.5f});Quad(r);
		const auto parentMatrix=r.GetMatrix(NativeUIMatrixMode::ModelView);
		r.BeginTarget(b1);r.ClearTarget(ScreenColor{0,0,0,0});Pixels(r,40,20);r.Color({0,0,1,0.75f});Quad(r);r.EndTarget();b->current=b1;
		Check(r.GetMatrix(NativeUIMatrixMode::ModelView)==parentMatrix,"nested target did not restore parent matrix");
		Check(r.CurrentState().color==ScreenColor{0,0,1,0.75f},"normal target incorrectly restored flowing current color");
		Reject([&]{r.SetTextureResource(a);},"active self target sampling accepted");
		Reject([&]{r.BeginTarget(Next(a1),a1);},"active target reentry accepted");
		r.SetTextureResource(b);Quad(r);r.SetTexture(nullptr);r.EndTarget();a->current=a1;
		Check(r.GetMatrix(NativeUIMatrixMode::ModelView)==callerMatrix,"target did not restore screen matrix");
		r.CallList(cached);auto frame=r.EndFrame();
		Check(frame.coordinateWidth==800 && frame.coordinateHeight==600,"target basis escaped into root frame");
		Check(frame.targetPasses.size()==2 && frame.targetPasses[0].destination==b1 && frame.targetPasses[1].destination==a1,"child target order/ownership lost");
		Check(frame.targetPasses[0].coordinateWidth==40 && frame.targetPasses[1].coordinateWidth==100,"nested target dimensions collapsed");
		Check(frame.targetPasses[0].load==ScreenTargetLoad::DontCare && !frame.targetPasses[0].previous,"initial target fabricated initialized previous contents");
		Check(frame.targetPasses[1].drawList->Commands().back().state.texture==b1,"parent sampled stale child generation");
		Check(frame.drawList->Commands()[0].state.texture==a1,"compiled resource binding retained creation generation");
		Check(Near(frame.drawList->Vertices()[0].x,7) && Near(frame.drawList->Vertices()[0].y,9),"screen basis after RTT differs");
		const auto& parentGeometry=*frame.targetPasses[1].drawList;
		Check(!parentGeometry.Commands()[0].state.blend.enabled && !parentGeometry.Commands()[0].state.texture && !parentGeometry.Commands()[0].state.alphaTest.enabled,"clear inherited blend/texture/alpha test");
		Check(Near(parentGeometry.Vertices()[4].x,0) && Near(parentGeometry.Vertices()[4].y,0) && Near(parentGeometry.Vertices()[6].x,10) && Near(parentGeometry.Vertices()[6].y,20),"original identity RTT plus R2t transform differed");
		auto order=OrderScreenRenderTargetPasses(frame);Check(order.size()==2 && order[0]==0 && order[1]==1,"producer dependency graph order differs");
		// No actual present yet: a no-drawable frame must not lose these producers.
		r.BeginFrame(800,600);r.BeginScreenStage();r.CallList(cached);frame=r.EndFrame();
		Check(frame.targetPasses.size()==2 && frame.drawList->Commands()[0].state.texture==a1,"skipped frame lost committed target producer");
		r.RetireCompletedFrame();

		r.BeginFrame(800,600);r.BeginScreenStage();r.CallList(cached);
		auto a2=Next(a1);r.BeginTarget(a2,a1);r.SetScissor(ScreenScissor{5,6,20,10});r.MultMatrix(TranslationMatrix(500,500,0));
		r.ClearTarget(ScreenColor{0,1,0,0.5f});r.EndTarget();a->current=a2;r.CallList(cached);frame=r.EndFrame();
		Check(frame.targetPasses.size()==1 && frame.targetPasses[0].previous==a1 && frame.targetPasses[0].load==ScreenTargetLoad::Load,"partial update discarded prior generation");
		const auto& clear=*frame.targetPasses[0].drawList;
		Check(clear.Commands()[0].state.scissor==ScreenScissor{5,6,20,10},"clear ignored original scissor");
		Check(Near(clear.Vertices()[0].x,0) && Near(clear.Vertices()[2].x,100) && Near(clear.Vertices()[2].y,50),"clear wrongly used caller matrix");
		Check(frame.drawList->Commands()[0].state.texture==a1 && frame.drawList->Commands()[1].state.texture==a2,"old/new immutable draw snapshots collapsed");
		Check(OrderScreenRenderTargetPasses(frame).size()==1,"persistent earlier frame source rejected by CPU graph");
		r.RetireCompletedFrame();

		r.BeginFrame(800,600);r.BeginScreenStage();r.Color({0.2f,0.3f,0.4f,0.6f});r.MultMatrix(TranslationMatrix(8,9,0));
		const auto before=r.CurrentState();const auto beforeMatrix=r.GetMatrix(NativeUIMatrixMode::ModelView);
		r.BeginTarget(Next(a2),a2);r.Color({1,0,0,0});r.ClearTarget(ScreenColor{1,0,0,1});r.Begin(ScreenPrimitive::Quads);r.Vertex(0,0);r.AbortTarget();
		Check(r.CurrentState().color==before.color && r.CurrentState().draw==before.draw && r.GetMatrix(NativeUIMatrixMode::ModelView)==beforeMatrix,"aborted callback leaked caller state");
		r.CallList(cached);frame=r.EndFrame();Check(frame.targetPasses.empty() && frame.drawList->Commands()[0].state.texture==a2,"failed target published incomplete generation");

		r.BeginFrame(800,600);r.BeginGenesisStage();r.BeginTarget(Next(a2),a2);r.SetScissor({});r.ClearTarget();r.EndTarget();r.EndGenesisStage();frame=r.EndFrame();
		Check(frame.drawList->Commands().empty() && frame.targetPasses.size()==1,"Genesis-only producers dropped with empty root");
		Check(frame.targetPasses[0].drawList->Vertices()[0].color==ScreenColor{0,1,0,0.5f},"failed callback/global clear state restoration differs");
		// Abort root submission after a committed nested producer. Its owned
		// generation must still have a real producer when the next frame submits.
		r.BeginFrame(800,600);r.BeginScreenStage();r.BeginTarget(Next(b1),b1);r.ClearTarget(ScreenColor{0,0,0,0});r.EndTarget();r.AbortFrame();
		r.BeginFrame(800,600);r.BeginScreenStage();frame=r.EndFrame();
		Check(frame.targetPasses.size()==2,"aborted root frame lost valid committed target generations");
		r.RetireCompletedFrame();r.BeginFrame(800,600);r.BeginScreenStage();frame=r.EndFrame();
		Check(frame.targetPasses.empty(),"successful actual-present ack retained producer records");
		r.RetireCompletedFrame();
		std::cout << "NativeUIRenderTargets: " << checks << " checks passed\n";return 0;
	} catch(const std::exception& error) { std::cerr << error.what() << '\n';return 1; }
}
