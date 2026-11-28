#include "extracted_journal.h"
#include <cassert>
#include <iostream>
template<class F> void rejects(F f){bool rejected=false;try{f();}catch(const std::logic_error&){rejected=true;}assert(rejected);}
int main(){
 int runtime=0,other=0;auto preparation=std::make_shared<int>(0);
 using R=NativeTextureProducerReceipt; using J=WorldTexturePreparationJournal;
 R r;assert(!r.Readable(0)&&!r.Readable(1));r.Record(&runtime,1);assert(r.Readable(1)&&!r.Readable(2));
 rejects([&]{r.Borrow(&other,1,2,preparation);});rejects([&]{r.Borrow(&runtime,3,2,preparation);});rejects([&]{r.Borrow(&runtime,1,1,preparation);});rejects([&]{r.Borrow(&runtime,1,0,preparation);});
 auto permit=r.Borrow(&runtime,1,2,preparation);assert(r.Readable(2)&&!r.ready&&!r.Readable(3));assert(r.Borrow(&runtime,1,2,preparation)==permit);
 rejects([&]{r.Borrow(&runtime,1,3,preparation);});auto foreignPreparation=std::make_shared<int>(0);rejects([&]{r.Borrow(&runtime,1,2,foreignPreparation);});rejects([&]{r.RequireCompleted(1);});rejects([&]{r.Complete(2);});rejects([&]{r.Record(&other,1);});rejects([&]{r.Record(&runtime,2);});
 permit.reset();assert(!r.Readable(2)); // abandoned CPU preparation has no authority
 auto retry=r.Borrow(&runtime,1,2,preparation);r.Record(&runtime,1);r.Complete(1);r.Complete(1);r.RequireCompleted(1);assert(r.ready&&r.Readable(2));rejects([&]{r.RequireCompleted(2);});
 // Actual production journal, with only ObjC ownership syntax substituted.
 J producer,consumer;J::Entry e;e.owner=std::make_shared<WorldTexture>();e.texture=std::make_shared<int>(7);e.sampled=e.texture;e.status->uploadSource=std::make_shared<int>(8);
 std::weak_ptr<int> texture=e.texture,upload=e.status->uploadSource;
 producer.Register(e);producer.Register(e);assert(producer.entries.size()==1);
 e.status->Record(&runtime,10);auto finishProducer=producer.TakeCompletion(10);assert(producer.entries.empty());
 e.borrow=e.status->Borrow(&runtime,10,11,preparation);consumer.Register(e);assert(consumer.entries[0].borrow);
 std::function<void()> check=[e]{e.status->RequireCompleted(10);}; // actual priorReadChecks ownership shape
 auto finishConsumer=consumer.TakeCompletion(11);finishConsumer();assert(!e.status->ready);rejects(check);
 J recovered;recovered.Recover(consumer);assert(recovered.entries.empty());
 J interrupted;interrupted.Register(e);recovered.Recover(interrupted);assert(recovered.entries.empty());
 recovered.Complete(11);assert(!e.status->ready); // borrow cannot publish producer success
 interrupted.entries.clear();e.texture.reset();e.sampled.reset();e.owner.reset();e.borrow.reset();assert(!texture.expired()&&!upload.expired());
 finishProducer();check();assert(e.status->ready&&upload.expired());
 check={};assert(texture.expired());
 // Timeout/GPU failure means producer completion not invoked. Consumer-only
 // completion never makes it submit-ready, even though CPU encoding was allowed.
 J failedProducer,failedConsumer;J::Entry f;f.owner=std::make_shared<WorldTexture>();failedProducer.Register(f);f.status->Record(&runtime,20);auto failed=failedProducer.TakeCompletion(20);
 f.borrow=f.status->Borrow(&runtime,20,21,preparation);failedConsumer.Register(f);failedConsumer.TakeCompletion(21)();assert(!f.status->ready);rejects([&]{f.status->RequireCompleted(20);});
 // Validate whole journal before any state is marked ready on mismatch.
 J mixed;J::Entry a,b;a.owner=std::make_shared<WorldTexture>();b.owner=std::make_shared<WorldTexture>();mixed.Register(a);mixed.Register(b);a.status->Record(&runtime,30);b.status->Record(&runtime,31);auto bad=mixed.TakeCompletion(30);rejects(bad);assert(!a.status->ready&&!b.status->ready);
 // A retained interrupted journal cannot authorize or obstruct a new lease.
 J abandoned;J::Entry old;old.owner=std::make_shared<WorldTexture>();old.status->Record(&runtime,40);
 auto oldPreparation=std::make_shared<int>(0);old.borrow=old.status->Borrow(&runtime,40,41,oldPreparation);abandoned.Register(old);
 oldPreparation.reset();assert(!old.status->Readable(41));J resumed;resumed.Recover(abandoned);assert(resumed.entries.empty());
 auto newPermit=old.status->Borrow(&runtime,40,42,preparation);assert(old.status->Readable(42)&&!old.status->Readable(41));
 std::cout<<"receipt identity, permit expiry, journal role/recovery, exact pins, failure and atomic completion PASS\n";
}
