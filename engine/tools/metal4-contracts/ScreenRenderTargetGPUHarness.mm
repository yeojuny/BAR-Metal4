/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Metal4/Metal4ScreenNative.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
using namespace rendering;
using namespace rendering::metal4;
constexpr uint32_t SIZE=32;
constexpr int TOLERANCE=2;
uint32_t checks=0,renders=0;
std::string device,queue,commandClass,encoderClass;
void Check(bool value,const std::string& label){++checks;if(!value)throw std::runtime_error(label);}
template<class F>void Throws(F action,const std::string& label){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}Check(rejected,label);}
int Byte(float value){return int(std::lround(std::clamp(value,0.0f,1.0f)*255));}
float Q(float value){return float(Byte(value))/255;}
ScreenColor Quantize(ScreenColor color){for(float& c:color)c=Q(c);return color;}
ScreenBlendState Disabled(){ScreenBlendState s;s.enabled=false;return s;}
ScreenBlendState PanelBlend(){ScreenBlendState s;s.sourceAlpha=ScreenBlendFactor::OneMinusDestinationAlpha;s.destinationAlpha=ScreenBlendFactor::One;return s;}
ScreenBlendState CompositeBlend(){ScreenBlendState s;s.sourceColor=s.sourceAlpha=ScreenBlendFactor::One;return s;}
ScreenColor PanelOver(ScreenColor source,ScreenColor destination){ScreenColor out{};for(size_t c=0;c<3;++c)out[c]=source[c]*source[3]+destination[c]*(1-source[3]);out[3]=source[3]*(1-destination[3])+destination[3];return Quantize(out);}
ScreenColor Composite(ScreenColor source,ScreenColor destination){ScreenColor out{};for(size_t c=0;c<4;++c)out[c]=source[c]+destination[c]*(1-source[3]);return Quantize(out);}
void Quad(ScreenDrawList& list,float left,float bottom,float right,float top,ScreenColor color)
{
	list.Color(color);list.BeginQuads();
	list.TexCoord(0,0);list.Vertex(left,bottom);list.TexCoord(1,0);list.Vertex(right,bottom);
	list.TexCoord(1,1);list.Vertex(right,top);list.TexCoord(0,1);list.Vertex(left,top);list.End();
}
void Sample(ScreenDrawList& list,std::shared_ptr<const ScreenTexture> texture,float left=0,float right=SIZE,ScreenBlendState blend=Disabled())
{
	list.SetBlend(blend);list.SetTexture(std::move(texture),ScreenMaterial::RGBA,ScreenUVMode::Normalized,
		{ScreenFilter::Nearest,ScreenWrap::ClampEdge,ScreenWrap::ClampEdge});Quad(list,left,0,right,SIZE,{1,1,1,1});
}
std::shared_ptr<ScreenTexture> Version(const std::shared_ptr<const ScreenRenderTargetIdentity>& identity,uint64_t generation)
{
	auto t=std::make_shared<ScreenTexture>();t->sourceName="Original R2tHelper semantic fixture";t->generation=generation;t->renderTarget=identity;
	t->width=identity->width;t->height=identity->height;t->flipV=true;return t;
}
ScreenRenderTargetPass Pass(std::shared_ptr<const ScreenTexture> texture,std::shared_ptr<const ScreenDrawList> list,
	ScreenTargetLoad load=ScreenTargetLoad::Clear,std::shared_ptr<const ScreenTexture> previous={})
{
	ScreenRenderTargetPass p;p.coordinateWidth=texture->width;p.coordinateHeight=texture->height;p.destination=std::move(texture);
	p.drawList=std::move(list);p.load=load;p.previous=std::move(previous);return p;
}
std::vector<uint8_t> Render(ScreenGPU& gpu,const ScreenDrawList& root,std::span<const ScreenRenderTargetPass> passes,ScreenColor clear={0,0,0,0},bool poisonedDepth=false)
{
	RuntimeLease lease;double shaderMs=0,readbackMs=0;
	MTLTextureDescriptor* d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:SIZE height:SIZE mipmapped:NO];
	d.storageMode=MTLStorageModeShared;d.usage=MTLTextureUsageRenderTarget;
	id<MTLTexture> image=[lease.Device() newTextureWithDescriptor:d];Require(image!=nil,"Cannot allocate RTT oracle output");lease.AddAllocation(image);
	id<MTLTexture> depth=nil;id<MTLBuffer> depthBytes=nil;id<MTLDepthStencilState> priorDepth=nil;
	if(poisonedDepth) {
		auto depthDescriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:SIZE height:SIZE mipmapped:NO];
		depthDescriptor.storageMode=MTLStorageModePrivate;depthDescriptor.usage=MTLTextureUsageRenderTarget;
		depth=[lease.Device() newTextureWithDescriptor:depthDescriptor];Require(depth!=nil,"Cannot allocate depth-state oracle attachment");lease.AddAllocation(depth);
		depthBytes=[lease.Device() newBufferWithLength:SIZE*256 options:MTLResourceStorageModeShared];Require(depthBytes!=nil,"Cannot allocate depth-state oracle readback");lease.AddAllocation(depthBytes);
		auto depthState=[MTLDepthStencilDescriptor new];depthState.depthCompareFunction=MTLCompareFunctionLessEqual;depthState.depthWriteEnabled=YES;
		priorDepth=[lease.Device() newDepthStencilStateWithDescriptor:depthState];Require(priorDepth!=nil,"Cannot allocate poisoned preceding world depth state");lease.Retain(priorDepth);
	}
	ScreenFrameView frame{&root,SIZE,SIZE,std::nullopt,passes};
	auto screen=gpu.Prepare(lease,frame,SIZE,SIZE,SIZE,SIZE,MTLPixelFormatRGBA8Unorm,true,shaderMs);
	lease.CommitResources();const auto prep=Clock::now();auto command=lease.Begin("Original cached-panel bounded GPU oracle");screen->EncodeDependencies(command);
	MTL4RenderPassDescriptor* pass=[MTL4RenderPassDescriptor new];pass.colorAttachments[0].texture=image;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
	pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(clear[0],clear[1],clear[2],clear[3]);pass.renderTargetWidth=pass.renderTargetHeight=SIZE;
	if(depth){pass.depthAttachment.texture=depth;pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.storeAction=MTLStoreActionStore;pass.depthAttachment.clearDepth=0.25;}
	id<MTL4RenderCommandEncoder> encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"Cannot create RTT oracle final encoder");
	if(depth){[encoder setDepthStencilState:priorDepth];[encoder setDepthClipMode:MTLDepthClipModeClamp];}
	encoderClass=NSStringFromClass([encoder class]).UTF8String;screen->Encode(encoder);[encoder endEncoding];
	if(depth) {
		id<MTL4ComputeCommandEncoder> copy=[command computeCommandEncoder];Require(copy!=nil,"Cannot encode depth-state oracle readback");
		[copy barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		[copy copyFromTexture:depth sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(SIZE,SIZE,1)
			toBuffer:depthBytes destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:0];[copy endEncoding];
	}
	const auto metrics=lease.SubmitAndWait(prep,renders);screen->DidComplete();Check(!metrics.cpuSimulationMs,"analytic fixture has no simulation");
	if(depth)for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x){float value=0;std::memcpy(&value,static_cast<const uint8_t*>(depthBytes.contents)+y*256+x*sizeof(float),sizeof(value));Check(value==0.25f,"screen draws wrote the preceding world depth attachment");}
	device=lease.Capabilities().deviceName;queue=lease.QueueClass();commandClass=lease.CommandBufferClass();++renders;
	return lease.Readback(image,SIZE,SIZE,readbackMs);
}
void Pixel(const std::vector<uint8_t>& image,uint32_t x,uint32_t y,ScreenColor expected,const std::string& label)
{
	Check(image.size()==size_t(SIZE)*SIZE*4,label+" exact output extent");const size_t offset=(size_t(SIZE-1-y)*SIZE+x)*4;
	for(size_t c=0;c<4;++c)Check(std::abs(int(image[offset+c])-Byte(expected[c]))<=TOLERANCE,
		label+" ("+std::to_string(x)+","+std::to_string(y)+") channel"+std::to_string(c)+" expected"+std::to_string(Byte(expected[c]))+" actual"+std::to_string(image[offset+c]));
}
void OrientationAndNestedTargets()
{
	ScreenGPU gpu;
	const auto childIdentity=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{1,8,8});
	const auto parentIdentity=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{2,16,16});
	auto child=Version(childIdentity,1),parent=Version(parentIdentity,1);
	auto childList=std::make_shared<ScreenDrawList>();childList->SetBlend(Disabled());
	Quad(*childList,0,0,4,4,{1,0,0,1});Quad(*childList,4,0,8,4,{0,1,0,1});Quad(*childList,0,4,4,8,{0,0,1,1});Quad(*childList,4,4,8,8,{1,1,0,1});
	auto parentList=std::make_shared<ScreenDrawList>();parentList->SetBlend(Disabled());parentList->SetTexture(child,ScreenMaterial::RGBA,ScreenUVMode::Normalized,{ScreenFilter::Nearest});
	Quad(*parentList,0,0,16,16,{1,1,1,1});
	// Deliberately put the consumer first: actual graph ordering must prevail.
	std::vector<ScreenRenderTargetPass> passes{Pass(parent,parentList),Pass(child,childList)};
	ScreenDrawList root;Sample(root,parent);const auto output=Render(gpu,root,passes);
	for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x){const ScreenColor color=y<16?(x<16?ScreenColor{1,0,0,1}:ScreenColor{0,1,0,1}):(x<16?ScreenColor{0,0,1,1}:ScreenColor{1,1,0,1});Pixel(output,x,y,color,"nested GL FBO orientation");}
	const auto cached=Render(gpu,root,{});Check(cached==output,"retired nested target retained without per-frame rerender");
}
void PartialUpdateAndImmutableVersions()
{
	ScreenGPU gpu;const auto identity=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{3,8,8});
	auto first=Version(identity,1),second=Version(identity,2);
	auto initial=std::make_shared<ScreenDrawList>();initial->SetBlend(Disabled());Quad(*initial,0,0,8,8,{0.1f,0.2f,0.3f,0.4f});
	std::vector<ScreenRenderTargetPass> firstPass{Pass(first,initial)};ScreenDrawList rootFirst;Sample(rootFirst,first);const auto before=Render(gpu,rootFirst,firstPass);
	auto update=std::make_shared<ScreenDrawList>();
	// Original R2tHelper: scissored zero clear, then custom separate blending.
	update->ClearColorAttachment({0,0,0,0},8,8,ScreenScissor{2,1,4,3});update->SetBlend(PanelBlend());update->SetScissor(ScreenScissor{2,1,4,3});
	Quad(*update,0,0,8,8,{0.8f,0.4f,0.2f,0.5f});
	std::vector<ScreenRenderTargetPass> updatePass{Pass(second,update,ScreenTargetLoad::Load,first)};
	ScreenDrawList comparison;Sample(comparison,first,0,16);Sample(comparison,second,16,32);const auto output=Render(gpu,comparison,updatePass);
	for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x){const uint32_t tx=x<16?x/2:(x-16)/2,ty=y/4;const bool changed=x>=16&&tx>=2&&tx<6&&ty>=1&&ty<4;
		Pixel(output,x,y,changed?PanelOver({0.8f,0.4f,0.2f,0.5f},{0,0,0,0}):Quantize({0.1f,0.2f,0.3f,0.4f}),"copy-on-write partial panel update");}
	const auto stillOld=Render(gpu,rootFirst,{});Check(stillOld==before,"old generation pixels remain immutable after later update");
	Throws([&]{Render(gpu,comparison,updatePass);},"successfully retired generation cannot be overwritten");
}
void OriginalPanelBlendAndClear()
{
	ScreenGPU gpu;const auto identity=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{4,8,8});auto target=Version(identity,1);
	auto list=std::make_shared<ScreenDrawList>();list->SetAlphaTest({true,ScreenAlphaCompare::Never,1});list->Color({1,1,1,1});
	list->ClearColorAttachment({0,0,0,0},8,8);list->SetAlphaTest({});list->SetBlend(PanelBlend());
	Quad(*list,0,0,8,8,{0.8f,0.4f,0.2f,0.5f});Quad(*list,0,0,8,8,{0.2f,0.6f,1.0f,0.4f});
	std::vector<ScreenRenderTargetPass> passes{Pass(target,list,ScreenTargetLoad::DontCare)};ScreenDrawList root;Sample(root,target,0,32,CompositeBlend());
	const ScreenColor background{0.1f,0.2f,0.3f,0.6f};const auto output=Render(gpu,root,passes,background);
	const auto cached=PanelOver({0.2f,0.6f,1.0f,0.4f},PanelOver({0.8f,0.4f,0.2f,0.5f},{0,0,0,0}));
	const auto expected=Composite(cached,Quantize(background));
	for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x)Pixel(output,x,y,expected,"exact R2tHelper alpha/color then premultiplied composition");
}
void EmptyClearAndInvalidResources()
{
	ScreenGPU gpu;const auto identity=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{5,8,8});auto version=Version(identity,1);
	auto empty=std::make_shared<ScreenDrawList>();auto pass=Pass(version,empty);pass.clearColor={0.25f,0.5f,0.75f,0.25f};
	std::array<ScreenRenderTargetPass,1> passes{pass};ScreenDrawList root;Sample(root,version);const auto output=Render(gpu,root,passes);
	for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x)Pixel(output,x,y,pass.clearColor,"empty target attachment clear/store");
	auto missing=Version(identity,7);ScreenDrawList invalid;Sample(invalid,missing);Throws([&]{Render(gpu,invalid,{});},"unproduced target fails before Begin");
	passes[0].destination=missing;passes[0].load=ScreenTargetLoad::Load;passes[0].previous=Version(identity,6);Throws([&]{Render(gpu,invalid,passes);},"unretired previous generation cannot silently provide data");
	ScreenDrawList clean;clean.ClearColorAttachment({1,0,1,1},SIZE,SIZE);const auto recovered=Render(gpu,clean,{});Pixel(recovered,3,5,{1,0,1,1},"setup rejection leaves shared runtime safe for later work");
}
void AllocationOnlyInitialGeneration()
{
	ScreenGPU gpu;
	const auto identity=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{6,8,8});
	auto initial=Version(identity,0);
	std::weak_ptr<const ScreenTexture> initialOwner=initial;
	ScreenDrawList root;Sample(root,initial);
	// Exact original Info/Grid warmup path: CreateTexture storage is sampled
	// before RenderToTexture. Its pixels have no defined oracle, so neither
	// assert transparent black nor substitute a synthetic image/clear.
	const auto first=Render(gpu,root,{});
	Check(first.size()==size_t(SIZE)*SIZE*4,"allocation-only storage is legally sampled before its first producer");
	const auto reused=Render(gpu,root,{});
	Check(reused.size()==first.size(),"allocation-only owned generation remains valid across frames");
	Check(!initialOwner.expired(),"initial storage stays owned by the original binding and queued sample");
	auto produced=Version(identity,1);
	auto write=std::make_shared<ScreenDrawList>();
	write->ClearColorAttachment({0.2f,0.4f,0.6f,0.8f},8,8);
	std::array<ScreenRenderTargetPass,1> passes{Pass(produced,write,ScreenTargetLoad::Load,initial)};
	ScreenDrawList result;Sample(result,produced);
	const auto defined=Render(gpu,result,passes);
	for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x)
		Pixel(defined,x,y,{0.2f,0.4f,0.6f,0.8f},"first producer Load from allocated generation0 then explicit original clear");
	passes[0].previous.reset();
	root=ScreenDrawList{};initial.reset();
	Check(initialOwner.expired(),"native allocation cache does not retain a deleted CPU generation owner");
	const auto retired=Render(gpu,result,{});
	Check(retired==defined,"defined retired generation survives release of allocation-only predecessor");
	auto updated=Version(identity,2);
	auto partial=std::make_shared<ScreenDrawList>();partial->ClearColorAttachment({1,0,0,1},8,8,ScreenScissor{2,1,4,3});
	passes[0]=Pass(updated,partial,ScreenTargetLoad::Load,produced);
	ScreenDrawList latest;Sample(latest,updated);const auto copied=Render(gpu,latest,passes);
	for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x)
		Pixel(copied,x,y,(x/4>=2&&x/4<6&&y/4>=1&&y/4<4)?ScreenColor{1,0,0,1}:ScreenColor{0.2f,0.4f,0.6f,0.8f},
			"later Load keeps actual initialized producer pixels");
	auto missing=Version(identity,9);ScreenDrawList invalid;Sample(invalid,missing);
	bool diagnosed=false;
	try {Render(gpu,invalid,{});} catch(const std::exception& error) {
		const std::string text=error.what();diagnosed=text.find("resource=6/generation=9")!=std::string::npos &&
			text.find("known target count=")!=std::string::npos && text.find("final screen draws")!=std::string::npos;
	}
	Check(diagnosed,"missing generation>0 remains an explicit identity-bearing producer failure");
}
ScreenColor GridTexel(int x,int y)
{
	return {float(32+x*48)/255,float(16+y*64)/255,float(8+(x+y)*16)/255,float(128+x*20+y*10)/255};
}
ScreenColor ClampOracle(float u,float v,const ScreenSampler& sampler)
{
	if(sampler.wrapS==ScreenWrap::LegacyClamp)u=std::clamp(u,0.f,1.f);
	if(sampler.wrapT==ScreenWrap::LegacyClamp)v=std::clamp(v,0.f,1.f);
	const auto index=[&](int i,ScreenWrap wrap) {
		if(wrap==ScreenWrap::Repeat)return (i%4+4)%4;
		if(wrap==ScreenWrap::ClampEdge || (wrap==ScreenWrap::LegacyClamp && sampler.filter==ScreenFilter::Nearest))return std::clamp(i,0,3);
		return i;
	};
	const auto fetch=[&](int x,int y) {
		x=index(x,sampler.wrapS);y=index(y,sampler.wrapT);
		return x<0||y<0||x>=4||y>=4?ScreenColor{0,0,0,0}:GridTexel(x,y);
	};
	if(sampler.filter==ScreenFilter::Nearest)return fetch(int(std::floor(u*4)),int(std::floor(v*4)));
	const float x=u*4-.5f,y=v*4-.5f;const int ix=int(std::floor(x)),iy=int(std::floor(y));
	const float a=x-float(ix),b=y-float(iy);
	const auto c00=fetch(ix,iy),c10=fetch(ix+1,iy),c01=fetch(ix,iy+1),c11=fetch(ix+1,iy+1);
	ScreenColor out{};for(size_t c=0;c<4;++c)out[c]=(1-b)*((1-a)*c00[c]+a*c10[c])+b*((1-a)*c01[c]+a*c11[c]);return out;
}
std::shared_ptr<ScreenTexture> GridImage()
{
	auto image=std::make_shared<ScreenTexture>();image->width=image->height=4;image->bytesPerRow=16;
	image->sourceName="Original Atlas GL_CLAMP independent RGBA texels";
	for(int y=0;y<4;++y)for(int x=0;x<4;++x)for(float c:GridTexel(x,y))image->bytes.push_back(uint8_t(Byte(c)));
	return image;
}
void ConstantCell(ScreenDrawList& list,float x,float y,float u,float v)
{
	list.Color({1,1,1,1});list.TexCoord(u,v);list.BeginQuads();
	list.Vertex(x,y);list.Vertex(x+1,y);list.Vertex(x+1,y+1);list.Vertex(x,y+1);list.End();
}
void OriginalAtlasClamp()
{
	ScreenGPU gpu;
	constexpr std::array<float,11> coords{-2,-.125f,0,.0625f,.125f,.4f,.875f,.9375f,1,1.125f,3};
	const std::array<ScreenSampler,7> samplers{{
		{ScreenFilter::Linear,ScreenWrap::LegacyClamp,ScreenWrap::LegacyClamp},
		{ScreenFilter::Linear,ScreenWrap::LegacyClamp,ScreenWrap::ClampEdge},
		{ScreenFilter::Linear,ScreenWrap::LegacyClamp,ScreenWrap::TransparentBorder},
		{ScreenFilter::Linear,ScreenWrap::Repeat,ScreenWrap::LegacyClamp},
		{ScreenFilter::Nearest,ScreenWrap::LegacyClamp,ScreenWrap::LegacyClamp},
		{ScreenFilter::Linear,ScreenWrap::ClampEdge,ScreenWrap::ClampEdge},
		{ScreenFilter::Linear,ScreenWrap::TransparentBorder,ScreenWrap::TransparentBorder}
	}};
	for(size_t mode=0;mode<samplers.size();++mode) {
		auto image=GridImage();ScreenDrawList root;root.SetBlend(Disabled());root.SetTexture(image,ScreenMaterial::RGBA,ScreenUVMode::Normalized,samplers[mode]);
		for(size_t y=0;y<coords.size();++y)for(size_t x=0;x<coords.size();++x)ConstantCell(root,float(x),float(y),coords[x],coords[y]);
		const auto result=Render(gpu,root,{});
		for(uint32_t y=0;y<coords.size();++y)for(uint32_t x=0;x<coords.size();++x)
			Pixel(result,x,y,ClampOracle(coords[x],coords[y],samplers[mode]),"GL_CLAMP independent per-axis/edge/border oracle mode"+std::to_string(mode));
	}
	Check(ClampOracle(-2,.4f,samplers[0])!=ClampOracle(-2,.4f,samplers[5]),"ClampToEdge alias is an independently distinguishable negative control");
	Check(ClampOracle(-2,.4f,samplers[0])!=ClampOracle(-2,.4f,samplers[6]),"full border alias is an independently distinguishable negative control");
	// Owned CPU-image flip plus texel coordinates must precede axis clamping.
	auto flipped=GridImage();flipped->flipV=true;ScreenDrawList texel;texel.SetBlend(Disabled());
	texel.SetTexture(flipped,ScreenMaterial::RGBA,ScreenUVMode::Texel,samplers[0]);
	for(size_t y=0;y<coords.size();++y)for(size_t x=0;x<coords.size();++x)ConstantCell(texel,float(x),float(y),coords[x]*4,coords[y]*4);
	const auto flipResult=Render(gpu,texel,{});
	for(uint32_t y=0;y<coords.size();++y)for(uint32_t x=0;x<coords.size();++x)
		Pixel(flipResult,x,y,ClampOracle(coords[x],1-coords[y],samplers[0]),"texel normalization and flip before clamp");
	// The actual original atlas is a GL_RGBA FBO with Linear/Linear/GL_CLAMP,
	// border width0 and no mip filter. Its defined producer must compose in
	// the same frame with the GL framebuffer orientation and RGBA storage.
	const auto identity=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{11,4,4});auto atlas=Version(identity,1);
	auto painting=std::make_shared<ScreenDrawList>();painting->SetBlend(Disabled());
	for(int y=0;y<4;++y)for(int x=0;x<4;++x)Quad(*painting,float(x),float(y),float(x+1),float(y+1),GridTexel(x,y));
	std::array<ScreenRenderTargetPass,1> passes{Pass(atlas,painting)};ScreenDrawList composite;composite.SetBlend(Disabled());
	composite.SetTexture(atlas,ScreenMaterial::RGBA,ScreenUVMode::Normalized,samplers[0]);
	for(size_t y=0;y<coords.size();++y)for(size_t x=0;x<coords.size();++x)ConstantCell(composite,float(x),float(y),coords[x],coords[y]);
	const auto atlasResult=Render(gpu,composite,passes);
	for(uint32_t y=0;y<coords.size();++y)for(uint32_t x=0;x<coords.size();++x)
		Pixel(atlasResult,x,y,ClampOracle(coords[x],coords[y],samplers[0]),"original same-frame RGBA atlas storage/orientation/border");
	// A large original footprint still uses base0 when mipFilter=None. Owned
	// mip payloads remain distinct; this does not claim mipfiltered GL_CLAMP.
	auto chain=GridImage();chain->mips.push_back({4,4,16,chain->bytes});chain->bytes.clear();chain->bytesPerRow=0;
	chain->mips.push_back({2,2,8,std::vector<uint8_t>(16,255)});chain->mips.push_back({1,1,4,{255,0,255,255}});
	ScreenDrawList minified;minified.SetBlend(Disabled());minified.SetTexture(chain,ScreenMaterial::RGBA,ScreenUVMode::Normalized,samplers[0]);
	minified.Color({1,1,1,1});minified.BeginQuads();minified.TexCoord(-16,-16);minified.Vertex(0,0);minified.TexCoord(16,-16);minified.Vertex(SIZE,0);
	minified.TexCoord(16,16);minified.Vertex(SIZE,SIZE);minified.TexCoord(-16,16);minified.Vertex(0,SIZE);minified.End();
	const auto minResult=Render(gpu,minified,{});
	for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x)
		Pixel(minResult,x,y,ClampOracle(float(x)-15.5f,float(y)-15.5f,samplers[0]),"no-mip original minification retains base level");
	for(auto mip:{ScreenMipFilter::Nearest,ScreenMipFilter::Linear}) {
		auto unsupported=samplers[0];unsupported.mipFilter=mip;Throws([&]{ValidateScreenSampler(unsupported);},"mipfiltered legacy clamp is explicitly unsupported");
	}
	auto mixed=samplers[0];mixed.magnificationFilter=ScreenFilter::Nearest;
	Throws([&]{ValidateScreenSampler(mixed);},"mixed legacy clamp filters cannot silently change border semantics");
}
void OrthographicClipDepth()
{
	ScreenGPU gpu;const ScreenColor background{.1f,.2f,.3f,.4f};
	const std::array<std::array<float,3>,6> depths{{{-1,-1,-1},{1,1,1},{-1.5f,-1.5f,-1.5f},{1.5f,1.5f,1.5f},{-2,0,0},{0,2,0}}};
	const std::array<ScreenColor,3> colors{{{1,0,0,.25f},{0,1,0,.5f},{0,0,1,1}}};
	const ScreenSampler sampler{ScreenFilter::Linear,ScreenWrap::ClampEdge,ScreenWrap::ClampEdge};
	for(size_t mode=0;mode<depths.size();++mode) {
		ScreenDrawList list;list.SetBlend(Disabled());list.SetTexture(GridImage(),ScreenMaterial::RGBA,ScreenUVMode::Normalized,sampler);
		list.Begin(ScreenPrimitive::Triangles);
		for(size_t v=0;v<3;++v){list.Color(colors[v]);list.TexCoord(v==1?1.f:0.f,v==2?1.f:0.f);list.Vertex(v==1?float(SIZE):0.f,v==2?float(SIZE):0.f,depths[mode][v]);}list.End();
		const auto image=Render(gpu,list,{},background,true);
		for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x) {
			const float b=(float(x)+.5f)/float(SIZE),c=(float(y)+.5f)/float(SIZE),a=1-b-c;
			const float z=a*depths[mode][0]+b*depths[mode][1]+c*depths[mode][2];
			// Avoid sample centers exactly on triangle/clip edges; coverage rules
			// at those edges are separate from this independent interior oracle.
			if(std::abs(a)<.0001f || (mode>=4 && (std::abs(z+1)<.0001f || std::abs(z-1)<.0001f)))continue;
			ScreenColor expected=background;
			if(a>0 && z>=-1 && z<=1){const auto texel=ClampOracle(b,c,sampler);for(size_t channel=0;channel<4;++channel)expected[channel]=(a*colors[0][channel]+b*colors[1][channel]+c*colors[2][channel])*texel[channel];}
			Pixel(image,x,y,expected,"orthographic near/far clipping and UV/color interpolation mode"+std::to_string(mode));
		}
	}
	// Depth comparison and writes must remain disabled after world rendering.
	ScreenDrawList ordered;ordered.SetBlend(Disabled());Quad(ordered,0,0,SIZE,SIZE,{1,0,0,1});
	ordered.Color({0,1,0,1});ordered.BeginQuads();ordered.Vertex(0,0,1);ordered.Vertex(SIZE,0,1);ordered.Vertex(SIZE,SIZE,1);ordered.Vertex(0,SIZE,1);ordered.End();
	const auto orderedImage=Render(gpu,ordered,{},background,true);
	for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x)Pixel(orderedImage,x,y,{0,1,0,1},"far-plane later screen draw ignores previous world depth and retains source order");

	// Actual AdvPlayersList's identity RTT + Ortho(...,0,1000), nested
	// target restore, then the original screen-space composition orientation.
	using namespace rendering::ui;
	NativeUIRecorder recorder;recorder.BeginFrame(SIZE,SIZE);recorder.BeginScreenStage();
	auto parent=Version(std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{1201,SIZE,SIZE}),1);
	auto child=Version(std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{1202,16,16}),1);
	recorder.BeginTarget(parent);recorder.ClearTarget(ScreenColor{0,0,0,0});recorder.PushMatrix();
	recorder.LoadMatrix(IdentityMatrix());recorder.MultMatrix(OrthoMatrix(100,132,200,232,0,1000));recorder.SetBlend(Disabled());
	const auto parentMatrix=recorder.GetMatrix(NativeUIMatrixMode::ModelView);
	recorder.Begin(ScreenPrimitive::Quads);recorder.Color({1,0,0,1});recorder.Vertex(100,200,0);recorder.Vertex(132,200,0);recorder.Vertex(132,216,0);recorder.Vertex(100,216,0);recorder.End();
	recorder.BeginTarget(child);recorder.ClearTarget(ScreenColor{1,1,0,1});recorder.EndTarget();
	Check(recorder.GetMatrix(NativeUIMatrixMode::ModelView)==parentMatrix,"actual nested RTT restored original background Ortho");
	recorder.Begin(ScreenPrimitive::Quads);recorder.Color({0,0,1,1});recorder.Vertex(100,216,0);recorder.Vertex(132,216,0);recorder.Vertex(132,232,0);recorder.Vertex(100,232,0);recorder.End();
	auto coverage=std::make_shared<ScreenTexture>();coverage->sourceName="near-plane font coverage oracle";coverage->format=ScreenTextureFormat::R8Unorm;
	coverage->width=coverage->height=1;coverage->bytesPerRow=1;coverage->bytes={255};
	recorder.AppendFontTriangles({{108,220,0,{0,1,0,1}},{116,220,0,{0,1,0,1}},{116,228,0,{0,1,0,1}},{108,228,0,{0,1,0,1}}},
		{{.5f,.5f},{.5f,.5f},{.5f,.5f},{.5f,.5f}},{0,1,2,0,2,3},coverage,false);
	recorder.PopMatrix();recorder.EndTarget();recorder.SetBlend(Disabled());recorder.SetTexture(parent,{ScreenFilter::Nearest});recorder.Color({1,1,1,1});
	recorder.Begin(ScreenPrimitive::Quads);recorder.TexCoord(0,0);recorder.Vertex(0,0);recorder.TexCoord(1,0);recorder.Vertex(SIZE,0);recorder.TexCoord(1,1);recorder.Vertex(SIZE,SIZE);recorder.TexCoord(0,1);recorder.Vertex(0,SIZE);recorder.End();
	const auto frame=recorder.EndFrame();Check(frame.targetPasses.size()==2,"actual orthographic RTT producer count");
	Check(frame.targetPasses[1].drawList->Vertices()[4].z==-1,"actual AdvPlayersList near-plane z0 was not preserved");
	const auto actual=Render(gpu,*frame.drawList,frame.targetPasses,background,true);
	for(uint32_t y=0;y<SIZE;++y)for(uint32_t x=0;x<SIZE;++x)Pixel(actual,x,y,(x>=8&&x<16&&y>=20&&y<28)?ScreenColor{0,1,0,1}:
		(y<16?ScreenColor{1,0,0,1}:ScreenColor{0,0,1,1}),"actual original Ortho z0 near-plane RTT/font and composition");
	recorder.RetireCompletedFrame();
}
}
int main()
{
	@autoreleasepool {
		try {OrientationAndNestedTargets();PartialUpdateAndImmutableVersions();OriginalPanelBlendAndClear();EmptyClearAndInvalidResources();AllocationOnlyInitialGeneration();OriginalAtlasClamp();OrthographicClipDepth();
			std::cout<<"{\"status\":\"passed\",\"renders\":"<<renders<<",\"checks\":"<<checks<<",\"rgba8Tolerance\":"<<TOLERANCE
				<<",\"device\":\""<<device<<"\",\"queueClass\":\""<<queue<<"\",\"commandBufferClass\":\""<<commandClass<<"\",\"renderEncoderClass\":\""<<encoderClass<<"\",\"simulation\":false,\"parityReference\":\"independent analytic RGBA8 panel pixels\"}\n";return 0;
		} catch(const std::exception& error){std::cerr<<"RTT GPU fixture failed: "<<error.what()<<'\n';return 1;}
	}
}
