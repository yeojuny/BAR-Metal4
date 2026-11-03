// One loop-local validation scope, not an address cache or public receipt.
// The retained source selection owns this command throughout Prepare. Only
// command-owned values are reused; independently owned typed inputs revalidate.
// BEGIN WORLD_BORROW168_EXTRACT
struct WorldBorrowMetrics168 {
 std::uint64_t firstValidations=0,borrowCandidates=0,reusedFullValidations=0,typedRevalidations=0;
 std::uint64_t vertexElementsCovered=0,indexElementsCovered=0,payloadBytesCovered=0;
};
class ScopedWorldBorrow168 final {
 const NativeWorldDrawCommand& source;
 WorldBorrowMetrics168& metrics;
 bool HasIndependentValidationOwner()const {
  return source.originalShadowInput||source.easyFacing||source.squadHull||source.terraformOverlay||source.losView||
   source.waterOverlay||source.unitEffect||source.unitGlass||source.distortion||source.objectSpotlight||source.grass||
   source.resourceSpots||source.sensorLOS||source.sourceLines||source.reclaimField||source.surface.terraformCapture||
   source.surface.diffusePainter||source.surface.splatPainter||source.surface.decalCapture||source.surface.deferredVisualizer;
 }
public:
 explicit ScopedWorldBorrow168(const NativeWorldDrawCommand& command,WorldBorrowMetrics168& work):source(command),metrics(work) {
  PreparationTraceScope validationProbe91(35);ValidateNativeWorldDrawCommand(source);++metrics.firstValidations;
 }
 ScopedWorldBorrow168(const ScopedWorldBorrow168&)=delete;
 ScopedWorldBorrow168& operator=(const ScopedWorldBorrow168&)=delete;
 ScopedWorldBorrow168(ScopedWorldBorrow168&&)=delete;
 ScopedWorldBorrow168& operator=(ScopedWorldBorrow168&&)=delete;
 bool Borrow(const std::array<double,4>& viewport,uint32_t width,uint32_t height,unsigned samples)const {
  if(source.primitive!=NativeDrawPrimitive::Triangles ||
   source.world.frontPolygonMode!=NativePolygonMode::Fill ||
   source.world.backPolygonMode!=NativePolygonMode::Fill || source.world.inheritedClipPlane)return false;
  ++metrics.borrowCandidates;
  if(HasIndependentValidationOwner()){
   ValidateNativeWorldDrawCommand(source);++metrics.typedRevalidations;
  }else{
   // This exact command was fully validated by this scope before the caller's
   // sequence/viewport work. The closed triangle path has no command writes,
   // producer callback or typed-owner dereference between validation and here.
   ++metrics.reusedFullValidations;
   metrics.vertexElementsCovered+=source.vertices.size();metrics.indexElementsCovered+=source.indices.size();
   metrics.payloadBytesCovered+=source.vertices.size()*sizeof(NativeWorldDrawVertex)+source.indices.size()*sizeof(uint32_t);
  }
  Require(samples&&samples<=32&&width&&height&&width<=16384&&height<=16384&&viewport[2]>0&&viewport[3]>0,"Native world raster extent invalid");
  for(double value:viewport)Require(std::isfinite(value),"Native world viewport non-finite");
  return true;
 }
};
// END WORLD_BORROW168_EXTRACT
