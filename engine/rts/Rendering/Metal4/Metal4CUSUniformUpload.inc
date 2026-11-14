// Original per-draw bytes in distinct aligned slices owned through completion.
struct CUSUniformUpload {
 struct Slice {
  __strong id<MTLBuffer> buffer=nil;
  size_t offset=0,length=0;
  std::span<uint8_t> Bytes() const {return {static_cast<uint8_t*>(buffer.contents)+offset,length};}
  MTLGPUAddress Address() const {return buffer.gpuAddress+offset;}
 } vertex,fragment;
};
CUSUniformUpload UploadCUSUniforms(RuntimeLease& lease,ModelUploadArena146& arena,
 const Program& program,const NativeCUSSourceDraw& draw,const NativeCUSSampleTransport& transport)
{
 const auto vertexSize=NativeCUSProgramUniformBytes(program.generated->vertex);
 const auto fragmentSize=NativeCUSProgramUniformBytes(program.generated->fragment);
 const size_t limit=lease.Device().maxBufferLength;
 Require(vertexSize&&fragmentSize&&vertexSize<=limit&&fragmentSize<=limit,
  "CUS original buffer exceeds actual native device capacity");
 // GPU constant-block starts remain 256-byte aligned; padding is not an input.
 const auto fragmentOffset=(vertexSize+255u)&~size_t(255u);
 CUSUniformUpload out;
 if(fragmentOffset>=vertexSize&&fragmentOffset<=limit&&fragmentSize<=limit-fragmentOffset){
  auto slice=arena.Allocate(lease,fragmentOffset+fragmentSize);
  out={{slice.buffer,slice.offset,vertexSize},{slice.buffer,slice.offset+fragmentOffset,fragmentSize}};
 }else{
  // Preserve individually valid large blocks even if their pair will not fit.
  auto vertex=arena.Allocate(lease,vertexSize),fragment=arena.Allocate(lease,fragmentSize);
  out={{vertex.buffer,vertex.offset,vertexSize},{fragment.buffer,fragment.offset,fragmentSize}};
 }
 program.vertexUniformPlan->Pack(draw,*program.source,transport,out.vertex.Bytes());
 program.fragmentUniformPlan->Pack(draw,*program.source,transport,out.fragment.Bytes());
 return out;
}
