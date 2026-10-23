/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSSourceCapture.h"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
NativeCUSSourceCapture* sourceOwner=nullptr;
unsigned GLFactor(ScreenBlendFactor f)
{
	switch(f){case ScreenBlendFactor::Zero:return 0;case ScreenBlendFactor::One:return 1;
	case ScreenBlendFactor::SourceColor:return 0x300;case ScreenBlendFactor::OneMinusSourceColor:return 0x301;
	case ScreenBlendFactor::SourceAlpha:return 0x302;case ScreenBlendFactor::OneMinusSourceAlpha:return 0x303;
	case ScreenBlendFactor::DestinationAlpha:return 0x304;case ScreenBlendFactor::OneMinusDestinationAlpha:return 0x305;
	case ScreenBlendFactor::DestinationColor:return 0x306;case ScreenBlendFactor::OneMinusDestinationColor:return 0x307;
	case ScreenBlendFactor::SourceAlphaSaturate:return 0x308;}
	throw std::runtime_error("CUS inherited blend factor has no original GL identity");
}
}
NativeCUSSourceCapture::NativeCUSSourceCapture(WorldFrameCapture& world,NativeTextureContext& context,ui::NativeUIRecorder& ui)
	:models(world),textures(context),recorder(ui){}
NativeCUSSourceCapture::~NativeCUSSourceCapture(){Reset();}
void NativeCUSSourceCapture::Initialize(NativeCUSSourceProviders input)
{
	Check(!installed&&!sourceOwner,"CUS source capture already belongs to an actual context owner");
	Check(textures.Initialized()&&textures.UIAttached(),"CUS source capture requires the original shared texture context");
	Check(input.ready&&input.compile&&input.parseTexture&&input.deleteTexture&&input.readClipDistance0&&input.writeClipDistance0,
		"CUS source capture is missing its actual compiler/resource/raster owners");
	providers=std::move(input);
	LuaNativeCUSSource::Services services;
	services.ready=[this]{return Ready();};services.compile=providers.compile;services.parseTexture=providers.parseTexture;services.deleteTexture=providers.deleteTexture;
	services.syncObservedBinding=[this](unsigned unit){recorder.SyncObservedTextureBinding(unit);};
	services.inheritedRaster=[this]{return Raster();};services.applyRaster=[this](const auto& d){ApplyRaster(d);};
	services.captureSubmission=[this](const auto& object){return models.CaptureSubmission(object);};
	services.captureObjectInputs=[this](const NativeCUSSourceDraw& d){
		const auto& original=OriginalCUSModelPublication();
		return models.CaptureDraw(d.textures->sourceDrawFrame,d.sequence,d.submissions,d.instances,original.Transforms(),original.ModelUniforms());
	};
	services.captureEngineUniforms=[]{return OriginalCUSUniformCapture().Current();};
	LuaNativeCUSSource::SetTextureContext(&textures.History(),&textures.Registry());
	LuaNativeCUSSource::Configure(std::move(services));sourceOwner=this;installed=true;
}
void NativeCUSSourceCapture::Reset()
{
	if(!installed)return;
	LuaNativeCUSSource::Configure({});LuaNativeCUSSource::Reset();LuaNativeCUSSource::SetTextureContext(nullptr,nullptr);
	providers={};installed=false;if(sourceOwner==this)sourceOwner=nullptr;
}
bool NativeCUSSourceCapture::Ready()const
{
	return installed&&textures.Initialized()&&textures.UIAttached()&&OriginalCUSModelPublication().Initialized()&&
		OriginalCUSUniformCapture().Initialized()&&providers.ready&&providers.ready();
}
NativeCUSSourceDraw NativeCUSSourceCapture::Raster()const
{
	Check(Ready(),"CUS source raster was queried before its actual context was ready");
	NativeCUSSourceDraw d;d.world=recorder.CurrentWorldState();d.surface=recorder.CurrentState().draw;d.rasterObserved=true;
	d.colorMask=d.world.colorWrite;d.depthTest=d.world.depthTest;d.depthWrite=d.world.depthWrite;d.depthFunction=0x200+unsigned(d.world.depthCompare);
	d.cullEnabled=d.world.cullEnabled;
	switch(d.world.cull){case NativeWorldCull::None:case NativeWorldCull::Back:d.cullFace=0x405;break;case NativeWorldCull::Front:d.cullFace=0x404;break;case NativeWorldCull::FrontAndBack:d.cullFace=0x408;break;}
	d.sourceBlend=GLFactor(d.surface.blend.sourceColor);d.destinationBlend=GLFactor(d.surface.blend.destinationColor);d.clipDistance0=providers.readClipDistance0();return d;
}
void NativeCUSSourceCapture::ApplyRaster(const NativeCUSSourceDraw& d)
{
	Check(d.rasterObserved&&d.depthFunction>=0x200&&d.depthFunction<=0x207,"CUS source raster did not come from its original callback");
	auto world=d.world;world.colorWrite=d.colorMask;world.combinedColorWrite=world.colorWrite;world.depthTest=d.depthTest;world.depthWrite=d.depthWrite;world.depthCompare=ScreenAlphaCompare(d.depthFunction-0x200);world.cullEnabled=d.cullEnabled;
	switch(d.cullFace){case 0x404:world.cull=NativeWorldCull::Front;break;case 0x405:world.cull=NativeWorldCull::Back;break;case 0x408:world.cull=NativeWorldCull::FrontAndBack;break;default:throw std::runtime_error("CUS source cull face is invalid");}
	recorder.UpdateWorldState([world](auto& target){target=world;},false);
	recorder.UpdateBlend([blend=d.surface.blend](auto& target){target=blend;});
	providers.writeClipDistance0(d.clipDistance0);
}
std::shared_ptr<const NativeCUSSourceFrame> NativeCUSSourceCapture::Capture(uint64_t sourceDrawFrame,const std::function<void()>& callback)
{
	Check(Ready()&&bool(callback),"CUS original callback lacks its connected source capture");
	LuaNativeCUSSource::BeginFrame(sourceDrawFrame);
	try{callback();return LuaNativeCUSSource::EndFrame();}catch(...){LuaNativeCUSSource::AbortFrame();throw;}
}
} // namespace rendering
