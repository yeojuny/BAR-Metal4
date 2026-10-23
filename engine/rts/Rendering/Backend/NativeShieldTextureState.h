/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeTextureRegistry.h"

namespace rendering {
enum class NativeShieldTextureKind { Empty, Depth, Color };
struct NativeShieldTextureSnapshot {
	uint64_t contextGeneration=0,eventSequence=0;
	std::array<NativeCUSTextureBinding,2> samplers;
};
// One context owner injects these same non-owning services into every family.
// Their lifetime must exceed that of the installed private Lua contexts. A
// frame, gadget reload or rendering-mode toggle is NOT a new texture context.
struct NativeShieldTextureContext {
	NativeCUSTextureHistory* history=nullptr;
	NativeTextureRegistry* registry=nullptr;
	NativeGeometryTextureNames* map=nullptr;
	NativeGeometryTextureNames* models=nullptr;
	// Calls the shared recorder's no-writer SyncObservedTextureBinding after
	// the actual History event. It must not observe/write that event again.
	std::function<void(unsigned actualUnit)> syncObservedBinding;
	void Validate() const;
	bool Texture(unsigned requestedUnit,bool enabled) const;
	bool Texture(unsigned requestedUnit,const std::string& originalName) const;
	std::shared_ptr<const NativeShieldTextureSnapshot> Capture(unsigned originalEffectBits) const;
};
NativeShieldTextureKind ClassifyNativeShieldTexture(const NativeCUSTextureBinding&);
void ValidateNativeShieldTextureSnapshot(const NativeShieldTextureSnapshot&,unsigned originalEffectBits);
} // namespace rendering
