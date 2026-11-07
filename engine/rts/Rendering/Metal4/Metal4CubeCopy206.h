/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4CubeTextureSource.h"
#include "Rendering/Backend/NativeWorldDrawData.h"
namespace rendering::metal4 {
// Only the actual main submission can authorize its retained first face clear.
// Does not touch pixels, publish a producer, change a lease, or alias storage.
void AuthorizeCubeFaceOverwrite206(const NativeCubeTextureBacking&,
 MTL4RenderPassDescriptor*,const NativeWorldAttachmentClear&,
 uint64_t sourceFrame,uint64_t submission);
inline bool PreserveCubeSubresource206(unsigned face,unsigned mip,unsigned selectedFace,
 bool fullFaceOverwrite,bool regenerateMipmaps) noexcept {
 return !fullFaceOverwrite || (mip==0 ? face!=selectedFace : !regenerateMipmaps);
}
} // namespace rendering::metal4
