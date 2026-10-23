/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "IInfoTextureHandler.h"
#include "Rendering/Backend/NativeMetalInfoCapture.h"
#include "Rendering/Backend/NativeWorldInfoCapture.h"
#include "Rendering/EngineBridge/NativeTerrainImagePublication.h"
#include "Map/ReadMap.h"
#include "System/Log/ILog.h"
#include <limits>
#include <stdexcept>

// Native info-mode state exposes only the actual shared logical source name,
// never a GL name or a completed GPU resource. Before its first real source
// event (and after reset), image lookup is explicitly unobserved.
// A rejected mode leaves the previous valid mode intact, including LOS return.
class CNativeInfoTextureHandler final: public IInfoTextureHandler {
public:
	CNativeInfoTextureHandler(){rendering::ResetNativeMetalInfo();rendering::ResetNativeWorldInfo();rendering::OriginalNativeTerrainImagePublication().InfoOwnerCreated(this);}
	~CNativeInfoTextureHandler() override {rendering::OriginalNativeTerrainImagePublication().InfoDestroyed(this);}
	void Update() override {
		rendering::UpdateNativeMetalInfo();rendering::UpdateNativeWorldInfo();
		rendering::OriginalNativeTerrainImagePublication().ObserveInfo(this,rendering::CaptureNativeWorldInfo());
	}
	bool IsEnabled() const override { return !mode.empty(); }
	bool InMetalMode() const override { return mode=="metal"; }
	void DisableCurrentMode() override { SetMode(returnToLos && mode!="los"?"los":""); }
	void SetMode(const std::string& name) override {
		if (!name.empty() && name!="los" && name!="metal") {
			LOG_L(L_ERROR,"Native info texture mode '%s' requires an implemented native producer; previous mode '%s' retained",name.c_str(),mode.c_str());
			return;
		}
		try { rendering::SelectNativeWorldInfoMode(name); }
		catch(const std::exception& error){LOG_L(L_ERROR,"Native metal info mode rejected: %s; previous mode retained",error.what());return;}
		returnToLos &= !name.empty();returnToLos |= name=="los";mode=name;
		// Publication follows the successful original mode commit. Its failures
		// must not be swallowed by the rejected-mode/previous-mode catch above.
		rendering::OriginalNativeTerrainImagePublication().ObserveInfo(this,rendering::CaptureNativeWorldInfo());
	}
	bool HasMode(const std::string& name) const override { return name=="los"||(name=="metal"&&rendering::IsNativeMetalInfoConsumerAvailable()); }
	void ToggleMode(const std::string& name) override { if (mode==name) DisableCurrentMode(); else SetMode(name); }
	const std::string& GetMode() const override { return mode; }
	const std::vector<std::string> GetModes() const override { return rendering::IsNativeMetalInfoConsumerAvailable()?std::vector<std::string>{"los","metal"}:std::vector<std::string>{"los"}; }
	GLuint GetCurrentInfoTexture() const override {
		const auto image=rendering::OriginalNativeTerrainImagePublication().InfoImage(this);
		if(image->name>std::numeric_limits<GLuint>::max())throw std::runtime_error("Original info source name exceeds its public source-ID range");
		return GLuint(image->name);
	}
	int2 GetCurrentInfoTextureSize() const override {
		const auto image=rendering::OriginalNativeTerrainImagePublication().InfoImage(this);
		return {int(image->allocation.width),int(image->allocation.height)};
	}
	const CInfoTexture* GetInfoTextureConst(const std::string&) const override { return nullptr; }
	CInfoTexture* GetInfoTexture(const std::string&) override { return nullptr; }
private:
	std::string mode;
	bool returnToLos=false;
};
