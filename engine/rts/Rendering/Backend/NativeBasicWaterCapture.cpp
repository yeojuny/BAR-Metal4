/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeBasicWaterCapture.h"
#include "Map/ReadMap.h"
#include "Rendering/Env/WaterRendering.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Textures/Bitmap.h"
#include "Rendering/Textures/nv_dds.h"
#include "System/Exceptions.h"
#include "System/FileSystem/FileSystem.h"
#include "System/Log/ILog.h"
#include "System/SpringMath.h"
#include <limits>

namespace rendering {
namespace {
uint64_t nextGeneration=0; // Original unsynced renderer-resource lifetime only.
bool OriginalLoadFails(const std::string& path)
{
	// The shared provider already tried the real original payload. Only its
	// original loader returning false permits BasicWater's blue dummy. A valid
	// image rejected by an unsupported native format must still report failure.
	if(FileSystem::GetExtensionLowerCase(path)=="dds"){
		nv_dds::CDDSImage image;return !image.load(path,path.find("unitpics")==std::string::npos);
	}
	CBitmap image;return !image.Load(path);
}
}
void NativeBasicWaterCapture::Initialize(TextureProvider provider)
{
	if(!provider||!readMap||!waterRendering)throw content_error("Original BasicWater initialization dependencies missing");
	static_assert(SQUARE_SIZE==8);
	if(nextGeneration==std::numeric_limits<uint64_t>::max())throw content_error("Original BasicWater generation exhausted");
	auto next=std::make_shared<NativeBasicWaterMaterial>();next->generation=++nextGeneration;
	const auto& w=*waterRendering;
	try {next->texture=provider(w.texture);}
	catch(const content_error&){
		if(!OriginalLoadFails(w.texture))throw;
		LOG_L(L_WARNING,"[NativeBasicWater] could not read water texture from file \"%s\"",w.texture.c_str());
		auto blue=std::make_shared<WorldTexture>();blue->sourcePath=w.texture;blue->mipmappedSampling=false;
		blue->mips.push_back({1,1,4,{0,0,255,255}});next->texture=std::move(blue);next->originalMissingTexture=true;
	}
	if(!next->texture||next->texture->mips.empty())throw content_error("Original BasicWater texture provider returned no image");
	const auto& image=next->texture->mips.front();
	next->mesh=std::make_shared<NativeBasicWaterMesh>(MakeNativeBasicWaterMesh(mapDims.mapx,mapDims.mapy,image.width,image.height,w.texture,w.repeatX,w.repeatY));
	ValidateNativeBasicWaterMaterial(*next);
	material=std::move(next);
}
std::shared_ptr<const NativeBasicWaterFrame> NativeBasicWaterCapture::Capture(const WorldFrame& world,bool wireframe)const
{
	if(!Initialized())throw content_error("Original BasicWater capture before InitResources");
	auto out=std::make_shared<NativeBasicWaterFrame>();out->sourceDrawFrame=world.sourceDrawFrame;out->material=material;out->camera=world.viewCamera;
	out->drawWater=globalRendering->drawWater;out->hasVisibleWater=readMap->HasVisibleWater();out->forceRendering=waterRendering->forceRendering;out->wireframe=wireframe;
	ValidateNativeBasicWater(*out);return out;
}
} // namespace rendering
