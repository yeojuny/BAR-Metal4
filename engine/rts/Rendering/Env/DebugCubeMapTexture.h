#pragma once

#include <cstdint>
#include <memory>
namespace rendering { struct NativeWorldSkyResource; }
#include "Rendering/GL/VAO.h"
#include "System/type2.h"

namespace Shader {
	struct IProgramObject;
}

class CCamera;

class DebugCubeMapTexture {
public:
	DebugCubeMapTexture();
	~DebugCubeMapTexture();

	uint32_t GetId() const { return texId; }
	int2 GetDimensions() const { return dims; }

	void Draw(uint32_t face = 0) const;
	std::shared_ptr<const rendering::NativeWorldSkyResource> GetNativeResource() const { return nativeResource; }

	static DebugCubeMapTexture& GetInstance();
private:
	uint32_t texId;
	int2 dims;
	VAO vao; //even though VAO has no attached VBOs, it's still needed to perform rendering
	Shader::IProgramObject* shader = nullptr;
	std::shared_ptr<const rendering::NativeWorldSkyResource> nativeResource;
};

#define debugCubeMapTexture (DebugCubeMapTexture::GetInstance())