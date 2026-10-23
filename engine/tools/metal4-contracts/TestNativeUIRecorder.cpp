/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeUIRecorder.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace rendering;
using namespace rendering::ui;
namespace {
unsigned checks = 0;
void Check(bool value, const char* message)
{
	++checks;
	if (!value) throw std::runtime_error(message);
}
bool Near(float a, float b) { return std::abs(a-b) < 0.0001f; }
template<class F> void Fails(F action, const char* message)
{
	bool failed = false;
	try { action(); } catch (const std::exception&) { failed = true; }
	Check(failed, message);
}
void Quad(NativeUIRecorder& recorder)
{
	recorder.Begin(ScreenPrimitive::Quads);
	recorder.Vertex(0,0); recorder.Vertex(10,0); recorder.Vertex(10,20); recorder.Vertex(0,20);
	recorder.End();
}
std::shared_ptr<const ScreenTexture> Atlas()
{
	auto result = std::make_shared<ScreenTexture>();
	result->sourceName = "independent-test-coverage"; result->generation = 1;
	result->format = ScreenTextureFormat::R8Unorm;
	result->width = result->height = 4; result->bytesPerRow = 4; result->bytes.assign(16,128);
	return result;
}
std::shared_ptr<const ScreenTexture> Target(unsigned identity, unsigned width, unsigned height)
{
	auto result=std::make_shared<ScreenTexture>();result->width=width;result->height=height;result->generation=1;
	result->renderTarget=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{identity,width,height});
	return result;
}
void OrthographicClipDepth()
{
	NativeUIRecorder r;r.BeginFrame(800,600);r.BeginScreenStage();
	const auto outerView=r.GetMatrix(NativeUIMatrixMode::ModelView),outerProjection=r.GetMatrix(NativeUIMatrixMode::Projection);
	const auto parent=Target(1201,32,32),child=Target(1202,16,16);
	r.BeginTarget(parent);r.ClearTarget(ScreenColor{0,0,0,0});r.PushMatrix();
	// Exact AdvPlayersList CreateBackground callback operations (2099-2101):
	// LoadIdentity; Ortho(absLeft,absRight,absBottom,absTop,0,1000);
	// then FlowUI's first RectRound vertex (px+cs,py,0).
	r.LoadMatrix(IdentityMatrix());r.MultMatrix(OrthoMatrix(100,132,200,232,0,1000));
	const auto parentView=r.GetMatrix(NativeUIMatrixMode::ModelView),parentProjection=r.GetMatrix(NativeUIMatrixMode::Projection);
	r.Begin(ScreenPrimitive::Triangles);r.Vertex(103,200,0);r.Vertex(132,200,0);r.Vertex(103,232,0);r.End();
	r.BeginTarget(child);r.MultMatrix(OrthoMatrix(0,16,0,16,-10,10));
	r.Begin(ScreenPrimitive::Triangles);r.Vertex(0,0,-10);r.Vertex(16,0,10);r.Vertex(0,16,-20);r.End();r.EndTarget();
	Check(r.GetMatrix(NativeUIMatrixMode::ModelView)==parentView && r.GetMatrix(NativeUIMatrixMode::Projection)==parentProjection,"nested target lost caller orthographic matrices");
	r.Begin(ScreenPrimitive::Triangles);r.Vertex(100,200,-1000);r.Vertex(132,200,-1001);r.Vertex(100,232,1);r.End();
	r.AppendFontTriangles({{100,200,0,{1,1,1,1}},{101,200,0,{1,1,1,1}},{100,201,0,{1,1,1,1}}},{{0,0},{1,0},{0,1}},{0,1,2},Atlas(),false);
	r.PopMatrix();r.EndTarget();
	Check(r.GetMatrix(NativeUIMatrixMode::ModelView)==outerView && r.GetMatrix(NativeUIMatrixMode::Projection)==outerProjection,"target lost outer screen matrices");
	Quad(r);auto frame=r.EndFrame();
	Check(frame.targetPasses.size()==2 && frame.targetPasses[0].destination==child && frame.targetPasses[1].destination==parent,"nested orthographic producer ordering changed");
	const auto vertices=frame.targetPasses[1].drawList->Vertices();
	Check(Near(vertices[4].x,3) && Near(vertices[4].y,0) && vertices[4].z==-1,"original AdvPlayersList z0 did not reach GL near clip plane");
	Check(Near(vertices[7].z,1) && vertices[8].z>1 && vertices[9].z<-1,"far/outside clip depth was rejected, flattened or clamped");
	Check(vertices[10].z==-1,"font under original background Ortho did not reach near plane");
	const auto childVertices=frame.targetPasses[0].drawList->Vertices();
	Check(childVertices[0].z==1 && childVertices[1].z==-1 && childVertices[2].z==2,"nested target near/far depth was lost");
	Check(frame.drawList->Vertices()[0].z==0,"orthographic RTT depth leaked into later root geometry");
	r.RetireCompletedFrame();

	// Retained geometry must use the caller's current z transform at replay.
	r.BeginList();r.Begin(ScreenPrimitive::Triangles);r.Vertex(0,0,0);r.Vertex(1,0,0);r.Vertex(0,1,0);r.End();auto list=r.EndList();
	r.BeginFrame(32,32);r.BeginScreenStage();r.MultMatrix(TranslationMatrix(0,0,0.25f));r.CallList(list);
	r.MultMatrix(TranslationMatrix(0,0,2));r.CallList(list);frame=r.EndFrame();
	Check(frame.drawList->Vertices()[0].z==-0.25f && frame.drawList->Vertices()[3].z==-2.25f,"retained list baked or clipped depth before actual replay");

	// Font vertices follow the same transform, not a special flattened path.
	r.BeginFrame(32,32);r.BeginScreenStage();r.MultMatrix(TranslationMatrix(0,0,0.5f));
	r.AppendFontTriangles({{0,0,0,{1,1,1,1}},{1,0,0,{1,1,1,1}},{0,1,0,{1,1,1,1}}},{{0,0},{1,0},{0,1}},{0,1,2},Atlas(),false);
	frame=r.EndFrame();Check(frame.drawList->Vertices()[0].z==-0.5f,"font triangle depth was flattened");

	r.BeginFrame(32,32);r.BeginScreenStage();r.Begin(ScreenPrimitive::Triangles);
	Fails([&]{r.Vertex(1,2,0,0);},"zero homogeneous w accepted");
	Fails([&]{r.Vertex(1,2,0,2);},"unsupported homogeneous division accepted");
	Fails([&]{r.Vertex(1,2,std::numeric_limits<float>::infinity());},"nonfinite depth accepted");r.AbortPrimitive();
	auto perspective=IdentityMatrix();perspective[3]=0.25f;r.LoadMatrix(perspective);r.Begin(ScreenPrimitive::Triangles);
	Fails([&]{r.Vertex(4,2,0);},"perspective matrix changing w was accepted");r.AbortPrimitive();r.AbortFrame();
}
}
int main()
{
	try {
		OrthographicClipDepth();
		NativeUIRecorder r;
		r.BeginList(); r.Color({0.2f,0.3f,0.4f,0.375f}); Quad(r); auto child = r.EndList();
		Check(r.CurrentState().color == ScreenColor{1,1,1,1},"compilation leaked current color");
		r.BeginList(); r.PushMatrix(); r.MultMatrix(TranslationMatrix(2,3,0)); r.CallList(child); r.PopMatrix(); auto parent = r.EndList();
		child.reset();
		r.BeginFrame(800,600); r.BeginScreenStage();
		r.MultMatrix(TranslationMatrix(100,200,0)); r.MultMatrix(ScaleMatrix(2,3,1)); r.CallList(parent);
		r.MultMatrix(TranslationMatrix(20,0,0)); r.CallList(parent);
		auto frame = r.EndFrame(); const auto& list = *frame.drawList;
		Check(frame.coordinateWidth==800 && frame.coordinateHeight==600,"logical frame extent changed");
		Check(list.Vertices().size()==8 && list.Indices().size()==12 && list.Commands().size()==2,"retained geometry/order mismatch");
		Check(Near(list.Vertices()[0].x,104) && Near(list.Vertices()[0].y,209),"caller-relative nested matrix was baked during compile");
		Check(Near(list.Vertices()[2].x,124) && Near(list.Vertices()[2].y,269),"quad transform wrong");
		Check(Near(list.Vertices()[4].x,144) && Near(list.Vertices()[4].y,209),"second replay ignored caller matrix");
		Check(list.Vertices()[0].color==ScreenColor{0.2f,0.3f,0.4f,0.375f},"continuous alpha/color lost");
		Check(list.Indices()[0]==0 && list.Indices()[1]==1 && list.Indices()[2]==2 && list.Indices()[3]==0 && list.Indices()[4]==2 && list.Indices()[5]==3,"planar quad diagonal/order changed");

		// Original TopBar wind1 pushes/translates, the caller rotates, wind2
		// draws/pops. Neither retained list is independently balanced.
		r.BeginList(); r.PushMatrix(); r.MultMatrix(TranslationMatrix(50,70,0)); auto wind1=r.EndList();
		r.BeginList(); Quad(r); r.PopMatrix(); auto wind2=r.EndList();
		r.BeginFrame(800,600); r.BeginScreenStage(); const auto outer=r.GetMatrix(NativeUIMatrixMode::ModelView);
		r.PushMatrix(); r.CallList(wind1); r.MultMatrix(RotationMatrix(90,0,0,1)); r.CallList(wind2); r.PopMatrix();
		Check(r.GetMatrix(NativeUIMatrixMode::ModelView)==outer,"relative list stack effects did not restore outer caller");
		frame=r.EndFrame();
		Check(Near(frame.drawList->Vertices()[0].x,50) && Near(frame.drawList->Vertices()[0].y,70),"split wind list anchor differs");
		Check(Near(frame.drawList->Vertices()[2].x,30) && Near(frame.drawList->Vertices()[2].y,80),"rotation between split lists lost");
		r.BeginFrame(800,600); r.BeginScreenStage();
		Fails([&]{r.CallList(wind2);},"actual unprovided caller stack pop accepted"); r.AbortFrame();

		r.BeginFrame(800,600); r.BeginScreenStage();
		r.SetAlphaTest({true,ScreenAlphaCompare::Greater,2}); Quad(r);
		r.SetAlphaTest({true,ScreenAlphaCompare::Greater,-2}); Quad(r);
		Fails([&]{r.SetAlphaTest({true,ScreenAlphaCompare::Greater,std::numeric_limits<float>::quiet_NaN()});},"nonfinite alpha accepted");
		frame = r.EndFrame();
		Check(frame.drawList->Commands()[0].state.alphaTest.reference==1,"GL alpha reference upper clamp missing");
		Check(frame.drawList->Commands()[1].state.alphaTest.reference==0,"GL alpha reference lower clamp missing");

		r.BeginFrame(800,600); r.BeginScreenStage();
		auto blend = r.CurrentState().draw.blend; blend.enabled=false;
		blend.sourceColor=ScreenBlendFactor::One; blend.destinationColor=ScreenBlendFactor::One;
		r.SetBlend(blend);
		auto atlas = Atlas(); std::weak_ptr<const ScreenTexture> lifetime = atlas;
		r.BeginList();
		r.AppendFontTriangles({{1,2,0,{1,0.5f,0.25f,0.4f}},{3,2,0,{1,0.5f,0.25f,0.4f}},{1,4,0,{1,0.5f,0.25f,0.4f}}},{{0,0},{4,0},{0,4}},{0,1,2},atlas,true);
		auto fontList = r.EndList(); atlas.reset();
		Check(!lifetime.expired(),"retained atlas lifetime lost before replay");
		r.CallList(fontList); frame=r.EndFrame();
		const auto& fontState=frame.drawList->Commands()[0].state;
		Check(fontState.blend.enabled,"font caller blend did not enable blending");
		Check(fontState.blend.sourceColor==ScreenBlendFactor::One && fontState.blend.destinationColor==ScreenBlendFactor::One,"font caller factors overwritten");
		Check(fontState.material==ScreenMaterial::FontCoverage && fontState.uvMode==ScreenUVMode::Texel,"font coverage/texel material lost");
		Check(fontState.sampler.wrapS==ScreenWrap::TransparentBorder && !fontState.alphaTest.enabled,"font border/alpha state differs from original");
		Check(Near(frame.drawList->Vertices()[0].color[3],0.4f),"font alpha quantized by recorder");
		fontList.reset(); r.BeginFrame(800,600); r.BeginScreenStage();
		Check(lifetime.expired(),"atlas snapshot survived after list/frame retirement");

		r.PushMatrix(); r.BeginList(); r.PopMatrix(); auto underflow = r.EndList(); r.PopMatrix();
		const auto before = r.GetMatrix(NativeUIMatrixMode::ModelView);
		Fails([&]{r.CallList(underflow);},"relative list underflow accepted");
		Check(r.GetMatrix(NativeUIMatrixMode::ModelView)==before,"failed replay did not restore caller matrix state");
		r.BeginList(); r.Color({0,0,0,0}); r.Begin(ScreenPrimitive::Quads); r.AbortList();
		Check(r.CurrentState().color==ScreenColor{1,1,1,1},"aborted compile leaked state");
		Quad(r); frame=r.EndFrame(); Check(frame.drawList->Vertices().size()==4,"failed replay/compile damaged subsequent frame");
		r.BeginFrame(800,600); r.BeginGenesisStage();
		Fails([&]{Quad(r);},"Genesis geometry silently treated as screen");
		r.EndGenesisStage(); r.BeginScreenStage(); Quad(r); frame=r.EndFrame();
		Check(frame.drawList->Vertices().size()==4,"resource-only Genesis failure prevented next screen stage");
		std::cout << "NativeUIRecorder: " << checks << " checks passed\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "NativeUIRecorder: " << error.what() << '\n'; return 1;
	}
}
