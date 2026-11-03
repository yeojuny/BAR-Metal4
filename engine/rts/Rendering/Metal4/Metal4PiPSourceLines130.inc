// Cache the selected immutable source interval, independent of unused VBO
// capacity. One current interval per source;replaced buffers remain lease-owned.
class PiPSourceVertexCache130 {
 struct Entry {std::weak_ptr<const NativeBufferSnapshot> owner;uint32_t first=0,count=0;__strong id<MTLBuffer> buffer=nil;};
 std::unordered_map<const NativeBufferSnapshot*,Entry> entries;
public:
 void BeginFrame(){std::erase_if(entries,[](const auto& e){return e.second.owner.expired();});}
 void Clear(){entries.clear();}
 id<MTLBuffer> Acquire(RuntimeLease& lease,const NativePiPDraw& source){
  const size_t first=source.firstVertex,count=source.vertexCount,bytes=count*24;
  Require(source.vertices&&count&&first<=source.vertices->bytes.size()/24&&count<=source.vertices->bytes.size()/24-first&&bytes<=lease.Device().maxBufferLength,"PiP selected source interval exceeds immutable storage/device limit");
  auto& entry=entries[source.vertices.get()];
  if(entry.owner.expired()||entry.first!=first||entry.count!=count){
   auto buffer=[lease.Device()newBufferWithBytes:source.vertices->bytes.data()+first*24 length:bytes options:MTLResourceStorageModeShared];Require(buffer,"Cannot retain selected original PiP source vertices");
   entry={source.vertices,source.firstVertex,source.vertexCount,buffer};
  }
  lease.AddAllocation(entry.buffer);return entry.buffer;
 }
};
// Only original uniforms and raster metadata change per draw. The selected
// vertex buffer starts at zero;no CPU vertex expression is evaluated here.
struct PiPSourceInputs130 {
 __strong id<MTLBuffer> buffer=nil;
 static constexpr size_t RasterOffset=sizeof(NativePiPUniformBlock),Bytes=RasterOffset+sizeof(pipSource130::RasterUniform);
 static_assert(RasterOffset==592&&Bytes==640);
};
PiPSourceInputs130 PreparePiPSourceInputs130(RuntimeLease& lease,const NativePiPDraw& source,const pipSource130::RasterUniform& raster){
 PiPSourceInputs130 out;out.buffer=[lease.Device()newBufferWithLength:PiPSourceInputs130::Bytes options:MTLResourceStorageModeShared];Require(out.buffer&&out.buffer.contents,"Cannot allocate immutable PiP source line inputs");lease.AddAllocation(out.buffer);
 auto selected=raster;selected.options[1]=0;
 std::memcpy(out.buffer.contents,&source.program.uniforms,sizeof(source.program.uniforms));std::memcpy(static_cast<uint8_t*>(out.buffer.contents)+PiPSourceInputs130::RasterOffset,&selected,sizeof(selected));return out;
}
