/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativePostprocessRecorder.h"
#include <iostream>
#include <stdexcept>
using namespace rendering;using namespace rendering::ui;
namespace {
unsigned checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>void Reject(F fn,const char* message){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}Check(failed,message);}
void Triangle(NativeUIRecorder& recorder){recorder.Begin(ScreenPrimitive::Triangles);recorder.Vertex(-1,-1);recorder.Vertex(1,-1);recorder.Vertex(-1,1);recorder.End();}
std::shared_ptr<ScreenTexture> ScreenCopy(std::shared_ptr<const NativePostprocessVersion> version){auto image=std::make_shared<ScreenTexture>();image->width=version->image->width;image->height=version->image->height;image->generation=version->generation;image->flipV=true;image->postprocessVersion=std::move(version);return image;}
auto Target(){auto binding=std::make_shared<NativeUITextureBinding>();binding->current=MakeRenderTargetTexture(8,8,"actual cached target");return binding;}
auto Next(const std::shared_ptr<NativeUITextureBinding>& cell){auto next=std::make_shared<ScreenTexture>(*cell->current);++next->generation;return next;}
void OrderedAndHidden(bool completedSource)
{
	NativePostprocessRecorder copies;NativeUIRecorder ui;unsigned queried=0;
	ui.SetSourceDependencyQuery([&](const auto& version){++queried;return copies.OwnsPendingVersion(version);});
	auto color=CreateNativePostprocessBinding(8,8,NativePostprocessFormat::RGBA8,{},false,16384);
	auto child=Target(),parent=Target();const auto oldChild=child->current,oldParent=parent->current;
	copies.BeginFrame(8,8,27);ui.BeginFrame(8,8);ui.BeginScreenStage();Triangle(ui);
	const NativePostprocessRect rect{0,0,8,8};
	auto version=copies.Copy(color,NativePostprocessOperationKind::CopyColor,rect,rect,ui.RootCommandCount());
	auto sampled=ScreenCopy(version);
	if(!completedSource){auto generated=*sampled;generated.generateMipmaps=true;Reject([&]{ValidateScreenTextureMipGeneration(generated);},"Generated image mips must reject the actual shared copied version");}
	const auto childImage=Next(child);ui.BeginTarget(childImage);ui.SetTextureEnabled(false,0);ui.SetTexture(sampled,{},1);Triangle(ui);ui.EndTarget();ui.CommitTargetBinding(child,oldChild,childImage);
	const auto parentImage=Next(parent);ui.BeginTarget(parentImage);ui.SetTexture(childImage,{},1);Triangle(ui);ui.EndTarget();ui.CommitTargetBinding(parent,oldParent,parentImage);
	ui.SetTexture(parentImage,{},1);Triangle(ui);
	auto frame=ui.EndFrame();auto postprocess=copies.EndFrame();frame.postprocess=postprocess;
	Check(frame.targetPasses.size()==2,"Both actual nested target producers retained");
	Check(frame.targetPasses[0].sourceDependent,"Secondary-only copied source must mark child source-dependent");
	Check(frame.targetPasses[1].sourceDependent,"Secondary-only child dependency must propagate to parent");
	Check(queried>0,"Source ownership query must inspect secondary-only copy");
	Check(frame.targetPasses[0].screenCommandOffset==1&&frame.targetPasses[1].screenCommandOffset==1,"Nested source targets retain exact original screen insertion");
	Check(frame.targetPasses[0].commandSequence<frame.targetPasses[1].commandSequence,"Parent follows actual child publication sequence");
	Check(frame.targetPasses[0].drawList->Commands()[0].state.material==ScreenMaterial::Solid&&!frame.targetPasses[0].drawList->Commands()[0].state.texture,"Regression genuinely has no primary texture");
	Check(frame.targetPasses[0].drawList->Commands()[0].state.secondary.texture==sampled,"Actual copy version carried only through unit one");
	Check(OrderScreenRenderTargetPasses(frame)==std::vector<std::size_t>{0,1},"Source-dependent child remains before parent");
	ValidateNativePostprocessConsumer(*postprocess,*version,NativePostprocessConsumer{NativePostprocessStage::Screen,0,1,frame.targetPasses[0].commandSequence},nullptr,nullptr);++checks;
	Reject([&]{ValidateNativePostprocessConsumer(*postprocess,*version,NativePostprocessConsumer{NativePostprocessStage::Screen,0,0},nullptr,nullptr);},"Screen secondary consumer cannot read copy from a later original command");
	ScreenDrawList early;early.SetSecondaryTexture({parentImage,ScreenUVMode::Normalized,{}});early.BeginQuads();early.Vertex(0,0);early.Vertex(8,0);early.Vertex(8,8);early.Vertex(0,8);early.End();
	auto invalid=frame;invalid.drawList=&early;
	Reject([&]{OrderScreenRenderTargetPasses(invalid);},"Secondary-only root consumer before source-dependent target must be rejected");
	std::vector<ScreenRenderTargetPass> passes(frame.targetPasses.begin(),frame.targetPasses.end());invalid=frame;invalid.targetPasses=passes;passes[1].sourceDependent=false;
	Reject([&]{OrderScreenRenderTargetPasses(invalid);},"Independent target cannot consume secondary-only source-dependent child");
	passes.assign(frame.targetPasses.begin(),frame.targetPasses.end());passes[1].commandSequence=passes[0].commandSequence;
	Reject([&]{OrderScreenRenderTargetPasses(invalid);},"Same-offset secondary child cannot be topologically moved ahead of its original publication");
	// A font's explicit sampler0 program ignores unrelated fixed-function unit1.
	auto atlas=std::make_shared<ScreenTexture>();atlas->width=atlas->height=1;atlas->bytesPerRow=1;atlas->format=ScreenTextureFormat::R8Unorm;atlas->bytes={255};
	ScreenDrawList font;font.SetTexture(atlas,ScreenMaterial::FontCoverage);font.SetSecondaryTexture({parentImage,ScreenUVMode::Normalized,{}});font.BeginQuads();font.Vertex(0,0);font.Vertex(8,0);font.Vertex(8,8);font.Vertex(0,8);font.End();
	invalid=frame;invalid.drawList=&font;Check(OrderScreenRenderTargetPasses(invalid).size()==2,"Explicit font shader must not acquire a false unit-one source-order dependency");
	const auto retained=std::vector<ScreenRenderTargetPass>(frame.targetPasses.begin(),frame.targetPasses.end());
	Check(ChooseNativeSourceSubmission(false,true,false,&frame)==NativeSourceSubmission::OffscreenSource,"Hidden window with actual captured source must submit real source work offscreen");
	Check(ChooseNativeSourceSubmission(false,false,false,&frame)==NativeSourceSubmission::IndependentTargets,"Missing actual source cannot pretend a source-dependent frame completed");
	if(completedSource){copies.RetireCompletedFrame(postprocess->serial);ui.RetireCompletedFrame(true);}else ui.RetireCompletedFrame(false);
	// Simulate a skipped/failed source submission; target-only completion is
	// insufficient. Actual hidden offscreen source completion uses the other arm.
	copies.BeginFrame(8,8,28);ui.BeginFrame(8,8);ui.BeginScreenStage();
	Check(color->current==(completedSource?version:nullptr),"Skipped copy rollback / completed-copy retention must follow actual source retirement");
	Check(child->current==(completedSource?childImage:oldChild),"Secondary-only child binding rolls back only when source did not complete");
	Check(parent->current==(completedSource?parentImage:oldParent),"Transitive secondary-only parent rolls back with missing source");
	Check(retained[0].destination==childImage&&retained[0].drawList->Commands()[0].state.secondary.texture==sampled&&sampled->postprocessVersion==version,"Immutable old target/copy packets survive cell rollback");
	auto next=ui.EndFrame();Check(next.targetPasses.empty(),"Old source-dependent targets cannot replay on a newer world");copies.AbortFrame();ui.RetireCompletedFrame();
}
void WorldStages()
{
	NativePostprocessRecorder copies;auto cell=CreateNativePostprocessBinding(8,8,NativePostprocessFormat::RGBA8,{},false,16384);const NativePostprocessRect rect{0,0,8,8};copies.BeginFrame(8,8,29);
	auto early=copies.Copy(cell,NativePostprocessOperationKind::CopyColor,rect,rect,0,NativePostprocessStage::PreUnit,10);auto frame=copies.EndFrame();
	ValidateNativePostprocessConsumer(*frame,*early,NativePostprocessConsumer{NativePostprocessStage::PreUnit,11,0},nullptr,nullptr);++checks;
	ValidateNativePostprocessConsumer(*frame,*early,NativePostprocessConsumer{NativePostprocessStage::World,12,0},nullptr,nullptr);++checks;
	Reject([&]{ValidateNativePostprocessConsumer(*frame,*early,NativePostprocessConsumer{NativePostprocessStage::PreUnit,9,0},nullptr,nullptr);},"Unit-one world consumer must retain actual stage and sequence");copies.AbortFrame();
}
}
int main(){try{OrderedAndHidden(false);OrderedAndHidden(true);WorldStages();std::cout<<"PASS "<<checks<<" secondary-only copy/RTT/order/hidden-failure rollback checks\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
