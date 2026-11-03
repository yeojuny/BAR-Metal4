// Diagnostic metadata only. Weak keys must never extend a GPU resource lifetime.
#ifdef RECOIL_METAL4_LIVE_SCENE
namespace {
struct ResourceMemoryLedger {
 const bool enabled=[] {const auto* p=std::getenv("BAR_NATIVE_MEMORY_CENSUS");return p&&p[0]=='1'&&p[1]=='\0';}();
 std::mutex mutex;
 __strong NSMapTable* entries=nil;
 bool finished=false;
 uint64_t began=0,observed=0,gameplayBegan=0;
 int64_t gameplayFirstFrame=0;
 __strong NSMutableDictionary* churn=nil;
};
ResourceMemoryLedger& MemoryLedger(){static ResourceMemoryLedger value;return value;}
id<MTLAllocation> MemoryLedgerRoot(id<MTLAllocation> value)
{
 // Canonicalization is for reporting only: residency membership is untouched.
 for(unsigned depth=0;depth<32&&[value conformsToProtocol:@protocol(MTLTexture)];++depth){
  auto texture=(id<MTLTexture>)value;
  if(texture.parentTexture){value=texture.parentTexture;continue;}
  if(texture.buffer)value=texture.buffer;
  break;
 }
 return value;
}
void TrackResourceMemory(id<MTLAllocation> allocation)
{
 auto& state=MemoryLedger();if(!state.enabled)return;
 std::lock_guard<std::mutex> lock(state.mutex);if(state.finished)return;
 @autoreleasepool {
  const auto now=SubmissionTraceNow();if(!state.began)state.began=now;
  if(now-state.began>uint64_t{300}*1000000000){state.finished=true;state.entries=nil;return;}
  if(!state.entries)state.entries=[[NSMapTable alloc] initWithKeyOptions:NSPointerFunctionsWeakMemory|NSPointerFunctionsObjectPointerPersonality valueOptions:NSPointerFunctionsStrongMemory capacity:1024];
  auto root=MemoryLedgerRoot(allocation);if([state.entries objectForKey:root])return;
  if(state.entries.count>=32768){state.finished=true;state.entries=nil;return;}
  const bool texture=[root conformsToProtocol:@protocol(MTLTexture)],buffer=[root conformsToProtocol:@protocol(MTLBuffer)];
  auto image=texture?(id<MTLTexture>)root:nil;auto bytes=buffer?(id<MTLBuffer>)root:nil;
  NSString* label=[root respondsToSelector:@selector(label)]?[(id<MTLResource>)root label]:@"";
  label=label?:@"";if(label.length>120)label=[label substringToIndex:120];
  label=[[label stringByReplacingOccurrencesOfString:@"\n" withString:@" "] stringByReplacingOccurrencesOfString:@"\r" withString:@" "];
  const auto mode=[root respondsToSelector:@selector(storageMode)]?[(id<MTLResource>)root storageMode]:MTLStorageModePrivate;
  const bool heap=[root respondsToSelector:@selector(heap)]&&[(id<MTLResource>)root heap]!=nil;
  [state.entries setObject:@{@"bytes":@(root.allocatedSize),@"kind":texture?@"texture":buffer?@"buffer":@"other",@"label":label,@"width":@(image.width),@"height":@(image.height),@"format":@(image.pixelFormat),@"mips":@(image.mipmapLevelCount),@"array":@(image.arrayLength),@"length":@(bytes.length),@"mode":@(mode),@"heap":@(heap)} forKey:root];
  ++state.observed;
  if(state.gameplayBegan){
   NSString* family=label;
   const NSRange generation=[family rangeOfString:@" generation "];
   if(generation.location!=NSNotFound)family=[family substringToIndex:generation.location];
   NSString* key=texture?[NSString stringWithFormat:@"texture|%@|%llux%llu format=%llu layers=%llu mips=%llu",family,(unsigned long long)image.width,(unsigned long long)image.height,(unsigned long long)image.pixelFormat,(unsigned long long)image.arrayLength,(unsigned long long)image.mipmapLevelCount]:[NSString stringWithFormat:@"%@|%@",buffer?@"buffer":@"other",family];
   if(!state.churn[key] && state.churn.count>=127)key=@"overflow|all remaining labels";
   NSDictionary* old=state.churn[key];
   state.churn[key]=@{@"count":@([old[@"count"] unsignedLongLongValue]+1),@"bytes":@([old[@"bytes"] unsignedLongLongValue]+root.allocatedSize)};
  }
 }
}
void SnapshotResourceMemory(id<MTLDevice> device,int64_t simulationFrame,id<MTL4CommandAllocator> allocator=nil)
{
 auto& state=MemoryLedger();if(!state.enabled||simulationFrame<=0)return;
 std::lock_guard<std::mutex> lock(state.mutex);if(state.finished)return;
 if(!state.gameplayBegan){state.gameplayBegan=SubmissionTraceNow();state.gameplayFirstFrame=simulationFrame;state.churn=[NSMutableDictionary new];}
 if(simulationFrame<300)return;
 state.finished=true;
 @autoreleasepool {
  auto rows=[NSMutableArray new];uint64_t total=0,textureBytes=0,bufferBytes=0,heapCount=0;
  for(id key in state.entries){NSDictionary* row=[state.entries objectForKey:key];if(!row)continue;[rows addObject:row];const auto n=[row[@"bytes"] unsignedLongLongValue];total+=n;
   if([row[@"kind"] isEqual:@"texture"])textureBytes+=n;else if([row[@"kind"] isEqual:@"buffer"])bufferBytes+=n;heapCount+=[row[@"heap"] boolValue];
  }
  [rows sortUsingComparator:^NSComparisonResult(NSDictionary* a,NSDictionary* b){return [b[@"bytes"] compare:a[@"bytes"]];}];
  LOG("[NativeMemoryLedger] frame=%lld live_roots=%llu observed_roots=%llu root_bytes=%llu texture_bytes=%llu buffer_bytes=%llu heap_roots=%llu device_allocated_bytes=%llu recommended_working_set=%llu command_allocator_bytes=%llu",
   (long long)simulationFrame,(unsigned long long)rows.count,(unsigned long long)state.observed,(unsigned long long)total,(unsigned long long)textureBytes,(unsigned long long)bufferBytes,(unsigned long long)heapCount,(unsigned long long)device.currentAllocatedSize,(unsigned long long)device.recommendedMaxWorkingSetSize,(unsigned long long)[allocator allocatedSize]);
  for(NSUInteger i=0;i<MIN(rows.count,NSUInteger(48));++i){NSDictionary* r=rows[i];
   LOG("[NativeMemoryRoot] rank=%llu bytes=%llu kind=%s mode=%llu width=%llu height=%llu format=%llu mips=%llu array=%llu length=%llu heap=%d label=%s",
    (unsigned long long)i,[r[@"bytes"] unsignedLongLongValue],[r[@"kind"] UTF8String],[r[@"mode"] unsignedLongLongValue],[r[@"width"] unsignedLongLongValue],[r[@"height"] unsignedLongLongValue],[r[@"format"] unsignedLongLongValue],[r[@"mips"] unsignedLongLongValue],[r[@"array"] unsignedLongLongValue],[r[@"length"] unsignedLongLongValue],int([r[@"heap"] boolValue]),[r[@"label"] UTF8String]);
  }
  auto keys=[state.churn.allKeys sortedArrayUsingComparator:^NSComparisonResult(NSString* a,NSString* b){return [state.churn[b][@"bytes"] compare:state.churn[a][@"bytes"]];}];
  uint64_t observedBytes=0,observedCount=0;
  for(NSString* key in keys){observedBytes+=[state.churn[key][@"bytes"] unsignedLongLongValue];observedCount+=[state.churn[key][@"count"] unsignedLongLongValue];}
  LOG("[NativeResourceChurn] first_frame=%lld last_frame=%lld elapsed_ms=%.3f observed_roots=%llu observed_bytes=%llu groups=%llu",(long long)state.gameplayFirstFrame,(long long)simulationFrame,double(SubmissionTraceNow()-state.gameplayBegan)/1000000.0,(unsigned long long)observedCount,(unsigned long long)observedBytes,(unsigned long long)keys.count);
  for(NSUInteger i=0;i<MIN(keys.count,NSUInteger(32));++i){NSString* key=keys[i];LOG("[NativeResourceChurnOwner] rank=%llu observed_roots=%llu observed_bytes=%llu owner=%s",(unsigned long long)i,[state.churn[key][@"count"] unsignedLongLongValue],[state.churn[key][@"bytes"] unsignedLongLongValue],key.UTF8String);}
  state.entries=nil;state.churn=nil;
 }
}
} // namespace
#else
void TrackResourceMemory(id<MTLAllocation>) {}
void SnapshotResourceMemory(id<MTLDevice>,int64_t,id<MTL4CommandAllocator> =nil) {}
#endif
