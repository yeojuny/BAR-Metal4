/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeTextureContext.h"
#include "NativeUIRecorder.h"

#include <stdexcept>
#include <utility>

namespace rendering {
namespace {
void Check(bool value, const char* message)
{
	if (!value) throw std::runtime_error(message);
}
}

NativeTextureContext::NativeTextureContext(): registry(history),
	map(registry, history, "GROUNDDRAWER-GBUFFER"),
	models(registry, history, "LUAOBJECTDRAWER-GBUFFER") {}
NativeTextureContext::~NativeTextureContext() = default;

void NativeTextureContext::RequireContext() const
{
	Check(initialized && registry.ContextGeneration(), "Original texture context is not initialized");
}

void NativeTextureContext::Initialize(std::function<uint64_t()> frame, NativeGeometryTextureServices services)
{
	Check(!initialized && !recorder && bool(frame), "Original texture context initialization owner differs");
	Check(bool(services.constructible) == bool(services.checkStatus), "Native GBuffer constructor/check-status providers must be paired");
	registry.NewContext();
	try {
		uiHistory = std::make_unique<ui::NativeUITextureHistory>(history, registry, std::move(frame));
		geometryServices = std::move(services);
		map.ForgetNames(); models.ForgetNames(); owners = {};
		initialized = true;
	} catch (...) { registry.CloseContext(); throw; }
}

void NativeTextureContext::AttachUI(ui::NativeUIRecorder& actualRecorder)
{
	RequireContext();
	Check(!recorder && actualRecorder.TextureObserver() == nullptr, "Original UI already has a texture writer owner");
	actualRecorder.SetTextureObserver(uiHistory.get());
	recorder = &actualRecorder;
}

void NativeTextureContext::DetachUI()
{
	if (!recorder) return;
	Check(recorder->TextureObserver() == uiHistory.get(), "Original UI texture observer was replaced by another owner");
	recorder->SetTextureObserver(nullptr);
	recorder = nullptr;
}

void NativeTextureContext::Destroy()
{
	if (!initialized) return;
	Check(!recorder && !owners[0].original && !owners[1].original,
		"Original texture context still has attached UI/GBuffer source owners");
	// Context destruction invalidates names without inventing another context's
	// default state. Retained submitted snapshots keep their own image owners.
	uiHistory.reset();
	geometryServices = {};
	map.ForgetNames(); models.ForgetNames();
	registry.CloseContext();
	initialized = false;
}

NativeCUSTextureHistory& NativeTextureContext::History() { RequireContext(); return history; }
NativeTextureRegistry& NativeTextureContext::Registry() { RequireContext(); return registry; }
ui::NativeUITextureHistory& NativeTextureContext::UI() { RequireContext(); return *uiHistory; }
NativeGeometryTextureNames& NativeTextureContext::Names(size_t i) { return i == 0 ? map : models; }
const NativeGeometryTextureNames& NativeTextureContext::Names(size_t i) const { return i == 0 ? map : models; }
NativeGeometryTextureNames& NativeTextureContext::Geometry(NativeGeometryTextureRole role)
{
	RequireContext();
	Check(role == NativeGeometryTextureRole::Map || role == NativeGeometryTextureRole::Models, "Unknown original GBuffer role");
	return Names(static_cast<size_t>(role));
}

size_t NativeTextureContext::OwnerIndex(const void* original) const
{
	RequireContext();
	for (size_t i = 0; i < owners.size(); ++i) if (original && owners[i].original == original) return i;
	throw std::runtime_error("Original GBuffer source owner is not registered");
}

bool NativeTextureContext::RegisterGeometry(const void* original, std::string_view name)
{
	if (!initialized) return false;
	const size_t index = name == "GROUNDDRAWER-GBUFFER" ? 0 : name == "LUAOBJECTDRAWER-GBUFFER" ? 1 : owners.size();
	if (index == owners.size()) return false;
	Check(original && (!owners[index].original || owners[index].original == original), "Original GBuffer role has two source owners");
	// Init memset discards IDs even after an invalid FBO's Kill skipped Delete.
	// Such old objects may remain bound and must survive until context teardown.
	Names(index).ForgetNames();
	if (!owners[index].original)
		owners[index] = {original, geometryServices.constructible && geometryServices.constructible(static_cast<NativeGeometryTextureRole>(index))};
	return true;
}

bool NativeTextureContext::OwnsGeometry(const void* original) const noexcept
{
	return initialized && original && (owners[0].original == original || owners[1].original == original);
}

bool NativeTextureContext::GeometryValid(const void* original) const { return owners[OwnerIndex(original)].valid; }
bool NativeTextureContext::GeometryMultisampleSupported(const void* original) const
{
	const auto index = OwnerIndex(original);
	return geometryServices.multisampleSupported && geometryServices.multisampleSupported(static_cast<NativeGeometryTextureRole>(index));
}

bool NativeTextureContext::CreateGeometry(const void* original, uint32_t width, uint32_t height,
	NativeCUSTextureTarget target, uint32_t samples, bool combinedColor)
{
	const auto index = OwnerIndex(original);
	Check(bool(geometryServices.checkStatus), "Original native GBuffer allocation provider is absent");
	auto& names = Names(index);
	// A direct original Create generates new IDs, even without a preceding
	// Detach. Update's actual Detach call owns deletion on the resize path.
	names.ForgetNames();
	Check(!combinedColor || index==0,"Only original Map GBuffer owns optional combined color");
	names.Update(width, height, target, samples, combinedColor);
	NativeGeometryTextureRequest request{static_cast<NativeGeometryTextureRole>(index), width, height, samples, target, {}};
	for (unsigned i = 0; i < request.attachments.size(); ++i) request.attachments[i] = registry.Find(names.Name(i));
	if(combinedColor)request.combinedColor=registry.Find(names.Name(NativeGeometryTextureNames::CombinedColorAttachment));
	// Names.Update has already performed the source's final current-unit Bind0.
	const bool complete = geometryServices.checkStatus(request, registry);
	for (unsigned i = 0; i < request.attachments.size(); ++i) {
		const auto actual = registry.Find(names.Name(i));
		Check(actual && actual->identity == request.attachments[i]->identity &&
			actual->storage != NativeTextureStorageKnowledge::Published,
			"GBuffer allocation readiness fabricated/replaced original image contents");
	}
	if(request.combinedColor){const auto actual=registry.Find(request.combinedColor->name);
		Check(actual&&actual->identity==request.combinedColor->identity&&actual->storage!=NativeTextureStorageKnowledge::Published,
			"Combined color allocation fabricated or replaced rendered contents");}
	return owners[index].valid = complete && width != 0 && height != 0;
}

void NativeTextureContext::DetachGeometry(const void* original, bool init)
{
	const auto index = OwnerIndex(original);
	if (!init) Names(index).Detach();
}

void NativeTextureContext::UnregisterGeometry(const void* original)
{
	const auto index = OwnerIndex(original);
	Names(index).ForgetNames();
	owners[index] = {};
}

uint64_t NativeTextureContext::GeometryName(const void* original, unsigned attachment) const
{
	return Names(OwnerIndex(original)).Name(attachment);
}

NativeTextureContext& OriginalNativeTextureContext()
{
	static NativeTextureContext context;
	return context;
}

} // namespace rendering
