/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Metal4/Metal4ScreenNative.h"
#include "Rendering/Backend/NativeMiniMapData.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace rendering;using namespace rendering::metal4;
constexpr uint32_t SIZE=32;uint32_t checks=0,renders=0;
void Check(bool test,const char* message){++checks;if(!test)throw std::runtime_error(message);}
ScreenBlendState Disabled(){ScreenBlendState value;value.enabled=false;return value;}
void Quad(ScreenDrawList& list){list.BeginQuads();list.TexCoord(0,0);list.Vertex(0,0);list.TexCoord(1,0);list.Vertex(SIZE,0);list.TexCoord(1,1);list.Vertex(SIZE,SIZE);list.TexCoord(0,1);list.Vertex(0,SIZE);list.End();}
std::shared_ptr<NativeMiniMapBackground> Source(float height=100) {
	auto image=std::make_shared<ScreenTexture>();image->width=image->height=2;image->bytesPerRow=8;image->bytes.assign(16,255);image->sourceName="Independent minimap oracle base";
	auto heights=std::make_shared<WorldTerrainHeights>();heights->width=heights->height=9;heights->corners.assign(81,height);
	auto source=std::make_shared<NativeMiniMapBackground>();source->squaresX=source->squaresZ=source->paddedSquaresX=source->paddedSquaresZ=8;source->base=image;source->heights=heights;
	source->lighting.groundAmbient={.25f,.5f,.75f,0};source->lighting.groundDiffuse={.25f,.125f,0,0};source->lighting.lightDirection={0,1,0,0};
	source->lighting.waterBase={.4f,.6f,.8f};source->lighting.waterAbsorb={.01f,.02f,.03f};source->lighting.waterMinimum={.1f,.15f,.2f};return source;
}
std::vector<uint8_t> Render(ScreenGPU& gpu,const ScreenDrawList& list,std::span<const ScreenRenderTargetPass> targets={}) {
	RuntimeLease lease;double shaderMs=0,readbackMs=0;
	auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:SIZE height:SIZE mipmapped:NO];descriptor.storageMode=MTLStorageModeShared;descriptor.usage=MTLTextureUsageRenderTarget;
	auto image=[lease.Device() newTextureWithDescriptor:descriptor];Require(image!=nil,"Cannot allocate minimap oracle target");lease.AddAllocation(image);
	auto frame=gpu.Prepare(lease,{&list,SIZE,SIZE,{},targets},SIZE,SIZE,SIZE,SIZE,MTLPixelFormatRGBA8Unorm,true,shaderMs);
	lease.CommitResources();const auto start=Clock::now();auto command=lease.Begin("Original native minimap GPU pixel oracle");frame->EncodeDependencies(command);
	auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=image;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.renderTargetWidth=pass.renderTargetHeight=SIZE;
	auto encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"Cannot create minimap oracle encoder");frame->Encode(encoder);[encoder endEncoding];
	lease.SubmitAndWait(start,renders++);frame->DidComplete();return lease.Readback(image,SIZE,SIZE,readbackMs);
}
std::vector<uint8_t> Background(ScreenGPU& gpu,std::shared_ptr<const NativeMiniMapBackground> source) {
	ScreenDrawList list;list.SetBlend(Disabled());list.SetMiniMapBackground(std::move(source));Quad(list);return Render(gpu,list);
}
int Byte(float value){return int(std::lround(std::clamp(value,0.f,1.f)*255));}
void Pixel(const std::vector<uint8_t>& bytes,uint32_t x,uint32_t y,std::array<float,4> expected,int tolerance=2) {
	const size_t offset=(size_t(SIZE-1-y)*SIZE+x)*4;
	for(size_t c=0;c<4;++c){++checks;if(std::abs(int(bytes[offset+c])-Byte(expected[c]))>tolerance)throw std::runtime_error("Minimap analytic pixel mismatch: "+std::to_string(x)+","+std::to_string(y)+" c"+std::to_string(c)+" actual"+std::to_string(bytes[offset+c])+" expected"+std::to_string(Byte(expected[c])));}
}
std::shared_ptr<const NativeMiniMapCoverage> Coverage(uint16_t count) {auto value=std::make_shared<NativeMiniMapCoverage>();value->width=value->height=4;value->counts.assign(16,count);return value;}
std::shared_ptr<NativeMiniMapInfo> Info(uint16_t ground,uint16_t air,uint16_t radar,uint16_t jammer,bool global=false) {
	auto info=std::make_shared<NativeMiniMapInfo>();info->mode=NativeMiniMapInfoMode::Los;info->groundLos=Coverage(ground);info->airLos=Coverage(air);info->radar=Coverage(radar);info->jammer=Coverage(jammer);info->globalLos=global;
	info->alwaysColor={.1f,.15f,.2f,0};info->losColor={.3f,.25f,.2f,0};info->radarColor={.05f,.1f,.15f,0};info->radarColor2={1.f,.6f,.2f,0};info->jammerColor={.2f,.1f,.05f,0};return info;
}
void LandWaterAndDeformation() {
	ScreenGPU gpu;auto source=Source();const auto land=Background(gpu,source);
	for(uint32_t y=8;y<24;++y)for(uint32_t x=8;x<24;++x)Pixel(land,x,y,{105.f/255,131.f/255,158.f/255,1});
	Check(Background(gpu,source)==land,"unchanged immutable minimap owner changed pixels");
	// Independent planar slope: dh/dx=1/2 and dh/dz=1/4, normal=(-2,4,-1)/sqrt21.
	auto sloped=std::make_shared<NativeMiniMapBackground>(*source);auto heights=std::make_shared<WorldTerrainHeights>(*source->heights);
	for(uint32_t z=0;z<9;++z)for(uint32_t x=0;x<9;++x)heights->corners[z*9+x]=100+4.f*x+2.f*z;
	sloped->heights=heights;const auto slope=Background(gpu,sloped);const float ndl=4/std::sqrt(21.f);
	Pixel(slope,16,16,{(.25f+.25f*ndl)*210/255,(.5f+.125f*ndl)*210/255,.75f*210/255,1});
	Check(slope!=land,"deformed owner did not update actual normal shading");
	auto shallow=Source(-5);const auto water=Background(gpu,shallow);
	Pixel(water,16,16,{.35f*.5f+(105.f/255)*.5f,.5f*.5f+(131.25f/255)*.5f,.65f*.5f+(157.5f/255)*.5f,205.f/255});
	auto deep=Source(-20);Pixel(Background(gpu,deep),16,16,{.2f,.2f,.2f,55.f/255});
	ScreenGPU reset;Check(Background(reset,source)==land,"new GPU resource lifecycle changed original minimap pixels");
}
void OrientationAndOriginalMips() {
	ScreenGPU gpu;auto source=Source();source->lighting.groundAmbient={255.f/210,255.f/210,255.f/210,0};source->lighting.groundDiffuse={0,0,0,0};
	auto image=std::make_shared<ScreenTexture>();image->width=image->height=4;image->bytesPerRow=16;
	for(uint32_t y=0;y<4;++y)for(uint32_t x=0;x<4;++x){const std::array<uint8_t,4> color=y<2?(x<2?std::array<uint8_t,4>{255,0,0,255}:std::array<uint8_t,4>{0,255,0,255}):(x<2?std::array<uint8_t,4>{0,0,255,255}:std::array<uint8_t,4>{255,255,0,255});image->bytes.insert(image->bytes.end(),color.begin(),color.end());}
	source->base=image;auto normal=Background(gpu,source);Pixel(normal,5,5,{1,0,0,1});Pixel(normal,26,5,{0,1,0,1});Pixel(normal,5,26,{0,0,1,1});Pixel(normal,26,26,{1,1,0,1});
	auto flipped=std::make_shared<ScreenTexture>(*image);flipped->flipV=true;source=std::make_shared<NativeMiniMapBackground>(*source);source->base=flipped;
	auto inverted=Background(gpu,source);Pixel(inverted,5,5,{0,0,1,1});Pixel(inverted,26,26,{0,1,0,1});
	Check(normal!=inverted,"base orientation negative control did not differ");
	// Deliberately different original levels: strong minification chooses level2
	// after original MiniMapFragProg's -2 bias, rather than discarding all mips.
	auto mip=std::make_shared<ScreenTexture>();mip->width=mip->height=512;
	for(uint32_t level=0,side=512;level<10;++level,side=std::max(1u,side/2)){ScreenTextureMip m;m.width=m.height=side;m.bytesPerRow=size_t(side)*4;const std::array<uint8_t,4> color=level==2?std::array<uint8_t,4>{255,0,255,255}:std::array<uint8_t,4>{0,255,255,255};for(size_t p=0;p<size_t(side)*side;++p)m.bytes.insert(m.bytes.end(),color.begin(),color.end());mip->mips.push_back(std::move(m));}
	source->base=mip;Pixel(Background(gpu,source),16,16,{1,0,1,1});
}
void LosRadarTemporalAndPaddedMap() {
	ScreenGPU gpu;auto source=Source();source->info=Info(0,0,0,0);source->paddedSquaresX=16;
	// Independent original RGB10_A2 blend recurrence. First LOS update clears
	// to .5, then applies sourceAlpha .05 with nearest RGB10 quantization.
	std::array<float,3> previous{.5f,.5f,.5f};
	for(uint32_t step=0;step<5;++step){auto packet=std::make_shared<NativeMiniMapBackground>(*source);auto info=Info(0,0,0,0);info->time=float(step);info->highQuality=(step%2)!=0;packet->info=info;
		for(size_t c=0;c<3;++c)previous[c]=std::round((info->alwaysColor[c]*.05f+previous[c]*.95f)*1023)/1023;
		auto bytes=Background(gpu,packet);Pixel(bytes,16,16,{105.f/255+previous[0]-.5f,131.f/255+previous[1]-.5f,158.f/255+previous[2]-.5f,1});
		Check(Background(gpu,packet)==bytes,"replaying same info snapshot blended it a second time");
	}
	// Large raw counts must produce binary coverage, not normalized 16-bit
	// intensity. All-LOS and global-LOS must match even with jammer present.
	ScreenGPU a,b;auto full=Source();full->info=Info(65535,19,5,0);auto global=Source();global->info=Info(0,0,0,65535,true);
	Check(Background(a,full)==Background(b,global),"global LOS differs from real full coverage/no jammer");
	ScreenGPU radarGPU;auto radar=Source();radar->info=Info(0,0,65535,0);auto values=Background(radarGPU,radar);
	std::array<float,3> expected{};for(size_t c=0;c<3;++c){const auto& i=*radar->info;const float infill=i.radarColor2[c]-std::floor(i.radarColor2[c]);expected[c]=std::round(((i.alwaysColor[c]+infill)*.05f+.5f*.95f)*1023)/1023;}
	Pixel(values,16,16,{105.f/255+expected[0]-.5f,131.f/255+expected[1]-.5f,158.f/255+expected[2]-.5f,1});
}
void CachedTargetDependency() {
	ScreenGPU gpu;auto source=Source();auto identity=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{991,SIZE,SIZE});
	auto texture=std::make_shared<ScreenTexture>();texture->width=texture->height=SIZE;texture->flipV=true;texture->generation=1;texture->renderTarget=identity;
	auto list=std::make_shared<ScreenDrawList>();list->SetBlend(Disabled());list->SetMiniMapBackground(source);Quad(*list);
	ScreenRenderTargetPass pass;pass.destination=texture;pass.coordinateWidth=pass.coordinateHeight=SIZE;pass.drawList=list;pass.load=ScreenTargetLoad::Clear;
	ScreenDrawList root;root.SetBlend(Disabled());root.SetTexture(texture);Quad(root);
	auto first=Render(gpu,root,{&pass,1});Check(Render(gpu,root)==first,"retired real minimap target changed cached pixels");
	Check(Background(gpu,source)==first,"GL RTT orientation differs from direct native minimap material");
}
}
int main(){@autoreleasepool {try{LandWaterAndDeformation();OrientationAndOriginalMips();LosRadarTemporalAndPaddedMap();CachedTargetDependency();std::cout<<"{\"checks\":"<<checks<<",\"renders\":"<<renders<<",\"validation\":true}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}}
