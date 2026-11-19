// Standalone, test-only Metal parity check. Do not link into the engine.
// It compiles the candidate's exact NativeSensorLOSShaderSource and uses its
// R03 fragment and retained R02 coverage reference; every Render compares both.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
// Compile with -I <fixed R03 engine>/rts/Rendering/Metal4. No fallback header.
#include "NativeSensorLOSShaders.h"

using namespace simd;
static constexpr unsigned W=128,H=128;
struct alignas(16) Uniform {
  float camera[48];
  float4 timeInfo,viewGeometry,mapSize,rangeColor,viewport,fragmentToLogical,values,parameters;
  uint4 options,counts;
  float4 distanceFade;
};
static_assert(sizeof(Uniform)==368);
struct alignas(16) Line {
  float4 a,b,color,dataA,dataB;
  float2 widths;
  uint32_t mode;
  uint32_t pad;
};
static_assert(sizeof(Line)==96);
static const char* TestVS=R"MSL(
vertex SensorLOSVarying ParityVS(uint vid [[vertex_id]],constant SensorLOSUniform& u [[buffer(3)]],
                                  constant float4* input [[buffer(5)]],
                                  device atomic_uint* hit [[buffer(6)]]) {
  // The host Line buffer is read as float4 slots: a,b,color,dataA,dataB,
  // widths+mode bits. Both draws use identical line and flat data.
  const uint base=(vid/6)*6;
  float4 a=input[base],b=input[base+1];
  const float4 color=input[base+2],extra=input[base+5];
  float4 da=input[base+3],db=input[base+4];
  const float2 hw=extra.xy;
  const uint mode=as_type<uint>(extra.z);
  const bool tight=(mode&1u)!=0;
  bool actuallyClipped=false;
  float4 ndc;
  if(mode&2u){
    // Optional clip-space input follows OriginalSensorLOSVertex's actual
    // SensorLOSClipLine and perspective/data interpolation before either quad.
    float enter,leave;
    float4 ca=a,cb=b;
    if(!SensorLOSClipLine(ca,cb,enter,leave)){
      SensorLOSVarying rejected{};rejected.position=float4(2,2,2,1);return rejected;
    }
    actuallyClipped=enter>0.f||leave<1.f;
    const float4 originalDA=da,originalDB=db;
    da=mix(originalDA,originalDB,enter);db=mix(originalDA,originalDB,leave);
    const float3 av=ca.xyz/ca.w,bv=cb.xyz/cb.w;
    ndc=float4(av.xy,bv.xy);
    a=float4(u.viewport.x+(av.x+1.f)*.5f*u.viewport.z,
             -u.viewport.y-(1.f-av.y)*.5f*u.viewport.w,av.z,1.f/ca.w);
    b=float4(u.viewport.x+(bv.x+1.f)*.5f*u.viewport.z,
             -u.viewport.y-(1.f-bv.y)*.5f*u.viewport.w,bv.z,1.f/cb.w);
  }else{
    ndc=float4(2.f*(a.x-u.viewport.x)/u.viewport.z-1.f,
               1.f+2.f*(a.y+u.viewport.y)/u.viewport.w,
               2.f*(b.x-u.viewport.x)/u.viewport.z-1.f,
               1.f+2.f*(b.y+u.viewport.y)/u.viewport.w);
  }
  const float extent=max(1.f,floor(u.parameters.x+.5f))*.5f+2.f;
  const float2 low=min(a.xy,b.xy)-extent,high=max(a.xy,b.xy)+extent;
  constexpr uint indices[6]={0,1,2,0,2,3};
  constexpr float2 corners[4]={float2(0,0),float2(1,0),float2(1,1),float2(0,1)};
  float2 pixel=mix(low,high,corners[indices[vid%6]]);
  // Negative control: undo R02's Y-major per-triangle reversal, recovering
  // the exact R01 order. X-major and fallback output are not changed.
  const int2 qaControl=int2(floor(a.xy*256.f+.5f)),qbControl=int2(floor(b.xy*256.f+.5f));
  const int2 deltaControl=qbControl-qaControl;
  constexpr uint undoY[6]={0,2,1,3,5,4};
  const uint tightCorner=((mode&4u) && abs(deltaControl.x)<abs(deltaControl.y))?undoY[vid%6]:vid%6;
  if(tight && SensorLOSTightPixel(a.xy,b.xy,u.parameters.x,u.viewport,u.counts.w,low,high,tightCorner,pixel)){
    atomic_fetch_add_explicit(hit,1u,memory_order_relaxed);
    if(actuallyClipped)atomic_fetch_add_explicit(hit+1,1u,memory_order_relaxed);
    const int2 qa=int2(floor(a.xy*256.f+.5f)),qb=int2(floor(b.xy*256.f+.5f));
    const int2 delta=qb-qa;
    atomic_fetch_add_explicit(hit+(abs(delta.x)>=abs(delta.y)?2:3),1u,memory_order_relaxed);
  }
  SensorLOSVarying out{};
  out.position=float4((pixel.x-u.viewport.x)/u.viewport.z*2.f-1.f,
                      1.f+2.f*(pixel.y+u.viewport.y)/u.viewport.w,.5f,1.f);
  out.lineA=a;out.lineB=b;out.dataA=da;out.dataB=db;
  out.color=color;out.halfWidths=hw;
  out.lineNDC=ndc;
  return out;
}
)MSL";

