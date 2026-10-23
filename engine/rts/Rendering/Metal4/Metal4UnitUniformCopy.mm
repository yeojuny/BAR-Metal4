/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeUnitUniformCopy.h"
#include <cstring>
#include <limits>

namespace rendering {
namespace {
using namespace metal4;
// Literal original 128-byte std140 member offsets; no whole-struct assignment:
// the original kernel deliberately leaves unused2..6 in the destination intact.
const char* UnitCopySource=R"MSL(
#include <metal_stdlib>
using namespace metal;
struct UnitUniform {
 uint composite,unused2,unused3,unused4;
 float maxHealth,health,unused5,unused6;
 float4 drawPos,speed,userDefined[4];
};
static_assert(sizeof(UnitUniform)==128);
kernel void OriginalUnitUniformCopy(device const UnitUniform* uni [[buffer(0)]],
 device UnitUniform* uniCopy [[buffer(1)]],constant uint& numEntries [[buffer(2)]],
 uint index [[thread_position_in_grid]]) {
 if(index>=numEntries)return;
 uniCopy[index].composite=uni[index].composite;
 uniCopy[index].maxHealth=uni[index].maxHealth;
 uniCopy[index].health=uni[index].health;
 uniCopy[index].drawPos=uni[index].drawPos*0.5f;
 uniCopy[index].speed=uni[index].speed;
 uniCopy[index].userDefined[0]=uni[index].userDefined[0];
 uniCopy[index].userDefined[1]=uni[index].userDefined[1];
 uniCopy[index].userDefined[2]=uni[index].userDefined[2];
 uniCopy[index].userDefined[3]=uni[index].userDefined[3];
}
)MSL";
class UnitCopyProgram final:public NativeUnitUniformCopyProgram {
public:
 __strong id<MTLLibrary> library=nil;
 __strong id<MTLComputePipelineState> pipeline=nil;
 std::vector<uint8_t> Dispatch(const NativeCUSPublishedBuffer& source,
  const NativeBufferSnapshot& previous,uint32_t entries,uint64_t frame) const override {
  Require(source.kind==NativeCUSPublishedKind::ModelUniforms&&source.stride==128&&
   source.contextGeneration&&source.publicationSerial&&source.publishedDrawFrame<=frame,
   "Unit uniform compute lacks its actual uploader publication");
  Require(entries&&entries<=source.capacityElements&&previous.layout.kind==NativeBufferKind::Storage&&
   previous.layout.stride==128&&entries<=previous.layout.elements&&
   previous.bytes.size()==CheckedNativeBufferByteSize(previous.layout),
   "Unit uniform compute source/destination extent differs");
  std::vector<uint8_t> input(size_t(entries)*128);
  for(size_t i=0;i<entries;++i)ReadNativeCUSPublishedElement(source,i,std::span(input).subspan(i*128,128));
  RuntimeLease lease;const auto start=Clock::now();
  Require(pipeline&&library&&library.device==lease.Device(),"Unit uniform compute owner/device changed");
  auto upload=[&](const void* bytes,size_t count){
   Require(count&&count<=lease.Device().maxBufferLength,"Unit uniform compute buffer exceeds device capacity");
   auto buffer=[lease.Device() newBufferWithLength:count options:MTLResourceStorageModeShared];
   Require(buffer!=nil,"Cannot allocate unit uniform compute buffer");
   std::memcpy(buffer.contents,bytes,count);lease.AddAllocation(buffer);return buffer;
  };
  auto sourceBuffer=upload(input.data(),input.size());
  auto output=upload(previous.bytes.data(),previous.bytes.size());
  auto count=upload(&entries,sizeof(entries));
  auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=3;
  NSError* error=nil;auto arguments=[lease.Device() newArgumentTableWithDescriptor:descriptor error:&error];
  Require(arguments!=nil,"Cannot allocate unit uniform compute arguments: "+Error(error));
  [arguments setAddress:sourceBuffer.gpuAddress atIndex:0];
  [arguments setAddress:output.gpuAddress atIndex:1];
  [arguments setAddress:count.gpuAddress atIndex:2];
  lease.Retain(arguments);lease.Retain(pipeline);lease.Retain(library);lease.CommitResources();
  auto command=lease.Begin("Original API UnitBufferUniform Copy dispatch");
  auto encoder=[command computeCommandEncoder];Require(encoder!=nil,"Cannot encode unit uniform copy");
  [encoder setComputePipelineState:pipeline];[encoder setArgumentTable:arguments];
  // Match ceil(entries/64) workgroups, including the original bounds check.
  [encoder dispatchThreads:MTLSizeMake(((size_t(entries)+63)/64)*64,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];
  [encoder barrierAfterStages:MTLStageDispatch beforeQueueStages:MTLStageDispatch|MTLStageVertex visibilityOptions:MTL4VisibilityOptionDevice];
  [encoder endEncoding];
  // This optional API has immediate buffer/Download visibility. Use the shared
  // native completion protocol before publishing bytes; no diagnostic readback.
  lease.SubmitAndWait(start,frame);
  std::vector<uint8_t> result(previous.bytes.size());std::memcpy(result.data(),output.contents,result.size());
  return result;
 }
};
}
std::shared_ptr<const NativeUnitUniformCopyProgram> CompileNativeUnitUniformCopy() {
 metal4::RuntimeLease lease;auto owner=std::make_shared<UnitCopyProgram>();
 auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Original API UnitBufferUniform Copy";descriptor.source=metal4::String(UnitCopySource);
 auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;
 NSError* error=nil;owner->library=[lease.Compiler() newLibraryWithDescriptor:descriptor error:&error];
 metal4::Require(owner->library!=nil,"Cannot compile original unit copy MSL: "+metal4::Error(error));
 auto function=[MTL4LibraryFunctionDescriptor new];function.library=owner->library;function.name=@"OriginalUnitUniformCopy";
 auto pipeline=[MTL4ComputePipelineDescriptor new];pipeline.computeFunctionDescriptor=function;
 pipeline.options=[MTL4PipelineOptions new];pipeline.options.shaderValidation=metal4::ShaderValidationMode(false);
 owner->pipeline=[lease.Compiler() newComputePipelineStateWithDescriptor:pipeline compilerTaskOptions:nil error:&error];
 metal4::Require(owner->pipeline!=nil&&owner->pipeline.maxTotalThreadsPerThreadgroup>=64,"Cannot compile original 64-thread unit copy pipeline: "+metal4::Error(error));
 return owner;
}
} // namespace rendering
