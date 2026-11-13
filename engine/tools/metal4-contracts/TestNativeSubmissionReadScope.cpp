#include "Rendering/Backend/NativeSubmissionReadScope.h"
#include <cassert>
#include <functional>
#include <iostream>
using namespace rendering;
class RuntimeLease {
 NativeSubmissionReadIdentity sourceSubmission;
public:
 #include "lease-submission.inc"
};
int main() {
 NativeSubmissionReadBinding binding;
 assert(binding.Current()==0);
 try {NativeSubmissionReadBinding::Scope bad(binding,0);assert(false);}catch(const std::logic_error&){}
 assert(binding.Current()==0);
 for(unsigned i=1;i<=1000;++i){
  std::function<std::uint64_t()> retained;
  try {
   NativeSubmissionReadBinding::Scope scope(binding,i);
   assert(binding.Current()==i);
   retained=[submission=binding.Current()]{return submission;};
   try {NativeSubmissionReadBinding::Scope overlap(binding,i+1);assert(false);}catch(const std::logic_error&){}
   assert(binding.Current()==i);
   if(i%2)throw std::runtime_error("prepare or encoding failure");
  }catch(const std::runtime_error&){}
  assert(binding.Current()==0);
  NativeSubmissionReadBinding::Scope next(binding,1000+i);
  assert(retained()==i && binding.Current()==1000+i);
 }
 assert(binding.Current()==0);
 for(unsigned i=1;i<=1000;++i){
  RuntimeLease a,b,standalone;
  assert(!a.SourceSubmission()&&!b.SourceSubmission()&&!standalone.SourceSubmission());
  try{a.BindSourceSubmission(0);assert(false);}catch(const std::logic_error&){}
  assert(!a.SourceSubmission());a.BindSourceSubmission(i);
  b.BindSourceSubmission(i+1000);
  const auto captured=[submission=a.SourceSubmission()]{return submission;};
  for(auto invalid:{0u,i,i+1}){try{a.BindSourceSubmission(invalid);assert(false);}catch(const std::logic_error&){}assert(a.SourceSubmission()==i);}
  assert(captured()==i&&b.SourceSubmission()==i+1000&&!standalone.SourceSubmission());
  // The unchanged provider predicates accept completed images outside a request,
  // never an unretired producer under a stale previous renderer identity.
  const auto readable=[](bool ready,uint64_t encoded,uint64_t current){return ready||(current&&encoded==current);};
  assert(readable(true,i,standalone.SourceSubmission()));
  assert(!readable(false,i,standalone.SourceSubmission()));
  assert(readable(false,i,a.SourceSubmission())&&!readable(false,i,b.SourceSubmission()));
 }
 std::cout<<"1000 normal/exception, nested rejection, retained exact identity cycles; 1000 lease A/B, zero/rebind rejection and standalone zero cycles passed\n";
}
