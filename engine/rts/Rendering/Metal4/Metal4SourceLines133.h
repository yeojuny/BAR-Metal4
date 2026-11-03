/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeSourceLineData133.h"
#include "NativeSourceLineShaders133.h"
#include "Metal4RetiredArguments.h"
#include "Metal4SharedUploadPool.h"
#include <cstring>
#include <map>
#include <atomic>
#include <cstdlib>
#include <cstdio>
namespace rendering::metal4 {
// Opt-in app activation evidence only. No per-frame logging or GPU reads.
struct SourceLineProbe133 {
 const char* path=std::getenv("BAR_NATIVE_COMMON_LINE_PROBE_FILE");
 struct Row {std::atomic<uint64_t> draws{0},segments{0},uploadBytes{0},scratchBytes{0},prefixPasses{0};};
 std::array<Row,6> rows;
 ~SourceLineProbe133(){if(!path||!*path)return;if(auto f=std::fopen(path,"w")){std::fprintf(f,"origin,samples,draws,segments,uploadBytes,scratchBytes,prefixPasses\n");for(unsigned i=0;i<6;++i){const auto& r=rows[i];std::fprintf(f,"%u,%u,%llu,%llu,%llu,%llu,%llu\n",i/2,i%2?4:1,(unsigned long long)r.draws.load(),(unsigned long long)r.segments.load(),(unsigned long long)r.uploadBytes.load(),(unsigned long long)r.scratchBytes.load(),(unsigned long long)r.prefixPasses.load());}std::fclose(f);}}
};
inline SourceLineProbe133& CommonLineProbe133(){static SourceLineProbe133 p;return p;}
struct SourceLineRaster133 {std::array<float,4> viewport,target;std::array<uint32_t,4> options;};
struct SourceLineSurface133 {std::array<float,4> scale;std::array<uint32_t,4> options;std::array<float,4> parameters,scale1;std::array<uint32_t,4> options1;std::array<float,4> parameters1;};
static_assert(sizeof(SourceLineRaster133)==48&&sizeof(SourceLineSurface133)==96);
struct PreparedSourceLines133 {
 struct Pass {__strong id<MTLComputePipelineState> pipeline=nil;__strong id<MTL4ArgumentTable> arguments=nil;uint32_t count=0;bool scan=false;};
 std::shared_ptr<const NativeSourceLineInput133> source;
 std::shared_ptr<ScreenUploadStorage> uploadToken;
 RetiredArgumentOwners argumentTokens;
 __strong id<MTLBuffer> upload=nil,scratch=nil;
 __strong id<MTL4ArgumentTable> arguments=nil;
 std::vector<Pass> passes;
 bool encoded=false;
 void EncodeDependencies(id<MTL4CommandBuffer> command){
  if(passes.empty())return;
  Require(!encoded,"Common line prefix was encoded twice");
  auto e=[command computeCommandEncoder];Require(e,"Cannot encode common line prefix");
  for(size_t i=0;i<passes.size();++i){const auto& p=passes[i];
   if(i)[e barrierAfterEncoderStages:MTLStageDispatch beforeEncoderStages:MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];
   [e setComputePipelineState:p.pipeline];[e setArgumentTable:p.arguments];
   if(p.scan)[e dispatchThreadgroups:MTLSizeMake((p.count+255)/256,1,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
   else [e dispatchThreads:MTLSizeMake(p.count,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];
  }
  [e barrierAfterStages:MTLStageDispatch beforeQueueStages:MTLStageVertex visibilityOptions:MTL4VisibilityOptionDevice];[e endEncoding];encoded=true;
 }
 void RequireReady()const{Require(passes.empty()||encoded,"Common line draw precedes its prefix");}
};
class SourceLinesGPU133 {
 ScreenUploadPool uploads;
 RetiredArguments arguments;
 const void* context=nullptr;__strong id<MTLDevice> device=nil;
 std::map<unsigned,id<MTLLibrary>> libraries;
 std::map<std::array<uint64_t,5>,id<MTLRenderPipelineState>> pipelines;
 std::array<__strong id<MTLComputePipelineState>,3> compute{};
 bool computeValidation=false;
 static size_t Align(size_t n){return (n+255)&~size_t(255);}
 static MTLBlendOperation Equation133(ScreenBlendEquation x){switch(x){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Common line blend equation");}
 static MTLBlendFactor Factor133(ScreenBlendFactor x){switch(x){case ScreenBlendFactor::Zero:return MTLBlendFactorZero;case ScreenBlendFactor::One:return MTLBlendFactorOne;case ScreenBlendFactor::SourceAlpha:return MTLBlendFactorSourceAlpha;case ScreenBlendFactor::OneMinusSourceAlpha:return MTLBlendFactorOneMinusSourceAlpha;case ScreenBlendFactor::DestinationAlpha:return MTLBlendFactorDestinationAlpha;case ScreenBlendFactor::OneMinusDestinationAlpha:return MTLBlendFactorOneMinusDestinationAlpha;case ScreenBlendFactor::SourceColor:return MTLBlendFactorSourceColor;case ScreenBlendFactor::OneMinusSourceColor:return MTLBlendFactorOneMinusSourceColor;case ScreenBlendFactor::DestinationColor:return MTLBlendFactorDestinationColor;case ScreenBlendFactor::OneMinusDestinationColor:return MTLBlendFactorOneMinusDestinationColor;case ScreenBlendFactor::SourceAlphaSaturate:return MTLBlendFactorSourceAlphaSaturated;}throw std::runtime_error("Common line blend factor");}
 static MTLColorWriteMask Mask133(unsigned m){return MTLColorWriteMask((m&1?MTLColorWriteMaskRed:0)|(m&2?MTLColorWriteMaskGreen:0)|(m&4?MTLColorWriteMaskBlue:0)|(m&8?MTLColorWriteMaskAlpha:0));}
 id<MTLLibrary> Library(RuntimeLease& lease,unsigned depth,bool mrt,bool combined,double& shaderMs){
  unsigned key=depth|unsigned(mrt)<<2|unsigned(combined)<<3;auto found=libraries.find(key);if(found!=libraries.end())return found->second;
  const auto start=Clock::now();auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
  options.preprocessorMacros=@{@"COMMON_DEPTH0_133":@(depth&1),@"COMMON_DEPTH1_133":@(depth&2),@"COMMON_MRT133":@(mrt),@"COMMON_COMBINED133":@(combined)};
  NSError* error=nil;auto lib=lease.NewLibraryWithSource(String(std::string(SourceLineMath133)+SourceLineStages133+SourceLineFragment133),options,&error);
  Require(lib,"Cannot compile original common line shaders: "+Error(error));libraries.emplace(key,lib);shaderMs+=Milliseconds(start);return lib;
 }
 void CompileCompute(RuntimeLease& lease,bool validation,double& shaderMs){
  if(compute[0]&&computeValidation==validation)return;
  auto lib=Library(lease,0,false,false,shaderMs);const auto start=Clock::now();
  NSArray<NSString*>* names=@[@"CommonCount133",@"CommonScan133",@"CommonCarry133"];
  for(unsigned i=0;i<3;++i){auto f=[MTL4LibraryFunctionDescriptor new];f.library=lib;f.name=names[i];auto d=[MTL4ComputePipelineDescriptor new];d.computeFunctionDescriptor=f;d.options=[MTL4PipelineOptions new];d.options.shaderValidation=ShaderValidationMode(validation);NSError* error=nil;compute[i]=[lease.Compiler() newComputePipelineStateWithDescriptor:d compilerTaskOptions:nil error:&error];Require(compute[i],"Cannot compile common line prefix: "+Error(error));}
  computeValidation=validation;shaderMs+=Milliseconds(start);
 }
public:
 void Clear(){arguments.Clear();libraries.clear();pipelines.clear();compute={};context=nullptr;device=nil;}
 void BeginBatch(RuntimeLease& lease){if(context!=lease.ContextIdentity()||device!=lease.Device()){Clear();context=lease.ContextIdentity();device=lease.Device();}uploads.BeginBatch(device);arguments.BeginBatch(lease);}
 id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const ScreenBlendState& b,const std::array<bool,4>& mask,MTLPixelFormat format,unsigned samples,bool alphaToCoverage,unsigned depth,bool validation,double& shaderMs,bool mrt=false,bool combined=false,unsigned combinedMask=15){
  Require(depth<=3&&(!combined||mrt),"Common line pipeline family differs");unsigned m=0;for(unsigned i=0;i<4;++i)if(mask[i])m|=1u<<i;
  const uint64_t blend=uint64_t(b.enabled)|uint64_t(b.sourceColor)<<1|uint64_t(b.destinationColor)<<5|uint64_t(b.sourceAlpha)<<9|uint64_t(b.destinationAlpha)<<13|uint64_t(b.colorEquation)<<17|uint64_t(b.alphaEquation)<<20;
  const std::array<uint64_t,5> key{blend,uint64_t(format),lease.SampleShadingKey(samples),uint64_t(m)|uint64_t(combinedMask)<<4,uint64_t(alphaToCoverage)|uint64_t(depth)<<1|uint64_t(validation)<<3|uint64_t(mrt)<<4|uint64_t(combined)<<5};
  if(auto i=pipelines.find(key);i!=pipelines.end()){lease.Retain(i->second);return i->second;}
  auto lib=Library(lease,depth,mrt,combined,shaderMs);const auto start=Clock::now();auto d=[MTL4RenderPipelineDescriptor new];
  auto vs=[MTL4LibraryFunctionDescriptor new];vs.library=lib;vs.name=@"CommonVertexMain133";d.vertexFunctionDescriptor=vs;
  auto fs=[MTL4LibraryFunctionDescriptor new];fs.library=lib;fs.name=@"CommonFragmentMain133";d.fragmentFunctionDescriptor=fs;
  d.rasterSampleCount=samples;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.alphaToCoverageState=alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;d.options=[MTL4PipelineOptions new];d.options.shaderValidation=ShaderValidationMode(validation);
  for(unsigned i=0;i<(combined?6u:mrt?5u:1u);++i){auto c=d.colorAttachments[i];c.pixelFormat=format;c.blendingState=b.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;c.sourceRGBBlendFactor=Factor133(b.sourceColor);c.destinationRGBBlendFactor=Factor133(b.destinationColor);c.sourceAlphaBlendFactor=Factor133(b.sourceAlpha);c.destinationAlphaBlendFactor=Factor133(b.destinationAlpha);c.rgbBlendOperation=Equation133(b.colorEquation);c.alphaBlendOperation=Equation133(b.alphaEquation);c.writeMask=Mask133(i==5?combinedMask:m);}
  NSError* error=nil;auto p=lease.NewRenderPipeline(d,&error);Require(p,"Cannot compile original common line pipeline: "+Error(error));if(pipelines.size()>=256)pipelines.erase(pipelines.begin());pipelines.emplace(key,p);lease.Retain(p);shaderMs+=Milliseconds(start);return p;
 }
 std::shared_ptr<PreparedSourceLines133> Prepare(RuntimeLease& lease,std::shared_ptr<const NativeSourceLineInput133> source,const std::array<double,4>& viewport,unsigned width,unsigned height,unsigned samples,const ScreenDrawState& s,id<MTLTexture> image,id<MTLSamplerState> sampler,id<MTLTexture> image1,id<MTLSamplerState> sampler1,bool validation,double& shaderMs){
  Require(source&&sourceLine133::Material(s)&&width&&height&&width<=16384&&height<=16384&&(samples==1||samples==4),"Common line source/attachment differs");
  sourceLine133::Validate(*source);auto p=std::make_shared<PreparedSourceLines133>();p->source=std::move(source);const auto& in=*p->source;uint32_t count=in.segments.size();
  SourceLineRaster133 raster{{},{float(width),float(height),0,0},{samples,uint32_t(in.connectedStipple),count,0}};
  for(unsigned i=0;i<4;++i){Require(std::isfinite(viewport[i])&&std::abs(viewport[i])<=32768,"Common line viewport outside bounds");raster.viewport[i]=viewport[i];}
  SourceLineSurface133 surface{{1,1,0,0},{uint32_t(s.material),uint32_t(s.texture&&s.texture->flipV),uint32_t(s.alphaTest.enabled),uint32_t(s.alphaTest.compare)},{s.alphaTest.reference,float(s.sampler.wrapS==ScreenWrap::LegacyClamp),float(s.sampler.wrapT==ScreenWrap::LegacyClamp),float(s.texture&&s.texture->incomplete)},{},{},{}};
  if(s.texture&&!s.texture->incomplete&&s.uvMode==ScreenUVMode::Texel){Require(s.texture->width&&s.texture->height,"Common line texture extent empty");surface.scale[0]=1.f/s.texture->width;surface.scale[1]=1.f/s.texture->height;}
  const auto& second=s.secondary;if(second.texture&&s.material!=ScreenMaterial::FontCoverage){surface.scale1={1,1,0,0};surface.options1={1,uint32_t(second.texture->flipV),0,0};surface.parameters1={0,float(second.sampler.wrapS==ScreenWrap::LegacyClamp),float(second.sampler.wrapT==ScreenWrap::LegacyClamp),float(second.texture->incomplete)};if(!second.texture->incomplete&&second.uvMode==ScreenUVMode::Texel){Require(second.texture->width&&second.texture->height,"Common line secondary extent empty");surface.scale1[0]=1.f/second.texture->width;surface.scale1[1]=1.f/second.texture->height;}}
  const size_t segmentOffset=Align(in.vertices.size()*sizeof(NativeWorldDrawVertex)),transformOffset=Align(segmentOffset+in.segments.size()*sizeof(NativeSourceLineSegment133)),rasterOffset=Align(transformOffset+in.transforms.size()*sizeof(NativeSourceLineTransform133)),surfaceOffset=Align(rasterOffset+sizeof(raster)),countsOffset=Align(surfaceOffset+sizeof(surface));
  struct Level {uint32_t n;size_t input,output,blocks;};std::vector<Level> levels;size_t scratchBytes=0;
  if(in.connectedStipple){CompileCompute(lease,validation,shaderMs);scratchBytes=size_t(count)*16;size_t previous=0;uint32_t n=count;do{size_t out=Align(scratchBytes),blocks=Align(out+size_t(n)*16);levels.push_back({n,previous,out,blocks});uint32_t next=(n+255)/256;scratchBytes=blocks+size_t(next)*16;previous=blocks;n=next;}while(n>1);}
  const size_t bytes=countsOffset+std::max(size_t(16),levels.size()*256);Require(bytes<=device.maxBufferLength&&scratchBytes<=device.maxBufferLength,"Common line packet exceeds device extent");
  p->uploadToken=uploads.Acquire(device,bytes);p->upload=p->uploadToken?p->uploadToken->buffer:[device newBufferWithLength:bytes options:MTLResourceStorageModeShared];Require(p->upload&&p->upload.contents,"Cannot allocate original common line input");lease.AddAllocation(p->upload);
  auto* dst=static_cast<uint8_t*>(p->upload.contents);std::memcpy(dst,in.vertices.data(),in.vertices.size()*sizeof(NativeWorldDrawVertex));std::memcpy(dst+segmentOffset,in.segments.data(),in.segments.size()*sizeof(NativeSourceLineSegment133));std::memcpy(dst+transformOffset,in.transforms.data(),in.transforms.size()*sizeof(NativeSourceLineTransform133));std::memcpy(dst+rasterOffset,&raster,sizeof(raster));std::memcpy(dst+surfaceOffset,&surface,sizeof(surface));std::memset(dst+countsOffset,0,bytes-countsOffset);
  auto table=[&](unsigned buffers,unsigned textures=0){auto d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=buffers;d.maxTextureBindCount=d.maxSamplerStateBindCount=textures;d.initializeBindings=YES;NSError* error=nil;return arguments.Acquire(lease,d,p->argumentTokens,&error);};
  const auto address=p->upload.gpuAddress;uint64_t prefix=address+countsOffset;
  if(in.connectedStipple){p->scratch=[device newBufferWithLength:scratchBytes options:MTLResourceStorageModePrivate];Require(p->scratch,"Cannot allocate bounded common line prefix scratch");lease.AddAllocation(p->scratch);const auto scratch=p->scratch.gpuAddress;prefix=scratch+levels.front().output;
   auto a=table(5);[a setAddress:address atIndex:0];[a setAddress:address+segmentOffset atIndex:1];[a setAddress:address+transformOffset atIndex:2];[a setAddress:address+rasterOffset atIndex:3];[a setAddress:scratch atIndex:4];p->passes.push_back({compute[0],a,count,false});
   for(size_t i=0;i<levels.size();++i){const auto& l=levels[i];std::memcpy(dst+countsOffset+i*256,&l.n,4);auto a=table(4);[a setAddress:scratch+l.input atIndex:0];[a setAddress:scratch+l.output atIndex:1];[a setAddress:scratch+l.blocks atIndex:2];[a setAddress:address+countsOffset+i*256 atIndex:3];p->passes.push_back({compute[1],a,l.n,true});}
   for(size_t i=levels.size()-1;i>0;--i){const auto& l=levels[i-1];auto a=table(3);[a setAddress:scratch+l.output atIndex:0];[a setAddress:scratch+levels[i].output atIndex:1];[a setAddress:address+countsOffset+(i-1)*256 atIndex:2];p->passes.push_back({compute[2],a,l.n,false});}
  }
  p->arguments=table(6,2);auto a=p->arguments;[a setAddress:address atIndex:0];[a setAddress:address+segmentOffset atIndex:1];[a setAddress:address+transformOffset atIndex:2];[a setAddress:address+rasterOffset atIndex:3];[a setAddress:prefix atIndex:4];[a setAddress:address+surfaceOffset atIndex:5];[a setTexture:image.gpuResourceID atIndex:0];[a setTexture:image1.gpuResourceID atIndex:1];[a setSamplerState:sampler.gpuResourceID atIndex:0];[a setSamplerState:sampler1.gpuResourceID atIndex:1];
  auto& probe=CommonLineProbe133();if(probe.path&&*probe.path){auto& r=probe.rows[in.sourceKind*2+(samples==4)];r.draws.fetch_add(1,std::memory_order_relaxed);r.segments.fetch_add(count,std::memory_order_relaxed);r.uploadBytes.fetch_add(bytes,std::memory_order_relaxed);r.scratchBytes.fetch_add(scratchBytes,std::memory_order_relaxed);r.prefixPasses.fetch_add(p->passes.size(),std::memory_order_relaxed);}return p;
 }
};
}
