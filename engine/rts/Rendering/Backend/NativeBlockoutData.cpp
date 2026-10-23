/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeBlockoutData.h"
#include "NativeS3OTextureSource.h"
#include <cmath>
#include <stdexcept>
namespace rendering {
namespace {
std::atomic<bool> available=false;
void Check(bool value,const char* why){if(!value)throw std::logic_error(why);}
}
void ValidateNativeBlockoutDraw(const NativeBlockoutDraw& draw)
{
	Check(draw.sequence&&draw.uniforms&&draw.textures,"Original Blockout lacks actual source order/UBO/material");
	ValidateNativeCUSUniformPublication(*draw.uniforms);
	for(unsigned n=0;n<2;++n)(void)NativeS3OTexturePixels(*draw.textures,n);
	Check(draw.clipDepth!=ClipDepthConvention::Unsupported,"Original Blockout clip convention is unknown");
	Check(draw.world.depthTest&&draw.world.depthWrite&&draw.world.depthCompare==ScreenAlphaCompare::LessEqual&&!draw.world.cullEnabled,
		"Original Blockout opaque depth/cull state differs");
	Check(!draw.hasNormal||bool(draw.normal),"Original Blockout selected normal sampler has no image");
	for(float x:draw.viewProjection)Check(std::isfinite(x),"Original Blockout projection is nonfinite");
	for(const auto& model:draw.models){
		Check(model.definition>=0&&model.model&&!model.transforms.empty(),"Original Blockout FeatureDef model/transform absent");
		for(float x:model.positionYaw)Check(std::isfinite(x),"Original Blockout position/yaw is nonfinite");
		Check(model.positionYaw[1]==0&&model.parameters[0]>=.25f&&model.parameters[0]<=4.f,
			"Original Blockout sea-level/scale contract differs");
		for(const auto& piece:model.model->pieces)for(const auto& vertex:piece.vertices)
			Check((vertex.boneIds[0]&255)<model.transforms.size(),"Original Blockout piece transform is out of range");
	}
}
void ValidateNativeBlockoutDepthPass(const NativeBlockoutDepthPass& pass)
{
	Check(pass.sequence&&pass.target&&pass.target->storage&&pass.target->width&&pass.target->height&&pass.output,
		"Original Blockout depth pass has no actual target/output");
	Check(pass.output->image&&pass.output->image->format==NativePostprocessFormat::Depth24Unorm&&
		pass.output->image->width==pass.target->width&&pass.output->image->height==pass.target->height&&pass.clearObserved&&pass.clearDepth==1,
		"Original Blockout depth allocation/clear differs");
	for(const auto& draw:pass.draws)ValidateNativeBlockoutDraw(draw);
}
void SetNativeBlockoutConsumerAvailable(bool value) noexcept{available.store(value,std::memory_order_release);}
bool IsNativeBlockoutConsumerAvailable() noexcept{return available.load(std::memory_order_acquire);}
}
