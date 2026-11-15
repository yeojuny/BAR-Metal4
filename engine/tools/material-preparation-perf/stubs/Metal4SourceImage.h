#pragma once
#include "NativeCUSTextureHistory.h"
#include <stdexcept>
#define __strong
#define __bridge
template<class T> using id=T*;
struct MTLDevice {};
namespace rendering::metal4 {
class CUSCompletedModelImageProof;
struct CUSSourceImage {
 std::shared_ptr<const void> owner;
 std::shared_ptr<const CUSCompletedModelImageProof> completedModelProof;
 int token=0;
};
struct RuntimeLease {
 const void* context=this;
 MTLDevice* device=nullptr;
 uint64_t submission=1;
 std::shared_ptr<int> journal=std::make_shared<int>(0);
 const void* ContextIdentity()const{return context;}
 MTLDevice* Device()const{return device;}
 uint64_t SourceSubmission()const{return submission;}
 std::shared_ptr<int> WorldTexturePreparation(){return journal;}
 void ValidatePreparationMutation()const{}
};
inline void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
}
