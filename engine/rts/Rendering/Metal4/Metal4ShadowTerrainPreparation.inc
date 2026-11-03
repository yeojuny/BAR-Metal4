// A CPU preparation owner, not a GPU completion receipt. The caller holds it
// across the existing retirement boundary. It owns exact immutable source
// data and strong buffer references, including entries evicted from the cache.
struct ShadowTerrainPreparation {
 const Runtime* context=nullptr;
 __strong id<MTLDevice> device=nil;
 std::shared_ptr<const NativeTerrainShadowFrame> source;
 std::vector<ShadowTerrainGeometryBuffers> geometry;
 ShadowTerrainGeometryCache::Statistics statistics;
 std::uint64_t nanoseconds=0,sourceBytes=0;
 bool Matches(const Runtime* selectedContext,id<MTLDevice> selectedDevice,
  const std::shared_ptr<const NativeTerrainShadowFrame>& selected) const {
  return context==selectedContext&&device==selectedDevice&&source==selected&&
   source&&geometry.size()==source->draws.size();
 }
 static std::shared_ptr<const ShadowTerrainPreparation> Prepare(ShadowTerrainGeometryCache& cache,
  const Runtime* context,id<MTLDevice> device,std::shared_ptr<const NativeTerrainShadowFrame> source,bool measure) {
  Require(source&&source->drawInvoked&&source->contextGeneration&&source->sourceDrawFrame,
   "Early MAP shadow preparation has no immutable original frame");
  auto result=std::make_shared<ShadowTerrainPreparation>();
  result->context=context;result->device=device;result->source=std::move(source);
  const auto began=measure?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
  result->geometry.reserve(result->source->draws.size());
  for(const auto& draw:result->source->draws) {
   const auto ordinal=result->geometry.size();
   result->geometry.push_back(cache.Acquire(context,device,result->source->contextGeneration,
    {draw.texSquare[0],draw.texSquare[1],draw.indexed,ordinal},draw,&result->statistics));
   result->sourceBytes+=draw.Positions().size()*sizeof(draw.Positions()[0])+draw.Indices().size()*sizeof(std::uint32_t);
  }
  if(measure)result->nanoseconds=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-began).count();
  return result;
 }
};
