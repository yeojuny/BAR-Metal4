/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeWater.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include <bit>
#include "Rendering/Backend/NativeBumpWaterData.h"
#include "Rendering/Metal4/Metal4SceneBridge.h"
#include "System/Config/ConfigHandler.h"
#include "System/Exceptions.h"

CNativeBasicWater::~CNativeBasicWater()
{
	FreeResources();
}

void CNativeBasicWater::InitResources(bool loadShader)
{
	(void)loadShader;
	rendering::scene::InitializeBasicWater();
}

void CNativeBasicWater::FreeResources()
{
	if (rendering::scene::IsInitialized())
		rendering::scene::ResetBasicWater();
}

void CNativeBasicWater::Draw()
{
	throw content_error("Native Water0 must be drawn by the ordered native world stage");
}

CNativeBumpWater::~CNativeBumpWater()
{
	FreeResources();
}

void CNativeBumpWater::InitResources(bool loadShader)
{
	// The original initialization argument controls legacy shader reload only;
	// native PSOs and all required scene producers must already be available.
	(void)loadShader;
	if (!rendering::scene::CanInitializeWater())
		throw content_error("Original Water4 requires the complete native scene consumer");
	rendering::NativeBumpWaterSettings settings;
	settings.reflectionSize = std::bit_ceil<uint32_t>(configHandler->GetInt("BumpWaterTexSizeReflection"));
	settings.reflectionDepthBits = configHandler->GetInt("BumpWaterDepthBits");
	settings.reflection = configHandler->GetInt("BumpWaterReflection");
	settings.refraction = configHandler->GetInt("BumpWaterRefraction");
	settings.anisotropy = configHandler->GetFloat("BumpWaterAnisotropy");
	settings.depthCopy = configHandler->GetBool("BumpWaterUseDepthTexture");
	settings.blurReflection = configHandler->GetBool("BumpWaterBlurReflection");
	settings.shoreWaves = configHandler->GetBool("BumpWaterShoreWaves");
	settings.endlessOcean = configHandler->GetBool("BumpWaterEndlessOcean");
	settings.dynamicWaves = configHandler->GetBool("BumpWaterDynamicWaves");
	// Actual native depth-format validation replaces the legacy GL probe, whose
	// HEADLESS value is not a measurement of the Metal attachment capability.
	// Map-specific shore/plane/tile eligibility remains in the original capture.
	rendering::scene::InitializeWater(settings);
}

void CNativeBumpWater::FreeResources()
{
	if (rendering::scene::IsInitialized())
		rendering::scene::ResetWater();
}

void CNativeBumpWater::Update()
{
	// CWorldDrawer invokes this once at the original new-simulation-frame
	// unsynced stage. Draw/replay never advances water recurrence or guRNG.
	if (rendering::scene::WaterInitialized())
		rendering::scene::UpdateWater();
}

void CNativeBumpWater::Draw()
{
	throw content_error("Native Water4 must be drawn by the ordered native world stage");
}
void CNativeReflectiveWater::InitResources(bool loadShader){(void)loadShader;rendering::scene::InitializeLegacyWater();}
void CNativeReflectiveWater::FreeResources(){if(rendering::scene::IsInitialized())rendering::scene::ResetLegacyWater();}
void CNativeReflectiveWater::Draw(){throw content_error("Native Water1 must use its ordered world stage");}
void CNativeRefractiveWater::InitResources(bool loadShader){(void)loadShader;rendering::scene::InitializeLegacyRefractiveWater();}
void CNativeRefractiveWater::FreeResources(){if(rendering::scene::IsInitialized())rendering::scene::ResetLegacyWater();}
void CNativeRefractiveWater::Draw(){throw content_error("Native refractive water must use its ordered world stage");}
void CNativeDynamicWater::InitResources(bool loadShader){(void)loadShader;rendering::scene::InitializeDynamicWater();}
void CNativeDynamicWater::FreeResources(){if(rendering::scene::IsInitialized())rendering::scene::ResetDynamicWater();}
void CNativeDynamicWater::Update(){rendering::scene::UpdateDynamicWater();}
void CNativeDynamicWater::AddExplosion(const float3& pos,float strength,float size){rendering::scene::AddDynamicWaterExplosion(pos,strength,size);}
void CNativeDynamicWater::Draw(){throw content_error("Native dynamic water requires its ordered world stage");}
#endif
