/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeTextureRegistry.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace rendering {

NativeTextureRegistry::NativeTextureRegistry(NativeCUSTextureHistory& originalHistory): history(originalHistory)
{
	history.SetObjectResolvers([this](auto object){return Resolve(std::move(object));},
		[this](auto object){return RestoreSaved(std::move(object));});
}

NativeTextureRegistry::~NativeTextureRegistry() { history.SetObjectResolvers({}, {}); }

void NativeTextureRegistry::NewContext()
{
	if (context == std::numeric_limits<uint64_t>::max())
		throw std::runtime_error("Native texture registry context generation overflows");
	history.ResetForNewContext();
	objects.clear();
	++context;
	contextOpen = true;
	nextName = 1;
}

void NativeTextureRegistry::CloseContext() noexcept
{
	history.CloseContext();
	objects.clear();
	contextOpen = false;
}

std::shared_ptr<const NativeCUSTextureObject> NativeTextureRegistry::Create(
	NativeCUSTextureTarget target, std::string source, uint64_t name)
{
	if (!ContextGeneration() || target >= NativeCUSTextureTarget::Count || source.empty())
		throw std::runtime_error("Native texture creation lacks an actual context, target or source owner");
	if (!name) {
		while (objects.contains(nextName)) {
			if (nextName == std::numeric_limits<uint64_t>::max()) throw std::runtime_error("Native texture names overflow");
			++nextName;
		}
		name = nextName;
	}
	if (objects.contains(name)) throw std::runtime_error("Native texture creation would replace a live logical object");
	if (nextIdentity == std::numeric_limits<uint64_t>::max()) throw std::runtime_error("Native texture object identities overflow");
	auto object = std::make_shared<NativeCUSTextureObject>();
	object->identity = nextIdentity;
	object->contextGeneration = context;
	object->name = name;
	object->version = 1;
	object->target = target;
	object->source = std::move(source);
	object->storage = NativeTextureStorageKnowledge::Empty;
	objects.emplace(name, ObjectEntry{object,{},0});
	++nextIdentity;
	history.ObjectStateChanged();
	return object;
}

std::shared_ptr<const NativeCUSTextureObject> NativeTextureRegistry::Find(uint64_t name) const
{
	const auto it = objects.find(name);
	if(it==objects.end())return nullptr;
	auto& entry=it->second;
	if(entry.restoredContent){
		auto snapshot=std::make_shared<NativeCUSTextureObject>(*entry.current);
		snapshot->image=entry.restoredContent->image;snapshot->backing=entry.restoredContent->backing;
		snapshot->storage=entry.restoredContent->storage;snapshot->version=entry.restoredVersion;
		entry.current=std::move(snapshot);entry.restoredContent.reset();entry.restoredVersion=0;
	}
	return entry.current;
}

std::shared_ptr<const NativeCUSTextureObject> NativeTextureRegistry::Resolve(std::shared_ptr<const NativeCUSTextureObject> object) const
{
	if (!ContextGeneration() || !object || object->contextGeneration != context)
		throw std::runtime_error("Native texture binding has no current context owner");
	const auto current = Find(object->name);
	if (!current || current->identity != object->identity || current->target != object->target)
		throw std::runtime_error("Native texture binding refers to a deleted or replaced logical object");
	return current;
}

void NativeTextureRegistry::Bind(unsigned unit, uint64_t name, NativeCUSTextureTarget target)
{
	if (!ContextGeneration()) throw std::runtime_error("Native texture binding has no actual context");
	if (!name) { history.BindDefault(unit, target); return; }
	const auto object = Find(name);
	if (!object || object->target != target) throw std::runtime_error("Native texture binding lacks the actual matching object");
	history.Bind(unit, object);
}

void NativeTextureRegistry::Replace(std::shared_ptr<NativeCUSTextureObject> object)
{
	const auto prior = Resolve(object);
	if (prior->version == std::numeric_limits<uint64_t>::max()) throw std::runtime_error("Native texture version overflows");
	object->version = prior->version + 1;
	const auto name=object->name;
	objects[name] = ObjectEntry{std::move(object),{},0};
	history.ObjectStateChanged();
}

void NativeTextureRegistry::SetSampling(uint64_t name, const NativeTextureSampling& actualState)
{
	const auto prior = Find(name);
	if (!prior) throw std::runtime_error("Native sampler update has no actual named texture");
	auto next = std::make_shared<NativeCUSTextureObject>(*prior);
	next->sampling = actualState;
	Replace(std::move(next));
}

void NativeTextureRegistry::SetBoundSampling(unsigned unit, NativeCUSTextureTarget target, const NativeTextureSampling& actual)
{
 const auto binding=history.Binding(unit,target);
 if(binding.knowledge==NativeCUSBindingKnowledge::DefaultObject)history.ObserveDefaultSampling(target,actual);
 else if(binding.knowledge==NativeCUSBindingKnowledge::BoundObject)SetSampling(binding.object->name,actual);
 else throw std::runtime_error("Original sampler writer has an unobserved binding");
}

std::shared_ptr<const NativeCUSTextureObject> NativeTextureRegistry::ResolveRecordedName(std::shared_ptr<const NativeCUSTextureObject> recorded)
{
 if(!ContextGeneration()||!recorded||recorded->contextGeneration!=context)throw std::runtime_error("Original retained texture name crossed context lifetime");
 auto current=Find(recorded->name);
 if(!current)current=Create(recorded->target,recorded->source,recorded->name);
 if(current->target!=recorded->target)throw std::runtime_error("Original retained name was reused for another texture target");
 return current; // Bind of a reused name must not restore old pixels or parameters.
}
bool NativeTextureRegistry::LuaTexture(unsigned requestedUnit, const NativeTextureLookup& lookup)
{
 return LuaTexture(requestedUnit,[lookup]{return lookup;});
}
bool NativeTextureRegistry::LuaTexture(unsigned requestedUnit,const std::function<NativeTextureLookup()>& parse)
{
 if(!ContextGeneration())throw std::runtime_error("Original texture lookup has no actual context");
 NativeTextureLookup lookup;
 return history.LuaTextureLookup(requestedUnit,[&]{
  lookup=parse();
  if(lookup.shadowSampler&&!lookup.parsed)throw std::runtime_error("Failed original texture lookup cannot execute shadow Bind");
  if(lookup.object)lookup.object=Resolve(lookup.object);
  return std::make_pair(lookup.parsed,lookup.object);
 },[&](unsigned unit){
  if(!lookup.shadowSampler)return;
  const auto binding=history.Binding(unit,NativeCUSTextureTarget::Image2D);
  auto sampling=binding.object?binding.object->sampling:binding.defaultSampling;
  sampling.compareMode=0x884E;sampling.compareFunction=0x0203;sampling.depthMode=0x1909;
  SetBoundSampling(unit,NativeCUSTextureTarget::Image2D,sampling);
 });
}

void NativeTextureRegistry::PublishContent(uint64_t name, std::shared_ptr<const WorldTexture> image,
	std::shared_ptr<const NativeTextureBacking> backing)
{
	const auto prior = Find(name);
	if (!prior || prior->storage==NativeTextureStorageKnowledge::Empty || prior->storage==NativeTextureStorageKnowledge::Unobserved ||
		(!image && !backing) || (backing && (!backing->storageGeneration || !backing->contentVersion)))
		throw std::runtime_error("Native texture publication lacks its actual logical object or immutable content owner");
	auto next = std::make_shared<NativeCUSTextureObject>(*prior);
	next->image = std::move(image);
	next->backing = std::move(backing);
	next->storage = next->backing && next->backing->content==NativeTextureContent::AllocationOnly
		? NativeTextureStorageKnowledge::AllocatedUndefined : NativeTextureStorageKnowledge::Published;
	Replace(std::move(next));
}

std::shared_ptr<const NativeCUSTextureObject> NativeTextureRegistry::PublishRedefinedContent(
	std::shared_ptr<const NativeCUSTextureObject> expected, const NativeTextureAllocation& allocation,
	std::shared_ptr<const NativeTextureBacking> backing)
{
	const auto prior = Resolve(expected);
	if (prior != expected || !allocation.levels || !allocation.samples || !allocation.originalInternalFormat ||
		!allocation.width || !allocation.height || !allocation.depth || !backing ||
		backing->storageGeneration != prior->identity || prior->version == std::numeric_limits<uint64_t>::max() ||
		backing->contentVersion != prior->version + 1 || backing->content != NativeTextureContent::ProducerVersion)
		throw std::runtime_error("TexImage redefinition lacks its current object and complete immutable publication");
	auto next = std::make_shared<NativeCUSTextureObject>(*prior);
	next->version = prior->version + 1;
	next->allocation = allocation;
	next->image.reset(); next->backing = std::move(backing);
	next->storage = NativeTextureStorageKnowledge::Published;
	auto& entry = objects.at(prior->name);
	history.ObjectStateChanged(); // Overflow throws before any contents/cell is changed.
	entry.current = next; entry.restoredContent.reset(); entry.restoredVersion = 0;
	return next;
}

void NativeTextureRegistry::Allocate(uint64_t name, const NativeTextureAllocation& allocation)
{
	const auto prior=Find(name);
	if(!prior || !allocation.levels ||
		!allocation.samples || !allocation.originalInternalFormat)
		throw std::runtime_error("Native texture allocation lacks actual source storage metadata");
	auto next=std::make_shared<NativeCUSTextureObject>(*prior);
	next->allocation=allocation;
	next->image.reset();next->backing.reset();
	// TexImage with a zero extent is legal and leaves an incomplete image.
	// It is not an absent producer, a fatal error or a fabricated depth clear.
	next->storage=allocation.width&&allocation.height&&allocation.depth
		?NativeTextureStorageKnowledge::AllocatedUndefined:NativeTextureStorageKnowledge::Empty;
	Replace(std::move(next));
}

bool NativeTextureRegistry::CancelContentPublication(std::shared_ptr<const NativeCUSTextureObject> expected,
	std::shared_ptr<const NativeCUSTextureObject> previous) noexcept
{
	if (!expected || !previous || expected==previous || expected->contextGeneration != context || previous->contextGeneration != context ||
		expected->identity != previous->identity || expected->name != previous->name || expected->target != previous->target ||
		expected->allocation != previous->allocation)
		return false;
	const auto it=objects.find(expected->name);
	if(it==objects.end())return false;
	auto& entry=it->second;const auto& current=entry.current;
	const auto& content=entry.restoredContent?entry.restoredContent:current;
	const auto version=entry.restoredContent?entry.restoredVersion:current->version;
	if (current->identity != expected->identity || current->allocation != expected->allocation ||
		content->image != expected->image || content->backing != expected->backing || content->storage != expected->storage ||
		version==std::numeric_limits<uint64_t>::max())
		return false;
	// Sampler changes are real source events, even if this image producer was
	// not submitted. Restore only image content, never object identity or state.
	entry.restoredContent=std::move(previous);entry.restoredVersion=version+1;
	// Cancellation is not an original GL writer event. In particular this
	// noexcept path must not allocate or call the throwing event-counter code.
	return true;
}

bool NativeTextureRegistry::Delete(uint64_t name)
{
	const auto object = Find(name);
	if (!object) return false; // Original unused/zero names are ignored.
	history.DeletedObject(object->identity);
	objects.erase(name);
	return true;
}

std::shared_ptr<const NativeCUSTextureObject> NativeTextureRegistry::RestoreSaved(std::shared_ptr<const NativeCUSTextureObject> saved)
{
	if (!saved || saved->contextGeneration != context)
		throw std::runtime_error("Native texture attribute restore crosses a context lifetime");
	auto object = Find(saved->name);
	// Original compatibility PopAttrib restores a NAME, not the old deleted
	// storage. A reused name resolves to its new object; unused names recreate.
	if (!object) object = Create(saved->target, saved->source, saved->name);
	if (object->target != saved->target)
		throw std::runtime_error("Original saved texture name was reused for another target");
	SetSampling(saved->name, saved->sampling);
	return Find(saved->name);
}

NativeGeometryTextureNames::NativeGeometryTextureNames(NativeTextureRegistry& r, NativeCUSTextureHistory& h, std::string originalOwner):
	registry(r), history(h), owner(std::move(originalOwner))
{
	if (owner.empty()) throw std::runtime_error("Native GBuffer lifecycle needs its actual original owner");
}

bool NativeGeometryTextureNames::Update(uint32_t x, uint32_t y, NativeCUSTextureTarget actualTarget, uint32_t actualSamples, bool combinedColor)
{
	if (!registry.ContextGeneration() || !history.ActiveUnit() || !actualSamples ||
		(actualTarget!=NativeCUSTextureTarget::Image2D&&actualTarget!=NativeCUSTextureTarget::Image2DMultisample) ||
		(actualTarget==NativeCUSTextureTarget::Image2D&&actualSamples!=1))
		throw std::runtime_error("Native GBuffer creation lacks the original extent/context/active unit");
	if (context != registry.ContextGeneration()) { names = {}; width = height = 0; context = registry.ContextGeneration(); }
	// msaa is fixed by original GeometryBuffer::Init, not changed each Update.
	if(names[0]&&(target!=actualTarget||samples!=actualSamples))throw std::runtime_error("Native GBuffer target changed without original reinitialization");
	if (names[0] && HasCombinedColor()!=combinedColor) throw std::runtime_error("Native GBuffer optional color changed without original reinitialization");
	if (combinedColor && (actualTarget!=NativeCUSTextureTarget::Image2D || actualSamples!=1)) throw std::runtime_error("Combined ground color requires the original single-sample allocation");
	if (names[0] && width == x && height == y) return false; // Original Update emits no event.
	Detach();
	target=actualTarget;samples=actualSamples;
	NativeTextureSampling sampling;
	sampling.wrapS = sampling.wrapT = 0x812D; // CLAMP_TO_BORDER, default border zero
	sampling.minFilter = sampling.magFilter = 0x2601; // LINEAR
	// Original GL order is material0..4, optional color, depth. Preserve that
	// event order while keeping the native depth name at index5.
	for (unsigned slot = 0; slot < AttachmentCount+unsigned(combinedColor); ++slot) {
		const unsigned i=combinedColor?(slot==5?CombinedColorAttachment:slot==6?DepthAttachment:slot):slot;
		auto object = registry.Create(target, owner + ":attachment" + std::to_string(i));
		names[i] = object->name;
		registry.Bind(*history.ActiveUnit(), object->name, object->target);
		// Original TexParameter wrap/filter calls on multisample objects are
		// rejected by GL and leave those parameters at their initial values.
		if(target==NativeCUSTextureTarget::Image2D)registry.SetSampling(object->name, sampling);
		// Original unsized GL_RGBA resolves through actual storage producer;
		// preserve the requested format rather than inventing uploaded pixels.
		registry.Allocate(object->name,{x,y,1,1,samples,i==DepthAttachment?0x8CACu:(target==NativeCUSTextureTarget::Image2D?0x1908u:0x8058u)});
	}
	// GeometryBuffer::Create ends with BindTexture(target,0) on the inherited
	// CURRENT unit. It does not leave its last depth image bound or select unit0.
	history.BindDefault(*history.ActiveUnit(), target);
	width = x; height = y;
	return true;
}

void NativeGeometryTextureNames::Detach()
{
	if (context && context == registry.ContextGeneration()) for (const uint64_t name: names) registry.Delete(name);
	names = {}; width = height = 0;
}

void NativeGeometryTextureNames::ForgetNames() noexcept
{
	names = {}; width = height = 0; samples = 0;
	context = registry.ContextGeneration();
}

uint64_t NativeGeometryTextureNames::Name(unsigned attachment) const
{
	if (attachment > CombinedColorAttachment) throw std::runtime_error("Native GBuffer attachment index differs from original");
	return context == registry.ContextGeneration() ? names[attachment] : 0;
}

} // namespace rendering
