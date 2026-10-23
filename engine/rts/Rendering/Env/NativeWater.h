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
class CNativeReflectiveWater final: public IWater {
public:
 ~CNativeReflectiveWater() override {FreeResources();}
 void InitResources(bool loadShader=true) override;
 void FreeResources() override;
 void Draw() override;
 WATER_RENDERER GetID() const override {return WATER_RENDERER_REFLECTIVE;}
 bool CanDrawReflectionPass() const override {return true;}
};
class CNativeRefractiveWater final: public IWater {
public:
 ~CNativeRefractiveWater() override {FreeResources();}
 void InitResources(bool loadShader=true) override;
 void FreeResources() override;
 void Draw() override;
 WATER_RENDERER GetID() const override {return WATER_RENDERER_REFL_REFR;}
 bool CanDrawReflectionPass() const override {return true;}
};
class CNativeDynamicWater final: public IWater {
public:
 ~CNativeDynamicWater() override {FreeResources();}
 void InitResources(bool loadShader=true) override;
 void FreeResources() override;
 void Update() override;
 void AddExplosion(const float3&,float strength,float size) override;
 void Draw() override;
 WATER_RENDERER GetID() const override {return WATER_RENDERER_DYNAMIC;}
 bool CanDrawReflectionPass() const override {return true;}
 bool CanDrawRefractionPass() const override {return true;}
};
#endif
