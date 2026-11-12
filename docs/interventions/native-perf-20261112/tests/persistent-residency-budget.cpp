#include "Rendering/Backend/NativePersistentResidencyBudget.h"
#include <cassert>
#include <limits>
using rendering::NativePersistentResidencyBudget;
int main(){
 NativePersistentResidencyBudget b;
 assert(!b.CanAdd(0));assert(!b.CanAdd(std::numeric_limits<uint64_t>::max()));
 assert(b.CanAdd(b.ByteLimit));b.Added(b.ByteLimit);assert(!b.CanAdd(1));
 b={};for(size_t i=0;i<b.AllocationLimit;++i){assert(b.CanAdd(1));b.Added(1);}assert(!b.CanAdd(1));
 b={};for(size_t i=0;i<b.SetLimit;++i){assert(b.CanAdd(1));b.Added(1);b.Sealed();}assert(!b.CanAdd(1));
 b={};assert(b.CanAdd(8));b.Added(8);assert(b.CanAdd(b.ByteLimit-8));
}
