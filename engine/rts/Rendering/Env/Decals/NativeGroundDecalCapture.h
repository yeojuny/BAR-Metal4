/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeGroundDecalData.h"
#include <functional>

class CBitmap;
class UpdateList;
struct GroundDecal;

namespace rendering {
using NativeGroundDecalImageProvider = std::function<std::shared_ptr<const WorldTexture>(const std::string&)>;
// Register the existing map-scoped WorldFrameCapture image cache. Readiness
// additionally requires the paired GPU consumer; this is not a GL capability.
void SetNativeGroundDecalImageProvider(NativeGroundDecalImageProvider);
bool NativeGroundDecalReady() noexcept;
std::shared_ptr<const WorldTexture> CaptureNativeGroundDecalBitmap(const CBitmap&, const std::string&);
std::shared_ptr<const WorldTexture> CaptureNativeGroundDecalImage(const CBitmap&, const std::string& resolvedPath);
uint64_t NextNativeGroundDecalResourceRevision();

class NativeGroundDecalInstanceCapture {
public:
	std::shared_ptr<const NativeGroundDecalInstances> Capture(std::span<const GroundDecal>, UpdateList&);
	void Reset() { snapshot.reset(); }
private:
	std::shared_ptr<const NativeGroundDecalInstances> snapshot;
};

std::shared_ptr<const NativeGroundDecalFrame> CaptureNativeGroundDecalState(
	std::shared_ptr<const NativeGroundDecalAtlas>, std::shared_ptr<const NativeGroundDecalInstances>, bool highQuality);
// Call at the original DrawPreDecals -> engine decals boundary, before
// DrawWorldPreUnit can alter Lua-created decals or engine drawing state.
std::shared_ptr<const NativeGroundDecalFrame> CaptureNativeGroundDecals();
}
