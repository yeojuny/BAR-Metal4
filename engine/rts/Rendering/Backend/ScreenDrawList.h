/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <vector>
#include "NativePostprocessData.h"
#include "NativeDepthStencilState.h"

namespace rendering {

struct NativeMiniMapBackground;
struct NativePiPDraw;
struct NativeStartBoxDraw;
struct NativeWeaponRangeDraw;
struct NativeBuildSquareDraw;
struct NativeSensorPreviewDraw;
struct NativeSensorLOSDraw;
struct NativeUnitPrimitiveFrame;
struct NativeHealthBarsDraw;
struct NativePanelShaderDraw;
struct NativePostprocessFrame;
struct NativePostprocessVersion;
struct NativeCUSTextureObject;
struct ScreenTextureLevelDefinitions;

using ScreenColor = std::array<float, 4>;
// Two independent planar fixed-function texture units: (u0,v0,u1,v1).
using ScreenTexCoord = std::array<float, 4>;

enum class ScreenTextureFormat : std::uint8_t { R8Unorm, RGBA8Unorm, BGRA8Unorm, BC1RGBA, BC2RGBA, BC3RGBA,
	RGBA16Float, Depth32Float, Depth24Unorm, RG16Float,
	// Retained original GPU color storage; actual format lives in sourceTexture.
	ExternalColor, R16Float, R32Float, RG11B10Float, Depth16Unorm, RG8Unorm,
	BC4Unorm, BC5Unorm, BC7RGBA, BC7RGBASRGB };

// Original Lua color RTT storage. Zero denotes a format without this producer.
std::uint32_t ScreenRenderTargetPixelBytes(ScreenTextureFormat format) noexcept;

// Logical identity shared by immutable versions of one original Lua RTT.
// Single-sample color storage; no Apple handles escape through this contract.
struct ScreenRenderTargetIdentity {
	std::uint64_t resourceId = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

struct ScreenTextureMip {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::size_t bytesPerRow = 0;
	std::vector<std::uint8_t> bytes;
	// A real TexImage allocation with nullptr data. Native allocation must
	// leave these texels undefined; it must not upload an invented clear.
	bool allocatedUndefined = false;
};

// One immutable owned image generation. Never mutate a snapshot referenced by
// a command/list; publish a new shared object when the real atlas changes.
struct ScreenTexture {
	std::string sourceName;
	std::uint64_t generation = 0;
	ScreenTextureFormat format = ScreenTextureFormat::RGBA8Unorm;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::size_t bytesPerRow = 0;
	std::vector<std::uint8_t> bytes;
	bool flipV = false;
	// Original texture zero after an explicit missing DDS build picture.
	// It has no storage. Original fixed-function blending skips this unit;
	// explicit shader sampling has its separate incomplete-texture contract.
	bool incomplete = false;
	// Complete original chain including mip 0. Required for BC; optional for
	// uncompressed images. With mips present, legacy bytes/bytesPerRow are empty
	// and width/height equal mip 0. This avoids competing image descriptions.
	// DDS loaders publish the original engine-flipped bytes, not a substituted
	// UV flip: the legacy loader's tiny BC tail-mip behavior must be retained.
	std::vector<ScreenTextureMip> mips;
	// RTT versions carry no CPU image/mip payload. Their frame-owned producer
	// pass supplies the contents. flipV=true preserves GL framebuffer UV origin.
	std::shared_ptr<const ScreenRenderTargetIdentity> renderTarget;
	// Real shared framebuffer-copy/typed-program contents. No CPU payload or
	// competing panel target identity; the persistent image provider resolves
	// this exact immutable version, never the latest mutable image name.
	std::shared_ptr<const NativePostprocessVersion> postprocessVersion;
	// Actual logical object/version frozen at the source draw. The native image
	// provider resolves its retained backing; no current-name lookup or CPU copy.
	std::shared_ptr<const NativeCUSTextureObject> sourceTexture;
	// Set by the actual producer owner, never inferred from a frame number.
	// Cached targets using this version must wait for its original source stage.
	bool sourceDependent = false;
	// Generate the full native GPU chain from the real immutable base only.
	// Never replace embedded source mip levels. Completion owns publication.
	bool generateMipmaps = false;
	// Logical TexImage definitions survive a redefinition of just one level.
	// The compatible CPU chain above is a sampled view, not the entire object.
	std::shared_ptr<const ScreenTextureLevelDefinitions> levelDefinitions;
};

// Validate the generated-image producer boundary before native encoding.
// Embedded DDS levels are immutable; base-only BC uses decoded upload storage.
void ValidateScreenTextureMipGeneration(const ScreenTexture& texture);
std::shared_ptr<const ScreenRenderTargetIdentity> ScreenTextureTargetIdentity(const ScreenTexture& texture);

enum class ScreenMaterial : std::uint8_t { Solid, RGBA, FontCoverage, MiniMapBackground, TargetPriorityStroke, FrameGrapher, DeferredVisualizer, SpectatorHUD, DecalCapture, SplatPainter, DiffusePainter, TerraformCapture };
enum class ScreenUVMode : std::uint8_t { Normalized, Texel };
enum class ScreenFilter : std::uint8_t { Nearest, Linear };
enum class ScreenMipFilter : std::uint8_t { None, Nearest, Linear };
enum class ScreenWrap : std::uint8_t { ClampEdge, TransparentBorder, Repeat, LegacyClamp };
struct ScreenSampler {
	ScreenFilter filter = ScreenFilter::Linear;
	ScreenWrap wrapS = ScreenWrap::ClampEdge;
	ScreenWrap wrapT = ScreenWrap::ClampEdge;
	ScreenMipFilter mipFilter = ScreenMipFilter::None;
	std::optional<ScreenFilter> magnificationFilter = std::nullopt;
	std::uint32_t anisotropy = 1;
	bool whiteBorder = false;
	bool operator==(const ScreenSampler&) const = default;
};
// Legacy GL_CLAMP is distinct from edge and full-border addressing. This
// bounded original-atlas slice supports matching min/mag filters without
// mip filtering; the default border is transparent black, with border width0.
void ValidateScreenSampler(const ScreenSampler& value);

struct ScreenAlphaTest {
	bool enabled = false;
	ScreenAlphaCompare compare = ScreenAlphaCompare::Greater;
	float reference = 0;
	bool operator==(const ScreenAlphaTest&) const = default;
};
enum class ScreenPrimitive : std::uint8_t { Triangles, TriangleStrip, TriangleFan, Quads };

// Coordinates are screen pixels with bottom-left origin, x right and y up.
// z is GL normalized clip depth (w=1), including values outside [-1,1] that
// require primitive clipping. NativeUIRecorder supplies transformed z; direct
// callers supply the same contract. The GPU maps z once to Metal [0,1], with
// attachment-aware depth comparison/writes. This list does not remap depth.
struct ScreenVertex {
	float x;
	float y;
	float z;
	ScreenColor color;
};

static_assert(std::is_standard_layout_v<ScreenVertex>);
static_assert(sizeof(float) == 4 && sizeof(ScreenVertex) == 28);
static_assert(offsetof(ScreenVertex, x) == 0);
static_assert(offsetof(ScreenVertex, y) == 4);
static_assert(offsetof(ScreenVertex, z) == 8);
static_assert(offsetof(ScreenVertex, color) == 12);

enum class ScreenBlendFactor : std::uint8_t {
	Zero,
	One,
	SourceAlpha,
	OneMinusSourceAlpha,
	DestinationAlpha, OneMinusDestinationAlpha,
	SourceColor, OneMinusSourceColor,
	DestinationColor, OneMinusDestinationColor,
	SourceAlphaSaturate,
};

enum class ScreenBlendEquation : std::uint8_t { Add, Subtract, ReverseSubtract, Minimum, Maximum };

struct ScreenBlendState {
	bool enabled = true;
	ScreenBlendFactor sourceColor = ScreenBlendFactor::SourceAlpha;
	ScreenBlendFactor destinationColor = ScreenBlendFactor::OneMinusSourceAlpha;
	ScreenBlendFactor sourceAlpha = ScreenBlendFactor::SourceAlpha;
	ScreenBlendFactor destinationAlpha = ScreenBlendFactor::OneMinusSourceAlpha;
	// Original RGB/alpha equations remain independent; MIN/MAX ignore factors.
	ScreenBlendEquation colorEquation = ScreenBlendEquation::Add;
	ScreenBlendEquation alphaEquation = ScreenBlendEquation::Add;
	bool operator==(const ScreenBlendState&) const = default;
};

struct ScreenScissor {
	std::int32_t x;
	std::int32_t y;
	std::uint32_t width;
	std::uint32_t height;
	// Lua gl.Scissor is absolute window coordinates after adding viewPos.
	// Existing direct C++ callers retain their frame-coordinate basis.
	bool windowCoordinates = false;
	bool operator==(const ScreenScissor&) const = default;
};

struct ScreenViewport {
	std::int32_t x = 0, y = 0;
	std::uint32_t width = 0, height = 0;
	bool operator==(const ScreenViewport&) const = default;
};

struct ScreenSecondaryTexture {
	std::shared_ptr<const ScreenTexture> texture;
	ScreenUVMode uvMode = ScreenUVMode::Normalized;
	ScreenSampler sampler;
	bool operator==(const ScreenSecondaryTexture&) const = default;
};

enum class ScreenCull : std::uint8_t { None, Front, Back, FrontAndBack };

struct NativeDeferredVisualizerInput;
struct NativeDecalCaptureInput;
struct NativeSplatPainterInput;
struct NativeDiffusePainterInput;
struct NativeTerraformCaptureInput;
struct ScreenDrawState {
 std::shared_ptr<const NativeTerraformCaptureInput> terraformCapture;
 std::shared_ptr<const NativeDiffusePainterInput> diffusePainter;
 std::shared_ptr<const NativeSplatPainterInput> splatPainter;
 std::shared_ptr<const NativeDecalCaptureInput> decalCapture;
 std::shared_ptr<const NativeDeferredVisualizerInput> deferredVisualizer;
	// Internal raster metadata; center/radius are physical Metal pixels. The
	// absolute fourth component matches the attachment sample count; its
	// sign selects a disk (>0) or square (<0).
	std::optional<std::array<float,4>> pointDisk;
	bool alphaToCoverage = false;
	ScreenBlendState blend;
	std::array<bool,4> colorWrite{true,true,true,true};
	std::optional<ScreenScissor> scissor;
	std::shared_ptr<const ScreenTexture> texture;
	ScreenMaterial material = ScreenMaterial::Solid;
	std::array<float,2> targetPriorityViewGeometry{}; // Actual engine UBO for typed screen diagnostics.
	std::array<float,4> frameGrapherParameters{};
	ScreenUVMode uvMode = ScreenUVMode::Normalized;
	ScreenSampler sampler;
	ScreenAlphaTest alphaTest;
	std::shared_ptr<const NativeMiniMapBackground> minimapBackground;
	ScreenSecondaryTexture secondary;
	ScreenDepthStencilState depthStencil;
	// Clear vertices use the owning frame coordinates; the consumer uses the
	// full attachment viewport and applies only the original scissor/masks.
	bool attachmentClear = false;
	// Absolute bottom-left logical window rectangle. Unset uses the host
	// default viewport; zero extent is valid and produces no raster samples.
	std::optional<ScreenViewport> viewport;
	ScreenCull cull = ScreenCull::None;
	bool operator==(const ScreenDrawState&) const = default;
};

struct ScreenDrawListState {
	ScreenColor color{1, 1, 1, 1};
	ScreenTexCoord texCoord{0, 0};
	ScreenDrawState draw;
};

struct ScreenDrawCommand {
	// Ordinary commands address indexed triangles; typed PiP markers preserve
	// their exact stream offset with indexCount=0 and their own immutable data.
	std::uint32_t firstIndex;
	std::uint32_t indexCount;
	ScreenDrawState state;
	std::shared_ptr<const NativePiPDraw> pip;
	std::shared_ptr<const NativeStartBoxDraw> startBox;
	std::shared_ptr<const NativeWeaponRangeDraw> weaponRange;
	std::shared_ptr<const NativeSensorPreviewDraw> sensorPreview;
 std::shared_ptr<const NativeSensorLOSDraw> sensorLOS;
 std::shared_ptr<const NativeUnitPrimitiveFrame> unitIcons;
	std::shared_ptr<const NativeBuildSquareDraw> buildSquare;
	std::shared_ptr<const NativeHealthBarsDraw> healthBars;
	std::shared_ptr<const NativePanelShaderDraw> panelShader;
};

std::array<std::shared_ptr<const ScreenTexture>,13> ScreenCommandImages(const ScreenDrawCommand& command);
bool ScreenCommandRequiresMapSource(const ScreenDrawCommand&);

// CPU command list for the planar, untextured FlowUI RectRound slice.
// Defaults reproduce pinned LuaOpenGL::ResetGLState's supported subset: white
// current color, smooth per-vertex colors, blend Add/SrcAlpha/OneMinusSrcAlpha
// for color and alpha, and scissor disabled. Depth/culling/textures/projection
// remain the consuming pass's responsibility and are not emulated here.
class ScreenDrawList {
public:
	void Color(ScreenColor value);
	void TexCoord(float u, float v);
	void MultiTexCoord(unsigned unit, float u, float v);
	void SetBlend(ScreenBlendState value);
	void SetScissor(std::optional<ScreenScissor> value);
	void SetTexture(std::shared_ptr<const ScreenTexture> texture, ScreenMaterial material = ScreenMaterial::RGBA,
		ScreenUVMode uvMode = ScreenUVMode::Normalized, ScreenSampler sampler = {});
	void SetSecondaryTexture(ScreenSecondaryTexture value);
	void SetMiniMapBackground(std::shared_ptr<const NativeMiniMapBackground> background);
	void SetAlphaTest(ScreenAlphaTest value);
	void SetSampler(ScreenSampler value);
	void SetUVMode(ScreenUVMode value);
	[[nodiscard]] ScreenDrawListState CaptureState() const { return {currentColor, currentTexCoord, currentState}; }
	void RestoreState(const ScreenDrawListState& value);

