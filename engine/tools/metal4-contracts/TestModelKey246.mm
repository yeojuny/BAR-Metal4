#import <Metal/Metal.h>
#include "Rendering/Metal4/Metal4ModelPipelineKey246.h"
#include "Rendering/Backend/ScreenDrawList.h"
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <cassert>
#include <locale>
#include <cstdio>
using namespace rendering;using namespace rendering::metal4;
struct Target {std::array<uint64_t,5>colors{MTLPixelFormatRGBA8Unorm,0,0,0,0};uint32_t samples=1;uint64_t combinedColor=0,depth=MTLPixelFormatDepth32Float,stencil=0;uint32_t width=1512,height=982;};
static int owners[32];
struct CInput{const void*p=&owners[0];Target t;ScreenBlendState b;std::array<bool,4>mask{true,true,true,true};bool validation=false,coverage=false,shade=false,identity=false,forceFragmentRelaxed=false,indirect=false;uint64_t sampler=0;};
uint64_t Mask(const std::array<bool,4>&m){return(m[0]?MTLColorWriteMaskRed:0)|(m[1]?MTLColorWriteMaskGreen:0)|(m[2]?MTLColorWriteMaskBlue:0)|(m[3]?MTLColorWriteMaskAlpha:0);}
std::vector<uint64_t> Legacy(const CInput&i){const auto&t=i.t;const auto&b=i.b;std::vector<uint64_t> k{uint64_t(i.p),t.samples,uint64_t(i.validation),uint64_t(b.enabled),uint64_t(b.sourceColor),uint64_t(b.destinationColor),uint64_t(b.sourceAlpha),uint64_t(b.destinationAlpha),uint64_t(b.colorEquation),uint64_t(b.alphaEquation),Mask(i.mask)};for(auto c:t.colors)k.push_back(c);k.push_back(i.coverage);k.push_back(t.combinedColor);k.push_back(i.shade);k.push_back(i.identity);k.push_back(i.sampler);k.push_back(i.forceFragmentRelaxed);k.push_back(i.indirect);return k;}
auto MakeFixed(const CInput&i){return pipelinekey246::CUS(i.p,i.t,i.b,Mask(i.mask),i.validation,i.coverage,i.shade,i.identity,i.sampler,i.forceFragmentRelaxed,i.indirect);}
struct FInput{bool modern=false,validation=false;uint32_t image=0;Target t;ScreenDrawState surface;uint64_t sampleKey=1;};
std::string Legacy(const FInput&i){const auto&t=i.t;const auto&s=i.surface;const auto&b=s.blend;std::ostringstream k;k<<i.modern<<':'<<i.validation<<':'<<i.image<<':'<<t.colors[0]<<':'<<t.depth<<':'<<t.stencil<<':'<<i.sampleKey<<':'<<b.enabled<<':'<<unsigned(b.sourceColor)<<':'<<unsigned(b.destinationColor)<<':'<<unsigned(b.sourceAlpha)<<':'<<unsigned(b.destinationAlpha)<<':'<<unsigned(b.colorEquation)<<':'<<unsigned(b.alphaEquation);for(bool w:s.colorWrite)k<<':'<<w;k<<':'<<s.alphaToCoverage;return k.str();}
auto MakeFixed(const FInput&i){return pipelinekey246::Forward(i.modern,i.validation,i.image,i.t,i.surface,i.sampleKey);}
template<class Input>void Check(const std::vector<Input>&inputs,unsigned&pairs,unsigned&hits,unsigned&misses){using Old=decltype(Legacy(inputs.front()));using New=decltype(MakeFixed(inputs.front()));std::vector<Old>old;std::vector<New>now;for(auto&i:inputs){old.push_back(Legacy(i));now.push_back(MakeFixed(i));}for(size_t a=0;a<old.size();++a)for(size_t b=0;b<old.size();++b){assert((old[a]==old[b])==(now[a]==now[b]));++pairs;}
 std::map<Old,unsigned>legacy;std::map<New,unsigned>fixed;for(unsigned pass=0;pass<3;++pass)for(size_t n=0;n<old.size();++n){auto a=legacy.find(old[n]);auto b=fixed.find(now[n]);assert((a==legacy.end())==(b==fixed.end()));if(a==legacy.end()){unsigned id=unsigned(legacy.size()+1);legacy.emplace(old[n],id);fixed.emplace(now[n],id);++misses;}else{assert(a->second==b->second);++hits;}}}
