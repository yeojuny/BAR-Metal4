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
std::shared_ptr<const ScreenTexture> Image(const char* name,unsigned width,unsigned generation)
{
	auto result=std::make_shared<ScreenTexture>();result->width=width;result->height=1;result->bytesPerRow=width*4;result->bytes.assign(width*4,255);result->sourceName=name;result->generation=generation;return result;
}
void Quad(NativeUIRecorder& recorder)
{
	recorder.Begin(ScreenPrimitive::Quads);recorder.Vertex(0,0);recorder.Vertex(8,0);recorder.Vertex(8,8);recorder.Vertex(0,8);recorder.End();
}
void PersistentBinding()
{
	auto identity=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{73,8,8});
	auto initial=std::make_shared<ScreenTexture>();initial->width=initial->height=8;initial->sourceName="!73";initial->renderTarget=identity;
	auto updated=std::make_shared<ScreenTexture>(*initial);updated->generation=1;
	auto binding=std::make_shared<NativeUITextureBinding>();binding->current=initial;
	NativeUIRecorder r;r.BeginFrame(8,8);r.BeginScreenStage();r.SetTextureResource(binding);Quad(r);
	r.BeginTarget(updated);r.ClearTarget(ScreenColor{1,0,0,1});r.EndTarget();binding->current=updated;Quad(r);
	binding->sampler.filter=ScreenFilter::Nearest;Quad(r);
	const auto frame=r.EndFrame();const auto commands=frame.drawList->Commands();
	Check(commands.size()==3 && frame.targetPasses.size()==1,"persistent binding lost ordered draws or producer");
	Check(commands[0].state.texture==initial,"texture mutation rewrote an earlier recorded draw");
	Check(commands[1].state.texture==updated && frame.targetPasses[0].destination==updated,"bound texture name retained old storage after RenderToTexture");
	Check(commands[1].state.sampler.filter==ScreenFilter::Linear && commands[2].state.sampler.filter==ScreenFilter::Nearest,"sampler mutation did not resolve at the next draw");

	// A list need not bind a texture itself: the caller's existing binding is
	// still a logical name at each replay, even after an attribute restore.
	r.BeginList();Quad(r);auto inheritedList=r.EndList();
	r.BeginFrame(8,8);r.BeginScreenStage();r.SetTextureResource(binding);r.PushAttrib(Texture);
	r.SetTexture(Image("temporary",1,0));binding->current=initial;r.PopAttrib();r.CallList(inheritedList);
	binding->current=updated;r.CallList(inheritedList);
	r.PushAttrib(Current);r.SetTexture(Image("explicit immutable",1,0));r.PopAttrib();Quad(r);
	r.SetTexture(nullptr);Quad(r);
	const auto restored=r.EndFrame();const auto restoredCommands=restored.drawList->Commands();
	Check(restoredCommands.size()==4,"attribute/list sequence did not emit four draws");
	Check(restoredCommands[0].state.texture==initial && restoredCommands[1].state.texture==updated,"texture attributes or inherited list froze the saved binding");
	Check(restoredCommands[2].state.texture->sourceName=="explicit immutable","non-texture attributes restored an unrelated binding");
	Check(!restoredCommands[3].state.texture && restoredCommands[3].state.material==ScreenMaterial::Solid,"texture disable retained the previous logical binding");

	r.BeginFrame(8,8);r.BeginScreenStage();r.SetTextureResource(binding);binding->current.reset();
	Reject([&]{r.Begin(ScreenPrimitive::Quads);},"deleted bound resource was sampled from an old snapshot");
	r.SetTexture(nullptr);Quad(r);Check(r.EndFrame().drawList->Commands().size()==1,"failed binding resolution left an unfinished primitive");

	binding->current=initial;
	r.BeginFrame(8,8);r.BeginScreenStage();r.SetTextureResource(binding);r.BeginTarget(updated);
	Reject([&]{r.Begin(ScreenPrimitive::Quads);},"binding before RenderToTexture bypassed feedback validation");
	r.SetTexture(nullptr);r.ClearTarget(ScreenColor{0,0,0,0});r.EndTarget();r.EndFrame();

	// A delegated unit picture follows the source cell even when no new bind
	// command occurs between consecutive draws.
	auto unit=std::make_shared<NativeUITextureBinding>();unit->source=binding;
	r.BeginFrame(8,8);r.BeginScreenStage();r.SetTextureResource(unit);Quad(r);binding->current=updated;Quad(r);
	const auto delegated=r.EndFrame();const auto delegatedCommands=delegated.drawList->Commands();
	Check(delegatedCommands[0].state.texture==initial && delegatedCommands[1].state.texture==updated,"persistent unit-picture delegation froze its source version");
}