	void Begin(ScreenPrimitive primitive);
	void BeginQuads();
	void Vertex(float x, float y, float z = 0.0f);
	void End();
	// Discard unfinished geometry; color mutations remain visible as in the
	// source helper. Caller error policy decides whether the whole pass fails.
	void Abort() noexcept;
	void ClearGeometry();
	// Actual source triangle geometry (font outline then primary, in source
	// order). Indices are local to supplied vertices. Does not change current
	// recorder state. UV stream is parallel to the preserved 28-byte ABI.
	void AppendTriangles(std::span<const ScreenVertex> sourceVertices, std::span<const ScreenTexCoord> sourceUVs,
		std::span<const std::uint32_t> sourceIndices, const ScreenDrawState& state);
	// Ordered color-attachment replacement, independent of transform, texture,
	// current color, blend and alpha test. The full attachment basis is explicit;
	// an optional bottom-left scissor restricts the clear, as original gl.Clear.
	void AppendPiP(std::shared_ptr<const NativePiPDraw> draw);
	void AppendStartBox(std::shared_ptr<const NativeStartBoxDraw> draw);
	void AppendWeaponRange(std::shared_ptr<const NativeWeaponRangeDraw> draw);
	void AppendBuildSquare(std::shared_ptr<const NativeBuildSquareDraw> draw);
	void AppendUnitIcons(std::shared_ptr<const NativeUnitPrimitiveFrame>);
	void AppendSensorLOS(std::shared_ptr<const NativeSensorLOSDraw>);
 void AppendSensorPreview(std::shared_ptr<const NativeSensorPreviewDraw> draw);
	void AppendHealthBars(std::shared_ptr<const NativeHealthBarsDraw> draw);
	void AppendPanelShader(std::shared_ptr<const NativePanelShaderDraw> draw);
	void ClearColorAttachment(ScreenColor color, std::uint32_t width, std::uint32_t height,
		std::optional<ScreenScissor> scissor = std::nullopt,
		std::array<bool,4> colorWrite = {true,true,true,true});

