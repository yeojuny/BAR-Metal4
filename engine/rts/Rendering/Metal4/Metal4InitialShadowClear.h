/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "../Backend/NativeWorldDrawData.h"

namespace rendering::metal4 {
// Only the actual post-DrawWorldShadow source marker may lie between these
// two clears. Opaque client entry, a prefix read, or another ordered event
// consumes its own sequence number and leaves both physical passes intact.
inline bool CanCombineInitialShadowClears(const NativeWorldAttachmentClear& depth,
 std::uint64_t colorMarker,const NativeWorldAttachmentClear& color)
{
 return depth.sequence>0&&depth.sequence<colorMarker&&colorMarker<color.sequence&&
        colorMarker-depth.sequence==1&&color.sequence-colorMarker==1&&
        depth.stage==NativeWorldDrawStage::ShadowOpaque&&color.stage==NativeWorldDrawStage::ShadowOpaque&&
        depth.mask==ScreenAttachmentDepth&&color.mask==ScreenAttachmentColor&&
        depth.depthWrite&&!depth.scissor&&!color.scissor&&
        color.colorWrite==std::array<bool,4>{true,true,true,false};
}
}
