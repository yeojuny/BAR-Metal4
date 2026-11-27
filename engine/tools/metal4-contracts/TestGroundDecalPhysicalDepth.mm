#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>
#include <vector>
#include <array>
#include <cmath>
#include <iostream>
#include "NativeGroundDecalShaders.h"
#include "Metal4GroundDecalDepthMapping.h"
using namespace rendering::metal4;
struct alignas(16) U {simd_float4x4 view,inverse,shadow;simd_float4 dims[8],frameScreen,fragmentToLogical;simd_uint4 flags,depthCrop;simd_float4 logicalOrigin;};
static_assert(sizeof(U)==400);
static void req(bool b,NSString* why){if(!b){std::cerr<<why.UTF8String<<"\n";std::exit(1);}}
static NSString* str(const std::string& s){return [NSString stringWithUTF8String:s.c_str()];}
static const char* writeShader=R"MSL(
#include <metal_stdlib>
using namespace metal;
vertex float4 WriteDepthVertex(uint i [[vertex_id]]) {const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return float4(p[i],0,1);}
struct DepthOutput {float value [[depth(any)]];};
fragment DepthOutput WriteDepth(float4 p [[position]],uint sid [[sample_id]]) {return {float((uint(p.x)+3*uint(p.y)+11*sid)%97+1)/128.f};}
)MSL";
static const char* probe=R"MSL(
kernel void ProbeDecalDepth(constant DecalUniforms& u [[buffer(0)]],
#ifdef HIGH_QUALITY
depth2d_ms<float> terrainDepth [[texture(0)]],
#else
depth2d<float> terrainDepth [[texture(0)]],
#endif
device float4* output [[buffer(1)]],device const float2* samplePos [[buffer(2)]],constant uint& samples [[buffer(3)]],uint3 p [[thread_position_in_grid]]) {
 if(p.x>=terrainDepth.get_width()||p.y>=terrainDepth.get_height()||p.z>=samples)return;
 DecalFragmentProgram program;program.depthTex=terrainDepth;program.flags=u.flags;program.screenSizeInverse=u.frameScreen.yz;program.gl_ModelViewProjectionMatrixInverse=u.inverseViewProjection;
#ifdef BAR_GROUND_DECAL_PHYSICAL_DEPTH
 const float2 position=float2(p.xy+u.depthCrop.xy)+samplePos[p.z];
 program.nativeDepthPixel=uint2(clamp(int2(floor(position))-int2(u.depthCrop.xy),int2(0),int2(u.depthCrop.zw)-1));
 program.nativeViewOrigin=u.logicalOrigin.xy;
#else
 const float2 position=float2(p.xy+uint2(0,uint(u.fragmentToLogical.w)-terrainDepth.get_height()))+samplePos[p.z];
#endif
 program.gl_FragCoord=float4(position*u.fragmentToLogical.xy+u.fragmentToLogical.zw,0,1);
 program.gl_SampleID=p.z;
 output[(p.y*terrainDepth.get_width()+p.x)*samples+p.z]=float4(program.GetFragmentWorldPos(),1);
}
)MSL";
int main(){@autoreleasepool{
 auto dev=MTLCreateSystemDefaultDevice();NSError* err=nil;auto q=[dev newCommandQueue];req(q,@"queue");auto compiler=[dev newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&err];req(compiler,err.description?:@"compiler");
 auto depthLib=[dev newLibraryWithSource:str(writeShader) options:nil error:&err];req(depthLib,err.description?:@"depth library");
 auto dsDesc=[MTLDepthStencilDescriptor new];dsDesc.depthWriteEnabled=YES;dsDesc.depthCompareFunction=MTLCompareFunctionAlways;auto ds=[dev newDepthStencilStateWithDescriptor:dsDesc];
 unsigned total=0,legacyMatches=0;double maxError=0;
 struct Geo{std::array<int32_t,4> view;std::array<uint32_t,2> logical,render;};
 Geo geo[]={{{0,0,17,13},{17,13},{17,13}},{{0,0,17,13},{17,15},{17,13}},{{2,1,7,5},{17,13},{31,23}},{{-2,3,11,8},{19,15},{27,31}},{{0,0,17,13},{17,13},{34,26}},{{0,0,23,17},{23,19},{12,10}}};
 for(unsigned n:{1u,2u,4u}){@autoreleasepool{
  auto pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=[depthLib newFunctionWithName:@"WriteDepthVertex"];pd.fragmentFunction=[depthLib newFunctionWithName:@"WriteDepth"];pd.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float;pd.rasterSampleCount=n;auto writePSO=[dev newRenderPipelineStateWithDescriptor:pd error:&err];req(writePSO,err.description?:@"write PSO");
  id<MTLComputePipelineState> psos[2];for(bool physical:{false,true}){
   auto msl=std::string(physical?"#define BAR_GROUND_DECAL_PHYSICAL_DEPTH 1\n":"")+NativeGroundDecalShaderSource(n>1,true)+probe;
   auto d=[MTL4LibraryDescriptor new];d.source=str(msl);auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;d.options=options;auto lib=[compiler newLibraryWithDescriptor:d error:&err];req(lib,err.description?:@"ground decal library");psos[physical]=[dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"ProbeDecalDepth"] error:&err];req(psos[physical],err.description?:@"ground decal probe PSO");
  }
  std::vector<MTLSamplePosition> positions(n);[dev getDefaultSamplePositions:positions.data() count:n];auto posBuffer=[dev newBufferWithBytes:positions.data() length:n*sizeof(MTLSamplePosition) options:MTLResourceStorageModeShared];
  auto countBuffer=[dev newBufferWithBytes:&n length:sizeof(n) options:MTLResourceStorageModeShared];
  for(auto g:geo){auto mapped=MapWorldViewport(g.view,g.logical[0],g.logical[1],g.render[0],g.render[1]);req(bool(mapped),@"map");auto m=MatchGroundDecalDepthMapping(g.view,g.logical,g.render,mapped->scissor);req(bool(m),@"crop");auto crop=m->crop;unsigned w=crop[2],h=crop[3];U u{};u.inverse=matrix_identity_float4x4;u.frameScreen={0,1.f/g.view[2],1.f/g.view[3],0};u.fragmentToLogical={m->fragmentToLogical[0],m->fragmentToLogical[1],m->fragmentToLogical[2],m->fragmentToLogical[3]};u.flags={1,0,0,1u<<5};u.depthCrop={crop[0],crop[1],crop[2],crop[3]};u.logicalOrigin={float(g.view[0]),float(g.view[1]),0,0};
   auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:w height:h mipmapped:NO];td.storageMode=MTLStorageModePrivate;td.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;if(n>1){td.textureType=MTLTextureType2DMultisample;td.sampleCount=n;}auto depth=[dev newTextureWithDescriptor:td];req(depth,@"depth");
   auto pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.depthAttachment.texture=depth;pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.storeAction=MTLStoreActionStore;pass.depthAttachment.clearDepth=1;auto cb=[q commandBuffer];auto e=[cb renderCommandEncoderWithDescriptor:pass];[e setRenderPipelineState:writePSO];[e setDepthStencilState:ds];[e setViewport:MTLViewport{0,0,double(w),double(h),0,1}];[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[e endEncoding];
   auto ub=[dev newBufferWithBytes:&u length:sizeof(u) options:MTLResourceStorageModeShared];std::array<id<MTLBuffer>,2> out{};
   for(bool physical:{false,true}){if(!physical&&!m->legacyExact)continue;out[physical]=[dev newBufferWithLength:w*h*n*16 options:MTLResourceStorageModeShared];auto c=[cb computeCommandEncoder];[c setComputePipelineState:psos[physical]];[c setTexture:depth atIndex:0];[c setBuffer:ub offset:0 atIndex:0];[c setBuffer:out[physical] offset:0 atIndex:1];[c setBuffer:posBuffer offset:0 atIndex:2];[c setBuffer:countBuffer offset:0 atIndex:3];[c dispatchThreads:MTLSizeMake(w,h,n) threadsPerThreadgroup:MTLSizeMake(4,4,1)];[c endEncoding];}
   [cb commit];[cb waitUntilCompleted];req(cb.status==MTLCommandBufferStatusCompleted,cb.error.description?:@"GPU completion");
   auto got=static_cast<const simd_float4*>(out[1].contents);
   for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)for(unsigned sid=0;sid<n;++sid){auto r=got[(y*w+x)*n+sid];double px=crop[0]+x+positions[sid].x,py=crop[1]+y+positions[sid].y;auto v=mapped->viewport;
    double expect[3]={2*(px-v[0])/v[2]-1,2*(v[1]+v[3]-py)/v[3]-1,double((x+3*y+11*sid)%97+1)/128.0};
    for(unsigned j=0;j<3;++j){double delta=std::abs(r[j]-expect[j]);maxError=std::max(maxError,delta);if(delta>2e-6){std::cerr<<"Mismatch n="<<n<<" x="<<x<<" y="<<y<<" sample="<<sid<<" channel="<<j<<" got="<<r[j]<<" expected="<<expect[j]<<"\n";return 2;}}
    ++total;
   }
   if(m->legacyExact){auto prior=static_cast<const simd_float4*>(out[0].contents);for(unsigned i=0;i<w*h*n;++i){req(memcmp(&got[i],&prior[i],16)==0,@"legacy mapping changed unit-scale GPU positions");++legacyMatches;}}
   std::cout<<"samples="<<n<<" crop="<<crop[0]<<","<<crop[1]<<","<<w<<","<<h<<" legacy="<<m->legacyExact<<" samples_checked="<<w*h*n<<"\n";
  }
 }}
 std::cout<<"PASS "<<total<<" actual GPU depth/unprojection samples; bit-identical legacy "<<legacyMatches<<"; max error "<<maxError<<"\n";
}}