	void ClearAttachments(unsigned mask, const ScreenClearValues& values, std::uint32_t width, std::uint32_t height,
		std::optional<ScreenScissor> scissor, std::array<bool,4> colorWrite,
		bool depthWrite, std::uint32_t stencilWriteMask);

	[[nodiscard]] bool IsRecording() const noexcept { return recording; }
	[[nodiscard]] ScreenColor CurrentColor() const noexcept { return currentColor; }
	[[nodiscard]] ScreenTexCoord CurrentTexCoord() const noexcept { return currentTexCoord; }
	[[nodiscard]] const ScreenDrawState& CurrentState() const noexcept { return currentState; }
	// Views belong to this list and can be invalidated by subsequent mutation.
	[[nodiscard]] std::span<const ScreenVertex> Vertices() const noexcept { return vertices; }
	[[nodiscard]] std::span<const ScreenTexCoord> TexCoords() const noexcept { return texCoords; }
	[[nodiscard]] std::span<const std::uint32_t> Indices() const noexcept { return indices; }
	[[nodiscard]] std::span<const ScreenDrawCommand> Commands() const noexcept { return commands; }

private:
	bool recording = false;
	ScreenColor currentColor{1.0f, 1.0f, 1.0f, 1.0f};
	ScreenTexCoord currentTexCoord{0, 0};
	ScreenPrimitive primitive = ScreenPrimitive::Quads;
	ScreenDrawState currentState;
	ScreenDrawState batchState;
	std::vector<ScreenVertex> pendingVertices;
	std::vector<ScreenTexCoord> pendingTexCoords;
	std::vector<ScreenVertex> vertices;
	std::vector<ScreenTexCoord> texCoords;
	std::vector<std::uint32_t> indices;
	std::vector<ScreenDrawCommand> commands;
};

enum class ScreenTargetLoad : std::uint8_t { Load, Clear, DontCare };
enum class ScreenTargetStore : std::uint8_t { Store, DontCare };
struct ScreenRenderTargetPass {
	std::shared_ptr<const ScreenTexture> destination;
	std::shared_ptr<const ScreenTexture> previous;
	std::shared_ptr<const ScreenDrawList> drawList;
	ScreenTargetLoad load = ScreenTargetLoad::Load;
	ScreenTargetStore store = ScreenTargetStore::Store;
	ScreenColor clearColor{0, 0, 0, 0};
	std::uint32_t coordinateWidth = 0;
	std::uint32_t coordinateHeight = 0;
	// Only targets consuming this source frame's copied/external images are
	// delayed to their original screen insertion point. Independent caches keep
	// their existing target-only path while hidden/minimized.
	bool sourceDependent = false;
	std::uint32_t screenCommandOffset = 0;
	std::uint64_t commandSequence = 0;
	// Recorder-owned receipt. Set only after the real command buffer Begin;
	// a suspended Lua callback cannot roll back submitted immutable contents.
	std::uint64_t actualSubmission = 0;
	bool editorTarget = false; // Explicit original editor color RTT, including empty SaveImage scopes.
	// Actual world RTT callin, separate from copied/map-image dependencies.
	// Actual StartBox PreUnit bake or paired range-coverage World FBO.
	NativePostprocessStage sourceStage = NativePostprocessStage::Screen;
	std::uint64_t sourceDrawFrame = 0;
	std::shared_ptr<const ScreenTexture> depthDestination,previousDepth;
};

// Borrowed immutable frame/list view through synchronous GPU retirement. The
// basis is logical UI pixels. An absent viewport covers the full window; an
// explicit bottom-left logical viewport maps through actual logical window
// dimensions to the independent requested raster pixels.
struct ScreenFrameView {
	const ScreenDrawList* drawList = nullptr;
	std::uint32_t coordinateWidth = 0;
	std::uint32_t coordinateHeight = 0;
	std::optional<ScreenScissor> logicalViewport;
	// Borrowed immutable producers through synchronous retirement. Versions
	// reference only lightweight identities, never an unbounded history graph.
	// Dependencies may be listed in any order; the consumer validates/sorts them.
	std::span<const ScreenRenderTargetPass> targetPasses{};
	// Ordered copies/MRT effects inserted among the original root commands.
	std::shared_ptr<const NativePostprocessFrame> postprocess;
};
inline bool ScreenFrameRequiresSource(const ScreenFrameView* frame)
{
	if(!frame)return false;
	if(frame->drawList)for(const auto& command:frame->drawList->Commands())if(ScreenCommandRequiresMapSource(command))return true;
	if(frame->postprocess && (!frame->postprocess->operations.empty() || !frame->postprocess->externalOutputs.empty()))return true;
	for(const auto& target:frame->targetPasses)if(target.sourceDependent || target.sourceStage!=NativePostprocessStage::Screen)return true;
	return false;
}
enum class NativeSourceSubmission { IndependentTargets, PresentWorld, OffscreenSource };
inline NativeSourceSubmission ChooseNativeSourceSubmission(bool presentRequested,bool hasCapturedWorld,bool canPresent,const ScreenFrameView* frame)
{
	if(!hasCapturedWorld)return NativeSourceSubmission::IndependentTargets;
	if(presentRequested && canPresent)return NativeSourceSubmission::PresentWorld;
	return ScreenFrameRequiresSource(frame)?NativeSourceSubmission::OffscreenSource:NativeSourceSubmission::IndependentTargets;
}

// Validate the bounded original-format RTT graph and return producer-before-consumer
// pass indices. Missing producers from earlier frames are checked by the GPU
// cache. Cycles, duplicate versions and discarded sampled outputs fail.
std::vector<std::size_t> OrderScreenRenderTargetPasses(const ScreenFrameView& frame);

} // namespace rendering
