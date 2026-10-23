/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeGroundDecalCapture.h"
#include "Rendering/Env/Decals/GroundDecal.h"
#include "Rendering/Env/Decals/GroundDecalHandler.h"
#include "Game/Camera.h"
#include "Map/ReadMap.h"
#include "Rendering/Common/UpdateList.h"
#include "Rendering/Env/SunLighting.h"
#include "Rendering/Env/WaterRendering.h"
#include "Rendering/Env/SkyLight.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Textures/Bitmap.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/Exceptions.h"
#include "System/FileSystem/FileSystem.h"
#include <cstring>
#include <limits>
#include <type_traits>

namespace rendering {
namespace {
NativeGroundDecalImageProvider imageProvider;
std::atomic<uint64_t> nextRevision{1};
void Check(bool value, const char* message) { if (!value) throw content_error(message); }
}
void SetNativeGroundDecalImageProvider(NativeGroundDecalImageProvider provider) { imageProvider = std::move(provider); }
bool NativeGroundDecalReady() noexcept { return IsNativeGroundDecalConsumerAvailable() && bool(imageProvider); }
uint64_t NextNativeGroundDecalResourceRevision()
{
	const uint64_t value = nextRevision.fetch_add(1, std::memory_order_relaxed);
	Check(value != 0, "Original ground decal resource revision exhausted");
	return value;
}
std::shared_ptr<const WorldTexture> CaptureNativeGroundDecalBitmap(const CBitmap& image, const std::string& path)
{
	Check(!image.Empty() && !image.compressed && image.channels == 4 && image.dataType == 0x1401 && image.xsize > 0 && image.ysize > 0,
		"Original ground decal bitmap is not decoded RGBA8");
	const size_t width = image.xsize, height = image.ysize;
	Check(width <= 16384 && height <= 16384 && width * height * 4 <= size_t(std::numeric_limits<int32_t>::max()),
		"Original ground decal bitmap dimensions exceed valid payload arithmetic");
	const size_t count = width * height * 4;
	Check(image.GetRawMem() && image.GetMemSize() == count, "Original ground decal bitmap payload extent differs");
	auto output = std::make_shared<WorldTexture>(); output->sourcePath = path;
	WorldTextureMip mip{uint32_t(width), uint32_t(height), width * 4, {}};
	mip.bytes.assign(image.GetRawMem(), image.GetRawMem() + count);
	output->mips.push_back(std::move(mip));
	// Original atlas renders every output mip from source LOD0. No source mip
	// generation is needed by this consumer, and processed BMP alpha is kept.
	return output;
}
std::shared_ptr<const WorldTexture> CaptureNativeGroundDecalImage(const CBitmap& bitmap, const std::string& resolvedPath)
{
	if (FileSystem::GetExtensionLowerCase(resolvedPath) != "dds")
		return CaptureNativeGroundDecalBitmap(bitmap, resolvedPath);
	Check(NativeGroundDecalReady(), "Original ground decal DDS image provider unavailable");
	auto output = imageProvider(resolvedPath);
	Check(output && !output->mips.empty(), "Original ground decal DDS image is missing");
	return output; // Actual original decoder bytes; never HEADLESS CBitmap's dummy.
}

static_assert(std::is_trivially_copyable_v<GroundDecal> && std::is_trivially_copyable_v<NativeGroundDecalInstance>);
static_assert(sizeof(GroundDecal) == sizeof(NativeGroundDecalInstance));
#define CHECK_ATTRIBUTE(source, target) static_assert(offsetof(GroundDecal, source) == offsetof(NativeGroundDecalInstance, target))
CHECK_ATTRIBUTE(refHeight, forcedHeight); CHECK_ATTRIBUTE(posTL, posT); CHECK_ATTRIBUTE(posBR, posB);
CHECK_ATTRIBUTE(texMainOffsets, uvMain); CHECK_ATTRIBUTE(texNormOffsets, uvNorm);
CHECK_ATTRIBUTE(alpha, createParams1); CHECK_ATTRIBUTE(rot, createParams2); CHECK_ATTRIBUTE(createFrameMin, createParams3);
CHECK_ATTRIBUTE(forcedNormal, createParams4); CHECK_ATTRIBUTE(infoRep, createParams5); CHECK_ATTRIBUTE(userDefined, userDefined);
#undef CHECK_ATTRIBUTE

std::shared_ptr<const NativeGroundDecalInstances> NativeGroundDecalInstanceCapture::Capture(std::span<const GroundDecal> rows, UpdateList& updates)
{
	Check(rows.size() == updates.Size(), "Original ground decal dirty flags differ from instance count");
	const bool replace = !snapshot || snapshot->values.size() != rows.size();
	if (!replace && !updates.NeedUpdate()) return snapshot;
	auto output = std::make_shared<NativeGroundDecalInstances>();
	if (replace) {
		output->values.resize(rows.size());
		if (!rows.empty()) std::memcpy(output->values.data(), rows.data(), rows.size_bytes());
	} else {
		output->values = snapshot->values;
		for (auto range = updates.GetNext(); range; range = updates.GetNext(range)) {
			const auto [offset, count] = updates.GetOffsetAndSize(*range);
			Check(offset <= rows.size() && count <= rows.size() - offset, "Original ground decal dirty range exceeds instances");
			if (count) std::memcpy(output->values.data() + offset, rows.data() + offset, count * sizeof(GroundDecal));
		}
	}
	output->revision = NextNativeGroundDecalResourceRevision();
	snapshot = output;
	updates.ResetNeedUpdateAll(); // The immutable upload revision now owns every dirty byte.
	return snapshot;
}

std::shared_ptr<const NativeGroundDecalFrame> CaptureNativeGroundDecalState(
	std::shared_ptr<const NativeGroundDecalAtlas> atlas, std::shared_ptr<const NativeGroundDecalInstances> instances, bool highQuality)
{
	Check(NativeGroundDecalReady(), "Original ground decal producer outlived its paired consumer");
	auto frame = std::make_shared<NativeGroundDecalFrame>();
	frame->atlas = std::move(atlas); frame->instances = std::move(instances);
	frame->sourceDrawFrame = globalRendering->drawFrame;
	frame->viewProjection = std::to_array(camera->GetViewProjectionMatrix().m);
	frame->inverseViewProjection = std::to_array(camera->GetViewProjectionMatrixInverse().m);
	frame->zeroToOneDepth = globalRendering->supportClipSpaceControl;
	frame->viewport = {globalRendering->viewPosX, globalRendering->viewPosY, globalRendering->viewSizeX, globalRendering->viewSizeY};
	frame->mapDims = {float(mapDims.mapx * SQUARE_SIZE), float(mapDims.mapy * SQUARE_SIZE), 1.f / (mapDims.mapx * SQUARE_SIZE), 1.f / (mapDims.mapy * SQUARE_SIZE)};
	frame->mapDimsPO2 = {float(mapDims.pwr2mapx * SQUARE_SIZE), float(mapDims.pwr2mapy * SQUARE_SIZE), 1.f / (mapDims.pwr2mapx * SQUARE_SIZE), 1.f / (mapDims.pwr2mapy * SQUARE_SIZE)};
	const auto& ambient = sunLighting->groundAmbientColor;
	frame->groundAmbient = {ambient.x, ambient.y, ambient.z, sunLighting->groundShadowDensity};
	auto xyz = [](const auto& v) { return WorldVector3{v.x, v.y, v.z}; };
	frame->groundDiffuse = xyz(sunLighting->groundDiffuseColor);
	frame->sunDirection = xyz(ISky::GetSky()->GetLight()->GetLightDir());
	frame->shadows = shadowHandler.ShadowsLoaded();
	frame->shadow = std::to_array(CMatrix44f::Identity().m);
	if (frame->shadows) std::copy_n(shadowHandler.GetShadowMatrixRaw(), 16, frame->shadow.begin());
	frame->waterAbsorption = readMap->HasVisibleWater();
	if (frame->waterAbsorption) {
		frame->waterMin = xyz(waterRendering->minColor); frame->waterBase = xyz(waterRendering->baseColor); frame->waterAbsorb = xyz(waterRendering->absorb);
	}
	frame->adjustedFrame = std::max(gs->frameNum, 0) + globalRendering->timeOffset;
	frame->highQuality = highQuality;
	frame->sampleCount = std::max(globalRendering->msaaLevel, 1);
	ValidateNativeGroundDecalFrame(*frame);
	return frame;
}
std::shared_ptr<const NativeGroundDecalFrame> CaptureNativeGroundDecals()
{
	if (!globalRendering->drawGround || !IGroundDecalDrawer::GetDrawDecals()) return {};
	auto* handler = dynamic_cast<CGroundDecalHandler*>(groundDecals);
	Check(handler != nullptr, "Original ground decal enabled without its original handler");
	return handler->CaptureNativeFrame();
}
}
