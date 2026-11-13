#include "Rendering/Backend/NativeOwnedScreenFrame.h"
#include <cassert>
#include <iostream>
#include <bit>
using namespace rendering;
void Quad(ScreenDrawList& l,float x){l.BeginQuads();l.Vertex(x,0);l.Vertex(x+1,0);l.Vertex(x+1,1);l.Vertex(x,1);l.End();}
int main(){
 NativeOwnedScreenFrame owner;ScreenDrawList root;std::vector<ScreenRenderTargetPass> passes(1);
 auto image=std::make_shared<ScreenTexture>();passes[0].destination=image;
 Quad(root,10);const auto* vertices=root.Vertices().data();
 auto first=owner.Seal(root,passes,800,600);assert(first.storage);assert(first.drawList->Vertices().data()==vertices);assert(first.coordinateWidth==800);
 auto copy=first;first={};owner.Recycle(root);root.ClearGeometry();Quad(root,20);passes.clear();
 auto second=owner.Seal(root,passes,400,300);
 assert(copy.drawList->Vertices()[0].x==10);assert(copy.targetPasses.size()==1);assert(copy.targetPasses[0].destination==image);
 assert(second.drawList->Vertices()[0].x==20);assert(second.targetPasses.empty());assert(copy.storage!=second.storage);
 const auto* reusable=second.drawList->Vertices().data();auto weak=std::weak_ptr<const ScreenFrameStorage>(second.storage);second={};
 owner.Recycle(root);root.ClearGeometry();Quad(root,30);assert(root.Vertices().data()==reusable);
 auto third=owner.Seal(root,passes,100,100);assert(third.drawList->Vertices().data()==reusable);assert(!weak.expired());
 auto held=third;third={};owner=NativeOwnedScreenFrame{};assert(held.drawList->Vertices()[0].x==30);assert(copy.drawList->Vertices()[0].x==10);
 root.ClearGeometry();root.Color({-0.0f,0.5f,1,1});Quad(root,40);
 auto stateFrame=owner.Seal(root,passes,1,1);assert(std::bit_cast<unsigned>(root.CaptureState().color[0])==std::bit_cast<unsigned>(-0.0f));
 for(int n=0;n<1000;++n){owner.Recycle(root);root.ClearGeometry();Quad(root,float(n));auto frame=owner.Seal(root,passes,1,1);assert(frame.drawList->Vertices()[0].x==float(n));assert(stateFrame.drawList->Vertices()[0].x==40);}
 std::cout<<"owned packet isolation, descriptor retention, move transport, copy lifetime and sole-owner capacity reuse passed\n";
}
