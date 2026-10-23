/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "IWater.h"

// Original IWater lifetime/configuration boundary. Rendering is performed by
// the connected native world stages using the same captured source frame.
class CNativeBasicWater final : public IWater {
public:
	~CNativeBasicWater() override;
	void InitResources(bool loadShader = true) override;
	void FreeResources() override;
	void Draw() override;
	WATER_RENDERER GetID() const override { return WATER_RENDERER_BASIC; }
};

class CNativeBumpWater final : public IWater {
public:
	~CNativeBumpWater() override;
	void InitResources(bool loadShader = true) override;
	void FreeResources() override;
	void Update() override;
	void Draw() override;
	WATER_RENDERER GetID() const override { return WATER_RENDERER_BUMPMAPPED; }
	bool CanDrawReflectionPass() const override { return true; }
	bool CanDrawRefractionPass() const override { return true; }
};
#endif
