/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4StartBoxNative.h"
#include "Rendering/Backend/NativeSensorLOSData.h"
namespace rendering { struct WorldCamera; }
namespace rendering::metal4 {
class PreparedSensorLOSDraw {
public:
	~PreparedSensorLOSDraw();
	void Encode(id<MTL4RenderCommandEncoder>) const;
	bool Empty() const;
private:
	PreparedSensorLOSDraw();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class SensorLOSGPU;
};
// Actual current31657 Radar/Sonar typed family. It shares the context's map
// resource resolver and normal RuntimeLease resource publication/retirement.
class SensorLOSGPU {
public:
	SensorLOSGPU();~SensorLOSGPU();
	void Initialize(RuntimeLease&,double& shaderMilliseconds);
	std::shared_ptr<const void> CompiledFamilyLifetime() const;
	void ClearResources(); // After the original source/frame has retired.
	std::unique_ptr<PreparedSensorLOSDraw> Prepare(RuntimeLease&,const NativeSensorLOSDraw&,
		const StartBoxImageResolver&,const WorldCamera&,uint32_t renderWidth,uint32_t renderHeight,
		uint32_t logicalWidth,uint32_t logicalHeight,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
		unsigned samples,bool validation,double& shaderMilliseconds,
		std::optional<NativePostprocessConsumer> consumer=std::nullopt);
private:
	struct Impl;std::shared_ptr<Impl> impl;
};
}
