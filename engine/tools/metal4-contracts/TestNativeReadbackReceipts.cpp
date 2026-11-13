#include "Rendering/Backend/NativeUIRecorder.h"
#include <cassert>
#include <iostream>
using namespace rendering;using namespace rendering::ui;
int main(){
 NativeUITargetPassJournal journal;
 ScreenRenderTargetPass pass;pass.destination=std::make_shared<ScreenTexture>();pass.depthDestination=std::make_shared<ScreenTexture>();pass.commandSequence=1;journal.Append(pass);
 NativeUIReadbackPrefix before;before.targets=journal.Passes();before.targetReceipts=journal.Receipts();assert(before.TargetActualSubmission(pass)==0);
 journal.Record(std::span<const ScreenRenderTargetPass>(&pass,1),91);
 NativeUIReadbackPrefix after;after.targets=journal.Passes();after.targetReceipts=journal.Receipts();assert(after.TargetActualSubmission(pass)==91);
 journal.Clear();assert(before.TargetActualSubmission(pass)==0&&after.TargetActualSubmission(pass)==91);
 auto mismatch=pass;mismatch.depthDestination=std::make_shared<ScreenTexture>();try{after.TargetActualSubmission(mismatch);assert(false);}catch(const std::logic_error&){}
 NativeUIReadbackPrefix missing;try{missing.TargetActualSubmission(pass);assert(false);}catch(const std::logic_error&){}
 // A just-closed target prefix adds a fresh zero receipt independent of older
 // submitted versions, retaining both exact color and depth identities.
 ScreenRenderTargetPass next;next.destination=std::make_shared<ScreenTexture>();next.depthDestination=std::make_shared<ScreenTexture>();next.commandSequence=2;
 after.targets.push_back(next);after.targetReceipts.push_back({next.destination,next.depthDestination,0});assert(after.TargetActualSubmission(next)==0&&after.TargetActualSubmission(pass)==91);
 for(unsigned i=0;i<1000;++i){auto copy=after;assert(copy.TargetActualSubmission(pass)==91&&copy.TargetActualSubmission(next)==0);}
 std::cout<<"Readback exact receipt snapshots: unsubmitted/submitted, journal retirement, depth mismatch, missing receipt, continuation and1000 copies passed\n";
}
