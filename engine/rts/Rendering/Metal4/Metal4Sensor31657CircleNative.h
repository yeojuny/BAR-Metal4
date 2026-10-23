/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4StartBoxNative.h"
#include "Rendering/Backend/NativeSensor31657CircleData.h"
namespace rendering { struct WorldCamera; }
namespace rendering::metal4 {
class PreparedSensor31657CircleDraw {
public:
	~PreparedSensor31657CircleDraw();
	void Encode(id<MTL4RenderCommandEncoder>) const;
	bool Empty() const;
private:
	PreparedSensor31657CircleDraw();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class Sensor31657CircleGPU;
};
// Actual current31657 Radar/Sonar typed family. It shares the context's map
// resource resolver and normal RuntimeLease resource publication/retirement.
class Sensor31657CircleGPU {
public:
	Sensor31657CircleGPU();~Sensor31657CircleGPU();
	void Initialize(RuntimeLease&,double& shaderMilliseconds);
	std::shared_ptr<const void> CompiledFamilyLifetime() const;
	void ClearResources(); // After the original source/frame has retired.
	std::unique_ptr<PreparedSensor31657CircleDraw> Prepare(RuntimeLease&,const NativeSensor31657CircleDraw&,
		const StartBoxImageResolver&,const WorldCamera&,uint32_t renderWidth,uint32_t renderHeight,
		uint32_t logicalWidth,uint32_t logicalHeight,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
		unsigned samples,bool validation,double& shaderMilliseconds,
		std::optional<NativePostprocessConsumer> consumer=std::nullopt);
private:
	struct Impl;std::shared_ptr<Impl> impl;
};
}