std::shared_ptr<NativeUITextureBinding> Binding(const char* name)
{
	auto result=std::make_shared<NativeUITextureBinding>();result->current=Image(name,1,0);return result;
}
void TextureAttributes()
{
	auto a=Binding("a"),b=Binding("b");
	ScreenSampler original{ScreenFilter::Nearest,ScreenWrap::Repeat,ScreenWrap::TransparentBorder,ScreenMipFilter::Linear,ScreenFilter::Linear};
	ScreenSampler middle{ScreenFilter::Linear,ScreenWrap::ClampEdge,ScreenWrap::Repeat,ScreenMipFilter::Nearest,ScreenFilter::Nearest};
	ScreenSampler temporary{ScreenFilter::Nearest,ScreenWrap::TransparentBorder,ScreenWrap::ClampEdge};
	NativeUIRecorder r;r.BeginFrame(8,8);r.BeginScreenStage();r.SetTextureResource(a);
	// ChangeTextureParams changes the object, not the recorder's cached draw
	// state. Push must capture these actual parameters even without a rebind.
	a->sampler=original;Quad(r);r.PushAttrib(Texture);a->sampler=middle;Quad(r);
	auto next=Image("a generation 1",1,1);a->current=next;
	r.PushAttrib(Texture);a->sampler=temporary;r.PopAttrib();Quad(r);
	Check(a->sampler==middle,"nested texture pop did not restore its own object parameters");
	r.SetTextureResource(b);b->sampler=temporary;r.PopAttrib();Quad(r);
	Check(a->sampler==original,"outer texture pop did not restore parameters globally");
	Check(b->sampler==temporary,"texture pop rewrote an unrelated object's parameters");
	Check(a->current==next,"texture pop rolled back current image storage");
	r.SetTextureResource(a);Quad(r);
	auto frame=r.EndFrame();const auto commands=frame.drawList->Commands();
	Check(commands.size()==5,"nested texture attributes lost draws");
	Check(commands[0].state.sampler==original && commands[1].state.sampler==middle,"parameter restoration mutated earlier immutable commands");
	Check(commands[2].state.texture==next && commands[2].state.sampler==middle,"nested pop did not preserve latest generation with restored parameters");
	Check(commands[3].state.texture==next && commands[3].state.sampler==original && commands[4].state.sampler==original,"subsequent logical resolution/rebind overwrote restored parameters");

	// A unit-definition cell can change its delegated object while attributes
	// are pushed. Restore the saved object's parameters, not the new leaf's.
	auto unit=Binding("unit");unit->source=a;
	r.BeginFrame(8,8);r.BeginScreenStage();r.SetTextureResource(unit);r.PushAttrib(Texture);
	a->sampler=middle;unit->source=b;b->sampler=temporary;r.PopAttrib();Quad(r);
	frame=r.EndFrame();
	Check(a->sampler==original && b->sampler==temporary,"delegation change restored parameters to the wrong object");
	Check(frame.drawList->Commands()[0].state.texture==b->current && frame.drawList->Commands()[0].state.sampler==temporary,"delegated logical binding was frozen by texture attributes");

	// Push/Pop are relative list commands and capture parameters at replay.
	r.SetTextureResource(a);r.BeginList();r.PushAttrib(Texture);Quad(r);r.PopAttrib();auto list=r.EndList();
	a->sampler=middle;r.BeginFrame(8,8);r.BeginScreenStage();r.SetTextureResource(a);r.CallList(list);Quad(r);frame=r.EndFrame();
	Check(a->sampler==middle && frame.drawList->Commands()[0].state.sampler==middle && frame.drawList->Commands()[1].state.sampler==middle,"compiled texture attributes froze compile-time parameters");
}
void EnableAttributes()
{
	auto a=Binding("enabled a"),b=Binding("enabled b");NativeUIRecorder r;
	const ScreenScissor first{1,2,3,4},second{5,6,7,8};
	r.BeginFrame(8,8);r.BeginScreenStage();r.SetTextureResource(a);r.SetScissor(first);
	r.SetAlphaTest({true,ScreenAlphaCompare::Greater,.1f});r.PushAttrib(Enable);
	r.SetTextureEnabled(false);r.SetTextureResource(b);r.SetTextureEnabled(false);
	b->sampler={ScreenFilter::Nearest,ScreenWrap::Repeat,ScreenWrap::Repeat};
	auto blend=r.CurrentState().draw.blend;blend.enabled=false;blend.destinationColor=ScreenBlendFactor::One;r.SetBlend(blend);
	r.SetAlphaTest({false,ScreenAlphaCompare::Less,.7f});r.SetScissor(second);r.SetScissor({});r.PopAttrib();Quad(r);
	r.SetTextureEnabled(false);Quad(r);auto next=Image("enabled b generation 1",1,1);b->current=next;r.SetTextureEnabled(true);Quad(r);
	auto frame=r.EndFrame();const auto commands=frame.drawList->Commands();
	Check(commands[0].state.texture->sourceName=="enabled b" && commands[0].state.sampler==b->sampler,"enable attributes restored an old binding or object parameters");
	Check(commands[0].state.blend.enabled && commands[0].state.blend.destinationColor==ScreenBlendFactor::One,"enable attributes restored blend factors");
	Check(commands[0].state.alphaTest==ScreenAlphaTest{true,ScreenAlphaCompare::Less,.7f},"enable attributes restored alpha-test parameters");
	Check(commands[0].state.scissor==second,"enable attributes restored an old scissor rectangle");
	Check(!commands[1].state.texture && commands[1].state.material==ScreenMaterial::Solid,"Texture(false) left texturing enabled");
	Check(commands[2].state.texture==next,"Texture(false/true) lost logical binding or latest image generation");

	r.BeginFrame(8,8);r.BeginScreenStage();r.SetTextureResource(b);r.SetTextureEnabled(false);r.PushAttrib(Texture);
	r.SetTextureEnabled(true);b->sampler.filter=ScreenFilter::Linear;r.PopAttrib();Quad(r);r.SetTextureEnabled(true);Quad(r);frame=r.EndFrame();
	Check(!frame.drawList->Commands()[0].state.texture && frame.drawList->Commands()[1].state.sampler.filter==ScreenFilter::Nearest,"texture attributes did not restore both disabled state and object parameters");
	r.BeginFrame(8,8);r.BeginScreenStage();Reject([&]{r.SetTextureEnabled(true);},"unimplemented default texture enable reported success");
	r.SetTextureResource(a);r.SetTextureEnabled(false);r.PushAttrib(Enable);r.SetTextureResource(b);r.PopAttrib();Quad(r);r.SetTextureEnabled(true);Quad(r);frame=r.EndFrame();
	Check(!frame.drawList->Commands()[0].state.texture && frame.drawList->Commands()[1].state.texture==b->current,"disabled enable attributes unbound the newly bound object");
}
void AttributeFailures()
{
	auto a=Binding("failure a"),b=Binding("failure b");NativeUIRecorder r;
	const ScreenSampler saved{},changed{ScreenFilter::Nearest,ScreenWrap::Repeat,ScreenWrap::Repeat};
	r.BeginFrame(8,8);r.BeginScreenStage();r.SetTextureResource(a);r.PushAttrib(Texture);a->sampler=changed;
	// Pop compiles against the caller's attribute stack; compilation must not
	// change the real object. The later matrix pop only fails at replay.
	r.BeginList();r.PopAttrib();r.PopMatrix();auto failing=r.EndList();
	Check(a->sampler==changed,"GL_COMPILE texture pop modified shared object parameters");
	Reject([&]{r.CallList(failing);},"relative replay matrix underflow was accepted");
	Check(a->sampler==changed,"failed replay leaked an earlier object-parameter restoration");
	Quad(r);r.PopAttrib();Quad(r);auto frame=r.EndFrame();
	Check(frame.drawList->Commands()[0].state.sampler==changed && frame.drawList->Commands()[1].state.sampler==saved,"failed replay damaged caller attribute stack/binding");

	// Inner replay success must remain reversible if its outer replay fails.
	r.BeginFrame(8,8);r.BeginScreenStage();r.SetTextureResource(a);r.PushAttrib(Texture);a->sampler=changed;
	r.BeginList();r.PopAttrib();auto inner=r.EndList();r.BeginList();r.CallList(inner);r.PopMatrix();auto outer=r.EndList();
	Reject([&]{r.CallList(outer);},"nested relative replay failure was accepted");
	Check(a->sampler==changed,"outer replay failure retained an inner sampler restoration");
	r.PopAttrib();Check(a->sampler==saved,"outer replay rollback lost caller texture attribute");r.EndFrame();

	// Invalid delegation must reject Push before mutating the attribute stack.
	auto unit=Binding("failure unit");unit->source=a;
	r.BeginFrame(8,8);r.BeginScreenStage();r.SetTextureResource(unit);unit->source=unit;
	Reject([&]{r.PushAttrib(Texture);},"cyclic texture push was accepted");unit->source=a;
	Reject([&]{r.PopAttrib();},"failed texture push left a partial attribute entry");
	b->current.reset();Reject([&]{r.SetTextureResource(b);},"deleted replacement binding was accepted");Quad(r);
	r.PushAttrib(Texture);a->current.reset();r.PopAttrib();
	Reject([&]{Quad(r);},"texture attribute pop resurrected a deleted image");
	r.SetTexture(nullptr);Quad(r);frame=r.EndFrame();
	Check(frame.drawList->Commands().size()==2,"failed binding/texture push damaged subsequent drawing");
}
}
int main()
{
	try {
		PersistentBinding();
		TextureAttributes();EnableAttributes();AttributeFailures();
		auto unit=std::make_shared<NativeUITextureBinding>(),rtt=std::make_shared<NativeUITextureBinding>();
		auto declared=Image("declared buildpic",2,1),override=Image("replacement file",3,2),a=Image("RTT",4,1),b=Image("RTT",4,2);
		unit->current=declared;rtt->current=a;
		NativeUIRecorder r;r.BeginList();r.SetTextureResource(unit);r.Begin(ScreenPrimitive::Quads);r.Vertex(0,0);r.Vertex(10,0);r.Vertex(10,10);r.Vertex(0,10);r.End();auto list=r.EndList();
		r.BeginFrame(800,600);r.BeginScreenStage();r.CallList(list);
		unit->current=override;r.CallList(list);
		unit->current.reset();unit->source=rtt;r.CallList(list);
		rtt->current=b;rtt->sampler.filter=ScreenFilter::Nearest;rtt->sampler.wrapS=ScreenWrap::Repeat;r.CallList(list);
		unit->source.reset();unit->current=declared;r.CallList(list);
		auto frame=r.EndFrame();const auto commands=frame.drawList->Commands();
		Check(commands.size()==5,"same compiled build-picture list did not observe five replacements");
		Check(commands[0].state.texture==declared && commands[1].state.texture==override,"filename replacement was frozen at compilation");
		Check(commands[2].state.texture==a && commands[3].state.texture==b,"delegated RTT generation was frozen at SetUnitDefImage");
		Check(commands[3].state.sampler.filter==ScreenFilter::Nearest && commands[3].state.sampler.wrapS==ScreenWrap::Repeat,"delegated current sampler not resolved with image");
		Check(commands[4].state.texture==declared,"declared-default reset did not reach the same list");
		rtt->current.reset();unit->current.reset();unit->source=rtt;
		Reject([&]{unit->Resolve();},"deleted Lua texture silently retained stale image");
		Check(commands[2].state.texture==a && commands[3].state.texture==b,"deletion invalidated already recorded immutable frame images");
		r.BeginFrame(800,600);r.BeginScreenStage();const auto before=r.CurrentState();
		Reject([&]{r.CallList(list);},"deleted delegate replay silently succeeded");
		Check(r.CurrentState().draw==before.draw,"failed resource resolution leaked replay state");r.EndFrame();
		unit->source=unit;Reject([&]{unit->Resolve();},"self delegation cycle accepted");unit->source=rtt;rtt->source=unit;
		Reject([&]{unit->Resolve();},"mutual delegation cycle accepted");rtt->source.reset();unit->source.reset();unit->current=declared;
		auto chain=unit;for(unsigned k=0;k<64;++k){auto next=std::make_shared<NativeUITextureBinding>();next->source=chain;chain=next;}
		Reject([&]{chain->Resolve();},"unbounded delegation chain accepted");
		Check(unit->Resolve().image==declared,"delegation rejection damaged source cell");
		std::cout<<"NativeUITextureBindings: "<<checks<<" checks passed\n";return 0;
	} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
