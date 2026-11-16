#import <Metal/Metal.h>
#include "Rendering/Metal4/Metal4GroundDecalUploadPolicyG02.h"
using namespace rendering::metal4;
void CheckG02IndexedSignature(id<MTL4RenderCommandEncoder> e,id<MTLBuffer> indices,NSUInteger instances,std::size_t indexOffset)
{
 if([e respondsToSelector:@selector(drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferLength:instanceCount:baseVertex:baseInstance:)])
  [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:GroundDecalIndicesG01.size() indexType:MTLIndexTypeUInt16 indexBuffer:(indices.gpuAddress+indexOffset) indexBufferLength:(indexOffset?GroundDecalIndexBytesG01:indices.length) instanceCount:instances baseVertex:0 baseInstance:0];
}
