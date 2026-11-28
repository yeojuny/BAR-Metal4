// Standalone test only. Uses the frozen production GroundDecalVertex AND
// GroundDecalFragment for both the 36-vertex and 36-index submissions.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>
#include <array>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "NativeGroundDecalShaders.h"
#include "Metal4GroundDecalIndexPolicyG01.h"

using namespace simd;
using namespace rendering::metal4;
static constexpr unsigned W=64,H=64;
static void Check(bool ok,const char* why,NSError* e=nil) {
 if(!ok){fprintf(stderr,"FAIL %s %s\n",why,e?e.localizedDescription.UTF8String:"");std::exit(1);}
}
struct alignas(16) Instance {
 float4 forcedHeight,posT,posB,uvMain,uvNorm,createParams1,createParams2,createParams3,createParams4;
 uint4 createParams5;float4 userDefined[2];
};
struct alignas(16) Uniform {
 float4x4 viewProjection,inverseViewProjection,shadow;
 float4 mapDims,mapDimsPO2,groundAmbient,groundDiffuse,sunDirection,waterMin,waterBase,waterAbsorb,frameScreen,fragmentToLogical;
 uint4 flags;
};
static_assert(sizeof(Instance)==192);
static_assert(sizeof(Uniform)==368);
struct Output {std::vector<uint8_t> color,depth,stencil,initialColor,initialDepth,initialStencil;};
static id<MTLTexture> Tex(id<MTLDevice> d,MTLPixelFormat f,unsigned w,unsigned h,bool target=false) {
 auto x=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f width:w height:h mipmapped:NO];
 x.storageMode=MTLStorageModeShared;x.usage=MTLTextureUsageShaderRead|(target?MTLTextureUsageRenderTarget:MTLTextureUsageUnknown);
 auto t=[d newTextureWithDescriptor:x];Check(t!=nil,"texture allocation");return t;
}
static id<MTLSamplerState> Sampler(id<MTLDevice> d,bool linear) {
 auto s=[MTLSamplerDescriptor new];s.supportArgumentBuffers=YES;
 s.minFilter=linear?MTLSamplerMinMagFilterLinear:MTLSamplerMinMagFilterNearest;
 s.magFilter=MTLSamplerMinMagFilterLinear;s.mipFilter=MTLSamplerMipFilterNotMipmapped;
 s.sAddressMode=s.tAddressMode=MTLSamplerAddressModeClampToEdge;
 auto out=[d newSamplerStateWithDescriptor:s];Check(out!=nil,"sampler");return out;
}
static void Upload(id<MTLTexture> t,const void* p,unsigned stride,unsigned w,unsigned h){
 [t replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0 withBytes:p bytesPerRow:stride];
}
static Output Render(id<MTLDevice> device,id<MTL4CommandQueue> queue,id<MTLRenderPipelineState> pipe,
                    unsigned seed,bool indexed,bool writeDepthStencil,unsigned degenerateMode) {
 @autoreleasepool {
 NSError* error=nil;
 auto color=Tex(device,MTLPixelFormatBGRA8Unorm,W,H,true);
 auto depth=Tex(device,MTLPixelFormatDepth32Float,W,H,true);
 auto stencil=Tex(device,MTLPixelFormatStencil8,W,H,true);
 auto height=Tex(device,MTLPixelFormatR32Float,32,32);
 auto normal=Tex(device,MTLPixelFormatRG32Float,32,32);
 auto minimap=Tex(device,MTLPixelFormatRGBA8Unorm,32,32);
 auto terrain=Tex(device,MTLPixelFormatDepth32Float,W,H);
 auto atlasDesc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:32 height:32 mipmapped:NO];
 atlasDesc.textureType=MTLTextureType2DArray;atlasDesc.arrayLength=1;
 atlasDesc.storageMode=MTLStorageModeShared;atlasDesc.usage=MTLTextureUsageShaderRead;
 auto atlas=[device newTextureWithDescriptor:atlasDesc];Check(atlas!=nil,"atlas");
 Output out;out.initialColor.resize(W*H*4);out.initialDepth.resize(W*H*4);out.initialStencil.resize(W*H);
 std::vector<float> d0(W*H),ground(32*32),norm(32*32*2,0),terrainPixels(W*H,.5f);
 std::vector<uint8_t> mini(32*32*4),atlasPixels(32*32*4);
 for(unsigned i=0;i<W*H;++i){
  out.initialColor[4*i+0]=uint8_t((i+seed*13)%211);out.initialColor[4*i+1]=uint8_t((i/3+seed)%193);
  out.initialColor[4*i+2]=31;out.initialColor[4*i+3]=255;
  d0[i]=.94f;out.initialStencil[i]=uint8_t((i+seed)%3);
 }
 std::memcpy(out.initialDepth.data(),d0.data(),out.initialDepth.size());
 for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x){
  const unsigned i=y*32+x;ground[i]=0;
  mini[4*i+0]=120;mini[4*i+1]=160;mini[4*i+2]=110;mini[4*i+3]=255;
  atlasPixels[4*i+0]=uint8_t(70+(x*5+seed)%150);
  atlasPixels[4*i+1]=uint8_t(30+(y*6+seed)%190);
  atlasPixels[4*i+2]=uint8_t(90+(x+y)%120);
  atlasPixels[4*i+3]=uint8_t((x+y+seed)%7==0?0:220);
 }
 Upload(color,out.initialColor.data(),W*4,W,H);Upload(depth,d0.data(),W*4,W,H);
 Upload(stencil,out.initialStencil.data(),W,W,H);Upload(height,ground.data(),32*4,32,32);
 Upload(normal,norm.data(),32*8,32,32);Upload(minimap,mini.data(),32*4,32,32);
 Upload(terrain,terrainPixels.data(),W*4,W,H);
 [atlas replaceRegion:MTLRegionMake2D(0,0,32,32) mipmapLevel:0 slice:0 withBytes:atlasPixels.data() bytesPerRow:32*4 bytesPerImage:32*32*4];
 std::array<Instance,3> instances{};
 for(unsigned j=0;j<instances.size();++j){
  auto& v=instances[j];float cx=float(int(j)-1)*.36f;float cy=float(int(j%2))*.11f;
  float r=(degenerateMode==2||(degenerateMode==1&&j==2))?0.f:(.34f+float(seed%3)*.02f);
  v.posT=float4{cx-r,-r+cy,cx+r,-r+cy};v.posB=float4{cx+r,r+cy,cx-r,r+cy};
  v.uvMain=v.uvNorm=float4{0,0,1,1};v.createParams1=float4{.9f,.003f,.1f,0};
  v.createParams2=float4{float(j)*.10f,.25f,0,1};v.createParams3=float4{0,30,0,0};
  v.createParams4=float4{0,1,0,1};
  // Different creation endpoints and glow colors make first-provoking flat
  // varyings observable; a reordered triangle can no longer hide behind a
  // uniform constant color.
  v.createParams5=uint4{1u,0xffffffffu,0xff103080u,0xffb09020u};
 }
 Uniform u{};u.viewProjection=matrix_identity_float4x4;
 u.inverseViewProjection=matrix_identity_float4x4;u.shadow=matrix_identity_float4x4;
 u.mapDims=float4{256,256,1.f/256,1.f/256};u.mapDimsPO2=u.mapDims;
 u.groundAmbient=float4{.3f,.3f,.3f,1};u.groundDiffuse=float4{.6f,.6f,.6f,1};
 u.sunDirection=float4{0,1,0,0};u.frameScreen=float4{80,1.f/W,1.f/H,0};
 u.fragmentToLogical=float4{1,1,0,0};u.flags=uint4{0,0,0,0};
 auto instBuffer=[device newBufferWithBytes:instances.data() length:sizeof(instances) options:MTLResourceStorageModeShared];
 auto uniformBuffer=[device newBufferWithBytes:&u length:sizeof(u) options:MTLResourceStorageModeShared];
 auto indexBuffer=[device newBufferWithBytes:GroundDecalIndicesG01.data() length:GroundDecalIndexBytesG01 options:MTLResourceStorageModeShared];
 Check(instBuffer&&uniformBuffer&&indexBuffer,"buffers");
 auto s0=Sampler(device,false),s1=Sampler(device,false),s2=Sampler(device,true),s5=Sampler(device,true);
 auto ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=2;ad.maxTextureBindCount=7;
 ad.maxSamplerStateBindCount=6;ad.initializeBindings=YES;
 auto args=[device newArgumentTableWithDescriptor:ad error:&error];Check(args!=nil,"arguments",error);
 [args setAddress:instBuffer.gpuAddress atIndex:0];[args setAddress:uniformBuffer.gpuAddress atIndex:1];
 [args setTexture:height.gpuResourceID atIndex:0];[args setTexture:normal.gpuResourceID atIndex:1];
 [args setTexture:minimap.gpuResourceID atIndex:2];[args setTexture:terrain.gpuResourceID atIndex:5];
 [args setTexture:atlas.gpuResourceID atIndex:6];
 [args setSamplerState:s0.gpuResourceID atIndex:0];[args setSamplerState:s1.gpuResourceID atIndex:1];
 [args setSamplerState:s2.gpuResourceID atIndex:2];[args setSamplerState:s5.gpuResourceID atIndex:5];
 auto ds=[MTLDepthStencilDescriptor new];ds.depthCompareFunction=MTLCompareFunctionAlways;
 ds.depthWriteEnabled=writeDepthStencil?YES:NO;
 if(writeDepthStencil){auto f=[MTLStencilDescriptor new];f.stencilCompareFunction=MTLCompareFunctionAlways;
  f.depthStencilPassOperation=MTLStencilOperationReplace;f.readMask=f.writeMask=255;
  ds.frontFaceStencil=ds.backFaceStencil=f;}
 auto state=[device newDepthStencilStateWithDescriptor:ds];Check(state!=nil,"depth state");
 auto allocator=[device newCommandAllocator];auto command=[device newCommandBuffer];
 auto rd=[MTLResidencySetDescriptor new];rd.initialCapacity=24;
 auto residency=[device newResidencySetWithDescriptor:rd error:&error];Check(residency!=nil,"residency",error);
 for(auto t:{color,depth,stencil,height,normal,minimap,terrain,atlas})[residency addAllocation:t];
 for(auto b:{instBuffer,uniformBuffer,indexBuffer})[residency addAllocation:b];
 [residency commit];[command beginCommandBufferWithAllocator:allocator];
 auto pass=[MTL4RenderPassDescriptor new];pass.renderTargetWidth=W;pass.renderTargetHeight=H;
 pass.defaultRasterSampleCount=1;
 pass.colorAttachments[0].texture=color;pass.colorAttachments[0].loadAction=MTLLoadActionLoad;
 pass.colorAttachments[0].storeAction=MTLStoreActionStore;
 pass.depthAttachment.texture=depth;pass.depthAttachment.loadAction=MTLLoadActionLoad;
 pass.depthAttachment.storeAction=MTLStoreActionStore;
 pass.stencilAttachment.texture=stencil;pass.stencilAttachment.loadAction=MTLLoadActionLoad;
 pass.stencilAttachment.storeAction=MTLStoreActionStore;
 auto e=[command renderCommandEncoderWithDescriptor:pass];Check(e!=nil,"render encoder");
 [e setViewport:MTLViewport{0,0,W,H,0,1}];[e setScissorRect:MTLScissorRect{seed%5,seed%4,W-seed%5,H-seed%4}];
 [e setRenderPipelineState:pipe];[e setDepthStencilState:state];[e setFrontFacingWinding:MTLWindingCounterClockwise];
 [e setCullMode:MTLCullModeBack];[e setDepthClipMode:MTLDepthClipModeClip];
 [e setTriangleFillMode:MTLTriangleFillModeFill];[e setDepthBias:0 slopeScale:0 clamp:0];
 [e setStencilFrontReferenceValue:7 backReferenceValue:7];
 [e setArgumentTable:args atStages:MTLRenderStageVertex|MTLRenderStageFragment];
 if(indexed)[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:36 indexType:MTLIndexTypeUInt16
  indexBuffer:indexBuffer.gpuAddress indexBufferLength:indexBuffer.length instanceCount:instances.size() baseVertex:0 baseInstance:0];
 else [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:36 instanceCount:instances.size()];
 [e endEncoding];[command useResidencySet:residency];[command endCommandBuffer];
 auto event=[device newSharedEvent];auto options=[MTL4CommitOptions new];auto done=dispatch_semaphore_create(0);
 __block NSError* gpuError=nil;
 [options addFeedbackHandler:^(id<MTL4CommitFeedback> r){gpuError=r.error;dispatch_semaphore_signal(done);}];
 id<MTL4CommandBuffer> batch[]={command};[queue commit:batch count:1 options:options];[queue signalEvent:event value:1];
 Check([event waitUntilSignaledValue:1 timeoutMS:30000],"GPU event");
 Check(dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,30*NSEC_PER_SEC))==0,"GPU feedback");
 Check(gpuError==nil,"GPU error",gpuError);
 out.color.resize(W*H*4);out.depth.resize(W*H*4);out.stencil.resize(W*H);
 [color getBytes:out.color.data() bytesPerRow:W*4 fromRegion:MTLRegionMake2D(0,0,W,H) mipmapLevel:0];
 [depth getBytes:out.depth.data() bytesPerRow:W*4 fromRegion:MTLRegionMake2D(0,0,W,H) mipmapLevel:0];
 [stencil getBytes:out.stencil.data() bytesPerRow:W fromRegion:MTLRegionMake2D(0,0,W,H) mipmapLevel:0];
 return out;
 }
}
int main(){@autoreleasepool{
 auto device=MTLCreateSystemDefaultDevice();Check(device!=nil,"Metal device");
 auto queue=[device newMTL4CommandQueue];Check(queue!=nil,"Metal4 queue");
 NSError* error=nil;auto compiler=[device newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&error];
 Check(compiler!=nil,"compiler",error);
 auto ld=[MTL4LibraryDescriptor new];ld.source=[NSString stringWithUTF8String:NativeGroundDecalShaderSource(false,false).c_str()];
 auto co=[MTLCompileOptions new];co.mathMode=MTLMathModeSafe;
 co.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;ld.options=co;
 auto library=[compiler newLibraryWithDescriptor:ld error:&error];Check(library!=nil,"actual shader library",error);
 auto pd=[MTL4RenderPipelineDescriptor new];auto vf=[MTL4LibraryFunctionDescriptor new],ff=[MTL4LibraryFunctionDescriptor new];
 vf.library=ff.library=library;vf.name=@"GroundDecalVertex";ff.name=@"GroundDecalFragment";
 pd.vertexFunctionDescriptor=vf;pd.fragmentFunctionDescriptor=ff;pd.rasterSampleCount=1;
 pd.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
 auto a=pd.colorAttachments[0];a.pixelFormat=MTLPixelFormatBGRA8Unorm;a.blendingState=MTL4BlendStateEnabled;
 a.rgbBlendOperation=a.alphaBlendOperation=MTLBlendOperationAdd;
 a.sourceRGBBlendFactor=a.sourceAlphaBlendFactor=MTLBlendFactorSourceAlpha;
 a.destinationRGBBlendFactor=a.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
 auto po=[MTL4PipelineOptions new];po.shaderValidation=MTLShaderValidationEnabled;pd.options=po;
 auto pipe=[compiler newRenderPipelineStateWithDescriptor:pd compilerTaskOptions:nil error:&error];
 Check(pipe!=nil,"actual shader pipeline",error);
 unsigned visible=0,depthChanged=0,stencilChanged=0,degenerateCases=0;
 for(unsigned seed=0;seed<16;++seed)for(unsigned mode=0;mode<2;++mode){
  const bool extraWrites=mode==1;
  const unsigned degenerateMode=seed==15?2:((seed%4)==0?1:0);
  auto old=Render(device,queue,pipe,seed,false,extraWrites,degenerateMode);
  auto next=Render(device,queue,pipe,seed,true,extraWrites,degenerateMode);
  Check(old.color==next.color,"color byte parity");
  Check(old.depth==next.depth,"depth byte parity");
  Check(old.stencil==next.stencil,"stencil byte parity");
  if(old.color!=old.initialColor)++visible;
  if(old.depth!=old.initialDepth)++depthChanged;
  if(old.stencil!=old.initialStencil)++stencilChanged;
  if(degenerateMode)++degenerateCases;
  if(degenerateMode!=2)Check(old.color!=old.initialColor,"nonclear color same case");
  if(extraWrites&&degenerateMode!=2){Check(old.depth!=old.initialDepth,"nonclear depth same case");
   Check(old.stencil!=old.initialStencil,"nonclear stencil same case");}
 }
 Check(visible>=30&&depthChanged>=15&&stencilChanged>=15&&degenerateCases==10,"case witness totals");
 printf("PASS cases=32 actual_GroundDecalVertex_Fragment=1 old36_indexed36=1 multiinstance=3 visible=%u depthChanged=%u stencilChanged=%u degenerateCase=%u color_depth_stencil_byte_equal=32\n",visible,depthChanged,stencilChanged,degenerateCases);
 return 0;
}}
