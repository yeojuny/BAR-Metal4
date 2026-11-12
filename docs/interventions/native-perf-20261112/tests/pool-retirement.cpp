// Separate intervention: exercise the actual ownership gate used by RetiredArguments.
#include "Rendering/Backend/NativeRetiredResourcePool.h"
#include <cassert>
#include <memory>
struct Value {unsigned id;unsigned resets=0;};
int main(){
 rendering::NativeRetiredResourcePool<unsigned,Value,2> pool;
 unsigned created=0;
 auto acquire=[&](unsigned key){return pool.Acquire(key,[&]{return Value{++created,0};},[](Value& v){++v.resets;});};
 pool.BeginBatch();auto a=acquire(7);auto b=acquire(7);assert(a!=b&&pool.CachedCount()==2);
 // In-flight owners keep mutable tables busy across batches.
 pool.BeginBatch();auto c=acquire(7);assert(c!=a&&c!=b&&pool.CachedCount()==2);
 auto completion=a;a.reset();pool.BeginBatch();auto d=acquire(7);assert(d!=completion&&d!=b);
 // Completion releases its token; only then can the exact slot be reset/reused.
 unsigned old=completion->id;completion.reset();pool.BeginBatch();auto e=acquire(7);assert(e->id==old&&e->resets==1);
 auto other=acquire(8);assert(other!=e);
 pool.Clear();assert(pool.CachedCount()==0);assert(e->id==old); // clear cannot invalidate consumers
 pool.BeginBatch();auto f=acquire(7);assert(f!=e);
}
