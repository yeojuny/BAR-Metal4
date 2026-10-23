/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeShieldDepthBacking.h"
#include "NativeTextureContext.h"
namespace rendering {
struct NativeDeferredColorResource {virtual ~NativeDeferredColorResource()=default;};
struct NativeDeferredColorMetadata {
	NativeGeometryTextureRole role=NativeGeometryTextureRole::Models;
	unsigned attachment=0;
	NativeShieldDepthMetadata view;
};
// Actual original GBuffer logical names come from GeometryBuffer creation.
// This color version and the corresponding depth version share one producer.
class NativeDeferredColorBacking final:public NativeTextureBacking {
public:
	NativeDeferredColorBacking(uint64_t storage,uint64_t version,uint64_t frame,
		NativeTextureContent,const NativeDeferredColorMetadata&);
	NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::ColorImage;}
	const NativeDeferredColorMetadata& Metadata()const{return metadata;}
	const std::shared_ptr<const NativeDeferredColorResource>& Resource()const{return resource;}
	void Fulfill(std::shared_ptr<const NativeDeferredColorResource>)const;
private:
	NativeDeferredColorMetadata metadata;
	mutable std::shared_ptr<const NativeDeferredColorResource> resource;
};
struct NativeDeferredColorVersion {
	std::shared_ptr<const NativeCUSTextureObject> previous,current;
	std::shared_ptr<const NativeDeferredColorBacking> backing;
	bool Changed()const{return previous!=current;}
};
// Called at the actual selected deferred source phase, before Lua consumers.
// false retains old allocation/contents and creates no fictitious clear.
NativeDeferredColorVersion PublishNativeDeferredColor(NativeTextureRegistry&,
	const NativeGeometryTextureNames&,NativeGeometryTextureRole,unsigned attachment,
	uint64_t sourceDrawFrame,const NativeShieldDepthMetadata&,bool sourceWrites);
void CancelNativeDeferredColor(NativeTextureRegistry&,const NativeDeferredColorVersion&)noexcept;
} // namespace rendering
