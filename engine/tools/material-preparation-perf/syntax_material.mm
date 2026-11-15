#include "Rendering/Metal4/Metal4ModelMaterial156.h"
struct SyntaxLease {
 id<MTLDevice> Device();
 const void* ContextIdentity();
 void AddAllocation(id<MTLBuffer>);
};
void InstantiateProduction(SyntaxLease& lease,const std::array<id<MTLTexture>,12>& t,
                           const std::array<id<MTLSamplerState>,12>& s){
 rendering::metal4::ModelMaterials156 material;
 material.BeginBatch(lease);material.Get(lease,t,s);material.Clear();
}
