/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeTerrainSourceData.h"
#include "WorldFrameData.h"
#include "NativeSMFTextureData.h"
#include <cmath>
#include <set>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool valid,const char* why){if(!valid)throw std::runtime_error(why);}
size_t Target(NativeCUSTextureTarget value){
	Check(value<NativeCUSTextureTarget::Count,"Original terrain texture target is invalid");return size_t(value);
}
void Object(const NativeCUSTextureObject& value,uint64_t context,NativeCUSTextureTarget target){
	Check(value.contextGeneration==context&&value.identity&&value.name&&value.version&&value.target==target&&!value.source.empty(),
		"Original terrain texture identity/lifetime differs");
	ValidateNativeTerrainSampling(value.sampling);
	if(value.backing)Check(value.backing->storageGeneration&&value.backing->contentVersion,
		"Original terrain backing has no immutable storage/content generation");
	if(value.image)Check(!value.image->mips.empty()&&value.image->mips.front().width==value.allocation.width&&
		value.image->mips.front().height==value.allocation.height&&value.image->mips.size()<=value.allocation.levels,
		"Original terrain retained image differs from its actual allocation");
}
}
void ValidateNativeTerrainProgram(const NativeTerrainProgramIdentity& identity,
	const std::shared_ptr<const NativeTerrainProgramConsumer>& consumer,uint64_t context)
{
	Check(context&&identity.contextGeneration==context&&identity.originalProgram&&identity.originalProgramGeneration&&
		identity.sourceRevision&&!identity.originalName.empty()&&identity.vertexSourceHash&&identity.fragmentSourceHash,
		"Original terrain source program identity is missing or from another context");
	switch(identity.selection){
		case NativeTerrainProgramSelection::ForwardStandard:case NativeTerrainProgramSelection::ForwardAdvanced:
		case NativeTerrainProgramSelection::DeferredAdvanced:case NativeTerrainProgramSelection::LuaForward:
		case NativeTerrainProgramSelection::LuaDeferred:case NativeTerrainProgramSelection::CombinedAdvanced:
		case NativeTerrainProgramSelection::Border:case NativeTerrainProgramSelection::Composite:break;
		default:throw std::runtime_error("Original terrain selected an unknown shader variant");
	}
	std::set<std::string> names;
	for(const auto& [name,value]:identity.defines){
		Check(!name.empty()&&name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")==std::string::npos&&
			name.find_first_of("0123456789")!=0&&names.insert(name).second&&value.find('\0')==std::string::npos,
			"Original terrain compile definitions are malformed or duplicated");
	}
	Check(consumer&&consumer->ContextGeneration()==context&&consumer->LibraryGeneration(),
		"Original terrain has no actual same-context compiled terrain consumer");
	Check(consumer->Matches(identity),"Compiled terrain owner belongs to another original program/source generation");
	Check(consumer->Supports(identity),"Actual selected original terrain program variant is unsupported by its compiled consumer");
}
void ValidateNativeTerrainSampling(const NativeTextureSampling& sampling)
{
	for(float value:sampling.borderColor)Check(std::isfinite(value),"Original terrain border color is nonfinite");
	for(float value:{sampling.minLod,sampling.maxLod,sampling.lodBias,sampling.anisotropy})
		Check(std::isfinite(value),"Original terrain texture parameter is nonfinite");
	Check(sampling.anisotropy>=1&&sampling.baseLevel>=0&&sampling.maxLevel>=0,
		"Original terrain texture parameter is outside the original source domain");
}
NativeTerrainImageSnapshot CaptureNativeTerrainImages(const NativeTextureRegistry& registry,
	const NativeCUSTextureHistory& history,uint64_t frame)
{
	Check(registry.ContextGeneration()&&registry.MatchesHistory(history),"Terrain capture lacks the actual shared texture context");
	NativeTerrainImageSnapshot result;result.contextGeneration=registry.ContextGeneration();
	result.sourceDrawFrame=frame;result.eventSequence=history.Sequence();result.activeUnit=history.ActiveUnit();
	for(unsigned unit=0;unit<result.units.size();++unit)for(size_t target=0;target<result.units[unit].size();++target)
		result.units[unit][target]=history.Binding(unit,NativeCUSTextureTarget(target));
	ValidateNativeTerrainImages(result,registry.ContextGeneration(),frame);return result;
}
void ValidateNativeTerrainImages(const NativeTerrainImageSnapshot& snapshot,uint64_t context,uint64_t frame)
{
	Check(context&&snapshot.contextGeneration==context&&snapshot.sourceDrawFrame==frame&&snapshot.eventSequence&&
		snapshot.activeUnit&&*snapshot.activeUnit<NativeCUSTextureHistory::Units,
		"Original terrain image history lacks its actual context/frame/active unit");
	for(const auto& unit:snapshot.units)for(size_t target=0;target<unit.size();++target){
		const auto& binding=unit[target];
		switch(binding.knowledge){
			case NativeCUSBindingKnowledge::Unobserved:
				Check(!binding.object,"Unobserved terrain texture binding has a fabricated object");break;
			case NativeCUSBindingKnowledge::DefaultObject:
				Check(!binding.object,"Original terrain default-object binding has a named object");
				ValidateNativeTerrainSampling(binding.defaultSampling);break;
			case NativeCUSBindingKnowledge::BoundObject:
				Check(bool(binding.object),"Original terrain named binding lost its object owner");
				Object(*binding.object,context,NativeCUSTextureTarget(target));break;
			default:throw std::runtime_error("Original terrain binding knowledge is invalid");
		}
	}
}
const NativeCUSTextureBinding& RequireNativeTerrainImage(const NativeTerrainImageSnapshot& snapshot,unsigned unit,NativeCUSTextureTarget target)
{
	Check(snapshot.contextGeneration&&snapshot.eventSequence&&snapshot.activeUnit&&unit<snapshot.units.size(),
		"Original terrain sampler lacks its actual captured context/history/unit");
	const auto& binding=snapshot.units[unit][Target(target)];
	Check(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object,
		"Original terrain sampled default/unobserved storage requires its actual supported image owner");
	Object(*binding.object,snapshot.contextGeneration,target);
	Check(binding.object->storage==NativeTextureStorageKnowledge::Published&&(binding.object->image||binding.object->backing),
		"Original terrain sampled image has no actual published source storage");
	return binding; // GLSL sampling is independent of fixed-function enable state.
}
void ValidateNativeTerrainSquareBacking(const NativeTerrainSquareBacking& backing)
{
	Check(backing.tiles&&backing.storageGeneration&&backing.contentVersion&&backing.content==NativeTextureContent::ProducerVersion,
		"Original terrain square lacks its actual retained tile/content owner");
	const auto& tiles=*backing.tiles;
	// Full payload/index validation belongs to the existing renderer Map owner
	// transition before UploadNativeSMFSquares, not each square publication.
	Check(tiles.gridWidth&&tiles.gridHeight&&tiles.gridWidth%NATIVE_SMF_TILES_PER_SQUARE==0&&
		tiles.gridHeight%NATIVE_SMF_TILES_PER_SQUARE==0&&backing.squareX<tiles.gridWidth/NATIVE_SMF_TILES_PER_SQUARE&&
		backing.squareZ<tiles.gridHeight/NATIVE_SMF_TILES_PER_SQUARE,
		"Original terrain square coordinates differ from the retained SMF tile grid");
	Check(backing.firstSourceMip<WorldTerrainTiles::MIP_TEXELS.size()&&backing.sourceMipCount&&
		backing.sourceMipCount<=WorldTerrainTiles::MIP_TEXELS.size()-backing.firstSourceMip,
		"Original terrain square source mip interval is invalid");
}
} // namespace rendering
