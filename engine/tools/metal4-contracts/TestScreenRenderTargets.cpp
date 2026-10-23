/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/ScreenDrawList.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace rendering;
int checks = 0;
void Check(bool value, const char* label) { ++checks; if (!value) throw std::runtime_error(label); }
template<class F> void Throws(F action, const char* label) { bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}Check(rejected,label); }
std::shared_ptr<ScreenTexture> Version(std::shared_ptr<const ScreenRenderTargetIdentity> identity, uint64_t generation)
{
	auto result=std::make_shared<ScreenTexture>();result->renderTarget=std::move(identity);result->generation=generation;
	result->width=result->renderTarget->width;result->height=result->renderTarget->height;result->flipV=true;return result;
}
std::shared_ptr<ScreenDrawList> Sampling(const std::shared_ptr<const ScreenTexture>& texture)
{
	auto list=std::make_shared<ScreenDrawList>();list->SetTexture(texture);list->BeginQuads();
	list->Vertex(0,0);list->Vertex(16,0);list->Vertex(16,8);list->Vertex(0,8);list->End();return list;
}
ScreenRenderTargetPass Pass(const std::shared_ptr<const ScreenTexture>& version, const std::shared_ptr<const ScreenDrawList>& list)
{
	ScreenRenderTargetPass pass;pass.destination=version;pass.drawList=list;pass.load=ScreenTargetLoad::Clear;
	pass.coordinateWidth=version->width;pass.coordinateHeight=version->height;return pass;
}
}
int main()
{
	try {
		ScreenDrawList clear;clear.Color({0.2f,0.3f,0.4f,0.5f});clear.TexCoord(12,7);clear.SetAlphaTest({true,ScreenAlphaCompare::Never,1});
		clear.SetBlend({true,ScreenBlendFactor::One,ScreenBlendFactor::One,ScreenBlendFactor::One,ScreenBlendFactor::One});
		const auto before=clear.CaptureState();const ScreenScissor scissor{-3,2,9,4};
		clear.ClearColorAttachment({0.7f,0.6f,0.5f,0.4f},16,8,scissor);
		Check(clear.CurrentState()==before.draw&&clear.CurrentColor()==before.color&&clear.CurrentTexCoord()==before.texCoord,"clear leaves caller state intact");
		Check(clear.Commands().size()==1&&!clear.Commands()[0].state.blend.enabled,"clear replaces attachment independent of blend");
		Check(!clear.Commands()[0].state.alphaTest.enabled&&clear.Commands()[0].state.material==ScreenMaterial::Solid&&!clear.Commands()[0].state.texture,"clear ignores alpha and texture");
		Check(clear.Commands()[0].state.scissor==scissor,"clear captures signed original scissor");
		Check(clear.Vertices()[0].x==0&&clear.Vertices()[0].y==0&&clear.Vertices()[2].x==16&&clear.Vertices()[2].y==8,"clear uses complete untransformed attachment extent");
		Check(clear.Vertices()[2].color==ScreenColor{0.7f,0.6f,0.5f,0.4f},"clear uses supplied color");
		Throws([&]{clear.ClearColorAttachment({0,0,0,0},0,8);},"zero clear extent rejected");
		clear.BeginQuads();Throws([&]{clear.ClearColorAttachment({0,0,0,0},16,8);},"clear during primitive rejected");clear.Abort();
		const auto identityA=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{1,16,8});
		const auto identityB=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{2,16,8});
		auto a1=Version(identityA,1),a2=Version(identityA,2),b1=Version(identityB,1);
		auto empty=std::make_shared<ScreenDrawList>();auto consumeA=Sampling(a1),consumeB=Sampling(b1);
		std::vector<ScreenRenderTargetPass> passes{Pass(b1,consumeA),Pass(a1,empty)};
		ScreenFrameView frame{consumeB.get(),16,8,std::nullopt,passes};
		Check(OrderScreenRenderTargetPasses(frame)==std::vector<size_t>{1,0},"producer ordering independent of storage order");
		passes[0]=Pass(a2,empty);passes[0].load=ScreenTargetLoad::Load;passes[0].previous=a1;
		Check(OrderScreenRenderTargetPasses(frame)==std::vector<size_t>{1,0},"partial update load orders old generation first");
		passes[0].previous=a2;Throws([&]{OrderScreenRenderTargetPasses(frame);},"same-version load rejected");
		passes[0].previous=b1;Throws([&]{OrderScreenRenderTargetPasses(frame);},"foreign-identity previous rejected");
		passes[0].previous=a1;passes[0].load=ScreenTargetLoad::Clear;Throws([&]{OrderScreenRenderTargetPasses(frame);},"unused previous payload rejected");
		passes[0]=Pass(b1,consumeA);passes[1]=Pass(a1,consumeB);Throws([&]{OrderScreenRenderTargetPasses(frame);},"nested dependency cycle rejected");
		passes[1]=Pass(a1,consumeA);Throws([&]{OrderScreenRenderTargetPasses(frame);},"self sampling rejected");
		passes[1]=Pass(a1,empty);passes[1].store=ScreenTargetStore::DontCare;Throws([&]{OrderScreenRenderTargetPasses(frame);},"discarded output cannot be sampled");
		passes[0]=Pass(b1,empty);passes[1]=Pass(a1,empty);passes[1].store=ScreenTargetStore::DontCare;frame.drawList=consumeA.get();Throws([&]{OrderScreenRenderTargetPasses(frame);},"root consumption of discarded output rejected");
		passes[1].store=ScreenTargetStore::Store;passes[0]=passes[1];Throws([&]{OrderScreenRenderTargetPasses(frame);},"duplicate producer rejected");
		passes.resize(1);frame.targetPasses=passes;passes[0].coordinateWidth=15;Throws([&]{OrderScreenRenderTargetPasses(frame);},"RTT basis cannot silently scale attachment");passes[0].coordinateWidth=16;
		a1->flipV=false;Throws([&]{OrderScreenRenderTargetPasses(frame);},"missing original framebuffer UV flip rejected");a1->flipV=true;
		a1->bytes={1};Throws([&]{OrderScreenRenderTargetPasses(frame);},"RTT cannot also carry static image bytes");a1->bytes.clear();
		a1->format=ScreenTextureFormat::R8Unorm;Throws([&]{OrderScreenRenderTargetPasses(frame);},"unsupported alpha-only target rejected");a1->format=ScreenTextureFormat::RGBA8Unorm;
		Check(OrderScreenRenderTargetPasses(frame)==std::vector<size_t>{0},"valid graph remains usable after rejected candidates");
		std::weak_ptr<const ScreenTexture> retained=a1;a1.reset();consumeA.reset();frame.drawList=nullptr;
		Check(!retained.expired(),"pending pass owns its exact destination version");passes.clear();
		Check(retained.expired(),"target identity does not retain unbounded generation history");
		std::cout<<"ScreenRenderTargets: "<<checks<<" checks passed\n";return 0;
	} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
