/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Metal4/Metal4MapEdgeRenderer.h"
#include "Rendering/Textures/NativeUIDDS.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace rendering;
using namespace rendering::metal4;
namespace {
constexpr uint32_t SIZE=64;constexpr float EXTENT=128;constexpr int TOLERANCE=2;
using Color=std::array<float,4>;
constexpr Color BACKGROUND{.05f,.07f,.09f,1};
uint32_t checks=0,renders=0;std::string device,queue,commandClass,encoderClass;
void Check(bool value,const std::string& reason){++checks;if(!value)throw std::runtime_error(reason);}
int Byte(float value){return int(std::lround(std::clamp(value,0.f,1.f)*255));}
WorldMatrix Identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
WorldMatrix Ortho(float l,float r,float b,float t){auto m=Identity();m[0]=2/(r-l);m[5]=2/(t-b);m[10]=-2/999.f;m[12]=-(r+l)/(r-l);m[13]=-(t+b)/(t-b);m[14]=-1001.f/999.f;return m;}
std::shared_ptr<WorldTexture> Image(std::array<uint8_t,4> texel)
{
	auto image=std::make_shared<WorldTexture>();image->sourcePath="independent exact RGBA fixture texel";image->mips.push_back({1,1,4,{texel.begin(),texel.end()}});return image;
}
WorldFrame Frame(std::vector<mapedge::Mirror> mirrors={{1,0,-1,0},{0,0,0,0}})
{
	WorldFrame frame;frame.viewCamera.clipDepth=ClipDepthConvention::MinusOneToOne;
	frame.viewCamera.view={1,0,0,0,0,0,1,0,0,-1,0,0,0,0,-100,1};frame.viewCamera.projection=Ortho(-EXTENT,0,-EXTENT,0);
	frame.terrain.squaresX=frame.terrain.squaresZ=16;
	auto heights=std::make_shared<WorldTerrainHeights>();heights->width=heights->height=17;heights->corners.assign(17*17,0);frame.terrain.heights=heights;
	auto draw=std::make_shared<mapedge::DrawPacket>();draw->mirrors=std::move(mirrors);draw->parameters.curvature=draw->parameters.fog=false;
	auto resources=std::make_shared<WorldMapEdgeResources>();resources->color=Image({64,96,128,255});resources->normal=Image({191,64,223,255});
	frame.mapEdge=WorldMapEdge{draw,resources,{.3f,.7f,.2f,1},{.2f,.25f,.3f,1},{50,250,0,1.f/200}};return frame;
}
void Change(WorldFrame& frame,mapedge::Parameters parameters)
{
	auto next=std::make_shared<mapedge::DrawPacket>(*frame.mapEdge->draw);next->parameters=parameters;frame.mapEdge->draw=next;
}
struct Pixels{std::vector<uint8_t> rgba;std::vector<float> depth;};
Pixels Render(MapEdgeGPU& gpu,const WorldFrame& frame)
{
	RuntimeLease lease;double shader=0,readback=0;auto edge=gpu.Prepare(lease,frame,MTLPixelFormatRGBA8Unorm,true,shader);
	auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:SIZE height:SIZE mipmapped:NO];descriptor.storageMode=MTLStorageModeShared;descriptor.usage=MTLTextureUsageRenderTarget;
	auto color=[lease.Device() newTextureWithDescriptor:descriptor];Require(color!=nil,"fixture color allocation failed");lease.AddAllocation(color);
	descriptor.pixelFormat=MTLPixelFormatDepth32Float;descriptor.storageMode=MTLStorageModePrivate;auto depth=[lease.Device() newTextureWithDescriptor:descriptor];Require(depth!=nil,"fixture depth allocation failed");lease.AddAllocation(depth);
	auto depthBytes=[lease.Device() newBufferWithLength:SIZE*SIZE*4 options:MTLResourceStorageModeShared];Require(depthBytes!=nil,"fixture depth buffer failed");lease.AddAllocation(depthBytes);
	lease.CommitResources();const auto start=Clock::now();auto command=lease.Begin("Original mirrored terrain family offscreen oracle");
	auto pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=color;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(BACKGROUND[0],BACKGROUND[1],BACKGROUND[2],BACKGROUND[3]);
	pass.depthAttachment.texture=depth;pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.storeAction=MTLStoreActionStore;pass.depthAttachment.clearDepth=1;pass.renderTargetWidth=pass.renderTargetHeight=SIZE;
	auto encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"fixture native encoder failed");encoderClass=NSStringFromClass([encoder class]).UTF8String;
	[encoder setViewport:MTLViewport{0,0,SIZE,SIZE,0,1}];[encoder setScissorRect:MTLScissorRect{0,0,SIZE,SIZE}];if(edge)edge->Encode(encoder);[encoder endEncoding];
	auto copy=[command computeCommandEncoder];Require(copy!=nil,"fixture native copy failed");[copy barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
	[copy copyFromTexture:depth sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(SIZE,SIZE,1) toBuffer:depthBytes destinationOffset:0 destinationBytesPerRow:SIZE*4 destinationBytesPerImage:0];[copy endEncoding];
	auto timing=lease.SubmitAndWait(start,renders++);Check(!timing.cpuSimulationMs,"offscreen fixture unexpectedly has simulation timing");
	device=lease.Capabilities().deviceName;queue=lease.QueueClass();commandClass=lease.CommandBufferClass();Pixels out;out.rgba=lease.Readback(color,SIZE,SIZE,readback);out.depth.resize(SIZE*SIZE);std::memcpy(out.depth.data(),depthBytes.contents,SIZE*SIZE*4);return out;
}
void Pixel(const Pixels& pixels,uint32_t x,uint32_t y,Color expected,const std::string& label)
{
	const size_t start=(size_t(SIZE-1-y)*SIZE+x)*4;for(size_t c=0;c<4;++c)Check(std::abs(int(pixels.rgba[start+c])-Byte(expected[c]))<=TOLERANCE,label+" pixel"+std::to_string(x)+","+std::to_string(y)+" channel"+std::to_string(c)+" expected"+std::to_string(Byte(expected[c]))+" actual"+std::to_string(pixels.rgba[start+c]));
}
Color Shade(std::array<float,3> normal,float flipX,float flipZ,float brightness=.3f)
{
	const float r=64.f/255,g=96.f/255,b=128.f/255;
	const float y=std::clamp((.2126f*r+.7152f*g+.0722f*b)*brightness,0.f,1.f),cb=-.114572f*r-.385428f*g+.5f*b,cr=.5f*r-.454153f*g-.0458471f*b;
	const float length=std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);if(length>.001f)for(float& v:normal)v/=length;else normal={0,1,0};normal[0]*=flipX!=0? -1.f:1.f;normal[2]*=flipZ!=0? -1.f:1.f;
	const float light=1+.5f*(normal[0]*.3f+normal[1]*.7f+normal[2]*.2f);
	return {std::clamp((y+1.5748f*cr)*light,0.f,1.f),std::clamp((y-.187324f*cb-.468124f*cr)*light,0.f,1.f),std::clamp((y+1.8556f*cb)*light,0.f,1.f),1};
}
Color ConstantShade(float flipX,float flipZ,float brightness=.3f){return Shade({191.f/255*2-1,223.f/255*2-1,64.f/255*2-1},flipX,flipZ,brightness);}
void MirrorsAndLifecycle()
{
	MapEdgeGPU gpu;auto frame=Frame();auto output=Render(gpu,frame);
	for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x)Pixel(output,x,y,ConstantShade(1,0),"original mirrored diffuse/normal-light/Y-Cb-Cr");
	auto p=frame.mapEdge->draw->parameters;p.brightness=.6f;Change(frame,p);output=Render(gpu,frame);Pixel(output,15,27,ConstantShade(1,0,.6f),"original brightness/night control");
	frame.mapEdge.reset();output=Render(gpu,frame);for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x)Pixel(output,x,y,BACKGROUND,"disabled original widget leaves no stale border");
	std::vector<mapedge::Mirror> all;for(float z:{-1.f,0.f,1.f})for(float x:{-1.f,0.f,1.f})if(x!=0||z!=0)all.push_back({float(x!=0),float(z!=0),x,z});all.push_back({});frame=Frame(all);frame.viewCamera.projection=Ortho(-EXTENT,2*EXTENT,-2*EXTENT,EXTENT);output=Render(gpu,frame);
	for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x){const float wx=-EXTENT+(float(x)+.5f)*3*EXTENT/SIZE,wz=2*EXTENT-(float(y)+.5f)*3*EXTENT/SIZE;const bool outsideX=wx<0||wx>=EXTENT,outsideZ=wz<0||wz>=EXTENT;Pixel(output,x,y,outsideX||outsideZ?ConstantShade(float(outsideX),float(outsideZ)):BACKGROUND,"eight original neighbors, mirrored lighting and untouched real-map bounds");}
}
void CurvatureFogAndDeformation()
{
	MapEdgeGPU gpu;auto frame=Frame();auto p=frame.mapEdge->draw->parameters;p.curvature=p.fog=true;Change(frame,p);
	for(float height:{0.f,7.f}) {
		auto heights=std::make_shared<WorldTerrainHeights>(*frame.terrain.heights);std::fill(heights->corners.begin(),heights->corners.end(),height);frame.terrain.heights=heights;
		const auto output=Render(gpu,frame);const auto base=ConstantShade(1,0);
		for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x){const float sx=EXTENT-(float(x)+.5f)*EXTENT/SIZE,sz=EXTENT-(float(y)+.5f)*EXTENT/SIZE;
			const float gx=std::floor(sx/32)*32,gz=std::floor(sz/32)*32,tx=(sx-gx)/32,tz=(sz-gz)/32;
			std::array<std::array<float,2>,3> corners;std::array<float,3> weights;
			if(tz>=tx){corners={{{gx,gz},{gx,gz+32},{gx+32,gz+32}}};weights={1-tz,tz-tx,tx};}else{corners={{{gx,gz},{gx+32,gz+32},{gx+32,gz}}};weights={1-tx,tz,tx-tz};}
			float alpha=0,fog=0,worldHeight=0;for(size_t i=0;i<3;++i){const float wx=-corners[i][0],wz=corners[i][1],wy=height-wx*wx/(150*150);worldHeight+=weights[i]*wy;alpha+=weights[i]*std::clamp(1+6*(.18f-wx*wx/(EXTENT*EXTENT)),0.f,1.f);const float distance=std::sqrt(wx*wx+wz*wz+(wy-100)*(wy-100));fog+=weights[i]*std::clamp((250-distance)/200,0.f,1.f);}
			Color expected{};for(size_t c=0;c<3;++c)expected[c]=(frame.mapEdge->fogColor[c]*(1-fog)+base[c]*fog)*alpha+BACKGROUND[c]*(1-alpha);expected[3]=alpha*alpha+BACKGROUND[3]*(1-alpha);
			Pixel(output,x,y,expected,"original curvature/interpolated alpha-fog and real height deformation");const auto depth=output.depth[size_t(SIZE-1-y)*SIZE+x];Check(std::abs(depth-(99-worldHeight)/999)<.00001f,"original curved/deformed depth differs");
		}
	}
}
void FourSeamSides()
{
	MapEdgeGPU gpu;
	for(unsigned side=0;side<4;++side){auto frame=Frame();
		if(side==0){frame.viewCamera.view={0,0,1,0,0,1,0,0,-1,0,0,0,0,0,-64,1};frame.viewCamera.projection=Ortho(-EXTENT,0,-128,0);}
		if(side==1){frame.viewCamera.view={0,0,-1,0,0,1,0,0,1,0,0,0,0,0,64,1};frame.viewCamera.projection=Ortho(0,EXTENT,-128,0);}
		if(side==2){frame.viewCamera.view={1,0,0,0,0,1,0,0,0,0,1,0,0,0,-64,1};frame.viewCamera.projection=Ortho(0,EXTENT,-128,0);}
		if(side==3){frame.viewCamera.view={-1,0,0,0,0,1,0,0,0,0,-1,0,0,0,64,1};frame.viewCamera.projection=Ortho(-EXTENT,0,-128,0);}
		const auto output=Render(gpu,frame);for(uint32_t y=4;y<SIZE-4;++y)for(uint32_t x=20;x<44;++x){Pixel(output,x,y,ConstantShade(0,0),"original GS-equivalent inward curtain side"+std::to_string(side));Check(std::abs(output.depth[size_t(SIZE-1-y)*SIZE+x]-63.f/999)<.00001f,"original seam curtain depth/height mismatch");}
	}
}
std::array<float,3> BC1Normal(const WorldTextureMip& mip,uint32_t x,uint32_t y)
{
	const auto* block=mip.bytes.data()+size_t(y/4)*mip.bytesPerRow+size_t(x/4)*8;const uint16_t a=uint16_t(uint16_t(block[0])|uint16_t(block[1])<<8),b=uint16_t(uint16_t(block[2])|uint16_t(block[3])<<8);
	const auto rgb=[](uint16_t v){return std::array<float,3>{float(v>>11)/31,float((v>>5)&63)/63,float(v&31)/31};};auto c0=rgb(a),c1=rgb(b);uint32_t indices=0;std::memcpy(&indices,block+4,4);const auto code=(indices>>((y%4*4+x%4)*2))&3;std::array<float,3> c{};
	if(code==0)c=c0;else if(code==1)c=c1;else if(a>b){for(size_t k=0;k<3;++k)c[k]=code==2?(2*c0[k]+c1[k])/3:(c0[k]+2*c1[k])/3;}else if(code==2){for(size_t k=0;k<3;++k)c[k]=(c0[k]+c1[k])*.5f;}
	return {c[0]*2-1,c[2]*2-1,c[1]*2-1};
}
void ActualJadeMipUpload(const char* path)
{
	std::ifstream file(path,std::ios::binary);Check(bool(file),"actual Jade normal source missing");std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});auto layout=ui::ValidateNativeUIDDS(bytes);
	Check(layout.format==ui::NativeDDSFormat::BC1&&layout.mips.size()==15&&layout.mips[0].width==16384,"actual Jade original 16K/15mip contract changed");
	auto normal=std::make_shared<WorldTexture>();normal->format=WorldTextureFormat::BC1RGBA;normal->sourcePath="Actual Jade raw BC1 mip upload fixture (orientation is tested separately by engine loader)";
	for(const auto& mip:layout.mips)normal->mips.push_back({mip.width,mip.height,mip.bytesPerRow,{bytes.begin()+static_cast<ptrdiff_t>(mip.offset),bytes.begin()+static_cast<ptrdiff_t>(mip.offset+mip.byteCount)}});
	auto frame=Frame();auto resources=std::make_shared<WorldMapEdgeResources>(*frame.mapEdge->resources);resources->normal=normal;frame.mapEdge->resources=resources;
	MapEdgeGPU gpu;const auto output=Render(gpu,frame);const auto& mip=normal->mips[8];Check(mip.width==SIZE&&mip.height==SIZE,"actual mip8 oracle alignment changed");
	for(uint32_t y=2;y<SIZE-2;++y)for(uint32_t x=2;x<SIZE-2;++x)Pixel(output,x,y,Shade(BC1Normal(mip,SIZE-1-x,SIZE-1-y),1,0),"actual unchanged Jade 16K/full-mip upload and independent BC1 mip8 texels");
}
}
int main(int argc,char** argv)
{
	@autoreleasepool{try{MirrorsAndLifecycle();CurvatureFogAndDeformation();FourSeamSides();if(argc>1)ActualJadeMipUpload(argv[1]);
		std::cout<<"{\"status\":\"passed\",\"renders\":"<<renders<<",\"checks\":"<<checks<<",\"rgba8Tolerance\":"<<TOLERANCE<<",\"device\":\""<<device<<"\",\"queueClass\":\""<<queue<<"\",\"commandBufferClass\":\""<<commandClass<<"\",\"renderEncoderClass\":\""<<encoderClass<<"\",\"simulation\":false,\"actualOriginalWidgetAcceptance\":false}\n";return 0;
	}catch(const std::exception& error){std::cerr<<"Map-edge GPU fixture: "<<error.what()<<'\n';return 1;}}
}
