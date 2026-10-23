/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeWaterGroundSourceData.h"
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace rendering {
class NativeTextureRegistry;
class NativeCUSTextureHistory;
class NativeSourceProgramBinding;
namespace ui {class NativeUIRecorder;}

// The Session passes its existing source owners. This class neither attaches
// another observer nor owns a second program/texture registry or mesh cache.
class NativeWaterGroundSourceCapture {
public:
	NativeWaterGroundSourceCapture(NativeTextureRegistry&,NativeCUSTextureHistory&,
		ui::NativeUIRecorder&,NativeSourceProgramBinding&);
	NativeWaterGroundSourceCapture(const NativeWaterGroundSourceCapture&)=delete;
	NativeWaterGroundSourceCapture& operator=(const NativeWaterGroundSourceCapture&)=delete;
	std::shared_ptr<const NativeWaterGroundSourceFrame> Capture(uint64_t sourceDrawFrame,
		NativeWorldDrawStage,const std::function<void()>& actualOriginalDraw,const WorldCamera* actualView=nullptr);
	// At original Draw entry, before either early return. A missing source hook
	// must fail instead of looking like a legitimate original early return.
	void BeginDraw();
	// Called by the original owner after its early-return/selection predicates.
	void EnterDraw();
	void EnterForward();
 void BeginDeferred(bool clearOnly,bool combined=false);
 void SubmitComposite(NativeTerrainProgramSnapshot);
 void RecordDeferredClear();
 void EndDeferred();
 void FinishDeferredCallbacks(std::shared_ptr<const NativeCUSSourceFrame>);
	void EnterBorder();
	void Callback(NativeGroundCallback,const std::function<void()>& actualOriginalCallback);
	void SubmitMapEdge(mapedge::DrawPacket actualOriginalDraw);
	// SetupBigSquare calls this AFTER its real bind and texSquare uniform write.
	// Empty ROAM patches keep these writes without allocating a draw ordinal.
	void SelectSquare(uint32_t x,uint32_t z);
	// At the real Basic/ROAM submit site, after the original empty predicate.
	// The caller suppresses only that GL submit while this capture is active.
	void SubmitSquare(NativeTerrainMeshSource,const void* actualPositions,size_t vertexCount,size_t vertexStride,
		std::span<const uint32_t> actualIndices,bool indexed,NativeTerrainProgramSnapshot actualCurrentProgram,
		const void* actualColors=nullptr,size_t colorStride=0,bool border=false);

private:
	void RequireActive() const;
	NativeTextureRegistry& registry;
	NativeCUSTextureHistory& history;
	ui::NativeUIRecorder& recorder;
	NativeSourceProgramBinding& programs;
	std::shared_ptr<NativeWaterGroundSourceFrame> frame;
	std::optional<NativeGroundCallback> callback;
	std::optional<std::pair<uint32_t,uint32_t>> square;
	bool drawInvoked=false;
 bool inDeferred=false;
 std::shared_ptr<NativeCUSDeferredPhase> mapPhase;
};

// Non-owning scope of the actual original invocation, absent outside Capture.
NativeWaterGroundSourceCapture* CurrentNativeWaterGroundSourceCapture() noexcept;
} // namespace rendering
