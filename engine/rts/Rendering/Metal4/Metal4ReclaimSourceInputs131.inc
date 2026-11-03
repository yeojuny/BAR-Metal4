// One immutable uniform block. Original VBO/SSBO images use the paired cache.
struct ReclaimSourceInputs131 {
 __strong id<MTLBuffer> buffer=nil;
 static constexpr size_t RasterOffset=sizeof(reclaimSource131::Uniform);
 static_assert(RasterOffset==256);
};
ReclaimSourceInputs131 PrepareReclaimSourceInputs131(RuntimeLease& lease,const reclaimSource131::Uniform& original,const std::optional<reclaimSource131::Raster>& raster){
 const size_t bytes=ReclaimSourceInputs131::RasterOffset+(raster?sizeof(*raster):0);ReclaimSourceInputs131 out;
 out.buffer=[lease.Device()newBufferWithLength:bytes options:MTLResourceStorageModeShared];Require(out.buffer&&out.buffer.contents,"Cannot retain original Reclaim uniform inputs");lease.AddAllocation(out.buffer);
 std::memcpy(out.buffer.contents,&original,sizeof(original));if(raster)std::memcpy(static_cast<uint8_t*>(out.buffer.contents)+ReclaimSourceInputs131::RasterOffset,&*raster,sizeof(*raster));return out;
}
