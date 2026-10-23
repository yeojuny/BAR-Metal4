/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeBloomData.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include <exception>
namespace rendering {
namespace {void Check(bool c,const char*s){if(!c)throw std::runtime_error(s);}bool Integral(float x){return std::isfinite(x)&&std::floor(x)==x;}}
void NativeBloomImageVersion::Fulfill(std::shared_ptr<const NativeBloomImageResource> r)const{Check(bool(r),"Original Bloom image resource is missing");Check(!resource||resource==r,"Original Bloom immutable image resource was replaced");resource=std::move(r);}
void ValidateNativeBloomDefines(const NativeBloomDefines& d){
 static const std::array<const char*,10> names={"VSX","VSY","HSX","HSY","IHSX","IHSY","PADX","PADY","DOWNSCALE","MIPCOUNT"};
 Check(d.size()==names.size(),"Original Bloom defines missing/extra");for(auto key:names)Check(d.contains(key)&&std::isfinite(d.at(key)),"Unknown/nonfinite original Bloom define");
 for(auto key:{"VSX","VSY","HSX","HSY","DOWNSCALE","MIPCOUNT"})Check(Integral(d.at(key)),"Original Bloom integer define fractional");
 Check(d.at("VSX")>=4&&d.at("VSY")>=4&&d.at("DOWNSCALE")>=1&&d.at("DOWNSCALE")<=3&&d.at("MIPCOUNT")>=4&&d.at("MIPCOUNT")<=6,"Original Bloom allocation domain differs");
 Check(d.at("HSX")==std::ceil(d.at("VSX")/d.at("DOWNSCALE"))&&d.at("HSY")==std::ceil(d.at("VSY")/d.at("DOWNSCALE")),"Original Bloom ceil extent differs");
 Check(d.at("PADX")==d.at("DOWNSCALE")*d.at("HSX")-d.at("VSX")&&d.at("PADY")==d.at("DOWNSCALE")*d.at("HSY")-d.at("VSY"),"Original Bloom padding differs");
}
void ValidateNativeBloomDraw(const NativeBloomDraw&d){
 ValidateNativeBloomDefines(d.defines);Check(d.sequence!=0,"Bloom has no actual world order");
 Check(!d.clear,"Original Bloom never requests a private clear operation");

 for(auto&v:d.vertices)for(float x:v)Check(std::isfinite(x),"Bloom vertex nonfinite");
 for(float x:{d.uniforms.sourceTexelSize[0],d.uniforms.sourceTexelSize[1],d.uniforms.illuminationThreshold,d.uniforms.kneeWidth,d.uniforms.glowAmplifier,d.uniforms.maxBrightContribution,d.uniforms.filterRadius,d.uniforms.historyMix,d.uniforms.bloomNorm})Check(std::isfinite(x),"Original Bloom uniform nonfinite");
 if(d.output)Check(d.output->image&&d.output->generation&&d.output->producerSequence==d.sequence,"Bloom output token differs from actual producer");
 for(auto&i:d.inputs)Check(!(i.local&&i.actual.object),"Bloom sampler has two unrelated owners");
 Check(!d.world.inheritedClipPlane,"Original Bloom main World path has no inherited user plane");
}
std::array<std::array<float,4>,6> MakeNativeBloomTexRect(float x0,float y0,float x1,float y1,float u0,float v0,float u1,float v1){return {{{x0,y0,u0,v0},{x1,y1,u1,v1},{x0,y1,u0,v1},{x1,y1,u1,v1},{x0,y0,u0,v0},{x1,y0,u1,v0}}};}
void NativeBloomRecorder::Begin(uint64_t n,std::shared_ptr<const NativeCUSUniformPublication>u){Check(!frame,"Nested Bloom frame");frame=std::make_shared<NativeBloomFrame>();frame->sourceDrawFrame=n;frame->publication=std::move(u);}
NativeBloomFrame::~NativeBloomFrame(){if(!completed)for(auto i=rollback.rbegin();i!=rollback.rend();++i)try{(*i)();}catch(...){std::terminate();}}
void NativeBloomRecorder::Record(NativeBloomDraw d){RecordTransactional(std::move(d),{},{});}
void NativeBloomRecorder::RecordTransactional(NativeBloomDraw d,std::function<void()> publish,std::function<void()> rollback){Check(bool(frame),"Bloom draw outside actual source callback");if(!d.sequence)d.sequence=NextNativeWorldDrawSequence();ValidateNativeBloomDraw(d);frame->draws.reserve(frame->draws.size()+1);if(rollback)frame->rollback.reserve(frame->rollback.size()+1);if(publish)publish();frame->draws.push_back(std::move(d));if(rollback)frame->rollback.push_back(std::move(rollback));}
std::shared_ptr<const NativeBloomFrame> NativeBloomRecorder::End(){Check(bool(frame),"Bloom End without Begin");return std::exchange(frame,{});}
void NativeBloomRecorder::Abort()noexcept{frame.reset();}
}
