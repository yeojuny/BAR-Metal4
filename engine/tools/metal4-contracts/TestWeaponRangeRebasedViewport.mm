#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include <simd/simd.h>
#include <vector>
#include <array>
#include <algorithm>
#include <atomic>
#include <memory>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cstdio>
#include "NativeWeaponRangeShaders.h"
using namespace rendering::metal4;
static NSString* String(const std::string& s){return [[NSString alloc] initWithBytes:s.data() length:s.size() encoding:NSUTF8StringEncoding];}
#include "WeaponRangeViewportCandidateDescriptor.inc"
#include "WeaponRangeViewportOriginalDescriptor.inc"
static void req(bool x,NSString* what) { if(!x){fprintf(stderr,"FAIL %s\n",what.UTF8String);exit(1);} }
struct alignas(16) Uniform {
 simd_float4x4 view,projection,inverse,mini;
 simd_float4 mapSize,fogColor,fogParams,mouse,values0,values1,pip,raster;
 simd_uint4 options;simd_float4 viewport;simd_uint4 renderOptions;simd_float4 outputViewport;
 simd_float4 maskParameters,fillColors[4],viewGeometry,sourceViewport;simd_uint4 programOptions;
};
static_assert(sizeof(Uniform)==576);
struct alignas(16) Instance {simd_float4 pos,col,visibility,projectile,additional;simd_uint4 ids;};
static_assert(sizeof(Instance)==96);
struct Result {__strong id<MTLLibrary> library=nil;__strong NSError* error=nil;std::atomic<bool> ready{false};};
struct Case {int program;int samples;int mode;bool perSample;bool blend;bool maskedFlip;bool rebased=false;};
const char* fragments[]={"OriginalWeaponRangeFillFragment","OriginalWeaponRangeLineFragment","OriginalWeaponRangeMaskFragment","OriginalWeaponRangeMaskedLineFragment","OriginalWeaponRangeMaskedFillFragment","OriginalWeaponRangeCompositeFragment"};
static id<MTLRenderPipelineState> pipeline(id<MTL4Compiler> c,id<MTLLibrary> l,const Case& t) {
 NSError* e=nil; auto d=[MTL4RenderPipelineDescriptor new]; auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=l;
 v.name=t.program==5?(t.rebased?@"OriginalWeaponRangeCompositeRebased":@"OriginalWeaponRangeComposite"):(t.program==1||t.program==3)?@"OriginalWeaponRangeLine":(t.rebased?@"OriginalWeaponRangeFillRebased":@"OriginalWeaponRangeFill");
 f.name=String(std::string(fragments[t.program])+((t.samples>1&&(t.program==1||t.program==3))?"MSAA":""));
 d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=t.samples;d.alphaToCoverageState=MTL4AlphaToCoverageStateDisabled;
 auto a=d.colorAttachments[0];a.pixelFormat=t.program==2?MTLPixelFormatR8Unorm:MTLPixelFormatRGBA8Unorm;a.blendingState=t.blend?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=MTLBlendFactorSourceAlpha;a.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;a.sourceAlphaBlendFactor=MTLBlendFactorOne;a.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;a.rgbBlendOperation=MTLBlendOperationAdd;a.alphaBlendOperation=MTLBlendOperationAdd;a.writeMask=MTLColorWriteMaskAll;
 auto o=[MTL4PipelineOptions new];o.shaderValidation=MTLShaderValidationDisabled;d.options=o;
 auto p=[c newRenderPipelineStateWithDescriptor:d compilerTaskOptions:nil error:&e];req(p!=nil,e.description?:@"Pipeline failed");return p;
}
static id<MTLBuffer> buffer(id<MTLDevice>d,const void* p,size_t n){auto b=[d newBufferWithBytes:p length:n options:MTLResourceStorageModeShared];req(b!=nil,@"buffer");return b;}
static id<MTLTexture> texture(id<MTLDevice> d,MTLPixelFormat fmt,int w,int h,int samples,MTLTextureUsage usage,MTLStorageMode storage){
 auto t=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt width:w height:h mipmapped:NO];t.usage=usage;t.storageMode=storage;if(samples>1){t.textureType=MTLTextureType2DMultisample;t.sampleCount=samples;} auto x=[d newTextureWithDescriptor:t];req(x!=nil,@"texture");return x;
}
struct Output {std::vector<uint8_t> color,depth;};
static Output render(id<MTLDevice> d,id<MTL4CommandQueue> queue,id<MTLRenderPipelineState> ps,const Case& t,std::array<float,4> view={0,0,257,239},bool forceFullReference=false) {
 constexpr int W=257,H=239,N=96;const bool line=t.program==1||t.program==3, comp=t.program==5;
 Uniform u{};u.view=matrix_identity_float4x4;u.inverse=matrix_identity_float4x4;u.mini=matrix_identity_float4x4;
 u.projection=(simd_float4x4){(simd_float4){.5,0,0,0},(simd_float4){0,0,0,0},(simd_float4){0,.5,0,0},(simd_float4){-256,-256,128,256}};
 u.mapSize={1024,1024,0,0};u.fogColor={.12,.22,.32,1};u.fogParams={0,10000,0,.0002};u.mouse={600,0,600,1};u.values0={1,t.mode==2?1.0f:0.0f,0,t.mode==1?1.0f:0.0f};u.values1={3,1,0,t.mode==3?0.0f:1.0f};u.pip={.05,.95,.05,.95};u.raster={.4f, t.mode==3?3.5f:1.75f,.01f,.15f};
 u.options={unsigned(t.mode==1?1:0),0,0,N};u.viewport={view[0],view[1],view[2],view[3]};u.outputViewport={0,0,W,H};u.renderOptions={unsigned(line),0,0,0};u.maskParameters={3,.9f,1,7.0f/255};u.fillColors[0]={.8,.15,.4,.5};u.fillColors[1]={.1,.8,.2,.6};u.fillColors[2]={.5,.2,.9,.2};u.fillColors[3]={.2,.6,.8,.3};u.viewGeometry={W,H,0,0};u.sourceViewport={0,0,W,H};u.programOptions={unsigned(t.program==2),8,unsigned(comp),unsigned(t.maskedFlip)};
 std::vector<simd_float4> verts;
 if(comp)verts={(simd_float4){-1,-1,0,0},(simd_float4){-1,1,0,0},(simd_float4){1,-1,0,0},(simd_float4){1,-1,0,0},(simd_float4){-1,1,0,0},(simd_float4){1,1,0,0}};
 else if(line){for(int i=0;i<N;i++){float a=float(i)*2*3.1415926535f/N;verts.push_back((simd_float4){cosf(a),sinf(a),float(i)/N,1});}}
 else {verts.push_back((simd_float4){0,0,.5,1});for(int i=0;i<=N;i++){float a=float(i)*2*3.1415926535f/N;verts.push_back((simd_float4){cosf(a),sinf(a),float(i)/N,1});}}
 constexpr int instanceCount=3;Instance instances[instanceCount]={};
 simd_float4 positions[instanceCount];for(int i=0;i<instanceCount;i++){auto &v=instances[i];v.pos={290.f+210*i,20.f,320.f+130*i,150.f+20*i};v.col={.3f+.2f*i,.8f-.2f*i,.4f,.75f};v.visibility={0,10000,1,.3};v.projectile={140,1.f,1.f,1.f};v.additional={1,0,0,0};positions[i]={v.pos.x,32,v.pos.z,.3f*i};}
 auto vb=buffer(d,verts.data(),verts.size()*sizeof(verts[0])),ib=buffer(d,instances,sizeof(instances)),pb=buffer(d,positions,sizeof(positions)),ub=buffer(d,&u,sizeof(u));
 auto height=texture(d,MTLPixelFormatR32Float,64,64,1,MTLTextureUsageShaderRead,MTLStorageModeShared);
 std::vector<float> heights(64*64);for(int y=0;y<64;y++)for(int x=0;x<64;x++)heights[y*64+x]=31.f+10*cosf(float(x)*.27f)*sinf(float(y)*.19f);
 [height replaceRegion:MTLRegionMake2D(0,0,64,64) mipmapLevel:0 withBytes:heights.data() bytesPerRow:64*sizeof(float)];
 auto mask=texture(d,MTLPixelFormatR8Unorm,W,H,1,MTLTextureUsageShaderRead,MTLStorageModeShared);
 std::vector<uint8_t> masks(W*H);for(int y=0;y<H;y++)for(int x=0;x<W;x++)masks[y*W+x]=uint8_t(((x/19)^(y/13))&15);
 [mask replaceRegion:MTLRegionMake2D(0,0,W,H) mipmapLevel:0 withBytes:masks.data() bytesPerRow:W];
 auto sd=[MTLSamplerDescriptor new];sd.minFilter=sd.magFilter=MTLSamplerMinMagFilterLinear;sd.supportArgumentBuffers=YES;auto sampler=[d newSamplerStateWithDescriptor:sd];req(sampler!=nil,@"sampler");
 auto ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=4;ad.maxTextureBindCount=2;ad.maxSamplerStateBindCount=1;ad.initializeBindings=YES;NSError* e=nil;
 auto args=[d newArgumentTableWithDescriptor:ad error:&e];req(args!=nil,e.description?:@"argument table");[args setAddress:vb.gpuAddress atIndex:0];[args setAddress:ib.gpuAddress atIndex:1];[args setAddress:pb.gpuAddress atIndex:2];[args setAddress:ub.gpuAddress atIndex:3];[args setTexture:height.gpuResourceID atIndex:0];[args setTexture:mask.gpuResourceID atIndex:1];[args setSamplerState:sampler.gpuResourceID atIndex:0];
 auto format=t.program==2?MTLPixelFormatR8Unorm:MTLPixelFormatRGBA8Unorm;
 auto result=texture(d,format,W,H,1,MTLTextureUsageRenderTarget,MTLStorageModeShared);
 auto target=t.samples==1?result:texture(d,format,W,H,t.samples,MTLTextureUsageRenderTarget,MTLStorageModePrivate);
 auto depth=texture(d,t.program==2?MTLPixelFormatDepth16Unorm:MTLPixelFormatDepth32Float,W,H,t.samples,MTLTextureUsageRenderTarget,MTLStorageModePrivate);
 auto dd=[MTLDepthStencilDescriptor new];dd.depthCompareFunction=MTLCompareFunctionLess;dd.depthWriteEnabled=YES;auto ds=[d newDepthStencilStateWithDescriptor:dd];
 auto rd=[MTLResidencySetDescriptor new];rd.initialCapacity=12;auto res=[d newResidencySetWithDescriptor:rd error:&e];req(res!=nil,e.description?:@"residency");
 for(id<MTLAllocation> r in @[vb,ib,pb,ub,height,mask,result,depth,target])[res addAllocation:r];[res commit];
 auto alloc=[d newCommandAllocator];auto cmd=[d newCommandBuffer];req(alloc&&cmd,@"command");[cmd beginCommandBufferWithAllocator:alloc];[cmd useResidencySet:res];
 auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=t.samples==1?MTLStoreActionStore:MTLStoreActionMultisampleResolve;pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,0);
 if(t.samples>1)pass.colorAttachments[0].resolveTexture=result;
 pass.depthAttachment.texture=depth;pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.storeAction=t.samples==1?MTLStoreActionStore:MTLStoreActionDontCare;pass.depthAttachment.clearDepth=1;
 auto enc=[cmd renderCommandEncoderWithDescriptor:pass];req(enc!=nil,@"render encoder");[enc setRenderPipelineState:ps];[enc setDepthStencilState:ds];[enc setViewport:(line||t.rebased||forceFullReference)?MTLViewport{0,0,W,H,0,1}:MTLViewport{view[0],view[1],view[2],view[3],0,1}];[enc setScissorRect:MTLScissorRect{6,4,W-12,H-8}];[enc setCullMode:MTLCullModeNone];[enc setArgumentTable:args atStages:MTLRenderStageVertex|MTLRenderStageFragment];
 [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:comp?6:line?N*6:(verts.size()-2)*3 instanceCount:comp?1:instanceCount];[enc endEncoding];[cmd endCommandBuffer];
 id<MTL4CommandBuffer> cmds[]={cmd};auto event=[d newSharedEvent];[queue commit:cmds count:1];[queue signalEvent:event value:1];req([event waitUntilSignaledValue:1 timeoutMS:10000],@"GPU completion timeout");
 int bpp=t.program==2?1:4;std::vector<uint8_t> out(W*H*bpp);[result getBytes:out.data() bytesPerRow:W*bpp fromRegion:MTLRegionMake2D(0,0,W,H) mipmapLevel:0];
 Output output{std::move(out),{}};
 if(t.samples==1){const int dp=t.program==2?2:4;const size_t pitch=(W*dp+255)&~size_t(255);auto bytes=[d newBufferWithLength:pitch*H options:MTLResourceStorageModeShared];auto readQueue=[d newCommandQueue];auto cb=[readQueue commandBuffer];auto blit=[cb blitCommandEncoder];[blit copyFromTexture:depth sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(W,H,1) toBuffer:bytes destinationOffset:0 destinationBytesPerRow:pitch destinationBytesPerImage:pitch*H];[blit endEncoding];[cb commit];[cb waitUntilCompleted];req(cb.status==MTLCommandBufferStatusCompleted,cb.error.description?:@"depth copy");output.depth.resize(W*H*dp);for(int y=0;y<H;y++)memcpy(output.depth.data()+y*W*dp,(uint8_t*)bytes.contents+y*pitch,W*dp);}
 return output;
}
static id<MTLLibrary> asyncCompile(id<MTL4Compiler> c,MTL4LibraryDescriptor* input) {
 auto r=std::make_shared<Result>();auto task=[c newLibraryWithDescriptor:input completionHandler:^(id<MTLLibrary> l,NSError* e){@autoreleasepool{r->library=l;r->error=e;r->ready.store(true,std::memory_order_release);}}];
 req(task!=nil,@"async task");for(int i=0;i<1000&&!r->ready.load(std::memory_order_acquire);i++)[NSThread sleepForTimeInterval:.02];req(r->ready.load(std::memory_order_acquire)&&r->library!=nil,r->error.description?:@"async compilation failed");return r->library;
}
int main(){@autoreleasepool{
 auto dev=MTLCreateSystemDefaultDevice();req(dev&&[dev supportsFamily:MTLGPUFamilyMetal4],@"Metal4");
 NSError* e=nil;auto compiler=[dev newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&e];req(compiler,e.description?:@"compiler");auto q=[dev newMTL4CommandQueue];
 auto descriptor=RangeLibraryDescriptor();auto lib=[compiler newLibraryWithDescriptor:descriptor error:&e];req(lib,e.description?:@"range library");
 uint64_t mismatches=0,depthMismatch=0,visible=0;int cases=0;
 for(int n:{1,2,4})for(int prog:{0,4,5})for(int mode:{0,1}){@autoreleasepool{
  Case a{prog,n,mode,false,false,false,false},b=a;b.rebased=true;
  const std::array<float,4> view={10.0f,8.0f,220.0f,200.0f};
  auto x=render(dev,q,pipeline(compiler,lib,a),a,view),y=render(dev,q,pipeline(compiler,lib,b),b,view);
  req(x.color.size()==y.color.size()&&x.depth.size()==y.depth.size(),@"size");
  uint64_t c=0,d=0,nz=0;for(size_t i=0;i<x.color.size();i+=4){nz+=x.color[i]!=0||x.color[i+1]!=0||x.color[i+2]!=0||x.color[i+3]!=0;c+=memcmp(x.color.data()+i,y.color.data()+i,4)!=0;}for(size_t i=0;i<x.depth.size();i+=4)d+=memcmp(x.depth.data()+i,y.depth.data()+i,4)!=0;
  printf("samples=%d program=%d mode=%d differing_pixels=%llu differing_depth=%llu nonzero=%llu\n",n,prog,mode,c,d,nz);mismatches+=c;depthMismatch+=d;visible+=nz;++cases;
 }}
 for(int n:{1,2,4}){@autoreleasepool{
  Case a{5,n,0,false,false,false,false},b=a;b.rebased=true;
  // Original viewport contains the full physical target; composite covers it.
  std::array<float,4> view={-11100,-1600,18200,22500};
  auto x=render(dev,q,pipeline(compiler,lib,a),a,view,true),y=render(dev,q,pipeline(compiler,lib,b),b,view);
  uint64_t c=0,nz=0;for(size_t i=0;i<x.color.size();i+=4){nz+=x.color[i]!=0||x.color[i+1]!=0||x.color[i+2]!=0||x.color[i+3]!=0;c+=memcmp(x.color.data()+i,y.color.data()+i,4)!=0;}printf("large samples=%d composite differing_pixels=%llu nonzero=%llu\n",n,c,nz);mismatches+=c;visible+=nz;++cases;
 }}
 printf("REBASE GPU cases=%d mismatches=%llu depth_difference=%llu nonzero=%llu\n",cases,mismatches,depthMismatch,visible);
 return mismatches?2:0;
}}
