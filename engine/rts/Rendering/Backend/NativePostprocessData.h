/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace rendering {
struct ScreenTexture;
struct NativeCUSUniformPublication;
struct NativeCASRaster;
struct ScreenRenderTargetIdentity;

enum class NativePostprocessFormat : std::uint8_t { RGBA8, RGBA16Float, Depth32, Depth24Unorm, BC1RGBA, R16Float, R32Float, RG8, R8Unorm };
static_assert(static_cast<std::uint8_t>(NativePostprocessFormat::RGBA8)==0 && static_cast<std::uint8_t>(NativePostprocessFormat::RG8)==7 && static_cast<std::uint8_t>(NativePostprocessFormat::R8Unorm)==8);
inline constexpr bool IsNativePostprocessDepth(NativePostprocessFormat format) { return format==NativePostprocessFormat::Depth32 || format==NativePostprocessFormat::Depth24Unorm; }
enum class NativePostprocessFilter : std::uint8_t { Nearest, Linear };
enum class NativePostprocessWrap : std::uint8_t { Repeat, ClampEdge, LegacyClamp };
struct NativePostprocessSampler {
	NativePostprocessFilter minFilter = NativePostprocessFilter::Linear;
	NativePostprocessFilter magFilter = NativePostprocessFilter::Linear;
	NativePostprocessWrap wrapS = NativePostprocessWrap::Repeat;
	NativePostprocessWrap wrapT = NativePostprocessWrap::Repeat;
	bool operator==(const NativePostprocessSampler&) const = default;
};

// Immutable identity and immutable contents version. The names below are native
// ownership tokens, never GL object IDs or evidence of a general shader API.
// Texels retain GL framebuffer semantics: normalized v=0 is the bottom row.
struct NativePostprocessImage {
	std::uint64_t object = 0;
	std::uint32_t width = 0, height = 0;
	NativePostprocessFormat format = NativePostprocessFormat::RGBA8;
	NativePostprocessSampler sampler;
	bool renderToTexture = false;
	// CopyTexSubImage into an existing Lua FBO keeps that SAME logical image.
	// This is provenance only; native contents live solely in this version.
	std::shared_ptr<const ScreenRenderTargetIdentity> originalScreenTarget;
};
struct NativePostprocessVersion {
	std::shared_ptr<const NativePostprocessImage> image;
	std::uint64_t generation = 0;
	// Immediate original CPU TexImage contents, resolved by the existing UI
	// upload cache. This is distinct from a declared/retired GPU producer.
	std::shared_ptr<const ScreenTexture> sourceImage;
};
struct NativePostprocessBinding {
	std::shared_ptr<const NativePostprocessImage> image;
	std::shared_ptr<const NativePostprocessVersion> current;
	std::uint64_t nextGeneration = 1;
	bool deleted = false;
};
// Shared main-thread allocator: private effect and public copy images must not
// have competing native identity counters across Lua contexts or games.
std::shared_ptr<NativePostprocessBinding> CreateNativePostprocessBinding(
	std::uint32_t width, std::uint32_t height, NativePostprocessFormat format,
	NativePostprocessSampler sampler, bool renderToTexture, std::uint32_t maximumDimension);
struct NativePostprocessRect { std::int32_t x = 0, y = 0; std::uint32_t width = 0, height = 0; };
struct NativeDOFUniforms {
	std::array<float, 16> projection{};
	std::array<float, 3> eyePosition{};
	std::array<float, 2> resolution{}, distanceLimits{}, mouseDepthCoordinate{};
	float autofocusFudgeFactor = 0, autofocusPower = 0, autofocusFocalLength = 0;
	float manualFocusDepth = 0, fStop = 0;
	std::int32_t autofocus = 0, mousefocus = 0, quality = 0, pass = 0;
	bool zeroToOneDepth = false;
};
// Actual statically used sampler units of each fixed31657 program. Disabled
// fixed-function texturing does not disable any of these GLSL sampler reads.
inline constexpr unsigned NativeDOFInputMask(std::int32_t pass, std::int32_t quality) {
	switch (pass) {
		case 0: return 0x03;
		case 1: case 3: return 0x21;
		case 2: return 0x2f;
		case 4: return 0x3f;
		case 5: return quality ? 0x07 : 0x03;
		case 6: case 7: return 0x01;
		default: return 0;
	}
}
struct NativePostprocessQuad {
	// Original post-projection GL clip bounds (w=1), and exact TexRect UVs.
	std::array<float, 4> clip{-1, -1, 1, 1};
	std::array<float, 4> uv{0, 0, 1, 1};
};
struct NativeCASUniforms {
 std::shared_ptr<const NativeCUSUniformPublication> engine;
 std::shared_ptr<const NativeCASRaster> raster;
 std::array<std::int32_t,4> viewRect{};
 float strength=0,groundY=0,debugFootprint=0;
 std::uint32_t variant=0;
};
struct NativeSepiaUniforms {
	std::shared_ptr<const NativeCUSUniformPublication> engine;
	std::shared_ptr<const NativeCASRaster> raster;
	std::array<std::int32_t,2> viewPos{};
	std::array<std::array<float,3>,3> rows{};
	float gamma=0;
};
enum class NativePostprocessOperationKind : std::uint8_t { CopyColor, CopyDepth, DOF, CAS, Sepia };
enum class NativePostprocessStage : std::uint8_t { PreDecals, PreUnit, World, Screen };
enum class NativeExternalImageFamily : std::uint8_t { VolumetricCloud, SensorPreview, MapBlockoutDepth };
struct NativeExternalImageProducer {
	NativeExternalImageFamily family = NativeExternalImageFamily::VolumetricCloud;
	NativePostprocessStage stage = NativePostprocessStage::PreUnit;
	std::uint64_t sequence = 0;
	std::vector<std::shared_ptr<const NativePostprocessVersion>> outputs, previous, inputs;
};
struct NativePostprocessOperation {
	NativePostprocessOperationKind kind = NativePostprocessOperationKind::DOF;
	NativePostprocessStage stage = NativePostprocessStage::Screen;
	// Actual common unsynced callback order; zero only for screen ordinals.
	std::uint64_t sequence = 0;
	// Insert after this many ORIGINAL root screen commands. PreparedScreenFrame
	// must retain these ordinals even if a command has zero raster coverage.
	std::uint32_t screenCommandOffset = 0;
	// Orders screen-copy/effect and nested RTT producers at the same root
	// command offset. It is not a GL handle and never resets between frames.
	std::uint64_t commandSequence = 0;
	std::vector<std::shared_ptr<const NativePostprocessVersion>> outputs;
	// Each previous version preserves untouched pixels; null means storage was
	// originally undefined. No invented clear, zero image or black substitute.
	std::vector<std::shared_ptr<const NativePostprocessVersion>> previous;
	// Actual canonical UI-cell contents before a CopyTexSubImage into its FBO.
	// A real RTT or copied version, never CPU contents or a second image name.
	std::shared_ptr<const ScreenTexture> priorScreen;
	std::array<std::shared_ptr<const NativePostprocessVersion>, 6> inputs{};
	std::array<bool, 6> inputEnabled{};
	NativePostprocessRect source, destination;
	// Original explicit GL viewport; RawBindFBO does not change this state.
	std::optional<NativePostprocessRect> viewport;
	NativeDOFUniforms uniforms;
 NativeCASUniforms cas;
 NativeSepiaUniforms sepia;
	NativePostprocessQuad quad;
	bool screenOutput = false;
};
struct NativePostprocessFrame {
	std::uint64_t serial = 0;
	// Real engine draw generation, not a simulated frame or transport counter.
	std::uint64_t sourceDrawFrame = 0;
	std::uint32_t logicalWidth = 0, logicalHeight = 0;
	std::vector<NativePostprocessOperation> operations;
	// A declaration is not executable success: its actual paired family must
	// encode the producer and acknowledge it before native contents retire.
	std::vector<NativeExternalImageProducer> externalOutputs;
};

struct NativePostprocessCommandRef { bool external = false; std::size_t index = 0; };
struct NativePostprocessConsumer {
	NativePostprocessStage stage = NativePostprocessStage::Screen;
	std::uint64_t sequence = 0;
	std::uint32_t screenCommandOffset = 0;
	std::uint64_t commandSequence = 0;
};
void ValidateNativeExternalImageProducer(const NativeExternalImageProducer& producer);
std::vector<NativePostprocessCommandRef> OrderNativePostprocessCommands(const NativePostprocessFrame& frame);
void ValidateNativePostprocessImage(const NativePostprocessImage& image);
bool HasNativePostprocessCPUContents(const NativePostprocessVersion& version);
void ValidateNativePostprocessOperation(const NativePostprocessOperation& operation,
	std::uint32_t logicalWidth, std::uint32_t logicalHeight);
// Ordered, not topologically reordered: screen copies and framebuffer writes
// observe the precise original call order. The callback must report only truly
// retired external versions; missing or failed GPU contents are never accepted.
void ValidateNativePostprocessFrame(const NativePostprocessFrame& frame,
	bool (*isRetired)(const NativePostprocessVersion&, void*), void* state);
// A declared image is readable only after its actual source in the same
// callback order, or after a previous successful native retirement.
void ValidateNativePostprocessConsumer(const NativePostprocessFrame& frame,
	const NativePostprocessVersion& version, const std::optional<NativePostprocessConsumer>& consumer,
	bool (*isRetired)(const NativePostprocessVersion&, void*), void* state);

inline std::atomic<bool> nativeSepiaConsumerAvailable{false};
inline bool IsNativeSepiaConsumerAvailable() noexcept {return nativeSepiaConsumerAvailable.load(std::memory_order_acquire);}
inline void SetNativeSepiaConsumerAvailable(bool value) noexcept {nativeSepiaConsumerAvailable.store(value,std::memory_order_release);}
inline std::atomic<bool> nativeCASConsumerAvailable{false};
inline bool IsNativeCASConsumerAvailable() noexcept {return nativeCASConsumerAvailable.load(std::memory_order_acquire);}
inline void SetNativeCASConsumerAvailable(bool value) noexcept {nativeCASConsumerAvailable.store(value,std::memory_order_release);}
inline std::atomic<bool> nativeDOFConsumerAvailable{false};
inline bool IsNativeDOFConsumerAvailable() noexcept { return nativeDOFConsumerAvailable.load(std::memory_order_acquire); }
inline void SetNativeDOFConsumerAvailable(bool value) noexcept { nativeDOFConsumerAvailable.store(value, std::memory_order_release); }
inline std::atomic<bool> nativeScreenCopyConsumerAvailable{false};
inline bool IsNativeScreenCopyConsumerAvailable() noexcept { return nativeScreenCopyConsumerAvailable.load(std::memory_order_acquire); }
// Published only by the actual renderer after ordered world/screen sources,
// immutable native storage and both generic image consumers are connected.
inline void SetNativeScreenCopyConsumerAvailable(bool value) noexcept { nativeScreenCopyConsumerAvailable.store(value, std::memory_order_release); }
} // namespace rendering
