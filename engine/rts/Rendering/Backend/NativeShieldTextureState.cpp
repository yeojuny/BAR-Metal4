/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeShieldTextureState.h"
#include "Rendering/Backend/NativeUITextureHistory.h"
#include "NativeShieldDepthBacking.h"
#include <cmath>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool ok,const char* why){if(!ok)throw std::invalid_argument(why);}
}
void NativeShieldTextureContext::Validate() const
{
	Check(history&&registry&&map&&models,"Original Shield requires the shared texture context owner");
	Check(registry->MatchesHistory(*history)&&registry->ContextGeneration()!=0,"Original Shield texture context differs from its registry");
}
bool NativeShieldTextureContext::Texture(unsigned unit,bool enabled) const
{
	Validate();Check(unit<2,"Original Shield texture unit differs");
	const auto actualUnit=unit?unit:history->ActiveUnit().value_or(NativeCUSTextureHistory::Units);
	const auto result=history->LuaTexture(unit,enabled); // false changes enable only.
	if(syncObservedBinding)syncObservedBinding(actualUnit);return result;
}
bool NativeShieldTextureContext::Texture(unsigned unit,const std::string& name) const
{
	Validate();Check(unit<2,"Original Shield texture unit differs");
	Check(name==(unit==0?"$map_gbuffer_zvaltex":"$model_gbuffer_zvaltex"),"Original Shield requires its exact named GBuffer source");
	const auto* names=unit==0?map:models;
	const auto actualUnit=unit?unit:history->ActiveUnit().value_or(NativeCUSTextureHistory::Units);
	// ParseTextureImage succeeds for this exact name even when GetTextureID=0.
	// LuaMatTexture::Bind then leaves the CURRENT binding untouched.
	const auto result=registry->LuaTexture(unit,{true,registry->Find(names->Name(NativeGeometryTextureNames::DepthAttachment)),false});
	if(syncObservedBinding)syncObservedBinding(actualUnit);return result;
}
NativeShieldTextureKind ClassifyNativeShieldTexture(const NativeCUSTextureBinding& binding)
{
	if(binding.knowledge==NativeCUSBindingKnowledge::DefaultObject){
		Check(binding.defaultStorageKnownEmpty&&!binding.object,"Original Shield default texture storage was not observed");
		return NativeShieldTextureKind::Empty; // Original incomplete object0: sampled red=0.
	}
	Check(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object,"Original Shield sampler binding was not observed");
	const auto& object=*binding.object;
	Check(object.identity&&object.contextGeneration&&object.name&&object.version&&object.target==NativeCUSTextureTarget::Image2D,"Original Shield sampler is not an actual Image2D object");
	if(ui::NativeUITextureHistory::SamplingIncomplete(binding))return NativeShieldTextureKind::Empty;
	Check(object.storage!=NativeTextureStorageKnowledge::Unobserved,"Original Shield bound image storage was not observed");
	if(object.backing){
		const auto kind=object.backing->Kind();
		if(kind==NativeTextureBackingKind::MapDepth||kind==NativeTextureBackingKind::ModelDepth){
			Check(object.backing->storageGeneration&&object.backing->contentVersion&&!object.image,"Original Shield depth backing lacks its immutable storage/content identity");
			const auto actual=std::dynamic_pointer_cast<const NativeShieldDepthBacking>(object.backing);
			Check(actual&&object.allocation.width==actual->Metadata().width&&object.allocation.height==actual->Metadata().height&&object.allocation.depth==1&&object.allocation.levels==1&&object.allocation.samples==1&&object.allocation.originalInternalFormat==0x8CAC,"Original Shield named-depth backing differs from its actual GeometryBuffer storage allocation");
			return NativeShieldTextureKind::Depth;
		}
		Check(kind==NativeTextureBackingKind::ColorImage,"Original Shield does not accept scene/shadow depth disguised as a GBuffer");
		return NativeShieldTextureKind::Color;
	}
	Check(bool(object.image)&&object.storage==NativeTextureStorageKnowledge::Published,"Original Shield allocated storage lacks its actual native backing");
	return NativeShieldTextureKind::Color;
}
void ValidateNativeShieldTextureSnapshot(const NativeShieldTextureSnapshot& snapshot,unsigned effects)
{
	Check(snapshot.contextGeneration!=0,"Original Shield texture snapshot has no actual context");
	for(unsigned unit=0;unit<2;++unit){
		if(!(effects&(2u<<unit)))continue;
		const auto& binding=snapshot.samplers[unit];
		ClassifyNativeShieldTexture(binding);
		if(binding.object)Check(binding.object->contextGeneration==snapshot.contextGeneration,"Original Shield sampler crosses a texture context lifetime");
		const auto& sampling=binding.object?binding.object->sampling:binding.defaultSampling;
		for(float value:sampling.borderColor)Check(std::isfinite(value),"Original Shield sampler border is nonfinite");
		Check(std::isfinite(sampling.minLod)&&std::isfinite(sampling.maxLod)&&std::isfinite(sampling.lodBias)&&std::isfinite(sampling.anisotropy),"Original Shield sampler state is nonfinite");
	}
}
std::shared_ptr<const NativeShieldTextureSnapshot> NativeShieldTextureContext::Capture(unsigned effects) const
{
	Validate();auto result=std::make_shared<NativeShieldTextureSnapshot>();
	result->contextGeneration=registry->ContextGeneration();result->eventSequence=history->Sequence();
	for(unsigned unit=0;unit<2;++unit)result->samplers[unit]=history->Binding(unit,NativeCUSTextureTarget::Image2D);
	ValidateNativeShieldTextureSnapshot(*result,effects);return result;
}
} // namespace rendering
