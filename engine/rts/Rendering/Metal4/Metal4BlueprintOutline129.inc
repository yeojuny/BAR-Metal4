// Immutable family inputs; the existing lease owns this buffer until retirement.
struct BlueprintOutlineInputs129 {
 __strong id<MTLBuffer> buffer=nil;
 static constexpr size_t AlphaOffset=sizeof(blueprintOutline129::Uniform),InstancesOffset=AlphaOffset+32;
 static_assert(AlphaOffset==144&&InstancesOffset==176);
};
BlueprintOutlineInputs129 PrepareBlueprintOutlineInputs129(RuntimeLease& lease,const NativeBlueprintOutlineDraw& source,const blueprintOutline129::Uniform& input){
 const size_t bytes=BlueprintOutlineInputs129::InstancesOffset+size_t(source.instanceCount)*36;
 Require(bytes<=lease.Device().maxBufferLength,"Blueprint outline input buffer exceeds device limit");
 BlueprintOutlineInputs129 packet;packet.buffer=[lease.Device()newBufferWithLength:bytes options:MTLResourceStorageModeShared];Require(packet.buffer&&packet.buffer.contents,"Cannot allocate immutable Blueprint outline inputs");lease.AddAllocation(packet.buffer);
 const std::array<uint32_t,4> flags{uint32_t(source.surface.alphaTest.enabled),uint32_t(source.surface.alphaTest.compare),0,0};const std::array<float,4> alpha{source.surface.alphaTest.reference,0,0,0};
 auto bytesOut=static_cast<uint8_t*>(packet.buffer.contents);std::memcpy(bytesOut,&input,sizeof(input));std::memcpy(bytesOut+BlueprintOutlineInputs129::AlphaOffset,flags.data(),16);std::memcpy(bytesOut+BlueprintOutlineInputs129::AlphaOffset+16,alpha.data(),16);std::memcpy(bytesOut+BlueprintOutlineInputs129::InstancesOffset,source.instances->bytes.data()+size_t(source.firstInstance)*36,size_t(source.instanceCount)*36);return packet;
}
