#import <Metal/Metal.h>
#include "Rendering/Metal4/Metal4GroundDecalIndexPolicyG01.h"
using namespace rendering::metal4;
// Syntax-only CPU check against the installed SDK. Never linked or executed.
void CheckG01IndexedSignature(id<MTL4RenderCommandEncoder> e,id<MTLBuffer> indices,NSUInteger instances)
{
 if([e respondsToSelector:@selector(drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferLength:instanceCount:baseVertex:baseInstance:)])
  [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:GroundDecalIndicesG01.size() indexType:MTLIndexTypeUInt16 indexBuffer:indices.gpuAddress indexBufferLength:indices.length instanceCount:instances baseVertex:0 baseInstance:0];
}
