#include "Rendering/Metal4/Metal4SceneBridge.h"
#include <cassert>
#include <iostream>
using rendering::scene::DrawResult;
int main(){
 // All existing five-field aggregate initializers retain their exact policy.
 for(unsigned bits=0;bits<8;++bits){DrawResult result{bool(bits&1),bool(bits&2),bool(bits&4),9,0};result.Validate();assert(!result.queued);assert(result.AdvanceCPUFrame()==bool(bits&1));}
 DrawResult accepted{false,false,false,0,0,true,9};accepted.Validate();assert(accepted.AdvanceCPUFrame());
 assert(!accepted.presented&&!accepted.sourceFrameCompleted&&!accepted.screenTargetsCompleted);
 for(unsigned bits=1;bits<8;++bits){auto invalid=accepted;invalid.presented=bits&1;invalid.screenTargetsCompleted=bits&2;invalid.sourceFrameCompleted=bits&4;bool rejected=false;try{invalid.AdvanceCPUFrame();}catch(const std::logic_error&){rejected=true;}assert(rejected);}
 auto badCompleted=accepted;badCompleted.completedPostprocessSerial=9;bool mixed=false;try{badCompleted.Validate();}catch(const std::logic_error&){mixed=true;}assert(mixed);
 auto orphan=accepted;orphan.queued=false;mixed=false;try{orphan.Validate();}catch(const std::logic_error&){mixed=true;}assert(mixed);
 accepted.queuedPostprocessSerial=0;bool rejected=false;try{accepted.Validate();}catch(const std::logic_error&){rejected=true;}assert(rejected);
 std::cout<<"Actual scene DrawResult: legacy aggregate matrix, accepted-not-presented CPU advancement, false GPU receipt rejection PASS\n";
}
