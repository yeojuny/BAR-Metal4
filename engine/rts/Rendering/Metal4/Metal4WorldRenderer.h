/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Metal4WindowSurface.h"
#include "Rendering/Backend/NativeMainSamplingData.h"
#include "../Backend/RenderTypes.h"
#include "../Backend/WorldFrameData.h"
#include "../Backend/ScreenDrawList.h"
#include <memory>
#include <functional>

namespace rendering {
namespace ui { struct NativeUIReadbackPrefix; }
struct NativeGeometryTextureRequest;
struct NativeBlockoutTarget;
class NativeEngineModelProgramSource;
class NativeEngineShadowProgramSource;
class NativeOriginalShadowParticlesProgramSource;
struct NativeCUSProgramSource;
struct NativeCUSProgramConsumer;
struct NativeDeferredLightProgramSource;
struct NativeDeferredLightProgramConsumer;
struct NativeUnitStencilProgramSource;
struct NativeUnitStencilProgramConsumer;
struct NativeUnitStencilFrame;
struct NativeOutlineProgramSource;
struct NativeOutlineProgramConsumer;
struct NativeDistortionProgramSource;
struct NativeDistortionProgramConsumer;
class NativeTextureRegistry;
class NativeTextureContext;
struct NativeMapDepthSelection;
struct NativeModelDepthSelection;
struct NativeShieldDepthFrame;
struct NativeShadowTextureFrame;
struct NativeOriginalShadowFrame;
struct NativeTerrainProgramRequest;
struct NativeTerrainProgramLink;
struct NativeSensorPreviewProgramSource;
struct NativeSensorPreviewProgramConsumer;

// Attaches to the engine's borrowed SDL window using the exact shared native
// runtime device. The engine still owns SDL video, events and window teardown.
Metal4WindowSurface AttachMetal4RendererSurface(SDL_Window* window, const Metal4SurfaceConfig& config);

// Query after Draw has returned/thrown. False after an unretired submission
// forbids normal renderer/surface/window teardown and requires process restart.
// A shader/setup error or completed GPU error can still detach safely.
bool Metal4RendererCanSafelyDetachSurface();

enum class Metal4PresentationStatus {
	Presented, Detached, CloseRequested, Hidden, Minimized, ZeroExtent, DrawableUnavailable, OffscreenSourceCompleted,
};

struct Metal4WorldPresentationResult;
struct Metal4WorldRenderOptions {
#ifdef RECOIL_METAL4_LIVE_SCENE
	static constexpr bool DefaultShaderValidation = false;
#else
	static constexpr bool DefaultShaderValidation = true;
#endif
	bool shaderValidation = DefaultShaderValidation;
	const ScreenFrameView* screenUI = nullptr; // Same drawable/encoder, before one present.
	// Non-null only for an explicit synchronous SaveImage cut after the world
	// callins. The common first batch retains main attachments for its suffix.
	const ui::NativeUIReadbackPrefix* readback = nullptr;
	ScreenScissor readbackRectangle;
	std::function<void(std::uint64_t,const NativePostprocessFrame*)> submissionBegan;
	std::function<void(std::uint64_t)> submissionCompleted;
	std::function<void(const Metal4WorldPresentationResult&)> sourceCompleted;
	bool presentWorld = true; // False only for real copy-dependent source work, never an extra present.
	// Last actually observed logical window extent, used only for captured
	// offscreen source work if SDL reports zero while minimizing.
	std::array<std::uint32_t,2> sourceLogicalExtent{};
};

struct Metal4MaintenanceResult {
	bool submitted = false;
	bool screenTargetsCompleted = false;
	std::array<std::uint64_t, 2> completedWorldInfo{};
};

struct Metal4WorldPresentationResult {
	Metal4PresentationStatus status = Metal4PresentationStatus::DrawableUnavailable;
	bool presented = false;
	bool sourceFrameCompleted = false;
	bool screenTargetsCompleted = false;
	std::uint64_t completedPostprocessSerial = 0;
	std::uint64_t sourceDrawFrame = 0;
	Metal4SurfaceSnapshot surface;
	std::array<double, 4> renderViewport{}; // x, top-left y, width, height pixels.
	DeviceCapabilities capabilities;
	std::optional<FrameMetrics> metrics;
	double cpuResourceUpdateMs = 0;
	double shaderCompileMs = 0;
	double cpuDrawableAcquireMs = 0;
	uint64_t simulationFrame = 0; // Source identity, not a measured simulation time.
	uint32_t terrainTiles = 0;
	uint32_t terrainDraws = 0;
	uint32_t mapEdgeDraws = 0;
	uint32_t mapEdgeInstances = 0; // Includes the original seam curtain instance.
	uint32_t unitsDrawn = 0;
	uint32_t piecesDrawn = 0;
	uint32_t selectedUnits = 0;
	uint32_t featuresDrawn = 0;
	uint32_t featurePiecesDrawn = 0;
	uint32_t opaqueFeatureDraws = 0;
	uint32_t alphaFeatureDraws = 0;
	uint32_t skyDraws=0,reflectionTerrainDraws=0;
	uint32_t reflectionFace=0,unitBRDFWidth=0,unitBRDFGOption=0;
	bool reflectionReady=false; // First actual complete face/mip cycle, not a neutral input.
	std::array<uint64_t,2> completedWorldInfo{}; // map identity, source completion serial
	std::array<uint64_t,3> completedWaterUpdates{}; // generation, normal, coast; only after GPU retirement
	uint32_t waterDraws=0, basicWaterDraws=0, waterSceneModelDraws=0, waterSceneTerrainDraws=0;
	uint32_t shadowTerrainDraws = 0;
	uint32_t shadowModelDraws = 0;
	uint32_t projectileModelsDrawn = 0;
	uint32_t projectilePiecesDrawn = 0;
	uint32_t particleDraws = 0;
	uint32_t groundDecalInstances = 0, groundDecalAtlasDraws = 0, luaPreDecalsDraws = 0;
	uint32_t luaGameDecalDraws = 0, luaGameDecalInstances = 0;
	uint32_t luaStartBoxDraws=0,luaStartBoxMiniMapDraws=0;
	std::array<uint32_t,3> luaUnitPrimitiveDraws{}, luaUnitPrimitiveInstances{};
	uint32_t engineNanoInstances = 0;
	uint32_t luaNanoInstances = 0;
	uint32_t luaNanoDraws = 0;
	uint32_t luaPreUnitDraws = 0, luaWorldDraws = 0;
	uint32_t luaProjectileEffectDraws = 0, luaProjectileEffectInstances = 0;
	uint32_t luaEnergyExplosionDraws = 0, luaEnergyExplosionInstances = 0;
	uint32_t luaLightningDraws = 0, luaLightningInstances = 0;
	uint32_t luaDeferredLightDraws = 0, luaDeferredLightInstances = 0;
	uint32_t luaFireSmokeDraws = 0, luaFireSmokeSubmittedInstances = 0;
	uint32_t luaMissileThrusterDraws = 0, luaMissileThrusterSubmittedInstances = 0;
	uint32_t luaFlamethrowerDraws = 0, luaFlamethrowerSubmittedInstances = 0;
	uint32_t luaNapalmPreviewDraws = 0;
	uint32_t luaSnowDraws = 0;
	uint32_t luaCommandsDraws = 0;
	uint32_t luaShieldDraws=0, mapGBufferDepthPasses=0, modelGBufferDepthPasses=0, modelGBufferDepthPieces=0;
	std::array<uint32_t, 4> luaMissileThrusterProgramDraws{};
	uint32_t buildingGridDraws=0,buildingSampleDraws=0,buildingSamplePieces=0;
	uint32_t luaBlueprintDraws = 0, luaBlueprintModels = 0, luaBlueprintOutlines = 0, luaBlueprintStencilClears = 0;
	std::vector<std::string> materialSources; // Actual selected original definitions/resources.
	std::vector<std::string> materialDependenciesPending; // Requested stages not satisfied by this submission.
	uint64_t residentBytes = 0;
	uint32_t residencyAllocationCount = 0;
	std::string queueClass;
	std::string commandBufferClass;
	std::string renderEncoderClass;
	std::uint32_t screenDraws = 0;
	std::uint32_t luaPiPDraws = 0; // Actual encoded commands acknowledged after successful GPU completion.
	std::uint32_t screenTextureUploads = 0;
	std::vector<std::uint8_t> readbackPixels; // Actual retired RGBA8, only for requested exports.

};

// Experimental native SMF/S3O scene, selected original forward material terms,
// shadow depth producers, particles and original UI in one presentation. Missing
// original passes/resources remain explicit in materialDependenciesPending; this
// interface does not assert complete original material or game graphics parity.
// Main-thread use only. Borrows surface through destruction; do not move or
// detach that surface while this renderer exists. Successful Draw is synchronous
// through GPU event+feedback retirement. Destroy renderer, detach surface, then
// destroy SDL_Window. An unsafe GPU failure poisons the shared runtime and
// requires process restart, not live surface teardown/reuse.
class Metal4WorldRenderer {
public:
	explicit Metal4WorldRenderer(Metal4WindowSurface& surface,int requestedSamples=0);
	NativeMainSampling ActualMainSampling() const;
	~Metal4WorldRenderer();
	Metal4WorldRenderer(Metal4WorldRenderer&&) noexcept;
	Metal4WorldRenderer& operator=(Metal4WorldRenderer&&) noexcept;
	Metal4WorldRenderer(const Metal4WorldRenderer&) = delete;
	Metal4WorldRenderer& operator=(const Metal4WorldRenderer&) = delete;

