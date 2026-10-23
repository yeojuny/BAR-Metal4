/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4TerrainSquareSource.h"
#include "Rendering/Backend/NativeTerrainSourceData.h"
#include "Rendering/Backend/NativeSMFTextureData.h"
#include <algorithm>

namespace rendering::metal4 {
namespace {
struct SquareImageOwner {
	std::shared_ptr<const NativeCUSTextureObject> object;
	std::shared_ptr<const WorldTerrainTiles> tiles;
	__strong id<MTLTexture> array=nil,view=nil;
};
} // namespace

CUSSourceImage ResolveNativeTerrainSquareImage(RuntimeLease& lease,const NativeCUSTextureBinding& binding,
	std::shared_ptr<const WorldTerrainTiles> cachedOwner,id<MTLTexture> array,uint32_t first,id<MTLSamplerState> sampler)
{
	Require(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object&&cachedOwner&&array!=nil&&sampler!=nil,
		"Original terrain square requires its actual bound object, committed cache batch and source sampler");
	const auto& object=binding.object;
	const auto backing=std::dynamic_pointer_cast<const NativeTerrainSquareBacking>(object->backing);
	Require(object->contextGeneration&&object->identity&&object->name&&object->target==NativeCUSTextureTarget::Image2D&&
		object->storage==NativeTextureStorageKnowledge::Published&&!object->image&&backing&&
		backing->storageGeneration==object->identity&&backing->contentVersion&&backing->contentVersion<=object->version,
		"Original terrain square lacks its matching immutable logical object/content owner");
	ValidateNativeTerrainSquareBacking(*backing); // O(1): no repeated full tile payload/index scan.
	Require(backing->tiles==cachedOwner,"Original terrain square belongs to another actual map cache owner");
	const auto mip=backing->firstSourceMip,count=backing->sourceMipCount;
	const uint32_t extent=NATIVE_SMF_SQUARE_TEXELS>>mip;
	Require(object->allocation==NativeTextureAllocation{extent,extent,1,count,1,0x83F1},
		"Original terrain square allocation differs from its actual BC1_RGBA source mip interval");

	const uint64_t columns=cachedOwner->gridWidth/NATIVE_SMF_TILES_PER_SQUARE;
	const uint64_t total=columns*(cachedOwner->gridHeight/NATIVE_SMF_TILES_PER_SQUARE);
	const uint64_t sourceSquare=uint64_t(backing->squareZ)*columns+backing->squareX;
	Require(first%2048==0&&first<total&&sourceSquare>=first&&
		sourceSquare-first<array.arrayLength&&array.arrayLength==std::min(uint64_t(2048),total-first),
		"Original terrain square does not belong to the supplied committed map batch range");
	Require(array.device==lease.Device()&&sampler.device==lease.Device()&&array.textureType==MTLTextureType2DArray&&
		array.pixelFormat==MTLPixelFormatBC1_RGBA&&array.width==NATIVE_SMF_SQUARE_TEXELS&&array.height==NATIVE_SMF_SQUARE_TEXELS&&
		array.depth==1&&array.sampleCount==1&&array.mipmapLevelCount==4&&array.storageMode==MTLStorageModeShared&&
		(array.usage&MTLTextureUsageShaderRead),
		"Original terrain square batch is not the actual uploaded shared BC1_RGBA array on this device");
	const NSUInteger layer=NSUInteger(sourceSquare-first);
	// Same format, without a swizzle or channel reinterpretation. Select source
	// mip k as view level0, exactly like streamed LoadSquareTexture(k)->TexImage0.
	// MTLTextureUsagePixelFormatView is not needed for this unchanged format.
	id<MTLTexture> view=[array newTextureViewWithPixelFormat:MTLPixelFormatBC1_RGBA textureType:MTLTextureType2D
		levels:NSMakeRange(mip,count) slices:NSMakeRange(layer,1)];
	Require(view!=nil,"Cannot create the original terrain square's same-storage layer/mip view");
	Require(view.parentTexture==array&&view.parentRelativeLevel==mip&&view.parentRelativeSlice==layer&&
		view.device==lease.Device()&&view.textureType==MTLTextureType2D&&view.pixelFormat==MTLPixelFormatBC1_RGBA&&
		view.width==extent&&view.height==extent&&view.depth==1&&view.arrayLength==1&&view.sampleCount==1&&view.mipmapLevelCount==count,
		"Original terrain square view differs from its actual source layer/mip allocation");
	lease.AddAllocation(array);lease.AddAllocation(view);lease.Retain(sampler);
	auto owner=std::make_shared<SquareImageOwner>();owner->object=object;owner->tiles=std::move(cachedOwner);owner->array=array;owner->view=view;
	CUSSourceImage out;out.texture=view;out.sampler=sampler;out.owner=std::move(owner);
	out.flipV=false;out.depthImage=false;out.clampDepthCompare=false;
	// The real cache uploader synchronously filled all four Shared mip levels
	// before this batch became visible. There is no future producer to encode,
	// fake readiness flag to advance, completion callback, submission or wait.
	return out;
}
} // namespace rendering::metal4
