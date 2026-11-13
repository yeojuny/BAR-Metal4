#include "Rendering/Backend/NativeOwnedWorldGeometry.h"
#include <cassert>
#include <limits>
#include <string>
using namespace rendering;
static unsigned validations=0;static bool rejectSource=false;
namespace rendering {
// Source validator intentionally mocked here; prefix/raster checks are actual CPP.
void ValidateNativeWorldDrawCommand(const NativeWorldDrawCommand&){++validations;if(rejectSource)throw std::runtime_error("original-source-validation");}
}
int main(){NativeWorldDrawCommand d;d.world.inheritedClipPlane=std::array<float,4>{1,0,0,0};
 auto check=[&](std::array<double,4> vp,uint32_t w,uint32_t h,unsigned samples,const char* expected){unsigned before=validations;std::string error;try{ValidateNativeWorldGeometryPrefix(d,vp,w,h,samples);}catch(const std::runtime_error& e){error=e.what();}assert(validations==before+1&&error==expected);};
 check({0,0,32,32},32,32,1,"");
 rejectSource=true;check({0,0,-1,32},0,0,0,"original-source-validation");rejectSource=false;
 check({0,0,32,32},0,32,1,"Native world raster extent invalid");check({0,0,32,32},32,32,0,"Native world raster extent invalid");check({0,0,32,32},32,32,33,"Native world raster extent invalid");
 auto nan=std::numeric_limits<double>::quiet_NaN();check({nan,0,32,32},32,32,1,"Native world viewport non-finite");
 (*d.world.inheritedClipPlane)[0]=std::numeric_limits<float>::infinity();check({nan,0,32,32},32,32,1,"Native world viewport non-finite");check({0,0,32,32},32,32,1,"Native inherited clip plane non-finite");
 auto source=std::make_shared<const NativeWorldDrawCommand>(d);NativeOwnedWorldGeometry task(source,{{0,0,32,32},32,32,32,32,1});bool threw=false;try{task.SealIndependentClippedFillOnCaller();}catch(const std::runtime_error& e){threw=true;assert(std::string(e.what())=="Native inherited clip plane non-finite");}assert(threw&&!task.HasIndependentClippedFillSeal());
}
