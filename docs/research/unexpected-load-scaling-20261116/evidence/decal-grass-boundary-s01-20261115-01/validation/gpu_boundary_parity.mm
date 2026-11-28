// Test-only Metal4 protocol harness, never linked into the engine.
// Exercises the SAME checkpoint and F->F split, with deterministic synthetic
// draws. This proves controlled attachment parity, not whole-game byte parity.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>
#include <array>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
using namespace simd;
static constexpr unsigned W=64,H=64;
struct Uniform {float4 tint,shape;};
static const char* Shader=R"MSL(
#include <metal_stdlib>
using namespace metal;
struct U {float4 tint,shape;};
struct O {float4 position [[position]];float4 color;};
vertex O PlainVS(uint v [[vertex_id]],constant U& u [[buffer(0)]]) {
 constexpr float2 p[6]={float2(-1,-1),float2(1,-1),float2(1,1),float2(-1,-1),float2(1,1),float2(-1,1)};
 O out;out.position=float4(u.shape.xy+p[v]*u.shape.w,u.shape.z,1);out.color=u.tint;return out;
}
vertex O BoundaryVS(uint v [[vertex_id]],constant U& u [[buffer(0)]],
 texture2d<float> a [[texture(0)]],texture2d<float> b [[texture(1)]],
 texture2d<float> c [[texture(2)]],texture2d<float> d [[texture(3)]],texture2d<float> e [[texture(4)]]) {
 constexpr float2 p[6]={float2(-1,-1),float2(1,-1),float2(1,1),float2(-1,-1),float2(1,1),float2(-1,1)};
 const float light=(a.read(uint2(0)).r+b.read(uint2(0)).r+c.read(uint2(0)).r+d.read(uint2(0)).r+e.read(uint2(0)).r)*.2f;
 O out;out.position=float4(u.shape.xy+p[v]*u.shape.w,u.shape.z,1);
 out.color=float4(u.tint.rgb*light,u.tint.a);return out;
}
fragment float4 BoundaryFS(O in [[stage_in]]){return in.color;}
)MSL";
static void Check(bool value,const char* what,NSError* e=nil){if(!value){fprintf(stderr,"FAIL %s %s\n",what,e?e.localizedDescription.UTF8String:"");std::exit(1);}}
static id<MTLTexture> Texture(id<MTLDevice> device,MTLPixelFormat format,unsigned size=64){
 auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:size height:size mipmapped:NO];
 d.storageMode=MTLStorageModeShared;d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
 auto image=[device newTextureWithDescriptor:d];Check(image!=nil,"texture");return image;
}
struct Output {std::vector<uint8_t> color,depth,stencil;};
static Output Render(id<MTLDevice> device,id<MTL4CommandQueue> queue,id<MTLRenderPipelineState> pipeline,id<MTLRenderPipelineState> plainPipeline,id<MTLRenderPipelineState> producerPipeline,
 id<MTLDepthStencilState> depthState,unsigned seed,bool split){
 @autoreleasepool {
 NSError* error=nil;
 auto color=Texture(device,MTLPixelFormatBGRA8Unorm),depth=Texture(device,MTLPixelFormatDepth32Float),stencil=Texture(device,MTLPixelFormatStencil8);
 std::vector<uint8_t> initialColor(W*H*4),initialStencil(W*H);std::vector<float> initialDepth(W*H);
 for(unsigned i=0;i<W*H;++i){initialColor[i*4]=uint8_t(i+seed);initialColor[i*4+1]=uint8_t(i/64);initialColor[i*4+2]=12;initialColor[i*4+3]=255;initialDepth[i]=((i+seed)%11==0)?.25f:1.f;initialStencil[i]=uint8_t((i+seed)%4);}
 const auto region=MTLRegionMake2D(0,0,W,H);
 [color replaceRegion:region mipmapLevel:0 withBytes:initialColor.data() bytesPerRow:W*4];
 [depth replaceRegion:region mipmapLevel:0 withBytes:initialDepth.data() bytesPerRow:W*4];
 [stencil replaceRegion:region mipmapLevel:0 withBytes:initialStencil.data() bytesPerRow:W];
 std::array<id<MTLTexture>,5> inputs;
 const uint8_t zero[4]={0,0,0,255};
 for(unsigned i=0;i<5;++i){inputs[i]=Texture(device,MTLPixelFormatRGBA8Unorm,1);uint8_t pixel[4]={uint8_t(40+i*30+seed%17),0,0,255};[inputs[i] replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:pixel bytesPerRow:4];}
 [inputs[0] replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:zero bytesPerRow:4];
 auto upload=Texture(device,MTLPixelFormatRGBA8Unorm,1);uint8_t uploaded[4]={uint8_t(180+seed%40),0,0,255};
 [upload replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:uploaded bytesPerRow:4];
 auto snapshot=Texture(device,MTLPixelFormatDepth32Float);
 auto allocator=[device newCommandAllocator];auto command=[device newCommandBuffer];
 auto rd=[MTLResidencySetDescriptor new];rd.initialCapacity=32;
 auto residency=[device newResidencySetWithDescriptor:rd error:&error];Check(residency!=nil,"residency",error);
 for(auto image:inputs)[residency addAllocation:image];
 for(auto image:{color,depth,stencil,upload,snapshot})[residency addAllocation:image];
 std::array<id<MTLBuffer>,12> constants;
 std::array<id<MTL4ArgumentTable>,12> arguments;
 for(unsigned n=0;n<12;++n){
  const bool grass=n>=4;Uniform u{};
  u.tint=float4{float((n*31+seed)%255)/255.f,float((n*67+29)%255)/255.f,float((n*13+79)%255)/255.f,grass?.45f:.3f};
  u.shape=float4{float(int(n%3)-1)*.28f,float(int(n%4)-1)*.18f,grass?(.15f+float(n%4)*.18f):(.3f+float(n)*.12f),grass?.5f:.85f};
  if(n==11){u.tint=float4{1,1,1,1};u.shape=float4{0,0,.01f,.125f};}
  constants[n]=[device newBufferWithBytes:&u length:sizeof(u) options:MTLResourceStorageModeShared];[residency addAllocation:constants[n]];
  auto ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=1;ad.maxTextureBindCount=5;ad.initializeBindings=YES;
  arguments[n]=[device newArgumentTableWithDescriptor:ad error:&error];Check(arguments[n]!=nil,"arguments",error);
  [arguments[n] setAddress:constants[n].gpuAddress atIndex:0];
  for(unsigned i=0;i<5;++i)[arguments[n] setTexture:inputs[i].gpuResourceID atIndex:i];
 }
 const unsigned producerByte=180+seed%50;
 Uniform producerUniform{float4{float(producerByte)/255.f,0,0,1},float4{0,0,.5f,1}};
 auto producerConstants=[device newBufferWithBytes:&producerUniform length:sizeof(producerUniform) options:MTLResourceStorageModeShared];[residency addAllocation:producerConstants];
 auto producerAD=[MTL4ArgumentTableDescriptor new];producerAD.maxBufferBindCount=1;producerAD.initializeBindings=YES;
 auto producerArguments=[device newArgumentTableWithDescriptor:producerAD error:&error];Check(producerArguments!=nil,"producer arguments",error);
 [producerArguments setAddress:producerConstants.gpuAddress atIndex:0];
 [residency commit];[command beginCommandBufferWithAllocator:allocator];
 // Actual Fragment producer. Its result is consumed by the later Vertex stage.
 auto producer=[MTL4RenderPassDescriptor new];producer.renderTargetWidth=producer.renderTargetHeight=1;
 producer.colorAttachments[0].texture=inputs[0];producer.colorAttachments[0].loadAction=MTLLoadActionDontCare;
 producer.colorAttachments[0].storeAction=MTLStoreActionStore;
 auto pe=[command renderCommandEncoderWithDescriptor:producer];Check(pe!=nil,"producer encoder");
 [pe setViewport:MTLViewport{0,0,1,1,0,1}];[pe setScissorRect:MTLScissorRect{0,0,1,1}];[pe setCullMode:MTLCullModeNone];
 [pe setRenderPipelineState:producerPipeline];[pe setArgumentTable:producerArguments atStages:MTLRenderStageVertex];
 [pe drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];[pe endEncoding];
 auto prior=[command computeCommandEncoder];Check(prior!=nil,"prior blit");
 [prior copyFromTexture:upload sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(1,1,1) toTexture:inputs[1] destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[prior endEncoding];
 // Exact baseline snapshot stage masks, unchanged by S01.
 auto copy=[command computeCommandEncoder];Check(copy!=nil,"checkpoint");
 [copy barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
 [copy copyFromTexture:depth sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(W,H,1) toTexture:snapshot destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
 [copy barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];
 auto pass=[MTL4RenderPassDescriptor new];pass.renderTargetWidth=W;pass.renderTargetHeight=H;pass.defaultRasterSampleCount=1;
 pass.colorAttachments[0].texture=color;pass.colorAttachments[0].loadAction=MTLLoadActionLoad;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
 pass.depthAttachment.texture=depth;pass.depthAttachment.loadAction=MTLLoadActionLoad;pass.depthAttachment.storeAction=MTLStoreActionStore;
 pass.stencilAttachment.texture=stencil;pass.stencilAttachment.loadAction=MTLLoadActionLoad;pass.stencilAttachment.storeAction=MTLStoreActionStore;
 auto encoder=[command renderCommandEncoderWithDescriptor:pass];Check(encoder!=nil,"scene encoder");
 for(unsigned n=0;n<12;++n){
  if(split&&n==4){
   [encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];
   encoder=[command renderCommandEncoderWithDescriptor:pass];Check(encoder!=nil,"split encoder");
   [encoder barrierAfterQueueStages:MTLStageFragment beforeStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
  }
  [encoder setViewport:MTLViewport{0,0,W,H,0,1}];
  const unsigned inset=seed%7;[encoder setScissorRect:MTLScissorRect{inset,inset,W-inset*2,H-inset*2}];
  [encoder setCullMode:MTLCullModeNone];[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
  [encoder setDepthClipMode:MTLDepthClipModeClip];[encoder setDepthBias:0 slopeScale:0 clamp:0];[encoder setTriangleFillMode:MTLTriangleFillModeFill];
  [encoder setStencilFrontReferenceValue:n+1 backReferenceValue:n+1];
  [encoder setRenderPipelineState:n<4?plainPipeline:pipeline];[encoder setDepthStencilState:depthState];
  [encoder setArgumentTable:arguments[n] atStages:MTLRenderStageVertex|MTLRenderStageFragment];
  [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
 }
 [encoder endEncoding];[command useResidencySet:residency];[command endCommandBuffer];
 auto event=[device newSharedEvent];auto options=[MTL4CommitOptions new];auto done=dispatch_semaphore_create(0);
 __block NSError* gpuError=nil;
 [options addFeedbackHandler:^(id<MTL4CommitFeedback> result){gpuError=result.error;dispatch_semaphore_signal(done);}];
 id<MTL4CommandBuffer> commands[]={command};[queue commit:commands count:1 options:options];[queue signalEvent:event value:1];
 Check([event waitUntilSignaledValue:1 timeoutMS:30000],"GPU event");
 Check(dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,30*NSEC_PER_SEC))==0,"GPU feedback");Check(gpuError==nil,"GPU error",gpuError);
 Output out;out.color.resize(W*H*4);out.depth.resize(W*H*4);out.stencil.resize(W*H);
 [color getBytes:out.color.data() bytesPerRow:W*4 fromRegion:region mipmapLevel:0];
 [depth getBytes:out.depth.data() bytesPerRow:W*4 fromRegion:region mipmapLevel:0];
 [stencil getBytes:out.stencil.data() bytesPerRow:W fromRegion:region mipmapLevel:0];
 std::vector<float> snapshotPixels(W*H);[snapshot getBytes:snapshotPixels.data() bytesPerRow:W*4 fromRegion:region mipmapLevel:0];
 Check(snapshotPixels==initialDepth,"snapshot copy executed before scene writes");
 bool grassWitness=false;for(auto value:out.stencil)grassWitness|=value>=5;Check(grassWitness,"Grass-only stencil witness");
 // Last G draw writes alpha=1 and the nearest depth at the center. Its RGB
 // must contain BOTH actual GPU-produced texels, not equally stale inputs.
 const unsigned sum=producerByte+uploaded[0]+(100+seed%17)+(130+seed%17)+(160+seed%17);
 const int expected=int((sum+2)/5);const size_t center=(H/2*W+W/2)*4;
 for(unsigned c=0;c<3;++c)Check(std::abs(int(out.color[center+c])-expected)<=1,"GPU producer to Grass Vertex value");
 Check(out.stencil[H/2*W+W/2]==12,"last Grass probe coverage");
 Check(out.color!=initialColor,"visible color coverage");Check(std::memcmp(out.depth.data(),initialDepth.data(),out.depth.size())!=0,"depth writes");Check(out.stencil!=initialStencil,"stencil writes");
 return out;
 }
}
int main(){@autoreleasepool {
 auto device=MTLCreateSystemDefaultDevice();Check(device!=nil,"device");auto queue=[device newMTL4CommandQueue];Check(queue!=nil,"Metal4 queue");
 NSError* error=nil;auto compiler=[device newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&error];Check(compiler!=nil,"compiler",error);
 auto ld=[MTL4LibraryDescriptor new];ld.source=[NSString stringWithUTF8String:Shader];
 auto co=[MTLCompileOptions new];co.mathMode=MTLMathModeSafe;co.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;ld.options=co;
 auto library=[compiler newLibraryWithDescriptor:ld error:&error];Check(library!=nil,"library",error);
 auto makePipeline=[&](NSString* vertex,MTLPixelFormat format,bool blend){
 auto pd=[MTL4RenderPipelineDescriptor new];auto vs=[MTL4LibraryFunctionDescriptor new],fs=[MTL4LibraryFunctionDescriptor new];vs.library=fs.library=library;vs.name=vertex;fs.name=@"BoundaryFS";
 pd.vertexFunctionDescriptor=vs;pd.fragmentFunctionDescriptor=fs;pd.rasterSampleCount=1;pd.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
 auto a=pd.colorAttachments[0];a.pixelFormat=format;a.blendingState=blend?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=MTLBlendFactorSourceAlpha;a.destinationRGBBlendFactor=MTLBlendFactorOneMinusSourceAlpha;a.sourceAlphaBlendFactor=MTLBlendFactorOne;a.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
 auto po=[MTL4PipelineOptions new];po.shaderValidation=MTLShaderValidationEnabled;pd.options=po;
 auto pipeline=[compiler newRenderPipelineStateWithDescriptor:pd compilerTaskOptions:nil error:&error];Check(pipeline!=nil,"pipeline",error);
 return pipeline;
 };
 auto pipeline=makePipeline(@"BoundaryVS",MTLPixelFormatBGRA8Unorm,true);
 auto plainPipeline=makePipeline(@"PlainVS",MTLPixelFormatBGRA8Unorm,true);
 auto producerPipeline=makePipeline(@"PlainVS",MTLPixelFormatRGBA8Unorm,false);
 auto ds=[MTLDepthStencilDescriptor new];ds.depthCompareFunction=MTLCompareFunctionLessEqual;ds.depthWriteEnabled=YES;
 auto face=[MTLStencilDescriptor new];face.stencilCompareFunction=MTLCompareFunctionAlways;face.depthStencilPassOperation=MTLStencilOperationReplace;face.readMask=face.writeMask=255;ds.frontFaceStencil=ds.backFaceStencil=face;
 auto depthState=[device newDepthStencilStateWithDescriptor:ds];Check(depthState!=nil,"depth state");
 for(unsigned seed=0;seed<128;++seed){auto before=Render(device,queue,pipeline,plainPipeline,producerPipeline,depthState,seed,false),after=Render(device,queue,pipeline,plainPipeline,producerPipeline,depthState,seed,true);
  Check(before.color==after.color,"color byte parity");Check(before.depth==after.depth,"depth byte parity");Check(before.stencil==after.stencil,"stencil byte parity");}
 puts("PASS cases=128 color_depth_stencil_byte_identical=128 visible_color_depth_stencil_writes=128 Metal4_checkpoint_and_split=1 actual_fragment_producer=1 grass_only_producer_value_witness=128 snapshot_copy_witness=128 synthetic_protocol_not_full_game=1");
 return 0;
}}
