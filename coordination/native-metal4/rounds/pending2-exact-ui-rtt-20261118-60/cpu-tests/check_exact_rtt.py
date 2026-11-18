from pathlib import Path
import subprocess
p=Path.cwd(); src=(p/'source/engine/rts/Rendering/Metal4/Metal4ScreenNative.mm').read_text()
def section(begin,end):
    return src[src.index(begin):src.index(end,src.index(begin))]
state=section('struct ScreenTargetVersionEncoding {','using ScreenArgumentLayout')
stamp=section('const ScreenTargetVersionEncoding encoded{target.submission','\n}')
selection=section('const bool initialized=entry.initialized&&*entry.initialized;', '\n\t\t\tRequire(!source->renderTarget')
ordering=section('const auto producer=entry.encodedVersion->submission;', '\n\t\t\t\t}\n\t\t\t\tif(entry.sourceDependent')
assert src.index('[encoder endEncoding];\n\ttarget.encoded=true;') < src.index('const ScreenTargetVersionEncoding encoded{target.submission')
assert 'depth?depth->encodedVersion:nullptr,std::move(requirePreviousDepthReadable),lease.SourceSubmission()' in src
assert src.count('encodedVersion->submission==lease.SourceSubmission()')==2
assert 'Require(*completed || encoded->stored' in src
prefix=r'''
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
using std::uint64_t;
constexpr int MTLStorageModePrivate=1,MTLTextureType2D=2;
enum class NativePriorRead {ScreenTarget};
struct FakeTexture {
 bool allocated=true;
 int device=1,storageMode=MTLStorageModePrivate,textureType=MTLTextureType2D;
 int width=32,height=16,sampleCount=1,arrayLength=1,mipmapLevelCount=1;
 bool parentTexture=false,iosurface=false,buffer=false,heap=false;
 explicit operator bool()const{return allocated;}
};
struct LogicalTarget {struct Name {int resourceId=81;};std::shared_ptr<Name> renderTarget=std::make_shared<Name>();int generation=7,width=32,height=16;};
struct Lease {
 uint64_t consumer=82,liveSource=79,liveEvent=31;
 bool pending=true,slotAvailable=true;
 uint64_t SourceSubmission()const{return consumer;}
 bool PreparingWhilePending()const{return pending;}
 int Device()const{return 1;}
 uint64_t PendingEventForSource(uint64_t source)const{return source&&source==liveSource?liveEvent:0;}
 std::function<void()> ordered,serial;
 bool OrderGPUReadAfterSubmission(const void*,uint64_t source,uint64_t event,std::function<void()> check,NativePriorRead){
  if(!slotAvailable||source!=liveSource||event!=liveEvent)return false;
  ordered=std::move(check);return true;
 }
 void OrderAfterPreviousSubmission(const void*,std::function<void()> check,NativePriorRead){serial=std::move(check);}
};
void Require(bool x,const char* e){if(!x)throw std::runtime_error(e);}
'''
decl=r'''
struct CacheEntry {
 std::shared_ptr<bool> initialized=std::make_shared<bool>(false);
 std::shared_ptr<ScreenTargetVersionEncoding> encodedVersion=std::make_shared<ScreenTargetVersionEncoding>();
 FakeTexture image;
};
struct Target {uint64_t submission;bool stored;std::shared_ptr<ScreenTargetVersionEncoding> encodedVersion,depthEncoded;};
void Stamp(Target& target){
'''
func=r'''
}
bool Bind(const std::shared_ptr<LogicalTarget>& source,const CacheEntry& entry,Lease& lease) {
'''
end=r'''
 if(previousSubmission){
'''
suffix=r'''
 }
 return previousSubmission;
}
template<typename F>bool Throws(const F& f){try{f();return false;}catch(const std::runtime_error&){return true;}}
int main(){
 auto source=std::make_shared<LogicalTarget>();
 CacheEntry color,depth;
 Target target{79,true,color.encodedVersion,depth.encodedVersion};
 Stamp(target);
 assert(color.encodedVersion->stored&&depth.encodedVersion->stored&&color.encodedVersion->submission==79&&depth.encodedVersion->submission==79);
 Lease good;
 assert(Bind(source,color,good));assert(good.ordered&&!good.serial);
 assert(Throws(good.ordered));*color.initialized=true;good.ordered();
 ++source->generation;assert(Throws(good.ordered));--source->generation;
 ++source->renderTarget->resourceId;assert(Throws(good.ordered));--source->renderTarget->resourceId;
 color.encodedVersion->submission=78;assert(Throws(good.ordered));color.encodedVersion->submission=79;
 *color.initialized=false;
 for(int i=0;i<10;i++){
  auto bad=color; Lease fallback;
  switch(i){
   case 0:bad.image.parentTexture=true;break;
   case 1:bad.image.heap=true;break;
   case 2:bad.image.iosurface=true;break;
   case 3:bad.image.buffer=true;break;
   case 4:bad.image.storageMode=3;break;
   case 5:bad.image.mipmapLevelCount=2;break;
   case 6:bad.image.arrayLength=2;break;
   case 7:bad.image.device=2;break;
   case 8:fallback.liveSource=999;break;
   case 9:fallback.slotAvailable=false;break;
  }
  assert(Bind(source,bad,fallback));assert(!fallback.ordered&&fallback.serial);
 }
 Lease same;same.consumer=79;assert(!Bind(source,color,same)&&!same.ordered&&!same.serial);
 Lease cold;cold.pending=false;assert(!Bind(source,color,cold));
 Lease retired;*color.initialized=true;assert(!Bind(source,color,retired));
 target.stored=false;Stamp(target);assert(!Bind(source,depth,retired));
}
'''
file=p/'cpu-tests/extracted.cpp';file.write_text(prefix+state+decl+stamp+func+selection+end+ordering+suffix)
subprocess.run(['/usr/bin/clang++','-std=c++23','-O1','-g','-fsanitize=address,undefined',str(file),'-o',str(file.parent/'check')],check=True)
subprocess.run([str(file.parent/'check')],check=True)
print('PASS: actual extracted RTT writer stamp, private exact path, success and foreign generation/source, 10 serialized fallback cases, same-frame and discarded output')
