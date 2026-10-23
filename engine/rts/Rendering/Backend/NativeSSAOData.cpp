/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeSSAOData.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include <exception>
namespace rendering {
namespace {void Check(bool c,const char*s){if(!c)throw std::runtime_error(s);}bool Integral(float x){return std::isfinite(x)&&std::floor(x)==x;}}
void NativeSSAOImageVersion::Fulfill(std::shared_ptr<const NativeSSAOImageResource> r)const{Check(bool(r),"Original SSAO image resource is missing");Check(!resource||resource==r,"Original SSAO immutable image resource was replaced");resource=std::move(r);}
void ValidateNativeSSAODefines(const NativeSSAODefines&d){
 static const std::map<std::string,std::pair<float,float>,std::less<>> range={
 {"DEPTH_CLIP01",{0,1}},{"MERGE_MISC",{0,0}},{"SSAO_FIBONACCI",{0,1}},{"SSAO_KERNEL_MINZ",{0,.2f}},{"SSAO_RANDOM_LENGTH",{.2f,3}},{"SSAO_KERNEL_SIZE",{1,64}},
 {"SSAO_RADIUS",{0,1e6f}},{"SSAO_RADIUS_FAR_SCALE",{1,8}},{"SSAO_MIN",{0,4}},{"SSAO_OCCLUSION_POWER",{0,16}},{"SSAO_FADE_DIST_1",{0,1e9f}},{"SSAO_FADE_DIST_0",{0,1e9f}},
 {"DEBUG_SSAO",{0,1}},{"BRIGHTEN",{0,255}},{"BLUR_HALF_KERNEL_SIZE",{1,12}},{"BLUR_SIGMA",{1,10}},{"MINCOSANGLE",{-3,1}},{"ZTHRESHOLD",{0,4.f/255}},
 {"MINSELFWEIGHT",{0,1}},{"OUTLIERCORRECTIONFACTOR",{0,1}},{"BLUR_POWER",{1,8}},{"BLUR_CLAMP",{0,1}},{"DEBUG_BLUR",{0,1}},{"USE_STENCIL",{0,1}},
 {"DOWNSAMPLE",{1,3}},{"ENABLE",{0,1}},{"SLOWFUSE",{0,1}},{"NOFUSE",{0,1}},{"SSAO_ALPHA_POW",{0,1e6f}},
 {"BLUR_COMPOSITE",{0,1}},{"TILE_PX",{32,32}},{"TILE_TEXELS",{1,32}},{"TILE_CULL",{1,1}},
 {"VSX",{1,16777216}},{"VSY",{1,16777216}},{"HSX",{1,16777216}},{"HSY",{1,16777216}},{"TEXPADDINGX",{0,3}},{"TEXPADDINGY",{0,3}}};
 for(auto&[key,value]:d){auto i=range.find(key);Check(i!=range.end(),"Unknown original SSAO define");Check(std::isfinite(value),"Original SSAO define outside supported source domain");}
 if(d.empty())return;
 for(auto&[key,_]:range)if(key!="TILE_CULL")Check(d.contains(key),"Original SSAO define missing");
 for(auto key:{"SSAO_KERNEL_SIZE","BLUR_HALF_KERNEL_SIZE","DOWNSAMPLE","VSX","VSY","HSX","HSY","DEPTH_CLIP01","USE_STENCIL","NOFUSE","DEBUG_SSAO","DEBUG_BLUR"})Check(Integral(d.at(key)),"Original SSAO integer define is fractional");
 Check(d.at("SSAO_KERNEL_SIZE")>=1&&d.at("SSAO_KERNEL_SIZE")<=64&&d.at("BLUR_HALF_KERNEL_SIZE")>=1&&d.at("BLUR_HALF_KERNEL_SIZE")<=12,"Original SSAO array capacity exceeded");
 Check(d.at("VSX")>0&&d.at("VSY")>0&&d.at("HSX")>0&&d.at("HSY")>0&&d.at("DOWNSAMPLE")>0,"Original SSAO image divisor/extent is nonpositive");
 Check(d.at("HSX")==std::ceil(d.at("VSX")/d.at("DOWNSAMPLE"))&&d.at("HSY")==std::ceil(d.at("VSY")/d.at("DOWNSAMPLE")),"Original SSAO ceil extent differs");
 Check(d.at("TEXPADDINGX")==d.at("DOWNSAMPLE")*d.at("HSX")-d.at("VSX")&&d.at("TEXPADDINGY")==d.at("DOWNSAMPLE")*d.at("HSY")-d.at("VSY"),"Original SSAO padding differs");
 Check((d.at("BLUR_COMPOSITE")==0||d.at("BLUR_COMPOSITE")==1)&&d.at("TILE_PX")==32&&
  Integral(d.at("TILE_TEXELS"))&&d.at("TILE_TEXELS")>=1&&d.at("TILE_TEXELS")<=32,
  "Original31657 SSAO blur/tile defines differ");
 if(d.contains("TILE_CULL"))Check(d.at("TILE_CULL")==1&&d.at("USE_STENCIL")==1,"SSAO tile culling lacks its actual stencil program");
}
void ValidateNativeSSAODraw(const NativeSSAODraw&d){
 ValidateNativeSSAODefines(d.defines);Check(d.sequence!=0,"SSAO has no actual world order");
 if(d.clear){Check(bool(d.output),"Private SSAO clear lacks actual FBO");for(float x:d.clearColor)Check(std::isfinite(x),"SSAO clear nonfinite");return;}
 Check(d.program==NativeSSAOProgram::TexRect||!d.defines.empty(),"SSAO program lacks original defines");
 if(d.program!=NativeSSAOProgram::TexRect)Check((d.program==NativeSSAOProgram::BlurComposite)==(d.defines.at("BLUR_COMPOSITE")==1),"SSAO fused blur program/defines differ");
 if(d.program==NativeSSAOProgram::BlurComposite)Check(d.defines.at("DOWNSAMPLE")==1&&!d.output,"SSAO fused vertical blur requires the original full-resolution screen target");
 if(d.tiles){
  Check(d.defines.contains("TILE_CULL")&&!d.tiles->instances.empty(),"SSAO indexed tiles lack the original tiled shader/instances");
  const auto columns=size_t(std::ceil(d.defines.at("VSX")/d.defines.at("TILE_PX")));
  const auto rows=size_t(std::ceil(d.defines.at("VSY")/d.defines.at("TILE_PX")));
  Check(d.tiles->instances.size()==columns*rows,"SSAO tile uploads differ from the original screen grid");
 }else Check(!d.defines.contains("TILE_CULL"),"SSAO tile shader cannot silently use a full-screen substitute");
 for(auto&v:d.vertices)for(float x:v)Check(std::isfinite(x),"SSAO vertex nonfinite");
 for(auto&v:d.uniforms.samplingKernel)for(float x:v)Check(std::isfinite(x),"SSAO kernel nonfinite");
 for(float x:d.uniforms.weights)Check(std::isfinite(x),"SSAO weight nonfinite");for(float x:d.uniforms.offsets)Check(std::isfinite(x),"SSAO offset nonfinite");
 if(d.output)Check(d.output->image&&d.output->generation&&d.output->producerSequence==d.sequence,"SSAO output token differs from actual producer");
 for(auto&i:d.inputs)Check(!(i.local&&i.actual.object),"SSAO sampler has two unrelated owners");
 Check(!d.world.inheritedClipPlane,"Original SSAO full-screen VS explicitly writes positive clip distances");
}
std::array<std::array<float,4>,6> MakeNativeSSAOTexRect(float x0,float y0,float x1,float y1,float u0,float v0,float u1,float v1){return {{{x0,y0,u0,v0},{x1,y1,u1,v1},{x0,y1,u0,v1},{x1,y1,u1,v1},{x0,y0,u0,v0},{x1,y0,u1,v0}}};}
void NativeSSAORecorder::Begin(uint64_t n,std::shared_ptr<const NativeCUSUniformPublication>u){Check(!frame,"Nested SSAO frame");frame=std::make_shared<NativeSSAOFrame>();frame->sourceDrawFrame=n;frame->publication=std::move(u);}
NativeSSAOFrame::~NativeSSAOFrame(){if(!completed)for(auto i=rollback.rbegin();i!=rollback.rend();++i)try{(*i)();}catch(...){std::terminate();}}
void NativeSSAORecorder::Record(NativeSSAODraw d){RecordTransactional(std::move(d),{},{});}
void NativeSSAORecorder::RecordTransactional(NativeSSAODraw d,std::function<void()> publish,std::function<void()> rollback){Check(bool(frame),"SSAO draw outside actual source callback");Check(bool(frame->publication),"Actual SSAO draw has no original uniform publication");ValidateNativeCUSUniformPublication(*frame->publication);if(!d.sequence)d.sequence=NextNativeWorldDrawSequence();ValidateNativeSSAODraw(d);frame->draws.reserve(frame->draws.size()+1);if(rollback)frame->rollback.reserve(frame->rollback.size()+1);if(publish)publish();frame->draws.push_back(std::move(d));if(rollback)frame->rollback.push_back(std::move(rollback));}
std::shared_ptr<const NativeSSAOFrame> NativeSSAORecorder::End(std::optional<NativeWorldDrawState> exit){Check(bool(frame),"SSAO End without Begin");frame->exitWorldState=std::move(exit);return std::exchange(frame,{});}
void NativeSSAORecorder::DisableClipDistance(unsigned i){Check(frame&&i<3,"SSAO clip-distance disable outside actual frame/source range");frame->exitClipDisableMask|=uint8_t(1u<<i);}
void NativeSSAORecorder::Abort()noexcept{frame.reset();}
}
