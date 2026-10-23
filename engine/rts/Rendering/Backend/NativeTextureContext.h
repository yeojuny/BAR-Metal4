/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "NativeUITextureHistory.h"

#include <array>
#include <functional>
#include <memory>
#include <string_view>

namespace rendering {
namespace ui { class NativeUIRecorder; }

enum class NativeGeometryTextureRole { Map, Models };

struct NativeGeometryTextureRequest {
	NativeGeometryTextureRole role;
	std::uint32_t width, height, samples;
	NativeCUSTextureTarget target;
	std::array<std::shared_ptr<const NativeCUSTextureObject>, 6> attachments;
	std::shared_ptr<const NativeCUSTextureObject> combinedColor;
};

// These callbacks belong to the actual native allocation owner. Constructor
// readiness is separate from completed image contents, and is never a global
// GL/FBO capability flag. CheckStatus may attach actual allocation-only backing;
// it must not publish a rendered/cleared image merely because storage exists.
struct NativeGeometryTextureServices {
	std::function<bool(NativeGeometryTextureRole)> constructible;
	std::function<bool(NativeGeometryTextureRole)> multisampleSupported;
	std::function<bool(const NativeGeometryTextureRequest&, NativeTextureRegistry&)> checkStatus;
};

// One instance follows the actual native renderer context, including LuaIntro.
// Frame/map/gadget reset must not recreate it or erase source binding history.
class NativeTextureContext {
public:
	NativeTextureContext();
	~NativeTextureContext();
	NativeTextureContext(const NativeTextureContext&) = delete;
	NativeTextureContext& operator=(const NativeTextureContext&) = delete;

	void Initialize(std::function<uint64_t()> actualSourceDrawFrame, NativeGeometryTextureServices);
	void AttachUI(ui::NativeUIRecorder&);
	void DetachUI();
	void Destroy(); // Actual context teardown, after families/source owners detach.
	bool Initialized() const noexcept { return initialized; }
	bool UIAttached() const noexcept { return recorder != nullptr; }

	NativeCUSTextureHistory& History();
	NativeTextureRegistry& Registry();
	ui::NativeUITextureHistory& UI();
	NativeGeometryTextureNames& Geometry(NativeGeometryTextureRole);

	bool RegisterGeometry(const void* originalOwner, std::string_view originalName);
	bool OwnsGeometry(const void*) const noexcept;
	bool GeometryValid(const void*) const;
	bool GeometryMultisampleSupported(const void*) const;
	bool CreateGeometry(const void*, uint32_t width, uint32_t height, NativeCUSTextureTarget, uint32_t samples, bool combinedColor=false);
	void DetachGeometry(const void*, bool originalInit);
	void UnregisterGeometry(const void*);
	uint64_t GeometryName(const void*, unsigned attachment) const;

private:
	struct GeometryOwner { const void* original = nullptr; bool valid = false; };
	NativeCUSTextureHistory history;
	NativeTextureRegistry registry;
	NativeGeometryTextureNames map, models;
	std::array<GeometryOwner, 2> owners;
	NativeGeometryTextureServices geometryServices;
	std::unique_ptr<ui::NativeUITextureHistory> uiHistory;
	ui::NativeUIRecorder* recorder = nullptr;
	bool initialized = false;
	void RequireContext() const;
	size_t OwnerIndex(const void*) const;
	NativeGeometryTextureNames& Names(size_t);
	const NativeGeometryTextureNames& Names(size_t) const;
};

NativeTextureContext& OriginalNativeTextureContext();

} // namespace rendering
