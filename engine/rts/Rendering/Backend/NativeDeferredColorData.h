/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <atomic>
#include "NativeShieldDepthBacking.h"
#include "NativeTextureContext.h"
namespace rendering {
struct NativeDeferredColorResource {virtual ~NativeDeferredColorResource()=default;
 virtual bool Completed()const noexcept{return false;}
 // Unknown fulfilled resources must not authorize rollback. Serial owner only.
 virtual bool CanCancelUnsubmitted()const noexcept{return false;}};
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
	std::shared_ptr<const NativeDeferredColorResource> Resource()const{return std::atomic_load_explicit(&resource,std::memory_order_acquire);}
 bool Completed()const noexcept{const auto actual=Resource();return actual&&actual->Completed();}
	// One-shot release publication; capture reads only Completed(), never GPU state.
	// Owned Resource snapshots remain valid for the lifetime of this backing.
	void Fulfill(std::shared_ptr<const NativeDeferredColorResource>)const;
 bool CanCancelUnsubmitted()const noexcept{const auto actual=Resource();return !actual||actual->CanCancelUnsubmitted();}
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
