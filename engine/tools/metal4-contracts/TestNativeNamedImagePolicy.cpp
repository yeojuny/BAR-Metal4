/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Textures/NativeNamedImagePolicy.h"
#include "Rendering/Textures/TextureCreationParams.hpp"
#include <bit>
#include <iostream>
#include <limits>
using namespace rendering;
using namespace rendering::ui;
namespace {
unsigned checks=0;
void Check(bool x,const char* why){++checks;if(!x)throw std::runtime_error(why);}
template<class F>void Reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}Check(failed,"Invalid sampler/qualifier was accepted");}
}
int main(){try{
 for(unsigned bits=0;bits<256;++bits){
  std::string prefix=":";const char flags[]="nlamcigb";for(unsigned bit=0;bit<8;++bit)if(bits&(1u<<bit))prefix+=flags[bit];prefix+=':';
  const auto q=ParseNativeUITextureQualifiers(prefix+"original.png");
  Check(q.anisotropic==bool(bits&4)&&q.inverted==bool(bits&32)&&q.whiteBorder==bool(bits&128),"Original a/i/b parse differs");
  for(bool dds:{false,true})for(unsigned side:{1u,2u,3u,48u,512u,16384u})for(unsigned embedded:{0u,1u,2u,5u}){
   const auto policy=GetNativeNamedImagePolicy(q,dds,side,side/2+1,embedded,16);
   GL::TextureCreationParams original;original.linearMipMapFilter=!q.mipNear;original.linearTextureFilter=!q.nearest;
   const bool need=(!(q.nearest||q.linear))||q.mipNear;
   const auto levels=dds?embedded:(need?std::bit_width(std::max(side,side/2+1)):1u);
   const auto min=original.GetMinFilter(levels),mag=original.GetMagFilter();
   const auto expectedFilter=(min==0x2600||min==0x2700)?ScreenFilter::Nearest:ScreenFilter::Linear;
   const auto expectedMip=(min==0x2600||min==0x2601)?ScreenMipFilter::None:(min==0x2700||min==0x2701)?ScreenMipFilter::Nearest:ScreenMipFilter::Linear;
   Check(policy.sampler.filter==expectedFilter&&policy.sampler.magnificationFilter.value_or(policy.sampler.filter)==(mag==0x2600?ScreenFilter::Nearest:ScreenFilter::Linear)&&policy.sampler.mipFilter==expectedMip,"Compiled original TextureCreationParams filter mismatch");
   Check(policy.generateMipmaps==(dds?(embedded==0&&min!=0x2600&&min!=0x2601):(levels>1)),"Original HandleDDSMipmap/ordinary producer decision differs");
   Check(policy.sampler.wrapS==((!dds&&q.clamped)?ScreenWrap::ClampEdge:ScreenWrap::Repeat)&&policy.sampler.wrapT==policy.sampler.wrapS,"DDS clamp bypass or bitmap wrapping differs");
   Check(policy.sampler.whiteBorder==(!dds&&q.whiteBorder)&&policy.bitmapOperations==!dds,"DDS bitmap/border bypass differs");
   Check(policy.sampler.anisotropy==(q.anisotropic?16u:1u),"Original maximum anisotropy differs");
  }
 }
 const auto fog=ParseNativeUITextureQualifiers(":a:LuaRules\\Images\\fog.png");const auto p=GetNativeNamedImagePolicy(fog,false,512,512,0,16);
 Check(p.generateMipmaps&&p.sampler.anisotropy==16&&p.sampler.mipFilter==ScreenMipFilter::Linear&&p.sampler.wrapS==ScreenWrap::Repeat,"Actual Seths fog policy differs");
 const auto b=GetNativeNamedImagePolicy(ParseNativeUITextureQualifiers(":b:real.png"),false,512,512,0,16);
 Check(b.sampler.whiteBorder&&b.sampler.wrapS==ScreenWrap::Repeat,"Original b changes border color, not wrapping");
 const auto combined=ParseNativeUITextureQualifiers(":ir32,48gt0.3,0.4,0.5:real.png");Check(combined.inverted&&combined.greyed&&combined.resize&&combined.tint,"Original transform flags not retained");
 for(auto n:{0u,17u,std::numeric_limits<unsigned>::max()})Reject([&]{GetNativeNamedImagePolicy(fog,false,32,32,0,n);});
 Reject([]{ParseNativeUITextureQualifiers(":x:bad.png");});
 Reject([]{ScreenSampler s;s.anisotropy=0;ValidateScreenSampler(s);});Reject([]{ScreenSampler s;s.anisotropy=17;ValidateScreenSampler(s);});
 // Actual reached base-only BC paths keep the approved existing native behavior.
 for(const auto format:{ScreenTextureFormat::BC1RGBA,ScreenTextureFormat::BC2RGBA,ScreenTextureFormat::BC3RGBA}){
  const auto ready=GetNativeNamedImageUploadPolicy(ParseNativeUITextureQualifiers(":lc:indicator.dds"),format,true,48,48,0,16);
  Check(!ready.generateMipmaps&&ready.originalCompressedMipmapsPending&&ready.sampler.mipFilter==ScreenMipFilter::None&&ready.sampler.wrapS==ScreenWrap::ClampEdge,"Reached Ready base-only BC behavior regressed");
  const auto glow=GetNativeNamedImageUploadPolicy(ParseNativeUITextureQualifiers("glow.dds"),format,true,32,32,0,16);
  Check(!glow.generateMipmaps&&glow.originalCompressedMipmapsPending&&glow.sampler.mipFilter==ScreenMipFilter::Linear&&glow.sampler.wrapS==ScreenWrap::Repeat,"Reached FlowUI base-only BC behavior regressed");
  const auto embedded=GetNativeNamedImageUploadPolicy(ParseNativeUITextureQualifiers(":cib:original.dds"),format,true,32,32,5,16);
  Check(!embedded.generateMipmaps&&!embedded.originalCompressedMipmapsPending&&embedded.sampler.wrapS==ScreenWrap::Repeat&&!embedded.sampler.whiteBorder&&!embedded.bitmapOperations,"Embedded DDS original bypass policy changed");
  ScreenTexture source;source.width=source.height=32;source.format=format;source.generateMipmaps=true;Reject([&]{ValidateScreenTextureMipGeneration(source);});
  source.generateMipmaps=false;ValidateScreenTextureMipGeneration(source);Check(true,"existing bounded BC base allowed");
 }
 {ScreenTexture t;t.width=t.height=32;t.generateMipmaps=true;ValidateScreenTextureMipGeneration(t);Check(true,"valid original uncompressed base rejected");
  t.mips.resize(2);Reject([&]{ValidateScreenTextureMipGeneration(t);});t.mips.clear();t.incomplete=true;Reject([&]{ValidateScreenTextureMipGeneration(t);});t.incomplete=false;t.renderTarget=std::make_shared<ScreenRenderTargetIdentity>();Reject([&]{ValidateScreenTextureMipGeneration(t);});}
 std::cout<<"PASS checks="<<checks<<" sourceOriginalGetMinFilter=1 actualSethsFog512=1\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL checks="<<checks<<" "<<e.what()<<'\n';return 1;}}
