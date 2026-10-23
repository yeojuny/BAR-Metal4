/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4CUSOutlineInputs.h"
#include "Metal4DeferredColorNative.h"
#include "Metal4ShieldNative.h"

namespace rendering::metal4 {
namespace {
MTLSamplerAddressMode Address(unsigned value)
{
	switch (value) {
		case 0x2901: return MTLSamplerAddressModeRepeat;
		case 0x8370: return MTLSamplerAddressModeMirrorRepeat;
		case 0x812F: return MTLSamplerAddressModeClampToEdge;
		case 0x812D: return MTLSamplerAddressModeClampToBorderColor;
		default: throw std::runtime_error("Outline GBuffer wrap needs its exact original sampler implementation");
	}
}
id<MTLSamplerState> Sampler(RuntimeLease& lease, const NativeTextureSampling& source)
{
	Require(source.compareMode == 0 && source.baseLevel == 0 && source.maxLevel >= 0 &&
		source.minLod <= 0 && source.maxLod >= 0 && source.lodBias == 0 && source.anisotropy == 1 &&
		(source.minFilter == 0x2600 || source.minFilter == 0x2601) &&
		(source.magFilter == 0x2600 || source.magFilter == 0x2601),
		"Outline GBuffer must retain its observed non-mip noncomparison sampler");
	Require(source.borderColor == std::array<float, 4>{0, 0, 0, 0} ||
		source.borderColor == std::array<float, 4>{0, 0, 0, 1} ||
		source.borderColor == std::array<float, 4>{1, 1, 1, 1},
		"Outline GBuffer custom border needs its exact original sampler implementation");
	auto descriptor = [MTLSamplerDescriptor new];
	descriptor.supportArgumentBuffers = YES;
	descriptor.minFilter = source.minFilter == 0x2600 ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
	descriptor.magFilter = source.magFilter == 0x2600 ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
	descriptor.mipFilter = MTLSamplerMipFilterNotMipmapped;
	descriptor.sAddressMode = Address(source.wrapS);
	descriptor.tAddressMode = Address(source.wrapT);
	descriptor.rAddressMode = Address(source.wrapR);
	descriptor.borderColor = source.borderColor[3] == 0 ? MTLSamplerBorderColorTransparentBlack :
		source.borderColor[0] == 1 ? MTLSamplerBorderColorOpaqueWhite : MTLSamplerBorderColorOpaqueBlack;
	auto sampler = [lease.Device() newSamplerStateWithDescriptor:descriptor];
	Require(sampler != nil, "Cannot allocate the outline's actual retained GBuffer sampler");
	lease.Retain(sampler);
	return sampler;
}
void Storage(id<MTLTexture> texture, MTLPixelFormat format, const NativeShieldDepthMetadata& view)
{
	Require(texture && texture.textureType == MTLTextureType2D && texture.pixelFormat == format &&
		texture.sampleCount == 1 && texture.mipmapLevelCount == 1 &&
		(texture.usage & MTLTextureUsageShaderRead) &&
		texture.width == view.width && texture.height == view.height && view.width && view.height,
		"Outline input is not its actual full-resolution single-sample GBuffer image");
}
void Readable(const std::shared_ptr<const ShieldDepthProducerState>& producer, uint64_t submission)
{
	Require(producer && IsNativeShieldDepthReadable(producer->encoded, producer->completed,
		producer->operation == ShieldDepthOperation::AllocationOnly, producer->allocationVerified,
		producer->submissionIdentity, submission),
		"Outline input has neither encoded in this actual submission nor completed as a retained prior version");
}
bool SameView(const NativeShieldDepthMetadata& a, const NativeShieldDepthMetadata& b)
{
	return a.width == b.width && a.height == b.height && a.view == b.view &&
		a.projection == b.projection && a.viewport == b.viewport;
}
} // namespace

CUSOutlineImage ResolveCUSOutlineInput(RuntimeLease& lease, const NativeCUSTextureBinding& binding,
	unsigned slot, uint64_t submission)
{
	Require(slot < 3 && submission && binding.knowledge == NativeCUSBindingKnowledge::BoundObject &&
		binding.object && binding.object->target == NativeCUSTextureTarget::Image2D && binding.object->backing,
		"Outline requires an observed original map/model GBuffer binding and real shared submission");
	const auto& object = *binding.object;
	CUSOutlineImage out;
	out.samplerSlot = slot;
	out.sampling = object.sampling;
	out.sourceDrawFrame = object.backing->sourceDrawFrame;
	std::shared_ptr<const ShieldDepthProducerState> producer;
	if (slot < 2) {
		const auto backing = std::dynamic_pointer_cast<const NativeShieldDepthBacking>(object.backing);
		const auto kind = slot == 0 ? NativeTextureBackingKind::MapDepth : NativeTextureBackingKind::ModelDepth;
		const auto role = slot == 0 ? ShieldDepthRole::TerrainOnly : ShieldDepthRole::ModelsOnly;
		Require(backing && backing->Kind() == kind, "Outline depth slot names another original producer role");
		const auto& image = GetShieldDepthBackingImage(*backing);
		out.view = backing->Metadata();
		Storage(image.texture, MTLPixelFormatDepth32Float, out.view);
		Require(image.storageOwner && image.producer && image.role == role && image.producer->role == role &&
			image.producerIdentity == image.producer->identity && image.sourceDrawFrame == out.sourceDrawFrame &&
			image.producer->sourceDrawFrame == out.sourceDrawFrame && image.view == out.view.view &&
			image.projection == out.view.projection && image.viewport == out.view.viewport,
			"Outline depth lost its actual producer, immutable publication or original view");
		out.image.texture = image.texture;
		out.image.owner = backing;
		out.image.depthImage = true;
		producer = image.producer;
	} else {
		const auto backing = std::dynamic_pointer_cast<const NativeDeferredColorBacking>(object.backing);
		Require(backing && backing->Metadata().role == NativeGeometryTextureRole::Models &&
			backing->Metadata().attachment == 4,
			"Outline misc slot is not original CUS model GBuffer attachment4");
		const auto& image = GetDeferredColorBackingImage(*backing);
		out.view = backing->Metadata().view;
		Storage(image.texture, MTLPixelFormatRGBA8Unorm, out.view);
		Require(image.storageOwner && image.producer && image.producer->role == ShieldDepthRole::ModelsOnly &&
			image.producer->sourceDrawFrame == out.sourceDrawFrame &&
			image.metadata.role == NativeGeometryTextureRole::Models && image.metadata.attachment == 4 &&
			SameView(image.metadata.view, out.view),
			"Outline unit IDs lost their original CUS MRT producer or attachment metadata");
		out.image.texture = image.texture;
		out.image.owner = backing;
		producer = image.producer;
	}
	Require(object.allocation.width == out.view.width && object.allocation.height == out.view.height &&
		object.allocation.depth == 1 && object.allocation.levels == 1 && object.allocation.samples == 1,
		"Outline retained logical GBuffer allocation differs from actual producer storage");
	const bool undefined = producer->operation == ShieldDepthOperation::AllocationOnly;
	Require(undefined == (object.backing->content == NativeTextureContent::AllocationOnly),
		"Outline cannot promote undefined storage to rendered unit IDs or depth");
	out.epoch = {producer, undefined};
	out.image.sampler = Sampler(lease, out.sampling);
	out.image.flipV = true;
	out.image.requireReadable = [producer, submission] { Readable(producer, submission); };
	// The common MRT producer owns dependency encoding and completion. This
	// consumer retains its exact epoch and checks it at the original draw; it
	// must not encode/complete the same producer a second time.
	lease.AddAllocation(out.image.texture);
	return out;
}

void ValidateCUSOutlineImages(const std::array<CUSOutlineImage, 3>& inputs)
{
	for (unsigned slot = 0; slot < inputs.size(); ++slot) {
		const auto& input = inputs[slot];
		Require(input.samplerSlot == slot && input.image.texture && input.image.sampler &&
			input.image.owner && input.image.requireReadable && input.epoch.actualProducer &&
			input.image.depthImage == (slot < 2) &&
			input.view.width == inputs[0].view.width && input.view.height == inputs[0].view.height &&
			input.image.flipV == inputs[0].image.flipV,
			"Outline GBuffer inputs disagree with their original slot, extent or orientation");
	}
	const std::array<NativeDeferredPostprocessEpoch, 2> model{inputs[1].epoch, inputs[2].epoch};
	ValidateNativeDeferredModelEpochs(model);
	Require(inputs[1].sourceDrawFrame == inputs[2].sourceDrawFrame && SameView(inputs[1].view, inputs[2].view),
		"Outline misc IDs and model depth came from different source frames or views");
	// Map and model have independent source-phase lifetimes. A retained map
	// frame is not guessed to be current; its real producer/readability token
	// and exact captured view are preserved above. The original shader samples
	// that current logical map image without requiring its camera matrices to
	// equal the independently published model phase.
}

} // namespace rendering::metal4
