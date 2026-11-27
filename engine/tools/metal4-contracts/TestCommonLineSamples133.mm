#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "Rendering/Metal4/Metal4SourceLines133.h"
#include <iostream>
#include <map>
#include <cmath>
using namespace rendering;using namespace rendering::metal4;
constexpr unsigned W=128,H=128;
constexpr float x0=31.5f,x1=95.5f,yTop=63.75f,lineWidth=1.20f;
std::shared_ptr<NativeSourceLineInput133> Source(bool stipple,const std::array<double,4>& viewport){
 auto s=std::make_shared<NativeSourceLineInput133>();
 const float a=2*(x0-viewport[0])/viewport[2]-1,b=2*(x1-viewport[0])/viewport[2]-1,y=2*(viewport[1]+viewport[3]-yTop)/viewport[3]-1;
 s->vertices={{{a,y,0,1},{1,1,1,1},{0,0,0,0}},{{b,y,0,1},{1,1,1,1},{0,0,0,0}}};
 s->transforms.push_back({sourceLine133::Identity(),sourceLine133::Identity(),sourceLine133::Identity(),{},{}});
 sourceLine133::AddSegment(*s,0,1,0,true,lineWidth,stipple,0x5555,1);return s;
}
int main(int argc,char** argv){@autoreleasepool{try{
 const bool shade=argc>1;
 auto device=MTLCreateSystemDefaultDevice();NSError* error=nil;auto compiler=[device newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&error];Require(compiler,Error(error));auto queue=[device newMTL4CommandQueue];auto oldQueue=[device newCommandQueue];auto allocator=[device newCommandAllocator];auto command=[device newCommandBuffer];auto event=[device newSharedEvent];uint64_t serial=0;
 auto code=@R"MSL(#include <metal_stdlib>
 using namespace metal;
 kernel void readMS(texture2d_ms<float,access::read> t [[texture(0)]],device float4* out [[buffer(0)]],uint2 p [[thread_position_in_grid]]) {if(p.x>=t.get_width()||p.y>=t.get_height())return;uint n=t.get_num_samples();for(uint s=0;s<n;++s)out[(p.y*t.get_width()+p.x)*n+s]=t.read(p,s);}
 kernel void readSingle(texture2d<float,access::read> t [[texture(0)]],device float4* out [[buffer(0)]],uint2 p [[thread_position_in_grid]]) {if(p.x>=t.get_width()||p.y>=t.get_height())return;out[p.y*t.get_width()+p.x]=t.read(p);}
 )MSL";
 auto lib=[device newLibraryWithSource:code options:nil error:&error];Require(lib,Error(error));
 auto readMS=[device newComputePipelineStateWithFunction:[lib newFunctionWithName:@"readMS"] error:&error];auto single=[device newComputePipelineStateWithFunction:[lib newFunctionWithName:@"readSingle"] error:&error];Require(readMS&&single,Error(error));
 SourceLinesGPU133 gpu;std::map<unsigned,id<MTLRenderPipelineState>> psos;unsigned checks=0,covered=0,masked=0,compared=0;
 std::map<std::pair<unsigned,bool>,std::vector<float>> base;
 const std::array<std::array<double,4>,3> viewports={{{0,0,W,H},{-32768,-32768,65536,65536},{-65536,-16384,131072,65536}}};
 Require(!ClassifyCommonLineViewport133({0,0,-1,H},H).valid,"negative rectangle should reject");
 Require(!ClassifyCommonLineViewport133({524288,0,1,H},H).valid,"corner overflow should reject");
 Require(!ClassifyCommonLineViewport133({0,NAN,W,H},H).valid,"nonfinite should reject");
 auto imageDesc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];imageDesc.storageMode=MTLStorageModeShared;imageDesc.usage=MTLTextureUsageShaderRead;auto image=[device newTextureWithDescriptor:imageDesc];uint32_t white=~0u;[image replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:&white bytesPerRow:4];auto sampler=[device newSamplerStateWithDescriptor:[MTLSamplerDescriptor new]];
 const std::vector<unsigned> order=shade?std::vector<unsigned>{2,4,2}:std::vector<unsigned>{1,2,4,2,1};
 for(auto viewport:viewports)for(auto n:order)for(bool stipple:{false,true}){@autoreleasepool{
  Require([device supportsTextureSampleCount:n],"Required sample count unavailable");
  std::vector<MTLSamplePosition> positions(n);[device getDefaultSamplePositions:positions.data() count:n];
  RuntimeLease lease;lease.device=device;lease.compiler=compiler;lease.shade=shade;gpu.BeginBatch(lease);
  ScreenDrawState s;s.material=ScreenMaterial::Solid;s.blend.enabled=false;double ms=0;
  auto data=gpu.Prepare(lease,Source(stipple,viewport),viewport,W,H,n,MTLPixelFormatRGBA8Unorm,MTLPixelFormatInvalid,MTLPixelFormatInvalid,s,image,sampler,image,sampler,true,ms);
  auto pso=gpu.Pipeline(lease,s.blend,{true,true,true,true},MTLPixelFormatRGBA8Unorm,n,false,0,true,ms);
  if(psos.count(n))Require(psos[n]==pso,"Cache did not reuse matching exact count");else{for(auto [k,p]:psos)Require(p!=pso,"Cache aliased different sample counts");psos[n]=pso;}
  auto desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:W height:H mipmapped:NO];desc.storageMode=MTLStorageModePrivate;desc.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;if(n>1){desc.textureType=MTLTextureType2DMultisample;desc.sampleCount=n;}auto target=[device newTextureWithDescriptor:desc];Require(target&&target.sampleCount==n,"Actual texture sample count differs");
  auto rd=[MTLResidencySetDescriptor new];rd.initialCapacity=16;auto resident=[device newResidencySetWithDescriptor:rd error:&error];Require(resident,Error(error));for(id<MTLAllocation> a in lease.allocations)[resident addAllocation:a];[resident addAllocation:image];[resident addAllocation:target];[resident commit];[queue addResidencySet:resident];
  [allocator reset];[command beginCommandBufferWithAllocator:allocator];data->EncodeDependencies(command);data->RequireReady();
  auto rp=[MTL4RenderPassDescriptor new];rp.colorAttachments[0].texture=target;rp.colorAttachments[0].loadAction=MTLLoadActionClear;rp.colorAttachments[0].storeAction=MTLStoreActionStore;rp.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,0);
  auto e=[command renderCommandEncoderWithDescriptor:rp];[e setRenderPipelineState:pso];[e setViewport:MTLViewport{0,0,W,H,0,1}];[e setCullMode:MTLCullModeNone];[e setDepthClipMode:MTLDepthClipModeClamp];[e setArgumentTable:data->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];[e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:1];[e endEncoding];[command endCommandBuffer];id<MTL4CommandBuffer> commands[]{command};auto done=dispatch_semaphore_create(0);__block NSError* fail=nil;auto options=[MTL4CommitOptions new];[options addFeedbackHandler:^(id<MTL4CommitFeedback> f){fail=f.error;dispatch_semaphore_signal(done);}];[queue commit:commands count:1 options:options];[queue signalEvent:event value:++serial];Require([event waitUntilSignaledValue:serial timeoutMS:30000],"Event timeout");Require(dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,30*NSEC_PER_SEC))==0,"Feedback timeout");Require(!fail,Error(fail));[queue removeResidencySet:resident];
  auto out=[device newBufferWithLength:W*H*n*16 options:MTLResourceStorageModeShared];auto read=[oldQueue commandBuffer];auto ce=[read computeCommandEncoder];[ce setComputePipelineState:n>1?readMS:single];[ce setTexture:target atIndex:0];[ce setBuffer:out offset:0 atIndex:0];[ce dispatchThreads:MTLSizeMake(W,H,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];[ce endEncoding];[read commit];[read waitUntilCompleted];Require(read.status==MTLCommandBufferStatusCompleted,Error(read.error));
  auto rgba=static_cast<const float*>(out.contents);
  auto k=std::pair{n,stipple};if(viewport==viewports[0])base[k]=std::vector<float>(rgba,rgba+W*H*n*4);else{
   Require(base.count(k)&&std::equal(base[k].begin(),base[k].end(),rgba),"Extended virtual viewport changed physical sample coverage/color");
   compared+=W*H*n;
  }
  unsigned seen=0,caseChecks=0,caseCovered=0;
  // Interior columns deliberately avoid segment end/top-left edge tie rules.
  for(unsigned y=61;y<67;++y)for(unsigned x=35;x<91;++x)for(unsigned k=0;k<n;++k){
   const auto actual=rgba+4*((y*W+x)*n+k);seen+=actual[0]>.5f;
   if(n==1)continue;const auto pt=positions[k];const float sx=x+pt.x,sy=y+pt.y;
   bool inside=std::abs(sy-yTop)<lineWidth*.5f;
   bool bit=!stipple||((0x5555u>>(unsigned(std::floor(sx-x0))&15))&1u);
   bool expected=inside&&bit;caseCovered+=expected;masked+=inside&&!bit;++caseChecks;
   for(unsigned c=0;c<4;++c)if(std::abs(actual[c]-(expected?1.f:0.f))>1e-6f){std::cerr<<"n="<<n<<" stipple="<<stipple<<" x/y="<<x<<"/"<<y<<" sample="<<k<<" pos="<<pt.x<<","<<pt.y<<" actual="<<actual[c]<<" expected="<<expected<<"\n";throw std::runtime_error("Actual per-sample coverage mismatch");}
  }
  Require(seen>0,"Line did not render");checks+=caseChecks;covered+=caseCovered;
  std::cout<<"samples="<<n<<" stipple="<<stipple<<" positions=";for(auto p:positions)std::cout<<p.x<<","<<p.y<<";";std::cout<<" checked="<<caseChecks<<" covered="<<caseCovered<<" nonzero="<<seen<<"\n";
 }}
 Require(psos.size()==(shade?2u:3u)&&covered>0&&masked>0,"Incomplete coverage/caching proof");
 std::cout<<"EXTENDED GPU matched "<<compared<<" actual pixels/samples across 65536- and 131072-wide virtual viewports\n";
 std::cout<<"PASS exact fixed common-line helper: sample counts and return cache sequence, shading="<<shade<<", "<<checks<<" actual per-sample tests, covered "<<covered<<" stipple masks "<<masked<<", zero mismatch\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}}
