/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Independently derived from pinned LuaOpenGL ResetGLState, AttribBits,
// Enable/DisableDrawInMiniMap, ResetMiniMapMatrices and RenderToTexture.
#include "Rendering/Backend/NativeUIRecorder.h"
#include <iostream>
#include <stdexcept>

using namespace rendering;
using namespace rendering::ui;
namespace {
unsigned checks=0,failures=0;
void Check(bool value,const char* why) {++checks;if(!value)throw std::runtime_error(why);}
template<class F> void Throws(F action,const char* why) {bool failed=false;try{action();}catch(const std::exception&){failed=true;}Check(failed,why);}
std::shared_ptr<const ScreenTexture> Target(unsigned id,unsigned w,unsigned h)
{
	auto image=std::make_shared<ScreenTexture>();image->width=w;image->height=h;image->generation=1;
	image->renderTarget=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{id,w,h});return image;
}
std::shared_ptr<const ScreenTexture> Image(const char* name)
{
	auto image=std::make_shared<ScreenTexture>();image->sourceName=name;image->width=image->height=1;image->bytesPerRow=4;image->bytes={1,2,3,4};return image;
}
const auto A=Image("original-before"),B=Image("original-callback");
const ScreenColor OUTER_COLOR{.2f,.3f,.4f,.5f},CALLBACK_COLOR{.7f,.6f,.5f,.4f},WHITE{1,1,1,1};
const ScreenTexCoord OUTER_UV{.25f,.75f},CALLBACK_UV{.8f,.2f};
const std::array<float,3> OUTER_NORMAL{2,3,4},CALLBACK_NORMAL{5,6,7};
const ScreenScissor OUTER_BOX{1,2,3,4},CALLBACK_BOX{9,8,7,6};
ScreenBlendState Blend(bool callback=false)
{
	ScreenBlendState b;b.enabled=callback;b.sourceColor=ScreenBlendFactor::One;b.destinationColor=ScreenBlendFactor::One;
	b.sourceAlpha=ScreenBlendFactor::One;b.destinationAlpha=ScreenBlendFactor::Zero;return b;
}
void Seed(NativeUIRecorder& r,bool callback=false)
{
	r.Color(callback?CALLBACK_COLOR:OUTER_COLOR);const auto uv=callback?CALLBACK_UV:OUTER_UV;r.TexCoord(uv[0],uv[1]);
	const auto n=callback?CALLBACK_NORMAL:OUTER_NORMAL;r.Normal(n[0],n[1],n[2]);
	r.SetTexture(callback?B:A,{ScreenFilter::Nearest,ScreenWrap::Repeat,ScreenWrap::Repeat});
	r.SetScissor(callback?CALLBACK_BOX:OUTER_BOX);r.SetBlend(Blend(callback));
	r.SetAlphaTest({true,ScreenAlphaCompare::NotEqual,callback?.61f:.37f});
}
void Current(NativeUIRecorder& r,ScreenColor color,ScreenTexCoord uv,std::array<float,3> normal)
{
	const auto s=r.CurrentState();Check(s.color==color,"current color differs from original GL reset/pop mask");
	Check(s.texCoord==uv,"current UV was reset/restored contrary to original GL mask");Check(r.CurrentNormal()==normal,"current normal was reset/restored contrary to original GL mask");
}
void DefaultAttributes(NativeUIRecorder& r)
{
	const auto s=r.CurrentState();Check(s.draw.blend==ScreenBlendState{},"ResetGLState blend differs");
	Check(s.draw.alphaTest==ScreenAlphaTest{false,ScreenAlphaCompare::Greater,.5f},"ResetGLState alpha function/reference differs");
	Check(!s.draw.texture&&!s.draw.scissor,"ResetGLState did not disable texture/scissor enables");
}
std::array<NativeUIMatrix,3> Matrices(NativeUIRecorder& r)
{
	return {r.GetMatrix(NativeUIMatrixMode::ModelView),r.GetMatrix(NativeUIMatrixMode::Projection),r.GetMatrix(NativeUIMatrixMode::Texture)};
}
void MiniMapMatrices(NativeUIRecorder& r,unsigned width=128,unsigned height=64)
{
	const auto m=Matrices(r);Check(m[0]==ScaleMatrix(1.f/float(width),1.f/float(height),1),"ResetMiniMapMatrices used target extent instead of original minimap size");
	Check(m[1]==OrthoMatrix(0,1,0,1,0,-1),"ResetMiniMapMatrices projection differs");Check(m[2]==IdentityMatrix(),"ResetMiniMapMatrices did not reset texture matrix");
}
void Begin(NativeUIRecorder& r)
{
	r.BeginFrame(800,600);r.BeginScreenStage();r.BeginTarget(Target(1,128,64));Seed(r);
	for(auto mode:{NativeUIMatrixMode::ModelView,NativeUIMatrixMode::Projection,NativeUIMatrixMode::Texture}){r.MatrixMode(mode);r.LoadMatrix(TranslationMatrix(11,12,0));}r.MatrixMode(NativeUIMatrixMode::ModelView);
}
void Finish(NativeUIRecorder& r) {r.EndTarget();r.EndFrame();r.RetireCompletedFrame();}
void SafeState()
{
	NativeUIRecorder r;Begin(r);const auto matrices=Matrices(r);r.BeginMiniMapStage(true);
	Current(r,WHITE,OUTER_UV,OUTER_NORMAL);DefaultAttributes(r);MiniMapMatrices(r);
	Seed(r,true);r.EndMiniMapStage(true,true);
	Current(r,CALLBACK_COLOR,CALLBACK_UV,CALLBACK_NORMAL);
	const auto s=r.CurrentState();Check(s.draw.blend==Blend(),"safe callback did not restore original blend factors/enables");
	Check(s.draw.alphaTest==ScreenAlphaTest{true,ScreenAlphaCompare::NotEqual,.37f},"safe callback alpha state not restored");
	Check(s.draw.texture==B,"safe callback incorrectly restored bound texture object");Check(s.draw.scissor==CALLBACK_BOX,"safe callback incorrectly restored scissor box");
	Check(Matrices(r)==matrices,"safe callback did not restore all three original matrix stacks");Finish(r);
}
void UnsafeState(bool returnToScreen)
{
	NativeUIRecorder r;Begin(r);const auto matrices=Matrices(r);
	// The outer enable save is an observer: reveal preserved binding/box after
	// unsafe return disables them without modifying the object/rectangle.
	r.PushAttrib(Enable);r.BeginMiniMapStage(false);Current(r,OUTER_COLOR,OUTER_UV,OUTER_NORMAL);
	Check(r.CurrentState().draw.texture==A&&r.CurrentState().draw.scissor==OUTER_BOX,"unsafe Begin reset original attribute state");MiniMapMatrices(r);
	Seed(r,true);r.EndMiniMapStage(false,returnToScreen);
	Current(r,returnToScreen?WHITE:CALLBACK_COLOR,CALLBACK_UV,CALLBACK_NORMAL);
	if(returnToScreen)DefaultAttributes(r);else Check(r.CurrentState().draw.blend==Blend(true),"unsafe non-screen exit reset callback blend");
	Check(Matrices(r)==matrices,"unsafe callback failed to restore matrices");r.PopAttrib();
	Check(r.CurrentState().draw.texture==B,"unsafe return reset bound texture object");Check(r.CurrentState().draw.scissor==CALLBACK_BOX,"unsafe return reset scissor rectangle");Finish(r);
}
void SeparateResets()
{
	NativeUIRecorder r;Begin(r);const auto matrices=Matrices(r);r.PushAttrib(Enable);r.ResetDrawingState();
	Current(r,WHITE,OUTER_UV,OUTER_NORMAL);DefaultAttributes(r);Check(Matrices(r)==matrices,"ResetState reset matrices/stacks");r.PopAttrib();
	Check(r.CurrentState().draw.texture==A&&r.CurrentState().draw.scissor==OUTER_BOX,"ResetState reset binding/box");
	r.BeginMiniMapStage(true);Seed(r,true);r.MultMatrix(TranslationMatrix(3,4,0));const auto state=r.CurrentState();r.ResetDrawingMatrices();
	Current(r,CALLBACK_COLOR,CALLBACK_UV,CALLBACK_NORMAL);Check(r.CurrentState().draw==state.draw,"ResetMatrices changed drawing state");MiniMapMatrices(r);
	r.ResetMiniMapStage();Current(r,WHITE,CALLBACK_UV,CALLBACK_NORMAL);DefaultAttributes(r);MiniMapMatrices(r);r.EndMiniMapStage(true,true);Finish(r);
	NativeUIRecorder screen;screen.BeginFrame(800,600);screen.BeginScreenStage();Seed(screen,true);screen.MultMatrix(TranslationMatrix(4,5,0));const auto s=screen.CurrentState();screen.ResetDrawingMatrices();
	Check(screen.CurrentState().draw==s.draw&&screen.CurrentState().color==s.color&&screen.CurrentNormal()==CALLBACK_NORMAL,"screen ResetMatrices reset current/attributes");
	Check(screen.GetMatrix(NativeUIMatrixMode::ModelView)==IdentityMatrix()&&screen.GetMatrix(NativeUIMatrixMode::Projection)==OrthoMatrix(0,800,0,600,-1,1),"screen ResetMatrices wrong projection");screen.EndFrame();
}
void NestedTargetResetUsesMiniMapContext()
{
	NativeUIRecorder r;Begin(r);r.BeginMiniMapStage(true);const auto outer=Matrices(r);
	r.BeginTarget(Target(2,32,16));r.ResetDrawingMatrices();
	// Original resetMatrixFunc remains ResetMiniMapMatrices even in nested RTT:
	// modelview uses minimap->GetSizeX/Y (128/64), not the32x16 FBO extent.
	MiniMapMatrices(r,128,64);r.EndTarget();Check(Matrices(r)==outer,"nested RTT lost minimap matrices");r.EndMiniMapStage(true,true);Finish(r);
}
void ErrorAndNesting()
{
	NativeUIRecorder r;r.BeginFrame(800,600);r.BeginScreenStage();Throws([&]{r.BeginMiniMapStage(true);},"minimap stage without target accepted");r.AbortFrame();
	Begin(r);r.BeginMiniMapStage(true);Throws([&]{r.EndMiniMapStage(false,true);},"changed safeMode accepted");r.EndMiniMapStage(true,true);Finish(r);
	Begin(r);r.BeginMiniMapStage(true);r.PushMatrix();Throws([&]{r.EndMiniMapStage(true,true);},"unbalanced minimap matrices accepted");r.PopMatrix();r.EndMiniMapStage(true,true);Finish(r);
	Begin(r);r.BeginMiniMapStage(true);r.PushAttrib(NativeUIAttribute::Current);Throws([&]{r.EndMiniMapStage(true,true);},"unbalanced minimap attributes accepted");r.PopAttrib();r.EndMiniMapStage(true,true);Finish(r);
	Begin(r);for(unsigned n=0;n<64;++n)r.BeginMiniMapStage(true);Throws([&]{r.BeginMiniMapStage(true);},"65th minimap callback accepted");for(unsigned n=0;n<64;++n)r.EndMiniMapStage(true,true);Finish(r);
}
void AbortTargetRemovesOnlyChildStages()
{
	NativeUIRecorder r;Begin(r);r.BeginMiniMapStage(true);r.AbortTarget();
	// An aborted FBO cannot retain a callback whose matrix/attribute stacks
	// were rolled back. A normal screen frame must remain usable.
	r.ResetDrawingMatrices();Check(r.GetMatrix(NativeUIMatrixMode::ModelView)==IdentityMatrix(),"AbortTarget left stale minimap callback context");r.EndFrame();
	Begin(r);r.BeginMiniMapStage(true);const auto outer=Matrices(r);r.BeginTarget(Target(2,32,16));r.BeginMiniMapStage(false);r.AbortTarget();
	Check(Matrices(r)==outer,"child target abort did not restore outer minimap matrices");r.EndMiniMapStage(true,true);Finish(r);
}
void EndTargetRejectsOpenStage()
{
	NativeUIRecorder r;Begin(r);r.BeginMiniMapStage(true);Throws([&]{r.EndTarget();},"EndTarget published a producer with open minimap callback");r.AbortFrame();
}
void ResetMatricesListCapturesOriginalCompileContext()
{
	NativeUIRecorder r;Begin(r);r.BeginMiniMapStage(true);
	r.BeginList();r.ResetDrawingMatrices();const auto list=r.EndList();
	r.EndMiniMapStage(true,true);r.EndTarget();
	// Original gl.ResetMatrices calls resetMatrixFunc during GL_COMPILE.
	// The generated glOrtho/glScale literals therefore retain the128x64
	// minimap context even when that GL list is called later in DrawScreen.
	r.CallList(list);MiniMapMatrices(r,128,64);r.EndFrame();r.RetireCompletedFrame();
}
}
int main()
{
	for(const auto& [name,test]:std::initializer_list<std::pair<const char*,void(*)()>>{{"safe-state",SafeState},{"unsafe-screen",[]{UnsafeState(true);}},{"unsafe-nonscreen",[]{UnsafeState(false);}},{"separate-resets",SeparateResets},{"nestedRTT-context",NestedTargetResetUsesMiniMapContext},{"bounds-retry-nesting",ErrorAndNesting},{"target-abort",AbortTargetRemovesOnlyChildStages},{"target-end-open-stage",EndTargetRejectsOpenStage},{"reset-list-compile-context",ResetMatricesListCapturesOriginalCompileContext}}) {
		try{test();std::cout<<name<<": PASS\n";}catch(const std::exception& e){++failures;std::cerr<<name<<": FAIL: "<<e.what()<<'\n';}
	}
	std::cout<<"MiniMapStages17: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
