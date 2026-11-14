#include "Rendering/Backend/NativeTerrainMatrixMemo.h"
#include <cassert>
#include <stdexcept>
#include <iostream>
using namespace rendering;
int main(){
 using Matrix=std::array<float,16>;Matrix a{};for(unsigned i=0;i<16;i+=5)a[i]=1;
 NativeTerrainMatrixMemo<Matrix> memo;unsigned count=0;
 auto compute=[&]{++count;return a;};
 for(unsigned i=0;i<100;++i){assert(memo.Get(a,compute)==a);}
 assert(count==1);
 auto b=a;b[1]=-0.0f;memo.Get(b,[&]{++count;return b;});assert(count==2);
 memo.Get(a,compute);assert(count==3); // A->B->A deliberately rebuilds.
 for(unsigned i=0;i<16;++i){b=a;b[i]=float(i+2);assert(memo.Get(b,[&]{++count;return b;})==b);}assert(count==19);
 memo.Get(a,compute);const auto before=count;
 b=a;b[0]=2;bool rejected=false;
 try{memo.Get(b,[&]()->Matrix{throw std::runtime_error("invalid inverse");});}catch(const std::runtime_error&){rejected=true;}
 assert(rejected);assert(memo.Get(a,compute)==a&&count==before);
 NativeTerrainMatrixMemo<Matrix> nextPrepare;nextPrepare.Get(a,compute);assert(count==before+1);
 std::cout<<"Terrain matrix memo: exact 16-scalar key, 100 repeats/one compute, signed zero, A/B/A, failure and Prepare lifetime passed\n";
}
