/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include <memory>
namespace rendering::metal4 {
class EngineShadowGPU;
class PreparedGenericShadow {
public:
	~PreparedGenericShadow();
	size_t DrawCount()const;uint64_t SequenceAt(size_t)const;
	void EncodeDependencies(id<MTL4CommandBuffer>);
	void EncodeDraw(size_t,id<MTL4RenderCommandEncoder>);
	void Finish();void DidComplete();
private:
	PreparedGenericShadow();struct Impl;std::unique_ptr<Impl> impl;
	friend class EngineShadowGPU;
};
}
