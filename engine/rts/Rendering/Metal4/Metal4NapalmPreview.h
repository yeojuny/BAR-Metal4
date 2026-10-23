/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4ScreenNative.h"
#include "Rendering/Backend/NativeNapalmPreviewData.h"
namespace rendering::metal4 {
class NapalmPreviewGPU;
class PreparedNapalmPreview {
public:
 ~PreparedNapalmPreview();
 std::size_t DrawCount() const;
 std::uint64_t SequenceAt(std::size_t) const;
 void EncodeDraw(std::size_t,id<MTL4RenderCommandEncoder>) const;
private:
 PreparedNapalmPreview();struct Impl;std::unique_ptr<Impl>impl;
 friend class NapalmPreviewGPU;
};
// Original fan/strip procedural fragment consumer. It shares actual scene
// attachments and command order; no private texture/pass/submission/present.
class NapalmPreviewGPU {
public:
 NapalmPreviewGPU();~NapalmPreviewGPU();
 void Initialize(RuntimeLease&,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& shaderMilliseconds);
 std::unique_ptr<PreparedNapalmPreview> Prepare(RuntimeLease&,std::shared_ptr<const NativeNapalmPreviewFrame>,const WorldCamera&,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,unsigned samples,bool validation,double& shaderMilliseconds);
 void ClearResources();
private:
 struct Impl;std::unique_ptr<Impl>impl;
};
}
