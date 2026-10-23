/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>

namespace rendering {

enum class ScreenAlphaCompare : std::uint8_t { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };
enum class NativeWorldStencilOp : std::uint8_t { Keep, Zero, Replace, IncrementClamp, DecrementClamp, Invert, IncrementWrap, DecrementWrap };
struct NativeWorldStencilFace {
	ScreenAlphaCompare compare = ScreenAlphaCompare::Always;
	NativeWorldStencilOp fail = NativeWorldStencilOp::Keep;
	NativeWorldStencilOp depthFail = NativeWorldStencilOp::Keep;
	NativeWorldStencilOp pass = NativeWorldStencilOp::Keep;
	std::uint32_t readMask = ~0u, writeMask = ~0u;
	std::int32_t reference = 0;
	bool operator==(const NativeWorldStencilFace&) const = default;
};

// Requested source state. The consumer gates it by the actual attachments:
// a color-only GL framebuffer has no depth/stencil comparisons or writes.
struct ScreenDepthStencilState {
	bool depthTest = false;
	bool depthWrite = false;
	ScreenAlphaCompare depthCompare = ScreenAlphaCompare::LessEqual;
	bool stencilTest = false;
	NativeWorldStencilFace frontStencil, backStencil;
	bool operator==(const ScreenDepthStencilState&) const = default;
};

enum ScreenAttachmentMask : unsigned {
	ScreenAttachmentColor = 1, ScreenAttachmentDepth = 2, ScreenAttachmentStencil = 4,
	ScreenAttachmentAll = 7,
};
struct ScreenClearValues {
	std::array<float,4> color{0,0,0,0};
	float depth = 1;
	std::int32_t stencil = 0;
	bool operator==(const ScreenClearValues&) const = default;
};

} // namespace rendering