	// Frame/resources remain immutable until return. Static resources are cached
	// by owned identity; camera/heights/poses update without re-decoding assets.
	// Logical camera viewport and requested render pixels remain independent:
	// camera's actual subview maps proportionally to the explicit target.
	Metal4WorldPresentationResult Draw(const WorldFrame& frame, const Metal4WorldRenderOptions& options = {});
	// Pin completed producer images at the original pre-unit IBL stage. Later
	// Lua mode switches must not retroactively change an earlier reflection.
	void CaptureEnvironmentInputs(std::shared_ptr<const NativeWorldEnvironmentFrame> frame, bool shaderValidation = Metal4WorldRenderOptions::DefaultShaderValidation);
	// Execute only this frame's original cached-panel producers, using the same
	// persistent UI resource cache as Draw. No drawable, world capture, readback
	// or presentation is required. Each target keeps its own extent/load/store.
	// Returns true only after GPU completion and feedback initialize all supplied
	// target versions; false means no target passes were supplied. The caller may
	// then retire those CPU producer records independently of window presentation.
	// The borrowed frame and all resources remain immutable until return.
	bool SubmitScreenTargets(const ScreenFrameView& frame, bool shaderValidation = Metal4WorldRenderOptions::DefaultShaderValidation);
	// Original global-info updates continue at their source cadence even when
	// hidden UI has no RTT producers. Share the existing maintenance submission;
	// acknowledge only retired work, or a token already completed by its owner.
	// Opt-in SaveImage only. Returns no value when the actual dependency graph
	// needs the current world transaction; that is not a successful read/save.
	std::optional<std::vector<std::uint8_t>> ReadIndependentTargetPrefix(
		const ui::NativeUIReadbackPrefix&,ScreenScissor rectangle,bool shaderValidation);
	Metal4MaintenanceResult SubmitMaintenance(const ScreenFrameView* frame,
		std::shared_ptr<const NativeWorldInfoFrame> worldInfo, bool shaderValidation = Metal4WorldRenderOptions::DefaultShaderValidation,
		std::shared_ptr<const NativeUnitStencilFrame> unitStencilFrame = {});
	bool HasScreenContinuation() const noexcept;
	void AbortScreenContinuation() noexcept;
	void ClearWaterResources();
	void ClearBasicWaterResources();
	bool CanInitializeWater() const;
	// Local PSOs only; never call Session source.Ready from a provider.ready callback.
	bool DeferredPostprocessConsumersInitialized() const noexcept;
	void ClearStaticResources(); // Only between completed Draw calls.
	// Actual compiled family owner; the Session publishes this only after its source services attach.
	std::shared_ptr<const void> Sensor31657CircleConsumerLifetime() const;
	std::uint32_t SensorPreviewMaximumDimension() const;
	std::shared_ptr<const NativeSensorPreviewProgramConsumer> CompileOriginalSensorPreviewProgram(const NativeSensorPreviewProgramSource&);
	bool CheckShieldGeometryAllocation(const NativeGeometryTextureRequest&,NativeTextureRegistry&,uint64_t sourceDrawFrame);
	void BeginShieldMapPublication(NativeTextureContext&,uint64_t,const WorldCamera&,const NativeMapDepthSelection&);
	std::shared_ptr<const NativeShieldDepthFrame> PublishShieldModelDepth(const WorldFrame&,const NativeModelDepthSelection&);
	// Borrow the actual original named model depth; opaque source owns writes
	// and cancellation. Map depth keeps its independent existing producer.
	std::shared_ptr<const NativeShieldDepthFrame> BorrowActualMRTModelDepth(const WorldFrame&);
	void AbortShieldPublications(std::shared_ptr<const NativeOpaqueModelFrame> unsubmittedOpaque = {},
  std::shared_ptr<const NativeOriginalShadowFrame> unsubmittedShadow = {},
  std::shared_ptr<const NativeTerrainSceneSourceFrame> unsubmittedGround = {}) noexcept;
	void ConnectShieldTextureContext(bool connected);
	void ConnectPBRSourceContext(bool connected);
	std::shared_ptr<const void> AllocateCoverageTarget(const std::shared_ptr<const ScreenTexture>& color,
		const std::shared_ptr<const ScreenTexture>& depth);
	std::shared_ptr<const NativeBlockoutTarget> AllocateBlockoutTarget(uint32_t width,uint32_t height);
	void ConstructOriginalModelPrograms(NativeEngineModelProgramSource&);
	void SetOriginalShadowCapture(std::function<void(std::shared_ptr<const NativeShadowTextureFrame>)> actualCapture);
	void ConstructOriginalShadowPrograms(NativeEngineShadowProgramSource&,const WorldCamera& actualShadowDrawingCamera);
	void ConstructOriginalShadowParticlePrograms(NativeOriginalShadowParticlesProgramSource&);
	std::shared_ptr<const NativeCUSProgramConsumer> CompileOriginalCUSProgram(const NativeCUSProgramSource&);
	std::shared_ptr<const NativeDeferredLightProgramConsumer> CompileOriginalDeferredLightProgram(const NativeDeferredLightProgramSource&);
	std::shared_ptr<const NativeUnitStencilProgramConsumer> CompileOriginalUnitStencilProgram(const NativeUnitStencilProgramSource&);
	std::shared_ptr<const NativeOutlineProgramConsumer> CompileOriginalOutlineProgram(const NativeOutlineProgramSource&);
	std::shared_ptr<const NativeDistortionProgramConsumer> CompileOriginalDistortionProgram(const NativeDistortionProgramSource&);
	NativeTerrainProgramLink CompileOriginalTerrainProgram(const NativeTerrainProgramRequest&);
	void ClearOriginalModelSources();
	bool HasPendingPBRSource() const;
	bool HasPendingEnvironmentSource() const;
	// Enable only after the selected original deferred normal/depth producers
	// are genuinely connected to the shared logical names and this draw sequence.
	void ConnectSSAODeferredSources(bool connected);
	void ConnectBloomDeferredSources(bool connected);


private:
	Metal4WorldPresentationResult ContinueScreen(const WorldFrame&,const Metal4WorldRenderOptions&);
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace rendering
