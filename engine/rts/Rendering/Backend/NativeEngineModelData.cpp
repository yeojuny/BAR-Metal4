/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeEngineModelData.h"
#include <cmath>
#include <stdexcept>

namespace rendering {
std::vector<NativeEngineModelDraw> NativeEngineBuildStages(const NativeEngineBuildInput& in)
{
	if (!std::isfinite(in.height)||!std::isfinite(in.progress))
		throw std::runtime_error("Engine build stage contains non-finite source bounds");
	const float wire=std::fabs(128.0f-((in.simulationFrame*4)&255))/255.0f+0.5f;
	const float flat=1.5f-wire;
	const float h=in.height,p=in.progress;
	const std::array<std::array<float,4>,3> upper{{
		{0,-1,0,h*(p*3)}, {0,-1,0,h*(p*3-1)}, {0,-1,0,h*(p*3-2)}}};
	const std::array<std::array<float,4>,2> lower{{
		{0,1,0,-h*(p*10-9)}, {0,1,0,-h*(p*3-2)}}};
	std::vector<NativeEngineModelDraw> result;
	auto stage=[&](float multiplier,bool lines,unsigned bound,bool lowerEnabled,bool offset) {
		NativeEngineModelDraw d; d.shader=in.shader;d.wireframe=lines;
		d.nanoColor={in.nanoColor[0]*multiplier,in.nanoColor[1]*multiplier,in.nanoColor[2]*multiplier,1};
		d.modelClipPlanes[0]=upper[bound];
		if(lowerEnabled)d.modelClipPlanes[1]=lower[bound];
		d.polygonOffset=offset;d.polygonOffsetFactor=offset?1:0;d.polygonOffsetUnits=offset?1:0;
		result.push_back(d);
	};
	stage(wire,true,0,true,false);
	if(in.shader==NativeEngineModelShader::Legacy && in.amdHacks)result.back().modelClipPlanes={};
	if(p>1.0f/3.0f)stage(flat,false,in.shader==NativeEngineModelShader::GL4?1:0,true,false);
	if(p>2.0f/3.0f) {
		// GL4's original fill uses FLAT upper bound; legacy uses FILL.
		stage(1,false,in.shader==NativeEngineModelShader::GL4?1:2,false,true);
		result.back().nanoColor={1,1,1,0};
		if(in.shader==NativeEngineModelShader::Legacy && in.amdHacks)result.back().modelClipPlanes={};
	}
	return result;
}
void ValidateNativeEngineModelDraw(const NativeEngineModelDraw& d)
{
	if(!d.object||!d.object->model||d.object->pieces.size()!=d.object->model->pieces.size())
		throw std::runtime_error("Original engine model draw has no immutable geometry/pose");
	if(!d.originalStateObserved||!d.compiledOriginalProgram||!d.program.observed||d.program.modern!=(d.shader==NativeEngineModelShader::GL4))
  throw std::runtime_error("Original engine draw has no actual shader/state publication");
 if(d.program.cameraMode<0||d.program.cameraMode>2||(!d.program.modern&&d.program.cameraMode!=0)||d.program.shadingMode!=0)
  throw std::runtime_error("Original engine capture does not own this camera/shading mode");
 if(d.sourceWorld.frontPolygonMode!=d.sourceWorld.backPolygonMode||d.sourceWorld.frontPolygonMode==NativePolygonMode::Point)
  throw std::runtime_error("Original engine model polygon mode requires a genuine unsupported consumer");
 if(d.shader==NativeEngineModelShader::GL4){
  if(!d.pose.transforms||!d.pose.modelUniforms||!d.pose.engineUniforms)
   throw std::runtime_error("Original GL4 model immutable upload owners are missing");
  ValidateNativeCUSUniformPublication(*d.pose.engineUniforms);
		if((d.pose.arrayModel&&!d.pose.staticModel)||d.program.matrixMode!=(d.pose.arrayModel?2:(d.pose.staticModel?1:0)))throw std::runtime_error("Original engine GL4 matrix mode has no corresponding source pose owner");
		if(d.object->model->gl4BoneLayout==WorldModelGL4BoneLayout::Unclassified)
			throw std::runtime_error("Original GL4 model geometry has no bone-layout classification");
		if(d.object->model->gl4BoneLayout!=WorldModelGL4BoneLayout::RigidPerPiece)
			throw std::runtime_error("Original engine weighted-bone format requires its genuine complete skinning consumer");
 }
 for(const auto& p:d.storedClipPlanes)for(float x:p)if(!std::isfinite(x))throw std::runtime_error("Original stored model clip plane is not finite");
 for(float x:d.nanoColor)if(!std::isfinite(x))throw std::runtime_error("Engine model nano color is not finite");
	for(const auto& p:d.modelClipPlanes)for(float x:p)if(!std::isfinite(x))throw std::runtime_error("Engine model clip plane is not finite");
}
} // namespace rendering
