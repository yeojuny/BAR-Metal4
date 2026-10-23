/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <cstdint>
#include <memory>
#include <functional>
#include <vector>

struct SDL_Window;
union SDL_Event;

namespace rendering {
struct ScreenFrameView;
struct NativeImageReadbackRequest;
namespace ui { struct NativeUIReadbackPrefix; }
struct NativeLuaNanoFrame;
struct NativeProjectileEffectFrame;
struct NativeEnergyExplosionFrame;
struct NativeWorldDrawFrame;
struct NativeVolumetricCloudFrame;
struct NativeLightningEffectFrame;
struct NativeFireSmokeFrame;
struct NativeMissileThrusterFrame;
struct NativeFlamethrowerFrame;
struct NativeFireGL4Frame;
struct NativeNapalmPreviewFrame;
struct NativeSnowFrame;
struct NativeCommandsFrame;
struct NativeBloomFrame;
struct NativeDeferredLightFrame;
struct NativeUnitStencilFrame;
struct NativeOutlineFrame;
struct NativeShieldFrame;
struct NativeBlueprintFrame;
struct NativeBlockoutFrame;
struct ScreenViewport;
namespace ui { struct NativeUIFrameRasterMapping; }
struct NativePlacementFrame;
struct NativeUnitPrimitiveFrame;
struct NativeOpaqueModelFrame;
struct NativeStartBoxFrame;
struct NativeWeaponRangeFrame;
struct NativeSensor31657CircleFrame;
struct NativeSensorPreviewFrame;
struct NativeBumpWaterSettings;
struct NativeTerrainSceneSourceFrame;
}

