/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeGenericShadowData.h"
#include "NativeDisplayListReference.h"

#include "ScreenDrawList.h"
#include "NativeScreenLine.h"
#include "NativeWorldDrawRecorder.h"
#include "NativePostprocessData.h"
#include "NativeUITextureObserver.h"
#include <functional>
#include <memory>
#include <string>

namespace rendering { struct WorldModel; struct NativeMiniMapIcon; struct NativeWorldUnitIcon; }

namespace rendering::ui {

struct NativeUICommandList;
using NativeUIMatrix = std::array<float, 16>; // column-major, post-multiplied
enum class NativeUIMatrixMode { ModelView, Projection, Texture };
enum NativeUIAttribute : unsigned { Current = 1, Blend = 2, Scissor = 4, Texture = 8, Alpha = 16, Enable = 32,
	Depth = 64, Polygon = 128, Stencil = 256, Line = 512, Point = 1024, ColorMask = 2048, Viewport = 4096, Multisample = 8192, All = 16383 };
struct NativeUIFrameRasterMapping {
	std::uint32_t windowLogicalWidth = 0, windowLogicalHeight = 0;
	std::uint32_t renderWidth = 0, renderHeight = 0;
	ScreenViewport defaultViewport;
	unsigned samples = 1; // Actual root attachment count published by the owning renderer.
};

struct NativeUITextureBinding {
	std::shared_ptr<const ScreenTexture> current;
	ScreenSampler sampler;
	std::optional<ScreenSampler> requestedSampler; // Original Lua texture description; GL state can be list-deferred.
	// Unit-definition images keep their own stable cell while delegating a
	// Spring.SetUnitDefImage override to the Lua-owned RTT cell. No Lua state is
	// borrowed, and the latest immutable version is resolved at list replay.
	std::shared_ptr<NativeUITextureBinding> source;
	std::shared_ptr<NativePostprocessBinding> postprocess;
	// Storage transport for CopyToTexture on this existing FBO. Resolve keeps
	// using current; copies and RTTs publish to that one original cell.
	std::shared_ptr<NativePostprocessBinding> framebufferCopies;
	struct Resolved {
		std::shared_ptr<const ScreenTexture> image;
		ScreenSampler sampler;
	};
	Resolved Resolve() const;
};
struct NativeUILuaTextureLookup {
	bool parsed=false;
	std::shared_ptr<NativeUITextureBinding> binding;
	std::shared_ptr<const ScreenTexture> image;
	ScreenSampler sampler;
};

// A non-destructive owned command prefix. Its View borrows only this object's
// immutable storage, not vectors that a resumed Lua callback can reallocate.
struct NativeUIReadbackPrefix {
	std::shared_ptr<const ScreenDrawList> root;
	std::vector<ScreenRenderTargetPass> targets;
	std::uint32_t coordinateWidth=0, coordinateHeight=0;
	std::uint64_t cutoff=0;
	bool screenStage=false; // Original callin, not the target-local projection.
	NativePostprocessStage readSourceStage=NativePostprocessStage::Screen;
	std::shared_ptr<const ScreenTexture> readColor, readDepth;
	ScreenFrameView View() const { return {root.get(),coordinateWidth,coordinateHeight,{},targets}; }
};

// CPU interpretation for the original planar screen Lua API. Lists retain
// relative commands/resources, not already transformed frame triangles.
class NativeUIRecorder {
public:
	NativeUIRecorder();
	~NativeUIRecorder();
	void BeginFrame(std::uint32_t logicalWidth, std::uint32_t logicalHeight,
		std::optional<NativeUIFrameRasterMapping> rasterMapping = std::nullopt);
	ScreenFrameView EndFrame();
	// SaveImage's active target prefix is sealed into a real immutable pass;
	// subsequent commands LOAD a fresh version without ending the Lua scope.
	NativeUIReadbackPrefix CaptureReadbackPrefix();
	// Only after the common runtime confirms this exact submission completed.
	// May retire a prefix while the original callback/target remains open.
	void RetireTargetSubmission(std::uint64_t actualSubmission);
	// Original root-command insertion point for screen-dependent copy/effect passes.
	std::uint32_t RootCommandCount() const;
	// Keep published producer generations until their GPU work completes,
	// including target-only submissions while the window cannot present.
	void RetireCompletedFrame(bool sourceFrameCompleted = true);
	void SetSourceDependencyQuery(std::function<bool(const std::shared_ptr<const NativePostprocessVersion>&)> query);
	void CommitTargetBinding(std::shared_ptr<NativeUITextureBinding> binding,
		std::shared_ptr<const ScreenTexture> previous,std::shared_ptr<const ScreenTexture> published);
	void CommitCopiedTargetBinding(std::shared_ptr<NativeUITextureBinding> binding,
		std::shared_ptr<const ScreenTexture> previous,std::shared_ptr<const ScreenTexture> published);
	void CommitTargetPair(std::shared_ptr<NativeUITextureBinding> color,std::shared_ptr<const ScreenTexture> previousColor,
		std::shared_ptr<const ScreenTexture> nextColor,std::shared_ptr<NativeUITextureBinding> depth,
		std::shared_ptr<const ScreenTexture> previousDepth,std::shared_ptr<const ScreenTexture> nextDepth);
	// Common renderer only: actual successful Begin has created this command.
	// Preserve exact publication identities through uncertain/retired failure;
	// no allocation or Lua/source callback occurs after Begin here.
	void RecordTargetSubmission(std::span<const ScreenRenderTargetPass> passes,std::uint64_t actualSubmission,
		std::shared_ptr<const NativePostprocessFrame> actualSourceFrame = {}) noexcept;
	void AbortFrame() noexcept;
	void BeginScreenStage();
	void ResetScreenStage();
	void EndScreenStage() noexcept;
	void BeginWorldStage(NativeWorldDrawStage stage,NativeUIMatrix view,NativeUIMatrix projection,NativeUIMatrix billboard,bool safeMode,bool zeroToOneDepth = false);
	void ResetWorldStage();
	void EndWorldStage();
	bool IsWorldStage() const noexcept;
	bool IsScreenStage() const noexcept;
	// Actual DrawGenesis call-in, excluding transport-only resource stages.
	bool IsGenesisStage() const noexcept;
	bool HasActiveTarget() const noexcept;
	// Read-only stage identity for real source-frame copies and typed programs.
	std::optional<NativeWorldDrawStage> CurrentWorldStage() const noexcept;
	// Retained external commands execute against the replaying recorder and its
	// current source stage, never the CreateList compiler's captured stage.
	void RecordOperation(std::function<void(NativeUIRecorder&)> operation);
	// The operation mutates only explicitly addressed state fields at replay,
	// retaining relative list semantics instead of freezing caller defaults.
	void UpdateWorldState(std::function<void(NativeWorldDrawState&)> operation,bool requireWorld = true,bool legacyPrimitiveRules = false);
	NativeWorldDrawState CurrentWorldState() const;
	void BeginMiniMapStage(bool safeMode);
	void ResetMiniMapStage();
	void EndMiniMapStage(bool safeMode,bool returnToScreen);
	// Original PiP forwards another widget callback after its own screen
	// viewport/matrix setup. This scope owns only that immediate invocation.
	void RunPiPMiniMapCallback(const std::function<void()>& callback);
	void ResetDrawingState();
	void ResetDrawingMatrices();
	// Offscreen transport alone does not issue original EnableCommon events.
	void BeginResourceStage();
	void EndResourceStage();
	void BeginGenesisStage(bool safeMode);
	void ResetGenesisStage();
	void EndGenesisStage();
	void RunFramebufferState(std::uint32_t width,std::uint32_t height,bool identities,const std::function<void()>&);
 // Scalar/matrix scope for an actual typed DrawWorld RTT producer. No screen
 // target/storage/pass is created. The family owns every target draw/clear.
 // As original RenderToTexture: restore projection/modelview+viewport, finish
 // in ModelView; scissor and other callback state writes remain live.
 void RunTypedTargetState(std::uint32_t width,std::uint32_t height,const std::function<void()>&);
 bool HasActiveTypedTarget() const noexcept;
	bool IsActive() const noexcept;
	bool IsCompiling() const noexcept;
	void BeginList();
	std::shared_ptr<const NativeUICommandList> EndList();
	void AbortList() noexcept;
	void CallList(std::shared_ptr<const NativeUICommandList> list);
	void CallList(NativeDisplayListReference sourceName);
	void Begin(ScreenPrimitive primitive,std::string origin = {});
	void Vertex(float x, float y, float z = 0, float w = 1);
	void End();
	// Original Lua BeginEnd calls GL unconditionally. A nested Begin sets the
	// invalid-operation flag without changing the active primitive; an inner
	// End closes that actual batch, and an extra End/Vertex outside is ignored
	// with the same error. Keep this adapter separate from strict CPU methods.
	void BeginLegacyPrimitive(ScreenPrimitive primitive,std::string origin = {});
	void BeginLegacyPrimitive(NativeDrawPrimitive primitive,std::string origin = {});
	void LegacyVertex(float x,float y,float z = 0,float w = 1);
	void EndLegacyPrimitive();
	bool TakeLegacyInvalidOperation() noexcept;
	std::uint64_t LegacyInvalidOperationCount() const noexcept;
	void AbortPrimitive() noexcept;
	void Color(ScreenColor value);
	// Current normals are retained with the original attribute/list state.
	// Planar, unlit screen draws do not consume them as lighting inputs.
	void Normal(float x,float y,float z);
	std::array<float,3> CurrentNormal() const;
	void TexCoord(float u, float v);
	void MultiTexCoord(unsigned unit, float u, float v);
	void SetBlend(ScreenBlendState value);
	void UpdateBlend(std::function<void(ScreenBlendState&)> operation);
	void SetScissor(std::optional<ScreenScissor> value);
	void SetScissorEnabled(bool enabled);
	void SetViewport(ScreenViewport value);
	// Non-owning shared observer; the actual native context owner installs and
	// detaches it. Ordinary public Lua calls must use the typed Lua methods.
	void SetTextureObserver(NativeUITextureObserver*);
	NativeUITextureObserver* TextureObserver() const;
	void LuaTextureEnabled(bool enabled,unsigned requestedUnit);
	void LuaActiveTexture(unsigned actualUnit);
	void LuaTextureImage(std::shared_ptr<const ScreenTexture>,ScreenSampler,unsigned requestedUnit);
	void LuaTextureResource(std::shared_ptr<NativeUITextureBinding>,unsigned requestedUnit);
	void LuaTextureNoBind(unsigned requestedUnit,bool parsed);
	// Parser runs synchronously exactly where original gl.Texture performs it.
	// A compiled list retains only its resolved name, never a Lua callback.
	bool LuaTextureLookup(unsigned requestedUnit,const std::function<NativeUILuaTextureLookup()>& actualParser);
	// A typed family already issued the shared original texture event. Retain
	// its current logical binding for ordinary draws without writing it twice.
	void SyncObservedTextureBinding(unsigned actualUnit);
	// Exact original EnableCommon/DisableCommon texture effects for Screen and
	// Genesis; transport-only native stages must not invent these GL events.
	void BeginOriginalTextureScope(bool safeMode,bool screenCallin);
	void ResetOriginalTextureState();
	void EndOriginalTextureScope();
	// Internal typed-family synchronization: its original event was captured
	// by that family, so these setters do not issue a second GL event.
	void SetTexture(std::shared_ptr<const ScreenTexture> texture, ScreenSampler sampler = {}, unsigned unit = 0);
	void SetMiniMapBackground(std::shared_ptr<const NativeMiniMapBackground> background);
	void SetTextureResource(std::shared_ptr<NativeUITextureBinding> resource, unsigned unit = 0);
	// GL_TEXTURE_2D enable state is independent of its bound texture object.
	void SetTextureEnabled(bool enabled, unsigned unit = 0);
	void BeginTarget(std::shared_ptr<const ScreenTexture> destination, std::shared_ptr<const ScreenTexture> previous = {},
		std::shared_ptr<const ScreenTexture> depthDestination = {}, std::shared_ptr<const ScreenTexture> previousDepth = {},
		bool identities = true,std::uint64_t actualSourceDrawFrame = 0,
		std::shared_ptr<NativeUITextureBinding> colorBinding = {},std::shared_ptr<NativeUITextureBinding> depthBinding = {},bool editorTarget = false);
	void EndTarget();
	void AbortTarget() noexcept;
	void AppendPiP(NativePiPDraw draw);
	void AppendStartBox(NativeStartBoxDraw draw);
	void AppendWeaponRange(NativeWeaponRangeDraw draw);
	void AppendBuildSquare(NativeBuildSquareDraw draw);
 // Immediate engine/PiP minimap callback marker; retains the already captured packet.
 void AppendEasyFacing(std::span<const std::array<float,4>>,std::shared_ptr<const NativeEasyFacingInput>);
 void AppendUnitIcons(std::shared_ptr<const NativeUnitPrimitiveFrame>);
	void AppendSensorLOS(std::shared_ptr<const NativeSensorLOSDraw>);
 void AppendSensorPreview(std::shared_ptr<const NativeSensorPreviewDraw> draw);
 void AppendHealthBars(std::shared_ptr<const NativeHealthBarsDraw> draw);
	void AppendPanelShader(NativePanelShaderDraw draw);
	bool IsMiniMapStage() const;
	void ClearTarget(std::optional<ScreenColor> color = {});
	using WorldClearHandler = std::function<void(unsigned, const ScreenClearValues&, const NativeWorldDrawState&,
		std::optional<ScreenScissor>, NativeWorldDrawStage)>;
	void SetWorldClearHandler(WorldClearHandler handler);
	using ShadowVertexSource = std::function<std::shared_ptr<const NativeGenericShadowInput>(
		std::span<const NativeGenericShadowVertex>,const NativeUIMatrix&,const NativeUIMatrix&,bool)>;
	// Snapshot only at actual shadow replay, before the common command is
	// queued. The source callback cannot issue another ordinal or draw.
	void SetShadowVertexSource(ShadowVertexSource);
	void ClearAttachments(unsigned mask, std::optional<ScreenColor> color = {},
		std::optional<float> depth = {}, std::optional<std::int32_t> stencil = {});
	ScreenClearValues CurrentClearValues() const;
 // Original glClearColor/glClearDepth source writes; these change state only.
 void SetOriginalClearColor(ScreenColor);
 void SetOriginalClearDepth(double);
 NativeUIMatrixMode CurrentMatrixMode() const;
	void SetAlphaTest(ScreenAlphaTest value);
	ScreenDrawListState CurrentState() const;
	void MatrixMode(NativeUIMatrixMode mode);
	void LoadMatrix(NativeUIMatrix value);
	void MultMatrix(NativeUIMatrix value);
	NativeUIMatrix GetMatrix(NativeUIMatrixMode mode) const;
	void PushMatrix();
	void PopMatrix();
	void PushAttrib(unsigned mask);
	void PopAttrib();
	void AppendWaterOverlay(std::span<const std::array<float,2>>,std::shared_ptr<const NativeWaterOverlayInput>);
	void AppendLOSView(std::span<const std::array<float,4>>,std::shared_ptr<const NativeLOSViewInput>);
	void AppendTerraformOverlay(std::span<const std::array<float,4>>,std::shared_ptr<const NativeTerraformOverlayInput>);
	void AppendSquadHull(std::span<const std::array<float,3>>,NativeDrawPrimitive,std::shared_ptr<const NativeSquadHullInput>);
	void AppendProfilerHistogram(std::vector<std::array<float,2>>,std::array<float,2> originalViewGeometry,std::array<float,4> boundingBox,ScreenColor color);
	void AppendTerraformCapture(std::shared_ptr<const NativeTerraformCaptureInput>,std::vector<ScreenVertex>,std::vector<ScreenTexCoord>,std::shared_ptr<const WorldModel> model={});
	void AppendDiffusePainter(std::shared_ptr<const NativeDiffusePainterInput>,std::vector<ScreenVertex>,std::vector<ScreenTexCoord>);
	void AppendSplatPainter(std::shared_ptr<const NativeSplatPainterInput>,std::vector<ScreenVertex>,std::vector<ScreenTexCoord>);
	void AppendDecalCapture(std::shared_ptr<const NativeDecalCaptureInput>,std::vector<ScreenVertex>,std::vector<ScreenTexCoord>);
	void AppendSpectatorHUDTriangles(std::vector<ScreenVertex>,std::vector<ScreenTexCoord>);
	void AppendFrameGrapherTriangles(std::vector<ScreenVertex>,std::array<float,2> originalViewGeometry,std::array<float,4> shaderParameters);
	void AppendDeferredVisualizer(std::shared_ptr<const NativeDeferredVisualizerInput>);
	void AppendTargetPriorityTriangles(std::vector<ScreenVertex>,std::vector<ScreenTexCoord>,std::array<float,2> originalViewGeometry);
	void AppendFontTriangles(std::vector<ScreenVertex> vertices, std::vector<ScreenTexCoord> uv,
		std::vector<std::uint32_t> indices, std::shared_ptr<const ScreenTexture> atlas, bool callerBlending);
	// Original UnitDrawer's filtered/sorted quads under the current caller
	// matrices and target. The icon shader owns its UV/color/alpha inputs.
	void AppendMiniMapIcons(std::vector<NativeMiniMapIcon> icons, bool secondAtlas);
	// Same immutable quad transport, original SCREEN alpha > 0.05, blending
	// and depth-test override; positions are normalized to the main view.
	void AppendScreenUnitIcons(std::vector<NativeMiniMapIcon> icons, bool secondAtlas);
	void AppendWorldUnitIcons(std::vector<NativeWorldUnitIcon> icons, bool secondAtlas, bool alphaToCoverage);
	void DrawLines(std::vector<ScreenLine> lines);
	// Original minimap projectile points use the default, non-smooth size 1.
	void DrawPixelPoints(std::vector<ScreenVertex> points);

private:
	void AppendEngineIcons(std::vector<NativeMiniMapIcon> icons, bool secondAtlas, bool screenIcons);
	friend struct NativeUICommandList;
	struct Impl;
	std::unique_ptr<Impl> impl;
};

NativeUIRecorder& GetRecorder();
std::shared_ptr<ScreenTexture> MakeRenderTargetTexture(std::uint32_t width,std::uint32_t height,const std::string& sourceName);
std::shared_ptr<ScreenTexture> MakeRenderTargetTexture(std::uint32_t width,std::uint32_t height,const std::string& sourceName,ScreenTextureFormat format);
NativeUIMatrix IdentityMatrix();
NativeUIMatrix TranslationMatrix(float x, float y, float z);
NativeUIMatrix ScaleMatrix(float x, float y, float z);
NativeUIMatrix RotationMatrix(float degrees, float x, float y, float z);
NativeUIMatrix OrthoMatrix(float left, float right, float bottom, float top, float nearValue, float farValue);

} // namespace rendering::ui
