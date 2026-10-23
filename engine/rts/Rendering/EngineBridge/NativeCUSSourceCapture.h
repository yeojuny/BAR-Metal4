/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Lua/LuaNativeCUSSource.h"
#include "Rendering/Backend/NativeCUSModelCapture.h"

namespace rendering {
class NativeTextureContext;
namespace ui {class NativeUIRecorder;}

// One real renderer/context owner. These resource/compiler callbacks must be
// connected before Ready becomes true; this adapter never invents an image.
struct NativeCUSSourceProviders {
	std::function<bool()> ready;
	std::function<std::shared_ptr<const NativeCUSProgramConsumer>(const NativeCUSProgramSource&)> compile;
	std::function<NativeTextureLookup(lua_State*,const std::string&)> parseTexture;
	std::function<bool(lua_State*,const std::string&)> deleteTexture;
	// Original shader clip-distance enable state, separate from legacy plane2.
	std::function<bool()> readClipDistance0;
	std::function<void(bool)> writeClipDistance0;
};

class NativeCUSSourceCapture {
public:
	NativeCUSSourceCapture(WorldFrameCapture&,NativeTextureContext&,ui::NativeUIRecorder&);
	~NativeCUSSourceCapture();
	NativeCUSSourceCapture(const NativeCUSSourceCapture&)=delete;
	NativeCUSSourceCapture& operator=(const NativeCUSSourceCapture&)=delete;
	void Initialize(NativeCUSSourceProviders);
	void Reset(); // After source Lua callbacks have detached, before context close.
	bool Ready()const;
	// Call at the existing original callback site. This helper does not invoke
	// another stage or update live model/camera buffers to satisfy a consumer.
	std::shared_ptr<const NativeCUSSourceFrame> Capture(uint64_t sourceDrawFrame,
		const std::function<void()>& originalCallback);
private:
	NativeCUSModelCapture models;
	NativeTextureContext& textures;
	ui::NativeUIRecorder& recorder;
	NativeCUSSourceProviders providers;
	bool installed=false;
	NativeCUSSourceDraw Raster()const;
	void ApplyRaster(const NativeCUSSourceDraw&);
};
} // namespace rendering
