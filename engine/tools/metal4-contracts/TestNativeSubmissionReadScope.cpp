#include "Rendering/Backend/NativeSubmissionReadScope.h"
#include <cassert>
#include <functional>
#include <iostream>
using namespace rendering;
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
 std::cout<<"1000 normal/exception, nested rejection, retained exact identity cycles passed\n";
}