int main(){unsigned pairs=0,hits=0,misses=0,fieldCases=0;std::vector<CInput>cs;std::vector<FInput>fs;CInput cb;cb.b.enabled=false;FInput fb;fb.surface.blend.enabled=false;cs.push_back(cb);fs.push_back(fb);
 for(unsigned field=0;field<23;++field){auto c=cb;switch(field){case 0:c.p=&owners[1];break;case 1:c.t.samples=2;break;case 2:c.validation=true;break;case 3:c.b.enabled=true;break;case 4:c.b.sourceColor=ScreenBlendFactor::One;break;case 5:c.b.destinationColor=ScreenBlendFactor::One;break;case 6:c.b.sourceAlpha=ScreenBlendFactor::One;break;case 7:c.b.destinationAlpha=ScreenBlendFactor::One;break;case 8:c.b.colorEquation=ScreenBlendEquation::Maximum;break;case 9:c.b.alphaEquation=ScreenBlendEquation::Minimum;break;case 10:c.mask[0]=false;break;case 16:c.coverage=true;break;case 17:c.t.combinedColor=MTLPixelFormatRGBA16Float;break;case 18:c.shade=true;break;case 19:c.identity=true;break;case 20:c.sampler=UINT64_MAX;break;case 21:c.forceFragmentRelaxed=true;break;case 22:c.indirect=true;break;default:c.t.colors[field-11]=MTLPixelFormatRG16Float;break;}assert(Legacy(c)!=Legacy(cb)&&MakeFixed(c)!=MakeFixed(cb));cs.push_back(c);++fieldCases;}
 for(unsigned field=0;field<19;++field){auto f=fb;switch(field){case 0:f.modern=true;break;case 1:f.validation=true;break;case 2:f.image=UINT32_MAX;break;case 3:f.t.colors[0]=MTLPixelFormatRGBA16Float;break;case 4:f.t.depth=MTLPixelFormatDepth16Unorm;break;case 5:f.t.stencil=MTLPixelFormatStencil8;break;case 6:f.sampleKey=UINT64_MAX;break;case 7:f.surface.blend.enabled=true;break;case 8:f.surface.blend.sourceColor=ScreenBlendFactor::One;break;case 9:f.surface.blend.destinationColor=ScreenBlendFactor::One;break;case 10:f.surface.blend.sourceAlpha=ScreenBlendFactor::One;break;case 11:f.surface.blend.destinationAlpha=ScreenBlendFactor::One;break;case 12:f.surface.blend.colorEquation=ScreenBlendEquation::Maximum;break;case 13:f.surface.blend.alphaEquation=ScreenBlendEquation::Minimum;break;case 18:f.surface.alphaToCoverage=true;break;default:f.surface.colorWrite[field-14]=false;break;}assert(Legacy(f)!=Legacy(fb)&&MakeFixed(f)!=MakeFixed(fb));fs.push_back(f);++fieldCases;}
 {auto c=cb;c.t.depth=123;c.t.stencil=456;c.t.width=100;c.t.height=200;assert(Legacy(c)==Legacy(cb)&&MakeFixed(c)==MakeFixed(cb));++fieldCases;auto f=fb;f.t.combinedColor=999;f.t.colors[4]=888;f.t.width=1;f.t.samples=32;assert(Legacy(f)==Legacy(fb)&&MakeFixed(f)==MakeFixed(fb));++fieldCases;}
 uint64_t random=0x123456789abcdefULL;auto rnd=[&](){random^=random<<13;random^=random>>7;return random^=random<<17;};
 for(unsigned n=0;n<512;++n){CInput c;c.p=&owners[rnd()%32];c.t.samples=1+rnd()%32;for(auto&v:c.t.colors)v=rnd()%512;c.t.combinedColor=rnd()%512;c.b.enabled=rnd()%2;c.b.sourceColor=ScreenBlendFactor(rnd()%11);c.b.destinationColor=ScreenBlendFactor(rnd()%11);c.b.sourceAlpha=ScreenBlendFactor(rnd()%11);c.b.destinationAlpha=ScreenBlendFactor(rnd()%11);c.b.colorEquation=ScreenBlendEquation(rnd()%5);c.b.alphaEquation=ScreenBlendEquation(rnd()%5);for(auto&w:c.mask)w=rnd()%2;c.validation=rnd()%2;c.coverage=rnd()%2;c.shade=rnd()%2;c.identity=rnd()%2;c.sampler=rnd();c.forceFragmentRelaxed=rnd()%2;c.indirect=rnd()%2;cs.push_back(c);if(n%7==0)cs.push_back(c);
 FInput f;f.modern=rnd()%2;f.validation=rnd()%2;f.image=uint32_t(rnd());f.t=c.t;f.surface.blend=c.b;for(auto&w:f.surface.colorWrite)w=rnd()%2;f.surface.alphaToCoverage=rnd()%2;f.sampleKey=rnd();fs.push_back(f);if(n%7==0)fs.push_back(f);}
 // Additional representation-width cases; these prove scalar encoding,
 // not admission of invalid render targets by unchanged production guards.
 for(unsigned i=0;i<5;++i){auto c=cb;c.t.colors[i]=UINT64_MAX-i;cs.push_back(c);++fieldCases;}
 {auto c=cb;c.t.combinedColor=1ull<<63;cs.push_back(c);++fieldCases;
 auto f=fb;f.t.colors[0]=1ull<<63;f.t.depth=UINT64_MAX;f.t.stencil=(1ull<<40)+7;f.image=UINT32_MAX;f.sampleKey=UINT64_MAX;fs.push_back(f);++fieldCases;}
 // Both current precision and indirect flags must partition a shared program/target.
 for(bool relaxed:{false,true})for(bool indirect:{false,true}){auto c=cb;c.forceFragmentRelaxed=relaxed;c.indirect=indirect;cs.push_back(c);++fieldCases;}
 for(const auto& c:cs){auto old=Legacy(c);auto fixed=MakeFixed(c);assert(old.size()==fixed.size());assert(std::equal(old.begin(),old.end(),fixed.begin()));}
 Check(cs,pairs,hits,misses);Check(fs,pairs,hits,misses);
 std::map<pipelinekey246::CUSKey,unsigned>cuA,cuB;cuA.emplace(MakeFixed(cb),111);cuB.emplace(MakeFixed(cb),222);assert(cuA.at(MakeFixed(cb))==111&&cuB.at(MakeFixed(cb))==222);
 std::map<pipelinekey246::ForwardKey,unsigned>ownerA,ownerB;ownerA.emplace(MakeFixed(fb),123);ownerB.emplace(MakeFixed(fb),456);assert(ownerA.at(MakeFixed(fb))==123&&ownerB.at(MakeFixed(fb))==456);
 static_assert(sizeof(pipelinekey246::CUSKey)==23*sizeof(uint64_t));static_assert(sizeof(pipelinekey246::ForwardKey)==19*sizeof(uint64_t));
 std::printf("PASS fieldCases=%u bidirectionalEqualityPairs=%u repeatedCacheHits=%u repeatedCacheMisses=%u ownerPartition=PASS locale=%s\n",fieldCases,pairs,hits,misses,std::locale().name().c_str());
}
