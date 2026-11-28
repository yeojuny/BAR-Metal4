static void TraceTextureOrdered(NativeTextureOrderedTraceEvent event,uint64_t producer=0,uint64_t consumer=0) noexcept {
#ifdef RECOIL_METAL4_LIVE_SCENE
 static const bool enabled=[] {const auto* value=std::getenv("BAR_NATIVE_TEXTURE_ORDERED_TRACE");return value&&std::string_view(value)=="1";}();
 if(!enabled)return;
 static NativeTextureOrderedTraceBudget budget;
 if(!budget.Take(event))return;
 static const char* names[]={"armed","admitted","encode_permit_check","producer_retired_check"};
 try {LOG("[NativeTextureOrderedTrace] event=%s producer=%llu consumer=%llu gpu_pending_limit=1 diagnostic_only=1 performance_valid=0 per_event_cap=%u",
  names[unsigned(event)],(unsigned long long)producer,(unsigned long long)consumer,event==NativeTextureOrderedTraceEvent::Armed?1u:64u);}catch(...){/* Diagnostics must not affect receipt or submission semantics. */}
#else
 (void)event;(void)producer;(void)consumer;
#endif
}
