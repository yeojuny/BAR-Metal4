/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativePostprocessData.h"
#include <functional>

namespace rendering {
// Main-thread recording only. Version cells are transactionally rolled back if
// the actual screen/color/depth frame was skipped or failed; target-only UI
// completion cannot retire screen-dependent copies. Per-operation rejection
// retains earlier commands for ordinary Lua error/removal behavior.
struct NativePostprocessPublication {
 virtual ~NativePostprocessPublication()=default;
 virtual void Cancel() noexcept=0;
};
class NativePostprocessRecorder {
public:
 using PublicationObserver=std::function<std::shared_ptr<NativePostprocessPublication>(const std::shared_ptr<NativePostprocessBinding>&,const std::shared_ptr<const NativePostprocessVersion>&,std::uint64_t)>;
 void SetPublicationObserver(PublicationObserver observer){publicationObserver=std::move(observer);}
	void BeginFrame(std::uint32_t width, std::uint32_t height, std::uint64_t sourceDrawFrame = 0);
	std::shared_ptr<const NativePostprocessFrame> EndFrame();
	// Immutable prefix for a same-frame readback. This does not retire any
	// publication, close the source callback, or advance the frame serial.
	std::shared_ptr<const NativePostprocessFrame> SnapshotPrefix(std::uint64_t cutoff) const;
	void AbortFrame() noexcept;
	void RetireCompletedFrame(std::uint64_t serial);
	// Frame transaction only: mark exactly the selected output publications
	// after real Begin, then retire them after their actual batch completes.
	void RecordSubmission(const NativePostprocessFrame&,std::uint64_t actualSubmission) noexcept;
	void RetireSubmission(std::uint64_t actualSubmission);
	void Reset() noexcept;
	bool IsActive() const noexcept { return active; }
	bool OwnsPendingVersion(const std::shared_ptr<const NativePostprocessVersion>& version) const noexcept;
	std::shared_ptr<const NativePostprocessVersion> Copy(std::shared_ptr<NativePostprocessBinding> destination,
		NativePostprocessOperationKind kind, NativePostprocessRect source, NativePostprocessRect offset, std::uint32_t screenCommandOffset,
		NativePostprocessStage stage = NativePostprocessStage::Screen, std::uint64_t sequence = 0,
		std::shared_ptr<const ScreenTexture> priorScreen = {},
		std::function<void(std::shared_ptr<const NativePostprocessVersion>)> commitScreen = {});
	// Strong cross-recorder transaction: preallocate and validate first, invoke
	// the actual family append callback, then publish with noexcept moves only.
	std::shared_ptr<const NativePostprocessVersion> PublishExternalOutput(
		std::shared_ptr<NativePostprocessBinding> destination, NativePostprocessStage stage,
		std::uint64_t sequence, NativeExternalImageFamily family,
		std::span<const std::shared_ptr<const NativePostprocessVersion>> inputs,
		std::function<void(std::shared_ptr<const NativePostprocessVersion>)> commitActualProducer);
	void Draw(std::span<const std::shared_ptr<NativePostprocessBinding>> destinations,
		std::array<std::shared_ptr<const NativePostprocessVersion>, 6> inputs, std::array<bool, 6> enabled,
		NativeDOFUniforms uniforms, NativePostprocessQuad quad, NativePostprocessRect viewport, std::uint32_t screenCommandOffset, bool screenOutput);
 void DrawSepia(std::shared_ptr<const NativePostprocessVersion> input, NativeSepiaUniforms uniforms,
  NativePostprocessRect viewport, std::uint32_t commandOffset);
 void DrawCAS(std::shared_ptr<const NativePostprocessVersion> input, NativeCASUniforms uniforms,
  NativePostprocessRect viewport, std::uint32_t screenCommandOffset);
private:
	struct Change {
		std::shared_ptr<NativePostprocessBinding> binding;
		std::shared_ptr<const NativePostprocessVersion> previous, published;
  std::shared_ptr<NativePostprocessPublication> publication;
  std::uint64_t actualSubmission=0;
	};
	void Commit(NativePostprocessOperation operation, std::span<const std::shared_ptr<NativePostprocessBinding>> outputs,
		std::function<void(std::shared_ptr<const NativePostprocessVersion>)> commitScreen = {});
	void Rollback() noexcept;
	bool active = false;
	std::uint64_t nextSerial = 1;
	std::shared_ptr<NativePostprocessFrame> recording;
	std::shared_ptr<const NativePostprocessFrame> pending;
	std::vector<Change> changes;
 PublicationObserver publicationObserver;
};
} // namespace rendering
