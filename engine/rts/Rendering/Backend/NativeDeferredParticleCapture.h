/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <stdexcept>
namespace rendering {
class NativeDeferredParticleCapture {
 enum class State {NotDeferred,Pending,Entered,Complete,Failed};
 State state=State::NotDeferred;
public:
 void Defer(){if(state!=State::NotDeferred)throw std::logic_error("Particle capture already deferred");state=State::Pending;}
 void RequireComplete()const{if(state!=State::Complete&&state!=State::NotDeferred)throw std::logic_error("Partial particle capture cannot be submitted");}
 template<class Capture> void Complete(Capture&& capture){
  if(state!=State::Pending)throw std::logic_error("Deferred particle writer is absent, repeated or failed");
  state=State::Entered;
  try{capture();state=State::Complete;}catch(...){state=State::Failed;throw;}
 }
};
}
