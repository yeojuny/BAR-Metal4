/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeModelDepthPlan.h"
#include "Rendering/Backend/NativeWorldMaterialData.h"
#include <set>
#include <stdexcept>
namespace rendering {
NativeModelDepthPlan BuildNativeModelDepthPlan(const WorldFrame& frame,const NativeModelDepthSelection& selection)
{
	NativeModelDepthPlan result;std::set<std::string> pending;
	// LuaObjectDrawer::DrawDeferredPass returns without clearing on these
	// actual source gates. A retained named image is not a fresh empty pass.
	if(!selection.drawDeferredEnabled||!selection.geometryBufferValid||!selection.canDrawDeferred)return result;
	if(selection.unitsDeferred)result.phases.push_back({false,true});
	if(selection.featuresDeferred)result.phases.push_back({true,!selection.unitsDeferred||selection.allowSecondClear});
	const auto append=[&](const auto& object,std::uint32_t index,std::int32_t id,bool feature){
		// Already captured original drawFlag/engineDrawMask, LOS and ownership.
		// Do not rerun ShouldDraw* (it mutates original Lua material queues).
		if(!object.opaque)return;
		if(!object.model||!object.material||object.pieces.size()!=object.model->pieces.size())throw std::invalid_argument("Original model GBuffer captured model/material/pose ownership differs");
		NativeModelDepthObject output;output.feature=feature;output.sourceIndex=index;output.objectId=id;
		const auto& material=*object.material;
		output.alpha=material.engineStandardGL4?NativeModelDepthAlpha::EngineGL4:material.family==WorldMaterialFamily::EngineModel?NativeModelDepthAlpha::EngineLegacy:material.alphaShadows?NativeModelDepthAlpha::CUSAlphaMask:NativeModelDepthAlpha::CUSOpaque;
		for(std::uint32_t piece=0;piece<object.pieces.size();++piece)if(object.pieces[piece].visible&&!object.model->pieces[piece].indices.empty())output.pieces.push_back(piece);
		if(output.pieces.empty())return;
		if(!material.engineStandardGL4&&material.family!=WorldMaterialFamily::EngineModel){
			if(material.healthDisplacement)pending.insert("Original full CUS health/noise displacement must feed this same depth vertex family");
			if(feature)pending.insert("Original CUS feature squared-distance scale fade must feed this same depth vertex family");
			else pending.insert("Original CUS construction/deferred fragment coverage requires captured build state and its separate full-shader gate");
		}
		result.objects.push_back(std::move(output));
	};
	if(selection.unitsDeferred)for(std::uint32_t i=0;i<frame.units.size();++i)append(frame.units[i],i,frame.units[i].unitId,false);
	if(selection.featuresDeferred)for(std::uint32_t i=0;i<frame.features.size();++i)append(frame.features[i],i,frame.features[i].featureId,true);
	result.pendingOriginalGeometry.assign(pending.begin(),pending.end());return result;
}
}
