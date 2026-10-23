/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4StartBoxNative.h"
#include "Rendering/Backend/NativeCommandsData.h"
namespace rendering { struct WorldCamera; }
namespace rendering::metal4 {
class PreparedCommands {
public:
	~PreparedCommands();
	size_t DrawCount()const;
	uint64_t SequenceAt(size_t)const;
	void EncodeDraw(size_t,id<MTL4RenderCommandEncoder>)const;
private:
	PreparedCommands();struct Impl;std::unique_ptr<Impl> impl;friend class CommandsGPU;
};
class CommandsGPU {
public:
	CommandsGPU();~CommandsGPU();
	void Initialize(RuntimeLease&,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& shaderMilliseconds);
	void ClearResources();
	std::unique_ptr<PreparedCommands> Prepare(RuntimeLease&,std::shared_ptr<const NativeCommandsFrame>,
		const StartBoxImageResolver&,const WorldCamera&,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,
		MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& shaderMilliseconds);
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
