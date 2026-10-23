/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTextureRegistry.h"
#include "NativeWorldParticleData.h"

namespace rendering {
// The original packed pages, retained at the actual atlas upload. GPU storage
// is materialized by the existing WorldParticlesGPU owner on the common lease.
struct NativeParticleAtlasBacking final:NativeTextureBacking {
 std::shared_ptr<const NativeWorldParticleAtlas> atlas;
 NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::ColorImage;}
};

// An actual native registry name, not a GL handle or a resource-ID surrogate.
// CTextureAtlas owns this exactly where it owns the original GL::TextureBase.
class NativeParticleAtlasTexture {
public:
 ~NativeParticleAtlasTexture();
 uint32_t Name()const noexcept{return uint32_t(name);}
 void Bind()const;
 void Unbind()const;
 std::shared_ptr<const NativeWorldParticleAtlas> Source()const;
 void DisOwn()noexcept{owned=false;}
private:
 uint64_t context=0,name=0;
 NativeCUSTextureTarget target=NativeCUSTextureTarget::Image2D;
 bool owned=true;
 friend bool PublishNativeParticleAtlas(std::shared_ptr<NativeParticleAtlasTexture>&,
  std::shared_ptr<const NativeWorldParticleAtlas>,uint64_t);
};

// Mirrors constructor sampling/storage, old wrapper deletion, and the second
// scoped upload binding. Listable operations replay through the existing
// recorder; name creation and wrapper deletion remain immediate. The result
// describes immediate publication only; Source() resolves replayed contents.
bool PublishNativeParticleAtlas(std::shared_ptr<NativeParticleAtlasTexture>&,
 std::shared_ptr<const NativeWorldParticleAtlas>,uint64_t actualSourceDrawFrame);
}
