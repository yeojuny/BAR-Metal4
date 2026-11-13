#include "Rendering/Backend/NativeResourceProducerEpoch.h"
#include <cassert>
#include <functional>
#include <iostream>
using namespace rendering;
struct Resource {NativeResourceProducerEpochs epochs; bool completed=false;};
int main() {
 for(int i=0;i<1000;++i) {
  auto a=std::make_shared<Resource>(),b=std::make_shared<Resource>();
  auto epoch=a->epochs.Begin(a->completed);
  // Early and late consumers capture exactly the same selected resource epoch.
  auto early=[a,e=a->epochs.Current()]{return e->Encoded();};
  auto late=[a,e=a->epochs.Current()]{return e->Encoded();};
  assert(!early()&&!late());
  auto unrelated=b->epochs.Begin(false);unrelated->DidEncode();
  assert(!early()); // Negative control: B must never satisfy A.
  epoch->DidEncode();assert(early()&&late());
  auto continuation=[a,e=a->epochs.Current()]{return e->Encoded();};
  assert(continuation()&&!a->completed); // Encoding != completion.
  a->completed=true;
  auto completed=a->epochs.Begin(a->completed);assert(completed->Encoded());
  // A new attempt cannot reset previously captured completed/encoded receipts.
  auto retry=a->epochs.Begin(false);assert(!retry->Encoded()&&early()&&continuation());
  auto abandoned=retry;
  auto next=a->epochs.Begin(false);next->DidEncode();
  assert(!abandoned->Encoded()&&next->Encoded());
  // Same-resource retries have independent receipts; immutable completion may seed a new one.
  auto standalone=a->epochs.Current();assert(standalone==next);
  std::weak_ptr<Resource> weak=a;
  a.reset();assert(!weak.expired()); // Consumer keeps physical image alive.
 }
 std::cout<<"1000 resource/epoch lifecycle cases passed\n";
}
