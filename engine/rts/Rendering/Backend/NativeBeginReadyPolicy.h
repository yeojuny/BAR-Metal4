/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering {
// Initial encoding-tail pilot, not preparation overlap or general U5 completion.
struct NativeBeginReadyPolicy {
 bool requested=false,worker=false,owned=false,visible=false,admitted=false;
 bool sourceCredit=false,deferred=false,screen=false,cpuSettlement=false;
 bool readback=false,continuation=false,dynamicWater=false,legacyWater=false;
 bool arbitraryCallback=false,previousRetirement=false;
 // Diagnostic only: bit i identifies the unmet field in declaration order.
 unsigned RejectionMask(bool preparation=false)const noexcept{
  const bool pass[]={requested,worker,owned,visible,admitted,sourceCredit,deferred,screen,cpuSettlement,
   !readback,!continuation,!dynamicWater,!legacyWater,!arbitraryCallback,preparation||!previousRetirement};
  unsigned mask=0;for(unsigned i=0;i<15;++i)if(!pass[i])mask|=1u<<i;return mask;
 }
 bool Eligible()const noexcept{
  return requested&&worker&&owned&&visible&&admitted&&sourceCredit&&deferred&&screen&&cpuSettlement&&
   !readback&&!continuation&&!dynamicWater&&!legacyWater&&!arbitraryCallback&&!previousRetirement;
 }
 // Prepare release retains the older GPU submission. Its retirement callback
 // is only an atomic worker notice; the main-thread action remains after Join.
 bool EligibleForPreparation()const noexcept{auto policy=*this;policy.previousRetirement=false;return policy.Eligible();}
};
}