namespace rendering::scene {

// Experimental main-thread bridge from the real engine lifecycle to native
// world capture/presentation. The caller owns SDL video and the SDL_Window.
// This does not establish LuaMenu, sound, or full gameplay capabilities.
void Initialize(SDL_Window* window, uint32_t renderWidth, uint32_t renderHeight);
// The engine's display policy owns these pixels; the surface never infers them
// from Retina backing size. No-op before Initialize or for an unchanged extent.
void SetRenderSize(uint32_t renderWidth, uint32_t renderHeight);
void HandleEvent(const SDL_Event& event);
// Actual window and chosen render pixels; backing scale never enters this map.
// Hidden/zero-size windows retain their last observed logical extent for RTTs.
ui::NativeUIFrameRasterMapping GetUIFrameRasterMapping(const ScreenViewport& defaultViewport);
struct DrawResult {
	bool presented = false;
	bool screenTargetsCompleted = false;
	bool sourceFrameCompleted = false;
	std::uint64_t completedPostprocessSerial = 0;
	std::uint64_t sourceDrawFrame = 0;
};
// Original LuaIntro uses a real screen frame before map/camera initialization.
DrawResult DrawLoadingScreen(const ScreenFrameView& screenUI);
void EndLoadingScreen();
// Original sky/sun dirty-state update precedes PreUnit and engine capture.
void InstallOriginalModelSourcesBeforeLua();
// After actual ShadowHandler target/projection initialization, including reload.
void InitializeOriginalShadowPrograms();
// ProjectileDrawer owns ProjFX definition and retirement, independently of
// ShadowHandler target initialization/reload. Construction follows definition.
void ConstructOriginalShadowParticlePrograms();
void ResetOriginalShadowParticlePrograms();
void CaptureOriginalGenesis(const std::function<void()>& originalCallback);
void PrepareOriginalWorldStateBeforePreUnit();
void PrepareOriginalSkyBeforePreUnit();
std::shared_ptr<const NativeOpaqueModelFrame> CaptureOriginalOpaqueModels();
void PrepareEnvironmentBeforePreUnit();
// Called inside the actual CreateReflectionFace ENVMAP camera/draw-mode scope.
void CaptureOriginalTerrainReflectionFace(unsigned face,bool skyOnly,uint64_t initialClearSequence);
std::shared_ptr<const NativeTerrainSceneSourceFrame> TakeOriginalTerrainReflectionFace();
void PrepareMapTextureInputs();
void CompleteStartBoxWorld(std::shared_ptr<const NativeStartBoxFrame>);
void CompleteBlockoutWorld(std::shared_ptr<const NativeBlockoutFrame>);
void CompleteWeaponRangeWorld(std::shared_ptr<const NativeWeaponRangeFrame>);
void CompleteSensor31657CircleWorld(std::shared_ptr<const NativeSensor31657CircleFrame>);
void CompleteSensorPreviewWorld(std::shared_ptr<const NativeSensorPreviewFrame>);
void CompleteUnitPrimitiveWorld(std::shared_ptr<const NativeUnitPrimitiveFrame>);
// Capture the original engine world after DrawWorldPreUnit, before later Lua
// DrawWorld callbacks can emit particles. Presentation consumes this frame
// once; it must never recapture engine particles after those callbacks.
// Dispatch the original DrawPreDecals callback, then snapshot the original
// engine decal rows before DrawWorldPreUnit can modify them. No GPU work.
void PrepareGroundDecalsBeforePreUnit();
void PrepareShieldMapBeforePreUnit();
void PrepareWorld(std::shared_ptr<const NativeUnitPrimitiveFrame> luaUnitPrimitiveFrame = {},
	std::shared_ptr<const NativeOpaqueModelFrame> actualOpaqueModels = {});
// The original IWater resource and new-simulation-frame lifecycle. These do
// not advance water from presentation, nor publish a generic GL capability.
bool CanInitializeWater();
void InitializeWater(const NativeBumpWaterSettings&);
void UpdateWater();
void ResetWater();
bool WaterInitialized();
void InitializeBasicWater();
void ResetBasicWater();
bool BasicWaterInitialized();
void AbortPreparedWorld();
// Borrowed snapshots at the exact SaveImage call. Closed typed world families
// are shared, while generic world/cloud/postprocess recorders stay open.
struct ReadbackWorldInputs {
	bool worldCallinsComplete=false;
	std::shared_ptr<const NativeLuaNanoFrame> luaNanoFrame;
	std::shared_ptr<const NativeProjectileEffectFrame> luaProjectileEffectsFrame;
	std::shared_ptr<const NativeWorldDrawFrame> luaWorldDrawFrame;
	std::shared_ptr<const NativeEnergyExplosionFrame> luaEnergyExplosionFrame;
	std::shared_ptr<const NativeVolumetricCloudFrame> luaVolumetricCloudFrame;
	std::shared_ptr<const NativeLightningEffectFrame> luaLightningEffectFrame;
	std::shared_ptr<const NativeBlueprintFrame> luaBlueprintFrame;
	std::shared_ptr<const NativePlacementFrame> placementFrame;
	std::shared_ptr<const NativeFireSmokeFrame> luaFireSmokeFrame;
	std::shared_ptr<const NativeMissileThrusterFrame> luaMissileThrusterFrame;
	std::shared_ptr<const NativeShieldFrame> luaShieldFrame;
	std::shared_ptr<const NativeFlamethrowerFrame> luaFlamethrowerFrame;
	std::shared_ptr<const NativeNapalmPreviewFrame> luaNapalmPreviewFrame;
	std::shared_ptr<const NativeCommandsFrame> luaCommandsFrame;
	std::shared_ptr<const NativeFireGL4Frame> luaFireGL4Frame;
	std::shared_ptr<const NativeBloomFrame> luaBloomFrame;
	std::shared_ptr<const NativeDeferredLightFrame> luaDeferredLightFrame;
	std::shared_ptr<const NativeUnitStencilFrame> luaUnitStencilFrame;
	std::shared_ptr<const NativeOutlineFrame> luaOutlineFrame;
	std::shared_ptr<const NativeSnowFrame> luaSnowFrame;
};
std::vector<std::uint8_t> ReadCurrentImage(const ui::NativeUIReadbackPrefix&,
	const NativeImageReadbackRequest&,const ReadbackWorldInputs&);

// Target producers can complete without a drawable. A false presentWorld
// preserves that work during the original inactive-window draw throttle.
DrawResult DrawWorld(const ScreenFrameView* screenUI = nullptr, bool presentWorld = true,
	std::shared_ptr<const NativeLuaNanoFrame> luaNanoFrame = {},
	std::shared_ptr<const NativeProjectileEffectFrame> luaProjectileEffectsFrame = {},
	std::shared_ptr<const NativeWorldDrawFrame> luaWorldDrawFrame = {},
	std::shared_ptr<const NativeEnergyExplosionFrame> luaEnergyExplosionFrame = {},
	std::shared_ptr<const NativeVolumetricCloudFrame> luaVolumetricCloudFrame = {},
	std::shared_ptr<const NativeLightningEffectFrame> luaLightningEffectFrame = {},
	std::shared_ptr<const NativeBlueprintFrame> luaBlueprintFrame = {},
	std::shared_ptr<const NativePlacementFrame> placementFrame = {},
	std::shared_ptr<const NativeFireSmokeFrame> luaFireSmokeFrame = {},
	std::shared_ptr<const NativeMissileThrusterFrame> luaMissileThrusterFrame = {},
	std::shared_ptr<const NativeShieldFrame> luaShieldFrame = {},
	std::shared_ptr<const NativeFlamethrowerFrame> luaFlamethrowerFrame = {},
	std::shared_ptr<const NativeNapalmPreviewFrame> luaNapalmPreviewFrame = {},
	std::shared_ptr<const NativeCommandsFrame> luaCommandsFrame = {},
	std::shared_ptr<const NativeFireGL4Frame> luaFireGL4Frame = {},
	std::shared_ptr<const NativeBloomFrame> luaBloomFrame = {},
	std::shared_ptr<const NativeDeferredLightFrame> luaDeferredLightFrame = {},
	std::shared_ptr<const NativeUnitStencilFrame> luaUnitStencilFrame = {},
	std::shared_ptr<const NativeOutlineFrame> luaOutlineFrame = {},
	std::shared_ptr<const NativeSnowFrame> luaSnowFrame = {});
void ResetWorld();
// After original Lua Shutdown, before LuaOpenGL and source owners are freed.
void DetachOriginalModelSourcesAfterLua();
void Shutdown();
bool IsInitialized();

} // namespace rendering::scene
