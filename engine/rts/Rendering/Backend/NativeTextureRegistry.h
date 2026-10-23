/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "NativeCUSTextureHistory.h"

#include <unordered_map>

namespace rendering {

// ParseTextureImage success and a nonzero GetTextureID are separate facts.
struct NativeTextureLookup {
 bool parsed=false;
 std::shared_ptr<const NativeCUSTextureObject> object;
 bool shadowSampler=false; // Exact LUATEX_SHADOWMAP post-Bind writer.
};

// Common native texture namespace. Logical names survive content updates;
// object identities change only at real create/delete/rebind lifetimes.
// The virtual name is not an MTLResourceID, path hash or producer-frame ID.
class NativeTextureRegistry {
public:
	explicit NativeTextureRegistry(NativeCUSTextureHistory&);
	~NativeTextureRegistry();
	NativeTextureRegistry(const NativeTextureRegistry&) = delete;
	NativeTextureRegistry& operator=(const NativeTextureRegistry&) = delete;
	void NewContext();
	void CloseContext() noexcept;
	uint64_t ContextGeneration() const { return contextOpen ? context : 0; }
	bool MatchesHistory(const NativeCUSTextureHistory& h) const noexcept { return &history==&h; }
	std::shared_ptr<const NativeCUSTextureObject> Create(NativeCUSTextureTarget,
		std::string originalSource, uint64_t originalVirtualName = 0);
	std::shared_ptr<const NativeCUSTextureObject> Find(uint64_t name) const;
	std::shared_ptr<const NativeCUSTextureObject> Resolve(std::shared_ptr<const NativeCUSTextureObject>) const;
	void Bind(unsigned unit, uint64_t name, NativeCUSTextureTarget);
	void SetSampling(uint64_t name, const NativeTextureSampling& actualState);
	void SetBoundSampling(unsigned unit, NativeCUSTextureTarget, const NativeTextureSampling&);
	bool LuaTexture(unsigned requestedUnit, const NativeTextureLookup&);
	bool LuaTexture(unsigned requestedUnit, const std::function<NativeTextureLookup()>& actualParser);
	std::shared_ptr<const NativeCUSTextureObject> ResolveRecordedName(std::shared_ptr<const NativeCUSTextureObject>);
	void Allocate(uint64_t name, const NativeTextureAllocation& actualAllocation);
	void PublishContent(uint64_t name, std::shared_ptr<const WorldTexture>, std::shared_ptr<const NativeTextureBacking>);
	// One original TexImage event: replace allocation metadata and immutable
	// contents together. The actual owner supplies retained per-level storage.
	// All throwing work precedes publication so its live cells can follow with
	// noexcept pointer assignments. No bind, sampler or name lifetime event.
	std::shared_ptr<const NativeCUSTextureObject> PublishRedefinedContent(
		std::shared_ptr<const NativeCUSTextureObject> expected, const NativeTextureAllocation&,
		std::shared_ptr<const NativeTextureBacking>);
	// Actual submission owner only: withdraw an unsubmitted content version,
	// preserving all later bind/enable/sampler events and retained draw tokens.
	// A deleted/recreated/reallocated/newly published object is never reverted.
	bool CancelContentPublication(std::shared_ptr<const NativeCUSTextureObject> expected,
		std::shared_ptr<const NativeCUSTextureObject> previous) noexcept;
	bool Delete(uint64_t name);
private:
	NativeCUSTextureHistory& history;
	uint64_t context = 0, nextName = 1, nextIdentity = 1;
	bool contextOpen = false;
	struct ObjectEntry {
		std::shared_ptr<const NativeCUSTextureObject> current;
		// Abort/skip can select prior content without allocating or issuing GL
		// events. The next ordinary query materializes its immutable snapshot.
		std::shared_ptr<const NativeCUSTextureObject> restoredContent;
		uint64_t restoredVersion=0;
	};
	mutable std::unordered_map<uint64_t, ObjectEntry> objects;
	std::shared_ptr<const NativeCUSTextureObject> RestoreSaved(std::shared_ptr<const NativeCUSTextureObject>);
	void Replace(std::shared_ptr<NativeCUSTextureObject>);
};

// Native implementation of the original GeometryBuffer texture lifecycle.
// Caller invokes Update at the original Create/Update decision, independently
// from per-frame image producers. False/no-redraw is not a delete or rebind.
class NativeGeometryTextureNames {
public:
	static constexpr unsigned AttachmentCount = 6, DepthAttachment = 5;
	static constexpr unsigned CombinedColorAttachment = 6; // Optional; existing depth index is stable.
	NativeGeometryTextureNames(NativeTextureRegistry&, NativeCUSTextureHistory&, std::string originalOwner);
	// Returns whether texture lifetime events occurred, NOT FBO CheckStatus.
	// Zero extent still creates six incomplete images and binds object0 last.
	bool Update(uint32_t width, uint32_t height, NativeCUSTextureTarget target, uint32_t actualSamples, bool combinedColor=false);
	void Detach();
	// Original Init/Kill can forget IDs without glDelete after invalid FBOs.
	void ForgetNames() noexcept;
	uint64_t Name(unsigned attachment) const;
	uint32_t Width() const { return width; }
	uint32_t Height() const { return height; }
	bool HasCombinedColor() const { return names[CombinedColorAttachment]!=0; }
private:
	NativeTextureRegistry& registry;
	NativeCUSTextureHistory& history;
	std::string owner;
	std::array<uint64_t, AttachmentCount+1> names{};
	uint32_t width = 0, height = 0;
	NativeCUSTextureTarget target=NativeCUSTextureTarget::Image2D;
	uint32_t samples=0;
	uint64_t context = 0;
};

} // namespace rendering
