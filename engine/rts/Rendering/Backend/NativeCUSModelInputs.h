/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "NativeBufferData.h"
#include "NativeCUSTextureHistory.h"
#include "NativeCUSPublishedData.h"

#include <array>
#include <memory>
#include <span>
#include <vector>

namespace rendering {
struct WorldModel;

// Captured when original Add*ToSubmission resolves the object, matching the
// lifetime of its indirect draw command. Submit does not re-resolve its ID.
struct NativeCUSModelSubmission {
	NativeCUSSubmittedObject object;
	std::shared_ptr<const WorldModel> geometry;
	uint32_t originalIndexFirst = 0, originalIndexCount = 0;
};

// Sealed at Submit, after successful builder mutations. Factory validation is
// structural only: frame publications, pose and textures never live here.
class NativeCUSSubmissionRoster final {
public:
 static std::shared_ptr<const NativeCUSSubmissionRoster> Seal(
  std::span<const NativeCUSSubmittedObject>,
  std::span<const std::shared_ptr<const NativeCUSModelSubmission>>,
  std::span<const std::string> uniformBins);
 const auto& Objects() const noexcept { return objects; }
 const auto& Submissions() const noexcept { return submissions; }
 const auto& UniformBins() const noexcept { return uniformBins; }
private:
 NativeCUSSubmissionRoster() = default;
 std::vector<NativeCUSSubmittedObject> objects;
 std::vector<std::shared_ptr<const NativeCUSModelSubmission>> submissions;
 std::vector<std::string> uniformBins;
};

struct NativeCUSModelInputs {
	uint64_t sourceDrawFrame = 0, sourceSequence = 0;
	std::shared_ptr<const NativeCUSSubmissionRoster> roster;
 const auto& Submissions() const noexcept { return roster->Submissions(); }
	NativeBufferSnapshotRef originalInstances;
	std::shared_ptr<const NativeCUSPublishedBuffer> transforms;
	std::shared_ptr<const NativeCUSPublishedBuffer> modelUniforms;
};

std::shared_ptr<const NativeCUSModelInputs> MakeNativeCUSModelInputs(
	uint64_t sourceDrawFrame, uint64_t sourceSequence,
	std::shared_ptr<const NativeCUSSubmissionRoster>,
	NativeBufferSnapshotRef originalInstances,
	std::shared_ptr<const NativeCUSPublishedBuffer> transforms,
	std::shared_ptr<const NativeCUSPublishedBuffer> modelUniforms);

std::array<uint32_t, 4> ReadNativeCUSInstance(const NativeCUSModelInputs&, size_t submission);
std::array<uint8_t, 32> ReadNativeCUSTransform(const NativeCUSModelInputs&, uint32_t originalIndex);
std::array<uint8_t, 128> ReadNativeCUSModelUniform(const NativeCUSModelInputs&, size_t submission);

} // namespace rendering
