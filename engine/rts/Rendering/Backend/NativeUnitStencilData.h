/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSSourceData.h"
#include <functional>
#include <optional>
namespace rendering {
struct NativeUnitStencilProgramSource {
	std::string vertex,geometry,fragment,vertexSHA512,geometrySHA512,fragmentSHA512;
	std::map<std::string,std::vector<float>> initialFloat;
};
struct NativeUnitStencilProgramConsumer {
	std::shared_ptr<const void> pipelineLifetime;
	std::string vertexSHA512,geometrySHA512,fragmentSHA512,log;
	std::vector<NativeCUSUniformDescription> uniforms;
	std::map<std::string,std::vector<float>> defaultFloat;
};
struct NativeUnitStencilImage {
	uint64_t identity=0;
	uint32_t width=0,height=0;
	NativeTextureSampling sampling;
};
struct NativeUnitStencilProducer {
	uint64_t submissionIdentity=0;
	bool allocationOnly=false,allocationVerified=false,encoded=false,completed=false;
};
struct NativeUnitStencilResource { virtual ~NativeUnitStencilResource()=default; };
struct NativeUnitStencilVersion {
	std::shared_ptr<const NativeUnitStencilImage> image;
	uint64_t generation=0,sourceDrawFrame=0;
	std::shared_ptr<NativeUnitStencilProducer> producer=std::make_shared<NativeUnitStencilProducer>();
	const std::shared_ptr<const NativeUnitStencilResource>& Resource()const{return resource;}
	void Fulfill(std::shared_ptr<const NativeUnitStencilResource>)const;
private:
	mutable std::shared_ptr<const NativeUnitStencilResource> resource;
};
struct NativeUnitStencilBacking final:NativeTextureBacking {
	std::shared_ptr<const NativeUnitStencilVersion> version;
	NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::ColorImage;}
};
struct NativeUnitStencilDraw {
	uint64_t sequence=0;
	std::shared_ptr<const NativeUnitStencilProgramConsumer> program;
	NativeBufferSnapshotRef records;
	// Actual original GS path consumes one point/record; there is no per-draw
	// object ID re-resolution or synthetic instance ID. Native expansion reads
	// this exact first..count range and emits the same3 triangle strips.
	uint32_t first=0,count=0;
	float addRadius=10,stencilColor=1;
	std::shared_ptr<const NativeCUSUniformPublication> engineUniforms;
	std::shared_ptr<const NativeCUSPublishedBuffer> modelUniforms;
	ScreenDrawState surface;
	NativeWorldDrawState world;
};
struct NativeUnitStencilCommand {
	bool clear=false;
	uint64_t sequence=0;
	std::array<float,4> clearColor{};
	ScreenDrawState surface;
	NativeWorldDrawState world;
	std::optional<NativeUnitStencilDraw> draw;
};
struct NativeUnitStencilPass {
	uint64_t sequence=0;
	std::shared_ptr<const NativeUnitStencilVersion> output,previous;
	std::vector<NativeUnitStencilCommand> commands;
};
struct NativeUnitStencilFrame {
	uint64_t sourceDrawFrame=0;
	mutable bool completed=false;
	std::vector<NativeUnitStencilPass> passes;
	std::vector<std::function<void()>> rollback;
	~NativeUnitStencilFrame();
};
void ValidateNativeUnitStencilProgram(const NativeUnitStencilProgramSource&);
void ValidateNativeUnitStencilDraw(const NativeUnitStencilDraw&);
} // namespace rendering
