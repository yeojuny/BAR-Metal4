/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBasicWaterData.h"
#include <functional>

namespace rendering {
class NativeBasicWaterCapture {
public:
	using TextureProvider=std::function<std::shared_ptr<const WorldTexture>(const std::string&)>;
	void Initialize(TextureProvider); // Original InitResources/map/mode/reload boundary.
	void Reset(){material.reset();}
	bool Initialized()const{return bool(material);}
	std::shared_ptr<const NativeBasicWaterFrame> Capture(const WorldFrame&,bool wireframe)const;
private:
	std::shared_ptr<const NativeBasicWaterMaterial> material;
};
} // namespace rendering
