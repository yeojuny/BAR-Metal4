/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Metal4RuntimeNative.h"
#include "../Backend/NativeSMFTextureData.h"

namespace rendering::metal4 {

inline id<MTLTexture> UploadNativeSMFSquares(RuntimeLease& lease, const WorldTerrainTiles& tiles, uint32_t first, uint32_t count)
{
	const uint32_t columns = tiles.gridWidth / NATIVE_SMF_TILES_PER_SQUARE;
	const uint32_t total = columns * (tiles.gridHeight / NATIVE_SMF_TILES_PER_SQUARE);
	Require(columns > 0 && count > 0 && count <= 2048 && first < total && count <= total - first,
		"Original spatial SMF square array range is invalid");
	MTLTextureDescriptor* descriptor = [MTLTextureDescriptor new];
	descriptor.textureType = MTLTextureType2DArray;
	descriptor.pixelFormat = MTLPixelFormatBC1_RGBA;
	descriptor.width = descriptor.height = NATIVE_SMF_SQUARE_TEXELS;
	descriptor.arrayLength = count;
	descriptor.mipmapLevelCount = 4;
	descriptor.storageMode = MTLStorageModeShared;
	descriptor.usage = MTLTextureUsageShaderRead;
	id<MTLTexture> image = [lease.Device() newTextureWithDescriptor:descriptor];
	Require(image != nil, "Cannot allocate original spatial SMF square BC1 array");
	image.label = @"Original spatial SMF square BC1 mip array partition";
	for (uint32_t layer = 0; layer < count; ++layer) {
		const uint32_t square = first + layer;
		const auto mips = AssembleNativeSMFSquare(tiles, square % columns, square / columns);
		for (size_t level = 0; level < mips.size(); ++level) {
			const auto& mip = mips[level];
			[image replaceRegion:MTLRegionMake2D(0, 0, mip.width, mip.height) mipmapLevel:level slice:layer withBytes:mip.bytes.data()
				bytesPerRow:mip.bytesPerRow bytesPerImage:0];
		}
	}
	lease.AddAllocation(image);
	return image;
}

inline id<MTLSamplerState> CreateNativeSMFSampler(RuntimeLease& lease, bool repeat, uint32_t anisotropy, bool mipmapped = true)
{
	Require(anisotropy >= 1 && anisotropy <= 16, "Original SMF sampler anisotropy exceeds the Metal4 device contract");
	MTLSamplerDescriptor* descriptor = [MTLSamplerDescriptor new];
	descriptor.minFilter = descriptor.magFilter = MTLSamplerMinMagFilterLinear;
	descriptor.mipFilter = mipmapped ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNotMipmapped;
	descriptor.sAddressMode = descriptor.tAddressMode = repeat ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeClampToEdge;
	descriptor.maxAnisotropy = anisotropy;
	descriptor.supportArgumentBuffers = YES;
	id<MTLSamplerState> sampler = [lease.Device() newSamplerStateWithDescriptor:descriptor];
	Require(sampler != nil, "Cannot create original SMF anisotropic sampler");
	lease.Retain(sampler);
	return sampler;
}

} // namespace rendering::metal4
