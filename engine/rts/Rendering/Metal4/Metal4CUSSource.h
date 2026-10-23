/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeCUSSourceData.h"
#include "Rendering/Backend/WorldFrameData.h"
#include <functional>

namespace rendering::metal4 {
struct CUSSourceTarget {
 std::array<MTLPixelFormat,5> colors{}; // Actual original material draw buffers.
 MTLPixelFormat combinedColor=MTLPixelFormatInvalid; // Optional original Map color5; no depth renumbering.
 MTLPixelFormat depth=MTLPixelFormatInvalid,stencil=MTLPixelFormatInvalid;
 unsigned samples=1,width=0,height=0,logicalWidth=0,logicalHeight=0;
 WorldCamera camera; // Raster viewport only; shader matrices come from publication.
 bool invertClipY=false;
 float shadowPolygonOffsetUnitsScale=1.f; // Original shadow destination only.
};

class PreparedCUSSource {
public:
 ~PreparedCUSSource();
 size_t DrawCount()const;
 uint64_t SequenceAt(size_t)const;
 void EncodeDependencies(id<MTL4CommandBuffer>);
 void EncodeDraw(size_t,id<MTL4RenderCommandEncoder>);
 void DidComplete();
private:
 struct Impl;std::unique_ptr<Impl> impl;
 PreparedCUSSource();friend class CUSSourceGPU;
};
class CUSSourceGPU {
public:
 using TextureProvider=std::function<CUSSourceImage(RuntimeLease&,const NativeCUSTextureBinding&,unsigned originalSamplerSlot)>;
 CUSSourceGPU();~CUSSourceGPU();
 // Original program construction has no framebuffer/target argument. This
 // compiles BOTH exact original native stages and verifies their entrypoints;
 // actual target/blend/mask PSOs are compiled by Prepare for the real draw.
 std::shared_ptr<const NativeCUSProgramConsumer> LinkOriginalStages(RuntimeLease&,
  const NativeCUSProgramSource&,double& compileMilliseconds);
 // Existing callers that already own a genuine target may also prime its PSO.
 // This does not make that target the source program's framebuffer identity.
 std::shared_ptr<const NativeCUSProgramConsumer> CompileProgram(RuntimeLease&,const NativeCUSProgramSource&,
  const CUSSourceTarget&,bool validation,double& compileMilliseconds);
 std::unique_ptr<PreparedCUSSource> Prepare(RuntimeLease&,std::shared_ptr<const NativeCUSSourceFrame>,
  const CUSSourceTarget&,const TextureProvider&,bool validation,double& compileMilliseconds);
 void ClearResources(); // Original context/game retirement owner only.
private:
 struct Impl;std::unique_ptr<Impl> impl;
};
} // namespace rendering::metal4
