/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Independent original ProjFX equations and blend/depth/clip oracle.
#include "Rendering/Metal4/Metal4WorldParticles.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace rendering;
using namespace rendering::metal4;
namespace {
unsigned checks=0,renders=0;
void Check(bool valid,const char* text){++checks;if(!valid)throw std::runtime_error(text);}
std::array<float,16> Identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
NativeWorldParticleFrame Frame(bool ground,bool animation,bool fog,float alpha) {
 NativeWorldParticleFrame f;f.cameraViewProjection=Identity();f.cameraRight={1,0,0};f.cameraUp={0,1,0};f.fogDistances=fog?std::array<float,2>{0,2}:std::array<float,2>{100,200};f.fogColor={.1f,.2f,.3f};
 auto atlas=std::make_shared<NativeWorldParticleAtlas>();atlas->sourceName="Actual independent 3-cell/two-page test";atlas->generateMipmaps=true;atlas->requestedMipLevels=2;
 for(unsigned page=0;page<2;++page){NativeWorldParticleMip m;m.width=3;m.height=1;m.rgba={32,64,128,128,128,160,192,128,224,224,224,128};if(page)for(size_t n=0;n<m.rgba.size();n+=4){m.rgba[n]=255-m.rgba[n];m.rgba[n+1]=255-m.rgba[n+1];}atlas->pages.push_back({m});}
 f.atlas=f.groundAtlas=atlas;
 for(auto position:std::array<std::array<float,3>,4>{{{-.75f,-.75f,0},{.75f,-.75f,0},{.75f,.75f,0},{-.75f,.75f,0}}}){NativeWorldParticleVertex v;v.position=position;v.uvw={.5f,.5f,1};v.uvInfo={0,0,1,1};v.animation=animation?std::array<float,3>{3,1,.25f}:std::array<float,3>{1,1,0};v.color={128,64,32,uint8_t(std::lround(alpha*255))};f.vertices.push_back(v);}
 f.indices={0,1,2,0,2,3};NativeWorldParticleRange r;r.indexCount=6;r.stage=ground?NativeWorldParticleStage::GroundFlashes:NativeWorldParticleStage::AlphaAll;r.groundAtlas=ground;r.alphaThreshold=ground?.01f:0;f.ranges={r};return f;
}
std::array<uint8_t,4> Reference(bool ground,bool animation,bool fog,float alpha) {
 // Same-revision ProjFXVertProg/FragProg, seeded independently of native GPU.
 std::array<float,4> tex=animation?std::array<float,4>{175.f/255,143.f/255,160.f/255,128.f/255}:std::array<float,4>{127.f/255,95.f/255,192.f/255,128.f/255};
 const std::array<float,4> color={128.f/255,64.f/255,32.f/255,float(std::lround(alpha*255))/255};std::array<float,4> c;for(unsigned n=0;n<4;++n)c[n]=tex[n]*color[n];
 const float amount=fog?std::clamp((2-std::sqrt(2*.75f*.75f))/2,0.f,1.f):1.f;for(unsigned n=0;n<3;++n)c[n]=c[n]*amount+float(n+1)*.1f*c[3]*(1-amount);
 std::array<uint8_t,4> result;const std::array<uint8_t,4> clear={5,8,10,255};for(unsigned n=0;n<4;++n)result[n]=uint8_t(std::lround(std::clamp(c[n]*(ground?c[3]:1.f)+(float(clear[n])/255)*(ground?1.f:1-c[3]),0.f,1.f)*255));return result;
}
std::vector<uint8_t> Render(WorldParticlesGPU& gpu,NativeWorldParticleFrame f,float depthClear,bool omit) {
 RuntimeLease lease;constexpr unsigned size=32;auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:size height:size mipmapped:NO];d.storageMode=MTLStorageModeShared;d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;auto color=[lease.Device() newTextureWithDescriptor:d];lease.AddAllocation(color);
 d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:size height:size mipmapped:NO];d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;auto depth=[lease.Device() newTextureWithDescriptor:d];lease.AddAllocation(depth);
 WorldCamera camera;camera.view=camera.projection=Identity();camera.clipDepth=ClipDepthConvention::MinusOneToOne;camera.viewport={0,0,size,size};double compile=0;auto prepared=omit?std::unique_ptr<PreparedWorldParticles>{}:gpu.Prepare(lease,f,camera,MTLPixelFormatRGBA8Unorm,MTLPixelFormatDepth32Float,nil,true,compile);
 auto pass=[MTL4RenderPassDescriptor new];pass.renderTargetWidth=pass.renderTargetHeight=size;pass.colorAttachments[0].texture=color;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(5./255,8./255,10./255,1);pass.depthAttachment.texture=depth;pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.storeAction=MTLStoreActionStore;pass.depthAttachment.clearDepth=depthClear;
 lease.CommitResources();auto start=Clock::now();auto command=lease.Begin("Actual particle atlas/blend/clip independent oracle");if(prepared)prepared->EncodeDependencies(command);auto e=[command renderCommandEncoderWithDescriptor:pass];[e setViewport:MTLViewport{0,0,size,size,0,1}];[e setScissorRect:MTLScissorRect{0,0,size,size}];if(prepared)prepared->Encode(f.ranges[0].stage,e);[e endEncoding];lease.SubmitAndWait(start,renders++);if(prepared)prepared->DidComplete();double read=0;return lease.Readback(color,size,size,read);
}
}
int main(int argc,char** argv){@autoreleasepool{try{
 Check(argc>=2,"usage: particle-atlas-gpu output.json [omit]");WorldParticlesGPU gpu;const bool omit=argc>2;
 for(bool ground:{false,true})for(bool animation:{false,true})for(bool fog:{false,true}){
  const auto pixels=Render(gpu,Frame(ground,animation,fog,.5f),.95f,omit);const auto expected=Reference(ground,animation,fog,.5f);for(unsigned n=0;n<4;++n)Check(std::abs(int(pixels[(16*32+16)*4+n])-int(expected[n]))<=1,"Original particle animation/fog/blend scalar pixel oracle failed");
 }
 for(unsigned kind=0;kind<4;++kind){auto frame=Frame(false,false,false,kind==0?0:.5f);if(kind==1)frame.ranges[0].stage=NativeWorldParticleStage::AlphaBelowWater;if(kind==2)frame.ranges[0].stage=NativeWorldParticleStage::AlphaAboveWater;for(auto& v:frame.vertices){if(kind==1)v.position[1]=.25f+v.position[1]*.2f;if(kind==2)v.position[1]=-.25f+v.position[1]*.2f;}const auto pixels=Render(gpu,frame,kind==3?.1f:.95f,false);for(size_t n=0;n<pixels.size();++n)Check(pixels[n]==std::array<uint8_t,4>{5,8,10,255}[n%4],"Original particle alpha/depth/water rejection did not preserve background");}
 std::ofstream out(argv[1]);out<<"{\"status\":\"PASS\",\"renders\":"<<renders<<",\"checks\":"<<checks<<",\"oracle\":\"Independent original ProjFX interpolation/fog/blend/alpha/depth/clip equations\"}\n";std::cout<<"PASS "<<renders<<" actual atlas/ground/alpha renders / "<<checks<<" checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}}
