#include "Rendering/Metal4/Metal4WorldRenderer.h"
#include "Rendering/Backend/NativeSerialRenderWorker.h"
#include <cassert>
#include <iostream>
using namespace rendering;
int main(){
 WorldFrame source;source.sourceDrawFrame=7;source.units.resize(2);const auto units=source.units.data();
 auto frame=std::make_shared<WorldFrame>(std::move(source));assert(frame->units.data()==units);
 auto storage=std::make_shared<ScreenFrameStorage>();ScreenFrameView screen{&storage->root,800,600,{},storage->targets,{},storage};
 Metal4WorldRenderOptions options;options.pbrDeclarations=NativePBRDeclarationReceipt{};options.screenUI=&screen;
 auto packet=std::make_shared<const Metal4WorldSubmission>(frame,options);assert(packet->Options().screenUI!=&screen&&packet->Options().screenUI->storage==storage);
 std::weak_ptr<const WorldFrame> weakFrame=frame;std::weak_ptr<const ScreenFrameStorage> weakScreen=storage;
 frame.reset();storage.reset();screen={};options={};
 NativeSerialRenderWorker worker;
 worker.Invoke([packet]{assert(packet->Frame().sourceDrawFrame==7&&packet->Frame().units.size()==2);assert(packet->Options().screenUI->coordinateWidth==800);});
 assert(!weakFrame.expired()&&!weakScreen.expired());packet.reset();assert(weakFrame.expired()&&weakScreen.expired());
 auto owned=std::make_shared<WorldFrame>();Metal4WorldRenderOptions valid;valid.pbrDeclarations=NativePBRDeclarationReceipt{};
 for(unsigned bad=0;bad<5;++bad){
  auto o=valid;ScreenFrameView borrowed;auto retained=std::make_shared<ScreenFrameStorage>();borrowed.storage=retained;borrowed.drawList=&retained->root;borrowed.targetPasses=retained->targets;
  if(bad==0)o.pbrDeclarations.reset();
  if(bad==1)o.readback=reinterpret_cast<const ui::NativeUIReadbackPrefix*>(1);
  if(bad>=2){o.screenUI=&borrowed;if(bad==2)borrowed.storage.reset();if(bad==3)borrowed.drawList=nullptr;if(bad==4)borrowed.targetPasses=std::span<const ScreenRenderTargetPass>(reinterpret_cast<const ScreenRenderTargetPass*>(1),0);}
  try{Metal4WorldSubmission rejected(owned,o);assert(false);}catch(const std::logic_error&){}
 }
 for(unsigned i=0;i<1000;++i){auto p=std::make_shared<const Metal4WorldSubmission>(owned,valid);worker.Invoke([p]{assert(!p->Options().screenUI);});}
 std::cout<<"Owned world packet:move identity,screen descriptor/storage lifetime,borrowed rejection and1000 actual worker handoffs passed\n";
}
