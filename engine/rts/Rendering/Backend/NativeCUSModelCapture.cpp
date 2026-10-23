/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSModelCapture.h"

#include "Lua/LuaUtils.h"
#include "Rendering/Models/3DModel.hpp"
#include "Rendering/Models/ModelsLock.h"
#include "Rendering/WorldFrameCapture.h"
#include "Sim/Features/Feature.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"

#include <stdexcept>

namespace rendering {
std::shared_ptr<const NativeCUSModelSubmission> NativeCUSModelCapture::CaptureSubmission(
	const NativeCUSSubmittedObject& object)
{
	const S3DModel* model = nullptr;
	// The same original owner resolver used by LuaVAOImpl::GetDrawIndicesImpl.
	switch (object.kind) {
		case NativeCUSObjectKind::Unit:
			model = LuaUtils::SolIdToObject<CUnit>(object.id, __func__)->model;
			break;
		case NativeCUSObjectKind::Feature:
			model = LuaUtils::SolIdToObject<CFeature>(object.id, __func__)->model;
			break;
		case NativeCUSObjectKind::UnitDefinition:
			model = LuaUtils::SolIdToObject<UnitDef>(object.id, __func__)->model;
			break;
	}
	if (!model) throw std::runtime_error("CUS original submitted object has no actual model");
	auto lock = CModelsLock::GetScopedLock();
	if (model->loadStatus != S3DModel::LOADED || model->indxStart == ~0u)
		throw std::runtime_error("CUS original model geometry/index publication is not ready");
	auto result = std::make_shared<NativeCUSModelSubmission>();
	result->object = object;
	result->geometry = sharedModels.CaptureModel(model);
	result->originalIndexFirst = model->indxStart;
	result->originalIndexCount = model->indxCount;
	return result;
}

std::shared_ptr<const NativeCUSModelInputs> NativeCUSModelCapture::CaptureDraw(
	uint64_t frame, uint64_t sequence,
	std::span<const std::shared_ptr<const NativeCUSModelSubmission>> submissions,
	NativeBufferSnapshotRef instances,
	std::shared_ptr<const NativeCUSPublishedBuffer> transforms,
	std::shared_ptr<const NativeCUSPublishedBuffer> uniforms)
{
	return MakeNativeCUSModelInputs(frame, sequence, submissions, std::move(instances),
		std::move(transforms), std::move(uniforms));
}

} // namespace rendering