struct Output { std::vector<uint8_t> color,depth,stencil; uint32_t tightVertices=0,clippedTightVertices=0,xMajorTight=0,yMajorTight=0; };
static void Fail(const char* what,NSError* error=nil) {
  fprintf(stderr,"FAIL %s %s\n",what,error?error.localizedDescription.UTF8String:"");exit(1);
}
static id<MTLTexture> Texture(id<MTLDevice> d,MTLPixelFormat fmt,MTLTextureUsage usage) {
  auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt width:W height:H mipmapped:NO];
  td.storageMode=MTLStorageModeShared;td.usage=usage;
  auto t=[d newTextureWithDescriptor:td];if(!t)Fail("texture allocation");return t;
}
struct RenderOptions {
  uint32_t frontReference=1,backReference=1;
  MTLWinding frontWinding=MTLWindingClockwise;
  // Negative means retain the original patterned buffer, rather than override.
  float clearDepth=-1.f;
  int clearStencil=-1;
};
static Output RenderOne(id<MTLDevice> d,id<MTLCommandQueue> q,id<MTLRenderPipelineState> p,
                     id<MTLDepthStencilState> ds,id<MTLTexture> mask,id<MTLSamplerState> sampler,
                     const Uniform& u,const std::vector<Line>& lines,const RenderOptions& setup=RenderOptions{}) {
  auto color=Texture(d,MTLPixelFormatBGRA8Unorm,MTLTextureUsageRenderTarget);
  auto depth=Texture(d,MTLPixelFormatDepth32Float,MTLTextureUsageRenderTarget);
  auto stencil=Texture(d,MTLPixelFormatStencil8,MTLTextureUsageRenderTarget);
  std::vector<float> initialDepth(W*H);
  std::vector<uint8_t> initialStencil(W*H);
  for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x){
    initialDepth[y*W+x]=.30f+.60f*float((x/7+y/11)%7)/6.f;
    initialStencil[y*W+x]=uint8_t((x/9+y/13)%4);
    if(setup.clearDepth>=0.f)initialDepth[y*W+x]=setup.clearDepth;
    if(setup.clearStencil>=0)initialStencil[y*W+x]=uint8_t(setup.clearStencil);
  }
  const MTLRegion region=MTLRegionMake2D(0,0,W,H);
  [depth replaceRegion:region mipmapLevel:0 withBytes:initialDepth.data() bytesPerRow:W*4];
  [stencil replaceRegion:region mipmapLevel:0 withBytes:initialStencil.data() bytesPerRow:W];
  auto hit=[d newBufferWithLength:4*sizeof(uint32_t) options:MTLResourceStorageModeShared];
  if(!hit)Fail("hit allocation");memset(hit.contents,0,4*sizeof(uint32_t));
  auto rp=[MTLRenderPassDescriptor renderPassDescriptor];
  rp.colorAttachments[0].texture=color;rp.colorAttachments[0].loadAction=MTLLoadActionClear;
  rp.colorAttachments[0].storeAction=MTLStoreActionStore;
  rp.colorAttachments[0].clearColor=MTLClearColorMake(.12,.25,.5,.75);
  rp.depthAttachment.texture=depth;rp.depthAttachment.loadAction=MTLLoadActionLoad;
  rp.depthAttachment.storeAction=MTLStoreActionStore;
  rp.stencilAttachment.texture=stencil;rp.stencilAttachment.loadAction=MTLLoadActionLoad;
  rp.stencilAttachment.storeAction=MTLStoreActionStore;
  auto cb=[q commandBuffer];auto enc=[cb renderCommandEncoderWithDescriptor:rp];
  if(!enc)Fail("render encoder");
  [enc setRenderPipelineState:p];[enc setDepthStencilState:ds];
  [enc setCullMode:MTLCullModeNone];[enc setFrontFacingWinding:setup.frontWinding];
  [enc setVertexBytes:&u length:sizeof(u) atIndex:3];
  [enc setFragmentBytes:&u length:sizeof(u) atIndex:3];
  if(!lines.empty())[enc setVertexBytes:lines.data() length:lines.size()*sizeof(Line) atIndex:5];
  [enc setVertexBuffer:hit offset:0 atIndex:6];
  [enc setFragmentTexture:mask atIndex:1];[enc setFragmentSamplerState:sampler atIndex:1];
  [enc setStencilFrontReferenceValue:setup.frontReference backReferenceValue:setup.backReference];
  if(!lines.empty())[enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:lines.size()*6];
  [enc endEncoding];[cb commit];[cb waitUntilCompleted];
  if(cb.status!=MTLCommandBufferStatusCompleted)Fail("command completion",cb.error);
  Output out;
  out.color.resize(W*H*4);out.depth.resize(W*H*4);out.stencil.resize(W*H);
  [color getBytes:out.color.data() bytesPerRow:W*4 fromRegion:region mipmapLevel:0];
  [depth getBytes:out.depth.data() bytesPerRow:W*4 fromRegion:region mipmapLevel:0];
  [stencil getBytes:out.stencil.data() bytesPerRow:W fromRegion:region mipmapLevel:0];
  out.tightVertices=static_cast<uint32_t*>(hit.contents)[0];
  out.clippedTightVertices=static_cast<uint32_t*>(hit.contents)[1];
  out.xMajorTight=static_cast<uint32_t*>(hit.contents)[2];
  out.yMajorTight=static_cast<uint32_t*>(hit.contents)[3];return out;
}
static id<MTLTexture> Mask(id<MTLDevice> d,unsigned size) {
  auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:size height:size mipmapped:YES];
  td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead;
  auto t=[d newTextureWithDescriptor:td];if(!t)Fail("mask allocation");
  for(unsigned level=0,n=size;n;level++,n>>=1){
    std::vector<uint8_t> bytes(n*n);
    for(unsigned y=0;y<n;++y)for(unsigned x=0;x<n;++x)
      bytes[y*n+x]=uint8_t(((x*37u+y*53u+level*71u)^((x+y)*19u))&255u);
    [t replaceRegion:MTLRegionMake2D(0,0,n,n) mipmapLevel:level withBytes:bytes.data() bytesPerRow:n];
  }return t;
}
static bool Equal(const Output& a,const Output& b,const char* label,unsigned caseID) {
  auto check=[&](const std::vector<uint8_t>& x,const std::vector<uint8_t>& y,const char* plane,size_t stride){
    if(x==y)return true;
    for(size_t i=0;i<x.size();++i)if(x[i]!=y[i]){
      fprintf(stderr,"MISMATCH case=%u %s/%s pixel=(%zu,%zu) byte=%zu old=%u tight=%u\n",
              caseID,label,plane,(i/stride)%W,(i/stride)/W,i%stride,x[i],y[i]);break;
    }return false;
  };
  return check(a.color,b.color,"color",4)&check(a.depth,b.depth,"depth",4)&check(a.stencil,b.stencil,"stencil",1);
}
// R03: every existing render compares unchanged R02 coverage with the candidate.
static id<MTLRenderPipelineState> r03Pipeline, r03ReferencePipeline, r03Probe, r03ReferenceProbe;
static unsigned r03Comparisons=0;
static Output Render(id<MTLDevice> d,id<MTLCommandQueue> q,id<MTLRenderPipelineState> p,
                     id<MTLDepthStencilState> ds,id<MTLTexture> mask,id<MTLSamplerState> sampler,
                     const Uniform& u,const std::vector<Line>& lines,const RenderOptions& setup=RenderOptions{}) {
  id<MTLRenderPipelineState> reference=(p==r03Pipeline)?r03ReferencePipeline:(p==r03Probe)?r03ReferenceProbe:nil;
  if(!reference)Fail("R03 reference pipeline missing");
  auto before=RenderOne(d,q,reference,ds,mask,sampler,u,lines,setup);
  auto after=RenderOne(d,q,p,ds,mask,sampler,u,lines,setup);
  if(!Equal(before,after,"R03 exact old-vs-new coverage",r03Comparisons))Fail("R03 pixel parity");
  ++r03Comparisons;return after;
}
static float4 ClipPoint(float2 screen,float w,float depth){
  return {(2.f*screen.x/float(W)-1.f)*w,(1.f+2.f*screen.y/float(H))*w,depth*w,w};
}
// Clone the exact fragment only for the front_facing probe. Its coverage,
// texture sampling/discard and depth path are preserved. Diagnostic output is
// deliberately different for front/back; the ordinary parity suite remains exact.
static std::string FacingProbe(const std::string& shader) {
  const auto start=shader.find("fragment SensorLOSFragment OriginalSensorLOSFragment(");
  const auto end=shader.find("struct SensorLOSFanVarying",start);
  if(start==std::string::npos||end==std::string::npos)Fail("front probe fragment boundaries");
  auto probe=shader.substr(start,end-start);
  auto replaceOne=[&](const std::string& from,const std::string& to){
    const auto pos=probe.find(from);
    if(pos==std::string::npos||probe.find(from,pos+from.size())!=std::string::npos)Fail("front probe exact marker");
    probe.replace(pos,from.size(),to);
  };
  replaceOne("OriginalSensorLOSFragment(","FacingProbeFragment(");
  replaceOne("SensorLOSVarying in [[stage_in]],","SensorLOSVarying in [[stage_in]],bool front [[front_facing]],");
  replaceOne("return {color,mix(in.lineA.z,in.lineB.z,t)};",
             "return {front?float4(1,0,0,1):float4(0,1,0,1),mix(in.lineA.z,in.lineB.z,t)};");
  return probe;
}
int main(){@autoreleasepool{
  id<MTLDevice> d=MTLCreateSystemDefaultDevice();if(!d)Fail("no Metal device");
  NSError* error=nil;
  const auto facingSource=FacingProbe(rendering::metal4::NativeSensorLOSShaderSource);
  auto source=[NSString stringWithFormat:@"%s\n%s\n%s",rendering::metal4::NativeSensorLOSShaderSource,TestVS,facingSource.c_str()];
  auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;
  options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
  auto lib=[d newLibraryWithSource:source options:options error:&error];if(!lib)Fail("shader compilation",error);
  // Use the byte-identical retained original coverage function, not the candidate
  // on both sides. Both ordinary and facing-probe fragments contain this call.
  NSString* candidateCall=@"SensorLineCoveredEarly(in.lineA.xyz,in.lineB.xyz,glPixel,u.parameters.x,u.counts.w,coverageDepth)";
  NSString* originalCall=@"SensorLineCovered(in.lineA.xyz,in.lineB.xyz,glPixel,u.parameters.x,coverageDepth)";
  if([[source componentsSeparatedByString:candidateCall] count]!=3)Fail("R03 two exact reference call markers");
  NSString* referenceSource=[source stringByReplacingOccurrencesOfString:candidateCall withString:originalCall];
  auto referenceLib=[d newLibraryWithSource:referenceSource options:options error:&error];
  if(!referenceLib)Fail("R03 original reference shader compilation",error);
  auto vs=[lib newFunctionWithName:@"ParityVS"],fs=[lib newFunctionWithName:@"OriginalSensorLOSFragment"];
  if(!vs||!fs)Fail("exact fragment or test vertex missing");
  auto pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=vs;pd.fragmentFunction=fs;
  pd.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm;
  pd.colorAttachments[0].blendingEnabled=YES;
  pd.colorAttachments[0].sourceRGBBlendFactor=MTLBlendFactorSourceAlpha;
  pd.colorAttachments[0].destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
  pd.colorAttachments[0].sourceAlphaBlendFactor=MTLBlendFactorSourceAlpha;
  pd.colorAttachments[0].destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
  pd.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float;
  pd.stencilAttachmentPixelFormat=MTLPixelFormatStencil8;pd.rasterSampleCount=1;
  auto pipeline=[d newRenderPipelineStateWithDescriptor:pd error:&error];if(!pipeline)Fail("pipeline",error);
  MTLRenderPipelineDescriptor* referenceDesc=[pd copy];
  referenceDesc.vertexFunction=[referenceLib newFunctionWithName:@"ParityVS"];
  referenceDesc.fragmentFunction=[referenceLib newFunctionWithName:@"OriginalSensorLOSFragment"];
  r03ReferencePipeline=[d newRenderPipelineStateWithDescriptor:referenceDesc error:&error];
  if(!r03ReferencePipeline)Fail("R03 original reference pipeline",error);
  r03Pipeline=pipeline;
  auto dd=[MTLDepthStencilDescriptor new];dd.depthCompareFunction=MTLCompareFunctionLessEqual;dd.depthWriteEnabled=NO;
  auto face=[MTLStencilDescriptor new];
  face.stencilFailureOperation=MTLStencilOperationKeep;face.depthFailureOperation=MTLStencilOperationKeep;
  face.stencilCompareFunction=MTLCompareFunctionNotEqual;
  face.depthStencilPassOperation=MTLStencilOperationReplace;face.readMask=0xff;face.writeMask=0xff;
  dd.frontFaceStencil=face;dd.backFaceStencil=face;
  auto ds=[d newDepthStencilStateWithDescriptor:dd];if(!ds)Fail("depth stencil state");
  dd.depthWriteEnabled=YES;
  auto dsWrite=[d newDepthStencilStateWithDescriptor:dd];if(!dsWrite)Fail("depth write state");
  auto sd=[MTLSamplerDescriptor new];sd.minFilter=MTLSamplerMinMagFilterNearest;
  sd.magFilter=MTLSamplerMinMagFilterLinear;sd.mipFilter=MTLSamplerMipFilterNotMipmapped;
  sd.sAddressMode=sd.tAddressMode=MTLSamplerAddressModeClampToEdge;
  auto sampler=[d newSamplerStateWithDescriptor:sd];if(!sampler)Fail("sampler");
  sd.mipFilter=MTLSamplerMipFilterLinear;
  auto samplerMipStress=[d newSamplerStateWithDescriptor:sd];if(!samplerMipStress)Fail("mip stress sampler");
  auto maskActual=Mask(d,32),maskMipStress=Mask(d,256);auto q=[d newCommandQueue];if(!q)Fail("queue");
  Uniform emptyUniform{};
  const auto blank=Render(d,q,pipeline,ds,maskActual,sampler,emptyUniform,{});
  unsigned cases=0,positive=0,visibleOptimized=0,depthOptimized=0,clipOptimized=0,seamVisibleOptimized=0,xVisibleOptimized=0,yVisibleOptimized=0;
  const std::array<float2,16> dirs={float2{1,0},float2{1,1},float2{0,1},float2{-1,1},
                                     float2{-1,0},float2{-1,-1},float2{0,-1},float2{1,-1},
                                     float2{2,1},float2{1,2},float2{-2,1},float2{-1,2},
                                     float2{-2,-1},float2{-1,-2},float2{2,-1},float2{1,-2}};
  for(float width:{1.f,1.49f,1.5f,2.f,3.f,4.4999f,4.5f,5.f,5.01923084f,16.f,64.f})for(unsigned direction=0;direction<dirs.size();++direction)
  for(unsigned variant=0;variant<5;++variant)for(unsigned alpha=0;alpha<4;++alpha){
    Uniform u{};u.rangeColor={.8f,.3f,.1f,1.f};u.viewport={0,0,float(W),float(H)};
    u.fragmentToLogical={1,float(H),1,0};u.values={float(W),float(H),0,width};
    u.parameters={width,alpha==0?.1f:alpha==1?.5f:.9f,alpha==3?1.f:0.f,0};
    u.options={0,0,0,alpha==0?0u:alpha==1?uint32_t(256|6):alpha==2?uint32_t(256|4):uint32_t(256|1)};
    u.counts={0,0,0,1};
    float2 start=variant==0?float2{64.f,-64.f}:variant==1?float2{24.25f,-24.25f}:
                 variant==2?float2{2.25f,-2.25f}:variant==3?float2{-24.f,-64.f}:float2{64.25f,-64.25f};
    float2 delta=dirs[direction]*((variant==4?.3f:variant==2?140.f:variant==3?160.f:variant==1?80.f:48.f)/std::max(std::abs(dirs[direction].x),std::abs(dirs[direction].y)));
    Line line{};line.a={start.x,start.y,.2f,1.f};
    line.b={start.x+delta.x,start.y+delta.y,.8f,1.f};
    line.color={.7f,.6f,.9f,1.f};line.dataA={0,0,0,1};line.dataB={0,0,0,.8f};
    line.widths={width*.5f,width*.5f};
    auto activeMask=variant==2?maskMipStress:maskActual;
    auto activeSampler=variant==2?samplerMipStress:sampler;
    auto activeDS=alpha==3?dsWrite:ds;
    line.mode=0;auto old=Render(d,q,pipeline,activeDS,activeMask,activeSampler,u,{line});
    line.mode=1;auto tight=Render(d,q,pipeline,activeDS,activeMask,activeSampler,u,{line});
    if(!Equal(old,tight,"parity",cases))return 2;
    if(variant==4 && tight.tightVertices)Fail("subpixel segment unexpectedly optimized");
    const bool optimized=tight.tightVertices>0;
    const bool visible=old.color!=blank.color && old.stencil!=blank.stencil;
    positive+=optimized;
    visibleOptimized+=optimized && visible;
    xVisibleOptimized+=tight.xMajorTight>0 && visible;
    yVisibleOptimized+=tight.yMajorTight>0 && visible;
    depthOptimized+=optimized && activeDS==dsWrite && old.depth!=blank.depth;
    cases++;
  }
  // Actual homogeneous clipping is applied by the test VS before both quads.
  // Include near-plane crossing, w variation, viewport crossings, and a
  // degenerate fallback; data interpolation is common to both modes.
  for(unsigned direction=0;direction<dirs.size();++direction)for(unsigned variant=0;variant<4;++variant){
    Uniform u{};u.viewport={0,0,float(W),float(H)};
    u.fragmentToLogical={1,float(H),1,0};u.values={float(W),float(H),0,3};
    u.parameters={3,.25f,0,0};u.counts={0,0,0,1};
    Line line{};const float2 start=variant==0?float2{64.f,-64.f}:
                        variant==1?float2{-20.f,-64.f}:float2{32.f,-32.f};
    const float2 delta=dirs[direction]*((variant==3?.2f:variant==1?150.f:75.f)/std::max(std::abs(dirs[direction].x),std::abs(dirs[direction].y)));
    const float wa=variant==2?.05f:1.f,wb=variant==2?5.f:1.f;
    line.a=ClipPoint(start,wa,variant==2?-.25f:.2f);
    line.b=ClipPoint(start+delta,wb,.8f);
    line.color={.7f,.6f,.9f,1.f};line.dataA={0,0,0,1};line.dataB={0,0,0,.8f};
    line.widths={1.5f,1.5f};
    line.mode=2;auto old=Render(d,q,pipeline,ds,maskActual,sampler,u,{line});
    line.mode=3;auto tight=Render(d,q,pipeline,ds,maskActual,sampler,u,{line});
    if(!Equal(old,tight,"clipped",cases))return 2;
    const bool optimized=tight.tightVertices>0;
    positive+=optimized;
    clipOptimized+=tight.clippedTightVertices>0 && old.color!=blank.color && old.stencil!=blank.stencil;
    visibleOptimized+=optimized && old.color!=blank.color && old.stencil!=blank.stencil;
    cases++;
  }
  // Adjacent segments force shared endpoint and raster seam behavior with
  // blending, depth, and stencil receiving multiple fragment invocations.
  for(unsigned direction=0;direction<dirs.size();++direction){
    Uniform u{};u.rangeColor={.8f,.3f,.1f,1.f};u.viewport={0,0,float(W),float(H)};
    u.fragmentToLogical={1,float(H),1,0};u.values={float(W),float(H),0,3};
    u.parameters={3,.25f,0,0};u.counts={0,0,0,1};
    Line a{},b{};const float2 start={64.f,-64.f};const float2 delta=dirs[direction]*(30.f/std::max(std::abs(dirs[direction].x),std::abs(dirs[direction].y)));
    a.a={start.x,start.y,.2f,1};a.b={start.x+delta.x,start.y+delta.y,.5f,1};
    b.a=a.b;b.b={start.x+2*delta.x,start.y+2*delta.y,.8f,1};
    a.color=b.color={.7f,.6f,.9f,1.f};a.dataA=a.dataB=b.dataA=b.dataB={0,0,0,1};
    a.widths=b.widths={1.5f,1.5f};
    auto old=Render(d,q,pipeline,ds,maskActual,sampler,u,{a,b});
    a.mode=b.mode=1;auto tight=Render(d,q,pipeline,ds,maskActual,sampler,u,{a,b});
    if(!Equal(old,tight,"seam",cases))return 2;
    const bool optimized=tight.tightVertices>0;
    positive+=optimized;
    const bool visible=old.color!=blank.color && old.stencil!=blank.stencil;
    visibleOptimized+=optimized && visible;
    seamVisibleOptimized+=optimized && visible;
    cases++;
  }
  if(!positive)Fail("no optimized vertex observed");
  if(!visibleOptimized)Fail("no same-case optimized color and stencil change observed");
  if(!depthOptimized)Fail("no optimized depth-write case changed depth");
  if(!clipOptimized)Fail("no clipped optimized case observed");
  if(!seamVisibleOptimized)Fail("no same-case optimized and visible shared seam");
  if(!xVisibleOptimized||!yVisibleOptimized)Fail("optimized visible x-major and y-major coverage missing");
  printf("PASS cases=%u optimized=%u visible=%u depth=%u clipped=%u seam=%u xMajor=%u yMajor=%u; color/depth/stencil byte-identical, sampleCount=1\n",cases,positive,visibleOptimized,depthOptimized,clipOptimized,seamVisibleOptimized,xVisibleOptimized,yVisibleOptimized);

  // R02: both winding conventions must preserve the AABB face contract.
  // Profiles independently expose stencil pass, depth fail, and stencil fail;
  // the last profile varies compare/read/write masks and per-face references.
  std::array<id<MTLDepthStencilState>,4> asymmetric{};
  for(unsigned profile=0;profile<asymmetric.size();++profile){
    auto state=[MTLDepthStencilDescriptor new];
    state.depthCompareFunction=MTLCompareFunctionLessEqual;state.depthWriteEnabled=YES;
    auto front=[MTLStencilDescriptor new],back=[MTLStencilDescriptor new];
    front.stencilCompareFunction=back.stencilCompareFunction=MTLCompareFunctionAlways;
    front.readMask=0x0f;back.readMask=0xf0;
    front.writeMask=0x0f;back.writeMask=0xf0;
    front.stencilFailureOperation=MTLStencilOperationReplace;
    back.stencilFailureOperation=MTLStencilOperationInvert;
    front.depthFailureOperation=MTLStencilOperationIncrementWrap;
    back.depthFailureOperation=MTLStencilOperationDecrementWrap;
    front.depthStencilPassOperation=MTLStencilOperationReplace;
    back.depthStencilPassOperation=MTLStencilOperationInvert;
    if(profile==2)front.stencilCompareFunction=back.stencilCompareFunction=MTLCompareFunctionNever;
    if(profile==3){front.stencilCompareFunction=MTLCompareFunctionEqual;back.stencilCompareFunction=MTLCompareFunctionNotEqual;}
    state.frontFaceStencil=front;state.backFaceStencil=back;
    asymmetric[profile]=[d newDepthStencilStateWithDescriptor:state];
    if(!asymmetric[profile])Fail("asymmetric depth stencil state");
  }
  MTLRenderPipelineDescriptor* probeDesc=[pd copy];probeDesc.fragmentFunction=[lib newFunctionWithName:@"FacingProbeFragment"];
  auto probePipeline=[d newRenderPipelineStateWithDescriptor:probeDesc error:&error];
  if(!probePipeline)Fail("front facing probe pipeline",error);
  MTLRenderPipelineDescriptor* referenceProbeDesc=[probeDesc copy];
  referenceProbeDesc.vertexFunction=[referenceLib newFunctionWithName:@"ParityVS"];
  referenceProbeDesc.fragmentFunction=[referenceLib newFunctionWithName:@"FacingProbeFragment"];
  r03ReferenceProbe=[d newRenderPipelineStateWithDescriptor:referenceProbeDesc error:&error];
  if(!r03ReferenceProbe)Fail("R03 original reference probe pipeline",error);
  r03Probe=probePipeline;
  auto probeDepth=[MTLDepthStencilDescriptor new];
  probeDepth.depthCompareFunction=MTLCompareFunctionAlways;probeDepth.depthWriteEnabled=NO;
  auto probeDS=[d newDepthStencilStateWithDescriptor:probeDepth];
  if(!probeDS)Fail("facing-only depth stencil state");
  unsigned asymmetricCases=0,negativeStencil=0,negativeFacing=0;
  unsigned redFacing=0,greenFacing=0,depthPassWitness=0,depthFailWitness=0,stencilFailWitness=0;
  unsigned comparePassWitness=0,compareFailWitness=0;
  std::array<unsigned,4> profileChanged{},profileNegative{};
  unsigned xOptimized=0,yOptimized=0,clippedOptimized=0,fallbackCases=0;
  for(unsigned winding=0;winding<2;++winding)for(unsigned direction=0;direction<dirs.size();++direction)
  for(unsigned reversed=0;reversed<2;++reversed)for(unsigned variant=0;variant<4;++variant){
    Uniform u{};u.viewport={0,0,float(W),float(H)};
    u.fragmentToLogical={1,float(H),1,0};u.values={float(W),float(H),0,3};
    u.parameters={3,.25f,0,0};u.counts={0,0,0,variant==3?4u:1u};
    Line line{};const float2 start=variant==1?float2{-25.f,-64.f}:float2{64.f,-64.f};
    const float length=variant==2?.2f:variant==1?160.f:48.f;
    const float2 delta=dirs[direction]*(length/std::max(std::abs(dirs[direction].x),std::abs(dirs[direction].y)));
    line.a={start.x,start.y,.5f,1};line.b={start.x+delta.x,start.y+delta.y,.5f,1};
    const unsigned clipMode=variant==1?2u:0u;
    if(clipMode){line.a=ClipPoint(start,1.f,.5f);line.b=ClipPoint(start+delta,1.f,.5f);}
    if(reversed)std::swap(line.a,line.b);
    line.color={.7f,.6f,.9f,1};line.dataA=line.dataB={0,0,0,1};line.widths={1.5f,1.5f};
    RenderOptions setup;setup.frontWinding=winding?MTLWindingCounterClockwise:MTLWindingClockwise;
    setup.frontReference=0x35;setup.backReference=0xca;setup.clearStencil=0x35;
    for(unsigned profile=0;profile<asymmetric.size();++profile){
      setup.clearDepth=profile==1?.1f:.9f;
      // Same stored value: front low nibble equals5, back high nibble equalsc0.
      // Equal passes the front; NotEqual rejects the back.
      setup.clearStencil=profile==3?0xc5:0x35;
      const auto currentDS=asymmetric[profile];
      const auto empty=Render(d,q,pipeline,currentDS,maskActual,sampler,u,{},setup);
      line.mode=clipMode;const auto aabb=Render(d,q,pipeline,currentDS,maskActual,sampler,u,{line},setup);
      line.mode=clipMode|1u;const auto fixed=Render(d,q,pipeline,currentDS,maskActual,sampler,u,{line},setup);
      if(!Equal(aabb,fixed,"asymmetric",asymmetricCases))return 3;
      line.mode=clipMode|1u|4u;const auto legacy=Render(d,q,pipeline,currentDS,maskActual,sampler,u,{line},setup);
      const bool changed=aabb.stencil!=empty.stencil;
      const bool negative=aabb.stencil!=legacy.stencil;
      profileChanged[profile]+=changed;profileNegative[profile]+=negative;
      if(profile==0)depthPassWitness+=aabb.depth!=empty.depth && aabb.color!=empty.color;
      if(profile==1||profile==2){
        if(aabb.color!=empty.color||aabb.depth!=empty.depth)Fail("forced rejection unexpectedly wrote color/depth");
        if(profile==1)depthFailWitness+=changed;else stencilFailWitness+=changed;
      }
      if(profile==3){
        const bool wrote=aabb.color!=empty.color && aabb.depth!=empty.depth;
        const bool rejected=aabb.color==empty.color && aabb.depth==empty.depth && changed;
        comparePassWitness+=wrote;compareFailWitness+=rejected;
      }
      negativeStencil+=negative;
      if(profile==0){
        xOptimized+=fixed.xMajorTight>0;yOptimized+=fixed.yMajorTight>0;
        clippedOptimized+=fixed.clippedTightVertices>0;
      }
      // AA count=4 exercises the production guard only. Raster samples remain1;
      // this is NOT an AA4 multisample parity result.
      if(variant==2||variant==3){if(fixed.tightVertices||legacy.tightVertices)Fail("R02 guard fallback optimized");fallbackCases++;}
      if(!fixed.yMajorTight && !Equal(aabb,legacy,"negative-control-non-Y",asymmetricCases))return 4;
      asymmetricCases++;
    }
    setup.clearDepth=.9f;
    line.mode=clipMode;const auto faceAABB=Render(d,q,probePipeline,probeDS,maskActual,sampler,u,{line},setup);
    line.mode=clipMode|1u;const auto faceFixed=Render(d,q,probePipeline,probeDS,maskActual,sampler,u,{line},setup);
    if(!Equal(faceAABB,faceFixed,"front-facing",asymmetricCases))return 5;
    line.mode=clipMode|1u|4u;const auto faceLegacy=Render(d,q,probePipeline,probeDS,maskActual,sampler,u,{line},setup);
    negativeFacing+=faceAABB.color!=faceLegacy.color;
    // BGRA8: exact red and green prove both front_facing outcomes were reached,
    // independently of asymmetric stencil filtering.
    for(size_t pixel=0;pixel<faceAABB.color.size();pixel+=4){
      redFacing+=faceAABB.color[pixel]==0 && faceAABB.color[pixel+1]==0 && faceAABB.color[pixel+2]==255;
      greenFacing+=faceAABB.color[pixel]==0 && faceAABB.color[pixel+1]==255 && faceAABB.color[pixel+2]==0;
    }
  }
  if(!depthPassWitness||!depthFailWitness||!stencilFailWitness)Fail("depth/stencil outcome witnesses missing");
  if(!comparePassWitness||!compareFailWitness)Fail("asymmetric compare pass/fail witnesses missing");
  if(!redFacing||!greenFacing)Fail("front-facing probe did not expose both face classes");
  if(!negativeStencil||!negativeFacing)Fail("legacy Y-major negative control was not detected");
  for(unsigned p=0;p<4;++p)if(!profileChanged[p]||!profileNegative[p])Fail("asymmetric profile had no effective writes or negative detection");
  if(!xOptimized||!yOptimized||!clippedOptimized||!fallbackCases)Fail("R02 geometry/guard coverage missing");
  printf("PASS R02 asymmetric=%u negativeStencil=%u negativeFacing=%u x=%u y=%u clipped=%u guardFallback=%u; AA4 guard-only, GPU execution required\n",
         asymmetricCases,negativeStencil,negativeFacing,xOptimized,yOptimized,clippedOptimized,fallbackCases);
  printf("PASS R03 old-vs-new pixel comparisons=%u; actual Metal execution required\n",r03Comparisons);
  return 0;
}}
