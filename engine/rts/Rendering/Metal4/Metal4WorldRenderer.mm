#include "Rendering/Backend/NativeTargetSubmissionNotice.h"
#include "Metal4CubeCopy206.h"
#include <cstdlib>
/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4StartBoxNative.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Metal4WeaponRangeNative.h"
#include "Metal4BuildSquareNative.h"
#include "Metal4Sensor31657CircleNative.h"
#include "Metal4SensorPreviewNative.h"
#include "Metal4NativeEffectState.h"
#include "Rendering/Backend/NativeMapTexturePublication.h"
#include "Rendering/Backend/NativeDeferredPostprocessSource.h"
#include "Metal4WorldRenderer.h"
#include "Rendering/Backend/NativePreparationTrace.h"
#include "Rendering/Backend/NativeCUSCPUReadScope.h"
#include "Metal4MainSampling.h"
#include "Metal4WorldBCDecode.h"
#include "Metal4RuntimeNative.h"
#include "Metal4InterventionPools.h"
#include "Rendering/Backend/NativeSubmissionTrace.h"
#include "Rendering/Backend/NativeCPUPhaseTrace.h"
#include "Metal4WindowSurfaceNative.h"
#include "Rendering/Backend/NativeSerialRenderWorker.h"
#include "Metal4WorldViewport.h"
#include "Metal4LogicalCopyMapping.h"
#include "Metal4ScreenNative.h"
#include "Metal4MapEdgeRenderer.h"
#include "Metal4WorldShaderSource.h"
#include "Rendering/Backend/NativeCUSMaterialControl.h"
#include "Rendering/Backend/NativeS3OTextureSource.h"
#include "Rendering/Backend/NativeParticleAtlasSource.h"
#include "Metal4OpaqueModelSource.h"
#include "Metal4ShadowTextureNative.h"
#include "Metal4OriginalShadowSource.h"
#include "Metal4InitialShadowClear.h"
#include "Rendering/Backend/NativeOriginalShadowParticlesProgramSource.h"
#include "Rendering/Backend/NativeEngineShadowProgramSource.h"
#include "Metal4NamedVolumeNative.h"
#include "Rendering/Backend/NativeEngineModelProgramSource.h"
#include "Rendering/Backend/NativeCUSSourceIdentity.h"
#include "Metal4WorldParticles.h"
#include "Metal4WorldEnvironment.h"
#include "Metal4CubeTextureSource.h"
#include "Metal4PBRSourceService.h"
#include "Rendering/Env/CubeMapHandler.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Metal4WorldOverlayRenderer.h"
#include "Metal4ProjectileEffects.h"
#include "Metal4EnergyExplosion.h"
#include "Metal4LightningEffects.h"
#include "Metal4FireSmokeNative.h"
#include "Rendering/Backend/NativeFireSmokeRecorder.h"
#include "Metal4MissileThrusterNative.h"
#include "Metal4Flamethrower.h"
#include "Metal4FireGL4.h"
#include "Metal4NapalmPreview.h"
#include "Metal4Snow.h"
#include "Metal4CommandsNative.h"
#include "Metal4ReclaimField.h"
#include "Metal4ObjectSpotlight.h"
#include "Metal4Grass.h"
#include "Metal4ResourceSpots.h"
#include "Metal4SensorLOSNative.h"
#include "Rendering/Backend/NativeMissileThrusterRecorder.h"
#include "Metal4ShieldNative.h"
#include "Metal4SSAO.h"
#include "Metal4Bloom.h"
#include "Metal4DeferredLights.h"
#include "Metal4UnitStencil.h"
#include "Metal4Outline.h"
#include "Metal4CUSOutlineInputs.h"
#include "Metal4Distortion.h"
#include "Rendering/Metal4/Metal4DeferredColorNative.h"
#include "Metal4MapDepthNative.h"
#include "Metal4ModelDepthNative.h"
#include "Rendering/Backend/NativeShieldRecorder.h"
#include "Rendering/Backend/NativeShieldDepthPublication.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Metal4Blueprint.h"
#include "Metal4Blockout.h"
#include "Metal4Placement.h"
#include "Rendering/Backend/NativeModelLightData.h"
#include "Metal4BumpWater.h"
#include "Metal4BasicWater.h"
#include "Metal4LegacyWater.h"
#include "Metal4DynamicWater.h"
#include "Metal4LegacyWaterBump.h"
#include "Metal4GroundDecal.h"
#include "Metal4GameDecal.h"
#include "Metal4UnitPrimitive.h"
#include "Metal4WorldInfo.h"
#include "Metal4TerrainInfoSource.h"
#include "Metal4TerrainShadingSource.h"
#include "Metal4TerrainSourceGPU.h"
#include "Metal4TerrainSquareSource.h"
#include "Metal4TerrainGroundClear.h"
#include "Rendering/Backend/NativeInitialClearLoad.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "System/Log/ILog.h"
#include "Metal4MRTObservation174.h"
#endif
#include "Metal4WorldEncodingProbe176.h"
#include "Rendering/EngineBridge/NativeTerrainImagePublication.h"
#include "Rendering/Backend/NativeTerrainInfoData.h"
#include "Metal4FireSmokeNative.h"
#include "../Backend/NativeWaterLuaFrame.h"
#include <functional>
#include "../Backend/NativeWaterSceneData.h"
#include "Metal4SMFTextures.h"
#include "Metal4MetalInfo.h"
#include "Metal4MiniMapNative.h"
#include "Metal4PostprocessNative.h"
#include "Metal4VolumetricClouds.h"
#include "../Backend/NativeTerrainShadingState.h"
#include "../Backend/NativeLuaNanoData.h"
#include "../Backend/NativeSMFTextureData.h"

#import <simd/simd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <unordered_map>
#include <utility>
#include <bit>
#include <set>

// Retained in the existing RuntimeLease feedback graph before Begin. In
// particular, a timeout cannot destroy C++ backing/PSO/pass owners on unwind.
@interface M4OriginalSourceLifetime : NSObject {
@public
 __strong id<MTLFence> combinedFence220;
 std::shared_ptr<rendering::metal4::PreparedOpaqueModelSource> opaque;
 std::shared_ptr<rendering::metal4::PreparedBlockoutFrame> blockout;
 std::shared_ptr<const rendering::NativeOpaqueModelFrame> source;
 std::shared_ptr<const rendering::NativeCUSSourceFrame> genesis;
	std::array<std::shared_ptr<rendering::metal4::PreparedOpaqueModelSource>,2> waterOpaque,waterAlpha;
	std::array<std::shared_ptr<rendering::metal4::PreparedOpaqueModelSource>,3> alpha;
 std::shared_ptr<const rendering::NativeWaterSceneFrame> waterSources;
 std::shared_ptr<rendering::metal4::PreparedOriginalShadowSource> shadow;
 std::shared_ptr<const rendering::NativeOriginalShadowFrame> shadowSource;
 std::array<std::shared_ptr<const rendering::NativeShadowTextureBacking>,2> initialShadow;
 std::shared_ptr<const rendering::NativeWorldDrawFrame> commonCommands;
 std::shared_ptr<rendering::metal4::PreparedReclaimField> reclaimFields;
 std::shared_ptr<rendering::metal4::PreparedObjectSpotlight> objectSpotlights;
 std::shared_ptr<rendering::metal4::PreparedGrass> grass;
 std::shared_ptr<rendering::metal4::PreparedResourceSpots> resourceSpots;
 std::vector<std::shared_ptr<rendering::metal4::PreparedSensorLOSDraw>> sensorLOS;
 std::shared_ptr<rendering::metal4::PreparedDeferredLights> deferredLights;
 std::shared_ptr<rendering::metal4::PreparedUnitStencil> unitStencil;
 std::shared_ptr<rendering::metal4::PreparedOutline> outline;
 std::shared_ptr<rendering::metal4::PreparedDistortion> distortions;
 std::vector<std::shared_ptr<const void>> waterGround;
 std::vector<rendering::metal4::PreparedScreenExternalImage> terrainImages;
 std::shared_ptr<rendering::metal4::MainFrameAttachments> mainAttachments;
 std::shared_ptr<rendering::metal4::MainTerrainDepthSnapshot> terrainSnapshot;
 std::shared_ptr<rendering::metal4::PreparedScreenFrame> screen;
 std::shared_ptr<rendering::metal4::PreparedScreenFrame> presentationMargins;
}
@end
@implementation M4OriginalSourceLifetime
@end

namespace rendering {
namespace {
using namespace metal4;

void MainThread() { Require([NSThread isMainThread]||Metal4WindowSurfaceNativeAccess::WorkerAccessActive(), "Metal4 world presentation requires main or pinned render worker"); }
void MatrixFinite(const WorldMatrix& matrix) { for (float value: matrix) Require(std::isfinite(value), "Non-finite world matrix"); }

struct TileInstance { uint32_t x, z, layer, padding; };
struct WorldUniform {
	simd_float4x4 viewProjection;
	simd_float4x4 world;
	simd_float4x4 piece;
	simd_float4 team;
	simd_uint4 terrain;
	simd_uint4 options;
	simd_float4 parameters;
};
struct SceneUniform {
	simd_float4 sun, cameraPosition, groundAmbient, groundDiffuse, groundSpecular;
	simd_float4 modelAmbient, modelDiffuse, modelSpecular, fogColor, fogParameters;
	simd_float4 waterBase, waterMinimum, waterAbsorb, parameters;
	simd_float4x4 shadow;
	simd_uint4 flags;
	simd_float4 splatScales, splatMultipliers, mapDimensions;
	simd_float4 modelClipPlane, terrainClipPlane;
};
static_assert(sizeof(SceneUniform) == 384);
simd_float4 Vec(const std::array<float,4>& v) { return {v[0],v[1],v[2],v[3]}; }
simd_float4 Vec(const std::array<float,3>& v) { return {v[0],v[1],v[2],0}; }
static_assert(sizeof(WorldUniform) == 256);
static_assert(sizeof(WorldModelVertex) == 76);
static_assert(offsetof(WorldModelVertex, uv) == 48);
static_assert(offsetof(WorldModelVertex, boneIds) == 64);
static_assert(offsetof(WorldModelVertex, boneWeights) == 72);

simd_float4x4 Matrix(const WorldMatrix& input)
{
	simd_float4x4 output;
	std::memcpy(&output, input.data(), sizeof(output));
	return output;
}



id<MTLBuffer> Buffer(id<MTLDevice> device, const void* bytes, size_t length, NSString* label)
{
	Require(length > 0 && length <= device.maxBufferLength, "World GPU buffer length exceeds device bounds");
	id<MTLBuffer> result = bytes ? [device newBufferWithBytes:bytes length:length options:MTLResourceStorageModeShared] :
		[device newBufferWithLength:length options:MTLResourceStorageModeShared];
	Require(result != nil, "Cannot allocate world GPU buffer");
	result.label = label;
	return result;
}

MTLPixelFormat TextureFormat(WorldTextureFormat format)
{
	switch (format) {
		case WorldTextureFormat::BC1RGBA: return MTLPixelFormatBC1_RGBA;
		case WorldTextureFormat::BC2RGBA: return MTLPixelFormatBC2_RGBA;
		case WorldTextureFormat::BC3RGBA: return MTLPixelFormatBC3_RGBA;
		case WorldTextureFormat::BC4Unorm: return MTLPixelFormatBC4_RUnorm;
		case WorldTextureFormat::BC5Unorm: return MTLPixelFormatBC5_RGUnorm;
		case WorldTextureFormat::BC7RGBA: return MTLPixelFormatBC7_RGBAUnorm;
		case WorldTextureFormat::BC7RGBASRGB: return MTLPixelFormatBC7_RGBAUnorm_sRGB;
		case WorldTextureFormat::BGRA8Unorm: return MTLPixelFormatBGRA8Unorm;
		case WorldTextureFormat::RGBA8Unorm: return MTLPixelFormatRGBA8Unorm;
	}
	throw std::runtime_error("Unsupported world texture format");
}

const WorldTexture& PublishedWorldTexture(const NativeCUSTextureObject& object)
{
	Require(object.image&&!object.backing&&object.target==NativeCUSTextureTarget::Image2D&&
		object.storage==NativeTextureStorageKnowledge::Published&&object.contextGeneration&&object.identity&&
		object.allocation.depth==1&&object.allocation.samples==1&&!object.image->mips.empty(),
		"Original world image lacks its actual published two-dimensional allocation");
	const auto& image=*object.image;const auto& base=image.mips.front();
	uint32_t levels=uint32_t(image.mips.size());
	if(image.generateMipmaps){levels=1;for(auto side=std::max(base.width,base.height);side>1;side>>=1)++levels;}
	Require(base.width==object.allocation.width&&base.height==object.allocation.height&&levels==object.allocation.levels&&
		image.originalInternalFormat==object.allocation.originalInternalFormat,
		"Original world image bytes/mip chain differ from the retained allocation");
	const auto& sampling=object.sampling;
	Require(sampling.compareMode==0&&sampling.baseLevel==0&&sampling.lodBias==0&&
		sampling.maxLevel>=int(levels-1)&&sampling.minLod<=0&&sampling.maxLod>=float(levels-1)&&
		std::isfinite(sampling.anisotropy)&&sampling.anisotropy>=1&&std::floor(sampling.anisotropy)==sampling.anisotropy,
		"Original world image needs its exact comparison/base/LOD/anisotropy sampler consumer");
	return image;
}

// Only the retained S3O publication proves a long-lived immutable model asset.
// Unknown/plain images keep their original storage, even if they share a path.
bool PrivateModelTexture(const NativeCUSTextureObject* object, const std::shared_ptr<const WorldTexture>& owner)
{
	if (!object || !owner || owner->mips.empty()) return false;
	const auto* model = dynamic_cast<const NativeModelTextureBacking*>(object->backing.get());
	uint32_t levels = uint32_t(owner->mips.size());
	if (owner->generateMipmaps) {
		levels = 1;
		for (auto side = std::max(owner->mips.front().width, owner->mips.front().height); side > 1; side >>= 1) ++levels;
	}
	return model && model->pixels == owner && object->image == owner &&
		object->contextGeneration && object->identity &&
		object->target == NativeCUSTextureTarget::Image2D && object->storage == NativeTextureStorageKnowledge::Published &&
		model->content == NativeTextureContent::ProducerVersion && model->storageGeneration == object->identity &&
		object->allocation.width == owner->mips.front().width && object->allocation.height == owner->mips.front().height &&
		object->allocation.levels == levels && object->allocation.originalInternalFormat == owner->originalInternalFormat &&
		object->allocation.depth == 1 && object->allocation.samples == 1;
}

struct UploadedTexture {
	__strong id<MTLTexture> texture = nil, uploadSource = nil;
};

UploadedTexture UploadTexture(id<MTLDevice> device, const WorldTexture& data, uint32_t maxDimension, bool privateModel)
{
	Require(!data.mips.empty() && data.mips.size() <= 16, "World texture mip count outside bounds");
	uint32_t width = data.mips[0].width, height = data.mips[0].height;
	Require(width > 0 && height > 0 && width <= maxDimension && height <= maxDimension, "World texture dimensions outside diagnostic bounds");
	uint32_t maximumLevels = 1;
	for (uint32_t dimension = std::max(width, height); dimension > 1; dimension >>= 1) ++maximumLevels;
	Require(data.mips.size() <= maximumLevels, "World texture has excess mip levels");
	const auto decoded = metal4::DecodeWorldBCBaseForMipmaps(data, maxDimension);
	const auto storageFormat = decoded ? WorldTextureFormat::RGBA8Unorm : data.format;
	const bool compressed = storageFormat != WorldTextureFormat::RGBA8Unorm && storageFormat != WorldTextureFormat::BGRA8Unorm;
	Require(!compressed || device.supportsBCTextureCompression, "World original BC texture unsupported on this device");
	MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(decoded && data.format == WorldTextureFormat::BC7RGBASRGB ? MTLPixelFormatRGBA8Unorm_sRGB : TextureFormat(storageFormat)) width:width height:height mipmapped:YES];
	descriptor.mipmapLevelCount = data.generateMipmaps ? maximumLevels : data.mips.size();
	descriptor.storageMode = MTLStorageModeShared;
	descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView;
	id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
	Require(texture != nil, "Cannot allocate original world unit texture");
	texture.label = String(data.sourcePath);
	for (size_t level = 0; level < data.mips.size(); ++level) {
		const auto& mip = decoded ? *decoded : data.mips[level];
		const size_t rows = compressed ? size_t((height + 3) / 4) : height;
		const size_t rowBytes = compressed ? size_t((width + 3) / 4) * ((storageFormat == WorldTextureFormat::BC1RGBA || storageFormat == WorldTextureFormat::BC4Unorm) ? 8 : 16) : size_t(width) * 4;
		Require(mip.width == width && mip.height == height && mip.bytesPerRow >= rowBytes &&
			mip.bytesPerRow <= 128 * 1024 && mip.bytes.size() >= mip.bytesPerRow * rows, "World texture mip extent/bytes mismatch");
		[texture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:level withBytes:mip.bytes.data() bytesPerRow:mip.bytesPerRow];
		width = std::max(1u, width / 2); height = std::max(1u, height / 2);
	}
	UploadedTexture uploaded; uploaded.texture = texture;
	if (privateModel) {
		// Preserve the exact format, extent, mip allocation and view usage.
		// The CPU upload remains the fallback if Private allocation fails.
		descriptor.storageMode = MTLStorageModePrivate;
		id<MTLTexture> destination = [device newTextureWithDescriptor:descriptor];
		if (destination != nil) {
			destination.label = texture.label;
			uploaded.texture = destination;
			uploaded.uploadSource = texture;
		}
	}
	return uploaded;
}

Metal4PresentationStatus SurfaceStatus(const Metal4SurfaceSnapshot& state)
{
	if (!state.attached) return Metal4PresentationStatus::Detached;
	if (state.closeRequested) return Metal4PresentationStatus::CloseRequested;
	if (state.minimized) return Metal4PresentationStatus::Minimized;
	if (!state.visible) return Metal4PresentationStatus::Hidden;
	if (!state.CanPresent()) return Metal4PresentationStatus::ZeroExtent;
	return Metal4PresentationStatus::Presented;
}

} // namespace

struct Metal4WorldRenderer::Impl {
 metal4::InterventionShadowViews interventionShadowViews;
	metal4::ScreenGPU screenGPU;
	metal4::StartBoxGPU startBoxGPU;
	metal4::WeaponRangeGPU weaponRangeGPU;
	metal4::BuildSquareGPU buildSquareGPU;
	metal4::Sensor31657CircleGPU sensor31657CircleGPU;
	metal4::SensorPreviewGPU sensorPreviewGPU;
	metal4::MiniMapGPU sensorRadarGPU;
	struct MapImage final:NativeMapTextureResource {
		__strong id<MTLTexture> texture=nil;mutable bool completed=false;
		// One RRRR view for this actual immutable height upload, never by name/frame.
		mutable __strong id<MTLTexture> heightRRRR=nil,heightRRRRSource=nil;
		std::shared_ptr<const metal4::PreparedMiniMapHeight> heightOwner;
		bool Completed()const noexcept override{return completed;}
	};
	std::shared_ptr<bool> normalsEncoded=std::make_shared<bool>(false);
	metal4::MapEdgeGPU mapEdgeGPU;
	metal4::WorldParticlesGPU particlesGPU;
	metal4::WorldEnvironmentGPU environmentGPU;
	metal4::PBRSourceService pbrSource;
	uint64_t pbrSubmission=0;
	metal4::WorldOverlayGPU overlayGPU;
	metal4::ProjectileEffectsGPU projectileEffectsGPU;
	metal4::EnergyExplosionGPU energyExplosionGPU;
	metal4::LightningEffectsGPU lightningGPU;
	metal4::FireSmokeGPU fireSmokeGPU;
	metal4::MissileThrusterGPU missileThrusterGPU;
	metal4::FlamethrowerGPU flamethrowerGPU;
	metal4::FireGL4GPU fireGL4GPU;
	metal4::NapalmPreviewGPU napalmPreviewGPU;
	metal4::SnowGPU snowGPU;
	metal4::CommandsGPU commandsGPU;
	metal4::ReclaimFieldGPU reclaimFieldGPU;
	metal4::ObjectSpotlightGPU objectSpotlightGPU;
	metal4::GrassGPU grassGPU;
 metal4::ResourceSpotsGPU resourceSpotsGPU;
 metal4::SensorLOSGPU sensorLOSGPU;
	metal4::CUSSourceGPU cusSourceGPU;
	metal4::TerrainSourceGPU terrainSourceGPU;
	metal4::EngineModelDeferredGPU originalDeferredGPU;
	metal4::EngineShadowGPU originalShadowGPU;
 metal4::OriginalShadowParticlesGPU originalShadowParticlesGPU;
	metal4::EngineModelForwardGPU originalForwardGPU;
	metal4::SSAOGPU ssaoGPU;
	metal4::BloomGPU bloomGPU;
	metal4::DeferredLightsGPU deferredLightsGPU;
	metal4::UnitStencilGPU unitStencilGPU;
	metal4::OutlineGPU outlineGPU;
	metal4::DistortionGPU distortionGPU;
	metal4::ShieldGPU shieldGPU;metal4::MapDepthGPU mapDepthGPU;metal4::ModelDepthGPU modelDepthGPU;
	double shieldCompileMs=0;
	std::vector<std::pair<NativeTextureSampling,id<MTLSamplerState>>> shieldColorSamplers;
	NativeTextureRegistry* shieldRegistry=nullptr;
	std::shared_ptr<NativeShieldDepthFrame> shieldPending;
	// Non-null only after a source submission actually began and until every
	// successful retirement callback has finished. An error retains it.
	std::shared_ptr<const NativeOpaqueModelFrame> submittedOpaqueSource;
	uint64_t activeSourceSubmission=0;
	metal4::BlueprintGPU blueprintGPU;
	metal4::BlockoutGPU blockoutGPU;
	metal4::PlacementGPU placementGPU;
	metal4::BumpWaterGPU waterGPU;
	metal4::BasicWaterGPU basicWaterGPU;
 metal4::LegacyWaterGPU legacyWaterGPU;
 metal4::DynamicWaterGPU dynamicWaterGPU;
 metal4::LegacyWaterBumpGPU legacyWaterBumpGPU;
	GroundDecalGPU groundDecalGPU;
	GameDecalGPU gameDecalGPU;
	UnitPrimitiveGPU unitPrimitiveGPU;
	metal4::FireSmokeGPU waterFireSmokeGPU;
	__strong id<MTLTexture> waterReflection=nil,waterReflectionDepth=nil;
	__strong id<MTLTexture> waterRefraction=nil,waterRefractionDepth=nil;
 std::array<bool,2> reportedPlanar173{};
#ifdef RECOIL_METAL4_LIVE_SCENE
 std::atomic<uint64_t> mrt174CompletedFrames{0};
#endif
	__strong id<MTLTexture> preWaterColor=nil,preWaterDepth=nil;
	__strong id<MTLTexture> logicalTerrainDepth=nil;
	std::array<id<MTLRenderPipelineState>,2> placementSamplePipelines{};
	metal4::PostprocessGPU postprocessGPU;
	metal4::VolumetricCloudsGPU volumetricCloudsGPU;
	__strong id<MTLTexture> offscreenSourceColor=nil;
	struct ScreenContinuation {
		std::shared_ptr<MainFrameAttachments> main;
		std::shared_ptr<const WorldFrame> source;
		Metal4WorldPresentationResult result;
		std::uint64_t cutoff=0,worldCutoff=0;
		std::uint32_t rootCommands=0,logicalWidth=0,logicalHeight=0;
	};
	std::shared_ptr<ScreenContinuation> screenContinuation;
	std::unique_ptr<MiniMapGPU> standardTerrainGPU = std::make_unique<MiniMapGPU>();
	__strong id<MTLSamplerState> standardShadeSampler = nil;
	uint32_t standardShadeAnisotropy = 0;
	struct TileBatch { __strong id<MTLTexture> texture = nil; __strong id<MTLBuffer> instances = nil; uint32_t count = 0; TerrainSquareViewCache squareViews; };
	struct TileRange {uint32_t batch=0,first=0,count=0;};
	struct MapCache {
		std::shared_ptr<const WorldTerrainTiles> owner;
		std::vector<TileBatch> batches;
		// Disjoint ranges of the SAME cached TileInstance buffers. Original
		// visible-square traversal selects these; this is no new geometry.
		std::vector<std::vector<TileRange>> squareRanges;
	};
	struct PieceCache { __strong id<MTLBuffer> vertices = nil; __strong id<MTLBuffer> indices = nil; uint32_t count = 0; };
	struct ModelCache { std::shared_ptr<const WorldModel> owner; std::vector<PieceCache> pieces; };
	// Readiness covers all uploaded/generated levels and their one-time GPU optimization.
	struct TextureUploadState {
		bool ready = false;
		uint64_t encodedSubmission = 0;
		__strong id<MTLTexture> uploadSource = nil;
	};
	struct TextureCache {
		std::shared_ptr<const WorldTexture> owner;
		__strong id<MTLTexture> texture = nil, sampled = nil;
		std::shared_ptr<TextureUploadState> status = std::make_shared<TextureUploadState>();
	};
	enum LegacyDiagnostic : uint8_t {
		CurrentUnitPose = 1, DamageDisplacement = 2, DamageTexturing = 4,
		TreadUV = 8, FeatureFadeAndNight = 16, ViewerInfo = 32, ModelGamma = 64,
	};
	struct DrawPiece {
		PieceCache* geometry;
		__strong id<MTLTexture> color, other, normal;
		size_t uniformOffset, shadowUniformOffset;
		bool feature, alpha, castsShadow, visible, engineModel, projectile, engineStandard;
		NativeWaterScenePass view=NativeWaterScenePass::Main;
		uint32_t sourceObject=0,sourcePiece=0;
  __strong id<MTLSamplerState> colorSampler=nil,otherSampler=nil;
  std::shared_ptr<const NativeS3OTexturePair> sourceTextures;
  std::function<void(uint64_t)> requireTextures;
		uint8_t legacyDiagnostics = 0; // Metadata only; report at the selected draw.
	};

	explicit Impl(Metal4WindowSurface& borrowed): surface(&borrowed)
	{
		MainThread();
		Require(Metal4WindowSurfaceNativeAccess::Device(borrowed) == SharedRuntime().Device(), "World surface must use the shared renderer's actual device");
	}
	~Impl()
	{
		if (![NSThread isMainThread] || !SharedRuntime().CanSafelyDetach()) std::terminate();
		SetNativeCUSMaterialConsumerReady(false);
		SetNativeTerrainShadingConsumerAvailable(false);
		SetNativeScreenCopyConsumerAvailable(false);
		SetNativeVolumetricCloudConsumerAvailable(false);
		SetNativeFireSmokeConsumerAvailable(false);
		SetNativeMissileThrusterConsumerAvailable(false);
		SetNativeFlamethrowerConsumerAvailable(false);
		SetNativeFireGL4ConsumerAvailable(false);
		SetNativeNapalmPreviewConsumerAvailable(false);
		SetNativeSnowConsumerAvailable(false);
		SetNativeCommandsConsumerAvailable(false);
		SetNativeShieldConsumerAvailable(false);
		SetNativeSSAOConsumerAvailable(false);
		SetNativeBloomConsumerAvailable(false);
		SetNativeGroundDecalConsumerAvailable(false);
		SetNativeGameDecalConsumerAvailable(false);
		SetNativeUnitPrimitiveConsumerAvailable(false);
	}

	void RetainTextureAllocations(RuntimeLease& lease, const TextureCache& cached)
	{
		lease.AddPersistentAllocation(cached.texture);
		if (cached.sampled != cached.texture) lease.AddPersistentAllocation(cached.sampled);
		if (cached.status->uploadSource != nil) lease.AddAllocation(cached.status->uploadSource);
	}

	id<MTLTexture> Texture(RuntimeLease& lease, const std::shared_ptr<const WorldTexture>& owner,
		const NativeCUSTextureObject* modelSource = nullptr)
	{
		Require(owner != nullptr, "World unit is missing an original texture resource");
		auto it = textures.find(owner.get());
		if (it == textures.end()) {
			Require(textures.size() < 512, "World texture cache diagnostic limit exceeded");
			TextureCache cache; cache.owner = owner;
			const auto uploaded = UploadTexture(lease.Device(), *owner, lease.Capabilities().maxTextureDimension2D,
				PrivateModelTexture(modelSource, owner)||InterventionPrivateTexturesEnabled());
			cache.texture = uploaded.texture;
			cache.status->uploadSource = uploaded.uploadSource;
			// Cache hits never change storage. The existing first-use receipt
			// covers any staged copy, generated mips and GPU optimization before
			// publishing this same immutable destination to later consumers.
			cache.status->ready = false;
			cache.sampled = cache.texture;
			if (owner->originalInternalFormat == 0x83F0) {
				// Original RGB BC1 samples alpha1 from the same underlying storage,
				// including upload-only RGBA8 when its base requires real GPU mips.
				cache.sampled = [cache.texture newTextureViewWithPixelFormat:cache.texture.pixelFormat textureType:MTLTextureType2D
					levels:NSMakeRange(0,cache.texture.mipmapLevelCount) slices:NSMakeRange(0,1)
					swizzle:MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed,MTLTextureSwizzleGreen,MTLTextureSwizzleBlue,MTLTextureSwizzleOne)];
				Require(cache.sampled != nil, "Cannot expose original RGB BC1 alpha1 storage view");
			}
			it = textures.emplace(owner.get(), std::move(cache)).first;
		}
		if(!it->second.status->ready&&it->second.status->encodedSubmission&&lease.PreparingWhilePending())
   lease.RetirePreviousSubmission(); // First-use texture producer still owns its receipt.
		RetainTextureAllocations(lease, it->second);
		if (!it->second.status->ready && std::find(pendingMipTextures.begin(),pendingMipTextures.end(),&it->second)==pendingMipTextures.end()) pendingMipTextures.push_back(&it->second);
		return it->second.sampled;
	}

	void EncodePendingTextureMipmaps(id<MTL4CommandBuffer> command, uint64_t submission)
	{
		Require(command != nil && submission != 0, "Original texture mip producer needs its actual shared submission");
		for (const auto* cached: pendingMipTextures) {
   if(cached->status->ready)continue; // N retired after N+1 prepared this list.
			Require(!cached->status->ready && (!cached->status->encodedSubmission || cached->status->encodedSubmission == submission),
				"Original texture mips retain an unretired different submission");
			auto mip = [command computeCommandEncoder]; Require(mip != nil, "Cannot encode original world texture first-use producer");
			if (cached->status->uploadSource != nil) {
				// Copy only supplied levels; generated levels are still undefined.
				[mip copyFromTexture:cached->status->uploadSource sourceSlice:0 sourceLevel:0
					toTexture:cached->texture destinationSlice:0 destinationLevel:0
					sliceCount:1 levelCount:cached->owner->mips.size()];
				[mip barrierAfterEncoderStages:MTLStageBlit beforeEncoderStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
			}
			if(cached->owner->generateMipmaps && cached->texture.mipmapLevelCount > 1){
				[mip generateMipmapsForTexture:cached->texture];
				// Both commands execute in the Blit stage. Preserve every generated
				// texel before optimizing the same immutable storage for GPU reads.
				[mip barrierAfterEncoderStages:MTLStageBlit beforeEncoderStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
			}
			[mip optimizeContentsForGPUAccess:cached->texture];
			[mip barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
			[mip endEncoding];
			cached->status->encodedSubmission = submission;
		}
	}

 std::function<void()> TakeTextureMipCompletion(uint64_t submission)
 {
  std::vector<std::shared_ptr<TextureUploadState>> states;
  for(const auto* cached:pendingMipTextures)if(!cached->status->ready)states.push_back(cached->status);
  pendingMipTextures.clear();
  return [states=std::move(states),submission]{
   for(const auto& state:states)Require(submission&&state->encodedSubmission==submission,"Texture mip completion lost its submitting owner");
   for(const auto& state:states){state->ready=true;state->uploadSource=nil;}
  };
 }
	void CompletePendingTextureMipmaps(uint64_t submission)
	{
		for (const auto* cached: pendingMipTextures)
			Require(submission && cached->status->encodedSubmission == submission, "Cannot complete original texture mips before their actual producer retires");
		for (const auto* cached: pendingMipTextures) { cached->status->ready = true; cached->status->uploadSource = nil; }
		pendingMipTextures.clear();
	}

	id<MTLSamplerState> TerrainSourceSampler(RuntimeLease& lease,const NativeCUSTextureBinding& binding)
	{
		Require(binding.object&&binding.object->allocation.levels,"Original terrain input sampler lacks its actual mip allocation");
		const auto& sampling=binding.object->sampling;const auto last=binding.object->allocation.levels-1;
		// The existing screen sampler cache represents this original state.
		// Other source LOD constraints need the final terrain sampler adapter;
		// never silently discard them while reusing that cache here.
		Require(sampling.compareMode==0&&sampling.baseLevel==0&&sampling.lodBias==0&&
			sampling.minLod<=0&&sampling.maxLod>=float(last)&&sampling.maxLevel>=int(last)&&
			std::isfinite(sampling.anisotropy)&&sampling.anisotropy>=1&&std::floor(sampling.anisotropy)==sampling.anisotropy,
			"Original terrain input requires its exact comparison/base/LOD/anisotropy sampler consumer");
		const bool border=sampling.wrapS==0x812D||sampling.wrapT==0x812D||sampling.wrapS==0x2900||sampling.wrapT==0x2900;
		Require(!border||sampling.borderColor==std::array<float,4>{0,0,0,0}||sampling.borderColor==std::array<float,4>{1,1,1,1},
			"Original terrain input border color requires its exact sampler consumer");
		switch(sampling.minFilter){case 0x2600:case 0x2601:case 0x2700:case 0x2701:case 0x2702:case 0x2703:break;
			default:throw std::runtime_error("Original terrain input minification filter is invalid");}
		Require(sampling.magFilter==0x2600||sampling.magFilter==0x2601,"Original terrain input magnification filter is invalid");
		return screenGPU.PrepareSampler(lease,ui::NativeUITextureHistory::DescribeSampler(binding));
	}
	CUSSourceImage OriginalModelImage(RuntimeLease& lease,const NativeCUSTextureBinding& binding,uint64_t submission,bool validation,double& shaderMs)
	{
		Require(submission&&binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object&&(binding.object->backing||binding.object->image),
			"Original model image lacks its retained source object/content/submission");
		if(std::dynamic_pointer_cast<const NativeLegacyRefractionBacking>(binding.object->backing))return legacyWaterGPU.ResolveCopy(lease,binding,submission);
		if(auto mask=std::dynamic_pointer_cast<const NativeUnitStencilBacking>(binding.object->backing))
			return unitStencilGPU.Resolve(lease,mask->version,submission);
		// S3O publishes both retained pixels and a typed backing. Let that
		// owner reach its existing consumer below; this path owns plain images.
		if(binding.object->image&&!binding.object->backing){
			const auto& source=PublishedWorldTexture(*binding.object);
			CUSSourceImage image;image.texture=Texture(lease,binding.object->image);
			image.sampler=TerrainSourceSampler(lease,binding);image.owner=binding.object;image.flipV=source.flipV;
			const auto status=textures.at(binding.object->image.get()).status;
			image.requireReadable=[status,submission]{Require(status->ready||status->encodedSubmission==submission,
				"Original world image sampled before its common upload/mip producer");};return image;
		}
		if(std::dynamic_pointer_cast<const NativeTerrainInfoBacking>(binding.object->backing))
			return ResolveNativeTerrainInfoImage(lease,binding,submission,TerrainSourceSampler(lease,binding),validation,shaderMs);
		if(std::dynamic_pointer_cast<const NativeTerrainShadingBacking>(binding.object->backing))
			return ResolveNativeTerrainShadingImage(lease,*standardTerrainGPU,binding,submission,TerrainSourceSampler(lease,binding),validation,shaderMs);
		if(std::dynamic_pointer_cast<const NativeCubeTextureBacking>(binding.object->backing))
			return ResolveNativeCubeTexture(lease,binding,submission);
		if(std::dynamic_pointer_cast<const NativeShadowTextureBacking>(binding.object->backing))
			return ResolveNativeShadowTexture(lease,binding,submission);
		if(std::dynamic_pointer_cast<const NativeNamedVolumeBacking>(binding.object->backing))
			return ResolveNativeNamedVolume(lease,binding,submission);
		if(std::dynamic_pointer_cast<const NativePBRImage>(binding.object->backing))
			return pbrSource.Image(lease,binding,submission);
		if(std::dynamic_pointer_cast<const NativeParticleAtlasBacking>(binding.object->backing))
			return particlesGPU.AtlasImage(lease,binding,screenGPU.PrepareSampler(lease,ui::NativeUITextureHistory::DescribeSampler(binding)),submission);
		Require(binding.object->target==NativeCUSTextureTarget::Image2D,
			"Original model image target has no actual paired source consumer");
		if(auto model=std::dynamic_pointer_cast<const NativeModelTextureBacking>(binding.object->backing)){
			Require(model->pixels!=nullptr,"Original model image has no actual retained upload bytes");
			CUSSourceImage image;image.texture=Texture(lease,model->pixels,binding.object.get());
			image.sampler=screenGPU.PrepareSampler(lease,ui::NativeUITextureHistory::DescribeSampler(binding));
			image.owner=binding.object;image.flipV=model->pixels->flipV;
			const auto status=textures.at(model->pixels.get()).status;
			image.requireReadable=[status,submission]{Require(status->ready||status->encodedSubmission==submission,
				"Original model image sampled before its common upload/mip producer");};return image;
		}
		if(auto map=std::dynamic_pointer_cast<const NativeMapTextureBacking>(binding.object->backing)){
			auto resource=std::dynamic_pointer_cast<const MapImage>(map->Resource());
			Require(resource&&resource->texture,"Original model map input has no existing paired resource");
			CUSSourceImage image;image.texture=resource->texture;
			image.sampler=screenGPU.PrepareSampler(lease,ui::NativeUITextureHistory::DescribeSampler(binding));
			image.owner=binding.object;
			if(map->role==NativeMapTextureRole::Height)image.requireReadable=[resource]{Require(resource->Completed(),"Original model height input has no completed upload");};
			else{const auto encoded=normalsEncoded;image.requireReadable=[encoded]{Require(encoded&&*encoded,"Original model normals input precedes its existing producer");};}
			lease.AddAllocation(image.texture);return image;
		}
		if(std::dynamic_pointer_cast<const NativeDeferredColorBacking>(binding.object->backing))
			return ResolveDeferredColorImage(lease,binding,submission);
		// Ordinary named/Lua color images keep the one existing UI image cache.
		// A source-dependent framebuffer needs its actual prepared producer;
		// an absent producer remains a strict source error at this boundary.
		const auto source=OriginalNativeTextureContext().UI().DescribeBinding(binding);
		Require(source.image!=nullptr,"Original model image lacks its captured UI storage");
		auto external=std::make_shared<PreparedScreenExternalImage>();
		auto prepared=screenGPU.PrepareImage(lease,source.image,source.sampler,nullptr,std::nullopt,
			[external](PreparedScreenExternalImage actual){*external=std::move(actual);});
		CUSSourceImage image;image.texture=prepared.texture;image.sampler=prepared.sampler;image.owner=binding.object;
		image.flipV=source.image->flipV;image.depthImage=source.image->format==ScreenTextureFormat::Depth32Float;
		image.requireReadable=[external]{if(external->requireReadable)external->requireReadable();};return image;
	}

	PreparedSensorPreviewImage SensorPreviewImage(RuntimeLease& lease,const NativeSensorPreviewImageInput& input,
		bool requireContents,NativePostprocessConsumer consumer,const PreparedScreenFrame* producers,bool validation,double& milliseconds)
	{
		if(input.radarSource){
			Require(requireContents&&!input.binding.object&&!input.dependency&&!input.version&&!input.allocation&&input.radarSource->info&&input.radarSource->info->radar,
				"Original Preview radar image has competing storage or no actual viewer source");
			auto plan=sensorRadarGPU.Prepare(lease,input.radarSource,validation,milliseconds,true);
			const auto& radar=*input.radarSource->info->radar;auto texture=plan->Radar();
			Require(texture&&texture.pixelFormat==MTLPixelFormatRG8Unorm&&texture.width==radar.width&&texture.height==radar.height,
				"Original Preview radar producer differs from its actual viewer grid");
			auto encoded=std::make_shared<bool>(false);PreparedSensorPreviewImage result;
			result.texture=texture;result.storageOwner=plan;result.flipV=false;
			result.encodeDependencies=[plan,encoded](id<MTL4CommandBuffer> command){if(!*encoded){plan->EncodeDependencies(command);*encoded=true;}};
			result.requireReadable=[encoded]{Require(*encoded,"Original Preview radar read precedes its real producer encoding");};
			result.didComplete=[plan,encoded]{Require(*encoded,"Original Preview radar producer did not encode");plan->DidComplete();};
			return result;
		}
		Require(input.binding.object!=nullptr,"Original Preview image has no captured registry owner");
		PreparedSensorPreviewImage result;result.original=input.binding.object;result.allocation=input.allocation;result.version=input.version;
		if(input.allocation){
			result.storageOwner=input.allocation;
			result.sampler=screenGPU.PrepareSampler(lease,ui::NativeUITextureHistory::DescribeSampler(input.binding));
			if(input.version){
				Require(input.version->image==input.allocation,"Original Preview produced version belongs to another allocation");
				result.texture=postprocessGPU.ResolveImage(lease,input.version,consumer);
				result.requireReadable=[this,version=input.version]{postprocessGPU.RequireReadable(version);};
			}else{
				Require(!requireContents,"Original Preview cannot read texels from undefined allocation-only storage");
				result.texture=postprocessGPU.ResolveAllocation(lease,input.allocation);
			}
			result.flipV=input.dependency?input.dependency->flipV:true;
			return result;
		}
		// Original height and map/model depth keep their existing actual owners,
		// physical sampled types, immutable bindings and producer-readability proof.
		Require(!input.version,"Original Preview version lost its canonical allocation identity");
		auto image=screenGPU.ResolveStartBoxImage(lease,input.binding,producers,consumer);
		result.texture=image.texture;result.sampler=image.sampler;result.storageOwner=std::move(image.storageOwner);
		result.flipV=image.flipV;result.requireReadable=std::move(image.requireReadable);
		Require(!requireContents||bool(result.requireReadable),"Original Preview texel source has no actual readability proof");
		return result;
	}

	CUSSourceImage OriginalTerrainImage(RuntimeLease& lease,const NativeCUSTextureBinding& binding,
		uint64_t submission,id<MTLSamplerState> actualSampler,bool validation,double& shaderMs)
	{
		Require(submission&&binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object&&
			actualSampler&&actualSampler.device==lease.Device(),"Original terrain image lacks its actual source object/sampler/submission");
		if(auto square=std::dynamic_pointer_cast<const NativeTerrainSquareBacking>(binding.object->backing)){
			Require(map.owner&&square->tiles==map.owner,"Original terrain square lost its actual committed map cache");
			const uint64_t index=uint64_t(square->squareZ)*(map.owner->gridWidth/NATIVE_SMF_TILES_PER_SQUARE)+square->squareX;
			const auto batch=size_t(index/2048);Require(batch<map.batches.size(),"Original terrain square has no uploaded map batch");
			return ResolveNativeTerrainSquareImage(lease,binding,map.owner,map.batches[batch].texture,uint32_t(batch*2048),actualSampler,map.batches[batch].squareViews);
		}
		if(std::dynamic_pointer_cast<const NativeTerrainInfoBacking>(binding.object->backing))
			return ResolveNativeTerrainInfoImage(lease,binding,submission,actualSampler,validation,shaderMs);
		if(std::dynamic_pointer_cast<const NativeTerrainShadingBacking>(binding.object->backing))
			return ResolveNativeTerrainShadingImage(lease,*standardTerrainGPU,binding,submission,actualSampler,validation,shaderMs);
		if(std::dynamic_pointer_cast<const NativeCubeTextureBacking>(binding.object->backing))
			return ResolveNativeCubeTexture(lease,binding,submission,actualSampler);
		if(std::dynamic_pointer_cast<const NativeShadowTextureBacking>(binding.object->backing))
			return ResolveNativeShadowTexture(lease,binding,submission,actualSampler);
		if(auto backing=std::dynamic_pointer_cast<const NativeDeferredColorBacking>(binding.object->backing)){
			const auto actual=GetDeferredColorBackingImage(*backing);CUSSourceImage image;image.texture=actual.texture;image.sampler=actualSampler;image.owner=binding.object;image.flipV=true;
			image.requireReadable=[actual,submission]{Require(IsDeferredColorReadable(actual,submission),"Original terrain composite color precedes its same-source MRT producer");};
			lease.AddAllocation(image.texture);lease.Retain(actualSampler);return image;
		}
		if(auto backing=std::dynamic_pointer_cast<const NativeShieldDepthBacking>(binding.object->backing)){
			const auto actual=GetShieldDepthBackingImage(*backing);CUSSourceImage image;image.texture=actual.texture;image.sampler=actualSampler;image.owner=binding.object;image.flipV=true;image.depthImage=true;
			image.requireReadable=[actual,submission]{Require(actual.producer&&IsNativeShieldDepthReadable(actual.producer->encoded,actual.producer->completed,actual.producer->operation==ShieldDepthOperation::AllocationOnly,actual.producer->allocationVerified,actual.producer->submissionIdentity,submission),"Original terrain composite depth precedes its same-source MRT producer");};
			lease.AddAllocation(image.texture);lease.Retain(actualSampler);return image;
		}
		Require(binding.object->target==NativeCUSTextureTarget::Image2D,"Original SMF image is not its actual selected sampler2D target");
		if(auto mapImage=std::dynamic_pointer_cast<const NativeMapTextureBacking>(binding.object->backing)){
			auto resource=std::dynamic_pointer_cast<const MapImage>(mapImage->Resource());
			Require(resource&&resource->texture,"Original terrain map input lacks its actual paired resource");
			CUSSourceImage image;image.texture=resource->texture;image.sampler=actualSampler;image.owner=binding.object;
			if(mapImage->role==NativeMapTextureRole::Height)image.requireReadable=[resource]{Require(resource->Completed(),"Original terrain height sampled before its actual upload completed");};
			else{const auto encoded=normalsEncoded;image.requireReadable=[encoded]{Require(encoded&&*encoded,"Original terrain normals sampled before their actual shared producer");};}
			lease.AddAllocation(image.texture);lease.Retain(actualSampler);return image;
		}
		std::shared_ptr<const WorldTexture> pixels=binding.object->image;
		if(auto model=std::dynamic_pointer_cast<const NativeModelTextureBacking>(binding.object->backing))pixels=model->pixels;
		if(pixels){
			Texture(lease,pixels,binding.object.get());const auto& cached=textures.at(pixels.get());
			CUSSourceImage image;image.texture=cached.sampled;image.sampler=actualSampler;image.owner=binding.object;image.flipV=pixels->flipV;
			const auto status=cached.status;
			image.requireReadable=[status,submission]{Require(status->ready||status->encodedSubmission==submission,
				"Original SMF bitmap sampled before its common upload/mip producer");};
			lease.Retain(actualSampler);return image;
		}
		if(auto uiImage=std::dynamic_pointer_cast<const ui::NativeScreenTextureBacking>(binding.object->backing)){
			Require(uiImage->image!=nullptr,"Original terrain Lua image lost its actual immutable UI backing");
			auto external=std::make_shared<PreparedScreenExternalImage>();
			// The source consumer supplied the exact sampler. Do not construct a
			// limited UI sampler while resolving this same persistent image cache.
			auto prepared=screenGPU.PrepareImage(lease,uiImage->image,ScreenSampler{},nullptr,std::nullopt,
				[external](PreparedScreenExternalImage actual){*external=std::move(actual);},actualSampler);
			CUSSourceImage image;image.texture=prepared.texture;image.sampler=actualSampler;image.owner=binding.object;
			image.flipV=uiImage->image->flipV;image.depthImage=uiImage->image->format==ScreenTextureFormat::Depth32Float||uiImage->image->format==ScreenTextureFormat::Depth24Unorm||uiImage->image->format==ScreenTextureFormat::Depth16Unorm;
			image.clampDepthCompare=uiImage->image->format==ScreenTextureFormat::Depth24Unorm||uiImage->image->format==ScreenTextureFormat::Depth16Unorm;
			image.requireReadable=[external]{if(external->requireReadable)external->requireReadable();};return image;
		}
		throw std::runtime_error("Original terrain image has no actual retained native producer for its selected source");
	}
	void Map(RuntimeLease& lease, const WorldTerrain& terrain)
	{
		const auto& owner = terrain.tiles;
		Require(owner != nullptr && terrain.heights != nullptr, "World scene requires original terrain tiles and corner heights");
		Require(terrain.squaresX > 0 && terrain.squaresZ > 0 && terrain.squaresX <= 8192 && terrain.squaresZ <= 8192 &&
			terrain.squaresX % 4 == 0 && terrain.squaresZ % 4 == 0, "World terrain square dimensions outside diagnostic bounds");
		Require(std::isfinite(terrain.squareSize) && terrain.squareSize > 0, "Invalid world terrain square size");
		Require(owner->gridWidth == terrain.squaresX / 4 && owner->gridHeight == terrain.squaresZ / 4 &&
			owner->tileIndices.size() == size_t(owner->gridWidth) * owner->gridHeight, "World tile grid differs from terrain dimensions");
		Require(owner->uniqueTileCount > 0 && owner->uniqueTileCount <= 1'000'000 &&
			owner->bc1Bytes.size() == size_t(owner->uniqueTileCount) * WorldTerrainTiles::BYTES_PER_TILE, "World original tile payload mismatch");
		Require(lease.Capabilities().bcTextureCompression, "World terrain requires original BC1 support; no substitute texture exists");
		const auto& inputHeights = *terrain.heights;
		Require(inputHeights.width == terrain.squaresX + 1 && inputHeights.height == terrain.squaresZ + 1 &&
			inputHeights.corners.size() == size_t(inputHeights.width) * inputHeights.height, "World corner-height dimensions mismatch");
		const size_t heightBytes = inputHeights.corners.size() * sizeof(float);
		const bool updateHeights = heightOwner != terrain.heights || heights == nil || heights.length != heightBytes;
		if (updateHeights) {
			for(float value:inputHeights.corners)Require(std::isfinite(value),"Non-finite actual terrain height");
		}
		if (map.owner != owner) {
			ValidateNativeSMFTiles(*owner);
			MapCache updated; updated.owner = owner;
			const uint32_t squareColumns = owner->gridWidth / NATIVE_SMF_TILES_PER_SQUARE;
			const uint32_t squareCount = squareColumns * (owner->gridHeight / NATIVE_SMF_TILES_PER_SQUARE);
			const uint32_t batchCount = (squareCount + 2047) / 2048;
			std::vector<std::vector<TileInstance>> instances(batchCount);
			updated.squareRanges.resize(squareCount);
			for (uint32_t z = 0; z < owner->gridHeight; ++z) for (uint32_t x = 0; x < owner->gridWidth; ++x) {
				const uint32_t square = (z / NATIVE_SMF_TILES_PER_SQUARE) * squareColumns + x / NATIVE_SMF_TILES_PER_SQUARE;
				const uint32_t batch=square/2048,first=uint32_t(instances[batch].size());
				auto& ranges=updated.squareRanges[square];
				if (!ranges.empty() && ranges.back().batch==batch && ranges.back().first+ranges.back().count==first)
					++ranges.back().count;
				else ranges.push_back({batch,first,1});
				instances[batch].push_back({x, z, square % 2048, 0});
			}
			updated.batches.reserve(batchCount);
			for (uint32_t batchIndex = 0; batchIndex < batchCount; ++batchIndex) {
				const uint32_t first = batchIndex * 2048;
				const uint32_t layers = std::min(2048u, squareCount - first);
				TileBatch batch;
				batch.texture = UploadNativeSMFSquares(lease, *owner, first, layers);
				batch.count = uint32_t(instances[batchIndex].size());
				if (batch.count) batch.instances = Buffer(lease.Device(), instances[batchIndex].data(), instances[batchIndex].size() * sizeof(TileInstance), @"Exact SMF tile-grid instances");
				updated.batches.push_back(std::move(batch));
			}
			map = std::move(updated);
		}
		if (updateHeights) {
			if (heights == nil || heights.length != heightBytes) heights = Buffer(lease.Device(), nullptr, heightBytes, @"Current unsynced terrain corner heights");
			std::memcpy(heights.contents, inputHeights.corners.data(), heightBytes);
			// Retain the validated immutable snapshot. A new/deformed owner must
			// validate and upload; address reuse cannot masquerade as unchanged.
			heightOwner = terrain.heights;
			normalsReady = false;
		}
		lease.AddAllocation(heights);
		if (terrainNormals == nil || terrainNormals.width != inputHeights.width || terrainNormals.height != inputHeights.height) {
			MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG16Float width:inputHeights.width height:inputHeights.height mipmapped:NO];
			d.storageMode = MTLStorageModePrivate; d.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsagePixelFormatView;
			terrainNormals = [lease.Device() newTextureWithDescriptor:d]; Require(terrainNormals != nil, "Cannot allocate original SMF normal producer"); normalsReady = false;
		}
		lease.AddAllocation(terrainNormals);
		for (const auto& batch: map.batches) if (batch.count) { lease.AddPersistentAllocation(batch.texture); lease.AddPersistentAllocation(batch.instances); }
	}

	ModelCache& Model(RuntimeLease& lease, const std::shared_ptr<const WorldModel>& owner)
	{
		Require(owner != nullptr && owner->pieces.size() <= 65'534, "Invalid world loaded model");
		auto it = models.find(owner.get());
		if (it == models.end()) {
			Require(models.size() < 256, "World model cache diagnostic limit exceeded");
			ModelCache cache; cache.owner = owner; cache.pieces.resize(owner->pieces.size());
			size_t vertexCount = 0, indexCount = 0;
			for (size_t pieceIndex = 0; pieceIndex < owner->pieces.size(); ++pieceIndex) {
				const auto& input = owner->pieces[pieceIndex]; auto& output = cache.pieces[pieceIndex];
				vertexCount += input.vertices.size(); indexCount += input.indices.size();
				Require(vertexCount <= 2'000'000 && indexCount <= 12'000'000, "World model geometry exceeds diagnostic bounds");
				// Original HasGeometryData/PostProcessGeometry leaves emitter-only
				// pieces unskinned. Keep their transform slots and CPU emit vertices,
				// but allocate and validate GPU geometry only for drawable pieces.
				if (input.indices.size() < 3) continue;
				Require(input.indices.size() % 3 == 0, "World model has incomplete triangles: " + owner->sourceName + "#" + input.name);
				for (const auto& inputVertex: input.vertices) {
					for (float value: inputVertex.position) Require(std::isfinite(value), "Non-finite world model position");
					for (float value: inputVertex.uv[0]) Require(std::isfinite(value), "Non-finite world model UV");
					if (inputVertex.boneIds[0] != pieceIndex || inputVertex.boneWeights != std::array<uint8_t,4>{255,0,0,0})
						throw std::runtime_error("Unsupported world model bone data: " + owner->sourceName + "#" + input.name + " bone=" + std::to_string(inputVertex.boneIds[0]) + " piece=" + std::to_string(pieceIndex));
				}
				for (uint32_t index: input.indices) Require(index < input.vertices.size(), "World model index exceeds local piece vertices");
				if (!input.indices.empty()) {
					output.vertices = Buffer(lease.Device(), input.vertices.data(), input.vertices.size() * sizeof(WorldModelVertex), String(input.name));
					output.indices = Buffer(lease.Device(), input.indices.data(), input.indices.size() * sizeof(uint32_t), @"Original loaded S3O local triangle indices");
					output.count = uint32_t(input.indices.size());
				}
			}
			it = models.emplace(owner.get(), std::move(cache)).first;
		}
		return it->second;
	}

	void Pipelines(RuntimeLease& lease, MTLPixelFormat format, bool validation, double& elapsed)
	{
		if (unitPipeline != nil && pipelineFormat == format && pipelineValidation == validation && pipelineSampleShading == lease.SampleShadingEnabled(mainSampling.samples)) return;
		const auto start = Clock::now(); NSError* error = nil;
		MTL4LibraryDescriptor* libraryDescriptor = [MTL4LibraryDescriptor new];
		libraryDescriptor.source = String(WORLD_SHADER); libraryDescriptor.name = @"Original SMF and selected CUS forward contributions";
		library = lease.NewLibrary(libraryDescriptor,&error);
		Require(library != nil, "Cannot compile actual-world MSL: " + Error(error));
		auto pipeline = [&](NSString* vertexName, NSString* fragmentName, bool alpha = false, bool depthOnly = false, MTLPixelFormat targetFormat = MTLPixelFormatInvalid, int shadowColorPresence = -1) -> id<MTLRenderPipelineState> {
			MTL4LibraryFunctionDescriptor* vertexFunction = [MTL4LibraryFunctionDescriptor new]; vertexFunction.library = library; vertexFunction.name = vertexName;
			MTL4LibraryFunctionDescriptor* fragmentFunction = [MTL4LibraryFunctionDescriptor new]; fragmentFunction.library = library; fragmentFunction.name = fragmentName;
			MTL4RenderPipelineDescriptor* descriptor = [MTL4RenderPipelineDescriptor new];
			descriptor.vertexFunctionDescriptor = vertexFunction; descriptor.fragmentFunctionDescriptor = fragmentFunction;
			if (shadowColorPresence >= 0) {
				const bool present = shadowColorPresence != 0;
				auto constants = [MTLFunctionConstantValues new];
				[constants setConstantValue:&present type:MTLDataTypeBool atIndex:0];
				auto specialized = [MTL4SpecializedFunctionDescriptor new];
				specialized.functionDescriptor = fragmentFunction; specialized.constantValues = constants;
				descriptor.fragmentFunctionDescriptor = specialized;
			}
			descriptor.rasterSampleCount = (depthOnly || targetFormat != MTLPixelFormatInvalid) ? 1 : mainSampling.samples;
			descriptor.colorAttachments[0].pixelFormat = depthOnly ? MTLPixelFormatInvalid : (targetFormat==MTLPixelFormatInvalid?format:targetFormat);
			if (alpha) {
				auto blend = descriptor.colorAttachments[0]; blend.blendingState = MTL4BlendStateEnabled;
				blend.sourceRGBBlendFactor = blend.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
				blend.destinationRGBBlendFactor = blend.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
			}
			descriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
			MTL4PipelineOptions* options = [MTL4PipelineOptions new]; options.shaderValidation = ShaderValidationMode(validation);
			descriptor.options = options;
			id<MTLRenderPipelineState> result = lease.NewRenderPipeline(descriptor,&error);
			Require(result != nil, "Cannot create actual-world Metal4 pipeline: " + Error(error));
			return result;
		};
		unitPipeline = pipeline(@"UnitVertexMain", @"UnitFragmentMain", true);
		engineModelPipeline = pipeline(@"UnitVertexMain", @"EngineModelFragmentMain");
		// The array index is part of each PSO's identity, independently of the
		// existing format/validation library generation. Never reuse a required
		// texture16 signature for a frame with no shadow-color publication.
		for (int presence = 0; presence < 2; ++presence) {
			placementSamplePipelines[presence] = pipeline(@"PlacementSampleVertex", @"PlacementSampleFragment", true, false, MTLPixelFormatInvalid, presence);
			engineStandardPipelines[presence] = pipeline(@"UnitVertexMain", @"EngineStandardModelFragmentMain", false, false, MTLPixelFormatInvalid, presence);
			engineStandardAlphaPipelines[presence] = pipeline(@"UnitVertexMain", @"EngineStandardModelFragmentMain", true, false, MTLPixelFormatInvalid, presence);
			waterStandardPipelines[presence] = pipeline(@"UnitVertexMain", @"EngineStandardModelFragmentMain", false, false, MTLPixelFormatRGBA8Unorm, presence);
			waterStandardAlphaPipelines[presence] = pipeline(@"UnitVertexMain", @"EngineStandardModelFragmentMain", true, false, MTLPixelFormatRGBA8Unorm, presence);
		}
		SetNativePlacementSamplesAvailable(true);
		engineStandardShadowPipeline = pipeline(@"EngineStandardShadowVertex", @"EngineStandardShadowFragment", false, true);
		alphaPipeline = pipeline(@"UnitVertexMain", @"UnitFragmentMain", true);
		waterUnitPipeline=pipeline(@"UnitVertexMain",@"UnitFragmentMain",true,false,MTLPixelFormatRGBA8Unorm);
		waterEnginePipeline=pipeline(@"UnitVertexMain",@"EngineModelFragmentMain",false,false,MTLPixelFormatRGBA8Unorm);
		shadowModelPipeline = pipeline(@"ShadowModelVertex", @"ShadowModelFragment", false, true);
		shadowTerrainPipeline = pipeline(@"ShadowTerrainVertex", @"ShadowTerrainFragment", false, true);
		logicalDepthCopyPipeline = pipeline(@"LogicalSceneDepthCopyVertex", @"LogicalSceneDepthCopyFragment", false, true);
		MTL4ComputePipelineDescriptor* compute = [MTL4ComputePipelineDescriptor new];
		MTL4LibraryFunctionDescriptor* function = [MTL4LibraryFunctionDescriptor new]; function.library = library; function.name = @"WorldTerrainNormals";
		compute.computeFunctionDescriptor = function;
		MTL4PipelineOptions* computeOptions = [MTL4PipelineOptions new]; computeOptions.shaderValidation = ShaderValidationMode(validation); compute.options = computeOptions;
		normalPipeline = [lease.Compiler() newComputePipelineStateWithDescriptor:compute compilerTaskOptions:nil error:&error];
		Require(normalPipeline != nil, "Cannot compile original terrain normal producer: " + Error(error));
		function.name=@"WaterSceneColorCopy";
		// Metal copies this function descriptor when assigned; changing the
		// local function name alone would compile WorldTerrainNormals again.
		compute.computeFunctionDescriptor=function;
		waterColorCopyPipeline=[lease.Compiler() newComputePipelineStateWithDescriptor:compute compilerTaskOptions:nil error:&error];
		Require(waterColorCopyPipeline!=nil,"Cannot compile original water scene color copy: "+Error(error));
		function.name=@"LogicalSceneColorCopy";compute.computeFunctionDescriptor=function;
		logicalColorCopyPipeline=[lease.Compiler() newComputePipelineStateWithDescriptor:compute compilerTaskOptions:nil error:&error];
		Require(logicalColorCopyPipeline!=nil,"Cannot compile logical scene color copy: "+Error(error));
		pipelineFormat = format; pipelineValidation = validation; pipelineSampleShading = lease.SampleShadingEnabled(mainSampling.samples);
		elapsed = Milliseconds(start);
	}

	Metal4WindowSurface* surface;
	MainAttachmentStorage mainStorage;
	NativeMainSampling mainSampling;
	__strong id<MTLTexture> terrainDepthSamples=nil;
	MapCache map;
	std::unordered_map<const WorldModel*, ModelCache> models;
	std::unordered_map<const WorldTexture*, TextureCache> textures;
	std::vector<TextureCache*> pendingMipTextures;
	__strong id<MTLBuffer> heights = nil;
	std::shared_ptr<const WorldTerrainHeights> heightOwner;
	__strong id<MTLTexture> depth = nil, stencil = nil, terrainDepth = nil, shadowDepth = nil, terrainNormals = nil;
	bool normalsReady = false;
	__strong id<MTLLibrary> library = nil;

	__strong id<MTLRenderPipelineState> unitPipeline = nil, engineModelPipeline = nil, engineStandardShadowPipeline = nil, alphaPipeline = nil, shadowModelPipeline = nil, shadowTerrainPipeline = nil;
	__strong id<MTLComputePipelineState> normalPipeline = nil,waterColorCopyPipeline=nil,logicalColorCopyPipeline=nil;
	__strong id<MTLRenderPipelineState> logicalDepthCopyPipeline=nil;
	__strong id<MTLRenderPipelineState> waterUnitPipeline=nil,waterEnginePipeline=nil;
	std::array<id<MTLRenderPipelineState>,2> engineStandardPipelines{},engineStandardAlphaPipelines{};
	std::array<id<MTLRenderPipelineState>,2> waterStandardPipelines{},waterStandardAlphaPipelines{};
	__strong id<MTLSamplerState> shadowSampler = nil;
	std::array<id<MTLSamplerState>, 5> terrainSamplers{};
	std::array<uint32_t, 5> terrainSamplerAnisotropy{};
	MTLPixelFormat pipelineFormat = MTLPixelFormatInvalid;
	bool pipelineValidation = false;
	bool pipelineSampleShading=false;
	uint64_t presentationFrame = 0;
	uint64_t screenTargetSubmission = 0;
};

Metal4WindowSurface AttachMetal4RendererSurface(SDL_Window* window, const Metal4SurfaceConfig& config)
{
	MainThread();
	return Metal4WindowSurfaceNativeAccess::AttachWithDevice(window, SharedRuntime().Device(), config);
}

bool Metal4RendererCanSafelyDetachSurface()
{
	return SharedRuntime().CanSafelyDetach();
}

Metal4WorldRenderer::Metal4WorldRenderer(Metal4WindowSurface& surface,int requestedSamples): impl(std::make_unique<Impl>(surface))
{
	@autoreleasepool {
	RuntimeLease lease;
	CAMetalLayer* layer = Metal4WindowSurfaceNativeAccess::Layer(surface);
	Require(layer != nil && layer.device == lease.Device(), "Native family initialization requires the renderer's actual attached device");
	const auto initial=surface.Snapshot();
	impl->mainSampling=SelectMainSampling(lease,impl->mainStorage,requestedSamples,initial.renderWidth,initial.renderHeight,layer.pixelFormat).actual;
	const auto mainSamples=impl->mainSampling.samples;
	double compileMs = 0;
	impl->Pipelines(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs);
	impl->standardTerrainGPU->Initialize(lease,NativeDiagnosticInstrumentation,compileMs);
	impl->screenGPU.InitializeDiagnosticPrograms(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->postprocessGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs);
	impl->volumetricCloudsGPU.Initialize(lease,NativeDiagnosticInstrumentation,compileMs);
	impl->sensor31657CircleGPU.Initialize(lease,compileMs);
	impl->screenGPU.SetSharedImageProvider([owner=impl.get()](metal4::RuntimeLease& sourceLease,
		const std::shared_ptr<const NativePostprocessVersion>& version,std::optional<NativePostprocessConsumer> consumer) {
		return metal4::PreparedScreenExternalImage{owner->postprocessGPU.ResolveImage(sourceLease,version,consumer),version,
			[owner,version]{owner->postprocessGPU.RequireReadable(version);}};
	});
	impl->postprocessGPU.SetCPUImageProvider([owner=impl.get()](metal4::RuntimeLease& sourceLease,
		const std::shared_ptr<const ScreenTexture>& image) {
		// Validated immediate CPU storage has no pending render producer.
		// Reuse the actual UI BC upload/cache and the submission's own lease.
		return owner->screenGPU.PrepareImage(sourceLease,image,ScreenSampler{}).texture;
	});
	impl->particlesGPU.InitializeLuaNano(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->projectileEffectsGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->energyExplosionGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->lightningGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->fireSmokeGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->missileThrusterGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->flamethrowerGPU.Initialize(lease,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,NativeDiagnosticInstrumentation,compileMs);
	impl->fireGL4GPU.Initialize(lease,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,NativeDiagnosticInstrumentation,compileMs);
	impl->napalmPreviewGPU.Initialize(lease,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,NativeDiagnosticInstrumentation,compileMs);
	impl->snowGPU.Initialize(lease,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,NativeDiagnosticInstrumentation,compileMs);
	impl->commandsGPU.Initialize(lease,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,NativeDiagnosticInstrumentation,compileMs);
	impl->overlayGPU.InitializeReclaimLines(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->reclaimFieldGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->objectSpotlightGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->grassGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->resourceSpotsGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
  impl->sensorLOSGPU.Initialize(lease,compileMs);
	impl->mapDepthGPU.Initialize(lease,impl->library,NativeDiagnosticInstrumentation,compileMs);
	impl->modelDepthGPU.Initialize(lease,impl->library,NativeDiagnosticInstrumentation,compileMs);
	impl->shieldGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->ssaoGPU.Initialize(lease,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->bloomGPU.Initialize(lease,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->blueprintGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->blockoutGPU.SetNormalImageProvider([owner=impl.get()](metal4::RuntimeLease& actual,const std::shared_ptr<const ScreenTexture>& image,const ScreenSampler& sampling){return owner->screenGPU.PrepareImage(actual,image,sampling);});
	impl->blockoutGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->placementGPU.Initialize(lease,layer.pixelFormat,NativeDiagnosticInstrumentation,compileMs,mainSamples);
	impl->groundDecalGPU.Initialize(lease,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,NativeDiagnosticInstrumentation,compileMs);
	impl->gameDecalGPU.Initialize(lease,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,NativeDiagnosticInstrumentation,compileMs);
	impl->unitPrimitiveGPU.Initialize(lease,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,NativeDiagnosticInstrumentation,compileMs);
	SetNativeCUSMaterialConsumerReady(true);
	SetNativeTerrainShadingConsumerAvailable(true);
	// These publish paired functional consumers only after every required
	// pipeline, shared image provider and source-order encoder is connected.
	// They do not assert cross-driver pixel equivalence or generic GLSL support.
	SetNativeScreenCopyConsumerAvailable(true);
	SetNativeVolumetricCloudConsumerAvailable(true);
	SetNativeFireSmokeConsumerAvailable(true);
	SetNativeMissileThrusterConsumerAvailable(true);
	// The actual Flame compiler, source-stage, shared atlas/mip and same-submit
	// consumer are paired. This flag does not assert complete pixel parity.
	SetNativeFlamethrowerConsumerAvailable(true);
	// The original Fire draw consumer has passed actual MSL/PSO and resource
	// validation. Original VFS/app pixels and deferred-light acceptance remain open.
	SetNativeFireGL4ConsumerAvailable(true);
	// The actual procedural program, original PreUnit/state capture and common
	// retirement are paired. Complete original pixel/app parity remains pending.
	SetNativeNapalmPreviewConsumerAvailable(true);
	SetNativeSnowConsumerAvailable(true);
	SetNativeCommandsConsumerAvailable(true);
	SetNativeGroundDecalConsumerAvailable(true);
	SetNativeGameDecalConsumerAvailable(true);
	SetNativeUnitPrimitiveConsumerAvailable(true);
	}
}
NativeMainSampling Metal4WorldRenderer::ActualMainSampling() const { return impl->mainSampling; }

Metal4WorldRenderer::~Metal4WorldRenderer() { if(impl && !SharedRuntime().CanSafelyDetach())std::terminate(); }
Metal4WorldRenderer::Metal4WorldRenderer(Metal4WorldRenderer&&) noexcept = default;
Metal4WorldRenderer& Metal4WorldRenderer::operator=(Metal4WorldRenderer&& other) noexcept { if(this!=&other){if(impl&&!SharedRuntime().CanSafelyDetach())std::terminate();impl=std::move(other.impl);}return *this; }

void Metal4WorldRenderer::ClearWaterResources()
{
 MainThread();Require(impl!=nullptr,"World renderer was moved from");
 RuntimeCleanupLease lease;
 impl->waterGPU.ClearResources();
 impl->waterReflection=nil;impl->waterReflectionDepth=nil;impl->preWaterColor=nil;
 impl->waterRefraction=nil;impl->waterRefractionDepth=nil;
 impl->preWaterColor=nil;impl->preWaterDepth=nil;
}

bool Metal4WorldRenderer::DeferredPostprocessConsumersInitialized() const noexcept
{
	return impl && impl->ssaoGPU.Initialized() && impl->bloomGPU.Initialized();
}

void Metal4WorldRenderer::ClearBasicWaterResources()
{
 MainThread();Require(impl!=nullptr,"World renderer was moved from");
 RuntimeCleanupLease lease;impl->basicWaterGPU.ClearResources();
}

void Metal4WorldRenderer::ClearDynamicWaterResources(){
 MainThread();Require(impl!=nullptr,"World renderer was moved from");RuntimeCleanupLease lease;impl->dynamicWaterGPU.ClearResources();
 impl->waterReflection=nil;impl->waterReflectionDepth=nil;impl->waterRefraction=nil;impl->waterRefractionDepth=nil;
}
void Metal4WorldRenderer::ClearLegacyWaterResources()
{
 MainThread();Require(impl!=nullptr,"World renderer was moved from");
 RuntimeCleanupLease lease;impl->legacyWaterBumpGPU.ClearResources();
 impl->waterReflection=nil;impl->waterReflectionDepth=nil;impl->preWaterColor=nil;
}

bool Metal4WorldRenderer::CanInitializeWater() const
{
 // Structural availability only: the constructor compiled the actual scene
 // consumers. Original terrain programs and all same-draw image contracts are checked
 // by Prepare; this does not assert cross-driver 100% pixel equivalence.
 return impl && SharedRuntime().Capabilities().directMetal4 &&
  impl->library && impl->waterUnitPipeline &&
  impl->waterEnginePipeline && impl->waterStandardPipelines[0] && impl->waterStandardPipelines[1] &&
  impl->waterStandardAlphaPipelines[0] && impl->waterStandardAlphaPipelines[1] && impl->waterColorCopyPipeline && impl->logicalColorCopyPipeline && impl->logicalDepthCopyPipeline;
}

std::shared_ptr<const void> Metal4WorldRenderer::Sensor31657CircleConsumerLifetime() const
{
	MainThread();Require(impl!=nullptr,"Sensor family queried after renderer destruction");
	return impl->sensor31657CircleGPU.CompiledFamilyLifetime();
}

std::uint32_t Metal4WorldRenderer::SensorPreviewMaximumDimension() const
{
	MainThread();Require(impl!=nullptr,"Sensor Preview has no actual renderer/device");
	return SharedRuntime().Capabilities().maxTextureDimension2D;
}
std::shared_ptr<const NativeSensorPreviewProgramConsumer> Metal4WorldRenderer::CompileOriginalSensorPreviewProgram(const NativeSensorPreviewProgramSource& source)
{
	MainThread();Require(impl!=nullptr,"Sensor Preview compile requires its actual renderer");
	RuntimeLease lease;double milliseconds=0;
	return impl->sensorPreviewGPU.CompileOriginalStages(lease,source,milliseconds);
}

void Metal4WorldRenderer::ClearStaticResources()
{
	MainThread(); Require(impl != nullptr, "World renderer was moved from");
	RuntimeCleanupLease lease;
	impl->waterGPU.ClearResources();
	impl->waterFireSmokeGPU.ClearResources();
	impl->groundDecalGPU.ClearResources();
	impl->gameDecalGPU.ClearResources();
	impl->unitPrimitiveGPU.ClearResources();impl->startBoxGPU.ClearResources();impl->weaponRangeGPU.ClearResources();impl->buildSquareGPU.ClearResources();
	impl->sensor31657CircleGPU.ClearResources();
	impl->sensorPreviewGPU.ClearResources();impl->sensorRadarGPU.ClearResources();
	impl->basicWaterGPU.ClearResources();
	impl->legacyWaterBumpGPU.ClearResources();impl->dynamicWaterGPU.ClearResources();
	impl->waterReflection=nil;impl->waterReflectionDepth=nil;impl->preWaterColor=nil;
	impl->waterRefraction=nil;impl->waterRefractionDepth=nil;
	impl->preWaterColor=nil;impl->preWaterDepth=nil;
	impl->map = {}; impl->models.clear(); impl->textures.clear(); impl->heights = nil;
	impl->heightOwner.reset(); impl->pendingMipTextures.clear();
	impl->mapEdgeGPU.ClearResources(); impl->particlesGPU.ClearResources(); impl->environmentGPU.ClearResources();
	impl->pbrSource.ResetWorld();impl->pbrSubmission=0;
	impl->overlayGPU.ClearResources(); impl->projectileEffectsGPU.ClearResources();
	impl->energyExplosionGPU.ClearResources();
	impl->lightningGPU.ClearResources();
	impl->fireSmokeGPU.ClearResources();
	impl->missileThrusterGPU.ClearResources();
	impl->flamethrowerGPU.ClearResources();
	impl->fireGL4GPU.ClearResources();
	impl->napalmPreviewGPU.ClearResources();
	impl->snowGPU.ClearResources();
	impl->commandsGPU.ClearResources();
	impl->reclaimFieldGPU.ClearResources();
	impl->objectSpotlightGPU.ClearResources();
	impl->grassGPU.ClearResources();
  impl->resourceSpotsGPU.ClearResources();
  impl->sensorLOSGPU.ClearResources();
	impl->ssaoGPU.ClearResources();
	impl->bloomGPU.ClearResources();
	impl->deferredLightsGPU.ClearResources();
	impl->unitStencilGPU.ClearResources();
	impl->outlineGPU.ClearResources();
	impl->distortionGPU.ClearResources();
	AbortShieldPublications();impl->shieldGPU.ClearResources();impl->mapDepthGPU.ClearResources();impl->modelDepthGPU.ClearResources();
	impl->blueprintGPU.ClearResources();
	impl->blockoutGPU.ClearResources();
	impl->terrainNormals = nil; impl->normalsReady = false;impl->normalsEncoded=std::make_shared<bool>(false);
	impl->standardTerrainGPU->ClearResources();
	impl->screenContinuation.reset();impl->postprocessGPU.ClearResources();impl->offscreenSourceColor=nil;
	impl->volumetricCloudsGPU.ClearResources();
	impl->depth = nil; impl->stencil = nil; impl->terrainDepth = nil; impl->terrainDepthSamples=nil; impl->shadowDepth = nil;impl->mainStorage.Clear();
	impl->screenGPU.ClearResources();
	SharedWorldInfoGPU().ClearResources();
}

NativeEnvironmentStageReceipt Metal4WorldRenderer::CaptureEnvironmentInputs(std::shared_ptr<const NativeWorldEnvironmentFrame> frame, bool shaderValidation) const
{
	MainThread(); Require(impl != nullptr && frame != nullptr, "Original environment stage is missing");
 // The scene capture owner keeps the receipt; no active renderer slot changes.
 return {std::move(frame),shaderValidation};
}

bool Metal4WorldRenderer::SubmitScreenTargets(const ScreenFrameView& frame, bool shaderValidation)
{
	return SubmitMaintenance(&frame, {}, shaderValidation, {}, CapturePBRSource()).screenTargetsCompleted;
}


std::optional<std::vector<std::uint8_t>> Metal4WorldRenderer::ReadIndependentTargetPrefix(
 const ui::NativeUIReadbackPrefix& prefix,ScreenScissor rectangle,bool shaderValidation)
{
 @autoreleasepool {
  MainThread();Require(impl!=nullptr,"World renderer was moved from");
  if(!prefix.readColor)return std::nullopt;
  Require(prefix.cutoff && prefix.root && prefix.coordinateWidth && prefix.coordinateHeight,
   "SaveImage target prefix lacks its actual source position/extent");
  std::vector<ScreenRenderTargetPass> selected;
  std::vector<const ScreenTexture*> visiting,visited;
  const auto collect=[&](auto&& self,const std::shared_ptr<const ScreenTexture>& image)->bool {
   if(!image)return true;
   if(std::find(visited.begin(),visited.end(),image.get())!=visited.end())return true;
   Require(std::find(visiting.begin(),visiting.end(),image.get())==visiting.end(),
    "SaveImage target dependency graph contains a feedback cycle");
   auto found=std::find_if(prefix.targets.begin(),prefix.targets.end(),[&](const auto& pass){
    return pass.destination==image || pass.depthDestination==image;
   });
   // External/current-frame copies require the source transaction even if a
   // caller forgot a broad sourceDependent flag. Retired RTT history itself is
   // verified by ScreenGPU's exact owner/initialization receipt below.
   if(found==prefix.targets.end()){
    if(image->postprocessVersion)return false;
    if(!image->sourceTexture)return true;
    const auto& object=*image->sourceTexture;
    if(object.image && !object.backing)return true; // Owned static pixels plus actual mip producer below.
    if(const auto model=std::dynamic_pointer_cast<const NativeModelTextureBacking>(object.backing))
     return model->pixels&&!model->pixels->mips.empty(); // Actual immutable model pixels use the same queued upload/mip producer.
    const auto height=std::dynamic_pointer_cast<const NativeMapTextureBacking>(object.backing);
    return height && height->role==NativeMapTextureRole::Height && height->heights;
   }
   if(found->sourceStage!=NativePostprocessStage::Screen &&
    !(found->editorTarget && found->sourceStage==NativePostprocessStage::World))return false;
   Require(found->commandSequence && found->commandSequence<prefix.cutoff && !found->actualSubmission,
    "SaveImage target prefix contains future or already-submitted work");
   visiting.push_back(image.get());
   if(!self(self,found->previous) || !self(self,found->previousDepth))return false;
   for(const auto& command:found->drawList->Commands()){
    if(ScreenCommandRequiresMapSource(command))return false;
    for(const auto& dependency:ScreenCommandImages(command))if(!self(self,dependency))return false;
   }
   visiting.pop_back();visited.push_back(image.get());
   if(std::none_of(selected.begin(),selected.end(),[&](const auto& pass){return pass.destination==found->destination;})){
    selected.push_back(*found);
    // The complete dependency closure above proved this target only uses
    // static uploads/immutable height or previously retired RTT versions.
    // A conservative recording-time flag is not a reason to draw the world.
    selected.back().sourceDependent=false;
   }
   return true;
  };
  if(!collect(collect,prefix.readColor))return std::nullopt;
  RuntimeLease lease;const auto start=Clock::now();double shaderCompileMs=0;
  const auto submission=CreateShieldSubmissionIdentity();impl->pbrSubmission=submission;
  impl->pendingMipTextures.clear();
  ScreenDrawList empty;
  ScreenFrameView frame{&empty,prefix.coordinateWidth,prefix.coordinateHeight,{},selected};
  std::shared_ptr<PreparedScreenFrame> prepared=impl->screenGPU.Prepare(lease,frame,
   prefix.coordinateWidth,prefix.coordinateHeight,prefix.coordinateWidth,prefix.coordinateHeight,
   MTLPixelFormatRGBA8Unorm,shaderValidation,shaderCompileMs);
  auto image=prepared->ResolvePreviousImage(lease,prefix.readColor,
   NativePostprocessConsumer{prefix.readSourceStage,prefix.readSourceStage==NativePostprocessStage::Screen?0:prefix.cutoff,
    std::uint32_t(prefix.root->Commands().size()),prefix.cutoff});
  auto readback=impl->screenGPU.PrepareReadback(lease,std::move(image),rectangle);
  std::shared_ptr<PreparedScreenMipmaps> mipmaps=impl->screenGPU.PrepareMipmaps(lease);
  auto lifetime=[M4OriginalSourceLifetime new];lifetime->screen=prepared;
  if(mipmaps)lifetime->waterGround.push_back(mipmaps);lease.Retain(lifetime);
  lease.CommitResources();auto command=lease.Begin("Original SaveImage independent target prefix");
  ui::GetRecorder().RecordTargetSubmission(selected,submission);
  impl->EncodePendingTextureMipmaps(command,submission);
  if(mipmaps)mipmaps->EncodeDependencies(command);
  // Only the proven dependency closure is present. Preserve its real World
  // and Screen metadata, and execute their shared topological target order.
  // No generic world recorder or future terrain draw is executed.
  prepared->EncodeIndependentTargetClosure(command);
  Require(prepared->AllTargetsEncoded(),"SaveImage independent closure did not encode every selected producer");
  readback->Encode(command);
  lease.SubmitAndWait(start,impl->screenTargetSubmission++);
  try {
   readback->DidComplete();
   impl->CompletePendingTextureMipmaps(submission);
   prepared->DidComplete();if(mipmaps)mipmaps->DidComplete();
   ui::GetRecorder().RetireTargetSubmission(submission);
  } catch(...) {
   // SubmitAndWait succeeded, so the lease destructor alone will not poison
   // a subsequent bookkeeping failure. Keep this exact request/receipts and
   // prohibit reuse until teardown; never roll actual GPU writes back or let
   // stale publication receipts enter another otherwise reusable context.
   lease.PoisonAfterBegin();throw;
  }
  return readback->PixelsRGBA8();
 }
}

Metal4MaintenanceResult Metal4WorldRenderer::SubmitMaintenance(const ScreenFrameView* frame,
	std::shared_ptr<const NativeWorldInfoFrame> worldInfoFrame, bool shaderValidation,
	std::shared_ptr<const NativeUnitStencilFrame> unitStencilFrame, std::optional<NativePBRDeclarationReceipt> capturedPBR)
{
	@autoreleasepool {
		MainThread(); Require(impl != nullptr, "World renderer was moved from");
		Metal4MaintenanceResult result;
  // This public maintenance path is synchronous/main-only. An explicit empty
  // capture must not be replaced by later source declarations.
  const auto pbrDeclarations=capturedPBR?*capturedPBR:CapturePBRSource();
		std::vector<ScreenRenderTargetPass> independent;
		if (frame) {
			for (const auto& pass: frame->targetPasses)
				if (!pass.sourceDependent && pass.sourceStage==NativePostprocessStage::Screen) independent.push_back(pass);
		}
		const bool stencilUpdates=unitStencilFrame && !unitStencilFrame->passes.empty();
		if (independent.empty() && !stencilUpdates && !worldInfoFrame && !HasPendingPBRSource(pbrDeclarations)) return result;
		RuntimeLease lease;
		const auto preparationStart = Clock::now();
		double shaderCompileMs = 0;
		// DrawGenesis can run while the original world draw is skipped. BRDF
		// has no world/cube dependency and still executes in the common submit.
		const auto sourceSubmission=CreateShieldSubmissionIdentity();
		impl->pbrSubmission=sourceSubmission;
		impl->particlesGPU.PrepareAtlasImages(sourceSubmission);
		auto pbrBRDF=impl->pbrSource.Prepare(lease,NativePBRKind::BRDF,{},pbrDeclarations);
		std::shared_ptr<PreparedUnitStencil> unitStencil;
		if(stencilUpdates)unitStencil=impl->unitStencilGPU.Prepare(lease,unitStencilFrame,shaderValidation,shaderCompileMs,sourceSubmission);
		std::shared_ptr<PreparedWorldInfo> worldInfo;
		if (worldInfoFrame)
			worldInfo = SharedWorldInfoGPU().Prepare(lease, std::move(worldInfoFrame), shaderValidation, shaderCompileMs);
		ScreenDrawList emptyScreen;
		ScreenFrameView targets;
		std::shared_ptr<PreparedScreenFrame> prepared;
		std::unique_ptr<PreparedScreenMipmaps> namedImageMipmaps;
		if (!independent.empty()) {
			targets = *frame;
			targets.targetPasses = independent;
			targets.postprocess.reset();
			targets.drawList = &emptyScreen;
			targets.logicalViewport.reset();
			prepared = impl->screenGPU.Prepare(lease, targets,
				frame->coordinateWidth, frame->coordinateHeight, frame->coordinateWidth, frame->coordinateHeight,
				MTLPixelFormatRGBA8Unorm, shaderValidation, shaderCompileMs);
			namedImageMipmaps = impl->screenGPU.PrepareMipmaps(lease);
		}
		if (prepared || unitStencil || pbrBRDF || (worldInfo && worldInfo->HasUpdates())) {
			auto targetLifetime=[M4OriginalSourceLifetime new];targetLifetime->screen=prepared;targetLifetime->unitStencil=unitStencil;lease.Retain(targetLifetime);
			// Maintenance can inherit a pending image from an interrupted prepare.
			// Register its destination/view/staging with this actual submission.
			for (const auto* cached: impl->pendingMipTextures) impl->RetainTextureAllocations(lease, *cached);
			lease.CommitResources();
			auto command = lease.Begin("Original info, PBR and UI maintenance without window presentation");
			if(prepared)ui::GetRecorder().RecordTargetSubmission(targets.targetPasses,sourceSubmission);
			impl->EncodePendingTextureMipmaps(command,sourceSubmission);
		impl->particlesGPU.EncodeAtlasImages(command,sourceSubmission);
			if(pbrBRDF)pbrBRDF->Encode(command,sourceSubmission);
			if (worldInfo) worldInfo->EncodeDependencies(command);
			if (namedImageMipmaps) namedImageMipmaps->EncodeDependencies(command);
			// The original Genesis mask precedes every later consumer, even without a drawable.
			if(unitStencil)for(size_t n=0;n<unitStencil->PassCount();++n)unitStencil->EncodePass(n,command);
			if (prepared) prepared->EncodeDependencies(command);
			lease.SubmitAndWait(preparationStart, impl->screenTargetSubmission++);
			// Mark the real Genesis output retired before any unrelated cleanup can throw.
			if(unitStencil)unitStencil->DidComplete();
			impl->CompletePendingTextureMipmaps(sourceSubmission);
			impl->particlesGPU.CompleteAtlasImages(sourceSubmission);
			if(pbrBRDF)pbrBRDF->DidComplete(sourceSubmission);
			// Producer declaration compaction occurs at the next main capture boundary.
			if (prepared) prepared->DidComplete();
			if (namedImageMipmaps) namedImageMipmaps->DidComplete();
			result.submitted = true;
			result.screenTargetsCompleted = prepared != nullptr;
		}
		if (worldInfo) {
			// No-work tokens reuse a previously retired global/metal-info image.
			// A plan with pending work reaches here only after the shared submit.
			worldInfo->DidComplete();
			result.completedWorldInfo = {worldInfo->MapIdentity(), worldInfo->CompletionSerial()};
		}
		return result;
	}
}

namespace {
struct ShieldAllocationBacking final : NativeTextureBacking {
	__strong id<MTLTexture> texture=nil;
	NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::Other;}
};
WorldCamera ShieldAllocationCamera(uint32_t width,uint32_t height)
{
	WorldCamera camera;camera.view=camera.projection={};for(unsigned i:{0u,5u,10u,15u})camera.view[i]=camera.projection[i]=1;
	camera.viewport={0,0,int32_t(width),int32_t(height)};camera.clipDepth=ClipDepthConvention::MinusOneToOne;return camera;
}
ScreenSampler ShieldScreenSampler(const NativeTextureSampling& p)
{
	Require(p.compareMode==0&&p.baseLevel==0&&p.lodBias==0&&p.minLod<=0&&p.maxLod>=1000&&p.maxLevel>=1000,"Original Shield color view requires exact comparison/base/LOD state");
	Require((p.minFilter==0x2600||p.minFilter==0x2601||(p.minFilter>=0x2700&&p.minFilter<=0x2703))&&(p.magFilter==0x2600||p.magFilter==0x2601),"Original Shield retained color sampler filter differs");
	Require(p.anisotropy>=1&&p.anisotropy<=16&&std::floor(p.anisotropy)==p.anisotropy,"Original Shield retained color anisotropy requires exact native state");
	Require(p.borderColor==std::array<float,4>{0,0,0,0}||p.borderColor==std::array<float,4>{0,0,0,1}||p.borderColor==std::array<float,4>{1,1,1,1},"Original Shield retained color custom border requires its actual consumer");
	ScreenSampler sampler;
	sampler.filter=p.minFilter==0x2600||p.minFilter==0x2700||p.minFilter==0x2702?ScreenFilter::Nearest:ScreenFilter::Linear;
	sampler.mipFilter=p.minFilter==0x2600||p.minFilter==0x2601?ScreenMipFilter::None:p.minFilter==0x2700||p.minFilter==0x2701?ScreenMipFilter::Nearest:ScreenMipFilter::Linear;
	sampler.magnificationFilter=p.magFilter==0x2600?ScreenFilter::Nearest:ScreenFilter::Linear;
	const auto wrap=[](unsigned v){switch(v){case 0x2901:return ScreenWrap::Repeat;case 0x812F:return ScreenWrap::ClampEdge;case 0x812D:return ScreenWrap::TransparentBorder;case 0x2900:return ScreenWrap::LegacyClamp;default:throw std::runtime_error("Shield color sampler needs its original wrap consumer");}};
	sampler.wrapS=wrap(p.wrapS);sampler.wrapT=wrap(p.wrapT);sampler.anisotropy=unsigned(p.anisotropy);sampler.whiteBorder=p.borderColor==std::array<float,4>{1,1,1,1};return sampler;

}
// The effect shaders apply depth mode/swizzle once. Resolve raw D32F here,
// retaining the exact immutable sampler and actual source producer.
ResolvedSSAOImage ResolvePostprocessDepth(RuntimeLease& lease,
 const NativeCUSTextureBinding& binding,uint64_t submission)
{
 Require(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object,
  "Deferred effect depth binding was not observed");
 auto backing=std::dynamic_pointer_cast<const NativeShieldDepthBacking>(binding.object->backing);
 Require(bool(backing),"Deferred effect requires actual map/model depth backing");
 const auto image=GetShieldDepthBackingImage(*backing);
 const auto role=backing->Kind()==NativeTextureBackingKind::MapDepth?ShieldDepthRole::TerrainOnly:ShieldDepthRole::ModelsOnly;
 Require(image.texture&&image.storageOwner&&image.producer&&image.role==role&&image.producer->role==role,
  "Deferred effect depth cannot substitute a scene/shadow producer");
 Require(image.texture.textureType==MTLTextureType2D&&image.texture.pixelFormat==MTLPixelFormatDepth32Float&&image.texture.sampleCount==1,
  "Deferred effect requires actual single-sample D32F storage");
 const auto& s=binding.object->sampling;
 Require(s.compareMode==0&&s.baseLevel==0&&s.maxLevel>=0&&s.minLod<=0&&s.maxLod>=0&&s.lodBias==0&&s.anisotropy==1&&
  (s.minFilter==0x2600||s.minFilter==0x2601)&&(s.magFilter==0x2600||s.magFilter==0x2601),
  "Deferred effect depth requires its observed non-mip sampler state");
 auto address=[](unsigned mode){switch(mode){
  case 0x2901:return MTLSamplerAddressModeRepeat;
  case 0x8370:return MTLSamplerAddressModeMirrorRepeat;
  case 0x812F:return MTLSamplerAddressModeClampToEdge;
  case 0x812D:return MTLSamplerAddressModeClampToBorderColor;
  default:throw std::runtime_error("Deferred effect depth wrap needs its exact sampler implementation");
 }};
 Require(s.borderColor==std::array<float,4>{0,0,0,0}||s.borderColor==std::array<float,4>{0,0,0,1}||s.borderColor==std::array<float,4>{1,1,1,1},
  "Deferred effect depth border is not representable by this sampler");
 auto d=[MTLSamplerDescriptor new];d.supportArgumentBuffers=YES;
 d.minFilter=s.minFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
 d.magFilter=s.magFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
 d.mipFilter=MTLSamplerMipFilterNotMipmapped;
 d.sAddressMode=address(s.wrapS);d.tAddressMode=address(s.wrapT);d.rAddressMode=address(s.wrapR);
 d.borderColor=s.borderColor[3]==0?MTLSamplerBorderColorTransparentBlack:s.borderColor[0]==1?MTLSamplerBorderColorOpaqueWhite:MTLSamplerBorderColorOpaqueBlack;
 id<MTLSamplerState> sampler=[lease.Device()newSamplerStateWithDescriptor:d];
 Require(sampler!=nil,"Cannot allocate deferred effect's retained depth sampler");
 lease.AddAllocation(image.texture);lease.Retain(sampler);
 return {image.texture,sampler,backing,[image,submission]{
  const auto& p=*image.producer;
  Require(IsNativeShieldDepthReadable(p.encoded,p.completed,p.operation==ShieldDepthOperation::AllocationOnly,p.allocationVerified,p.submissionIdentity,submission),
   "Deferred effect depth has not encoded in this shared submission or retired earlier");
 },true,{image.producer,image.producer->operation==ShieldDepthOperation::AllocationOnly}};
}
}
bool Metal4WorldRenderer::CheckShieldGeometryAllocation(const NativeGeometryTextureRequest& request,NativeTextureRegistry& registry,uint64_t sourceFrame)
{
	if(!request.width||!request.height||request.target!=NativeCUSTextureTarget::Image2D||request.samples!=1)return false;
	RuntimeLease lease;
	if(request.width>lease.Capabilities().maxTextureDimension2D||request.height>lease.Capabilities().maxTextureDimension2D)return false;
	std::array<std::shared_ptr<const NativeTextureBacking>,7> allocations;
	Require(!request.combinedColor||request.role==NativeGeometryTextureRole::Map,"Combined color allocation belongs to a non-map owner");
	const unsigned attachmentCount=request.combinedColor?7:6;
	for(unsigned i=0;i<attachmentCount;++i){
		const auto& object=i==6?request.combinedColor:request.attachments[i];Require(object&&object->target==request.target&&object->allocation.width==request.width&&object->allocation.height==request.height,"Original GBuffer allocation request differs from source objects");
		if(i==5){
			const auto camera=ShieldAllocationCamera(request.width,request.height);
			ShieldDepthImage image;
			if(request.role==NativeGeometryTextureRole::Map){NativeMapDepthSelection selection;selection.requestedDeferred=false;selection.namedDepthStorageExists=true;selection.storageWidth=request.width;selection.storageHeight=request.height;
				auto prepared=impl->mapDepthGPU.Prepare(lease,sourceFrame,camera,request.width,request.height,request.width,request.height,selection,nullptr,nullptr,impl->library,NativeDiagnosticInstrumentation,impl->shieldCompileMs);image=prepared->Image();
			}else{ModelDepthSource source{{},true,request.width,request.height,nullptr};
				auto prepared=impl->modelDepthGPU.Prepare(lease,sourceFrame,camera,request.width,request.height,request.width,request.height,{},impl->library,NativeDiagnosticInstrumentation,impl->shieldCompileMs,&source);image=prepared->Image();}
			NativeShieldDepthMetadata metadata{camera.view,camera.projection,camera.viewport,request.width,request.height};
			auto backing=std::make_shared<NativeShieldDepthBacking>(request.role==NativeGeometryTextureRole::Map?NativeTextureBackingKind::MapDepth:NativeTextureBackingKind::ModelDepth,object->identity,object->version+1,sourceFrame,NativeTextureContent::AllocationOnly,metadata);
			FulfillShieldDepthBacking(*backing,image);allocations[i]=std::move(backing);
		}else{
			auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:request.width height:request.height mipmapped:NO];descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
			auto backing=std::make_shared<ShieldAllocationBacking>();backing->texture=[lease.Device()newTextureWithDescriptor:descriptor];if(!backing->texture)return false;
			backing->storageGeneration=object->identity;backing->contentVersion=object->version+1;backing->sourceDrawFrame=sourceFrame;backing->content=NativeTextureContent::AllocationOnly;
   {
    // A first masked/scissored clear also preserves allocated model pixels.
    // Retain the actual typed allocation; never invent cleared contents.
    const auto camera=ShieldAllocationCamera(request.width,request.height);
    NativeDeferredColorMetadata metadata{request.role,i,{camera.view,camera.projection,camera.viewport,request.width,request.height}};
    auto actual=std::make_shared<NativeDeferredColorBacking>(object->identity,object->version+1,sourceFrame,NativeTextureContent::AllocationOnly,metadata);
    auto producer=CreateShieldDepthProducer(request.role==NativeGeometryTextureRole::Map?ShieldDepthRole::TerrainOnly:ShieldDepthRole::ModelsOnly,sourceFrame);producer->operation=ShieldDepthOperation::AllocationOnly;producer->allocationVerified=true;
    FulfillDeferredColorBacking(*actual,{backing->texture,backing,producer,metadata});allocations[i]=std::move(actual);
   }
		}
	}
	for(unsigned i=0;i<attachmentCount;++i)registry.PublishContent((i==6?request.combinedColor:request.attachments[i])->name,{},allocations[i]);
	return true; // Actual six attachments exist; this does not certify their rendered color contents.
}
NativePBRDeclarationReceipt Metal4WorldRenderer::CapturePBRSource()
{
 MainThread();return impl?impl->pbrSource.CapturePending():NativePBRDeclarationReceipt{};
}
bool Metal4WorldRenderer::HasPendingPBRSource(const NativePBRDeclarationReceipt& batch) const
{
 return impl&&HasPendingNativePBR(batch,NativePBRKind::BRDF);
}
bool Metal4WorldRenderer::HasPendingEnvironmentSource(const NativeEnvironmentStageReceipt& stage,const NativePBRDeclarationReceipt& batch) const
{
	if(!impl)return false;
	if(HasPendingNativePBR(batch,NativePBRKind::Environment))return true;
	return stage.HasPendingCubeSource();
}
std::shared_ptr<const void> Metal4WorldRenderer::AllocateCoverageTarget(const std::shared_ptr<const ScreenTexture>& color,
 const std::shared_ptr<const ScreenTexture>& depth)
{
 MainThread();Require(impl && color && depth && color->format==ScreenTextureFormat::R8Unorm && depth->format==ScreenTextureFormat::Depth16Unorm &&
  color->width==depth->width && color->height==depth->height,"Coverage FBO allocation metadata differs from original R8+D16");
 auto c=impl->screenGPU.ReserveAttachmentAllocationForCapture(color),d=impl->screenGPU.ReserveAttachmentAllocationForCapture(depth);
 Require(c && d && c.pixelFormat==MTLPixelFormatR8Unorm && d.pixelFormat==MTLPixelFormatDepth16Unorm &&
  c.width==d.width && c.height==d.height && c.sampleCount==1 && d.sampleCount==1 &&
  (c.usage&MTLTextureUsageRenderTarget) && (d.usage&MTLTextureUsageRenderTarget),"Coverage FBO has no actual native attachment pair");
 struct Owner {std::shared_ptr<const ScreenTexture> colorSource,depthSource;__strong id<MTLTexture> color=nil,depth=nil;};
 return std::make_shared<Owner>(Owner{color,depth,c,d});
 // No command Begin/submit, clear, generation or contents proof here.
}
std::shared_ptr<const NativeBlockoutTarget> Metal4WorldRenderer::AllocateBlockoutTarget(uint32_t width,uint32_t height)
{
 MainThread();Require(bool(impl),"Blockout FBO has no actual renderer owner");
 return ReserveNativeBlockoutTargetForCapture(width,height);
}

void Metal4WorldRenderer::ConnectPBRSourceContext(bool connected)
{
	auto& context=OriginalNativeTextureContext();
	if(!connected){impl->pbrSource.Shutdown();OriginalNativeCubeTextures().Detach();return;}
	NativeCubeTextureServices cubeServices;
	cubeServices.sourceDrawFrame=[]{return uint64_t(globalRendering->drawFrame);};
	cubeServices.allocate=[](NativeCubeTextureKind kind,uint32_t size,uint32_t levels){return ReserveNativeCubeTextureForCapture(kind,size,levels);};
	OriginalNativeCubeTextures().Attach(context.Registry(),context.History(),std::move(cubeServices));
	try {
		impl->pbrSource.Initialize(context,[](lua_State*,const std::string& name)->NativeTextureLookup {
			Require(name=="$reflection","Original PBR source requested an unconnected texture owner");
			return {true,OriginalNativeTextureContext().Registry().Find(cubeMapHandler.GetEnvReflectionTextureID()),false};
		},[]{return OriginalNativeCubeTextures().Attached();},[]{return uint64_t(globalRendering->drawFrame);},[]{
			const auto& draw=ui::GetRecorder().CurrentState().draw;
			Require(!draw.blend.enabled&&!draw.scissor&&draw.colorWrite==std::array<bool,4>{true,true,true,true}&&!draw.alphaTest.enabled,
				"Original PBR full-target raster state differs from its source producer");
			// The original PBR FBO has no depth/stencil attachment. Those
			// inherited tests therefore do not reject fragments.
		},NativeDiagnosticInstrumentation);
	} catch (...) {OriginalNativeCubeTextures().Detach();throw;}
}

void Metal4WorldRenderer::ConnectShieldTextureContext(bool connected)
{
	impl->screenGPU.SetPiPSourceImageProvider(connected?PiPSourceImageProvider(
		[owner=impl.get()](RuntimeLease& lease,const NativeCUSTextureBinding& binding,const ScreenSampler& sampling,bool validation,double& milliseconds){
			Require(binding.object!=nullptr,"Typed source image has no original binding");
   if(std::dynamic_pointer_cast<const NativeShadowTextureBacking>(binding.object->backing)){
    auto image=ResolveNativeShadowTexture(lease,binding,owner->pbrSubmission);
    return PreparedPiPSourceImage{image.texture,image.sampler,image.owner,image.flipV,std::move(image.encodeDependencies),std::move(image.requireReadable),std::move(image.didComplete)};
   }
   Require(bool(std::dynamic_pointer_cast<const NativeTerrainInfoBacking>(binding.object->backing)),"Typed source image is outside its actual shadow/$info families");
			auto sampler=owner->screenGPU.PrepareSampler(lease,sampling);
			auto image=ResolveNativeTerrainInfoImage(lease,binding,owner->pbrSubmission,sampler,validation,milliseconds);
			return PreparedPiPSourceImage{image.texture,image.sampler,image.owner,image.flipV,
				std::move(image.encodeDependencies),std::move(image.requireReadable),std::move(image.didComplete)};
		}):PiPSourceImageProvider{});
 impl->screenGPU.SetSensorLOSProvider(connected?metal4::ScreenGPU::SensorLOSProvider(
  [owner=impl.get()](RuntimeLease& lease,const NativeSensorLOSDraw& draw,const SensorPreviewTarget& target,const PreparedScreenFrame* producers,bool validation,double& ms,NativePostprocessConsumer consumer){
   WorldCamera camera;camera.viewport=target.cameraViewport;
   auto resolver=[&](RuntimeLease& l,const NativeCUSTextureBinding& b,std::optional<NativePostprocessConsumer> at){return owner->screenGPU.ResolveStartBoxImage(l,b,producers,at);};
   return owner->sensorLOSGPU.Prepare(lease,draw,resolver,camera,target.width,target.height,target.logicalWidth,target.logicalHeight,target.color,target.depth,target.stencil,target.samples,validation,ms,consumer);
  }):metal4::ScreenGPU::SensorLOSProvider{});
	impl->screenGPU.SetSensorPreviewProvider(connected?metal4::ScreenGPU::SensorPreviewProvider(
		[owner=impl.get()](RuntimeLease& lease,const NativeSensorPreviewDraw& draw,const SensorPreviewTarget& target,
			const PreparedScreenFrame* producers,bool validation,double& milliseconds,NativePostprocessConsumer consumer){
			Require(draw.callin==NativeSensorPreviewCallin::Minimap,"Preview UI marker is outside its actual minimap callback");
			NativeSensorPreviewCommand command;command.kind=NativeSensorPreviewCommandKind::Draw;
			command.sequence=draw.sequence;command.sourceDrawFrame=draw.sourceDrawFrame;command.callin=draw.callin;
			command.draw=std::make_shared<const NativeSensorPreviewDraw>(draw);command.surface=draw.surface;command.raster=draw.raster;
			auto images=[&](RuntimeLease& sourceLease,const NativeSensorPreviewImageInput& input,bool contents,NativePostprocessConsumer at){
				return owner->SensorPreviewImage(sourceLease,input,contents,at,producers,validation,milliseconds);
			};
			return owner->sensorPreviewGPU.Prepare(lease,command,target,images,owner->screenGPU,validation,milliseconds,consumer);
		}):metal4::ScreenGPU::SensorPreviewProvider{});

	impl->screenGPU.SetStartBoxImageProvider(connected?metal4::ScreenGPU::StartBoxImageProvider([this](RuntimeLease& lease,const NativeCUSTextureBinding& binding,const PreparedScreenFrame* producers,std::optional<NativePostprocessConsumer> consumer){
		Require(binding.object!=nullptr,"Original StartBox resolver has no actual object");
		const auto kind=ClassifyNativeStartBoxImage(binding);
		if(auto backing=std::dynamic_pointer_cast<const NativeMapTextureBacking>(binding.object->backing)){
			auto resource=std::dynamic_pointer_cast<const Impl::MapImage>(backing->Resource());
			Require(resource&&resource->texture,"Original map image has no paired existing producer storage");
			auto sampler=impl->screenGPU.PrepareSampler(lease,ui::NativeUITextureHistory::DescribeSampler(binding));
			if(backing->role==NativeMapTextureRole::Height)
				return PreparedStartBoxImage{kind,resource->texture,sampler,binding.object,resource,false,[resource]{Require(resource->Completed(),"Original height sampled before its CPU upload completed");}};
			const auto encoded=impl->normalsEncoded;
			return PreparedStartBoxImage{kind,resource->texture,sampler,binding.object,resource,false,[encoded]{Require(*encoded,"Original map normal image sampled before its existing producer");}};
		}
		const auto source=OriginalNativeTextureContext().UI().DescribeBinding(binding);
		Require(source.image!=nullptr,"Original StartBox binding has no actual retained image");
		if(source.image->renderTarget && (source.image->format==ScreenTextureFormat::RGBA16Float || source.image->format==ScreenTextureFormat::R8Unorm))
			Require(source.image->generation,"Original StartBox SDF cannot sample allocation-only unbaked contents");
		auto external=std::make_shared<PreparedScreenExternalImage>();
		auto prepared=impl->screenGPU.PrepareImage(lease,source.image,source.sampler,producers,consumer,[external](PreparedScreenExternalImage v){*external=std::move(v);});
		return PreparedStartBoxImage{kind,prepared.texture,prepared.sampler,binding.object,source.image,source.image->flipV,[external]{if(external->requireReadable)external->requireReadable();}};
	}):metal4::ScreenGPU::StartBoxImageProvider{});

	SetNativeShieldConsumerAvailable(false);
	auto& context=OriginalNativeTextureContext();
	if(!connected){if(context.Initialized())context.UI().SetExternalImageDescriptorProvider({});impl->screenGPU.SetExternalImageProvider({});return;}
	context.UI().SetExternalImageDescriptorProvider([](const NativeCUSTextureBinding& binding)->std::shared_ptr<const ScreenTexture>{
		if(auto mask=std::dynamic_pointer_cast<const NativeUnitStencilBacking>(binding.object->backing)){
			Require(mask->version&&mask->version->image&&mask->version->producer,"Original Unit Stencil descriptor lost its retained R8 producer");
			const auto& version=*mask->version;
			auto image=std::make_shared<ScreenTexture>();image->sourceName=binding.object->source;image->generation=binding.object->version;
			// External color retains the real R8 allocation without selecting the UI font-coverage material.
			image->format=ScreenTextureFormat::ExternalColor;image->width=version.image->width;image->height=version.image->height;
			image->sourceTexture=binding.object;image->flipV=true;
			image->sourceDependent=!version.producer->allocationOnly&&!version.producer->completed;return image;
		}
		if(auto height=std::dynamic_pointer_cast<const NativeMapTextureBacking>(binding.object->backing);height&&height->role==NativeMapTextureRole::Height){
			const auto& object=*binding.object;const auto& a=object.allocation;
			Require(!object.image&&object.target==NativeCUSTextureTarget::Image2D&&height->heights&&height->originalRevision&&
				height->storageGeneration==object.identity&&height->contentVersion&&height->content==NativeTextureContent::ProducerVersion&&
				a.width==height->heights->width&&a.height==height->heights->height&&a.depth==1&&a.levels==1&&a.samples==1&&a.originalInternalFormat==0x822E,
				"Original UI height descriptor differs from its retained R32F map publication");
			auto image=std::make_shared<ScreenTexture>();image->sourceName=object.source;image->generation=object.version;
			image->format=ScreenTextureFormat::R32Float;image->width=a.width;image->height=a.height;image->sourceTexture=binding.object;
			image->flipV=false;image->sourceDependent=!height->Resource()||!height->Resource()->Completed();return image;
		}
		if(binding.object->image&&!binding.object->backing){
			const auto& source=PublishedWorldTexture(*binding.object);
			auto image=std::make_shared<ScreenTexture>();image->sourceName=binding.object->source;image->generation=binding.object->version;
			image->format=ScreenTextureFormat::ExternalColor;image->width=binding.object->allocation.width;image->height=binding.object->allocation.height;
			image->sourceTexture=binding.object;image->flipV=source.flipV;image->sourceDependent=false;return image;
		}
		if(auto atlas=std::dynamic_pointer_cast<const NativeParticleAtlasBacking>(binding.object->backing)){
			Require(atlas->atlas&&binding.object->target==NativeCUSTextureTarget::Image2D,"Original UI atlas input is not its actual single-page2D source");
			auto image=std::make_shared<ScreenTexture>();image->sourceName=binding.object->source;image->generation=binding.object->version;
			image->format=ScreenTextureFormat::ExternalColor;image->width=binding.object->allocation.width;image->height=binding.object->allocation.height;
			image->sourceTexture=binding.object;image->sourceDependent=false;return image;
		}
		if (auto model = std::dynamic_pointer_cast<const NativeModelTextureBacking>(binding.object->backing)) {
			Require(model->pixels && !model->pixels->mips.empty(), "Original S3O binding has no actual decoded source");
			auto image = std::make_shared<ScreenTexture>();
			image->sourceName = binding.object->source; image->generation = binding.object->version;
			image->format = ScreenTextureFormat::ExternalColor;
			image->width = binding.object->allocation.width; image->height = binding.object->allocation.height;
			image->sourceTexture = binding.object; image->flipV = model->pixels->flipV;
			// Source decode and queued mip generation are camera-independent.
			// Their real resource readiness is checked by the encode-time owner.
			image->sourceDependent = false;
			return image;
		}
		if(auto pbr=GetNativePBRImage(binding)){
			auto image=std::make_shared<ScreenTexture>();image->sourceName=binding.object->source;image->generation=binding.object->version;
			image->format=pbr->allocation.originalFormat==0x822F?ScreenTextureFormat::RG16Float:ScreenTextureFormat::RGBA16Float;
			image->width=pbr->allocation.width;image->height=pbr->allocation.height;image->sourceTexture=binding.object;image->flipV=false;
			image->sourceDependent=pbr->content==NativeTextureContent::ProducerVersion&&!pbr->completed.load(std::memory_order_acquire);return image;
		}
		if(auto backing=std::dynamic_pointer_cast<const NativeBloomImageBacking>(binding.object?binding.object->backing:nullptr)){
   Require(backing->version&&backing->version->image,"Original Bloom binding has no immutable HDR image");auto image=std::make_shared<ScreenTexture>();image->sourceName=binding.object->source;image->generation=binding.object->version;switch(backing->version->image->format){
    case NativeBloomFormat::RG11B10Float:image->format=ScreenTextureFormat::RG11B10Float;break;
    case NativeBloomFormat::RGBA16Float:image->format=ScreenTextureFormat::RGBA16Float;break;
    default:throw std::runtime_error("Original Bloom descriptor has an unsupported captured HDR format");
   }
   image->width=backing->version->image->width;image->height=backing->version->image->height;image->flipV=true;image->sourceTexture=binding.object;image->sourceDependent=!backing->version->producer->completed;return image;
  }
  if(auto backing=std::dynamic_pointer_cast<const NativeSSAOImageBacking>(binding.object?binding.object->backing:nullptr)){
   Require(backing->version&&backing->version->image,"Original SSAO binding has no immutable image generation");
   auto image=std::make_shared<ScreenTexture>();image->sourceName=binding.object->source;image->generation=binding.object->version;
   image->format=backing->version->image->format==NativeSSAOFormat::RGBA8?ScreenTextureFormat::RGBA8Unorm:ScreenTextureFormat::RGBA16Float;
   image->width=backing->version->image->width;image->height=backing->version->image->height;image->flipV=true;image->sourceTexture=binding.object;
   image->sourceDependent=!backing->version->producer->completed;return image;
  }
  if(auto backing=std::dynamic_pointer_cast<const NativeDeferredColorBacking>(binding.object?binding.object->backing:nullptr)){
   auto image=std::make_shared<ScreenTexture>();image->sourceName=binding.object->source;image->generation=binding.object->version;
   image->format=ScreenTextureFormat::RGBA8Unorm;image->width=backing->Metadata().view.width;image->height=backing->Metadata().view.height;
   image->flipV=true;image->sourceTexture=binding.object;
   image->sourceDependent=backing->content==NativeTextureContent::ProducerVersion&&(!backing->Resource()||!GetDeferredColorBackingImage(*backing).producer->completed);return image;
  }
  Require(ClassifyNativeShieldTexture(binding)==NativeShieldTextureKind::Depth,"Original UI external image needs its actual typed resource provider");
		auto backing=std::dynamic_pointer_cast<const NativeShieldDepthBacking>(binding.object->backing);Require(backing!=nullptr,"Original UI named depth backing type differs");
		auto image=std::make_shared<ScreenTexture>();image->sourceName=binding.object->source;image->generation=binding.object->version;image->format=ScreenTextureFormat::Depth32Float;
		image->width=backing->Metadata().width;image->height=backing->Metadata().height;image->flipV=true;image->sourceTexture=binding.object;
		image->sourceDependent=backing->content==NativeTextureContent::ProducerVersion&&(!backing->Resource()||!GetShieldDepthBackingImage(*backing).producer->completed);
		return image;
	});
	impl->screenGPU.SetExternalImageProvider([owner=impl.get()](RuntimeLease& lease,const std::shared_ptr<const NativeCUSTextureObject>& object,std::optional<NativePostprocessConsumer>)->PreparedScreenExternalImage{
		Require(object!=nullptr,"Original external depth consumer has no immutable object");
		if(auto mask=std::dynamic_pointer_cast<const NativeUnitStencilBacking>(object->backing)){
			auto image=owner->unitStencilGPU.Resolve(lease,mask->version,owner->pbrSubmission);
			return {image.texture,image.owner,image.requireReadable};
		}
		if(auto height=std::dynamic_pointer_cast<const NativeMapTextureBacking>(object->backing);height&&height->role==NativeMapTextureRole::Height){
			if(!height->Resource()){
				// Exact captured CPU corner image: the same owner/upload used by
				// the world path, without executing any later world producer.
				auto upload=owner->standardTerrainGPU->PrepareHeight(lease,height->heights);
				auto actual=std::make_shared<Impl::MapImage>();actual->heightOwner=upload;
				actual->texture=upload->texture;actual->completed=true;height->Fulfill(std::move(actual));
			}
			auto resource=std::dynamic_pointer_cast<const Impl::MapImage>(height->Resource());
			Require(resource&&resource->heightOwner&&resource->heightOwner->source==height->heights&&resource->texture==resource->heightOwner->texture,
				"Original UI height lost its immutable map upload owner");
			auto image=resource->texture;const auto& a=object->allocation;
			Require(image&&image.textureType==MTLTextureType2D&&image.pixelFormat==MTLPixelFormatR32Float&&image.sampleCount==1&&image.mipmapLevelCount==1&&
				image.width==a.width&&image.height==a.height&&(image.usage&MTLTextureUsageShaderRead),
				"Original UI height resource differs from its actual R32F allocation");
			const auto& channels=object->sampling.swizzle;
			if(channels!=std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906}){
				Require(channels==std::array<unsigned,4>{0x1903,0x1903,0x1903,0x1903}&&(image.usage&MTLTextureUsagePixelFormatView),
					"Original UI height requires its actual RRRR channel view");
				lease.AddAllocation(image);
				if(resource->heightRRRR==nil || resource->heightRRRRSource!=image || resource->heightRRRR.device!=lease.Device()) {
					auto view=[image newTextureViewWithPixelFormat:MTLPixelFormatR32Float textureType:MTLTextureType2D levels:NSMakeRange(0,1) slices:NSMakeRange(0,1)
						swizzle:MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed,MTLTextureSwizzleRed,MTLTextureSwizzleRed,MTLTextureSwizzleRed)];
					Require(view!=nil,"Cannot create original UI height RRRR view");
					resource->heightRRRRSource=image;resource->heightRRRR=view;
				}
				image=resource->heightRRRR;
			}
			return {image,object,[resource]{Require(resource->Completed(),"Original UI height sampled before its retained upload completed");}};
		}
		if(object->image&&!object->backing){
			PublishedWorldTexture(*object);auto image=owner->Texture(lease,object->image);
			const auto status=owner->textures.at(object->image.get()).status;const auto submission=owner->pbrSubmission;
			return {image,object,[status,submission]{Require(status->ready||(submission&&status->encodedSubmission==submission),
				"Original UI world image sampled before its common upload/mip producer");}};
		}
		if(std::dynamic_pointer_cast<const NativeParticleAtlasBacking>(object->backing)){
			NativeCUSTextureBinding binding;binding.knowledge=NativeCUSBindingKnowledge::BoundObject;binding.object=object;
			Require(object->target==NativeCUSTextureTarget::Image2D,"Original UI cannot substitute an array atlas for sampler2D");
			auto image=owner->particlesGPU.AtlasImage(lease,binding,owner->screenGPU.PrepareSampler(lease,ui::NativeUITextureHistory::DescribeSampler(binding)),owner->pbrSubmission);
			return {image.texture,image.owner,image.requireReadable};
		}
		if (auto model = std::dynamic_pointer_cast<const NativeModelTextureBacking>(object->backing)) {
			auto image = owner->Texture(lease,model->pixels,object.get());
			const auto status = owner->textures.at(model->pixels.get()).status;
			const auto submission = owner->pbrSubmission;
			return {image,model,[status,submission] {
				Require(status->ready || (submission && status->encodedSubmission == submission),
					"Original S3O mip producer has not executed before this consumer");
			}};
		}
		if(std::dynamic_pointer_cast<const NativePBRImage>(object->backing)){
			NativeCUSTextureBinding binding;binding.knowledge=NativeCUSBindingKnowledge::BoundObject;binding.object=object;
			const auto submission=owner->pbrSubmission;
			auto image=owner->pbrSource.Image(lease,binding,submission);return {image.texture,image.owner,image.requireReadable};
		}
  const uint64_t actualSubmission=owner->shieldPending?owner->shieldPending->submissionIdentity:0;
  if(auto backing=std::dynamic_pointer_cast<const NativeBloomImageBacking>(object->backing)){
   auto image=owner->bloomGPU.Resolve(lease,backing->version,actualSubmission);return{image.texture,image.owner,image.requireReadable};
  }
  if(auto backing=std::dynamic_pointer_cast<const NativeSSAOImageBacking>(object->backing)){
   auto image=owner->ssaoGPU.Resolve(lease,backing->version,actualSubmission);
   return{image.texture,image.owner,image.requireReadable};
  }
  if(std::dynamic_pointer_cast<const NativeDeferredColorBacking>(object->backing)){
   NativeCUSTextureBinding binding;binding.knowledge=NativeCUSBindingKnowledge::BoundObject;binding.object=object;
   auto image=ResolveDeferredColorImage(lease,binding,actualSubmission);return{image.texture,image.owner,image.requireReadable};
  }
		auto backing=std::dynamic_pointer_cast<const NativeShieldDepthBacking>(object->backing);Require(backing!=nullptr,"Original external depth consumer has no actual map/model backing");
		const auto image=GetShieldDepthBackingImage(*backing);const uint64_t submission=owner->shieldPending?owner->shieldPending->submissionIdentity:0;
		return{image.texture,backing,[image,submission]{
			Require(image.producer&&IsNativeShieldDepthReadable(image.producer->encoded,image.producer->completed,image.producer->operation==ShieldDepthOperation::AllocationOnly,image.producer->allocationVerified,image.producer->submissionIdentity,submission)&&
				(image.producer->completed||image.producer->operation==ShieldDepthOperation::AllocationOnly||submission!=0),"Original UI depth version was not produced in this actual submission or retired earlier");
		}};
	});
	SetNativeShieldConsumerAvailable(true);
}

void Metal4WorldRenderer::ConstructOriginalModelPrograms(NativeEngineModelProgramSource& source)
{
	MainThread();Require(impl!=nullptr,"Original model construction has no actual renderer");
	RuntimeLease lease;auto* layer=Metal4WindowSurfaceNativeAccess::Layer(*impl->surface);
	Require(layer&&layer.device==lease.Device(),"Original model source belongs to another actual device");
	double milliseconds=0;CUSSourceTarget forward,mrt;
	forward.samples=impl->mainSampling.samples;forward.colors[0]=layer.pixelFormat;forward.depth=MTLPixelFormatDepth32Float;forward.stencil=MTLPixelFormatStencil8;
	mrt.colors.fill(MTLPixelFormatRGBA8Unorm);mrt.depth=MTLPixelFormatDepth32Float;
	for(bool modern:{false,true})for(bool shadows:{false,true}){
		source.Construct(modern,shadows,false,impl->originalForwardGPU.CompileOriginalProgram(lease,modern,shadows,forward,NativeDiagnosticInstrumentation,milliseconds));
		source.Construct(modern,shadows,true,impl->originalDeferredGPU.CompileOriginalProgram(lease,modern,shadows,mrt,NativeDiagnosticInstrumentation,milliseconds));
	}
}
void Metal4WorldRenderer::SetOriginalShadowCapture(std::function<void(std::shared_ptr<const NativeShadowTextureFrame>)> capture)
{
 MainThread();Require(impl!=nullptr,"Original shadow source has no actual renderer");
 auto& source=OriginalNativeShadowTextures();
 if(!capture){source.Detach();return;}
 auto& context=OriginalNativeTextureContext();NativeShadowTextureServices services;
 // Allocation is needed before ShadowHandler::Init and Lua program creation.
 // This structural callback must not recursively depend on final CUS readiness.
 services.ready=[this]{return impl!=nullptr;};
 services.allocate=[](const NativeShadowTextureRequest& request){return ReserveNativeShadowTextureForCapture(request);};
 services.checkFramebuffer=[](const NativeShadowTextureBacking& depth,const NativeShadowTextureBacking& color,bool){return CheckNativeShadowFramebuffer(depth,color);};
 services.sourceDrawFrame=[]{return uint64_t(globalRendering->drawFrame);};
 // Physical native floating-point depth has 32 bits. Original InitFBO keeps
 // its min(bits,24) request; the shadow owner separately reports D32Float
 // storage and preserves fixed-request compare and shadow raster semantics.
 services.depthBufferBits=[]{const auto& runtime=SharedRuntime();Require(runtime.Capabilities().directMetal4 && runtime.Capabilities().unifiedMemory && runtime.Device()!=nil,"Original native shadow capability has no actual device");return 32;};
 services.capture=std::move(capture);source.Attach(context.Registry(),context.History(),std::move(services));
}
void Metal4WorldRenderer::ConstructOriginalShadowPrograms(NativeEngineShadowProgramSource& source,const WorldCamera& drawing)
{
 MainThread();Require(impl!=nullptr,"Original ShadowGen construction has no actual renderer");
 RuntimeLease lease;auto* layer=Metal4WindowSurfaceNativeAccess::Layer(*impl->surface);
 Require(layer&&layer.device==lease.Device(),"Original ShadowGen source belongs to another actual device");
 const auto depth=OriginalNativeShadowTextures().Image(shadowHandler.GetShadowTextureID());
 const auto color=OriginalNativeShadowTextures().Image(shadowHandler.GetColorTextureID());
 Require(depth&&color&&NativeShadowTextureImage(*depth).device==lease.Device()&&NativeShadowTextureImage(*color).device==lease.Device(),"Original ShadowGen construction has no same-device actual named shadow images");
 const auto target=OriginalShadowSourceTarget(*depth,*color,drawing);double milliseconds=0;
 for(bool modern:{false,true})source.Construct(modern,impl->originalShadowGPU.CompileOriginalProgram(lease,modern,target,NativeDiagnosticInstrumentation,milliseconds));
 source.Construct(NativeEngineShadowProfile::Projectile,impl->originalShadowGPU.CompileOriginalProgram(lease,NativeEngineShadowProfile::Projectile,target,NativeDiagnosticInstrumentation,milliseconds));
 source.Construct(NativeEngineShadowProfile::Terrain,impl->originalShadowGPU.CompileOriginalProgram(lease,NativeEngineShadowProfile::Terrain,target,NativeDiagnosticInstrumentation,milliseconds));
}
void Metal4WorldRenderer::ConstructOriginalShadowParticlePrograms(NativeOriginalShadowParticlesProgramSource& source)
{
	MainThread();Require(impl!=nullptr,"Original ProjFX construction has no actual renderer");
	RuntimeLease lease;auto* layer=Metal4WindowSurfaceNativeAccess::Layer(*impl->surface);
	Require(layer&&layer.device==lease.Device(),"Original ProjFX compiler belongs to another actual device");
	// ProjFX stages belong to ProjectileDrawer, not the earlier ShadowHandler.
	// Compile both real variants before publishing either source owner. Actual
	// draw targets select their PSOs later, including after ShadowHandler reload.
	double milliseconds=0;
	const auto texture2D=impl->originalShadowParticlesGPU.CompileOriginalProgram(lease,false,milliseconds);
	const auto textureArray=impl->originalShadowParticlesGPU.CompileOriginalProgram(lease,true,milliseconds);
	source.Construct(false,texture2D);
	source.Construct(true,textureArray);
}
std::shared_ptr<const NativeCUSProgramConsumer> Metal4WorldRenderer::CompileOriginalCUSProgram(const NativeCUSProgramSource& source)
{
 MainThread();Require(impl!=nullptr,"Original CUS construction has no actual renderer");
 RuntimeLease lease;auto* layer=Metal4WindowSurfaceNativeAccess::Layer(*impl->surface);
 Require(layer&&layer.device==lease.Device(),"Original CUS compiler belongs to another actual device");
 // Original gl.CreateShader/link does not bind a framebuffer. Compile the
 // exact full stages now; each real source draw supplies its genuine target.
 double milliseconds=0;return impl->cusSourceGPU.LinkOriginalStages(lease,source,milliseconds);
}
NativeTerrainProgramLink Metal4WorldRenderer::CompileOriginalTerrainProgram(const NativeTerrainProgramRequest& source)
{
 MainThread();Require(impl!=nullptr,"Original terrain construction has no actual renderer");
 RuntimeLease lease;auto* layer=Metal4WindowSurfaceNativeAccess::Layer(*impl->surface);
 Require(layer&&layer.device==lease.Device(),"Original terrain compiler belongs to another actual device");
 // This source route owns the real CBumpWater reflection/refraction targets.
 // A later different destination must obtain its own genuine PSO variant.
 CUSSourceTarget target;target.colors[0]=MTLPixelFormatRGBA8Unorm;
 if(source.identity.selection==NativeTerrainProgramSelection::DeferredAdvanced||source.identity.selection==NativeTerrainProgramSelection::CombinedAdvanced)target.colors.fill(MTLPixelFormatRGBA8Unorm);
 if(source.identity.selection==NativeTerrainProgramSelection::CombinedAdvanced)target.combinedColor=MTLPixelFormatRGBA8Unorm;
 target.depth=MTLPixelFormatDepth32Float;target.stencil=MTLPixelFormatInvalid;target.samples=1;
 double milliseconds=0;
 return impl->terrainSourceGPU.CompileOriginalProgram(lease,source,target,NativeDiagnosticInstrumentation,milliseconds);
}
std::shared_ptr<const NativeDeferredLightProgramConsumer> Metal4WorldRenderer::CompileOriginalDeferredLightProgram(const NativeDeferredLightProgramSource& source)
{
	MainThread();Require(impl!=nullptr,"Original deferred-light compiler has no actual renderer");
	RuntimeLease lease;auto* layer=Metal4WindowSurfaceNativeAccess::Layer(*impl->surface);
	Require(layer&&layer.device==lease.Device(),"Original deferred-light compiler belongs to another device");
	double milliseconds=0;
	return impl->deferredLightsGPU.LinkOriginalStages(lease,source,milliseconds);
}
std::shared_ptr<const NativeUnitStencilProgramConsumer> Metal4WorldRenderer::CompileOriginalUnitStencilProgram(const NativeUnitStencilProgramSource& source)
{
	MainThread();Require(impl!=nullptr,"Original Unit Stencil compiler has no actual renderer");
	RuntimeLease lease;auto* layer=Metal4WindowSurfaceNativeAccess::Layer(*impl->surface);
	Require(layer&&layer.device==lease.Device(),"Original Unit Stencil compiler belongs to another device");
	double milliseconds=0;return impl->unitStencilGPU.LinkOriginalProgram(lease,source,NativeDiagnosticInstrumentation,milliseconds);
}
std::shared_ptr<const NativeOutlineProgramConsumer> Metal4WorldRenderer::CompileOriginalOutlineProgram(const NativeOutlineProgramSource& source)
{
	MainThread();Require(impl!=nullptr,"Original Outline compiler has no actual renderer");
	RuntimeLease lease;auto* layer=Metal4WindowSurfaceNativeAccess::Layer(*impl->surface);
	Require(layer&&layer.device==lease.Device(),"Original Outline compiler belongs to another device");
	double milliseconds=0;return impl->outlineGPU.LinkOriginalStages(lease,source,milliseconds);
}
std::shared_ptr<const NativeDistortionProgramConsumer> Metal4WorldRenderer::CompileOriginalDistortionProgram(const NativeDistortionProgramSource& source)
{
	MainThread();Require(impl!=nullptr,"Original Distortion compiler has no actual renderer");
	RuntimeLease lease;auto* layer=Metal4WindowSurfaceNativeAccess::Layer(*impl->surface);
	Require(layer&&layer.device==lease.Device(),"Original Distortion compiler belongs to another device");
	double milliseconds=0;
	return impl->distortionGPU.LinkOriginalStages(lease,source,milliseconds);
}
void Metal4WorldRenderer::ClearOriginalModelSources()
{
	MainThread();Require(Metal4RendererCanSafelyDetachSurface(),"Original model sources still own unretired GPU work");
	impl->cusSourceGPU.ClearResources();
}

void Metal4WorldRenderer::ConnectBloomDeferredSources(bool connected){MainThread();Require(!connected||impl->bloomGPU.Initialized(),"Bloom source connection requires its genuine initialized consumer");SetNativeBloomConsumerAvailable(connected);}
void Metal4WorldRenderer::ConnectSSAODeferredSources(bool connected)
{
 MainThread();
 // The deferred owner invokes this only after actual original MRT publication
 // and its same-submission encoder are installed. Compiled PSOs alone are insufficient.
 Require(!connected||impl->ssaoGPU.Initialized(),"SSAO source connection requires its genuine initialized consumer");
 SetNativeSSAOConsumerAvailable(connected);
}

void Metal4WorldRenderer::BeginShieldMapPublication(NativeTextureContext& context,uint64_t frame,const WorldCamera& camera,const NativeMapDepthSelection& selection)
{
	Require(!impl->shieldPending,"Original map GBuffer publication was not consumed or aborted");
	auto next=std::make_shared<NativeShieldDepthFrame>();next->sourceDrawFrame=frame;next->submissionIdentity=CreateShieldSubmissionIdentity();next->camera=camera;next->mapSelection=selection;
	// The actual phase published before its callbacks. This shared summary
 // borrows that current version (or the retained previous allocation).
 const auto mapName=context.Geometry(NativeGeometryTextureRole::Map).Name(NativeGeometryTextureNames::DepthAttachment);
 const auto object=context.Registry().Find(mapName);
 const auto backing=object?std::dynamic_pointer_cast<const NativeShieldDepthBacking>(object->backing):nullptr;
 Require(backing&&backing->Kind()==NativeTextureBackingKind::MapDepth,"Retained original map depth has no actual named backing");
 next->map={object,object,backing};
	impl->shieldRegistry=&context.Registry();impl->shieldPending=std::move(next);
}
std::shared_ptr<const NativeShieldDepthFrame> Metal4WorldRenderer::PublishShieldModelDepth(const WorldFrame& frame,const NativeModelDepthSelection& selection)
{
	Require(!frame.opaqueModelFrame,"Standalone model depth cannot replace actual MRT model publications");
	Require(impl->shieldPending&&impl->shieldPending->sourceDrawFrame==frame.sourceDrawFrame,"Original model GBuffer publication lacks this draw's prior map stage");
	Require(impl->shieldPending->camera.view==frame.viewCamera.view&&impl->shieldPending->camera.projection==frame.viewCamera.projection&&impl->shieldPending->camera.viewport==frame.viewCamera.viewport,"Main camera changed between original map and model GBuffer writers");
	auto& context=OriginalNativeTextureContext();auto& output=*impl->shieldPending;
	output.modelPlan=BuildNativeModelDepthPlan(frame,selection);
	output.models=PublishNativeShieldDepth(context.Registry(),context.Geometry(NativeGeometryTextureRole::Models),NativeTextureBackingKind::ModelDepth,frame.sourceDrawFrame,frame.viewCamera,!output.modelPlan.phases.empty());
	return impl->shieldPending;
}
std::shared_ptr<const NativeShieldDepthFrame> Metal4WorldRenderer::BorrowActualMRTModelDepth(const WorldFrame& frame)
{
	MainThread();
	Require(frame.opaqueModelFrame&&frame.opaqueModelFrame->sourceDrawFrame==frame.sourceDrawFrame,
		"Actual MRT depth borrowing requires the retained original opaque source frame");
	Require(impl->shieldPending&&impl->shieldPending->sourceDrawFrame==frame.sourceDrawFrame,
		"Actual MRT depth borrowing lacks this source draw's independent map publication");
	auto& output=*impl->shieldPending;
	Require(output.camera.view==frame.viewCamera.view&&output.camera.projection==frame.viewCamera.projection&&output.camera.viewport==frame.viewCamera.viewport,
		"Actual opaque source and independent map publication use different cameras");
	Require(!output.models.published&&!output.models.Changed(),
		"A standalone model depth publication already owns this source draw");
	auto& context=OriginalNativeTextureContext();
	const auto depthName=context.Geometry(NativeGeometryTextureRole::Models).Name(5);
	Require(depthName!=0,"Actual original model depth5 name is absent");
	const auto object=context.Registry().Find(depthName);
	const auto backing=object?std::dynamic_pointer_cast<const NativeShieldDepthBacking>(object->backing):nullptr;
	Require(backing&&backing->Kind()==NativeTextureBackingKind::ModelDepth,
		"Actual original model depth5 has no retained model-depth backing");
	// The latest selected original phase or genuinely retained prior contents.
	// No publication, binding event, clear or source-coverage claim occurs here.
	output.models={object,object,backing};
	return impl->shieldPending;
}

void Metal4WorldRenderer::AbortShieldPublications(std::shared_ptr<const NativeOpaqueModelFrame> unsubmittedOpaque,
 std::shared_ptr<const NativeOriginalShadowFrame> unsubmittedShadow,
 std::shared_ptr<const NativeTerrainSceneSourceFrame> unsubmittedGround) noexcept
{
	if(impl&&impl->activeSourceSubmission)return; // Recorded work must never roll back.
	if(unsubmittedOpaque)CancelNativeOpaqueModels(OriginalNativeTextureContext().Registry(),*unsubmittedOpaque);
 if(unsubmittedGround&&unsubmittedGround->ground&&unsubmittedGround->ground->mapMRT)
  CancelNativeCUSDeferredPhase(OriginalNativeTextureContext().Registry(),*unsubmittedGround->ground->mapMRT);
	if(impl&&impl->shieldPending&&impl->shieldRegistry)CancelNativeShieldDepth(*impl->shieldRegistry,*impl->shieldPending);
	if(OriginalNativeCubeTextures().Attached()){
  const auto update=OriginalNativeCubeTextures().CurrentReflectionUpdate();
  if(update.backing&&!update.backing->completed.load()&&!update.backing->encodedSubmission.load()&&!update.backing->dependenciesSubmission.load()&&
   (!update.mipBacking||(!update.mipBacking->completed.load()&&!update.mipBacking->encodedSubmission.load()&&!update.mipBacking->dependenciesSubmission.load())))
   OriginalNativeCubeTextures().CancelReflection(update);
 }
	if(unsubmittedShadow&&unsubmittedShadow->textures)OriginalNativeShadowTextures().Cancel(*unsubmittedShadow->textures);
	if(impl)impl->shieldPending.reset();
}


bool Metal4WorldRenderer::HasScreenContinuation() const noexcept
{
 return impl && bool(impl->screenContinuation);
}
void Metal4WorldRenderer::AbortScreenContinuation() noexcept
{
 // Each successfully completed prefix is retired already. In the failure
 // case the poisoned RuntimeLease owns the exact in-flight allocations.
 if(impl)impl->screenContinuation.reset();
}

Metal4WorldPresentationResult Metal4WorldRenderer::ContinueScreen(const WorldFrame& frame,const Metal4WorldRenderOptions& options,const NativePresentationPolicy& presentation)
{
 @autoreleasepool {
  MainThread();const auto continuation=impl->screenContinuation;
  Require(continuation && continuation->main && continuation->source && options.screenUI &&
   options.screenUI->drawList && options.submissionBegan && options.submissionCompleted &&
   frame.sourceDrawFrame==continuation->source->sourceDrawFrame && frame.luaWorldDrawFrame && frame.luaVolumetricCloudFrame &&
   frame.luaWorldDrawFrame->serial==continuation->source->luaWorldDrawFrame->serial &&
   frame.luaVolumetricCloudFrame->sourceDrawFrame==frame.sourceDrawFrame,
   "Screen continuation lost its exact source frame/world/cloud recorder or publication receipts");
  // Only the screen-stage source-read branch can start this transaction.
  // These original world families must already be complete. An unexpected
  // later world producer is an ownership error, never a skipped/double draw.
  for(const auto& draw:frame.luaWorldDrawFrame->draws)Require(draw.sequence<continuation->worldCutoff,"World draw appended after the source-read screen boundary");
  for(const auto& clear:frame.luaWorldDrawFrame->clears)Require(clear.sequence<continuation->worldCutoff,"World clear appended after the source-read screen boundary");
  for(const auto& draw:frame.luaVolumetricCloudFrame->draws)Require(draw.sequence<continuation->worldCutoff,"Cloud draw appended after the source-read screen boundary");
  const auto first=continuation->rootCommands;
  Require(options.screenUI->drawList->Commands().size()>=first,"Original root screen prefix disappeared during SaveImage");
  if(options.readback)Require(options.readback->cutoff>continuation->cutoff && options.readback->root &&
   options.readback->root->Commands().size()==options.screenUI->drawList->Commands().size(),
   "SaveImage suffix cutoff/root cursor is not monotonic");
  auto selected=*options.screenUI;
  auto post=selected.postprocess?std::make_shared<NativePostprocessFrame>(*selected.postprocess):nullptr;
  if(post){
   Require(post->sourceDrawFrame==frame.sourceDrawFrame,"Screen suffix postprocess belongs to another source frame");
   std::erase_if(post->operations,[&](const auto& op){return op.commandSequence<continuation->cutoff;});
   std::erase_if(post->externalOutputs,[&](const auto& producer){return producer.sequence<continuation->cutoff;});
   for(const auto& op:post->operations)Require(op.stage==NativePostprocessStage::Screen && op.screenCommandOffset>=first,
    "Screen suffix contains a world operation or replays an earlier root offset");
   Require(post->externalOutputs.empty(),"Screen suffix has an unpaired new typed external producer");
   selected.postprocess=post;
  }
  auto result=continuation->result;result.readbackPixels.clear();result.presented=false;
  // First-prefix source callbacks already retired these simulation-owned tokens.
  result.completedWorldInfo={};result.completedWaterUpdates={};
  const auto main=continuation->main;
  RuntimeLease lease;const auto start=Clock::now();lease.SetMinSampleShadingRate(frame.minSampleShadingRate);
  main->RetainForContinuation(lease);
  const auto submission=CreateShieldSubmissionIdentity();impl->pbrSubmission=submission;
  impl->pendingMipTextures.clear();impl->particlesGPU.PrepareAtlasImages(submission);
  if(post)impl->postprocessGPU.PrepareFrameImages(lease,post,frame.sourceDrawFrame);
  std::shared_ptr<PreparedScreenFrame> screen=impl->screenGPU.Prepare(lease,selected,
   std::uint32_t(main->Color().width),std::uint32_t(main->Color().height),continuation->logicalWidth,continuation->logicalHeight,
   main->Color().pixelFormat,options.shaderValidation,result.shaderCompileMs,
   main->Depth().pixelFormat,main->Stencil().pixelFormat,main->Samples(),first);
  Require(screen->WorldTargets().empty(),"Screen suffix contains an unexecuted world target producer");
  std::shared_ptr<PreparedPostprocessFrame> effects;
  // The same fixed ScreenGPU providers as the main route read separate
  // allocations/views and write only attachments. Preserve this contract
  // when adding providers; unknown shader access must disable the opt-in.
  if(post && (!post->operations.empty() || !screen->DeferredTargets().empty()))
   effects=impl->postprocessGPU.Prepare(lease,post,main->Color(),main->Depth(),*screen,
    options.shaderValidation,result.shaderCompileMs,main->Stencil(),main,true);
  Require(effects || screen->DeferredTargets().empty(),"Screen suffix target lacks its actual postprocess insertion owner");
  auto mainReadReady=std::make_shared<bool>(false);
  std::shared_ptr<PreparedScreenReadback> readback;
  if(options.readback){
   PreparedScreenExternalImage image;
   if(options.readback->readColor)image=screen->ResolvePreviousImage(lease,options.readback->readColor,
    {NativePostprocessStage::Screen,0,std::uint32_t(selected.drawList->Commands().size()),options.readback->cutoff});
   else image={main->ReadColor(),main,[mainReadReady]{Require(*mainReadReady,"Main screen suffix has not encoded before readback");}};
   readback=impl->screenGPU.PrepareReadback(lease,std::move(image),options.readbackRectangle);
  }
  id<CAMetalDrawable> drawable=nil;
  std::shared_ptr<PreparedScreenFrame> margins;
  if(!options.readback && options.presentWorld){
   impl->surface->Refresh();const auto current=impl->surface->Snapshot();
   if(current.CanPresent() && current.renderWidth==main->Color().width && current.renderHeight==main->Color().height){
    auto layer=Metal4WindowSurfaceNativeAccess::Layer(*impl->surface);
    {NativeSubmissionTraceDrawableScope trace(&lease);
     drawable=[layer nextDrawable];}
    if(drawable){
     main->PrepareFinalDestination(lease,drawable.texture);lease.Retain(drawable);
     if(presentation.drawMargins){
      const auto& safe=presentation.safeViewport;
      margins=impl->screenGPU.PreparePresentationMargins(lease,safe,current.renderWidth,current.renderHeight,
       continuation->logicalWidth,continuation->logicalHeight,main->Color().pixelFormat,options.shaderValidation,result.shaderCompileMs,main->Samples());
     }
    }
   }
  }
  std::shared_ptr<PreparedScreenMipmaps> mipmaps=impl->screenGPU.PrepareMipmaps(lease);
  auto lifetime=[M4OriginalSourceLifetime new];lifetime->mainAttachments=main;lifetime->screen=screen;
  lifetime->presentationMargins=margins;lifetime->waterGround.push_back(continuation);
  if(effects)lifetime->waterGround.push_back(effects);if(mipmaps)lifetime->waterGround.push_back(mipmaps);
  lease.Retain(lifetime);lease.CommitResources();
  struct Failure {RuntimeLease& lease;bool began=false,completed=false;~Failure(){if(began&&!completed)lease.PoisonAfterBegin();}} failure{lease};
  auto command=lease.Begin("Original current-frame screen continuation after SaveImage");failure.began=true;
  ui::GetRecorder().RecordTargetSubmission(selected.targetPasses,submission,post);
  options.submissionBegan(submission,post.get());
  impl->EncodePendingTextureMipmaps(command,submission);impl->particlesGPU.EncodeAtlasImages(command,submission);
  if(mipmaps)mipmaps->EncodeDependencies(command);
  screen->EncodeDependencies(command);
  if(effects)effects->Encode(command,*screen,first);
  else {
   auto pass=main->LoadedPass();auto encoder=[command renderCommandEncoderWithDescriptor:pass];
   Require(encoder!=nil,"Cannot resume retained main attachments for original screen suffix");
   [encoder barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
   screen->EncodeRange(encoder,first,screen->CommandCount());[encoder endEncoding];
  }
  Require(screen->AllTargetsEncoded(),"Screen suffix has unexecuted original target producers");
  if(readback){
   if(!options.readback->readColor)main->EncodeResolve(command,MainReadImages::Color);
   *mainReadReady=true;readback->Encode(command);
  }
  if(margins){auto encoder=[command renderCommandEncoderWithDescriptor:main->LoadedPass()];
   Require(encoder!=nil,"Cannot encode final presentation margins after the screen suffix");margins->Encode(encoder);[encoder endEncoding];}
  if(drawable)main->EncodePresentationResolve(command);
  result.metrics=lease.SubmitAndWait(start,options.readback?impl->screenTargetSubmission++:impl->presentationFrame++,drawable);
  impl->CompletePendingTextureMipmaps(submission);impl->particlesGPU.CompleteAtlasImages(submission);
  screen->DidComplete();if(effects)effects->DidComplete();if(mipmaps)mipmaps->DidComplete();
  if(readback)readback->DidComplete();
  options.submissionCompleted(submission);
  if(options.readback)ui::GetRecorder().RetireTargetSubmission(submission);
  result.screenDraws+=screen->DrawCountFrom(first);result.screenTextureUploads+=screen->TextureUploads();
  result.luaPiPDraws+=screen->PiPDrawsCompleted();result.luaStartBoxMiniMapDraws+=screen->StartBoxDrawsCompleted();
  result.screenTargetsCompleted=true;result.sourceFrameCompleted=true;result.sourceDrawFrame=frame.sourceDrawFrame;
  result.completedPostprocessSerial=selected.postprocess?selected.postprocess->serial:0;
  result.presented=drawable!=nil;result.status=result.presented?Metal4PresentationStatus::Presented:Metal4PresentationStatus::OffscreenSourceCompleted;
  if(options.readback){
   continuation->cutoff=options.readback->cutoff;continuation->rootCommands=screen->CommandCount();continuation->result=result;
  }else impl->screenContinuation.reset();
  failure.completed=true;
  if(readback)result.readbackPixels=readback->PixelsRGBA8();
  return result;
 }
}

std::shared_ptr<const void> Metal4WorldRenderer::PrepareIndependentResources(const WorldFrame& frame)
{
 MainThread();Require(impl!=nullptr,"World renderer was moved from");
 if(impl->screenContinuation||!frame.originalShadow||!frame.originalShadow->terrain)return {};
 return impl->originalShadowGPU.PrepareTerrainGeometry(frame.originalShadow->terrain);
}
Metal4WorldPresentationResult Metal4WorldRenderer::Draw(const WorldFrame& frame, const Metal4WorldRenderOptions& options)
{return DrawDispatch(frame,options,{});}
Metal4WorldPresentationResult Metal4WorldRenderer::DrawPreparingWhilePending(const WorldFrame& frame,const Metal4WorldRenderOptions& options,std::function<void()> previousRetired)
{return DrawDispatch(frame,options,std::move(previousRetired));}
Metal4WorldPresentationResult Metal4WorldRenderer::DrawDispatch(const WorldFrame& frame,const Metal4WorldRenderOptions& sourceOptions,std::function<void()> previousRetired)
{
 // Reject invalid callers before reading source policy, as preparation did.
 MainThread();Require(impl!=nullptr,"World renderer was moved from");
 // Compatibility for direct synchronous API callers: capture only here on
 // main, never inside DrawPrepared. Engaged-empty receipts remain empty.
 std::optional<Metal4WorldRenderOptions> capturedOptions;
 if(!sourceOptions.pbrDeclarations){capturedOptions.emplace(sourceOptions);capturedOptions->pbrDeclarations=CapturePBRSource();}
 const auto& options=capturedOptions?*capturedOptions:sourceOptions;
 // Freeze source-side policy before worker dispatch. Continuation suffixes enter
 // here again, so a SaveImage prefix never freezes a later presentation policy.
 // Existing synchronous callbacks retire source receipts, not window events.
 const auto presentation=globalRendering?CaptureNativePresentationPolicy(
  globalRendering->fullScreen,globalRendering->dualScreenMode,
  globalRendering->viewPosX,globalRendering->viewPosY,
  globalRendering->viewSizeX,globalRendering->viewSizeY):NativePresentationPolicy{};

 static const bool enabled=[] {const char* p=std::getenv("BAR_NATIVE_INTERVENTION_RENDER_WORKER");return p&&std::string_view(p)=="1";}();
 // Readback/continuation and arbitrary engine source callbacks stay on main.
 if(!enabled||![NSThread isMainThread]||!impl||impl->screenContinuation||options.readback||
    options.submissionBegan||options.submissionCompleted||options.sourceCompleted)
  return DrawPrepared(frame,options,presentation,std::move(previousRetired),[&](std::uint64_t submission) noexcept {
   ui::GetRecorder().RecordTargetSubmission(options.screenUI->targetPasses,submission,options.screenUI->postprocess);
  });
 // Allocate/retain the exact publication identities before worker Begin.
 NativeTargetSubmissionNotice targetNotice(options.screenUI);
 auto deliverTargetNotice=[&]() noexcept {targetNotice.Deliver([](auto passes,std::uint64_t submission,auto postprocess) noexcept {
  ui::GetRecorder().RecordTargetSubmission(passes,submission,std::move(postprocess));
 });};
 const auto snapshot=Metal4WindowSurfaceNativeAccess::PublishWorkerSnapshot(*impl->surface);
 static NativeSerialRenderWorker worker;
 bool receiptReady=false;
 auto mainReceipt=std::move(previousRetired);
 auto deferredReceipt=mainReceipt?std::function<void()>([&]{receiptReady=true;}):std::function<void()>{};
 auto deliverReceipt=[&]{if(receiptReady){receiptReady=false;mainReceipt();}};
 try {
  auto result=worker.Invoke([&,presentation]{@autoreleasepool {
   Metal4SurfaceWorkerScope scope(snapshot);
   return DrawPrepared(frame,options,presentation,std::move(deferredReceipt),[&](std::uint64_t submission) noexcept {targetNotice.MarkBegun(submission);});
  }});
  deliverTargetNotice();
  deliverReceipt();
#ifdef RECOIL_METAL4_LIVE_SCENE
  static bool reported=false;if(!reported){reported=true;LOG("[NativeInterventionRenderWorker] serial_worker=1 main_wait=1 pinned_surface=1 simulation_overlap=0");}
#endif
  return result;
 }catch(...){deliverTargetNotice();deliverReceipt();throw;}
}
Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(const WorldFrame& frame,const Metal4WorldRenderOptions& options,const NativePresentationPolicy& presentation,std::function<void()> previousRetired,std::function<void(std::uint64_t)> targetSubmissionBegan)
{
 NativeCPUPhaseScope cpuPhase(3,frame.simulationFrame);
	@autoreleasepool {
		MainThread(); Require(impl != nullptr, "World renderer was moved from");
		if(impl->screenContinuation)return ContinueScreen(frame,options,presentation);
		if(options.readback)Require(options.screenUI && options.screenUI->drawList &&
			options.submissionBegan && options.submissionCompleted && frame.luaWorldDrawFrame && frame.luaVolumetricCloudFrame &&
			options.readback->screenStage && options.readback->cutoff && options.readback->root &&
			options.readback->root->Commands().size()==options.screenUI->drawList->Commands().size(),
			"Source SaveImage cut lacks its actual world/cloud prefix, root cursor or publication receipts");
		Metal4WorldPresentationResult result;
		result.simulationFrame = frame.simulationFrame;
		if(options.screenUI && options.screenUI->postprocess)
			Require(options.screenUI->postprocess->sourceDrawFrame==frame.sourceDrawFrame,
				"Copied-image commands do not belong to this actual captured world draw");
		result.capabilities = SharedRuntime().Capabilities();
		if (!impl->surface->Snapshot().attached) { result.surface = impl->surface->Snapshot(); result.status = Metal4PresentationStatus::Detached; return result; }
		impl->surface->Refresh(); result.surface = impl->surface->Snapshot(); result.status = SurfaceStatus(result.surface);
		const bool requiresSource=bool(options.readback)||bool(frame.originalShadow)||(frame.opaqueModelFrame&&frame.opaqueModelFrame->HasWriters())||ScreenFrameRequiresSource(options.screenUI)||HasNativeDeferredPostprocessWrites(frame)||(frame.shieldDepthFrame&&frame.shieldDepthFrame->HasWriters())||
			options.environmentStage.HasCubeSource()||HasPendingNativePBR(*options.pbrDeclarations,NativePBRKind::BRDF)||HasPendingNativePBR(*options.pbrDeclarations,NativePBRKind::Environment);
		bool offscreenSource=bool(options.readback) || !options.presentWorld || result.status!=Metal4PresentationStatus::Presented;
		if(offscreenSource && (!requiresSource || result.surface.closeRequested))return result;
		Require(frame.viewCamera.clipDepth != ClipDepthConvention::Unsupported && frame.viewCamera.viewport[2] > 0 && frame.viewCamera.viewport[3] > 0, "World renderer requires actual supported engine camera/viewport");
		const auto logicalWidth=offscreenSource && !result.surface.logicalWidth?options.sourceLogicalExtent[0]:result.surface.logicalWidth;
		const auto logicalHeight=offscreenSource && !result.surface.logicalHeight?options.sourceLogicalExtent[1]:result.surface.logicalHeight;
		const auto mappedViewport = MapWorldViewport(frame.viewCamera.viewport, logicalWidth, logicalHeight,
			result.surface.renderWidth, result.surface.renderHeight);
		if (!mappedViewport) { result.status = Metal4PresentationStatus::ZeroExtent; return result; }
		result.renderViewport = mappedViewport->viewport;
		for (double value: result.renderViewport) Require(std::isfinite(value) && std::abs(value) <= 16'384,
			"Mapped world viewport exceeds experimental numeric bounds");
		MatrixFinite(frame.viewCamera.view); MatrixFinite(frame.viewCamera.projection);
		for (float value: frame.viewCamera.position) Require(std::isfinite(value), "Non-finite actual camera position");
		Require(frame.units.size() <= 100'000, "World unit count exceeds diagnostic bounds");
		cpuPhase.Mark(0);
		PreparationTraceFrame preparationDetail(frame.simulationFrame);
  CountPreparationTrace(35,NativeCUSCPUReadScope::ActiveCount());
  CountPreparationTrace(38,NativeCUSCPUReadScope::ActivePreparationNanoseconds());
		std::optional<NativeCPUPhaseScope> prepPhase;prepPhase.emplace(4,frame.simulationFrame);
  RuntimeLease lease(std::move(previousRetired));
  // Sweep once even when main/water effect frames are absent or empty.
  impl->projectileEffectsGPU.PruneExpiredImages();
  impl->lightningGPU.PruneExpiredImages();
  // These existing stores upload in place. Keep their original last-reader
  // boundary on deformation/map changes; unchanged immutable inputs overlap.
  if(impl->heightOwner!=frame.terrain.heights || impl->map.owner!=frame.terrain.tiles)
   lease.RetirePreviousSubmission();
		lease.SetMinSampleShadingRate(frame.minSampleShadingRate);
  // Render-only preparation of this request. Main can replace its own receipt
  // without resetting these resources or the completion owner below.
  std::shared_ptr<PreparedMetalInfo> reflectionMetalInfo;
  std::shared_ptr<PreparedWorldInfo> reflectionWorldInfo;
  if(options.environmentStage.frame){
   const auto& input=options.environmentStage.frame;double compileMs=0;
   if(input->metalInfoFrame){
    reflectionMetalInfo=SharedMetalInfoGPU().Prepare(lease,input->metalInfoFrame,options.environmentStage.shaderValidation,compileMs);
    Require(!reflectionMetalInfo->HasUpdates(),"Original reflection must pin a completed metal producer");
   }
   if(input->worldInfoFrame && input->worldInfoFrame->mode==NativeWorldInfoMode::Los)
    reflectionWorldInfo=SharedWorldInfoGPU().Prepare(lease,input->worldInfoFrame,options.environmentStage.shaderValidation,compileMs);
  }

		Require(frame.mainSampling.samples==impl->mainSampling.samples && frame.mainSampling.effective==impl->mainSampling.effective,"Captured main sampling differs from the allocated renderer context");
		prepPhase->Mark(0);
		if(options.screenUI && options.screenUI->postprocess)
			impl->postprocessGPU.PrepareFrameImages(lease,options.screenUI->postprocess,frame.sourceDrawFrame);
		prepPhase->Mark(1);
		CAMetalLayer* layer = Metal4WindowSurfaceNativeAccess::Layer(*impl->surface);
		Require(layer != nil && layer.device == lease.Device(), "World layer/device ownership mismatch");
		const auto resourceStart = Clock::now();
		impl->pendingMipTextures.clear();
		impl->Map(lease, frame.terrain);
		WorldUniform terrainUniform{};
		terrainUniform.viewProjection = simd_mul(Matrix(frame.viewCamera.projection), Matrix(frame.viewCamera.view));
		terrainUniform.world = matrix_identity_float4x4; terrainUniform.piece = matrix_identity_float4x4;
		terrainUniform.terrain = simd_make_uint4(frame.terrain.heights->width, frame.terrain.heights->height, 0, 0);
		terrainUniform.options = simd_make_uint4(frame.viewCamera.clipDepth == ClipDepthConvention::MinusOneToOne ? 1 : 0, 0, 0, 0);
		terrainUniform.parameters = simd_make_float4(frame.terrain.squareSize, 0, 0, 0);
		std::vector<WorldUniform> uniforms{terrainUniform};
  Require(unsigned(bool(frame.bumpWater))+unsigned(bool(frame.basicWater))+unsigned(bool(frame.legacyWater))+unsigned(bool(frame.dynamicWater))<=1,"Original water modes must be mutually exclusive");
  const bool bumpWaterActive=bool(frame.bumpWater),basicWaterActive=bool(frame.basicWater),legacyWaterActive=bool(frame.legacyWater),dynamicWaterActive=bool(frame.dynamicWater);
  const bool waterActive=(bumpWaterActive || basicWaterActive || legacyWaterActive || dynamicWaterActive) && frame.waterScene;
  if(bumpWaterActive || basicWaterActive || legacyWaterActive || dynamicWaterActive)Require(waterActive,"Water frame lacks its actual original scene stages");
  if(waterActive)ValidateNativeWaterSceneFrame(*frame.waterScene,frame.sourceDrawFrame,
   (dynamicWaterActive || legacyWaterActive || (bumpWaterActive && frame.bumpWater->settings.reflection>0)),dynamicWaterActive || (bumpWaterActive && frame.bumpWater->settings.refraction>1));
		const bool shadowEnabled = frame.shadow && frame.lighting.requested.shadows > 0;
		WorldUniform shadowTerrainUniform = terrainUniform;
		if (shadowEnabled) {
			MatrixFinite(frame.shadow->view); MatrixFinite(frame.shadow->projection);
			Require(frame.shadow->clipDepth != ClipDepthConvention::Unsupported, "Original shadow clip-control unsupported");
			shadowTerrainUniform.viewProjection = Matrix(frame.shadow->projection);
			shadowTerrainUniform.options.x = frame.shadow->clipDepth == ClipDepthConvention::MinusOneToOne;
		}
		const size_t shadowTerrainOffset = uniforms.size() * sizeof(WorldUniform);
		uniforms.push_back(shadowTerrainUniform);
		std::vector<Impl::DrawPiece> pieces;
		std::set<std::string> sources, pending;
		std::array<uint8_t,4> legacyDiagnosticMasks{}; // Shadow, main, reflection, refraction.
		const auto markLegacyPiece = [&](const Impl::DrawPiece& piece, bool shadow) {
			uint8_t bits = piece.legacyDiagnostics;
			if (shadow) bits &= Impl::CurrentUnitPose | Impl::DamageDisplacement;
			else if (frame.lighting.requested.modelGamma != 1) bits |= Impl::ModelGamma;
			const unsigned route = shadow ? 0 : piece.view == NativeWaterScenePass::Main ? 1 :
				piece.view == NativeWaterScenePass::Reflection ? 2 : 3;
			legacyDiagnosticMasks[route] |= bits;
		};
		auto append = [&](const std::shared_ptr<const WorldModel>& owner, const std::shared_ptr<const NativeS3OTexturePair>& sourceTextures, const std::vector<WorldPiecePose>& poses,
			const WorldMatrix& world, const std::array<uint8_t,4>& team, const std::shared_ptr<const WorldModelMaterial>& material,
			uint32_t objectId, float alpha, float health, bool feature, bool fading, bool castsShadow, bool visible, bool projectile = false, const WorldCamera* camera = nullptr, NativeWaterScenePass view=NativeWaterScenePass::Main) {
			Require(owner != nullptr && poses.size() == owner->pieces.size(), "Actual model pose/piece count mismatch");
			Require(material != nullptr && (material->family == WorldMaterialFamily::EngineModel || material->normal != nullptr), "Actual model is missing its selected material/normal");
			Require(std::isfinite(alpha) && alpha >= 0 && alpha <= 1 && std::isfinite(health), "Invalid original model alpha/health");
			MatrixFinite(world);
			auto& model = impl->Model(lease, owner);
			bool drawn = false;
   Require(!sourceTextures||sourceTextures->sourceDrawFrame==frame.sourceDrawFrame,"Original S3O model pair belongs to another actual draw frame");
   std::array<std::shared_ptr<const WorldTexture>,2> images=owner->textures;
   std::array<id<MTLSamplerState>,2> imageSamplers{nil,nil};
   std::array<std::shared_ptr<Impl::TextureUploadState>,2> imageStatus;
   for(unsigned slot=0;slot<2;++slot){
    ScreenSampler sampler;sampler.filter=ScreenFilter::Linear;sampler.magnificationFilter=ScreenFilter::Linear;sampler.mipFilter=ScreenMipFilter::Linear;sampler.wrapS=sampler.wrapT=ScreenWrap::Repeat;
    // The unattached path preserves its original compiled repeat/linear/mip
    // sampler. Attached draws consume the actual retained original object.
    if(sourceTextures){
     images[slot]=NativeS3OTexturePixels(*sourceTextures,slot);const auto& object=*sourceTextures->objects[slot];const auto& p=object.sampling;
     Require(p.compareMode==0&&p.baseLevel==0&&p.maxLevel>=int(object.allocation.levels)-1&&p.minLod<=0&&p.maxLod>=float(object.allocation.levels-1)&&p.lodBias==0,
      "Original S3O model source requires its actual comparison/base/max/LOD consumer");
     Require(p.swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906}&&p.anisotropy>=1&&p.anisotropy<=16&&std::floor(p.anisotropy)==p.anisotropy,
      "Original S3O model source requires its actual swizzle/anisotropy consumer");
     Require(p.borderColor==std::array<float,4>{0,0,0,0}||p.borderColor==std::array<float,4>{1,1,1,1},
      "Original S3O model source requires its actual custom border consumer");
     Require(p.wrapS!=0x2900&&p.wrapT!=0x2900,"Original S3O model GL_CLAMP requires its actual coordinate/gradient consumer");
     Require((p.minFilter==0x2600||p.minFilter==0x2601||(p.minFilter>=0x2700&&p.minFilter<=0x2703))&&(p.magFilter==0x2600||p.magFilter==0x2601),"Original S3O model filter has no actual consumer");
     NativeCUSTextureBinding binding;binding.knowledge=NativeCUSBindingKnowledge::BoundObject;binding.object=sourceTextures->objects[slot];sampler=ui::NativeUITextureHistory::DescribeSampler(binding);
    }
    Require(images[slot]!=nullptr,"Original model draw has no actual retained primary/secondary source");
    imageSamplers[slot]=impl->screenGPU.PrepareSampler(lease,sampler);lease.Retain(imageSamplers[slot]);
   }
   id<MTLTexture> color = nil, other = nil, normal = nil;
			sources.insert(owner->sourceName); sources.insert(material->definitionSource + "#" + material->definitionName);
			if (material->normal) sources.insert(material->normal->sourcePath);

			uint8_t legacyDiagnostics = !feature && !projectile ? Impl::CurrentUnitPose : 0;
			if (!material->engineStandardGL4 && material->family != WorldMaterialFamily::EngineModel) {
				if (material->healthDisplacement) legacyDiagnostics |= Impl::DamageDisplacement;
				if (material->healthTexturing) legacyDiagnostics |= Impl::DamageTexturing;
				if (material->definitionName == "armunit" || material->definitionName == "corunit" || material->definitionName == "legunit" ||
					material->definitionName == "armscavenger" || material->definitionName == "corscavenger" || material->definitionName == "legscavenger")
					legacyDiagnostics |= Impl::TreadUV;
				// Original unit/unitskinning variants have no active losMapTex.
				if (feature) legacyDiagnostics |= Impl::FeatureFadeAndNight | Impl::ViewerInfo;
			}
			for (size_t i = 0; i < poses.size(); ++i) {
				if (!poses[i].visible || model.pieces[i].count == 0) continue;
				MatrixFinite(poses[i].modelSpace);
				if (color == nil) {
					color = impl->Texture(lease,images[0],sourceTextures ? sourceTextures->objects[0].get() : nullptr);
					other = impl->Texture(lease,images[1],sourceTextures ? sourceTextures->objects[1].get() : nullptr);
     for(unsigned slot=0;slot<2;++slot)imageStatus[slot]=impl->textures.at(images[slot].get()).status;
					if (material->normal) normal = impl->Texture(lease, material->normal);
					sources.insert(images[0]->sourcePath);sources.insert(images[1]->sourcePath);
				}
				WorldUniform u = terrainUniform;
				if(camera){u.viewProjection=simd_mul(Matrix(camera->projection),Matrix(camera->view));u.options.x=camera->clipDepth==ClipDepthConvention::MinusOneToOne;}
				u.world = Matrix(world); u.piece = Matrix(poses[i].modelSpace);
				u.team = {float(team[0])/255, float(team[1])/255, float(team[2])/255, alpha};
				u.options.w = uint32_t(material->treeWind) | (uint32_t(material->alphaShadows)<<1) | (uint32_t(material->pbrOverride)<<2) |
					(uint32_t(material->normalMapping && frame.lighting.requested.normalMapping)<<3) | (uint32_t(material->alphaShadows)<<4);
				u.options.y = uint32_t(images[0]->flipV) | (uint32_t(sourceTextures?false:!images[0]->mipmappedSampling)<<1);
				u.options.z = uint32_t(images[1]->flipV) | (uint32_t(sourceTextures?false:!images[1]->mipmappedSampling)<<1);
				if (material->normal) u.options.w |= (uint32_t(material->normal->flipV)<<7) | (uint32_t(!material->normal->mipmappedSampling)<<8);
				u.options.w |= (uint32_t(material->vertexAO)<<9) | (uint32_t(material->flashlights)<<10) | (uint32_t(fading&&!feature&&!projectile)<<11);
				u.terrain.z = std::bit_cast<uint32_t>(health); u.terrain.w = objectId;
				u.parameters.y = float(frame.simulationFrame) + frame.interpolationOffset; u.parameters.w = material->brightness;
				u.parameters.z = fading ? .1f : .5f; // Original CModelDrawerStateGL4 alphaCtrl.
				auto& geometry = model.pieces[i]; lease.AddPersistentAllocation(geometry.vertices); lease.AddPersistentAllocation(geometry.indices);
				const size_t offset = uniforms.size() * sizeof(WorldUniform); uniforms.push_back(u);
				u.viewProjection = shadowTerrainUniform.viewProjection; u.options.x = shadowTerrainUniform.options.x;
				const size_t shadowOffset = uniforms.size() * sizeof(WorldUniform); uniforms.push_back(u);
				pieces.push_back({&geometry,color,other,normal,offset,shadowOffset,feature,fading,castsShadow,visible,material->family==WorldMaterialFamily::EngineModel,projectile,material->engineStandardGL4,view,objectId,uint32_t(i)});
    auto& piece=pieces.back();piece.legacyDiagnostics=legacyDiagnostics;piece.colorSampler=imageSamplers[0];piece.otherSampler=imageSamplers[1];piece.sourceTextures=sourceTextures;
    piece.requireTextures=[imageStatus](uint64_t submission){for(const auto& status:imageStatus)Require(status&&(status->ready||(submission&&status->encodedSubmission==submission)),"Original S3O model mip producer has not executed before its consumer");};
				drawn |= visible;
			}
			return drawn;
		};
		for (const auto& unit : frame.units) {
			if (append(unit.model,unit.modelTextures,unit.pieces,unit.world,unit.teamColor,unit.material,uint32_t(unit.unitId),unit.drawAlpha,unit.healthFraction,false,unit.alpha,unit.castsShadow,unit.opaque||unit.alpha)) {
				++result.unitsDrawn; result.selectedUnits += unit.selected;
			}
		}
		for (const auto& feature : frame.features) {
			if (append(feature.model,feature.modelTextures,feature.pieces,feature.world,feature.teamColor,feature.material,uint32_t(feature.featureId),feature.drawAlpha,
				feature.healthFraction,true,feature.fading,feature.castsShadow,feature.opaque||feature.fading)) ++result.featuresDrawn;
		}
		// Opaque projectile models consume the same actual geometry/texture cache.
		if (frame.particleFrame) {
			for (const auto& projectile : frame.particleFrame->models) {
				if (!projectile.model) continue;
				auto material = std::make_shared<WorldModelMaterial>(); material->definitionSource = "ProjectileDrawer::DrawOpaque";
				material->definitionName = "original-model"; material->normal = projectile.modelTextures?NativeS3OTexturePixels(*projectile.modelTextures,1):projectile.model->textures[1]; material->normalMapping = false; material->brightness = 1;

				std::vector<WorldPiecePose> poses; poses.reserve(projectile.pieces.size());
				for (const auto& pose : projectile.pieces) poses.push_back({pose.modelSpace,pose.visible});
				if (append(projectile.model,projectile.modelTextures,poses,projectile.world,projectile.teamColor,material,uint16_t(projectile.projectileId),1,1,false,false,false,true,true))
					++result.projectileModelsDrawn;
			}
			pending.insert(frame.particleFrame->unsupported.begin(),frame.particleFrame->unsupported.end());
		}

		// The same immutable geometry/texture caches serve fresh camera-dependent
  // reflection/refraction captures. Only poses/uniforms are view-specific.
  if(waterActive)for(const auto& view:{frame.waterScene->refraction,frame.waterScene->reflection})if(view){
   const auto& input=view->frame;
   for(const auto& unit:input.units)if(!input.opaqueModelFrame||unit.alpha)append(unit.model,unit.modelTextures,unit.pieces,unit.world,unit.teamColor,unit.material,
    uint32_t(unit.unitId),unit.drawAlpha,unit.healthFraction,false,unit.alpha,false,unit.opaque||unit.alpha,false,&input.viewCamera,view->pass);
   for(const auto& feature:input.features)if(!input.opaqueModelFrame||feature.fading)append(feature.model,feature.modelTextures,feature.pieces,feature.world,feature.teamColor,feature.material,
    uint32_t(feature.featureId),feature.drawAlpha,feature.healthFraction,true,feature.fading,false,feature.opaque||feature.fading,false,&input.viewCamera,view->pass);
   if(input.particleFrame)for(const auto& projectile:input.particleFrame->models){
    auto material=std::make_shared<WorldModelMaterial>();material->definitionSource="ProjectileDrawer::DrawOpaque";
    material->definitionName="original-model";material->normal=projectile.modelTextures?NativeS3OTexturePixels(*projectile.modelTextures,1):projectile.model->textures[1];material->normalMapping=false;material->brightness=1;
    std::vector<WorldPiecePose> poses;for(const auto& pose:projectile.pieces)poses.push_back({pose.modelSpace,pose.visible});
    append(projectile.model,projectile.modelTextures,poses,projectile.world,projectile.teamColor,material,uint16_t(projectile.projectileId),1,1,false,false,false,true,true,&input.viewCamera,view->pass);
   }
  }
  struct PlacementPieces {uint64_t sequence;size_t begin,end;__strong id<MTLBuffer> camera,modelLights;};
		std::vector<PlacementPieces> placementPieces;
		if(frame.placementFrame) for(const auto& sample:frame.placementFrame->samples) {
			ValidateNativeBuildingSample(sample);
			Require(sample.modelLights && sample.modelLights->sourceDrawFrame==frame.sourceDrawFrame,
				"Original building preview lacks its same-draw model light capture");
			const auto lightBytes=PackNativeModelLightFrame(*sample.modelLights);
			auto lightBuffer=Buffer(lease.Device(),lightBytes.data(),lightBytes.size(),@"Original building preview dynamic model lights");
			lease.AddAllocation(lightBuffer);
			auto material=std::make_shared<WorldModelMaterial>();material->definitionSource="ModelVertProg.glsl+ModelFragProg.glsl";material->definitionName="DrawIndividualDefAlpha";
			std::vector<WorldPiecePose> poses;poses.reserve(sample.model->pieces.size());for(const auto& piece:sample.model->pieces)poses.push_back({piece.bindPose,true});
			const size_t begin=pieces.size();
			append(sample.model,sample.modelTextures,poses,sample.world,sample.teamColor,material,0,sample.alpha,1,false,true,false,false);
			const std::array<float,4> camera{sample.legacyCameraPosition[0],sample.legacyCameraPosition[1],sample.legacyCameraPosition[2],0};
			auto cameraBuffer=Buffer(lease.Device(),camera.data(),sizeof(camera),@"Original building preview inverse projection camera");lease.AddAllocation(cameraBuffer);
			placementPieces.push_back({sample.sequence,begin,pieces.size(),cameraBuffer,lightBuffer});
		}
		// A config request is not proof that an original deferred invocation is
		// missing. Connected source graphs enforce their actual selected phases.
		if (frame.lighting.requested.deferredMap && (!frame.mainGround || !frame.mainGround->ground))
			pending.insert("Requested deferred map setting has no captured original ground source; legacy terrain path selected");
		if (frame.lighting.requested.deferredModel && !frame.opaqueModelFrame)
			pending.insert("Requested deferred model setting has no captured original model source; legacy model path selected");


		// Legacy info-map gaps are reported at their actual draws above.
		// Non-default legacy model gamma is also scoped to actual selected draws.
		result.materialSources.assign(sources.begin(),sources.end());
		result.materialDependenciesPending.assign(pending.begin(),pending.end());

		for (const auto& uniform: uniforms) for (size_t c = 0; c < 4; ++c) for (size_t r = 0; r < 4; ++r)
			Require(std::isfinite(uniform.viewProjection.columns[c][r]), "Actual camera matrix product overflow");
		id<MTLBuffer> uniformBuffer = Buffer(lease.Device(), uniforms.data(), uniforms.size() * sizeof(WorldUniform), @"Actual camera/unit current piece matrices");
		lease.AddAllocation(uniformBuffer);
		const auto allocateTarget = [&](__strong id<MTLTexture>& target, MTLPixelFormat format, uint32_t width, uint32_t height, MTLTextureUsage usage) {
			Require(width && height && width <= lease.Capabilities().maxTextureDimension2D && height <= lease.Capabilities().maxTextureDimension2D, "World target extent exceeds actual device bounds");
			if (target == nil || target.width != width || target.height != height || target.pixelFormat != format) {
				MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:width height:height mipmapped:NO];
				descriptor.storageMode = MTLStorageModePrivate; descriptor.usage = usage;
				target = [lease.Device() newTextureWithDescriptor:descriptor]; Require(target != nil, "Cannot allocate original world target");
			}
			lease.AddAllocation(target);
		};
		prepPhase->Mark(2);
		const auto mainSamples=impl->mainSampling.samples;
		auto mainAttachments=impl->mainStorage.Prepare(lease,result.surface.renderWidth,result.surface.renderHeight,layer.pixelFormat,mainSamples);
		impl->mainSampling.allocationGeneration=mainAttachments->Generation();
		impl->depth=mainAttachments->Depth();impl->stencil=mainAttachments->Stencil();
		// Preserve the cropped per-sample and decal images. Soft particles get a
		// separate logical-size single-depth image at this same terrain-only event.
		const auto& crop = mappedViewport->scissor;
		const bool needsSampleDepth=frame.groundDecals&&frame.groundDecals->highQuality;
		auto terrainSnapshot=MainTerrainDepthSnapshot::Prepare(lease,mainAttachments,crop,needsSampleDepth,true,impl->terrainDepthSamples,impl->terrainDepth);
		impl->terrainDepthSamples=terrainSnapshot->SamplesImage();impl->terrainDepth=terrainSnapshot->SingleImage();
		const bool needsLogicalParticleDepth=frame.particleFrame&&frame.particleFrame->softenRequested&&!frame.particleFrame->ranges.empty();
		std::optional<LogicalCopyMapping> logicalCopyMapping;
		id<MTLBuffer> logicalCopyCoordinates=nil,terrainCopyCoordinates=nil,waterCopyOrigin=nil;
		id<MTLTexture> particleTerrainDepth=impl->terrainDepth;
		if(bumpWaterActive||(legacyWaterActive&&frame.legacyWater->refraction)||needsLogicalParticleDepth){
			logicalCopyMapping=MakeLogicalCopyMapping(frame.viewCamera.viewport,logicalWidth,logicalHeight,
				result.surface.renderWidth,result.surface.renderHeight,crop,frame.sourceDrawFrame,mainAttachments->Generation(),mainSamples);
			const auto& coordinates=logicalCopyMapping->coordinates;
			if(!logicalCopyMapping->unitScale){
				logicalCopyCoordinates=Buffer(lease.Device(),coordinates.data(),coordinates.size()*sizeof(uint32_t),@"Same-frame logical copy pixel-center mapping");
				lease.AddAllocation(logicalCopyCoordinates);
			}else if(bumpWaterActive||(legacyWaterActive&&frame.legacyWater->refraction)){
				const simd_uint2 origin{uint32_t(crop[0]),uint32_t(crop[1])};
				waterCopyOrigin=Buffer(lease.Device(),&origin,sizeof(origin),@"Original pre-water copy viewport origin");lease.AddAllocation(waterCopyOrigin);
			}
			if(needsLogicalParticleDepth&&!logicalCopyMapping->unitScale){
				allocateTarget(impl->logicalTerrainDepth,MTLPixelFormatDepth32Float,logicalCopyMapping->width,logicalCopyMapping->height,
					MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget);
				particleTerrainDepth=impl->logicalTerrainDepth;
				auto relative=coordinates;
				for(uint32_t x=0;x<logicalCopyMapping->width;++x)relative[1+x]-=crop[0];
				for(uint32_t y=0;y<logicalCopyMapping->height;++y)relative[1+logicalCopyMapping->width+y]-=crop[1];
				terrainCopyCoordinates=Buffer(lease.Device(),relative.data(),relative.size()*sizeof(uint32_t),@"Terrain snapshot logical mapping relative to retained crop");
				lease.AddAllocation(terrainCopyCoordinates);
			}
		}
		std::vector<PreparedScreenExternalImage> terrainImages;
		const auto retainTerrainImage=[&](PreparedScreenExternalImage image){terrainImages.push_back(std::move(image));};
		const uint64_t sourceSubmission=frame.shieldDepthFrame?frame.shieldDepthFrame->submissionIdentity:CreateShieldSubmissionIdentity();
		std::shared_ptr<PreparedUnitStencil> unitStencil;
		if(frame.luaUnitStencilFrame && !frame.luaUnitStencilFrame->passes.empty()){
			Require(frame.luaUnitStencilFrame->sourceDrawFrame==frame.sourceDrawFrame,"Original Unit Stencil Genesis belongs to another source frame");
			unitStencil=impl->unitStencilGPU.Prepare(lease,frame.luaUnitStencilFrame,options.shaderValidation,result.shaderCompileMs,sourceSubmission);
		}
		impl->pbrSubmission=sourceSubmission;
		impl->particlesGPU.PrepareAtlasImages(sourceSubmission);
		prepPhase->Mark(3);
  std::shared_ptr<PreparedOriginalShadowSource> originalShadow;
  std::array<std::shared_ptr<const NativeShadowTextureBacking>,2> initialShadow;
  id<MTLTexture> sampledShadowColor=nil;
  const auto shadowColorView=[&](const NativeShadowTextureBacking& backing){
   auto target=NativeShadowTextureImage(backing);
   const auto channels=backing.request.colored?
    MTLTextureSwizzleChannels{MTLTextureSwizzleRed,MTLTextureSwizzleGreen,MTLTextureSwizzleBlue,MTLTextureSwizzleOne}:
    MTLTextureSwizzleChannels{MTLTextureSwizzleRed,MTLTextureSwizzleRed,MTLTextureSwizzleRed,MTLTextureSwizzleOne};
   auto create=[&](){return [target newTextureViewWithPixelFormat:target.pixelFormat textureType:MTLTextureType2D levels:NSMakeRange(0,1) slices:NSMakeRange(0,1) swizzle:channels];};
   auto view=InterventionPoolsEnabled()?impl->interventionShadowViews.Acquire(lease,target,backing.request.colored,create):create();
   Require(view!=nil,"Original shadow sampling has no actual RGB1/RRR1 resource view");lease.AddAllocation(target);lease.Retain(view);return view;
  };
  if(frame.originalShadow){
   Require(frame.originalShadow->sourceDrawFrame==frame.sourceDrawFrame&&frame.originalShadow->textures&&frame.originalShadow->cus,
    "Original shadow graph has another actual source draw");
   const auto& named=*frame.originalShadow->textures;
   Require(named.depth.backing!=nullptr,"Original shadow graph lost its actual named depth version");
   const auto color=named.redrawColor?named.color.backing:named.retainedColor;
   Require(color!=nullptr,"Original shadow graph lost its actual color version");
   impl->shadowDepth=NativeShadowTextureImage(*named.depth.backing);sampledShadowColor=shadowColorView(*color);
   lease.AddAllocation(impl->shadowDepth);lease.AddAllocation(sampledShadowColor);
  }else if(OriginalNativeShadowTextures().Attached()&&shadowHandler.HasNativeTextureOwner()){
   Require(!shadowEnabled,"Actual original pre-IBL shadow producer is missing");
   initialShadow={OriginalNativeShadowTextures().Image(shadowHandler.GetShadowTextureID()),OriginalNativeShadowTextures().Image(shadowHandler.GetColorTextureID())};
   for(const auto& backing:initialShadow)Require(backing&&(backing->operation==NativeShadowTextureOperation::InitialClear||backing->completed.load()),
    "Native disabled-shadow target has an unexecuted original draw producer");
   impl->shadowDepth=NativeShadowTextureImage(*initialShadow[0]);sampledShadowColor=shadowColorView(*initialShadow[1]);
   lease.AddAllocation(impl->shadowDepth);lease.AddAllocation(sampledShadowColor);
  }else{
   allocateTarget(impl->shadowDepth,MTLPixelFormatDepth32Float,shadowEnabled?frame.shadow->size:1,shadowEnabled?frame.shadow->size:1,MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead);
  }
  std::unique_ptr<PreparedShadowColor> shadowColor;
  if(!frame.originalShadow&&frame.shadowColorFrame){
   shadowColor=impl->particlesGPU.PrepareShadowColor(lease,sourceSubmission,frame.shadowColorFrame,impl->shadowDepth,options.shaderValidation,result.shaderCompileMs);
   sampledShadowColor=shadowColor->Color();
  }
  if(shadowEnabled)Require(sampledShadowColor&&(frame.originalShadow||
   (shadowColor&&shadowColor->SourceDrawFrame()==frame.sourceDrawFrame)),
   "Original model shading requires its actual same-draw named shadow source");
  const unsigned shadowColorPresence = sampledShadowColor != nil;
		prepPhase->Mark(4);
  std::shared_ptr<PreparedWorldInfo> worldInfo;
  if(frame.worldInfoFrame)worldInfo=SharedWorldInfoGPU().Prepare(lease,frame.worldInfoFrame,options.shaderValidation,result.shaderCompileMs);
  Require(frame.environmentFrame!=nullptr,"Original pre-unit environment frame is missing");
		const auto& environmentFrame=*frame.environmentFrame;
		Require(options.environmentStage.Matches(frame.environmentFrame), "Original reflection image stage was not retained before Lua PreUnit");
		if (reflectionMetalInfo) lease.AddAllocation(reflectionMetalInfo->Info());
		if (reflectionWorldInfo) reflectionWorldInfo->RegisterResources(lease);
		Require(environmentFrame.terrain.tiles==frame.terrain.tiles&&environmentFrame.terrain.heights==frame.terrain.heights,"Original reflection terrain changed after its pre-unit capture stage");
		auto environment=impl->environmentGPU.Prepare(lease,environmentFrame,frame.viewCamera,layer.pixelFormat,options.shaderValidation,result.shaderCompileMs,sourceSubmission,mainSamples);
		auto pbrImages=[sourceSubmission](RuntimeLease& sourceLease,const NativeCUSTextureBinding& binding,unsigned unit){
			Require(unit==7,"Original EnvLUT sampled an unexpected unit");return ResolveNativeCubeTexture(sourceLease,binding,sourceSubmission);
		};
		auto pbrBRDF=impl->pbrSource.Prepare(lease,NativePBRKind::BRDF,pbrImages,*options.pbrDeclarations);
		auto pbrEnvironment=impl->pbrSource.Prepare(lease,NativePBRKind::Environment,pbrImages,*options.pbrDeclarations);
		result.reflectionFace=environment->Face();result.reflectionReady=environment->ReflectionReady();
		result.unitBRDFWidth=512;result.unitBRDFGOption=1;
		// SceneBridge reports warm-up/readiness transitions separately from missing consumers.
		if(environmentFrame.sky->kind!=NativeSkyKind::OriginalNull)sources.insert(environmentFrame.sky->sourcePath);
		sources.insert("LuaRules/Gadgets/Include/GenBrdfLut.lua#512-RG16F-G_OPTION1-1024samples");
		prepPhase->Mark(5);
		SceneUniform scene{};scene.modelClipPlane=scene.terrainClipPlane={0,0,0,1};
		const auto& lighting = frame.lighting;
		scene.sun = Vec(lighting.sunDirection); scene.cameraPosition = Vec(frame.viewCamera.position);
		scene.groundAmbient = Vec(lighting.groundAmbient); scene.groundDiffuse = Vec(lighting.groundDiffuse); scene.groundSpecular = Vec(lighting.groundSpecular);
		scene.modelAmbient = Vec(lighting.modelAmbient); scene.modelDiffuse = Vec(lighting.modelDiffuse); scene.modelSpecular = Vec(lighting.modelSpecular);
		scene.fogColor = Vec(lighting.fogColor); scene.fogParameters = Vec(lighting.fogParameters);
		scene.waterBase = Vec(lighting.waterBase); scene.waterMinimum = Vec(lighting.waterMinimum); scene.waterAbsorb = Vec(lighting.waterAbsorb);
		scene.parameters = {lighting.specularExponent,lighting.groundShadowDensity,lighting.modelShadowDensity,0};
		scene.shadow = shadowEnabled ? Matrix(frame.shadow->view) : matrix_identity_float4x4;
		scene.flags.x = shadowEnabled;
		Require(frame.terrain.material != nullptr, "Original SMF material capture missing");
		const auto& terrainMaterial = *frame.terrain.material;
		std::shared_ptr<PreparedMiniMap> standardShade;
		if (bumpWaterActive || frame.groundDecals || (frame.luaGameDecalFrame && !frame.luaGameDecalFrame->draws.empty())) {
			Require(terrainMaterial.standardShading!=nullptr,"Original STD terrain shading source missing");
			standardShade = impl->standardTerrainGPU->Prepare(lease,terrainMaterial.standardShading,options.shaderValidation,result.shaderCompileMs);
		}
		scene.flags.y = uint32_t(terrainMaterial.blendNormals!=nullptr) | (uint32_t(terrainMaterial.specular!=nullptr)<<1) | (uint32_t(terrainMaterial.emission!=nullptr)<<2) |
			(uint32_t(terrainMaterial.splatDistribution&&(terrainMaterial.splatDetail||terrainMaterial.splatNormals[0]))<<3) |
			(uint32_t(terrainMaterial.splatDistribution&&terrainMaterial.splatNormals[0])<<4) | (uint32_t(terrainMaterial.splatNormalDiffuseAlpha)<<5) | (uint32_t(!terrainMaterial.advancedShading)<<6);
		scene.flags.z = (environment->ReflectionReady()?65536u:0u)|(environmentFrame.reflectionMipmaps?131072u:0u)|uint32_t(terrainMaterial.voidWater) | (uint32_t(terrainMaterial.voidGround)<<1) | (uint32_t(terrainMaterial.waterAbsorption)<<2);
		std::shared_ptr<PreparedMetalInfo> metalInfo;
		if (frame.metalInfoFrame) {
			Require(frame.metalInfoFrame->squaresX == frame.terrain.squaresX && frame.metalInfoFrame->squaresZ == frame.terrain.squaresZ && frame.terrain.squareSize == 8.f,
				"Original metal overlay and actual terrain map dimensions differ");
			metalInfo = SharedMetalInfoGPU().Prepare(lease, frame.metalInfoFrame, options.shaderValidation, result.shaderCompileMs);
			Require(!metalInfo->HasUpdates(), "Terrain must consume the completed original metal update, without advancing its history");
			scene.flags.z |= NativeTerrainInfoFlags(NativeWorldInfoMode::Metal);
		}
		if(frame.worldInfoFrame && frame.worldInfoFrame->mode==NativeWorldInfoMode::Los){
			Require(worldInfo&&worldInfo->MatchesFrame(*frame.worldInfoFrame),"Terrain LOS needs its actual original combiner version");
			scene.flags.z|=NativeTerrainInfoFlags(NativeWorldInfoMode::Los);
		}
		scene.splatScales = Vec(terrainMaterial.splatScales); scene.splatMultipliers = Vec(terrainMaterial.splatMultipliers);
		scene.mapDimensions = {float(frame.terrain.squaresX)*frame.terrain.squareSize,float(frame.terrain.squaresZ)*frame.terrain.squareSize,terrainMaterial.voidAlphaMinimum,terrainMaterial.textureLodBias};
		scene.parameters.w = terrainMaterial.minimumHeight;
		const std::array<std::shared_ptr<const WorldTexture>,11> filters={terrainMaterial.detail,terrainMaterial.specular,terrainMaterial.blendNormals,terrainMaterial.emission,{},terrainMaterial.splatDetail,terrainMaterial.splatDistribution,terrainMaterial.splatNormals[0],terrainMaterial.splatNormals[1],terrainMaterial.splatNormals[2],terrainMaterial.splatNormals[3]};
		for(size_t i=0;i<filters.size();++i)if(filters[i]&&!filters[i]->mipmappedSampling)scene.flags.w|=uint32_t(1u<<i);
		for (const auto* vector : {&scene.sun,&scene.cameraPosition,&scene.groundAmbient,&scene.groundDiffuse,&scene.groundSpecular,&scene.modelAmbient,&scene.modelDiffuse,&scene.modelSpecular,&scene.fogColor,&scene.fogParameters,&scene.waterBase,&scene.waterMinimum,&scene.waterAbsorb,&scene.parameters,&scene.splatScales,&scene.splatMultipliers,&scene.mapDimensions})
			for (size_t i=0;i<4;++i) Require(std::isfinite((*vector)[i]),"Non-finite actual scene material/lighting input");
		Require(simd_length(scene.sun.xyz)>0 && scene.fogParameters.w>0,"Original sun/fog input unavailable");
		prepPhase->Mark(6);
		id<MTLBuffer> sceneBuffer = Buffer(lease.Device(),&scene,sizeof(scene),@"Original sun/fog/water/material/shadow state"); lease.AddAllocation(sceneBuffer);
		id<MTLTexture> brdf = impl->Texture(lease,frame.featureBRDF);
		for(const auto& owner:{terrainMaterial.detail,terrainMaterial.specular,terrainMaterial.blendNormals,terrainMaterial.emission,terrainMaterial.splatDetail,terrainMaterial.splatDistribution})if(owner)sources.insert(owner->sourcePath);
		for(const auto& owner:terrainMaterial.splatNormals)if(owner)sources.insert(owner->sourcePath);
		result.materialSources.assign(sources.begin(),sources.end()); result.materialDependenciesPending.assign(pending.begin(),pending.end());
		impl->Pipelines(lease, layer.pixelFormat, options.shaderValidation, result.shaderCompileMs);
		for (id object : {id(impl->unitPipeline),id(impl->engineModelPipeline),id(impl->engineStandardShadowPipeline),id(impl->alphaPipeline),id(impl->shadowModelPipeline),id(impl->shadowTerrainPipeline),id(impl->normalPipeline),id(impl->waterUnitPipeline),id(impl->waterEnginePipeline),id(impl->waterColorCopyPipeline),id(impl->logicalColorCopyPipeline),id(impl->logicalDepthCopyPipeline),id(impl->library)}) lease.Retain(object);
		for (unsigned presence = 0; presence < 2; ++presence)
			for (id object : {id(impl->placementSamplePipelines[presence]),id(impl->engineStandardPipelines[presence]),id(impl->engineStandardAlphaPipelines[presence]),id(impl->waterStandardPipelines[presence]),id(impl->waterStandardAlphaPipelines[presence])}) lease.Retain(object);
		const MapEdgeImageProvider mapEdgeImages = [&](RuntimeLease& imageLease,const std::shared_ptr<const WorldTexture>& owner) {
			CUSSourceImage image;
			image.texture=impl->Texture(imageLease,owner);image.owner=owner;image.flipV=owner->flipV;
			ScreenSampler sampler;sampler.filter=ScreenFilter::Linear;sampler.magnificationFilter=ScreenFilter::Linear;
			sampler.wrapS=sampler.wrapT=ScreenWrap::Repeat;
			sampler.mipFilter=owner->mipmappedSampling?ScreenMipFilter::Linear:ScreenMipFilter::None;
			image.sampler=impl->screenGPU.PrepareSampler(imageLease,sampler);imageLease.Retain(image.sampler);
			const auto status=impl->textures.at(owner.get()).status;
			// The frame's shared mip producer runs before all world draws. Keep
			// its completion contract instead of adding a private upload/submit.
			image.requireReadable=[status,sourceSubmission]{Require(status&&(status->ready||status->encodedSubmission==sourceSubmission),
				"Map-edge original mip producer has not executed before its consumer");};
			return image;
		};
		prepPhase->Mark(7);
		auto mapEdge=impl->mapEdgeGPU.Prepare(lease,frame,layer.pixelFormat,options.shaderValidation,result.shaderCompileMs,mapEdgeImages,mainSamples);
		if(mapEdge)result.mapEdgeInstances=mapEdge->MirrorInstances();
		MTLDepthStencilDescriptor* depthDescriptor = [MTLDepthStencilDescriptor new]; depthDescriptor.depthCompareFunction = MTLCompareFunctionLessEqual; depthDescriptor.depthWriteEnabled = YES;
		id<MTLDepthStencilState> depthState = [lease.Device() newDepthStencilStateWithDescriptor:depthDescriptor];
		Require(depthState != nil, "Cannot create experimental world depth state"); lease.Retain(depthState);
		id<MTLDepthStencilState> logicalCopyDepthState=nil;
		if(logicalCopyMapping&&!logicalCopyMapping->unitScale){
			auto descriptor=[MTLDepthStencilDescriptor new];descriptor.depthCompareFunction=MTLCompareFunctionAlways;descriptor.depthWriteEnabled=YES;
			logicalCopyDepthState=[lease.Device() newDepthStencilStateWithDescriptor:descriptor];
			Require(logicalCopyDepthState!=nil,"Cannot create logical depth copy write state");lease.Retain(logicalCopyDepthState);
		}
		NSError* error = nil;
		MTL4ArgumentTableDescriptor* argumentDescriptor = [MTL4ArgumentTableDescriptor new];
		argumentDescriptor.maxBufferBindCount = 6; argumentDescriptor.maxTextureBindCount = 17; argumentDescriptor.maxSamplerStateBindCount = 9; argumentDescriptor.initializeBindings = YES;
		id<MTL4ArgumentTable> arguments = [lease.Device() newArgumentTableWithDescriptor:argumentDescriptor error:&error];
		Require(arguments != nil, "Cannot create world Metal4 argument table: " + Error(error)); lease.Retain(arguments);
		if(impl->shadowSampler==nil){MTLSamplerDescriptor* sampler=[MTLSamplerDescriptor new];sampler.minFilter=sampler.magFilter=MTLSamplerMinMagFilterLinear;
			sampler.sAddressMode=sampler.tAddressMode=MTLSamplerAddressModeClampToBorderColor;sampler.borderColor=MTLSamplerBorderColorOpaqueWhite;
			sampler.compareFunction=MTLCompareFunctionLessEqual;sampler.supportArgumentBuffers=YES;impl->shadowSampler=[lease.Device() newSamplerStateWithDescriptor:sampler];
			Require(impl->shadowSampler!=nil,"Cannot create original white-border shadow comparison sampler");}
		lease.Retain(impl->shadowSampler);[arguments setSamplerState:impl->shadowSampler.gpuResourceID atIndex:0];
  const auto bindModelSource=[&](const Impl::DrawPiece& piece){
   Require(piece.colorSampler&&piece.otherSampler&&piece.requireTextures,"Original model source samplers/producer owner missing");piece.requireTextures(sourceSubmission);
   [arguments setSamplerState:piece.colorSampler.gpuResourceID atIndex:7];[arguments setSamplerState:piece.otherSampler.gpuResourceID atIndex:8];
  };
		MTLDepthStencilDescriptor* alphaDepthDescriptor = [MTLDepthStencilDescriptor new]; alphaDepthDescriptor.depthCompareFunction = MTLCompareFunctionLessEqual; alphaDepthDescriptor.depthWriteEnabled = NO;
		id<MTLDepthStencilState> alphaDepthState = [lease.Device() newDepthStencilStateWithDescriptor:alphaDepthDescriptor]; Require(alphaDepthState != nil,"Cannot create original alpha depth state"); lease.Retain(alphaDepthState);
		std::unique_ptr<PreparedWorldParticles> particles, luaNano;
  struct WaterLuaGPU {
   std::unique_ptr<PreparedWorldParticles> nano;
   std::unique_ptr<PreparedProjectileEffects> projectile;
   std::unique_ptr<PreparedEnergyExplosion> energy;
   std::unique_ptr<PreparedLightningEffects> lightning;
   std::unique_ptr<PreparedFireSmoke> fireSmoke;
   std::unique_ptr<PreparedSSAO> ssao;
   std::vector<std::pair<uint64_t,std::function<void(id<MTL4RenderCommandEncoder>)>>> draws;
   std::vector<std::pair<uint64_t,std::function<void(id<MTL4CommandBuffer>)>>> commands;
   std::shared_ptr<const NativeOpaqueModelFrame> alphaInvocation166;
   size_t alphaPhase166=0;
   uint64_t alphaSubmission166=0;
   mutable bool alphaDependencies166=false,alphaRetired166=false;
   AlphaCallbackExecution166 InspectExecution166(const NativeOpaqueModelFrame* invocation,size_t phase,
    uint64_t frame,uint64_t submission)const {
    Require(alphaInvocation166&&invocation==alphaInvocation166.get()&&phase==alphaPhase166&&
     phase<invocation->passes.size()&&invocation->alphaStage&&invocation->passes[phase].luaEffects&&
     frame==invocation->sourceDrawFrame&&submission&&submission==alphaSubmission166&&
     !alphaDependencies166&&!alphaRetired166,"Alpha typed inspection lost its actual invocation/phase/frame/submission or lifecycle");
    // SSAO noop can still copy/read/publish. Generic functions and every
    // existing typed family retain Unknown even with no indexed draws.
    if(ssao||!commands.empty())return {SourceDependencyWork160::Unknown,AlphaCallbackBody166::OrderedCommands,false,false};
    if(nano||projectile||energy||lightning||fireSmoke||!draws.empty())return {};
    return {SourceDependencyWork160::NoCommands,AlphaCallbackBody166::CPULifecycleOnly,true,true};
   }
   void Dependencies(id<MTL4CommandBuffer> command)const {
    if(alphaInvocation166)Require(command&&!alphaDependencies166&&!alphaRetired166,"Alpha typed dependencies repeated or retired");
    if(nano)nano->EncodeDependencies(command);if(projectile)projectile->EncodeDependencies(command);
    if(energy)energy->EncodeDependencies(command);if(lightning)lightning->EncodeDependencies(command);
    if(alphaInvocation166)alphaDependencies166=true;
   }
   void Encode(id<MTL4RenderCommandEncoder> encoder)const {Require(commands.empty()&&!ssao,"Original command-level callback requires the common command-buffer scheduler");for(const auto& draw:draws)draw.second(encoder);}
   // Merge command-level postprocessing with ordinary draws by the original
   // shared ordinal. All resumed passes Load/Store the SAME scene attachments.
   void Encode(id<MTL4CommandBuffer> command,id<MTL4RenderCommandEncoder> __strong& encoder,
    MTL4RenderPassDescriptor* loadedScene,bool* borrowedScene218=nullptr)const {
    size_t regular=0,effect=0,phase=0;
    constexpr uint64_t absent=std::numeric_limits<uint64_t>::max();
    while(regular<draws.size()||(ssao&&effect<ssao->DrawCount())||phase<commands.size()){
     const uint64_t r=regular<draws.size()?draws[regular].first:absent;
     const uint64_t s=ssao&&effect<ssao->DrawCount()?ssao->SequenceAt(effect):absent;
     const uint64_t c=phase<commands.size()?commands[phase].first:absent;
     Require((r==absent||r!=s)&&(r==absent||r!=c)&&(s==absent||s!=c),"Original ordinary/postprocess/MRT source ordinal duplicated");
     const bool commandLevel=std::min(s,c)<r;
     if(commandLevel){
      if(encoder){[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];encoder=nil;}
      if(c<s)commands[phase++].second(command);
      else if(borrowedScene218&&effect+1==ssao->DrawCount()&&regular==draws.size()&&phase==commands.size()&&
       ssao->CanEncodeSceneDraw218(effect,loadedScene)){
       // Only the main above-water caller supplies this entitlement, after
       // proving its immediate engine particle and the entire ordinary suffix.
       // The scheduler opens/owns the pass; SSAO borrows its first draw only.
       encoder=[command renderCommandEncoderWithDescriptor:loadedScene];Require(encoder!=nil,"Cannot open scheduler-owned SSAO scene pass");
       *borrowedScene218=true;
       ssao->EncodeSceneDraw218(effect++,encoder);
       // Preserve the original terminal resume's earlier-producer wait,
       // including Dispatch. Same-pass safety is supplied by the caller proof.
       [encoder barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
      }else ssao->EncodeDraw(effect++,command);
     }else{
      if(encoder==nil){encoder=[command renderCommandEncoderWithDescriptor:loadedScene];Require(encoder!=nil,"Cannot resume original callback target after command-level phase");[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];}
      draws[regular++].second(encoder);
     }
    }
    if(encoder==nil){encoder=[command renderCommandEncoderWithDescriptor:loadedScene];Require(encoder!=nil,"Cannot resume original scene after its actual callback commands");[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];}
   }
   void Complete()const {
    if(alphaInvocation166){Require(alphaDependencies166&&!alphaRetired166,"Alpha typed completion before dependencies or repeated");alphaRetired166=true;}
    if(ssao)ssao->DidComplete();if(nano)nano->DidComplete();if(projectile)projectile->DidComplete();if(lightning)lightning->DidComplete();
   }
  };
  struct TerrainSceneGPU {
   std::shared_ptr<const NativeTerrainSceneSourceFrame> source;
   CUSSourceTarget target;
   std::vector<NativeWorldAttachmentClear> initialClearSource,clearSource;
   std::shared_ptr<PreparedTerrainGroundClear> initialClear,clears;
   std::vector<NativeWorldAttachmentClear> mapClearSource;
   std::shared_ptr<PreparedCUSDeferred> mapMRT;
   std::shared_ptr<PreparedTerrainSource> terrain;
   std::shared_ptr<PreparedCUSSource> cus;
   std::shared_ptr<PreparedWorldOverlayFrame> overlays;
   std::shared_ptr<WaterLuaGPU> ordered=std::make_shared<WaterLuaGPU>();
  };
  std::array<TerrainSceneGPU,2> groundScenes;
  Require(frame.mainGround&&environmentFrame.sourceCubeUpdate&&environmentFrame.reflectionGround,
   "Original main/IBL terrain caller frames are missing; cached tile drawing cannot substitute");
  groundScenes[0].source=frame.mainGround;groundScenes[1].source=environmentFrame.reflectionGround;
  auto& mainGround=groundScenes[0];auto& reflectionGround=groundScenes[1];
  mainGround.target.colors[0]=layer.pixelFormat;mainGround.target.depth=impl->depth.pixelFormat;
  mainGround.target.stencil=impl->stencil.pixelFormat;mainGround.target.samples=mainSamples;
  mainGround.target.width=result.surface.renderWidth;mainGround.target.height=result.surface.renderHeight;
  mainGround.target.logicalWidth=logicalWidth;mainGround.target.logicalHeight=logicalHeight;mainGround.target.camera=mainGround.source->viewCamera;
  const auto reflectionPass=environment->ReflectionPass();
  reflectionGround.target.colors[0]=reflectionPass.colorAttachments[0].texture.pixelFormat;
  reflectionGround.target.depth=reflectionPass.depthAttachment.texture.pixelFormat;reflectionGround.target.stencil=MTLPixelFormatInvalid;
  reflectionGround.target.width=uint32_t(reflectionPass.renderTargetWidth);reflectionGround.target.height=uint32_t(reflectionPass.renderTargetHeight);
  reflectionGround.target.logicalWidth=reflectionGround.target.width;reflectionGround.target.logicalHeight=reflectionGround.target.height;
  reflectionGround.target.camera=reflectionGround.source->viewCamera;
  Require(mainGround.source->stage==NativeWorldDrawStage::Ground&&mainGround.source->viewCamera.view==frame.viewCamera.view&&
   mainGround.source->viewCamera.projection==frame.viewCamera.projection&&mainGround.source->viewCamera.viewport==frame.viewCamera.viewport,
   "Original main terrain camera changed before its world consumer");
  Require(reflectionGround.source->stage==NativeWorldDrawStage::TerrainReflection&&
   reflectionGround.source->reflectionFace==environmentFrame.sourceCubeUpdate->face&&
   reflectionGround.source->skyOnly==environment->ReflectionSkyOnly()&&
   reflectionGround.source->viewCamera.view==environment->ReflectionCamera().view&&
   reflectionGround.source->viewCamera.projection==environment->ReflectionCamera().projection,
   "Original IBL terrain does not belong to the actual selected face/camera");
  struct WaterViewGPU {
   const NativeWaterSceneView* source=nullptr;
   __strong id<MTLTexture> color=nil,depth=nil;
   __strong id<MTLBuffer> scene=nil,modelScene=nil,legacyModelScene=nil;
   uint32_t sky=0;
   std::unique_ptr<PreparedWorldParticles> particles;
   std::shared_ptr<PreparedWorldOverlayFrame> overlays;
			std::shared_ptr<PreparedOpaqueModelSource> opaqueModels,alphaModels;
			std::vector<std::shared_ptr<WaterLuaGPU>> alphaCallbacks;
   std::shared_ptr<PreparedTerrainSource> groundTerrain;
   std::shared_ptr<PreparedCUSSource> groundCUS;
   std::shared_ptr<PreparedTerrainGroundClear> groundClears;
   std::shared_ptr<PreparedTerrainGroundClear> initialClear;
   std::shared_ptr<WaterLuaGPU> groundLua=std::make_shared<WaterLuaGPU>();
   std::vector<NativeWorldAttachmentClear> groundClearSource;
   std::vector<NativeWorldAttachmentClear> initialClearSource;
   WaterLuaGPU preParticlesLua,worldLua;
  };
  std::array<WaterViewGPU,2> waterViews;
  WaterLuaGPU belowWaterLua,aboveWaterLua,alphaAllLua,waterPostLua,shadowLua;
  std::shared_ptr<PreparedWorldOverlayFrame> shadowLuaOverlays;
  // Fixed owner slots: map MRT, transparent shadow, planar refraction/reflection,
  // main ground, IBL ground. Only immutable line packets run early; sampled-image
  // and other effect dependencies keep their existing original scheduling.
  std::array<std::shared_ptr<PreparedWorldOverlayFrame>,6> separateLineOwners;
  id<MTLBuffer> belowWaterScene=nil,aboveWaterScene=nil;
  if(waterActive){
   SceneUniform below=scene;below.modelClipPlane={0,-1,0,0};
   SceneUniform above=scene;above.modelClipPlane={0,1,0,0};
   belowWaterScene=Buffer(lease.Device(),&below,sizeof(below),@"Original below-water model plane3");lease.AddAllocation(belowWaterScene);
   aboveWaterScene=Buffer(lease.Device(),&above,sizeof(above),@"Original above-water model plane3");lease.AddAllocation(aboveWaterScene);
  }
  if(bumpWaterActive){
   const auto& water=*frame.bumpWater;
   Require(logicalCopyMapping&&water.camera.viewport==frame.viewCamera.viewport&&
    water.logicalWidth==logicalWidth&&water.logicalHeight==logicalHeight&&
    water.screenTextureWidth==logicalCopyMapping->width&&water.screenTextureHeight==logicalCopyMapping->height&&
    water.material&&water.material->shaderViewport==water.camera.viewport,
    "Original water copy allocation/camera requires its same-frame logical resize reload");
   allocateTarget(impl->preWaterColor,MTLPixelFormatRGBA8Unorm,water.screenTextureWidth,water.screenTextureHeight,MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite);
   allocateTarget(impl->preWaterDepth,MTLPixelFormatDepth32Float,water.screenTextureWidth,water.screenTextureHeight,MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget);
  }
  if(legacyWaterActive&&frame.legacyWater->refraction){
   const auto& ref=*frame.legacyWater->refraction;
   Require(logicalCopyMapping&&logicalCopyMapping->unitScale,"Refractive fragment window mapping currently requires actual 1:1 render extent");
   Require(ref.copyViewport==frame.viewCamera.viewport,"Refractive copy viewport differs from actual main source viewport");
   Require(int64_t(crop[0])==ref.copyViewport[0]&&int64_t(result.surface.renderHeight)-crop[1]-crop[3]==ref.copyViewport[1],"Refractive physical window origin differs from original fragment.position");
   // Refractive copy owns a retained image per source content version, not Water4 scratch storage.
  }
  if(bumpWaterActive || legacyWaterActive || dynamicWaterActive){
   for(size_t i=0;i<waterViews.size();++i){
    auto& target=waterViews[i];target.source=(i==0?frame.waterScene->refraction:frame.waterScene->reflection).get();
    if(!target.source)continue;
    const bool reflected=i==1;
    auto& color=reflected?impl->waterReflection:impl->waterRefraction;
    auto& depth=reflected?impl->waterReflectionDepth:impl->waterRefractionDepth;
    uint32_t width=dynamicWaterActive?(reflected?512:frame.dynamicWater->material->refractionExtent):legacyWaterActive?512:(reflected?frame.bumpWater->settings.reflectionSize:frame.bumpWater->screenTextureWidth);
    uint32_t height=dynamicWaterActive?width:legacyWaterActive?512:(reflected?frame.bumpWater->settings.reflectionSize:frame.bumpWater->screenTextureHeight);
    allocateTarget(color,MTLPixelFormatRGBA8Unorm,width,height,MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget);
    allocateTarget(depth,MTLPixelFormatDepth32Float,width,height,MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget);
    // Original BumpWater FBOs own color and depth only.
    target.color=color;target.depth=depth;
    const auto& view=*target.source;
    SceneUniform viewScene=scene;viewScene.cameraPosition=Vec(view.frame.viewCamera.position);
    viewScene.modelClipPlane=Vec(view.modelClipPlane);viewScene.terrainClipPlane=Vec(view.terrainClipPlane);
    target.scene=Buffer(lease.Device(),&viewScene,sizeof(viewScene),@"Original planar water scene planes");lease.AddAllocation(target.scene);
    // Engine GL4 and CUS use the pre-world cameraViewInv/lighting UBO even
    // when reflectionViewProj determines positions. Legacy models read the
    // active camera and live (temporarily tinted) refraction lighting.
    SceneUniform models=scene;models.modelClipPlane=Vec(view.modelClipPlane);models.terrainClipPlane=Vec(view.terrainClipPlane);
    target.modelScene=Buffer(lease.Device(),&models,sizeof(models),@"Original water model player-camera UBO lighting");lease.AddAllocation(target.modelScene);
    SceneUniform legacy=viewScene;legacy.modelAmbient=Vec(view.frame.lighting.modelAmbient);legacy.modelDiffuse=Vec(view.frame.lighting.modelDiffuse);
    target.legacyModelScene=Buffer(lease.Device(),&legacy,sizeof(legacy),@"Original water legacy active-camera lighting");lease.AddAllocation(target.legacyModelScene);
    target.sky=environment->PrepareWaterSky(lease,view.frame.viewCamera);
    target.particles=impl->particlesGPU.Prepare(lease,sourceSubmission,*view.frame.particleFrame,view.frame.viewCamera,
     MTLPixelFormatRGBA8Unorm,MTLPixelFormatDepth32Float,nil,options.shaderValidation,result.shaderCompileMs);
   }
  }
		if(frame.particleFrame){
			Require(!needsLogicalParticleDepth || logicalCopyMapping->unitScale || mainGround.source->groundInvoked,"Rescaled soft particles require this frame's actual terrain-only logical depth producer");
			const auto disabledClip0=[](const std::shared_ptr<const NativeWaterLuaFrame>& bundle){return bundle&&bundle->ssao&&!bundle->ssao->draws.empty()&&(bundle->ssao->exitClipDisableMask&1u);};
   uint32_t clipDisabled=0;if(disabledClip0(frame.alphaAllLua))clipDisabled|=1u<<unsigned(NativeWorldParticleStage::AlphaAll);if(disabledClip0(frame.waterScene?frame.waterScene->aboveWaterLua:nullptr))clipDisabled|=1u<<unsigned(NativeWorldParticleStage::AlphaAboveWater);
   particles=impl->particlesGPU.Prepare(lease,sourceSubmission,*frame.particleFrame,frame.viewCamera,layer.pixelFormat,MTLPixelFormatDepth32Float,particleTerrainDepth,options.shaderValidation,result.shaderCompileMs,clipDisabled,mainSamples);
			result.engineNanoInstances=uint32_t(frame.particleFrame->engineNano.size());
		}
		if(frame.luaNanoFrame){
			luaNano=impl->particlesGPU.PrepareLuaNano(lease,*frame.luaNanoFrame,frame.viewCamera,float(frame.simulationFrame)+frame.interpolationOffset,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,options.shaderValidation,result.shaderCompileMs,mainSamples);
			for(const auto& draw:frame.luaNanoFrame->draws)result.luaNanoInstances+=draw.instanceCount;
			if (std::any_of(frame.luaNanoFrame->draws.begin(),frame.luaNanoFrame->draws.end(),
				[](const auto& draw){return draw.instanceCount && draw.ghostStencilPass;}))
				pending.insert("Lua nano ghost-stencil 0x40 reads selected; original ghost-shape writer parity remains unverified");
			pending.insert(frame.luaNanoFrame->unsupported.begin(),frame.luaNanoFrame->unsupported.end());
		}
		result.materialDependenciesPending.assign(pending.begin(),pending.end());
		std::unique_ptr<PreparedModelDepth> modelGBuffer;
		std::unique_ptr<PreparedShield> shield;
		if(frame.shieldDepthFrame){
			const auto& depthFrame=*frame.shieldDepthFrame;
			Require(impl->shieldPending==frame.shieldDepthFrame&&depthFrame.sourceDrawFrame==frame.sourceDrawFrame,"Shield depth plan is not this actual captured world draw");
			const auto previousImage=[](const NativeShieldDepthVersion& version)->const ShieldDepthImage*{
				if(!version.previous||!version.previous->backing)return nullptr;
				auto backing=std::dynamic_pointer_cast<const NativeShieldDepthBacking>(version.previous->backing);
				return backing?&GetShieldDepthBackingImage(*backing):nullptr;
			};
   Require(!depthFrame.map.Changed()&&depthFrame.map.previous==depthFrame.map.published&&depthFrame.map.backing,
    "Original map summary must borrow the retained actual phase backing without a second writer");
			if(frame.opaqueModelFrame){
				Require(frame.opaqueModelFrame->sourceDrawFrame==frame.sourceDrawFrame&&depthFrame.models.backing&&
					depthFrame.models.published&&depthFrame.models.previous==depthFrame.models.published,
					"Actual MRT model depth was not borrowed from its retained original source object");
				Require(depthFrame.models.published->backing==depthFrame.models.backing&&
					depthFrame.models.backing->Kind()==NativeTextureBackingKind::ModelDepth,
					"Actual MRT summary and model depth object belong to different backings");
			}else{
			std::vector<ModelDepthDraw> modelDraws;std::vector<ModelDepthPhase> modelPhases;
			std::map<std::tuple<bool,uint32_t,uint32_t>,const Impl::DrawPiece*> pieceIndex;
			for(const auto& piece:pieces)if(piece.view==NativeWaterScenePass::Main&&!piece.projectile&&piece.visible&&!piece.alpha)
				Require(pieceIndex.emplace(std::make_tuple(piece.feature,piece.sourceObject,piece.sourcePiece),&piece).second,"Duplicate actual model depth piece identity");
			for(const auto& phase:depthFrame.modelPlan.phases){
				const size_t first=modelDraws.size();
				for(const auto& object:depthFrame.modelPlan.objects)if(object.feature==phase.feature)for(auto pieceId:object.pieces){
					const auto found=pieceIndex.find({object.feature,uint32_t(object.objectId),pieceId});Require(found!=pieceIndex.end(),"Original deferred depth piece differs from actual main geometry");const auto& piece=*found->second;
					modelDraws.push_back({piece.geometry->vertices,piece.geometry->indices,uniformBuffer,sceneBuffer,piece.uniformOffset,piece.geometry->count,piece.other,object.alpha,object.feature,false,true});
				}
				modelPhases.push_back({phase.feature,first,modelDraws.size()-first,phase.clearBefore});
			}
			const auto& modelNames=OriginalNativeTextureContext().Geometry(NativeGeometryTextureRole::Models);
			ModelDepthSource source{modelPhases,modelNames.Name(5)!=0,modelNames.Width(),modelNames.Height(),previousImage(depthFrame.models)};
			modelGBuffer=impl->modelDepthGPU.Prepare(lease,frame.sourceDrawFrame,frame.viewCamera,result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
				modelDraws,impl->library,options.shaderValidation,result.shaderCompileMs,&source,depthFrame.submissionIdentity);
			if(depthFrame.models.Changed()){Require(modelGBuffer&&depthFrame.models.backing,"Published model depth has no actual native producer");FulfillShieldDepthBacking(*depthFrame.models.backing,modelGBuffer->Image());}
			pending.insert(depthFrame.modelPlan.pendingOriginalGeometry.begin(),depthFrame.modelPlan.pendingOriginalGeometry.end());
			}
		}

		Require(frame.mapTextures && frame.mapTextures->sourceDrawFrame==frame.sourceDrawFrame,"Original map publication is not this source draw");
		impl->normalsEncoded=std::make_shared<bool>(impl->normalsReady);
		const auto normalObject=frame.mapTextures->images[0];
		auto normalBacking=normalObject?std::dynamic_pointer_cast<const NativeMapTextureBacking>(normalObject->backing):nullptr;
		Require(normalBacking && normalBacking->heights==frame.terrain.heights,"Original normal texture and terrain do not share the retained height cache");
		if(!normalBacking->Resource()){auto actual=std::make_shared<Impl::MapImage>();actual->texture=impl->terrainNormals;actual->completed=impl->normalsReady;normalBacking->Fulfill(std::move(actual));}
		else {auto actual=std::dynamic_pointer_cast<const Impl::MapImage>(normalBacking->Resource());Require(actual && actual->texture==impl->terrainNormals,"Original normal allocation was replaced without a new source version");}
		// Original height is a retained R32F corner image; reuse the minimap
		// owner's upload independently of terrain shading selection.
		const auto heightObject=frame.mapTextures->images[1];
		auto heightBacking=heightObject?std::dynamic_pointer_cast<const NativeMapTextureBacking>(heightObject->backing):nullptr;
		Require(heightBacking && heightBacking->role==NativeMapTextureRole::Height && heightBacking->heights==frame.terrain.heights,"Original height version does not share the retained terrain source");
		if(!heightBacking->Resource()){
			auto height=impl->standardTerrainGPU->PrepareHeight(lease,heightBacking->heights);
			auto actual=std::make_shared<Impl::MapImage>();actual->heightOwner=height;actual->texture=height->texture;actual->completed=true;heightBacking->Fulfill(std::move(actual));
		}else {
			auto actual=std::dynamic_pointer_cast<const Impl::MapImage>(heightBacking->Resource());
			Require(actual && actual->heightOwner && actual->heightOwner->source==heightBacking->heights && actual->texture==actual->heightOwner->texture && actual->Completed(),"Original height backing lost its actual retained source/resource owner");
			lease.AddAllocation(actual->texture);
		}
			const auto sourceGeometry=[&](RuntimeLease& sourceLease,std::shared_ptr<const WorldModel> owner){
				auto& actual=impl->Model(sourceLease,owner);std::vector<EngineDeferredPieceBuffers> buffers;
				buffers.reserve(actual.pieces.size());
				for(const auto& piece:actual.pieces){
					if(piece.vertices)sourceLease.AddAllocation(piece.vertices);if(piece.indices)sourceLease.AddAllocation(piece.indices);
					buffers.push_back({piece.vertices,piece.indices,piece.count});
				}return buffers;
			};
			const auto sourceImages=[&](RuntimeLease& sourceLease,const NativeCUSTextureBinding& binding,unsigned){
				return impl->OriginalModelImage(sourceLease,binding,sourceSubmission,options.shaderValidation,result.shaderCompileMs);
			};
  std::vector<NativeWorldAttachmentClear> shadowClears;
  if(frame.luaWorldDrawFrame){
   for(const auto& clear:frame.luaWorldDrawFrame->clears){
    if(clear.stage==NativeWorldDrawStage::ShadowOpaque){shadowClears.push_back(clear);continue;}
    bool routed=false;
    for(auto& ground:groundScenes)if(clear.stage==ground.source->stage) {
     if(clear.sequence==ground.source->initialClearSequence){
      Require(!routed&&ground.initialClearSource.empty(),"Original main/IBL initial clear is repeated");
      ground.initialClearSource.push_back(clear);routed=true;continue;
     }
     if(ground.source->ground&&ground.source->ground->source.Contains(clear.sequence)){
      Require(!routed,"Original clear belongs to multiple terrain invocations");if(ground.source->ground->deferred&&ground.source->ground->deferred->Contains(clear.sequence))ground.mapClearSource.push_back(clear);
      else ground.clearSource.push_back(clear);routed=true;
     }
    }
    for(auto& view:waterViews)if(view.source){
     const auto stage=view.source->pass==NativeWaterScenePass::Reflection?NativeWorldDrawStage::WaterReflection:NativeWorldDrawStage::WaterRefraction;
     if(clear.stage==stage&&clear.sequence==view.source->initialClearSequence){
      Require(!routed&&view.initialClearSource.empty(),"Original water initial clear is repeated");
      view.initialClearSource.push_back(clear);routed=true;continue;
     }
     if(!view.source->groundSource)continue;
     const auto& ground=*view.source->groundSource;
     if(clear.stage==ground.stage&&ground.source.Contains(clear.sequence)){
      Require(!routed,"Original attachment clear belongs to more than one water ground source");
      view.groundClearSource.push_back(clear);routed=true;
     }
    }
    Require(routed,"Common attachment clear has no selected original target/source span consumer");
   }
  }
		prepPhase->Mark(8);preparationDetail.Mark(0);
  if(frame.originalShadow){
   // Opaque ShadowGen has no color output in the supported default path.
   // RGB8 is physically padded; arbitrary Lua color/blend output needs its
   // exact original RGB attachment semantics before it may be accepted.
   for(const auto& draw:frame.originalShadow->cus->draws)Require(!draw.colorMask[0]&&!draw.colorMask[1]&&!draw.colorMask[2]&&!draw.colorMask[3],
    "Original opaque shadow CUS color writer has no exact named RGB/R target consumer");
   // Typed Lua families retain their real callback source. Their existing
   // world consumers cannot silently substitute for a named RGB/R shadow
   // target: each nonempty family needs its selected shadow program contract.
   if(const auto& lua=frame.originalShadow->transparentLua){
    Require(!lua->nano||lua->nano->draws.empty(),"Original shadow Lua nano needs its actual named-target consumer");
    Require(!lua->projectile||lua->projectile->draws.empty(),"Original shadow Lua projectile effects need their actual named-target consumer");
    Require(!lua->energy||lua->energy->draws.empty(),"Original shadow Lua energy needs its actual named-target consumer");
    Require(!lua->lightning||lua->lightning->draws.empty(),"Original shadow Lua lightning needs its actual named-target consumer");
    Require(!lua->fireSmoke||lua->fireSmoke->draws.empty(),"Original shadow Lua fire/smoke needs its actual named-target consumer");
    Require(!lua->ssao||lua->ssao->draws.empty(),"Original shadow Lua SSAO needs its actual named-target command consumer");
   }
   originalShadow=PrepareOriginalShadowSource(lease,frame.originalShadow,frame.luaWorldDrawFrame,impl->cusSourceGPU,impl->originalShadowGPU,impl->originalShadowParticlesGPU,
    sourceGeometry,sourceImages,shadowClears,frame.sourceDrawFrame,sourceSubmission,options.shaderValidation,result.shaderCompileMs);
  }else {
   Require(shadowClears.empty(),"Common shadow clears lost their original named source frame");
   if(frame.luaWorldDrawFrame)for(const auto& draw:frame.luaWorldDrawFrame->draws)Require(draw.stage!=NativeWorldDrawStage::ShadowOpaque,
    "Common shadow geometry lost its original named source frame");
  }
		prepPhase->Mark(9);preparationDetail.Mark(1);
  struct ShadowEvent {uint64_t sequence;size_t index;bool clear;};
  std::vector<ShadowEvent> shadowEvents;
  if(originalShadow){
   for(size_t n=0;n<originalShadow->DrawCount();++n)shadowEvents.push_back({originalShadow->SequenceAt(n),n,false});
   for(size_t n=0;n<shadowClears.size();++n)shadowEvents.push_back({shadowClears[n].sequence,n,true});
   std::sort(shadowEvents.begin(),shadowEvents.end(),[](const auto& a,const auto& b){return a.sequence<b.sequence;});
   auto previous=originalShadow->BeginSequence();
   for(const auto& event:shadowEvents){Require(event.sequence>previous&&event.sequence<originalShadow->EndSequence(),
    "Original common shadow clear/draw ordinal is repeated or outside its actual source scope");previous=event.sequence;}
   Require(!shadowEvents.empty()&&shadowEvents.front().clear&&(shadowClears[shadowEvents.front().index].mask&ScreenAttachmentDepth),
    "Original shadow destination has no initial actual depth clear before its draws");
  }
		prepPhase->Mark(10);
  // Prepare the actual map MRT before model/screen/SSAO consumers inspect its
  // published versions. This reuses the existing common MRT target owner.
  // One invocation-local source index; no validation receipt or cross-frame cache.
  static const bool worldPlanDiagnostic167=[] {const char* value=std::getenv("BAR_NATIVE_WORLD_PLAN167_DIAGNOSTIC");return value&&std::string(value)=="1";}();
  static unsigned worldPlanDiagnosticSamples167=0;
  const bool timeWorldPlan167=worldPlanDiagnostic167&&worldPlanDiagnosticSamples167<32&&frame.sourceDrawFrame%16==0;
  if(timeWorldPlan167)++worldPlanDiagnosticSamples167;
  NativeWorldDrawPlan167 preparedWorld167(frame.luaWorldDrawFrame,frame.sourceDrawFrame,timeWorldPlan167);
  if(const auto& original=mainGround.source->ground;original&&original->mapMRT){
   Require(original->deferred.has_value(),"Original map MRT has no completed source span");
   const auto& phase=original->mapMRT;const auto& span=*original->deferred;
   Require(original->deferredEntered&&span.begin<span.end&&phase->role==NativeGeometryTextureRole::Map&&
    phase->sourceDrawFrame==frame.sourceDrawFrame&&!mainGround.mapClearSource.empty(),
    "Original map MRT lost its selected phase, callback source or initial clear");
   const auto checkTyped=[&](const auto& family){if(family)for(const auto& draw:family->draws)
    Require(!span.Contains(draw.sequence),"Original map MRT typed callback requires its actual five-output source profile");};
   if(const auto& lua=mainGround.source->lua){checkTyped(lua->nano);checkTyped(lua->projectile);checkTyped(lua->energy);
    checkTyped(lua->lightning);checkTyped(lua->fireSmoke);checkTyped(lua->ssao);}
   struct MapSourceOwners {
    std::shared_ptr<const NativeWaterGroundSourceFrame> source;
    std::shared_ptr<PreparedTerrainSource> terrain;
    std::shared_ptr<PreparedTerrainGroundClear> clears;
    std::shared_ptr<PreparedWorldOverlayFrame> generic;
    std::vector<std::shared_ptr<PreparedMapEdge>> mapEdges;
    std::vector<std::pair<uint64_t,std::function<void(id<MTL4RenderCommandEncoder>)>>> ordered;
   };
   auto owners=std::make_shared<MapSourceOwners>();owners->source=original;
   const auto& metadata=phase->depth.backing->Metadata();CUSSourceTarget target;
   target.colors.fill(MTLPixelFormatRGBA8Unorm);target.depth=MTLPixelFormatDepth32Float;target.stencil=MTLPixelFormatInvalid;
   if(phase->combinedColor.Changed())target.combinedColor=MTLPixelFormatRGBA8Unorm;
   target.width=target.logicalWidth=metadata.width;target.height=target.logicalHeight=metadata.height;target.camera=phase->camera;
   target.camera.viewport={0,0,int32_t(metadata.width),int32_t(metadata.height)};
   owners->terrain=impl->terrainSourceGPU.Prepare(lease,original,target,
    [&](RuntimeLease& l,const NativeCUSTextureBinding& b,unsigned,id<MTLSamplerState> sampler){
     return impl->OriginalTerrainImage(l,b,sourceSubmission,sampler,options.shaderValidation,result.shaderCompileMs);
    },options.shaderValidation,result.shaderCompileMs);
   owners->clears=PrepareTerrainGroundClears(lease,mainGround.mapClearSource,target,options.shaderValidation,result.shaderCompileMs);
   const auto generic=preparedWorld167.StageWhere(NativeWorldDrawStage::Ground,[&](const auto& draw){return span.Contains(draw.sequence);});
   owners->generic=impl->overlayGPU.Prepare(lease,generic,impl->screenGPU,nullptr,phase->camera,
    target.width,target.height,target.logicalWidth,target.logicalHeight,target.colors[0],target.depth,target.stencil,
    options.shaderValidation,result.shaderCompileMs,true,target.samples,target.combinedColor!=MTLPixelFormatInvalid);
   separateLineOwners[0]=owners->generic;
   const auto add=[&](const auto& family){if(family)for(size_t n=0;n<family->DrawCount();++n)
    owners->ordered.emplace_back(family->SequenceAt(n),[family,n](id<MTL4RenderCommandEncoder> e){family->EncodeDraw(n,e);});};
   add(owners->terrain);add(owners->clears);add(owners->generic);
   for(const auto& draw:original->mapEdges){
    Require(draw&&draw->mapMRT==phase&&draw->sourceDrawFrame==frame.sourceDrawFrame&&span.Contains(draw->sequence)&&
     original->deferredCallback&&original->deferredCallback->Contains(draw->sequence),
     "Original deferred map-edge belongs to another Map MRT/callback span");
    auto images=[&](RuntimeLease& l,const NativeCUSTextureBinding& binding,unsigned){
     auto sampler=impl->screenGPU.PrepareSampler(l,ui::NativeUITextureHistory::DescribeSampler(binding));
     return impl->OriginalTerrainImage(l,binding,sourceSubmission,sampler,options.shaderValidation,result.shaderCompileMs);
    };
    auto prepared=impl->mapEdgeGPU.PrepareDeferred(lease,draw,target,images,options.shaderValidation,result.shaderCompileMs);
    Require(bool(prepared),"Original deferred map-edge has no paired same-target consumer");
    owners->mapEdges.push_back(prepared);
    owners->ordered.emplace_back(draw->sequence,[prepared](id<MTL4RenderCommandEncoder> e){prepared->Encode(e);});
   }
   std::sort(owners->ordered.begin(),owners->ordered.end(),[](const auto& a,const auto& b){return a.first<b.first;});
   uint64_t previous=span.begin;for(const auto& event:owners->ordered){Require(event.first>previous&&span.Contains(event.first),
    "Original map MRT event ordinal is repeated or outside its actual source phase");previous=event.first;}
   Require(!owners->ordered.empty()&&owners->ordered.front().first==mainGround.mapClearSource.front().sequence,
    "Original map MRT does not start with its actual source clear");
   CUSDeferredOtherDraws other;other.observed=true;other.sourceOrder=true;other.lifetime=owners;
   for(const auto& event:owners->ordered)other.drawSequences.push_back(event.first);
   other.dependencies=[owners](id<MTL4CommandBuffer> c){owners->terrain->EncodeDependencies(c);for(const auto& edge:owners->mapEdges)edge->EncodeDependencies(c);};
   other.drawAt=[owners](size_t n,id<MTL4RenderCommandEncoder> e){owners->ordered.at(n).second(e);};
   other.initialAttachmentClear182=[owners,sourceFrame=frame.sourceDrawFrame,simulationFrame=frame.simulationFrame](MTL4RenderPassDescriptor* pass){
    return owners->clears->TryInitialAttachmentClear182(pass,sourceFrame,simulationFrame);
   };
   other.didEncodeSourceDraws=[owners]{owners->terrain->FinishOrderedDraws();for(const auto& edge:owners->mapEdges)edge->FinishDeferredDraw();};
   other.didComplete=[owners]{owners->terrain->DidComplete();for(const auto& edge:owners->mapEdges)edge->DidComplete();};
   mainGround.mapMRT=PrepareCUSDeferred(lease,impl->cusSourceGPU,phase,sourceImages,std::move(other),
    sourceSubmission,options.shaderValidation,result.shaderCompileMs,&mainGround.mapClearSource.front());
  }

		prepPhase->Mark(11);prepPhase.reset();
		cpuPhase.Mark(1);preparationDetail.Mark(2);
		std::shared_ptr<PreparedOpaqueModelSource> opaqueModels;
		std::array<std::shared_ptr<PreparedOpaqueModelSource>,3> alphaModels;
		std::array<std::vector<std::shared_ptr<WaterLuaGPU>>,3> alphaCallbacks;
		if(frame.opaqueModelFrame){
			CUSSourceTarget actualTarget;actualTarget.colors[0]=layer.pixelFormat;actualTarget.samples=mainSamples;
			actualTarget.depth=impl->depth.pixelFormat;actualTarget.stencil=impl->stencil.pixelFormat;
			actualTarget.width=result.surface.renderWidth;actualTarget.height=result.surface.renderHeight;
			actualTarget.logicalWidth=logicalWidth;actualTarget.logicalHeight=logicalHeight;actualTarget.camera=frame.viewCamera;
			// All six images of EVERY original deferred phase are fulfilled before
			// the screen/Shield/SSAO/Bloom resolver can inspect their snapshots.
   // NativeCUSCPUReadScope counts cached CPU geometry validation entries,
   // not texture readers or nested capture. MRT ownership is checked by209.
			opaqueModels=PrepareOpaqueModelSource(lease,impl->cusSourceGPU,impl->originalDeferredGPU,impl->originalForwardGPU,
				frame.opaqueModelFrame,actualTarget,sourceGeometry,sourceImages,frame.sourceDrawFrame,sourceSubmission,
				options.shaderValidation,result.shaderCompileMs,
    !offscreenSource&&!options.readback&&options.presentWorld?
     &OriginalNativeTextureContext().Registry():nullptr);
		}
		for(size_t slot=0;slot<frame.alphaModelFrames.size();++slot){
			const auto& source=frame.alphaModelFrames[slot];
			const bool required=waterActive?slot<2:slot==2;
			Require(!frame.opaqueModelFrame||bool(source)==required,"Original main alpha source route is missing or duplicated");
			if(!source)continue;
			const auto expected=slot==0?NativeWorldDrawStage::AlphaBelowWater:slot==1?NativeWorldDrawStage::AlphaAboveWater:NativeWorldDrawStage::AlphaAll;
			Require(source->alphaStage&&*source->alphaStage==expected&&!source->drawReflection&&!source->drawRefraction,
				"Original main alpha source belongs to another water half or planar view");
			CUSSourceTarget target;target.colors[0]=layer.pixelFormat;target.samples=mainSamples;target.depth=impl->depth.pixelFormat;target.stencil=impl->stencil.pixelFormat;
			target.width=result.surface.renderWidth;target.height=result.surface.renderHeight;target.logicalWidth=logicalWidth;target.logicalHeight=logicalHeight;target.camera=frame.viewCamera;
			alphaModels[slot]=PrepareOpaqueModelSource(lease,impl->cusSourceGPU,impl->originalDeferredGPU,impl->originalForwardGPU,
				source,target,sourceGeometry,sourceImages,frame.sourceDrawFrame,sourceSubmission,options.shaderValidation,result.shaderCompileMs);
		}
  for(auto& view:waterViews)if(view.source){
   const auto& actualSource=view.source->frame.opaqueModelFrame;
   Require(actualSource&&actualSource->sourceDrawFrame==frame.sourceDrawFrame,
    "Original planar water models lack their actual same-draw source graph");
   for(const auto& phase:actualSource->passes)Require(!phase.deferred&&!phase.mrt&&!phase.postDeferredSequence,
    "Original planar water cannot execute the main-view deferred graph");
   CUSSourceTarget actualTarget;actualTarget.colors[0]=view.color.pixelFormat;
   actualTarget.depth=view.depth.pixelFormat;actualTarget.stencil=MTLPixelFormatInvalid;
   actualTarget.width=uint32_t(view.color.width);actualTarget.height=uint32_t(view.color.height);
   actualTarget.logicalWidth=actualTarget.width;actualTarget.logicalHeight=actualTarget.height;
   actualTarget.camera=view.source->frame.viewCamera;
   Require(view.initialClearSource.size()==1,"Original water target lost its actual initial clear source event");
   view.initialClear=PrepareTerrainGroundClears(lease,view.initialClearSource,actualTarget,options.shaderValidation,result.shaderCompileMs);
   Require(view.source->drawGround==bool(view.source->groundSource),"Original water ground invocation and retained source disagree");
   if(const auto& ground=view.source->groundSource){
    Require(ground->sourceDrawFrame==frame.sourceDrawFrame&&ground->contextGeneration==OriginalNativeTextureContext().Registry().ContextGeneration()&&
     ground->stage==(view.source->pass==NativeWaterScenePass::Reflection?NativeWorldDrawStage::WaterReflection:NativeWorldDrawStage::WaterRefraction),
     "Original water ground source belongs to another draw/context/stage");
    Require(view.source->groundCUS&&view.source->groundLua,"Original water ground lost its same-invocation callback owners");
    view.groundTerrain=impl->terrainSourceGPU.Prepare(lease,ground,actualTarget,
     [&](RuntimeLease& l,const NativeCUSTextureBinding& binding,unsigned,id<MTLSamplerState> sampler){
      return impl->OriginalTerrainImage(l,binding,sourceSubmission,sampler,options.shaderValidation,result.shaderCompileMs);
     },options.shaderValidation,result.shaderCompileMs);
    view.groundCUS=impl->cusSourceGPU.Prepare(lease,view.source->groundCUS,actualTarget,sourceImages,options.shaderValidation,result.shaderCompileMs);
    view.groundClears=PrepareTerrainGroundClears(lease,view.groundClearSource,actualTarget,options.shaderValidation,result.shaderCompileMs);
   }
   view.opaqueModels=PrepareOpaqueModelSource(lease,impl->cusSourceGPU,impl->originalDeferredGPU,impl->originalForwardGPU,
    actualSource,actualTarget,sourceGeometry,sourceImages,frame.sourceDrawFrame,sourceSubmission,options.shaderValidation,result.shaderCompileMs);
			const auto& alpha=view.source->frame.alphaModelFrames[0];
			const auto stage=view.source->pass==NativeWaterScenePass::Reflection?NativeWorldDrawStage::WaterReflection:NativeWorldDrawStage::WaterRefraction;
			Require(alpha&&alpha->alphaStage&&*alpha->alphaStage==stage&&alpha->drawReflection==actualSource->drawReflection&&alpha->drawRefraction==actualSource->drawRefraction,
				"Original planar alpha source lacks its actual same-view callback/model owners");
			view.alphaModels=PrepareOpaqueModelSource(lease,impl->cusSourceGPU,impl->originalDeferredGPU,impl->originalForwardGPU,
				alpha,actualTarget,sourceGeometry,sourceImages,frame.sourceDrawFrame,sourceSubmission,options.shaderValidation,result.shaderCompileMs);
  }
		cpuPhase.Mark(2);preparationDetail.Mark(3);
		std::shared_ptr<metal4::PreparedScreenFrame> screen;
		if (options.screenUI != nullptr) {
			screen = impl->screenGPU.Prepare(lease, *options.screenUI, result.surface.renderWidth, result.surface.renderHeight,
				logicalWidth, logicalHeight, layer.pixelFormat, options.shaderValidation, result.shaderCompileMs,
				MTLPixelFormatDepth32Float, MTLPixelFormatStencil8,mainSamples);
			result.screenDraws = screen->DrawCount();
			result.screenTextureUploads = screen->TextureUploads();
		}
		cpuPhase.Mark(3);preparationDetail.Mark(4);
		std::shared_ptr<PreparedWorldOverlayFrame> overlays;
		if (frame.luaWorldDrawFrame) {
   const auto mainDraws=preparedWorld167.Main();
   overlays = impl->overlayGPU.Prepare(lease, mainDraws, impl->screenGPU, screen.get(), frame.viewCamera,
				result.surface.renderWidth, result.surface.renderHeight, logicalWidth, logicalHeight,
				layer.pixelFormat, MTLPixelFormatDepth32Float, MTLPixelFormatStencil8, options.shaderValidation, result.shaderCompileMs,false,mainSamples,false,
    [&](RuntimeLease& l,const NativeCUSTextureBinding& binding,unsigned){return impl->OriginalModelImage(l,binding,sourceSubmission,options.shaderValidation,result.shaderCompileMs);});
		}
		if(frame.luaWorldDrawFrame && waterActive)for(auto& view:waterViews)if(view.source){
   const auto stage=view.source->pass==NativeWaterScenePass::Reflection?NativeWorldDrawStage::WaterReflection:NativeWorldDrawStage::WaterRefraction;
   const auto draws=preparedWorld167.Stage(stage);
   const auto width=uint32_t(view.color.width),height=uint32_t(view.color.height);
   view.overlays=impl->overlayGPU.Prepare(lease,draws,impl->screenGPU,screen.get(),view.source->frame.viewCamera,
    width,height,width,height,MTLPixelFormatRGBA8Unorm,MTLPixelFormatDepth32Float,MTLPixelFormatInvalid,
    options.shaderValidation,result.shaderCompileMs);
   separateLineOwners[stage==NativeWorldDrawStage::WaterRefraction?2:3]=view.overlays;
  }
  const auto prepareWaterLua=[&](WaterLuaGPU& output,const std::shared_ptr<const NativeWaterLuaFrame>& input,
   const WorldCamera& camera,uint32_t width,uint32_t height,uint32_t logicalWidth,uint32_t logicalHeight,MTLPixelFormat format,MTLPixelFormat stencilFormat=MTLPixelFormatStencil8,unsigned outputSamples=1){
   if(!input)return;
   if(input->nano && !input->nano->draws.empty())output.nano=impl->particlesGPU.PrepareLuaNano(lease,*input->nano,camera,
    float(frame.simulationFrame)+frame.interpolationOffset,format,MTLPixelFormatDepth32Float,stencilFormat,options.shaderValidation,result.shaderCompileMs,outputSamples);
   if(input->projectile && !input->projectile->draws.empty())output.projectile=impl->projectileEffectsGPU.Prepare(lease,*input->projectile,camera,format,MTLPixelFormatDepth32Float,options.shaderValidation,result.shaderCompileMs,outputSamples);
   if(input->energy && !input->energy->draws.empty())output.energy=impl->energyExplosionGPU.Prepare(lease,*input->energy,camera,format,MTLPixelFormatDepth32Float,options.shaderValidation,result.shaderCompileMs,outputSamples);
   if(input->lightning && !input->lightning->draws.empty())output.lightning=impl->lightningGPU.Prepare(lease,*input->lightning,camera,width,height,logicalWidth,logicalHeight,format,MTLPixelFormatDepth32Float,stencilFormat,options.shaderValidation,result.shaderCompileMs,outputSamples);
   if(input->fireSmoke && !input->fireSmoke->draws.empty())output.fireSmoke=impl->waterFireSmokeGPU.Prepare(lease,*input->fireSmoke,camera,width,height,logicalWidth,logicalHeight,impl->screenGPU,screen.get(),format,MTLPixelFormatDepth32Float,options.shaderValidation,result.shaderCompileMs,outputSamples);
   const auto add=[&](auto* family){if(family)for(size_t i=0;i<family->DrawCount();++i)output.draws.emplace_back(family->SequenceAt(i),[family,i](id<MTL4RenderCommandEncoder> encoder){family->EncodeDraw(i,encoder);});};
   add(output.nano.get());add(output.projectile.get());add(output.energy.get());add(output.lightning.get());add(output.fireSmoke.get());
  };
		const auto alphaPhase=[&](const std::shared_ptr<const NativeOpaqueModelFrame>& source,uint64_t sequence)->std::optional<size_t>{
			if(!source)return {};
			for(size_t phase=0;phase<source->passes.size();++phase){
				const auto end=phase+1<source->passes.size()?source->passes[phase+1].sequence:source->endSequence;
				if(source->passes[phase].sequence<sequence&&sequence<end)return phase;
			}return {};
		};
		const auto prepareAlphaCallbacks=[&](const std::shared_ptr<const NativeOpaqueModelFrame>& source,
			std::vector<std::shared_ptr<WaterLuaGPU>>& callbacks,const WorldCamera& camera,
			uint32_t width,uint32_t height,uint32_t logicalWidth,uint32_t logicalHeight,MTLPixelFormat format,MTLPixelFormat stencil,unsigned outputSamples){
			if(!source)return;
			for(size_t phaseIndex=0;phaseIndex<source->passes.size();++phaseIndex){
				const auto& phase=source->passes[phaseIndex];
				Require(phase.luaEffects!=nullptr,"Original alpha callbacks lost their same-invocation typed frame");
				auto output=std::make_shared<WaterLuaGPU>();output->alphaInvocation166=source;output->alphaPhase166=phaseIndex;output->alphaSubmission166=sourceSubmission;
				prepareWaterLua(*output,phase.luaEffects,camera,width,height,logicalWidth,logicalHeight,format,stencil,outputSamples);callbacks.push_back(std::move(output));
			}
		};
		for(size_t slot=0;slot<alphaModels.size();++slot)if(alphaModels[slot])
			prepareAlphaCallbacks(frame.alphaModelFrames[slot],alphaCallbacks[slot],frame.viewCamera,result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,layer.pixelFormat,impl->stencil.pixelFormat,mainSamples);
		for(auto& view:waterViews)if(view.alphaModels)
			prepareAlphaCallbacks(view.source->frame.alphaModelFrames[0],view.alphaCallbacks,view.source->frame.viewCamera,uint32_t(view.color.width),uint32_t(view.color.height),uint32_t(view.color.width),uint32_t(view.color.height),view.color.pixelFormat,MTLPixelFormatInvalid,1);
  const auto resolveDeferredPostprocess=[&](RuntimeLease& sourceLease,const NativeCUSTextureBinding& binding,uint64_t submission)->ResolvedSSAOImage{
    Require(binding.object!=nullptr,"SSAO source image has no observed logical binding");
    if(auto mask=std::dynamic_pointer_cast<const NativeUnitStencilBacking>(binding.object->backing)){
     auto image=impl->unitStencilGPU.Resolve(sourceLease,mask->version,submission);
     return{image.texture,image.sampler,image.owner,image.requireReadable,image.flipV,{}};
    }
    if(std::dynamic_pointer_cast<const NativeDeferredColorBacking>(binding.object->backing)){
     auto image=ResolveDeferredColorImage(sourceLease,binding,submission);
     const auto& actual=GetDeferredColorBackingImage(*std::dynamic_pointer_cast<const NativeDeferredColorBacking>(binding.object->backing));
     return{image.texture,image.sampler,image.owner,image.requireReadable,image.flipV,{actual.producer,actual.producer->operation==ShieldDepthOperation::AllocationOnly}};
    }
    std::shared_ptr<const ScreenTexture> descriptor;NativeDeferredPostprocessEpoch deferredEpoch;
    if(auto backing=std::dynamic_pointer_cast<const ui::NativeScreenTextureBacking>(binding.object->backing))descriptor=backing->image;
    else if(std::dynamic_pointer_cast<const NativeShieldDepthBacking>(binding.object->backing))
     return ResolvePostprocessDepth(sourceLease,binding,submission);
    Require(descriptor!=nullptr,"SSAO source requires its genuine shared image provider");
    auto retained=std::make_shared<std::vector<PreparedScreenExternalImage>>();
    auto image=impl->screenGPU.PrepareImage(sourceLease,descriptor,ShieldScreenSampler(binding.object->sampling),screen.get(),std::nullopt,
     [retained](PreparedScreenExternalImage image){retained->push_back(std::move(image));});
    return{image.texture,image.sampler,descriptor,[retained]{for(const auto& image:*retained)if(image.requireReadable)image.requireReadable();},descriptor->flipV,std::move(deferredEpoch)};
   };
		preparationDetail.Mark(5);
		std::shared_ptr<PreparedDeferredLights> deferredLights;
		if(frame.luaDeferredLightFrame&&!frame.luaDeferredLightFrame->draws.empty()){
			Require(frame.luaDeferredLightFrame->sourceDrawFrame==frame.sourceDrawFrame&&frame.shieldDepthFrame,
				"Original deferred lights lost their actual draw frame/deferred submission");
			const auto submission=frame.shieldDepthFrame->submissionIdentity;
			DeferredLightTarget target{layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,
				result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,frame.viewCamera,mainSamples};
			deferredLights=impl->deferredLightsGPU.Prepare(lease,frame.luaDeferredLightFrame,target,
				[&](RuntimeLease& sourceLease,const NativeCUSTextureBinding& binding,unsigned slot)->DeferredLightImage{
					if(slot==0||slot==1||slot==2||slot==3||slot==6||slot==7){
						auto actual=resolveDeferredPostprocess(sourceLease,binding,submission);
						CUSSourceImage image;image.texture=actual.texture;image.sampler=actual.sampler;
						image.owner=actual.owner;image.flipV=actual.flipV;image.depthImage=slot<2;
						image.requireReadable=std::move(actual.requireReadable);
						return{std::move(image),std::move(actual.deferredEpoch)};
					}
					return{impl->OriginalModelImage(sourceLease,binding,submission,options.shaderValidation,result.shaderCompileMs),{}};
				},options.shaderValidation,result.shaderCompileMs);
			for(const auto& draw:frame.luaDeferredLightFrame->draws)result.luaDeferredLightInstances+=draw.instanceCount;
		}
		std::shared_ptr<PreparedOutline> outline;
		if(frame.luaOutlineFrame){
			Require(frame.luaOutlineFrame->sourceDrawFrame==frame.sourceDrawFrame,"Original Outline belongs to another source frame");
			OutlineTarget target{layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,
				result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,frame.viewCamera,mainSamples};
			outline=impl->outlineGPU.Prepare(lease,frame.luaOutlineFrame,target,
				[&](RuntimeLease& sourceLease,const NativeCUSTextureBinding& binding,unsigned slot){
					return ResolveCUSOutlineInput(sourceLease,binding,slot,sourceSubmission);
				},options.shaderValidation,result.shaderCompileMs);
		}
  const auto prepareSSAO=[&](WaterLuaGPU& output,const std::shared_ptr<const NativeWaterLuaFrame>& input,id<MTLTexture> actualSceneColor){
   if(!input||!input->ssao||input->ssao->draws.empty())return;
   Require(frame.shieldDepthFrame!=nullptr,"SSAO has no actual common deferred submission");
   const uint64_t submission=frame.shieldDepthFrame->submissionIdentity;
   output.ssao=impl->ssaoGPU.Prepare(lease,input->ssao,frame.viewCamera,logicalWidth,logicalHeight,
    actualSceneColor,impl->depth,impl->stencil,[&](RuntimeLease& l,const NativeCUSTextureBinding& b){return resolveDeferredPostprocess(l,b,submission);},options.shaderValidation,result.shaderCompileMs,submission);
  };
  for(auto& ground:groundScenes) {
   const auto& source=*ground.source;const auto& target=ground.target;
   Require(source.sourceDrawFrame==frame.sourceDrawFrame&&ground.initialClearSource.size()==1,
    "Original main/IBL ground lost its same-draw actual initial clear");
   Require(source.groundInvoked==bool(source.ground),"Original ground call predicate and actual source disagree");
   ground.initialClear=PrepareTerrainGroundClears(lease,ground.initialClearSource,target,options.shaderValidation,result.shaderCompileMs);
   const auto generic=preparedWorld167.StageWhere(source.stage,[&](const auto& draw){
    Require(source.ground&&source.ground->source.Contains(draw.sequence),"Original terrain generic draw has no actual ground invocation span");
    return !source.ground->deferred||!source.ground->deferred->Contains(draw.sequence);
   });
   ground.overlays=impl->overlayGPU.Prepare(lease,generic,impl->screenGPU,screen.get(),source.viewCamera,
    target.width,target.height,target.logicalWidth,target.logicalHeight,target.colors[0],target.depth,target.stencil,
    options.shaderValidation,result.shaderCompileMs,false,target.samples);
   separateLineOwners[source.stage==NativeWorldDrawStage::Ground?4:5]=ground.overlays;
   if(const auto& actual=source.ground){
    Require(source.cus&&source.lua&&actual->sourceDrawFrame==frame.sourceDrawFrame&&actual->stage==source.stage&&
     actual->contextGeneration==OriginalNativeTextureContext().Registry().ContextGeneration(),
     "Original main/IBL ground lost its same-invocation program/callback owners");
    ground.terrain=impl->terrainSourceGPU.Prepare(lease,actual,target,
     [&](RuntimeLease& l,const NativeCUSTextureBinding& binding,unsigned,id<MTLSamplerState> sampler){
      return impl->OriginalTerrainImage(l,binding,sourceSubmission,sampler,options.shaderValidation,result.shaderCompileMs);
     },options.shaderValidation,result.shaderCompileMs);
    auto forwardCUS=actual->deferred?SelectNativeCUSSourceFrame(*source.cus,[&](const auto& draw){return !actual->deferred->Contains(draw.sequence);}):source.cus;
    ground.cus=impl->cusSourceGPU.Prepare(lease,forwardCUS,target,sourceImages,options.shaderValidation,result.shaderCompileMs);
    ground.clears=PrepareTerrainGroundClears(lease,ground.clearSource,target,options.shaderValidation,result.shaderCompileMs);
    prepareWaterLua(*ground.ordered,source.lua,source.viewCamera,target.width,target.height,target.logicalWidth,target.logicalHeight,
     target.colors[0],target.stencil,target.samples);
   }
   const auto add=[&](const auto& family){if(family)for(size_t i=0;i<family->DrawCount();++i)
    ground.ordered->draws.emplace_back(family->SequenceAt(i),[family,i](id<MTL4RenderCommandEncoder> encoder){family->EncodeDraw(i,encoder);});};
   add(ground.terrain);add(ground.cus);add(ground.clears);add(ground.overlays);
   if(ground.mapMRT){
    const auto& span=*source.ground->deferred;
    ground.ordered->commands.emplace_back(span.begin,[phase=ground.mapMRT](id<MTL4CommandBuffer> c){phase->Encode(c);});
    auto* typed=ground.ordered.get();
    ground.ordered->commands.emplace_back(span.end,[terrain=ground.terrain,cus=ground.cus,typed](id<MTL4CommandBuffer> c){
     terrain->EncodeDependencies(c);if(cus)cus->EncodeDependencies(c);typed->Dependencies(c);
    });
   }
  }
  if(shadowColor){
   const auto& source=*frame.shadowColorFrame;
   WorldCamera shadowCamera;
   shadowCamera.view=source.view;shadowCamera.projection=source.projection;shadowCamera.position=source.position;
   shadowCamera.viewport={0,0,int32_t(source.size),int32_t(source.size)};
   shadowCamera.clipDepth=source.zeroToOneDepth?ClipDepthConvention::ZeroToOne:ClipDepthConvention::MinusOneToOne;
   // The original shadow target has no stencil attachment. Do not invent one
   // to make an unsupported typed stencil operation appear to succeed.
   prepareWaterLua(shadowLua,source.luaEffects,shadowCamera,source.size,source.size,source.size,source.size,
    shadowColor->Color().pixelFormat,MTLPixelFormatInvalid);
   if(frame.luaWorldDrawFrame){
    const auto draws=preparedWorld167.Stage(NativeWorldDrawStage::ShadowTransparent);
    shadowLuaOverlays=impl->overlayGPU.Prepare(lease,draws,impl->screenGPU,screen.get(),shadowCamera,
     source.size,source.size,source.size,source.size,shadowColor->Color().pixelFormat,MTLPixelFormatDepth32Float,
     MTLPixelFormatInvalid,options.shaderValidation,result.shaderCompileMs);
    separateLineOwners[1]=shadowLuaOverlays;
    auto* family=shadowLuaOverlays.get();
    for(size_t i=0;i<family->DrawCount();++i)shadowLua.draws.emplace_back(family->SequenceAt(i),
     [family,i](id<MTL4RenderCommandEncoder> encoder){family->EncodeDraw(i,encoder);});
   }
   std::sort(shadowLua.draws.begin(),shadowLua.draws.end(),[](const auto& a,const auto& b){return a.first<b.first;});
   uint64_t previous=0;for(const auto& draw:shadowLua.draws){Require(draw.first>previous,"Shadow callback family sequence duplicated");previous=draw.first;}
   Require(source.redraw || shadowLua.draws.empty(),"Native shadow callback emitted outside original sun eligibility");
  }

  if(waterActive){
   for(auto& view:waterViews)if(view.source){
    const auto width=uint32_t(view.color.width),height=uint32_t(view.color.height);
    if(const auto& ground=view.source->groundSource){
     prepareWaterLua(*view.groundLua,view.source->groundLua,view.source->frame.viewCamera,width,height,width,height,MTLPixelFormatRGBA8Unorm,MTLPixelFormatInvalid);
     if(const auto& lua=view.source->groundLua;lua->ssao&&!lua->ssao->draws.empty()){
      Require(frame.shieldDepthFrame!=nullptr,"Original ground postprocess callback lacks its actual shared deferred source submission");
      const auto submission=frame.shieldDepthFrame->submissionIdentity;
      view.groundLua->ssao=impl->ssaoGPU.Prepare(lease,lua->ssao,view.source->frame.viewCamera,width,height,
       view.color,view.depth,nil,[&](RuntimeLease& l,const NativeCUSTextureBinding& b){return resolveDeferredPostprocess(l,b,submission);},
       options.shaderValidation,result.shaderCompileMs,submission);
     }
     const auto addGround=[&](const auto& family){if(family)for(size_t i=0;i<family->DrawCount();++i)
      view.groundLua->draws.emplace_back(family->SequenceAt(i),[family,i](id<MTL4RenderCommandEncoder> encoder){family->EncodeDraw(i,encoder);});};
     addGround(view.groundTerrain);addGround(view.groundCUS);addGround(view.groundClears);
    }
    prepareWaterLua(view.preParticlesLua,view.source->preParticlesLua,view.source->frame.viewCamera,width,height,width,height,MTLPixelFormatRGBA8Unorm,MTLPixelFormatInvalid);
    prepareWaterLua(view.worldLua,view.source->worldLua,view.source->frame.viewCamera,width,height,width,height,MTLPixelFormatRGBA8Unorm,MTLPixelFormatInvalid);
    if(view.overlays)for(size_t i=0;i<view.overlays->DrawCount();++i){
     auto* family=view.overlays.get();const auto sequence=family->SequenceAt(i);
					if(const auto phase=alphaPhase(view.source->frame.alphaModelFrames[0],sequence)){
						auto owner=view.overlays;view.alphaCallbacks.at(*phase)->draws.emplace_back(sequence,[owner,i](id<MTL4RenderCommandEncoder> encoder){owner->EncodeDraw(i,encoder);});continue;
					}
     if(view.source->groundSource&&view.source->groundSource->source.Contains(sequence)){
      view.groundLua->draws.emplace_back(sequence,[family,i](id<MTL4RenderCommandEncoder> encoder){family->EncodeDraw(i,encoder);});continue;
     }
     if(const auto& models=view.source->frame.opaqueModelFrame)for(size_t n=0;n<models->passes.size();++n){
      const auto end=n+1<models->passes.size()?models->passes[n+1].sequence:models->endSequence;
      Require(sequence<models->passes[n].sequence||sequence>=end,
       "Original water model callback emitted generic geometry without its actual phase consumer");
     }
     auto& output=sequence<view.source->preParticlesBoundary?view.preParticlesLua:view.worldLua;
     output.draws.emplace_back(sequence,[family,i](id<MTL4RenderCommandEncoder> encoder){family->EncodeDraw(i,encoder);});
    }
    if(const auto& source=view.source->groundSource){
     auto& ordered=view.groundLua->draws;
     std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.first<b.first;});
     uint64_t previous=source->source.begin;
     for(const auto& draw:ordered){Require(draw.first>previous&&source->source.Contains(draw.first),
      "Original water ground command is repeated or outside its actual source invocation");previous=draw.first;}
     if(const auto& ssao=view.groundLua->ssao)for(size_t i=0;i<ssao->DrawCount();++i){
      const auto sequence=ssao->SequenceAt(i);Require(source->source.Contains(sequence),"Original ground postprocess command is outside its source invocation");
      const auto found=std::lower_bound(ordered.begin(),ordered.end(),sequence,[](const auto& draw,uint64_t value){return draw.first<value;});
      Require(found==ordered.end()||found->first!=sequence,"Original ground postprocess and ordinary source command share an ordinal");
     }
    }
   }
   for(auto [output,input]:{std::pair{&belowWaterLua,frame.waterScene->belowWaterLua},std::pair{&aboveWaterLua,frame.waterScene->aboveWaterLua},std::pair{&waterPostLua,frame.waterScene->waterPostLua}})
    prepareWaterLua(*output,input,frame.viewCamera,result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,layer.pixelFormat,MTLPixelFormatStencil8,mainSamples);
   if(overlays)for(size_t i=0;i<overlays->DrawCount();++i){
    auto stage=overlays->StageAt(i);WaterLuaGPU* output=stage==NativeWorldDrawStage::AlphaBelowWater?&belowWaterLua:stage==NativeWorldDrawStage::AlphaAboveWater?&aboveWaterLua:stage==NativeWorldDrawStage::WaterPost?&waterPostLua:nullptr;
				if(output){
					const size_t slot=stage==NativeWorldDrawStage::AlphaBelowWater?0:1;
					if(stage!=NativeWorldDrawStage::WaterPost)if(const auto phase=alphaPhase(frame.alphaModelFrames[slot],overlays->SequenceAt(i))){
						auto owner=overlays;alphaCallbacks[slot].at(*phase)->draws.emplace_back(owner->SequenceAt(i),[owner,i](id<MTL4RenderCommandEncoder> encoder){owner->EncodeDraw(i,encoder);});continue;
					}
					auto* family=overlays.get();output->draws.emplace_back(family->SequenceAt(i),[family,i](id<MTL4RenderCommandEncoder> encoder){family->EncodeDraw(i,encoder);});
				}
   }
   for(auto* output:{&belowWaterLua,&aboveWaterLua,&waterPostLua,&waterViews[0].preParticlesLua,&waterViews[0].worldLua,&waterViews[1].preParticlesLua,&waterViews[1].worldLua}){
    std::sort(output->draws.begin(),output->draws.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    uint64_t previous=0;for(const auto& draw:output->draws){Require(draw.first>previous,"Water callback family sequence duplicated");previous=draw.first;}
   }
  }
  // The original SSAO widget only runs the non-reflection above-water callback.
  // Its offscreen operations must be interleaved with generic callback draws.
  if(frame.alphaAllLua){
   prepareWaterLua(alphaAllLua,frame.alphaAllLua,frame.viewCamera,result.surface.renderWidth,result.surface.renderHeight,result.surface.logicalWidth,result.surface.logicalHeight,layer.pixelFormat,MTLPixelFormatStencil8,mainSamples);
			if(overlays)for(size_t i=0;i<overlays->DrawCount();++i)if(overlays->StageAt(i)==NativeWorldDrawStage::AlphaAll){
				if(const auto phase=alphaPhase(frame.alphaModelFrames[2],overlays->SequenceAt(i))){
					auto owner=overlays;alphaCallbacks[2].at(*phase)->draws.emplace_back(owner->SequenceAt(i),[owner,i](id<MTL4RenderCommandEncoder> encoder){owner->EncodeDraw(i,encoder);});continue;
				}
				auto* family=overlays.get();alphaAllLua.draws.emplace_back(family->SequenceAt(i),[family,i](id<MTL4RenderCommandEncoder> encoder){family->EncodeDraw(i,encoder);});
			}
   std::sort(alphaAllLua.draws.begin(),alphaAllLua.draws.end(),[](const auto&a,const auto&b){return a.first<b.first;});
  }
  preparationDetail.Mark(6);
  std::unique_ptr<PreparedProjectileEffects> projectileEffects;
		if (frame.luaProjectileEffectsFrame) {
			projectileEffects = impl->projectileEffectsGPU.Prepare(lease, *frame.luaProjectileEffectsFrame, frame.viewCamera,
				layer.pixelFormat, MTLPixelFormatDepth32Float, options.shaderValidation, result.shaderCompileMs,mainSamples);
			for (const auto& draw: frame.luaProjectileEffectsFrame->draws)
				result.luaProjectileEffectInstances += draw.instanceCount;
		}
		std::unique_ptr<PreparedEnergyExplosion> energyExplosion;
		if (frame.luaEnergyExplosionFrame) {
			energyExplosion = impl->energyExplosionGPU.Prepare(lease, *frame.luaEnergyExplosionFrame, frame.viewCamera,
				layer.pixelFormat, MTLPixelFormatDepth32Float, options.shaderValidation, result.shaderCompileMs,mainSamples);
			for (const auto& draw: frame.luaEnergyExplosionFrame->draws)
				result.luaEnergyExplosionInstances += draw.instanceCount;
		}
		std::unique_ptr<PreparedLightningEffects> lightning;
		if (frame.luaLightningEffectFrame) {
			lightning = impl->lightningGPU.Prepare(lease, *frame.luaLightningEffectFrame, frame.viewCamera,
				result.surface.renderWidth, result.surface.renderHeight, logicalWidth, logicalHeight,
				layer.pixelFormat, MTLPixelFormatDepth32Float, MTLPixelFormatStencil8, options.shaderValidation, result.shaderCompileMs,mainSamples);
			for (const auto& draw: frame.luaLightningEffectFrame->draws)
				result.luaLightningInstances += draw.instanceCount;
		}
		std::unique_ptr<PreparedFireSmoke> fireSmoke;
		if (frame.luaFireSmokeFrame) {
			fireSmoke = impl->fireSmokeGPU.Prepare(lease, *frame.luaFireSmokeFrame, frame.viewCamera,
				result.surface.renderWidth, result.surface.renderHeight, logicalWidth, logicalHeight,
				impl->screenGPU, screen.get(), layer.pixelFormat, MTLPixelFormatDepth32Float, options.shaderValidation, result.shaderCompileMs,mainSamples);
			for (const auto& draw: frame.luaFireSmokeFrame->draws)
				result.luaFireSmokeSubmittedInstances += draw.instanceCount;
		}
		if(frame.shieldDepthFrame){
			const auto& depthFrame=*frame.shieldDepthFrame;
			if(frame.luaShieldFrame){
				ShieldDepthSources sources; sources.submissionIdentity=depthFrame.submissionIdentity;
				if(frame.shieldDepthFrame->map.backing)sources.map=GetShieldDepthBackingImage(*frame.shieldDepthFrame->map.backing);
				if(frame.opaqueModelFrame){
					Require(depthFrame.models.backing!=nullptr,"Shield lacks the actual MRT model depth backing");
					sources.models=GetShieldDepthBackingImage(*depthFrame.models.backing);
					Require(sources.models.role==ShieldDepthRole::ModelsOnly&&sources.models.producer&&
						sources.models.producer->role==ShieldDepthRole::ModelsOnly,
						"Shield cannot substitute scene/shadow depth for actual model MRT depth");
				}else if(modelGBuffer)sources.models=modelGBuffer->Image();
				shield=impl->shieldGPU.Prepare(lease,*frame.luaShieldFrame,frame.viewCamera,result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
					sources,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,options.shaderValidation,result.shaderCompileMs,
					[&](RuntimeLease& sourceLease,const NativeCUSTextureBinding& binding)->ShieldSampledImage{
						Require(binding.object!=nullptr,"Original Shield color binding has no retained source object");
						if(auto backing=std::dynamic_pointer_cast<const ui::NativeScreenTextureBacking>(binding.object->backing)){
							Require(backing->image!=nullptr,"Original shared color backing has no actual image");
							std::vector<PreparedScreenExternalImage> retained;
							// This resolver has no per-draw world ordinal. Preserve the
							// existing retired-only rule instead of inventing a consumer.
							auto image=impl->screenGPU.PrepareImage(sourceLease,backing->image,ShieldScreenSampler(binding.object->sampling),screen.get(),std::nullopt,
								[&](PreparedScreenExternalImage value){retained.push_back(std::move(value));});
							return {image.texture,image.sampler,backing,backing->image->flipV,
								[retained=std::move(retained)]{for(const auto& value:retained)value.requireReadable();}};
						}
						Require(binding.object->image!=nullptr,"Original Shield color backing requires its actual shared image provider");
						const auto& image=binding.object->image;auto texture=impl->Texture(sourceLease,image);
						const auto sampling=ShieldScreenSampler(binding.object->sampling);
						// Reuse the actual immutable world image; only sampler state is cached separately.
						auto found=std::find_if(impl->shieldColorSamplers.begin(),impl->shieldColorSamplers.end(),[&](const auto& p){return p.first==binding.object->sampling;});
						if(found==impl->shieldColorSamplers.end()){
							auto descriptor=[MTLSamplerDescriptor new];descriptor.supportArgumentBuffers=YES;descriptor.minFilter=sampling.filter==ScreenFilter::Nearest?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
							descriptor.magFilter=sampling.magnificationFilter==ScreenFilter::Nearest?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;descriptor.mipFilter=sampling.mipFilter==ScreenMipFilter::None?MTLSamplerMipFilterNotMipmapped:sampling.mipFilter==ScreenMipFilter::Nearest?MTLSamplerMipFilterNearest:MTLSamplerMipFilterLinear;
							const auto address=[](ScreenWrap w){return w==ScreenWrap::Repeat?MTLSamplerAddressModeRepeat:w==ScreenWrap::ClampEdge?MTLSamplerAddressModeClampToEdge:MTLSamplerAddressModeClampToBorderColor;};descriptor.sAddressMode=address(sampling.wrapS);descriptor.tAddressMode=address(sampling.wrapT);descriptor.borderColor=sampling.whiteBorder?MTLSamplerBorderColorOpaqueWhite:MTLSamplerBorderColorTransparentBlack;descriptor.maxAnisotropy=sampling.anisotropy;
							auto sampler=[sourceLease.Device()newSamplerStateWithDescriptor:descriptor];Require(sampler!=nil,"Cannot create actual Shield retained world-image sampler");impl->shieldColorSamplers.emplace_back(binding.object->sampling,sampler);found=std::prev(impl->shieldColorSamplers.end());
						}
						sourceLease.Retain(found->second);return{texture,found->second,image,image->flipV,{}};
					},mainSamples);
			}
		}

		result.materialDependenciesPending.assign(pending.begin(),pending.end());
		std::unique_ptr<PreparedMissileThruster> missileThruster;
		std::vector<const NativeMissileThrusterDraw*> missileThrusterDraws;
		if (frame.luaMissileThrusterFrame) {
			missileThruster = impl->missileThrusterGPU.Prepare(lease, *frame.luaMissileThrusterFrame, frame.viewCamera,
				result.surface.renderWidth, result.surface.renderHeight, logicalWidth, logicalHeight,
				impl->screenGPU, screen.get(), layer.pixelFormat, MTLPixelFormatDepth32Float, options.shaderValidation, result.shaderCompileMs,mainSamples);
			if (missileThruster) {
				for (const auto& draw: frame.luaMissileThrusterFrame->draws) {
					if (draw.instanceCount == 0)
						continue;
					Require(missileThrusterDraws.size() < missileThruster->DrawCount() &&
						missileThruster->SequenceAt(missileThrusterDraws.size()) == draw.sequence,
						"Original MissileThruster prepared draw order differs from its source");
					missileThrusterDraws.push_back(&draw);
				}
				Require(missileThrusterDraws.size() == missileThruster->DrawCount(),
					"Original MissileThruster prepared draw count differs from its source");
			}
		}
		std::unique_ptr<PreparedFireGL4> fireGL4;
		if(frame.luaFireGL4Frame) {
			Require(frame.luaFireGL4Frame->uniforms.sourceDrawFrame==frame.sourceDrawFrame,
				"Original FireGL4 packet is not this source draw");
			fireGL4=impl->fireGL4GPU.Prepare(lease,frame.luaFireGL4Frame,frame.viewCamera,
				result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
				impl->screenGPU,screen.get(),layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,options.shaderValidation,result.shaderCompileMs);
		}
		std::unique_ptr<PreparedFlamethrower> flamethrower;
		if (frame.luaFlamethrowerFrame)
			flamethrower=impl->flamethrowerGPU.Prepare(lease,frame.luaFlamethrowerFrame,frame.viewCamera,
				result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
				impl->screenGPU,screen.get(),layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,options.shaderValidation,result.shaderCompileMs);
		std::unique_ptr<PreparedCommands> commands;
		if(frame.luaCommandsFrame){
			Require(frame.luaCommandsFrame->sourceDrawFrame==frame.sourceDrawFrame,"Original Commands FX packet is not this source draw");
			auto resolver=[&](RuntimeLease& sourceLease,const NativeCUSTextureBinding& binding,std::optional<NativePostprocessConsumer> consumer){
				return impl->screenGPU.ResolveStartBoxImage(sourceLease,binding,screen.get(),consumer);
			};
			commands=impl->commandsGPU.Prepare(lease,frame.luaCommandsFrame,resolver,frame.viewCamera,
				result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
				layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,options.shaderValidation,result.shaderCompileMs);
		}
		std::unique_ptr<PreparedSnow> snow;
		if(frame.luaSnowFrame){
			Require(frame.luaSnowFrame->sourceDrawFrame==frame.sourceDrawFrame,"Original Snow packet is not this source draw");
			snow=impl->snowGPU.Prepare(lease,frame.luaSnowFrame,frame.viewCamera,
				result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
				impl->screenGPU,screen.get(),layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,options.shaderValidation,result.shaderCompileMs);
		}
		std::unique_ptr<PreparedNapalmPreview> napalmPreview;
		if(frame.luaNapalmPreviewFrame){
			Require(frame.luaNapalmPreviewFrame->sourceDrawFrame==frame.sourceDrawFrame,"Original AoE Napalm packet is not this source draw");
			napalmPreview=impl->napalmPreviewGPU.Prepare(lease,frame.luaNapalmPreviewFrame,frame.viewCamera,
				result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
				layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,options.shaderValidation,result.shaderCompileMs);
		}
		preparationDetail.Mark(7);
		std::unique_ptr<PreparedBlueprintFrame> blueprint;
		if (frame.luaBlueprintFrame) {
			blueprint = impl->blueprintGPU.Prepare(lease,*frame.luaBlueprintFrame,frame.viewCamera,
				result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
				layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,options.shaderValidation,result.shaderCompileMs,mainSamples,*impl->standardTerrainGPU,impl->heightOwner);
			for (const auto& draw: frame.luaBlueprintFrame->models) result.luaBlueprintModels += draw.submissions.size();
			for (const auto& draw: frame.luaBlueprintFrame->outlines) result.luaBlueprintOutlines += draw.instanceCount;
			result.luaBlueprintStencilClears = frame.luaBlueprintFrame->clears.size();
		}
		std::unique_ptr<PreparedGameDecal> gameDecals;
		std::unordered_map<uint64_t,const NativeGameDecalDraw*> gameDecalSources;
		if (frame.luaGameDecalFrame && !frame.luaGameDecalFrame->draws.empty()) {
			Require(standardShade && terrainMaterial.standardShading && worldInfo && frame.worldInfoFrame && worldInfo->MatchesFrame(*frame.worldInfoFrame),"Original game decals need their actual same-frame height/minimap/current-info owners");
			Require(frame.lighting.requested.shadows>=0,"Original game decal negative Shadows texture-zero branch is unavailable");
			GameDecalSceneImages images;images.sourceDrawFrame=frame.sourceDrawFrame;
			images.infoOwner=worldInfo;images.requestedInfo=frame.worldInfoFrame;
			ScreenSampler heightSampler;heightSampler.filter=ScreenFilter::Nearest;heightSampler.magnificationFilter=ScreenFilter::Linear;
			ScreenSampler minimapSampler;minimapSampler.wrapS=minimapSampler.wrapT=ScreenWrap::Repeat;minimapSampler.mipFilter=ScreenMipFilter::Linear;
			ScreenSampler infoSampler;infoSampler.mipFilter=ScreenMipFilter::Nearest;
			ScreenSampler shadowSampler;shadowSampler.wrapS=shadowSampler.wrapT=ScreenWrap::TransparentBorder;shadowSampler.whiteBorder=true;
			const auto& minimap=terrainMaterial.standardShading->base;
			images.images[0]={standardShade->Height(),heightSampler,false};
			images.images[1]={impl->screenGPU.PrepareImage(lease,minimap,minimapSampler,screen.get(),std::nullopt,retainTerrainImage).texture,minimapSampler,minimap->flipV};
			images.images[2]={worldInfo->Info(),infoSampler,false};
			// Original Shadows=0 allocates/clears a real1x1 depth FBO. The
			// existing same-submit native shadow pass preserves that lifetime.
			images.images[3]={impl->shadowDepth,shadowSampler,true};
			images.images[4]={impl->terrainNormals,heightSampler,false};
			gameDecals=impl->gameDecalGPU.Prepare(lease,*frame.luaGameDecalFrame,images,frame.viewCamera,
				result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
				impl->screenGPU,screen.get(),layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,options.shaderValidation,result.shaderCompileMs);
			for(const auto& draw:frame.luaGameDecalFrame->draws)gameDecalSources.emplace(draw.sequence,&draw);
		}
		std::unique_ptr<PreparedUnitPrimitive> unitPrimitives;
		std::unordered_map<uint64_t,const NativeUnitPrimitiveDraw*> unitPrimitiveSources;
		if (frame.luaUnitPrimitiveFrame && !frame.luaUnitPrimitiveFrame->draws.empty()) {
			unitPrimitives=impl->unitPrimitiveGPU.Prepare(lease,frame.luaUnitPrimitiveFrame,frame.viewCamera,
				result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
				impl->screenGPU,screen.get(),layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,options.shaderValidation,result.shaderCompileMs);
			for(const auto& draw:frame.luaUnitPrimitiveFrame->draws)unitPrimitiveSources.emplace(draw.sequence,&draw);
		}
		// Families share the real original call ordinal. Keep PreUnit separate
		// from later World, then merge within each stage before any GPU work.
		std::unique_ptr<PreparedPlacementGrid> placementGrid;
        PlacementGridHeight128 placementHeights;
        if(frame.placementFrame&&!frame.placementFrame->grids.empty()){auto h=impl->standardTerrainGPU->PrepareHeight(lease,impl->heightOwner);placementHeights={h->source,h->texture};}
		if(frame.placementFrame)placementGrid=impl->placementGPU.Prepare(lease,*frame.placementFrame,frame.viewCamera,
			result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
			layer.pixelFormat,impl->stencil.pixelFormat,options.shaderValidation,result.shaderCompileMs,mainSamples,placementHeights);
		std::vector<std::pair<const NativeBuildSquareDraw*,std::unique_ptr<PreparedBuildSquareDraw>>> buildSquares;
		if(frame.placementFrame){
			for(const auto& draw:frame.placementFrame->buildSquares){
				Require(draw.sourceDrawFrame==frame.sourceDrawFrame,"Original BuildSquare world source frame differs");
				auto sourceCamera=frame.viewCamera;
				if(draw.surface.viewport){const auto& v=*draw.surface.viewport;sourceCamera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
				auto raster=MapBuildSquareRaster(sourceCamera.viewport,{logicalWidth,logicalHeight},{result.surface.renderWidth,result.surface.renderHeight},draw.surface.scissor);
				if(!raster)continue;
				auto resolver=[&](RuntimeLease& l,const NativeCUSTextureBinding& b,std::optional<NativePostprocessConsumer> c){return impl->screenGPU.ResolveStartBoxImage(l,b,screen.get(),c);};
				auto prepared=impl->buildSquareGPU.Prepare(lease,draw,resolver,raster->viewport,raster->scissor,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,options.shaderValidation,result.shaderCompileMs,
					NativePostprocessConsumer{draw.stage==NativeWorldDrawStage::PreUnit?NativePostprocessStage::PreUnit:NativePostprocessStage::World,draw.sequence,0});
				if(!prepared->Empty())buildSquares.emplace_back(&draw,std::move(prepared));
			}
		}
		std::unique_ptr<PreparedBloom> bloom;
		std::shared_ptr<PreparedDistortion> distortions;
		std::vector<std::pair<const NativeStartBoxDraw*,std::unique_ptr<PreparedStartBoxDraw>>> startBoxes;
		if(frame.luaStartBoxFrame){
			Require(frame.luaStartBoxFrame->sourceDrawFrame==frame.sourceDrawFrame,"Original StartBox world frame differs");
			for(const auto& draw:frame.luaStartBoxFrame->draws){
				auto sourceCamera=frame.viewCamera;
				if(draw.surface.viewport){const auto& v=*draw.surface.viewport;sourceCamera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
				auto raster=effectstate::Viewport(sourceCamera,result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,draw.surface.scissor);
				if(!raster)continue;
				auto resolver=[&](RuntimeLease& l,const NativeCUSTextureBinding& b,std::optional<NativePostprocessConsumer> c){return impl->screenGPU.ResolveStartBoxImage(l,b,screen.get(),c);};
				auto prepared=impl->startBoxGPU.Prepare(lease,draw,resolver,raster->viewport,raster->scissor,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,options.shaderValidation,result.shaderCompileMs,
					NativePostprocessConsumer{draw.stage==NativeStartBoxStage::WorldPreUnit?NativePostprocessStage::PreUnit:NativePostprocessStage::World,draw.sequence,0});
				if(!prepared->Empty())startBoxes.emplace_back(&draw,std::move(prepared));
			}
		}
		std::vector<std::pair<const NativeWeaponRangeDraw*,std::unique_ptr<PreparedWeaponRangeDraw>>> weaponRanges;
		if(frame.luaWeaponRangeFrame){
			Require(frame.luaWeaponRangeFrame->sourceDrawFrame==frame.sourceDrawFrame,"Original range world frame differs");
			for(const auto& draw:frame.luaWeaponRangeFrame->draws){
				auto sourceCamera=frame.viewCamera;
				if(draw.surface.viewport){const auto& v=*draw.surface.viewport;sourceCamera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
				auto raster=MapWeaponRangeRaster(sourceCamera.viewport,{logicalWidth,logicalHeight},{result.surface.renderWidth,result.surface.renderHeight},draw.surface.scissor);
				if(!raster)continue;
				auto resolver=[&](RuntimeLease& l,const NativeCUSTextureBinding& binding,std::optional<NativePostprocessConsumer> c){return impl->screenGPU.ResolveStartBoxImage(l,binding,screen.get(),c);};
				auto prepared=impl->weaponRangeGPU.Prepare(lease,draw,resolver,raster->viewport,raster->scissor,{result.surface.renderWidth,result.surface.renderHeight},layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,mainSamples,options.shaderValidation,result.shaderCompileMs,NativePostprocessConsumer{NativePostprocessStage::World,draw.sequence,0});
				if(!prepared->Empty())weaponRanges.emplace_back(&draw,std::move(prepared));
			}
		}

		std::vector<std::pair<const NativeSensor31657CircleDraw*,std::unique_ptr<PreparedSensor31657CircleDraw>>> sensorCircles;
		if(frame.luaSensor31657CircleFrame){
			Require(frame.luaSensor31657CircleFrame->sourceDrawFrame==frame.sourceDrawFrame,"Original Sensor circle frame differs from actual source draw");
			for(const auto& draw:frame.luaSensor31657CircleFrame->draws){
				auto resolver=[&](RuntimeLease& l,const NativeCUSTextureBinding& binding,std::optional<NativePostprocessConsumer> consumer){
					return impl->screenGPU.ResolveStartBoxImage(l,binding,screen.get(),consumer);
				};
				auto prepared=impl->sensor31657CircleGPU.Prepare(lease,draw,resolver,frame.viewCamera,
					result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
					layer.pixelFormat,impl->depth.pixelFormat,impl->stencil.pixelFormat,mainSamples,options.shaderValidation,result.shaderCompileMs,
					NativePostprocessConsumer{NativePostprocessStage::World,draw.sequence,0});
				if(!prepared->Empty())sensorCircles.emplace_back(&draw,std::move(prepared));
			}
		}

  auto objectSpotlights=impl->objectSpotlightGPU.Prepare(lease,preparedWorld167.Family(NativeWorldFamily167::ObjectSpotlight),frame.sourceDrawFrame,frame.viewCamera,
   result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,layer.pixelFormat,
   impl->depth.pixelFormat,impl->stencil.pixelFormat,options.shaderValidation,result.shaderCompileMs,mainSamples);
  auto reclaimFields=impl->reclaimFieldGPU.Prepare(lease,preparedWorld167.Family(NativeWorldFamily167::ReclaimField),frame.sourceDrawFrame,frame.viewCamera,
   result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,layer.pixelFormat,
   impl->depth.pixelFormat,impl->stencil.pixelFormat,options.shaderValidation,result.shaderCompileMs,mainSamples);

  auto grass=impl->grassGPU.Prepare(lease,preparedWorld167.Family(NativeWorldFamily167::Grass),frame.sourceDrawFrame,frame.viewCamera,
   result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
   [&](RuntimeLease& l,const NativeCUSTextureBinding& binding,unsigned slot){return impl->OriginalModelImage(l,binding,sourceSubmission,options.shaderValidation,result.shaderCompileMs);},
   layer.pixelFormat,impl->depth.pixelFormat,impl->stencil.pixelFormat,options.shaderValidation,result.shaderCompileMs,mainSamples);

  auto resourceSpots=impl->resourceSpotsGPU.Prepare(lease,preparedWorld167.Family(NativeWorldFamily167::ResourceSpots),frame.sourceDrawFrame,frame.viewCamera,
   result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
   [&](RuntimeLease& l,const NativeCUSTextureBinding& binding,NativeWorldDrawStage stage,uint64_t sequence){
    // Keep the real DrawGenesis atlas producer and its read receipt. A plain
    // model-image lookup without screen/consumer loses this RTT dependency.
    auto actual=std::make_shared<PreparedStartBoxImage>(impl->screenGPU.ResolveStartBoxImage(l,binding,screen.get(),
     NativePostprocessConsumer{stage==NativeWorldDrawStage::PreUnit?NativePostprocessStage::PreUnit:NativePostprocessStage::World,sequence,0}));
    CUSSourceImage image;image.texture=actual->texture;image.sampler=actual->sampler;image.owner=actual;image.flipV=actual->flipV;
    Require(actual->requireReadable!=nullptr,"Resource spot height/atlas lacks its actual producer receipt");
    image.requireReadable=[actual]{actual->requireReadable();};return image;
   },layer.pixelFormat,impl->depth.pixelFormat,impl->stencil.pixelFormat,options.shaderValidation,result.shaderCompileMs,mainSamples);

  std::vector<std::shared_ptr<PreparedSensorLOSDraw>> sensorLOS;
  std::vector<const NativeSensorLOSDraw*> sensorLOSSources;
  for(const auto& command:preparedWorld167.Family(NativeWorldFamily167::SensorLOS)){
   ValidateNativeWorldDrawCommand(command);const auto& draw=*command.sensorLOS;
   Require(draw.sourceDrawFrame==frame.sourceDrawFrame,"LOS outline belongs to another actual source frame");
   auto resolver=[&](RuntimeLease& l,const NativeCUSTextureBinding& b,std::optional<NativePostprocessConsumer> consumer){return impl->screenGPU.ResolveStartBoxImage(l,b,screen.get(),consumer);};
   auto prepared=impl->sensorLOSGPU.Prepare(lease,draw,resolver,frame.viewCamera,result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,layer.pixelFormat,impl->depth.pixelFormat,impl->stencil.pixelFormat,mainSamples,options.shaderValidation,result.shaderCompileMs,NativePostprocessConsumer{NativePostprocessStage::World,draw.sequence,0});
   sensorLOSSources.push_back(&draw);sensorLOS.emplace_back(std::move(prepared));
  }

		enum class LuaDrawKind { BuildSquare, Outline, Blockout, DeferredLights, Bloom, ScreenTarget, SensorPreviewTarget, SensorPreviewFinal, Sensor31657Circle, WeaponRange, Commands, StartBox, GameDecal, UnitPrimitive, MapEdge, Overlay, Nano, ProjectileEffect, EnergyExplosion, Lightning, FireSmoke, MissileThruster, FireGL4, Flamethrower, NapalmPreview, Shield, Blueprint, PlacementGrid, PlacementSample, SourceCopy, VolumetricCloud, ReclaimField, Grass, ResourceSpots, SensorLOS, ObjectSpotlight, Snow, Distortion };
		struct LuaDraw { uint64_t sequence; LuaDrawKind kind; size_t index; };
		std::vector<LuaDraw> preDecalDraws, preUnitDraws, laterWorldDraws;
		if(frame.luaSensorPreviewFrame){
			Require(frame.luaSensorPreviewFrame->sourceDrawFrame==frame.sourceDrawFrame,"Original Preview frame differs from its actual world source");
			for(size_t i=0;i<frame.luaSensorPreviewFrame->commands.size();++i){
				const auto& command=frame.luaSensorPreviewFrame->commands[i];
				Require(command.sourceDrawFrame==frame.sourceDrawFrame&&command.callin==NativeSensorPreviewCallin::World,
					"Original Preview command belongs to another actual world frame/callin");
				laterWorldDraws.push_back({command.sequence,command.output?LuaDrawKind::SensorPreviewTarget:LuaDrawKind::SensorPreviewFinal,i});
			}
		}
		for(size_t i=0;i<sensorCircles.size();++i)laterWorldDraws.push_back({sensorCircles[i].first->sequence,LuaDrawKind::Sensor31657Circle,i});
		for(size_t i=0;i<buildSquares.size();++i)(buildSquares[i].first->stage==NativeWorldDrawStage::PreUnit?preUnitDraws:laterWorldDraws).push_back({buildSquares[i].first->sequence,LuaDrawKind::BuildSquare,i});
		for(size_t i=0;i<weaponRanges.size();++i)laterWorldDraws.push_back({weaponRanges[i].first->sequence,LuaDrawKind::WeaponRange,i});
		for(size_t i=0;i<startBoxes.size();++i)(startBoxes[i].first->stage==NativeStartBoxStage::WorldPreUnit?preUnitDraws:laterWorldDraws).push_back({startBoxes[i].first->sequence,LuaDrawKind::StartBox,i});
		if(screen)for(const auto& target:screen->WorldTargets()){
			Require((target.stage==NativePostprocessStage::PreUnit || target.stage==NativePostprocessStage::World) && target.sourceDrawFrame==frame.sourceDrawFrame && target.sequence,
				"StartBox RTT target has no matching actual world PreUnit capture");
			(target.stage==NativePostprocessStage::PreUnit?preUnitDraws:laterWorldDraws).push_back({target.sequence,LuaDrawKind::ScreenTarget,target.index});
		}
		if(gameDecals)for(size_t i=0;i<gameDecals->DrawCount();++i)preDecalDraws.push_back({gameDecals->SequenceAt(i),LuaDrawKind::GameDecal,i});
		if(napalmPreview)for(size_t i=0;i<napalmPreview->DrawCount();++i)preUnitDraws.push_back({napalmPreview->SequenceAt(i),LuaDrawKind::NapalmPreview,i});
		if(commands)for(size_t i=0;i<commands->DrawCount();++i)preUnitDraws.push_back({commands->SequenceAt(i),LuaDrawKind::Commands,i});
		if(reclaimFields)for(size_t i=0;i<reclaimFields->DrawCount();++i)preUnitDraws.push_back({reclaimFields->SequenceAt(i),LuaDrawKind::ReclaimField,i});
		if(objectSpotlights)for(size_t i=0;i<objectSpotlights->DrawCount();++i)laterWorldDraws.push_back({objectSpotlights->SequenceAt(i),LuaDrawKind::ObjectSpotlight,i});
		if(grass)for(size_t i=0;i<grass->DrawCount();++i)preUnitDraws.push_back({grass->SequenceAt(i),LuaDrawKind::Grass,i});
  if(resourceSpots)for(size_t i=0;i<resourceSpots->DrawCount();++i)(resourceSpots->StageAt(i)==NativeWorldDrawStage::PreUnit?preUnitDraws:laterWorldDraws).push_back({resourceSpots->SequenceAt(i),LuaDrawKind::ResourceSpots,i});
  for(size_t i=0;i<sensorLOS.size();++i)laterWorldDraws.push_back({sensorLOSSources[i]->sequence,LuaDrawKind::SensorLOS,i});
		if(unitPrimitives)for(size_t i=0;i<unitPrimitives->DrawCount();++i){
   const auto sequence=unitPrimitives->SequenceAt(i);const auto stage=unitPrimitiveSources.at(sequence)->stage;
   auto& draws=stage==NativeWorldDrawStage::PreUnit?preUnitDraws:laterWorldDraws;
   draws.push_back({sequence,LuaDrawKind::UnitPrimitive,i});
  }
		if(placementGrid)for(size_t i=0;i<placementGrid->DrawCount();++i)preUnitDraws.push_back({placementGrid->SequenceAt(i),LuaDrawKind::PlacementGrid,i});
		for(size_t i=0;i<placementPieces.size();++i)laterWorldDraws.push_back({placementPieces[i].sequence,LuaDrawKind::PlacementSample,i});
		if (mapEdge) preUnitDraws.push_back({frame.mapEdge->draw->sequence, LuaDrawKind::MapEdge, 0});
		if (overlays) for (size_t i=0; i<overlays->DrawCount(); ++i) {
			const auto stage=overlays->StageAt(i);
   if(stage==NativeWorldDrawStage::AlphaBelowWater || stage==NativeWorldDrawStage::AlphaAboveWater || stage==NativeWorldDrawStage::WaterPost || stage==NativeWorldDrawStage::AlphaAll)continue;
   auto& draws=stage==NativeWorldDrawStage::PreDecals?preDecalDraws:(stage==NativeWorldDrawStage::PreUnit?preUnitDraws:laterWorldDraws);
			draws.push_back({overlays->SequenceAt(i), LuaDrawKind::Overlay, i});
		}
		if (blueprint) for (size_t i=0; i<blueprint->DrawCount(); ++i) {
			auto& draws = blueprint->StageAt(i)==NativeWorldDrawStage::PreDecals ? preDecalDraws : (blueprint->StageAt(i)==NativeWorldDrawStage::PreUnit ? preUnitDraws : laterWorldDraws);
			draws.push_back({blueprint->SequenceAt(i),LuaDrawKind::Blueprint,i});
		}
		if (luaNano) for (size_t i=0; i<luaNano->DrawCount(); ++i)
			laterWorldDraws.push_back({luaNano->SequenceAt(i), LuaDrawKind::Nano, i});
		if (projectileEffects) for (size_t i=0; i<projectileEffects->DrawCount(); ++i)
			laterWorldDraws.push_back({projectileEffects->SequenceAt(i), LuaDrawKind::ProjectileEffect, i});
		if (energyExplosion) for (size_t i=0; i<energyExplosion->DrawCount(); ++i)
			laterWorldDraws.push_back({energyExplosion->SequenceAt(i), LuaDrawKind::EnergyExplosion, i});
		if (fireSmoke) for (size_t i=0; i<fireSmoke->DrawCount(); ++i)
			laterWorldDraws.push_back({fireSmoke->SequenceAt(i), LuaDrawKind::FireSmoke, i});
		if(fireGL4)for(size_t i=0;i<fireGL4->DrawCount();++i)laterWorldDraws.push_back({fireGL4->SequenceAt(i),LuaDrawKind::FireGL4,i});
		if(snow)for(size_t i=0;i<snow->DrawCount();++i)laterWorldDraws.push_back({snow->SequenceAt(i),LuaDrawKind::Snow,i});
		if(flamethrower)for(size_t i=0;i<flamethrower->DrawCount();++i)laterWorldDraws.push_back({flamethrower->SequenceAt(i),LuaDrawKind::Flamethrower,i});
		if(shield)for(size_t i=0;i<shield->DrawCount();++i)laterWorldDraws.push_back({shield->SequenceAt(i),LuaDrawKind::Shield,i});
		if (missileThruster) for (size_t i=0; i<missileThruster->DrawCount(); ++i)
			laterWorldDraws.push_back({missileThruster->SequenceAt(i), LuaDrawKind::MissileThruster, i});
		if (lightning) for (size_t i=0; i<lightning->DrawCount(); ++i)
			laterWorldDraws.push_back({lightning->SequenceAt(i), LuaDrawKind::Lightning, i});
		if(deferredLights)for(size_t i=0;i<deferredLights->DrawCount();++i)
			laterWorldDraws.push_back({deferredLights->SequenceAt(i),LuaDrawKind::DeferredLights,i});
		if(outline)for(size_t i=0;i<outline->OperationCount();++i)
			laterWorldDraws.push_back({outline->SequenceAt(i),LuaDrawKind::Outline,i});
		if(options.screenUI && options.screenUI->postprocess)for(std::size_t i=0;i<options.screenUI->postprocess->operations.size();++i){
			const auto& op=options.screenUI->postprocess->operations[i];if(op.stage==NativePostprocessStage::Screen)continue;
			(op.stage==NativePostprocessStage::PreDecals?preDecalDraws:(op.stage==NativePostprocessStage::PreUnit?preUnitDraws:laterWorldDraws)).push_back({op.sequence,LuaDrawKind::SourceCopy,i});
		}
		if(frame.luaVolumetricCloudFrame)for(std::size_t i=0;i<frame.luaVolumetricCloudFrame->draws.size();++i)
			preUnitDraws.push_back({frame.luaVolumetricCloudFrame->draws[i].sequence,LuaDrawKind::VolumetricCloud,i});
		if(frame.luaBloomFrame)for(size_t i=0;i<frame.luaBloomFrame->draws.size();++i)laterWorldDraws.push_back({frame.luaBloomFrame->draws[i].sequence,LuaDrawKind::Bloom,i});
  if(frame.luaWorldDrawFrame){size_t index=0;for(const auto& draw:frame.luaWorldDrawFrame->draws)if(draw.distortion)laterWorldDraws.push_back({draw.sequence,LuaDrawKind::Distortion,index++});}

  for(const auto* draws:{&preDecalDraws,&preUnitDraws,&laterWorldDraws})for(const auto& draw:*draws) {
   for(const auto& ground:groundScenes)Require(!ground.source->ground||!ground.source->ground->source.Contains(draw.sequence),
    "Original ground callback command requires its actual typed/RTT target consumer; moving it to a later world phase is forbidden");
   for(const auto& view:waterViews)if(view.source&&view.source->groundSource)
    Require(!view.source->groundSource->source.Contains(draw.sequence),
     "Original water-ground callback command has no selected same-invocation target consumer");
  }
		for (auto* draws: {&preDecalDraws, &preUnitDraws, &laterWorldDraws}) {
			std::sort(draws->begin(), draws->end(), [](const LuaDraw& a, const LuaDraw& b) { return a.sequence<b.sequence; });
			uint64_t previous=0;
			for (const auto& draw: *draws) {
				Require(draw.sequence>previous, "Original Lua world draw order is missing or duplicated across native families");
				previous=draw.sequence;
			}
		}
		std::vector<std::vector<LuaDraw>> opaquePostDeferredDraws;
		if(opaqueModels){
			opaquePostDeferredDraws.resize(opaqueModels->PhaseCount());
			const auto& source=*frame.opaqueModelFrame;
			std::vector<LuaDraw> remaining;
			for(const auto& draw:laterWorldDraws){
				bool consumed=false;
				for(size_t n=0;n<source.passes.size();++n){
					const auto& phase=source.passes[n];
					const auto end=n+1<source.passes.size()?source.passes[n+1].sequence:source.endSequence;
					if(draw.sequence<phase.sequence||draw.sequence>=end)continue;
					Require(phase.postDeferredSequence&&phase.postDeferredSequence<draw.sequence,
						"Original opaque callback emitted a generic/typed draw without its actual phase consumer");
					Require(!phase.postDeferredFixedFogEnabled||draw.kind!=LuaDrawKind::Overlay,"Original fixed-function PostDeferred geometry requires its actual fog consumer");
					opaquePostDeferredDraws[n].push_back(draw);consumed=true;break;
				}
				if(!consumed)remaining.push_back(draw);
			}
			laterWorldDraws=std::move(remaining);
		}
  #if defined(RECOIL_METAL4_LIVE_SCENE)
  static bool worldPlanReceipt167=false;
  if(timeWorldPlan167||(!worldPlanReceipt167&&frame.sourceDrawFrame>=300)){
   const auto m=preparedWorld167.Metrics();worldPlanReceipt167=true;
   LOG("[NativeWorldPlan167] source_frame=%llu serial=%llu input_draws=%llu plan_visits=%llu selected_visits=%llu entry_checks=%llu added_entry_family_predicates=%llu added_plan_family_predicates=%llu old_family_visits=%llu old_selection_visits=%llu copied_commands_avoided=%llu command_value_bytes_avoided=%llu vertex_copy_bytes_avoided=%llu index_copy_bytes_avoided=%llu plan_owner_allocations=%llu plan_vector_growths=%llu plan_requested_bytes=%llu plan_capacity_bytes=%llu selection_owner_allocations=%llu selection_vector_growths=%llu selection_requested_bytes=%llu preparation_only=1 validation_calls_avoided=0 allocator_overhead_included=0 timing=%u plan_us=%.3f selection_us=%.3f diagnostic=%u",
    (unsigned long long)frame.sourceDrawFrame,(unsigned long long)preparedWorld167.Serial(),(unsigned long long)m.inputDraws,(unsigned long long)m.indexBuildVisits,(unsigned long long)m.selectionIndexVisits,(unsigned long long)m.entryChecks,(unsigned long long)(5*m.entryChecks),(unsigned long long)(5*m.indexBuildVisits),(unsigned long long)m.legacyFamilyScanVisits,(unsigned long long)m.legacySelectionScanVisits,(unsigned long long)m.copiedCommandsAvoided,(unsigned long long)m.copiedCommandBytesAvoided,(unsigned long long)m.copiedVertexBytesAvoided,(unsigned long long)m.copiedIndexBytesAvoided,(unsigned long long)m.planOwnerAllocations,(unsigned long long)m.planVectorGrowths,(unsigned long long)m.planRequestedBytes,(unsigned long long)m.planCapacityBytes,(unsigned long long)m.selectionOwnerAllocations,(unsigned long long)m.selectionVectorGrowths,(unsigned long long)m.selectionRequestedBytes,unsigned(m.timingCollected),m.planMicroseconds,m.selectionMicroseconds,unsigned(worldPlanDiagnostic167));
  }
  #endif
		cpuPhase.Mark(4);preparationDetail.Mark(8);
		result.cpuResourceUpdateMs = Milliseconds(resourceStart) - result.shaderCompileMs;
		const auto acquireStart = Clock::now();
		id<CAMetalDrawable> drawable=nil;id<MTLTexture> sceneColor=nil;
		if(!offscreenSource){
			{NativeSubmissionTraceDrawableScope trace(&lease);
			 drawable=[layer nextDrawable];}
			if(drawable==nil){result.status=Metal4PresentationStatus::DrawableUnavailable;if(!requiresSource)return result;offscreenSource=true;}
			else{sceneColor=drawable.texture;Require(sceneColor.width==result.surface.renderWidth && sceneColor.height==result.surface.renderHeight,"Native drawable differs from requested render pixels");}
		}
		result.cpuDrawableAcquireMs=Milliseconds(acquireStart);
		cpuPhase.Mark(5);preparationDetail.Mark(9);
		// Isolate drawable writes from intermediate world/postprocess passes.
		// The original serialized RuntimeLease still retires storage before reuse.
		static const bool finalColorStageRequested=[] {const char* p=std::getenv("BAR_NATIVE_FINAL_COLOR_STAGE");return p&&p[0]=='1'&&p[1]==0;}();
		const bool finalColorStage=finalColorStageRequested&&!offscreenSource&&drawable&&mainSamples==1;
		if(offscreenSource||finalColorStage){
			if(!impl->offscreenSourceColor || impl->offscreenSourceColor.width!=result.surface.renderWidth || impl->offscreenSourceColor.height!=result.surface.renderHeight || impl->offscreenSourceColor.pixelFormat!=layer.pixelFormat){
				auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:layer.pixelFormat width:result.surface.renderWidth height:result.surface.renderHeight mipmapped:NO];descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
				auto image=[lease.Device()newTextureWithDescriptor:descriptor];Require(image!=nil,"Cannot allocate actual offscreen source-frame color");impl->offscreenSourceColor=image;
			}
			sceneColor=impl->offscreenSourceColor;
		}
		mainAttachments->BindDestination(lease,sceneColor);
		if(finalColorStage)mainAttachments->PrepareFinalDestination(lease,drawable.texture);
		static bool reportedFinalColorStage=false;
		if(!reportedFinalColorStage&&!offscreenSource){LOG("[NativeFinalColorStage] active=%d samples=%u present_order_original=1 width=%u height=%u",int(finalColorStage),mainSamples,result.surface.renderWidth,result.surface.renderHeight);reportedFinalColorStage=true;}

		sceneColor=mainAttachments->Color();
		std::shared_ptr<PreparedScreenFrame> presentationMargins;
		if(!offscreenSource && drawable && presentation.drawMargins) {
			const auto& safe=presentation.safeViewport;
			presentationMargins=impl->screenGPU.PreparePresentationMargins(lease,safe,
				result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,
				layer.pixelFormat,options.shaderValidation,result.shaderCompileMs,mainSamples);
		}
		prepareSSAO(aboveWaterLua,frame.waterScene?frame.waterScene->aboveWaterLua:nullptr,sceneColor);
  prepareSSAO(alphaAllLua,frame.alphaAllLua,sceneColor);
		for(size_t slot=0;slot<alphaModels.size();++slot)if(alphaModels[slot])for(size_t phase=0;phase<alphaCallbacks[slot].size();++phase)
			prepareSSAO(*alphaCallbacks[slot][phase],frame.alphaModelFrames[slot]->passes[phase].luaEffects,sceneColor);
		for(auto& view:waterViews)if(view.alphaModels)for(size_t phase=0;phase<view.alphaCallbacks.size();++phase){
			const auto& lua=view.source->frame.alphaModelFrames[0]->passes[phase].luaEffects;
			if(!lua->ssao||lua->ssao->draws.empty())continue;
			Require(frame.shieldDepthFrame!=nullptr,"Original planar alpha postprocess lacks its actual common deferred submission");
			const auto submission=frame.shieldDepthFrame->submissionIdentity;
			view.alphaCallbacks[phase]->ssao=impl->ssaoGPU.Prepare(lease,lua->ssao,view.source->frame.viewCamera,uint32_t(view.color.width),uint32_t(view.color.height),
				view.color,view.depth,nil,[&](RuntimeLease& l,const NativeCUSTextureBinding& b){return resolveDeferredPostprocess(l,b,submission);},options.shaderValidation,result.shaderCompileMs,submission);
		}
		const auto attachAlphaCallbacks=[&](const std::shared_ptr<PreparedOpaqueModelSource>& models,const std::vector<std::shared_ptr<WaterLuaGPU>>& callbacks){
			if(!models)return;Require(models->PhaseCount()==callbacks.size(),"Original alpha callback phase count differs from its actual model frame");
			for(size_t phase=0;phase<callbacks.size();++phase){const auto& owner=callbacks[phase];
				OpaqueModelForwardCallbacks selected;selected.lifetime=owner;selected.draws=owner->draws;selected.commands=owner->commands;
				if(owner->ssao)for(size_t draw=0;draw<owner->ssao->DrawCount();++draw)selected.commands.emplace_back(owner->ssao->SequenceAt(draw),[owner,draw](id<MTL4CommandBuffer> c){owner->ssao->EncodeDraw(draw,c);});
				selected.dependencies=[owner](id<MTL4CommandBuffer> c){owner->Dependencies(c);};selected.didComplete=[owner]{owner->Complete();};
				selected.inspectExecution166=[owner](const NativeOpaqueModelFrame* invocation,size_t phase,uint64_t frame,uint64_t submission){return owner->InspectExecution166(invocation,phase,frame,submission);};
				models->AttachForwardCallbacks(phase,std::move(selected));
			}
		};
		for(size_t slot=0;slot<alphaModels.size();++slot)attachAlphaCallbacks(alphaModels[slot],alphaCallbacks[slot]);
		for(auto& view:waterViews)attachAlphaCallbacks(view.alphaModels,view.alphaCallbacks);

  prepareSSAO(*mainGround.ordered,mainGround.source->lua,sceneColor);
  Require(!reflectionGround.source->lua||!reflectionGround.source->lua->ssao||reflectionGround.source->lua->ssao->draws.empty(),
   "Original IBL SSAO callback requires its actual cube-face postprocess target consumer");
  for(auto& ground:groundScenes) {
   auto& ordered=ground.ordered->draws;
   std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.first<b.first;});
   uint64_t previous=ground.source->ground?ground.source->ground->source.begin:0;
   for(const auto& draw:ordered){Require(draw.first>previous&&ground.source->ground&&ground.source->ground->source.Contains(draw.first),
    "Original main/IBL ground ordinal is repeated or outside its actual invocation");previous=draw.first;}
   if(const auto& ssao=ground.ordered->ssao)for(size_t i=0;i<ssao->DrawCount();++i){
    const auto sequence=ssao->SequenceAt(i);Require(ground.source->ground&&ground.source->ground->source.Contains(sequence),
     "Original main ground postprocess lies outside its actual invocation");
    const auto found=std::lower_bound(ordered.begin(),ordered.end(),sequence,[](const auto& draw,uint64_t value){return draw.first<value;});
    Require(found==ordered.end()||found->first!=sequence,"Original main ground postprocess duplicated an ordinary source ordinal");
   }
  }
		id<MTLDepthStencilState> ssaoAboveDepth=nil,ssaoAllDepth=nil;
  const auto callbackDepth=[&](const std::shared_ptr<const NativeWaterLuaFrame>& bundle)->id<MTLDepthStencilState>{if(!bundle||!bundle->ssao||bundle->ssao->draws.empty()||!bundle->ssao->exitWorldState)return nil;return effectstate::Depth(lease,*bundle->ssao->exitWorldState,MTLPixelFormatStencil8);};
  ssaoAboveDepth=callbackDepth(frame.waterScene?frame.waterScene->aboveWaterLua:nullptr);ssaoAllDepth=callbackDepth(frame.alphaAllLua);
  if(frame.luaWorldDrawFrame&&std::any_of(frame.luaWorldDrawFrame->draws.begin(),frame.luaWorldDrawFrame->draws.end(),[](const auto& draw){return bool(draw.distortion);})){
   Require(frame.shieldDepthFrame!=nullptr,"Distortion requires its actual common independent deferred producers");const auto submission=frame.shieldDepthFrame->submissionIdentity;
   distortions=impl->distortionGPU.Prepare(lease,frame.luaWorldDrawFrame,frame.sourceDrawFrame,frame.viewCamera,logicalWidth,logicalHeight,sceneColor,impl->depth,impl->stencil,
    [&](RuntimeLease& l,const NativeCUSTextureBinding& binding,unsigned slot)->DeferredLightImage{
     if(slot<2){auto actual=resolveDeferredPostprocess(l,binding,submission);CUSSourceImage image;image.texture=actual.texture;image.sampler=actual.sampler;image.owner=actual.owner;image.flipV=actual.flipV;image.depthImage=true;image.requireReadable=std::move(actual.requireReadable);return{std::move(image),std::move(actual.deferredEpoch)};}
     return{impl->OriginalModelImage(l,binding,submission,options.shaderValidation,result.shaderCompileMs),{}};
    },options.shaderValidation,result.shaderCompileMs,submission,mainAttachments);
  }
  if(frame.luaBloomFrame&&!frame.luaBloomFrame->draws.empty()){
   Require(frame.shieldDepthFrame!=nullptr,"Bloom has no actual common deferred submission");const auto submission=frame.shieldDepthFrame->submissionIdentity;
   bloom=impl->bloomGPU.Prepare(lease,frame.luaBloomFrame,frame.viewCamera,logicalWidth,logicalHeight,sceneColor,impl->depth,impl->stencil,
    [&](RuntimeLease& l,const NativeCUSTextureBinding& b)->ResolvedBloomImage{auto image=resolveDeferredPostprocess(l,b,submission);return{image.texture,image.sampler,image.owner,image.requireReadable,image.flipV,std::move(image.deferredEpoch)};},options.shaderValidation,result.shaderCompileMs,submission);
  }
		std::shared_ptr<PreparedPostprocessFrame> postprocess;
		if(options.screenUI && options.screenUI->postprocess && (!options.screenUI->postprocess->operations.empty() || !options.screenUI->postprocess->externalOutputs.empty() || !screen->DeferredTargets().empty())) {
			Require(screen!=nullptr && !sceneColor.framebufferOnly,"Original postprocess requires real readable screen/color contents");
			// Fixed ScreenGPU providers have read-only shader resources separate
			// from the live main attachments, including all texture-view backings.
			// Re-audit this opt-in when adding providers or shader write access.
			postprocess=impl->postprocessGPU.Prepare(lease,options.screenUI->postprocess,sceneColor,impl->depth,*screen,options.shaderValidation,result.shaderCompileMs,impl->stencil,mainAttachments,true);
		}
		std::shared_ptr<Impl::ScreenContinuation> continuation;
		cpuPhase.Mark(6);
		std::shared_ptr<PreparedScreenReadback> readback;
		std::shared_ptr<bool> mainReadReady;
		if(options.readback){
			mainReadReady=std::make_shared<bool>(false);
			continuation=std::make_shared<Impl::ScreenContinuation>();
			continuation->main=mainAttachments;continuation->source=std::make_shared<const WorldFrame>(frame);
			continuation->logicalWidth=logicalWidth;continuation->logicalHeight=logicalHeight;
			PreparedScreenExternalImage image;
			if(options.readback->readColor)image=screen->ResolvePreviousImage(lease,options.readback->readColor,
				{NativePostprocessStage::Screen,0,std::uint32_t(options.readback->root->Commands().size()),options.readback->cutoff});
			else image={mainAttachments->ReadColor(),mainAttachments,[mainReadReady]{Require(*mainReadReady,"SaveImage main source prefix is not encoded");}};
			readback=impl->screenGPU.PrepareReadback(lease,std::move(image),options.readbackRectangle);
		}
		std::vector<std::unique_ptr<PreparedSensorPreviewCommand>> sensorPreview;
		std::shared_ptr<PreparedBlockoutFrame> blockout;
		if(frame.luaBlockoutFrame){
			Require(frame.luaBlockoutFrame->sourceDrawFrame==frame.sourceDrawFrame,"Blockout packet is not this actual source frame");
			std::vector<BlockoutDepthImage> images;
			for(const auto& source:frame.luaBlockoutFrame->depthPasses){
				Require(postprocess!=nullptr,"Blockout depth export lacks its actual shared publication frame");
				images.push_back({source.output,postprocess->ProducedImage(source.output)});
			}
			blockout=impl->blockoutGPU.Prepare(lease,frame.luaBlockoutFrame,images,frame.viewCamera,
				result.surface.renderWidth,result.surface.renderHeight,logicalWidth,logicalHeight,layer.pixelFormat,impl->stencil.pixelFormat,options.shaderValidation,result.shaderCompileMs,mainSamples);
			for(size_t i=0;i<blockout->DrawCount();++i){
				const auto sequence=blockout->SequenceAt(i);
				for(const auto& ground:groundScenes)Require(!ground.source->ground||!ground.source->ground->source.Contains(sequence),"Blockout cannot migrate a ground callback to PreUnit");
				for(const auto& view:waterViews)if(view.source&&view.source->groundSource)Require(!view.source->groundSource->source.Contains(sequence),"Blockout cannot migrate a water-ground callback to PreUnit");
				preUnitDraws.push_back({sequence,LuaDrawKind::Blockout,i});
			}
			std::sort(preUnitDraws.begin(),preUnitDraws.end(),[](const LuaDraw& a,const LuaDraw& b){return a.sequence<b.sequence;});
			for(size_t i=1;i<preUnitDraws.size();++i)Require(preUnitDraws[i-1].sequence<preUnitDraws[i].sequence,"Blockout/PreUnit source command order is repeated");
		}
		if(frame.luaSensorPreviewFrame){
			sensorPreview.reserve(frame.luaSensorPreviewFrame->commands.size());
			for(const auto& command:frame.luaSensorPreviewFrame->commands){
				const NativePostprocessConsumer consumer{NativePostprocessStage::World,command.sequence,0};
				SensorPreviewTarget target;target.width=result.surface.renderWidth;target.height=result.surface.renderHeight;
				target.logicalWidth=logicalWidth;target.logicalHeight=logicalHeight;target.cameraViewport=frame.viewCamera.viewport;
				target.color=layer.pixelFormat;target.depth=impl->depth.pixelFormat;target.stencil=impl->stencil.pixelFormat;target.samples=mainSamples;
				if(command.output){
					Require(postprocess!=nullptr,"Original Preview target lacks its shared actual postprocess frame");
					target.outputVersion=command.output;target.previousVersion=command.previous;
					target.output=postprocess->ProducedImage(command.output);target.color=target.output.pixelFormat;target.samples=unsigned(target.output.sampleCount);
					target.width=uint32_t(target.output.width);target.height=uint32_t(target.output.height);
					target.logicalWidth=target.width;target.logicalHeight=target.height;
					target.cameraViewport={0,0,int32_t(target.width),int32_t(target.height)};
					target.depth=MTLPixelFormatInvalid;target.stencil=MTLPixelFormatInvalid;
					if(command.previous){
						target.previous=impl->postprocessGPU.ResolveImage(lease,command.previous,consumer);
						target.requirePreviousReadable=[owner=impl.get(),version=command.previous]{owner->postprocessGPU.RequireReadable(version);};
					}
				}
				auto images=[&](RuntimeLease& sourceLease,const NativeSensorPreviewImageInput& input,bool contents,NativePostprocessConsumer at){
					return impl->SensorPreviewImage(sourceLease,input,contents,at,screen.get(),options.shaderValidation,result.shaderCompileMs);
				};
				// Even an empty raster target must preserve the previous image and
				// encode/publish its actual output generation at this original ordinal.
				sensorPreview.push_back(impl->sensorPreviewGPU.Prepare(lease,command,target,images,impl->screenGPU,
					options.shaderValidation,result.shaderCompileMs,consumer));
			}
		}
		std::unique_ptr<PreparedVolumetricClouds> clouds;
		if(frame.luaVolumetricCloudFrame && !frame.luaVolumetricCloudFrame->draws.empty()){
			Require(postprocess && frame.luaVolumetricCloudFrame->sourceDrawFrame==frame.sourceDrawFrame,"Cloud commands lack their exact shared source frame");
			std::vector<VolumetricCloudImages> images;images.reserve(frame.luaVolumetricCloudFrame->draws.size());
			for(const auto& draw:frame.luaVolumetricCloudFrame->draws)images.push_back({draw.depth,draw.output,
				impl->postprocessGPU.ResolveImage(lease,draw.depth,NativePostprocessConsumer{NativePostprocessStage::PreUnit,draw.sequence,0}),postprocess->ProducedImage(draw.output)});
			clouds=impl->volumetricCloudsGPU.Prepare(lease,frame.luaVolumetricCloudFrame,images,options.shaderValidation,result.shaderCompileMs);
		}
		std::unique_ptr<PreparedBumpWater> waterSurface;
  std::unique_ptr<PreparedBasicWater> basicSurface;
  std::unique_ptr<PreparedLegacyWaterBump> legacyBump;
  std::unique_ptr<PreparedDynamicWater> dynamicSurface;
  if(dynamicWaterActive){
   DynamicWaterSceneImages images{impl->waterReflection,impl->waterRefraction,true,true};
   auto resolve=[&](const NativeCUSTextureBinding& binding){return impl->OriginalModelImage(lease,binding,sourceSubmission,options.shaderValidation,result.shaderCompileMs);};
   lease.RetirePreviousSubmission();
   dynamicSurface=impl->dynamicWaterGPU.Prepare(lease,frame.dynamicWater,images,resolve,layer.pixelFormat,mainSamples,options.shaderValidation,result.shaderCompileMs);
  }
  std::unique_ptr<PreparedLegacyWater> legacySurface;
  if(legacyWaterActive){
   Require(frame.legacyWater->sourceDrawFrame==frame.sourceDrawFrame,"Water1 frame differs from actual world draw");
   legacyBump=impl->legacyWaterBumpGPU.Prepare(lease,frame.legacyWater->bump,options.shaderValidation,result.shaderCompileMs);
   CUSSourceImage shading;
   if(frame.legacyWater->refraction)shading=impl->OriginalModelImage(lease,frame.legacyWater->refraction->shading,sourceSubmission,options.shaderValidation,result.shaderCompileMs);
   lease.RetirePreviousSubmission();
   legacySurface=impl->legacyWaterGPU.Prepare(lease,frame.legacyWater,impl->waterReflection,legacyBump->Texture(),legacyBump->FlipV(),layer.pixelFormat,mainSamples,options.shaderValidation,result.shaderCompileMs,std::move(shading),result.surface.renderHeight,sourceSubmission);
  }
  if(basicWaterActive)basicSurface=impl->basicWaterGPU.Prepare(lease,frame.basicWater,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,options.shaderValidation,result.shaderCompileMs,mainSamples);
  if(bumpWaterActive){
   const auto& water=*frame.bumpWater;
   BumpWaterSceneImages images;images.sourceDrawFrame=frame.sourceDrawFrame;
   images.renderWidth=result.surface.renderWidth;images.renderHeight=result.surface.renderHeight;
   images.shading={standardShade?standardShade->Shading():nil,false};
   images.planarReflection={impl->waterReflection,true};
   images.preWaterColor={water.settings.refraction>1?impl->waterRefraction:impl->preWaterColor,true};
   images.preWaterDepth={impl->preWaterDepth,true};
   // Same-draw source owners supply the genuine transparent shadow and
   // original current-info images before the water leaf is prepared.
   if(water.shadows){
    Require(sampledShadowColor&&(frame.originalShadow||(shadowColor&&shadowColor->SourceDrawFrame()==frame.sourceDrawFrame)),"Water4 needs its same-draw transparent shadow-color producer");
    images.shadowDepth={impl->shadowDepth,true};images.shadowColor={sampledShadowColor,true};
   }
   if(water.infoTexture){
    Require(worldInfo && frame.worldInfoFrame && worldInfo->MatchesFrame(*frame.worldInfoFrame),"Water4 needs its actual requested original info image version");
    images.info={worldInfo->Info(),false};
   }
   // BumpWater pins its recurrence predecessor and verifies real retirement at submit.
   waterSurface=impl->waterGPU.Prepare(lease,frame.bumpWater,images,layer.pixelFormat,MTLPixelFormatDepth32Float,
    MTLPixelFormatStencil8,options.shaderValidation,result.shaderCompileMs,mainSamples);
  }
  std::unique_ptr<PreparedGroundDecal> groundDecals;
  if(frame.groundDecals){
   const auto& decals=*frame.groundDecals;
   Require(decals.sourceDrawFrame==frame.sourceDrawFrame && standardShade,"Original decal CPU/framebuffer generations differ");
   Require(decals.sampleCount==impl->depth.sampleCount,"Original decal MSAA requires matching actual terrain/main targets");
   GroundDecalSceneImages images;images.sourceDrawFrame=frame.sourceDrawFrame;
   images.height={standardShade->Height(),false};images.normals={impl->terrainNormals,false};
   const auto& base=terrainMaterial.standardShading->base;
   images.minimap={impl->screenGPU.PrepareImage(lease,base,ScreenSampler{},screen.get(),std::nullopt,retainTerrainImage).texture,base->flipV};
   images.terrainDepth={decals.highQuality?terrainSnapshot->SamplesImage():terrainSnapshot->SingleImage(),true};
   Require(decals.sampleCount==mainSamples,"Original ground decal output count differs from the allocated main framebuffer");
   if(decals.shadows){Require(sampledShadowColor&&(frame.originalShadow||(shadowColor&&shadowColor->SourceDrawFrame()==frame.sourceDrawFrame)),"Original decals need this draw's real shadow-color producer");images.shadowDepth={impl->shadowDepth,true};images.shadowColor={sampledShadowColor,true};}
   images.fragmentToLogical={float(logicalWidth)/result.surface.renderWidth,-float(logicalHeight)/result.surface.renderHeight,0,float(logicalHeight)};
   groundDecals=impl->groundDecalGPU.Prepare(lease,frame.groundDecals,images,layer.pixelFormat,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,options.shaderValidation,result.shaderCompileMs,
    [&](RuntimeLease& lease,std::shared_ptr<const WorldTexture> source){return impl->Texture(lease,source);});
   result.groundDecalInstances=uint32_t(decals.instances->values.size());result.groundDecalAtlasDraws=groundDecals->AtlasDrawCount();
  }
  // World overlays and screen/RTT preparation share this named-image
		// cache. Collect only after every family has prepared its images.
		cpuPhase.Mark(7);
		auto namedImageMipmaps=impl->screenGPU.PrepareMipmaps(lease);
		auto originalLifetime=[M4OriginalSourceLifetime new];
  originalLifetime->opaque=opaqueModels;originalLifetime->source=frame.opaqueModelFrame;originalLifetime->genesis=frame.genesisCUSFrame;
		originalLifetime->alpha=alphaModels;
  originalLifetime->waterSources=frame.waterScene;
  originalLifetime->shadow=originalShadow;originalLifetime->shadowSource=frame.originalShadow;
  originalLifetime->waterGround.push_back(overlays);
  for(const auto& lines:separateLineOwners)if(lines)originalLifetime->waterGround.push_back(lines);
  originalLifetime->initialShadow=initialShadow;originalLifetime->commonCommands=frame.luaWorldDrawFrame;originalLifetime->reclaimFields=reclaimFields;originalLifetime->objectSpotlights=objectSpotlights;
  originalLifetime->grass=grass;originalLifetime->resourceSpots=resourceSpots;originalLifetime->sensorLOS=sensorLOS;
  originalLifetime->mainAttachments=mainAttachments;originalLifetime->terrainSnapshot=terrainSnapshot;
  originalLifetime->terrainImages=std::move(terrainImages);originalLifetime->screen=screen;originalLifetime->presentationMargins=presentationMargins;
  originalLifetime->deferredLights=deferredLights;originalLifetime->distortions=distortions;
  originalLifetime->unitStencil=unitStencil;originalLifetime->outline=outline;
  originalLifetime->blockout=blockout;
  for(size_t n=0;n<waterViews.size();++n){
			const auto& view=waterViews[n];originalLifetime->waterOpaque[n]=view.opaqueModels;originalLifetime->waterAlpha[n]=view.alphaModels;
   if(view.source)originalLifetime->waterGround.push_back(view.initialClear);
   if(view.source&&view.source->groundSource){
    originalLifetime->waterGround.push_back(view.groundLua);
    originalLifetime->waterGround.push_back(view.overlays);
    originalLifetime->waterGround.push_back(view.groundTerrain);
    originalLifetime->waterGround.push_back(view.groundCUS);
    originalLifetime->waterGround.push_back(view.groundClears);
   }
  }
  for(const auto& ground:groundScenes){
   originalLifetime->waterGround.push_back(ground.source);originalLifetime->waterGround.push_back(ground.initialClear);
   originalLifetime->waterGround.push_back(ground.terrain);originalLifetime->waterGround.push_back(ground.cus);
   originalLifetime->waterGround.push_back(ground.clears);originalLifetime->waterGround.push_back(ground.overlays);
   originalLifetime->waterGround.push_back(ground.mapMRT);
   originalLifetime->waterGround.push_back(ground.ordered);
  }
  if(postprocess)originalLifetime->waterGround.push_back(postprocess);
  if(continuation)originalLifetime->waterGround.push_back(continuation);
  // Original ordinal/frame/face routing and clear preparation were validated
  // above. Authorize only this main submission's exact first reflection clear.
  // The same clear still executes at its original position below (load or draw).
  if(!offscreenSource&&!continuation&&reflectionGround.initialClearSource.size()==1&&
   reflectionGround.initialClearSource.front().sequence==reflectionGround.source->initialClearSequence)
   AuthorizeCubeFaceOverwrite206(*environmentFrame.sourceCubeUpdate->backing,reflectionPass,
    reflectionGround.initialClearSource.front(),frame.sourceDrawFrame,sourceSubmission);
  lease.RetainTerrainUniformSources220(mainGround.source?mainGround.source->ground:nullptr,
   reflectionGround.source?reflectionGround.source->ground:nullptr);
  lease.Retain(originalLifetime);
		lease.AddAllocation(sceneColor); if(drawable)lease.Retain(drawable); cpuPhase.Mark(8); preparationDetail.Mark(10); lease.CommitResourcesWhileEncoding();
		cpuPhase.Mark(9);preparationDetail.Mark(11);
		WorldEncodingProbe176 encodingProbe(frame.sourceDrawFrame,!offscreenSource && !continuation && frame.simulationFrame>0);
		using Family176 = WorldEncodingProbe176::Family;
		result.queueClass = lease.QueueClass(); result.commandBufferClass = lease.CommandBufferClass();
		MTL4RenderPassDescriptor* pass=mainAttachments->LoadedPass();
		const auto prepStart = Clock::now();
  struct OriginalSubmissionFailure {
   RuntimeLease& lease;bool began=false,retired=false;
   ~OriginalSubmissionFailure(){if(began&&!retired)lease.PoisonAfterBegin();}
  } originalSubmission{lease};
		id<MTL4CommandBuffer> command = lease.Begin("Actual engine terrain and current S3O unit poses to CAMetalLayer");
  originalSubmission.began=true;impl->submittedOpaqueSource=frame.opaqueModelFrame;impl->activeSourceSubmission=sourceSubmission;
  if(screen && options.screenUI)targetSubmissionBegan(sourceSubmission);
  if(options.submissionBegan)options.submissionBegan(sourceSubmission,options.screenUI?options.screenUI->postprocess.get():nullptr);
  const auto requireLogicalCopySource=[&]{
   Require(logicalCopyMapping.has_value(),"Logical copy lost its prepared source mapping");
   logicalCopyMapping->RequireSource(frame.sourceDrawFrame,mainAttachments->Generation(),
    uint32_t(mainAttachments->Depth().width),uint32_t(mainAttachments->Depth().height),mainAttachments->Samples());
  };
  const auto encodeLogicalDepth=[&](id<MTLTexture> source,id<MTLTexture> destination,id<MTLBuffer> coordinates){
   requireLogicalCopySource();
   Require(source&&destination&&source!=destination&&coordinates&&logicalCopyDepthState&&
    source.sampleCount==1&&destination.sampleCount==1&&source.pixelFormat==MTLPixelFormatDepth32Float&&
    destination.pixelFormat==MTLPixelFormatDepth32Float&&destination.width==logicalCopyMapping->width&&
    destination.height==logicalCopyMapping->height,"Logical depth copy lost its single-sample owned source/output");
   auto copyPass=[MTL4RenderPassDescriptor new];copyPass.renderTargetWidth=destination.width;copyPass.renderTargetHeight=destination.height;
   copyPass.defaultRasterSampleCount=1;copyPass.depthAttachment.texture=destination;
   copyPass.depthAttachment.loadAction=MTLLoadActionDontCare;copyPass.depthAttachment.storeAction=MTLStoreActionStore;
   auto copy=[command renderCommandEncoderWithDescriptor:copyPass];Require(copy!=nil,"Cannot encode logical depth copy");
   [copy barrierAfterQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
   [copy setRenderPipelineState:impl->logicalDepthCopyPipeline];[copy setDepthStencilState:logicalCopyDepthState];
   [copy setViewport:MTLViewport{0,0,double(destination.width),double(destination.height),0,1}];
   [copy setScissorRect:MTLScissorRect{0,0,destination.width,destination.height}];[copy setCullMode:MTLCullModeNone];
   [copy setDepthClipMode:MTLDepthClipModeClip];[copy setTriangleFillMode:MTLTriangleFillModeFill];
   [arguments setAddress:coordinates.gpuAddress atIndex:0];[arguments setTexture:source.gpuResourceID atIndex:0];
   [copy setArgumentTable:arguments atStages:MTLRenderStageFragment];[copy drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
   [copy barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
   [copy endEncoding];
  };


		impl->EncodePendingTextureMipmaps(command,sourceSubmission);
		for(const auto& packet:sensorPreview)packet->EncodeDependencies(command);
		for(const auto& image:originalLifetime->terrainImages)image.requireReadable();
		impl->particlesGPU.EncodeAtlasImages(command,sourceSubmission);
		namedImageMipmaps->EncodeDependencies(command);
		encodingProbe.Select(Family176::Effects);
		if(groundDecals)groundDecals->EncodeDependencies(command);
		if(clouds)clouds->EncodeDependencies(command);
		if(blockout)blockout->EncodeDependencies(command);
		// Materialize the original VS float32 outputs before any render pass.
		// The family owns its dispatch-to-vertex barrier in this same submission.
		if (energyExplosion) energyExplosion->EncodeDependencies(command);
		if (lightning) lightning->EncodeDependencies(command);
		if(deferredLights)deferredLights->EncodeDependencies(command);
		if(outline)outline->EncodeDependencies(command);
		if(unitStencil)for(size_t n=0;n<unitStencil->PassCount();++n)unitStencil->EncodePass(n,command);
		if(distortions)distortions->EncodeDependencies(command);
		if (blueprint) blueprint->EncodeDependencies(command);
  if(overlays)overlays->EncodeDependencies(command);
  if(placementGrid)placementGrid->EncodeDependencies(command);
  for(const auto& lines:separateLineOwners)if(lines)lines->EncodeLineDependencies(command);
		encodingProbe.Select(Family176::Other);
		if(pbrBRDF)pbrBRDF->Encode(command,sourceSubmission);
  // GenerateIBL's actual named shadow producer precedes reflection and later
  // source consumers in this SAME command. No extra lease/submit/wait.
  encodingProbe.Select(Family176::Shadow);
  for(const auto& backing:initialShadow)if(backing&&!backing->completed.load())
   EncodeNativeShadowInitialClear(backing,command,sourceSubmission);
  if(originalShadow){
   originalShadow->EncodeDependencies(command);
   id<MTL4RenderCommandEncoder> shadowEncoder=nil;
   bool depthInitialized=false,colorInitialized=bool(frame.originalShadow->textures->retainedColor);
   const auto closeShadow=[&]{
    if(!shadowEncoder)return;
    [shadowEncoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
    [shadowEncoder endEncoding];shadowEncoder=nil;
   };
   const auto openShadow=[&](const NativeWorldAttachmentClear* depthClear,const NativeWorldAttachmentClear* colorClear){
    auto pass=[MTL4RenderPassDescriptor new];
    const bool clearDepth=depthClear&&(depthClear->mask&ScreenAttachmentDepth);
    const bool clearColor=colorClear&&(colorClear->mask&ScreenAttachmentColor);
    pass.depthAttachment.texture=originalShadow->Depth();
    pass.depthAttachment.loadAction=clearDepth?MTLLoadActionClear:(depthInitialized?MTLLoadActionLoad:MTLLoadActionDontCare);
    pass.depthAttachment.storeAction=MTLStoreActionStore;
    if(clearDepth)pass.depthAttachment.clearDepth=depthClear->values.depth;
    pass.colorAttachments[0].texture=originalShadow->Color();
    pass.colorAttachments[0].loadAction=clearColor?MTLLoadActionClear:(colorInitialized?MTLLoadActionLoad:MTLLoadActionDontCare);
    pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    if(clearColor){const auto& c=colorClear->values.color;pass.colorAttachments[0].clearColor=MTLClearColorMake(c[0],c[1],c[2],c[3]);}
    pass.renderTargetWidth=originalShadow->Target().width;pass.renderTargetHeight=originalShadow->Target().height;
    shadowEncoder=[command renderCommandEncoderWithDescriptor:pass];Require(shadowEncoder!=nil,"Cannot encode actual ordered native shadow attachments");
    depthInitialized|=clearDepth;colorInitialized|=clearColor;
   };
   std::span<const ShadowEvent> remaining(shadowEvents);
   const auto& shadowTarget=originalShadow->Target();
   // PrepareOriginalShadowSource has already verified the queue, fresh named
   // versions, actual attachment sizes, formats and single-sample pairing.
   const bool combineInitialShadowClears=
    !offscreenSource&&!continuation&&frame.originalShadow->textures->redrawColor&&!colorInitialized&&
    shadowTarget.samples==1&&shadowTarget.depth==MTLPixelFormatDepth32Float&&shadowTarget.colors[0]==MTLPixelFormatRGBA8Unorm&&
    remaining.size()>=2&&remaining[0].clear&&remaining[1].clear&&
    CanCombineInitialShadowClears(shadowClears[remaining[0].index],frame.originalShadow->colorClearSequence,
                                shadowClears[remaining[1].index]);
   if(combineInitialShadowClears){
    openShadow(&shadowClears[remaining[0].index],&shadowClears[remaining[1].index]);
    remaining=remaining.subspan(2);
   }
   for(const auto& event:remaining){
    if(event.clear){
     const auto* clear=&shadowClears[event.index];
     closeShadow();openShadow(clear,clear);
    }
    else{
     Require(depthInitialized,"Original shadow draw preceded its actual full depth initialization");
     if(!shadowEncoder)openShadow(nullptr,nullptr);
     originalShadow->EncodeDraw(event.index,shadowEncoder);++result.shadowModelDraws;
    }
   }
   Require(shadowEncoder&&depthInitialized,"Original shadow source lost its final encoder/actual clear");
   originalShadow->EndEncoding(command,shadowEncoder);shadowEncoder=nil;
  }
		encodingProbe.Select(Family176::Other);
		if(reflectionWorldInfo)reflectionWorldInfo->EncodeDependencies(command);
		environment->EncodeDependencies(command);
		if (standardShade) standardShade->EncodeDependencies(command);
		if (!impl->normalsReady) {
			id<MTL4ComputeCommandEncoder> normals = [command computeCommandEncoder]; Require(normals != nil,"Cannot encode original terrain normal producer");
			[arguments setAddress:impl->heights.gpuAddress atIndex:0]; [arguments setTexture:impl->terrainNormals.gpuResourceID atIndex:0];
			[normals setComputePipelineState:impl->normalPipeline]; [normals setArgumentTable:arguments];
			[normals dispatchThreads:MTLSizeMake(impl->terrainNormals.width,impl->terrainNormals.height,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];
			[normals barrierAfterStages:MTLStageDispatch beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice]; [normals endEncoding];
		}
		*impl->normalsEncoded=true;
		if (screen != nullptr) screen->EncodeDependencies(command);
		encodingProbe.Select(Family176::Effects);
		if (particles) particles->EncodeDependencies(command);
  encodingProbe.Select(Family176::Other);
  if(waterSurface)waterSurface->EncodeDependencies(command);
  if(basicSurface)basicSurface->EncodeDependencies(command);
  encodingProbe.Select(Family176::Shadow);
  if(shadowColor){shadowColor->EncodeDependencies(command);shadowLua.Dependencies(command);}
  encodingProbe.Select(Family176::Other);
  if(worldInfo)worldInfo->EncodeDependencies(command);
  encodingProbe.Select(Family176::Effects);
  if(grass)grass->EncodeDependencies(command);
  if(resourceSpots)resourceSpots->EncodeDependencies(command);
  for(auto& view:waterViews)if(view.source){if(view.particles)view.particles->EncodeDependencies(command);view.preParticlesLua.Dependencies(command);view.worldLua.Dependencies(command);}
  belowWaterLua.Dependencies(command);aboveWaterLua.Dependencies(command);waterPostLua.Dependencies(command);alphaAllLua.Dependencies(command);
		if (luaNano) luaNano->EncodeDependencies(command);
		if (projectileEffects) projectileEffects->EncodeDependencies(command);

		encodingProbe.Select(Family176::Other);
		[arguments setAddress:sceneBuffer.gpuAddress atIndex:3];
		encodingProbe.Select(Family176::Shadow);
		if(!originalShadow&&!initialShadow[0]){
		// Exact original shadow camera, with independent resource success. Native
		// shadow generation does not set GL ShadowsLoaded or advertise GL4.
		MTL4RenderPassDescriptor* shadowPass = [MTL4RenderPassDescriptor new];
		shadowPass.depthAttachment.texture=impl->shadowDepth;shadowPass.depthAttachment.loadAction=MTLLoadActionClear;
		shadowPass.depthAttachment.storeAction=MTLStoreActionStore;shadowPass.depthAttachment.clearDepth=1;
		shadowPass.renderTargetWidth=impl->shadowDepth.width;shadowPass.renderTargetHeight=impl->shadowDepth.height;
		id<MTL4RenderCommandEncoder> shadowEncoder=[command renderCommandEncoderWithDescriptor:shadowPass]; Require(shadowEncoder!=nil,"Cannot encode original native shadow producer");
		[shadowEncoder setDepthStencilState:depthState];[shadowEncoder setDepthClipMode:MTLDepthClipModeClip];
		[shadowEncoder setViewport:MTLViewport{0,0,double(impl->shadowDepth.width),double(impl->shadowDepth.height),0,1}];
		[shadowEncoder setScissorRect:MTLScissorRect{0,0,impl->shadowDepth.width,impl->shadowDepth.height}];
		[shadowEncoder setFrontFacingWinding:MTLWindingCounterClockwise];[shadowEncoder setCullMode:MTLCullModeBack];
		if(shadowEnabled && (frame.shadow->generationBits&2u)){
			[shadowEncoder setRenderPipelineState:impl->shadowTerrainPipeline];
			[shadowEncoder setDepthBias:frame.shadow->terrainDepthBias slopeScale:frame.shadow->terrainSlopeBias clamp:0];
			[arguments setAddress:impl->heights.gpuAddress atIndex:0];[arguments setAddress:uniformBuffer.gpuAddress+shadowTerrainOffset atIndex:1];
			for(const auto& batch:impl->map.batches)if(batch.count){
				[arguments setAddress:batch.instances.gpuAddress atIndex:2];[shadowEncoder setArgumentTable:arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
				[shadowEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:96 instanceCount:batch.count];++result.shadowTerrainDraws;
			}
		}
		[shadowEncoder setDepthBias:0 slopeScale:0 clamp:0];
		if(shadowEnabled && (frame.shadow->generationBits&4u)){
			[shadowEncoder setRenderPipelineState:impl->shadowModelPipeline];
			for(const auto& piece:pieces)if(piece.castsShadow){
				markLegacyPiece(piece,true);
				[shadowEncoder setRenderPipelineState:piece.engineStandard?impl->engineStandardShadowPipeline:impl->shadowModelPipeline];
				[arguments setAddress:piece.geometry->vertices.gpuAddress atIndex:0];[arguments setAddress:uniformBuffer.gpuAddress+piece.shadowUniformOffset atIndex:1];
				bindModelSource(piece);[arguments setTexture:piece.other.gpuResourceID atIndex:1];[shadowEncoder setArgumentTable:arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
				[shadowEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:piece.geometry->count indexType:MTLIndexTypeUInt32 indexBuffer:piece.geometry->indices.gpuAddress indexBufferLength:piece.geometry->indices.length];++result.shadowModelDraws;
			}
		}
		[shadowEncoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[shadowEncoder endEncoding];
  if(shadowColor)shadowColor->Encode(command,[&](id<MTL4RenderCommandEncoder> encoder){shadowLua.Encode(encoder);});
		}

		encodingProbe.Select(Family176::Other);
		reflectionGround.initialClear->TryInitialAttachmentClear182(reflectionPass,frame.sourceDrawFrame,frame.simulationFrame);
		id<MTL4RenderCommandEncoder> reflectionEncoder=[command renderCommandEncoderWithDescriptor:reflectionPass];Require(reflectionEncoder!=nil,"Cannot encode original environment reflection face");
  reflectionPass.colorAttachments[0].loadAction=MTLLoadActionLoad;
  reflectionPass.depthAttachment.loadAction=MTLLoadActionLoad;
		[reflectionEncoder setViewport:MTLViewport{0,0,double(environmentFrame.reflectionSize),double(environmentFrame.reflectionSize),0,1}];
		[reflectionEncoder setScissorRect:MTLScissorRect{0,0,environmentFrame.reflectionSize,environmentFrame.reflectionSize}];
  reflectionGround.initialClear->EncodeDraw(0,reflectionEncoder);
		environment->EncodeSky(reflectionEncoder,true);
  if(reflectionGround.terrain){
   [reflectionEncoder barrierAfterStages:MTLStageVertex|MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
   [reflectionEncoder endEncoding];reflectionEncoder=nil;
   reflectionGround.terrain->EncodeDependencies(command);if(reflectionGround.cus)reflectionGround.cus->EncodeDependencies(command);
   reflectionGround.ordered->Dependencies(command);
   reflectionEncoder=[command renderCommandEncoderWithDescriptor:reflectionPass];Require(reflectionEncoder,"Cannot resume original IBL face for actual ground source");
   [reflectionEncoder barrierAfterQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
   reflectionGround.ordered->Encode(command,reflectionEncoder,reflectionPass);reflectionGround.terrain->FinishOrderedDraws();
   result.reflectionTerrainDraws=uint32_t(reflectionGround.terrain->EncodedDrawCount());
  }
		[reflectionEncoder endEncoding];environment->FinishReflection(command);
		if(pbrEnvironment)pbrEnvironment->Encode(command,sourceSubmission);
  // The source clear was already prepared/validated. Discard only attachments
  // it fully overwrites; its original draw and ordinal remain unchanged.
  const unsigned initialDiscard=NativeInitialClearDiscardMask(mainGround.initialClearSource.front(),
   mainGround.target.logicalWidth,mainGround.target.logicalHeight);
  if(initialDiscard&ScreenAttachmentColor)pass.colorAttachments[0].loadAction=MTLLoadActionDontCare;
  if(initialDiscard&ScreenAttachmentDepth)pass.depthAttachment.loadAction=MTLLoadActionDontCare;
  if(initialDiscard&ScreenAttachmentStencil)pass.stencilAttachment.loadAction=MTLLoadActionDontCare;
  mainGround.initialClear->TryInitialAttachmentClear182(pass,frame.sourceDrawFrame,frame.simulationFrame);
		id<MTL4RenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
  // Metal consumes the descriptor when creating the encoder. Every later
  // reuse must preserve this frame's preceding writes, including MRT resumes.
  pass.colorAttachments[0].loadAction=MTLLoadActionLoad;
  pass.depthAttachment.loadAction=MTLLoadActionLoad;
  pass.stencilAttachment.loadAction=MTLLoadActionLoad;
		Require(encoder != nil, "Cannot create direct Metal4 world encoder");
  // The new borrowed pass remains scheduler-owned through its original close,
  // including errors after encodeLuaDraws returns and before final publication.
  // Both217 Bloom and218 SSAO use this same scheduler ownership/error guard.
  struct BorrowedSceneFailure217 {
   id<MTL4RenderCommandEncoder> __strong& live;bool borrowed=false;
   ~BorrowedSceneFailure217(){if(borrowed&&live){[live endEncoding];live=nil;}}
  } borrowedScene217{encoder};
#ifdef RECOIL_METAL4_LIVE_SCENE
  static bool reportedInitialLoad=false;
  if(!reportedInitialLoad){LOG("[NativeTBDR] initial_load_discard_mask=%u sourceDrawFrame=%llu",initialDiscard,
   static_cast<unsigned long long>(frame.sourceDrawFrame));reportedInitialLoad=true;}
#endif
		result.renderEncoderClass = NSStringFromClass([encoder class]).UTF8String;
		const auto& viewport = mappedViewport->viewport;
		const auto& scissor = mappedViewport->scissor;
		const auto restoreWorldRaster = [&] {
			[encoder setViewport:MTLViewport{viewport[0],viewport[1],viewport[2],viewport[3],0,1}];
			[encoder setScissorRect:MTLScissorRect{scissor[0],scissor[1],scissor[2],scissor[3]}];
		};
		auto setupWorldState = [&](MTLCullMode cull,id<MTLDepthStencilState> depth,id<MTLBuffer> scene) {
			restoreWorldRaster();
			[encoder setCullMode:cull];[encoder setDepthClipMode:MTLDepthClipModeClip];[encoder setDepthStencilState:depth];
			[encoder setDepthBias:0 slopeScale:0 clamp:0];[encoder setStencilFrontReferenceValue:0 backReferenceValue:0];
			[encoder setTriangleFillMode:MTLTriangleFillModeFill];
			[arguments setAddress:scene.gpuAddress atIndex:3];
		};
  auto setupViewport = [&] { setupWorldState(MTLCullModeNone,depthState,sceneBuffer); };
  ModelPassCounters160 passCounters160;
  if(frame.opaqueModelFrame)for(const auto& phase:frame.opaqueModelFrame->passes)if(phase.deferred&&phase.mrt){
   ++passCounters160.modelMRTPhases;if(!phase.mrt->clearBefore)++passCounters160.modelMRTLoadPhases;
  }
  const bool pass160Enabled=ModelPass160Enabled();
  // Submission-local strong identity: an ended encoder cannot be destroyed
  // and have its address recycled while its consumer coverage is remembered.
  id<MTL4RenderCommandEncoder> __strong coveredConsumerEncoder196=nil;
  MTLStages coveredConsumerQueueStages196=0;
  const auto preserveModelPassState160=[&](
   id<MTL4RenderCommandEncoder> selectedEncoder,MTL4RenderPassDescriptor* selectedPass,
   MTLStages priorProducerStages,MTLStages priorQueueStages,bool reuseConsumer196){
   // The intervention admits only zero-draw forward/alpha phases or the
   // MRT CPU-lifecycle proof. No GPU producer/consumer is added at this
   // boundary, so keep the encoder WITHOUT an unsupported in-pass fragment
   // wait. The next nonempty phase still takes the original full boundary.
   if(!EmptyModelPassFusionInterventionEnabled()){
   // Keep every producer on the historical broad-continuation path.
   [selectedEncoder barrierAfterStages:priorProducerStages beforeQueueStages:priorQueueStages visibilityOptions:MTL4VisibilityOptionDevice];
   if(!reuseConsumer196){coveredConsumerEncoder196=nil;coveredConsumerQueueStages196=0;}
   else if(coveredConsumerEncoder196!=selectedEncoder){
    coveredConsumerEncoder196=selectedEncoder;coveredConsumerQueueStages196=0;
   }
   // This consumer only waits for encoders preceding selectedEncoder. The
   // first consumer already covers all subsequent Vertex/Fragment work in
   // this SAME encoder. Destination stages and Device visibility stay fixed.
   if(!reuseConsumer196||(coveredConsumerQueueStages196&priorQueueStages)!=priorQueueStages){
    [selectedEncoder barrierAfterQueueStages:priorQueueStages beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
    if(reuseConsumer196)coveredConsumerQueueStages196|=priorQueueStages;
   }
   [selectedEncoder barrierAfterEncoderStages:MTLStageVertex|MTLStageFragment beforeEncoderStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
   }
   // The removed new encoder formerly reset these dynamic states. Keep that
   // local reset, including empty/culled phases; the next real draw binds its
   // own pipeline, depth state and argument table as in the original path.
   [selectedEncoder setViewport:MTLViewport{0,0,double(selectedPass.renderTargetWidth),double(selectedPass.renderTargetHeight),0,1}];
   [selectedEncoder setScissorRect:MTLScissorRect{0,0,selectedPass.renderTargetWidth,selectedPass.renderTargetHeight}];
   [selectedEncoder setVertexAmplificationCount:1 viewMappings:nullptr];
   [selectedEncoder setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0];
   // No native path enables depth bounds. Do not call the unsupported Apple10
   // depth-bounds setter on Apple9 merely to restate its disabled default.
   [selectedEncoder setFrontFacingWinding:MTLWindingClockwise];
   [selectedEncoder setCullMode:MTLCullModeNone];
   [selectedEncoder setDepthClipMode:MTLDepthClipModeClip];
   [selectedEncoder setDepthBias:0 slopeScale:0 clamp:0];
   [selectedEncoder setStencilFrontReferenceValue:0 backReferenceValue:0];
   [selectedEncoder setBlendColorRed:0 green:0 blue:0 alpha:0];
   [selectedEncoder setTriangleFillMode:MTLTriangleFillModeFill];
   [selectedEncoder setDepthStencilState:nil];
   [selectedEncoder setArgumentTable:nil atStages:MTLRenderStageVertex|MTLRenderStageFragment];
  };
  const auto keepForwardModelPass160=[&](const std::shared_ptr<PreparedOpaqueModelSource>& models,size_t phase,
   id<MTL4RenderCommandEncoder> selectedEncoder,MTL4RenderPassDescriptor* selectedPass,
   MTLStages priorProducerStages,MTLStages priorQueueStages){
   if(!pass160Enabled||!selectedEncoder)return false;
   const auto report=models->TryKeepForwardPass160(phase,selectedPass);
   passCounters160.Observe(report);
   if(!report.noCommands)return false;
   preserveModelPassState160(selectedEncoder,selectedPass,priorProducerStages,priorQueueStages,true);
   return true;
  };
  ModelPassCounters165 passCounters165;
  const bool pass165Enabled=ModelPass165Enabled();
  const auto loadedMainPass165=[&]{
   // Every main opaque entry/resume above uses this same descriptor. Check
   // its actual backing identities and subresources, not matching dimensions
   // alone. Water/alpha/MSAA and PostDeferred callbacks are outside this gate.
   if(!pass||mainAttachments->Samples()!=1||pass.defaultRasterSampleCount!=1||
    !pass.renderTargetWidth||!pass.renderTargetHeight)return false;
   const auto preserved=[&](MTLRenderPassAttachmentDescriptor* attachment,id<MTLTexture> expected){
    return expected&&attachment.texture==expected&&expected.sampleCount==1&&
     expected.textureType==MTLTextureType2D&&expected.storageMode!=MTLStorageModeMemoryless&&
     expected.width==pass.renderTargetWidth&&expected.height==pass.renderTargetHeight&&
     !attachment.level&&!attachment.slice&&!attachment.depthPlane&&
     attachment.loadAction==MTLLoadActionLoad&&attachment.storeAction==MTLStoreActionStore&&!attachment.resolveTexture;
   };
   if(!preserved(pass.colorAttachments[0],mainAttachments->Color())||
    !preserved(pass.depthAttachment,mainAttachments->Depth())||
    !preserved(pass.stencilAttachment,mainAttachments->Stencil()))return false;
   for(unsigned n=1;n<8;++n)if(pass.colorAttachments[n].texture||pass.colorAttachments[n].resolveTexture)return false;
   return true;
  };
  const auto keepMRTBoundary165=[&](size_t phase){
   if(!pass165Enabled){++passCounters165.disabled;return false;}
   if(!encoder){++passCounters165.noEncoder;return false;}
   if(!opaquePostDeferredDraws[phase].empty()){++passCounters165.postDeferred;return false;}
   if(!loadedMainPass165()){++passCounters165.attachment;return false;}
   ++passCounters165.queries;
   const auto effect=opaqueModels->InspectPhaseExecution165(phase,frame.opaqueModelFrame.get(),frame.sourceDrawFrame,sourceSubmission);
   if(!effect.HasOnlyCPULifecycle()){++passCounters165.unknown;return false;}
   return true;
  };
		setupViewport();
  mainGround.initialClear->EncodeDraw(0,encoder);
  if(mainGround.terrain){
   // A leading MRT/postprocess command closes the current encoder itself.
   // With no ground dependencies here, closing/reopening before that command
   // creates an empty Load/Store pass. Keep the clear encoder until its actual
   // consumer boundary. If an ordinary draw comes first, preserve the old
   // fresh-encoder state and all dependency/barrier behavior.
   const auto& ordered=*mainGround.ordered;
   constexpr uint64_t absent=std::numeric_limits<uint64_t>::max();
   const uint64_t firstDraw=ordered.draws.empty()?absent:ordered.draws.front().first;
   const uint64_t firstCommand=ordered.commands.empty()?absent:ordered.commands.front().first;
   const uint64_t firstEffect=ordered.ssao&&ordered.ssao->DrawCount()?ordered.ssao->SequenceAt(0):absent;
   const bool skipEmptyPass=mainGround.mapMRT&&std::min(firstCommand,firstEffect)<firstDraw;
   if(!skipEmptyPass){
    [encoder barrierAfterStages:MTLStageVertex|MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
    [encoder endEncoding];encoder=nil;
    if(!mainGround.mapMRT){mainGround.terrain->EncodeDependencies(command);if(mainGround.cus)mainGround.cus->EncodeDependencies(command);
     mainGround.ordered->Dependencies(command);}
    encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder,"Cannot resume main target for original ground source");
    [encoder barrierAfterQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
   }
#ifdef RECOIL_METAL4_LIVE_SCENE
   // One receipt per process, outside the positive-gameplay measurement in the
   // usual route. No per-frame timers or extra GPU synchronization.
   static bool reportedEmptyPass=false;
   if(skipEmptyPass&&!reportedEmptyPass){
    LOG("[NativeTBDR] eliminated_empty_main_pass=1 sourceDrawFrame=%llu firstDraw=%llu firstCommand=%llu firstEffect=%llu",
     static_cast<unsigned long long>(frame.sourceDrawFrame),static_cast<unsigned long long>(firstDraw),
     static_cast<unsigned long long>(firstCommand),static_cast<unsigned long long>(firstEffect));
    reportedEmptyPass=true;
   }
#endif
   mainGround.ordered->Encode(command,encoder,pass);mainGround.terrain->FinishOrderedDraws();
   if(mainGround.mapMRT)++result.mapGBufferDepthPasses;
   result.terrainDraws=uint32_t(mainGround.terrain->EncodedDrawCount());
  }
  static const bool combined220=[](){const char* v=std::getenv("BAR_NATIVE_COMBINED_OVERLAP220");return v&&std::strcmp(v,"1")==0;}();
  const auto localDraws220=[](const auto& draws){return std::all_of(draws.begin(),draws.end(),[](const auto& d){
   return d.kind==LuaDrawKind::ReclaimField||d.kind==LuaDrawKind::ResourceSpots||d.kind==LuaDrawKind::MapEdge||d.kind==LuaDrawKind::UnitPrimitive;
  });};
  bool scopedPair220=combined220&&mainAttachments->Samples()==1&&mainGround.source->groundInvoked&&
   !terrainCopyCoordinates&&terrainSnapshot->SingleImage()&&!terrainSnapshot->SamplesImage()&&
   (!modelGBuffer||!modelGBuffer->RequiresEncode())&&localDraws220(preDecalDraws)&&localDraws220(preUnitDraws)&&
   opaqueModels&&opaqueModels->CanScopeFirstPair220()&&opaquePostDeferredDraws.size()>=2&&opaquePostDeferredDraws[0].empty();
  if(scopedPair220){originalLifetime->combinedFence220=[lease.Device() newFence];if(!originalLifetime->combinedFence220)scopedPair220=false;}
		[encoder endEncoding];
  if(mainGround.source->groundInvoked){
   terrainSnapshot->EncodeScopedVertex220(command,scopedPair220,originalLifetime->combinedFence220);
   // Resample the immutable terrain-only snapshot now, never a later main
   // depth resolve after units, below-water objects, or water have written.
   if(terrainCopyCoordinates)encodeLogicalDepth(terrainSnapshot->SingleImage(),particleTerrainDepth,terrainCopyCoordinates);
  }
		pass.colorAttachments[0].loadAction=MTLLoadActionLoad;pass.depthAttachment.loadAction=MTLLoadActionLoad;pass.stencilAttachment.loadAction=MTLLoadActionLoad;
		encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"Cannot restore original world attachments after terrain-depth producer");
  // Preserve the whole callback pass, including inherited non-snapshot reads.
  if(scopedPair220)[encoder waitForFence:originalLifetime->combinedFence220 beforeEncoderStages:MTLStageVertex];
  setupViewport();
		auto encodeLuaDraws = [&](const std::vector<LuaDraw>& draws, bool preUnit, bool preDecals=false) {
			WorldEncodingProbe176::Scope effectScope(encodingProbe,Family176::Effects);
   // Only the fixed main-world providers are admitted. Prepared packet indices
   // and source ordinals are distinct; use the retained source map where needed.
   const auto sceneConsumer217=[&](const LuaDraw& next){
    if(next.kind==LuaDrawKind::SensorLOS){
     if(next.index>=sensorLOS.size()||next.index>=sensorLOSSources.size()||!sensorLOS[next.index]||sensorLOS[next.index]->Empty())return false;
     const auto* source=sensorLOSSources[next.index];
     if(!source||source->sequence!=next.sequence||source->sourceDrawFrame!=frame.sourceDrawFrame||!source->program.defines||source->program.defines->stencilPass)return false;
     const auto* height=source->heightImage.object?dynamic_cast<const NativeMapTextureBacking*>(source->heightImage.object->backing.get()):nullptr;
     if(!height||height->role!=NativeMapTextureRole::Height)return false;
     const auto* backing=source->stencilImage.object?dynamic_cast<const ui::NativeScreenTextureBacking*>(source->stencilImage.object->backing.get()):nullptr;
     const auto& image=source->stencilDependency;
     return backing&&backing->image==image&&image&&image->renderTarget&&image->generation&&
      image->format==ScreenTextureFormat::R8Unorm&&!image->sourceTexture&&!image->postprocessVersion&&!image->incomplete&&!image->levelDefinitions;
    }
    if(next.kind==LuaDrawKind::UnitPrimitive){
     if(!unitPrimitives||next.index>=unitPrimitives->DrawCount()||unitPrimitives->SequenceAt(next.index)!=next.sequence)return false;
     const auto& source=*unitPrimitiveSources.at(next.sequence);
     return !source.atlas&&(source.kind==NativeUnitPrimitiveKind::TeamPlatter||source.kind==NativeUnitPrimitiveKind::AllySelected||
      (source.selected31657&&IsNativeSelectedUnits(source.kind)));
    }
    if(next.kind==LuaDrawKind::WeaponRange){
     if(next.index>=weaponRanges.size()||weaponRanges[next.index].second->Empty())return false;
     const auto& source=*weaponRanges[next.index].first;
     if(source.sequence!=next.sequence||!source.program.defines)return false;
     if(!source.program.defines->composite){
      const auto* height=source.height.object?dynamic_cast<const NativeMapTextureBacking*>(source.height.object->backing.get()):nullptr;
      if(!height||height->role!=NativeMapTextureRole::Height)return false;
     }
     if(source.program.defines->composite||source.program.maskClip>0.5f){
      const auto* backing=source.coverageMask.object?dynamic_cast<const ui::NativeScreenTextureBacking*>(source.coverageMask.object->backing.get()):nullptr;
      const auto& image=source.coverageDependencyImage;
      if(!backing||backing->image!=image||!image||!image->renderTarget||!image->generation||
       image->format!=ScreenTextureFormat::R8Unorm||image->sourceTexture||image->postprocessVersion||image->incomplete||image->levelDefinitions)return false;
     }
     return true;
    }
    return false;
   };
#if defined(RECOIL_METAL4_LIVE_SCENE)
   static const bool probe217=[](){const char* value=std::getenv("BAR_NATIVE_BLOOM217_PROBE");return value&&std::strcmp(value,"1")==0;}();
   static std::atomic<unsigned> probeFrames217{0};
   const bool probeWindow217=probe217&&frame.sourceDrawFrame>=2048&&frame.simulationFrame>=450&& &draws==&laterWorldDraws;
   bool probeThis217=false;
   uint64_t probeNext217=0;id<MTL4RenderCommandEncoder> __unsafe_unretained probeEncoder217=nil;
#endif
			for (const auto& draw: draws) {
    const bool commandLevel=(draw.kind==LuaDrawKind::Blockout&&blockout->IsDepthPass(draw.index))||draw.kind==LuaDrawKind::ScreenTarget||draw.kind==LuaDrawKind::Bloom||draw.kind==LuaDrawKind::Distortion||draw.kind==LuaDrawKind::SourceCopy||draw.kind==LuaDrawKind::VolumetricCloud||draw.kind==LuaDrawKind::SensorPreviewTarget;
    if(!commandLevel&&encoder==nil){encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"Cannot resume actual scene for ordered Lua draw");[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];}
    if(!commandLevel)setupViewport();
				switch (draw.kind) {
					case LuaDrawKind::ObjectSpotlight: objectSpotlights->EncodeDraw(draw.index,encoder);break;
					case LuaDrawKind::Blockout:
						if(blockout->IsDepthPass(draw.index)){
							if(encoder){[encoder endEncoding];encoder=nil;}
							blockout->EncodeDepthPass(draw.index,command);
							Require(postprocess!=nullptr,"Blockout depth pass lost its publication owner");
							postprocess->MarkExternalEncoded(blockout->OutputAt(draw.index));
						}else blockout->EncodeDraw(draw.index,encoder);
						break;
					case LuaDrawKind::ReclaimField: reclaimFields->EncodeDraw(draw.index,encoder);break;
					case LuaDrawKind::Grass: grass->EncodeDraw(draw.index,encoder);break;
     case LuaDrawKind::ResourceSpots: resourceSpots->EncodeDraw(draw.index,encoder);break;
     case LuaDrawKind::SensorLOS: sensorLOS[draw.index]->Encode(encoder);break;
					case LuaDrawKind::Commands: commands->EncodeDraw(draw.index,encoder);++result.luaCommandsDraws;break;
					case LuaDrawKind::BuildSquare: buildSquares[draw.index].second->Encode(encoder);break;
					case LuaDrawKind::WeaponRange: weaponRanges[draw.index].second->Encode(encoder);break;
					case LuaDrawKind::Sensor31657Circle: sensorCircles[draw.index].second->Encode(encoder);break;
					case LuaDrawKind::SensorPreviewFinal:
						Require(!sensorPreview.at(draw.index)->IsTarget(),"Original Preview final draw became a private target");
						sensorPreview[draw.index]->Encode(encoder);break;
					case LuaDrawKind::SensorPreviewTarget:
						Require(postprocess&&sensorPreview.at(draw.index)->IsTarget(),"Original Preview target has no common image transaction");
						if(encoder){[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];encoder=nil;}
						sensorPreview[draw.index]->EncodeTarget(command);
						Require(sensorPreview[draw.index]->Encoded(),"Original Preview target did not encode its actual source command");
						postprocess->MarkExternalEncoded(sensorPreview[draw.index]->OutputVersion());break;
					case LuaDrawKind::ScreenTarget:
						Require(screen!=nullptr && !preDecals,"World RTT lost its original actual callin insertion");
						if(encoder){[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];encoder=nil;}
						screen->EncodeDeferredTarget(command,draw.index);break;
					case LuaDrawKind::StartBox: startBoxes[draw.index].second->Encode(encoder);++result.luaStartBoxDraws;break;
     case LuaDrawKind::Distortion:
      Require(distortions!=nullptr,"Original Distortion has no paired source operation consumer");
      if(encoder){[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];encoder=nil;}
      distortions->EncodeDraw(draw.index,command);break;
     case LuaDrawKind::Bloom:
      Require(bloom!=nullptr,"Original Bloom command has no prepared producer");
      if(encoder){[encoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];encoder=nil;}
      {
       const auto ordinal=size_t(&draw-draws.data());
       const LuaDraw* next=ordinal+1<draws.size()?&draws[ordinal+1]:nullptr;
       const bool admitted=!offscreenSource&&!continuation&&postprocess&& &draws==&laterWorldDraws&&next&&
        sceneConsumer217(*next)&&bloom->CanEncodeSceneDraw(draw.index,pass);
#if defined(RECOIL_METAL4_LIVE_SCENE)
       if(probeWindow217&&draw.index+1==bloom->DrawCount()&&probeFrames217.load(std::memory_order_relaxed)<4&&
          probeFrames217.fetch_add(1,std::memory_order_relaxed)<4){
        probeThis217=true;
        // Diagnose the effect independently: the ordinary admission conjunction
        // may reject the successor before ever evaluating Bloom's own gate.
        const unsigned effectRejection217=unsigned(bloom->InspectSceneDraw(draw.index,pass));
        const bool scopeEligible217=!offscreenSource&&!continuation&&postprocess&& &draws==&laterWorldDraws;
        const bool consumerEligible217=next&&sceneConsumer217(*next);
        const auto* bloomSource217=frame.luaBloomFrame&&draw.index<frame.luaBloomFrame->draws.size()?&frame.luaBloomFrame->draws[draw.index]:nullptr;
        const unsigned nextNonempty217=next&&next->kind==LuaDrawKind::SensorLOS&&next->index<sensorLOS.size()&&sensorLOS[next->index]?unsigned(!sensorLOS[next->index]->Empty()):2u;
        LOG("[NativeBloom217Inspect] source_frame=%llu sim=%llu bloom_sequence=%llu bloom_index=%llu next_sequence=%llu next_kind=%u next_index=%llu admitted=%u scope_eligible=%u consumer_eligible=%u effect_rejection=%u bloom_program=%u next_nonempty=%u",
         (unsigned long long)frame.sourceDrawFrame,(unsigned long long)frame.simulationFrame,(unsigned long long)draw.sequence,
         (unsigned long long)draw.index,(unsigned long long)(next?next->sequence:0),next?unsigned(next->kind):~0u,
         (unsigned long long)(next?next->index:0),unsigned(admitted),unsigned(scopeEligible217),unsigned(consumerEligible217),
         effectRejection217,bloomSource217?unsigned(bloomSource217->program):~0u,nextNonempty217);
       }
#endif
       if(admitted){
        encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"Cannot open scheduler-owned Bloom scene pass");
        borrowedScene217.borrowed=true;
        bloom->EncodeSceneDraw(draw.index,encoder);
        // Preserve the original successor's wait for earlier encoders. This
        // does not synchronize Bloom within this pass; admission proves that edge.
        [encoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
        // Bloom changed winding; the original resumed encoder began Clockwise.
        // Existing setupViewport and the admitted packet restore all other state.
        [encoder setFrontFacingWinding:MTLWindingClockwise];
#if defined(RECOIL_METAL4_LIVE_SCENE)
        if(probeThis217){probeNext217=next->sequence;probeEncoder217=encoder;
         LOG("[NativeBloom217Borrow] source_frame=%llu bloom_sequence=%llu next_sequence=%llu encoder=%p encoded=1",
          (unsigned long long)frame.sourceDrawFrame,(unsigned long long)draw.sequence,(unsigned long long)next->sequence,(__bridge void*)encoder);}
#endif
       }else bloom->EncodeDraw(draw.index,command);
      }
      break;
					case LuaDrawKind::GameDecal: {
						gameDecals->EncodeDraw(draw.index,encoder);const auto& source=*gameDecalSources.at(draw.sequence);
						++result.luaGameDecalDraws;result.luaGameDecalInstances+=source.program==NativeGameDecalProgram::Geometry?source.count:source.instanceCount;break;
					}
					case LuaDrawKind::UnitPrimitive: {
						unitPrimitives->EncodeDraw(draw.index,encoder);const auto& source=*unitPrimitiveSources.at(draw.sequence);const auto family=size_t(source.kind);
						if(family<result.luaUnitPrimitiveDraws.size()){++result.luaUnitPrimitiveDraws[family];result.luaUnitPrimitiveInstances[family]+=source.count;}break;
					}
					case LuaDrawKind::MapEdge: mapEdge->Encode(encoder); ++result.mapEdgeDraws; break;
					case LuaDrawKind::Overlay:
						overlays->EncodeDraw(draw.index,encoder);
						if (preDecals) ++result.luaPreDecalsDraws; else if (preUnit) ++result.luaPreUnitDraws; else ++result.luaWorldDraws;
						break;
					case LuaDrawKind::Nano: luaNano->EncodeDraw(draw.index,encoder); ++result.luaNanoDraws; break;
					case LuaDrawKind::ProjectileEffect: projectileEffects->EncodeDraw(draw.index,encoder); ++result.luaProjectileEffectDraws; break;
					case LuaDrawKind::EnergyExplosion: energyExplosion->EncodeDraw(draw.index,encoder); ++result.luaEnergyExplosionDraws; break;
					case LuaDrawKind::Lightning: lightning->EncodeDraw(draw.index,encoder); ++result.luaLightningDraws; break;
					case LuaDrawKind::DeferredLights: deferredLights->EncodeDraw(draw.index,encoder); ++result.luaDeferredLightDraws; break;
					case LuaDrawKind::Outline: outline->EncodeOperation(draw.index,encoder); break;
					case LuaDrawKind::FireSmoke: fireSmoke->EncodeDraw(draw.index,encoder); ++result.luaFireSmokeDraws; break;
					case LuaDrawKind::NapalmPreview: napalmPreview->EncodeDraw(draw.index,encoder);++result.luaNapalmPreviewDraws;break;
					case LuaDrawKind::Snow: snow->EncodeDraw(draw.index,encoder);++result.luaSnowDraws;break;
					case LuaDrawKind::FireGL4: {
						fireGL4->EncodeDraw(draw.index,encoder);
						break;
					}
					case LuaDrawKind::Flamethrower: {
						flamethrower->EncodeDraw(draw.index,encoder);
						++result.luaFlamethrowerDraws;result.luaFlamethrowerSubmittedInstances+=flamethrower->InstanceCountAt(draw.index);break;
					}
					case LuaDrawKind::Shield: shield->EncodeDraw(draw.index,encoder);++result.luaShieldDraws;break;
					case LuaDrawKind::MissileThruster: {
						missileThruster->EncodeDraw(draw.index, encoder);
						const auto& source = *missileThrusterDraws.at(draw.index);
						++result.luaMissileThrusterDraws;
						result.luaMissileThrusterSubmittedInstances += source.instanceCount;
						++result.luaMissileThrusterProgramDraws.at(static_cast<std::size_t>(source.program));
						break;
					}
					case LuaDrawKind::Blueprint: blueprint->EncodeDraw(draw.index,encoder); ++result.luaBlueprintDraws; break;
					case LuaDrawKind::PlacementGrid: placementGrid->EncodeDraw(draw.index,encoder); ++result.buildingGridDraws; break;
					case LuaDrawKind::PlacementSample: {
						const auto& sample=placementPieces[draw.index];
						[encoder setRenderPipelineState:impl->placementSamplePipelines[shadowColorPresence]];[encoder setDepthStencilState:alphaDepthState];[encoder setFrontFacingWinding:MTLWindingCounterClockwise];[encoder setCullMode:MTLCullModeBack];
						[arguments setAddress:sample.camera.gpuAddress atIndex:4];[arguments setTexture:impl->shadowDepth.gpuResourceID atIndex:7];[arguments setTexture:sampledShadowColor ? sampledShadowColor.gpuResourceID : MTLResourceID{0} atIndex:16];[arguments setTexture:environment->Reflection().gpuResourceID atIndex:14];[arguments setTexture:environment->Specular().gpuResourceID atIndex:15];
						for(size_t i=sample.begin;i<sample.end;++i){const auto& piece=pieces[i];[arguments setAddress:piece.geometry->vertices.gpuAddress atIndex:0];[arguments setAddress:uniformBuffer.gpuAddress+piece.uniformOffset atIndex:1];
							// Slot5 belongs to this pipeline's immutable light frame. Rebind
							// before each draw; another model family may use its own layout.
							[arguments setAddress:sample.modelLights.gpuAddress atIndex:5];
							bindModelSource(piece);[arguments setTexture:piece.color.gpuResourceID atIndex:0];[arguments setTexture:piece.other.gpuResourceID atIndex:1];[encoder setArgumentTable:arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:piece.geometry->count indexType:MTLIndexTypeUInt32 indexBuffer:piece.geometry->indices.gpuAddress indexBufferLength:piece.geometry->indices.length];++result.buildingSamplePieces;}
						++result.buildingSampleDraws;break;
					}
					case LuaDrawKind::SourceCopy:
					case LuaDrawKind::VolumetricCloud:
						Require(postprocess!=nullptr,"Actual source operation lacks its owned postprocess frame");
						if(encoder){[encoder endEncoding];encoder=nil;}
						if(draw.kind==LuaDrawKind::SourceCopy)postprocess->EncodeOperation(command,draw.index);
						else{Require(clouds!=nullptr,"Cloud producer lacks its actual GPU consumer");clouds->EncodeDraw(draw.index,command);postprocess->MarkExternalEncoded(clouds->OutputAt(draw.index));}
						// Consecutive command-level operations share stored scene contents; resume only for the next actual scene draw.
						break;
				}
#if defined(RECOIL_METAL4_LIVE_SCENE)
    if(probeThis217&&probeNext217==draw.sequence){
     LOG("[NativeBloom217Successor] source_frame=%llu sequence=%llu kind=%u index=%llu encoder=%p same_encoder=%u emitted=1",
      (unsigned long long)frame.sourceDrawFrame,(unsigned long long)draw.sequence,unsigned(draw.kind),(unsigned long long)draw.index,
      (__bridge void*)encoder,unsigned(encoder==probeEncoder217));probeNext217=0;
    }
#endif
    if(commandLevel&&encoder==nil)borrowedScene217.borrowed=false;
			}
   if(encoder==nil){encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"Cannot resume original world after ordered effect chain");[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];}
		};
		encodingProbe.Select(Family176::Effects);
		encodeLuaDraws(preDecalDraws,false,true);
		if(groundDecals){setupViewport();groundDecals->Encode(encoder);}
		setupViewport();
		if(particles)particles->Encode(NativeWorldParticleStage::GroundFlashes,encoder);
		encodingProbe.Select(Family176::Other);
		setupViewport();environment->EncodeSky(encoder,false);if(environmentFrame.drawSky&&environmentFrame.sky->kind!=NativeSkyKind::OriginalNull)++result.skyDraws;

		encodeLuaDraws(preUnitDraws,true);
		encodingProbe.Select(Family176::Model);
		if(modelGBuffer&&modelGBuffer->RequiresEncode()){
			[encoder endEncoding];modelGBuffer->Encode(command);result.modelGBufferDepthPasses=uint32_t(std::count_if(frame.shieldDepthFrame->modelPlan.phases.begin(),frame.shieldDepthFrame->modelPlan.phases.end(),[](const auto& phase){return phase.clearBefore;}));result.modelGBufferDepthPieces=uint32_t(modelGBuffer->PieceCount());
			encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"Cannot restore world attachments after original model GBuffer depth");setupViewport();
		}

		auto drawPieces = [&](bool alpha,id<MTLBuffer> selectedScene=nil) {
			WorldEncodingProbe176::Scope modelScope(encodingProbe,Family176::Model);
			encodingProbe.LegacyModelSetup(selectedScene!=nil);
			setupWorldState(MTLCullModeBack,alpha?alphaDepthState:depthState,selectedScene?selectedScene:sceneBuffer);
			[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
			[encoder setRenderPipelineState:alpha?impl->alphaPipeline:impl->unitPipeline];
			[arguments setTexture:impl->shadowDepth.gpuResourceID atIndex:7];[arguments setTexture:sampledShadowColor ? sampledShadowColor.gpuResourceID : MTLResourceID{0} atIndex:16];
			[arguments setTexture:environment->Reflection().gpuResourceID atIndex:14];[arguments setTexture:environment->Specular().gpuResourceID atIndex:15];
			for(const auto& piece:pieces)if(piece.visible&&piece.alpha==alpha&&piece.view==NativeWaterScenePass::Main&&
				(!opaqueModels||alpha||piece.projectile)&&(!alpha||!alphaModels[waterActive?0:2]||piece.projectile)){
				markLegacyPiece(piece,false);
				[encoder setRenderPipelineState:piece.engineStandard?(alpha?impl->engineStandardAlphaPipelines[shadowColorPresence]:impl->engineStandardPipelines[shadowColorPresence]):(piece.engineModel?impl->engineModelPipeline:(alpha?impl->alphaPipeline:impl->unitPipeline))];
				[arguments setAddress:piece.geometry->vertices.gpuAddress atIndex:0];[arguments setAddress:uniformBuffer.gpuAddress+piece.uniformOffset atIndex:1];
				[arguments setTexture:(piece.feature?brdf:environment->UnitBRDF()).gpuResourceID atIndex:3];
				bindModelSource(piece);[arguments setTexture:piece.color.gpuResourceID atIndex:0];[arguments setTexture:piece.other.gpuResourceID atIndex:1];[arguments setTexture:piece.normal.gpuResourceID atIndex:2];
				[encoder setArgumentTable:arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
				[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:piece.geometry->count indexType:MTLIndexTypeUInt32 indexBuffer:piece.geometry->indices.gpuAddress indexBufferLength:piece.geometry->indices.length];
				if(piece.feature){++result.featurePiecesDrawn;if(alpha)++result.alphaFeatureDraws;else ++result.opaqueFeatureDraws;}
				else if(piece.projectile) ++result.projectilePiecesDrawn; else ++result.piecesDrawn;
			}
		};
  static const bool forwardBoundary220=[](){const char* v=std::getenv("BAR_NATIVE_FORWARD_BOUNDARY220");return v&&std::strcmp(v,"1")==0;}();
  // A lookahead uses the actual next phase and is consumed exactly once at
  // that phase. It cannot cross a PostDeferred draw or an intervening phase.
  std::optional<bool> nextMRTKeep220;
		bool previousMRTKept165=false;
		if(opaqueModels)for(size_t n=0;n<opaqueModels->PhaseCount();++n){
			if(opaqueModels->IsDeferredPhase(n)){
    const bool keep=nextMRTKeep220.has_value()?*nextMRTKeep220:keepMRTBoundary165(n);
    nextMRTKeep220.reset();
				if(!keep&&encoder){[encoder barrierAfterStages:MTLStageVertex|MTLStageFragment beforeQueueStages:MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];encoder=nil;}
				// Always execute the actual phase once. In the proven readonly case
				// this retains prior-readability checks, empty consumer transitions,
				// the distinct producer publication and the source ordinal advance.
				if(scopedPair220&&n==0)opaqueModels->EncodeFirstMRTScoped220(command);else opaqueModels->EncodeMRTPhase(n,command);
				if(keep){
					preserveModelPassState160(encoder,pass,MTLStageVertex|MTLStageFragment,MTLStageFragment|MTLStageBlit,false);
					++passCounters165.kept;
				}
				previousMRTKept165=keep;
			}else{
				if(previousMRTKept165&&pass160Enabled&&encoder)++passCounters165.followingQueries160;
				const bool keep=keepForwardModelPass160(opaqueModels,n,encoder,pass,MTLStageVertex|MTLStageFragment,MTLStageFragment|MTLStageBlit);
				if(previousMRTKept165){if(keep)++passCounters165.followingKept160;else ++passCounters165.followingBoundary160;}
				previousMRTKept165=false;
				if(!keep){
					if(encoder){[encoder barrierAfterStages:MTLStageVertex|MTLStageFragment beforeQueueStages:MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];encoder=nil;}
					opaqueModels->EncodeForwardDependencies(n,command);
					encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder,"Cannot resume actual original forward scene attachments");
     [encoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:(scopedPair220&&n==1?MTLStageFragment:(MTLStageVertex|MTLStageFragment)) visibilityOptions:MTL4VisibilityOptionDevice];
				}
				opaqueModels->EncodeForwardPhase(n,encoder);
    if(scopedPair220&&n==1){
     // ForwardBoundary220 begin: exercised with the actual phase-transition
     // code in the CPU fixture. Do not add an encoder or move a source draw.
     bool isolatedForward220=false;
     if(forwardBoundary220&&opaquePostDeferredDraws[n].empty()&&
        n+1<opaqueModels->PhaseCount()&&opaqueModels->IsDeferredPhase(n+1)){
      nextMRTKeep220=keepMRTBoundary165(n+1);
      isolatedForward220=!*nextMRTKeep220;
     }
     // Preserve snapshot visibility for all future encoders in both modes.
     [encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex visibilityOptions:MTL4VisibilityOptionDevice];
     if(isolatedForward220){
      // A queue consumer can be moved earlier within its encoder by Metal.
      // Publish to future encoders instead, then take the exact boundary the
      // next MRT phase already required. No unknown work remains in this
      // encoder. The second publication and end are moved from that boundary.
      [encoder barrierAfterStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeQueueStages:MTLStageVertex visibilityOptions:MTL4VisibilityOptionDevice];
      [encoder barrierAfterStages:MTLStageVertex|MTLStageFragment beforeQueueStages:MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
      [encoder endEncoding];encoder=nil;
     }else{
      [encoder barrierAfterQueueStages:MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageVertex visibilityOptions:MTL4VisibilityOptionDevice];
     }
     // ForwardBoundary220 end
     static const bool receipts220=[](){const char* v=std::getenv("BAR_NATIVE_COMBINED_OVERLAP220_COUNTERS");return v&&std::strcmp(v,"1")==0;}();
     if(receipts220){static std::atomic<unsigned> seen220{0};const unsigned bit220=frame.simulationFrame==450?1u:frame.simulationFrame==1200?2u:frame.simulationFrame==2250?4u:0u;if(bit220&&!(seen220.fetch_or(bit220,std::memory_order_relaxed)&bit220))
      LOG("[NativeCombinedOverlap220] sim=%llu source_frame=%llu submission=%llu snapshot=%llu first_pair=1 local_vertex_restored=1 future_vertex_restored=1 existing_forward_boundary=%u",(unsigned long long)frame.simulationFrame,(unsigned long long)frame.sourceDrawFrame,(unsigned long long)sourceSubmission,(unsigned long long)terrainSnapshot->SingleImage().gpuResourceID._impl,unsigned(isolatedForward220));}
    }
			}
			if(!opaquePostDeferredDraws[n].empty()){
				if(!encoder){encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder,"Cannot resume actual PostDeferred scene attachments");[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];}
				encodeLuaDraws(opaquePostDeferredDraws[n],false);
			}
		}
		if(!encoder){encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder,"Cannot resume actual scene after original opaque source");[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];}
		drawPieces(false);
		encodingProbe.Select(Family176::Effects);
		if(particles)particles->Encode(NativeWorldParticleStage::OpaqueProjectiles,encoder);
  AlphaPassCounters166 passCounters166;
  const bool pass166Enabled=ModelPass166Enabled();
  const auto loadedAlphaPass166=[](MTL4RenderPassDescriptor* selectedPass,
   id<MTLTexture> color,id<MTLTexture> depth,id<MTLTexture> stencil){
   if(!selectedPass||selectedPass.defaultRasterSampleCount!=1||!selectedPass.renderTargetWidth||!selectedPass.renderTargetHeight)return false;
   const auto preserved=[&](MTLRenderPassAttachmentDescriptor* attachment,id<MTLTexture> expected){
    if(!expected)return !attachment.texture&&!attachment.resolveTexture;
    return attachment.texture==expected&&expected.sampleCount==1&&expected.textureType==MTLTextureType2D&&
     expected.storageMode!=MTLStorageModeMemoryless&&expected.width==selectedPass.renderTargetWidth&&expected.height==selectedPass.renderTargetHeight&&
     !attachment.level&&!attachment.slice&&!attachment.depthPlane&&attachment.loadAction==MTLLoadActionLoad&&
     attachment.storeAction==MTLStoreActionStore&&!attachment.resolveTexture;
   };
   if(!color||!depth||!preserved(selectedPass.colorAttachments[0],color)||
    !preserved(selectedPass.depthAttachment,depth)||!preserved(selectedPass.stencilAttachment,stencil))return false;
   for(unsigned n=1;n<8;++n)if(selectedPass.colorAttachments[n].texture||selectedPass.colorAttachments[n].resolveTexture)return false;
   return true;
  };
  const auto keepAlphaModelPass166=[&](const std::shared_ptr<PreparedOpaqueModelSource>& models,size_t phase,
   id<MTL4RenderCommandEncoder> selectedEncoder,MTL4RenderPassDescriptor* selectedPass,
   const NativeOpaqueModelFrame* invocation,id<MTLTexture> color,id<MTLTexture> depth,id<MTLTexture> stencil){
   ++passCounters166.attempts;
   if(!pass166Enabled){++passCounters166.disabled;return false;}
   if(!selectedEncoder){++passCounters166.noEncoder;return false;}
   // The caller passes its CURRENT encoder variable and the descriptor used
   // for every entry/resume on that variable, with the actual target owners.
   if(!loadedAlphaPass166(selectedPass,color,depth,stencil)){++passCounters166.attachment;return false;}
   ++passCounters166.queries;
   const auto report=models->TryKeepAlphaPass166(phase,command,selectedPass,invocation,frame.sourceDrawFrame,sourceSubmission);
   passCounters166.Observe(report,invocation->drawReflection||invocation->drawRefraction);
   if(report.reason!=AlphaKeepReason166::Kept)return false;
   preserveModelPassState160(selectedEncoder,selectedPass,MTLStageVertex|MTLStageFragment,MTLStageVertex|MTLStageFragment|MTLStageBlit,true);
   return true;
  };
		const auto encodeAlphaModels=[&](const std::shared_ptr<PreparedOpaqueModelSource>& models,
			id<MTL4RenderCommandEncoder> __strong& selectedEncoder,MTL4RenderPassDescriptor* selectedPass,
			const NativeOpaqueModelFrame* invocation,id<MTLTexture> color,id<MTLTexture> depth,id<MTLTexture> stencil){
			if(!models)return;
			WorldEncodingProbe176::Scope modelScope(encodingProbe,Family176::Model);
			for(size_t phase=0;phase<models->PhaseCount();++phase){
				Require(!models->IsDeferredPhase(phase),"Original alpha model source cannot target a deferred MRT");
				if(!keepAlphaModelPass166(models,phase,selectedEncoder,selectedPass,invocation,color,depth,stencil)&&
					!keepForwardModelPass160(models,phase,selectedEncoder,selectedPass,MTLStageVertex|MTLStageFragment,MTLStageVertex|MTLStageFragment|MTLStageBlit)){
					[selectedEncoder barrierAfterStages:MTLStageVertex|MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[selectedEncoder endEncoding];selectedEncoder=nil;
					models->EncodeForwardDependencies(phase,command);
					selectedEncoder=[command renderCommandEncoderWithDescriptor:selectedPass];Require(selectedEncoder,"Cannot resume actual alpha model color/depth attachments");
					[selectedEncoder barrierAfterQueueStages:MTLStageVertex|MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
				}
				models->EncodeForwardPhase(phase,command,selectedEncoder,selectedPass);
			}
		};
  if(waterSurface || basicSurface || legacySurface || dynamicSurface){
			encodeAlphaModels(alphaModels[0],encoder,pass,frame.alphaModelFrames[0].get(),mainAttachments->Color(),mainAttachments->Depth(),mainAttachments->Stencil());
   drawPieces(true,belowWaterScene);
   belowWaterLua.Encode(encoder);
   if(particles){particles->Encode(NativeWorldParticleStage::AlphaBelowWater,encoder);particles->Encode(NativeWorldParticleStage::NanoBelowWater,encoder);}
   encodingProbe.Select(Family176::Other);
   if(waterSurface || legacySurface || dynamicSurface){
   [encoder endEncoding];
   if(dynamicSurface){dynamicSurface->EncodeUpdates(command);dynamicSurface->EncodeDerived(command);}
   if(legacySurface)legacySurface->EncodeDependencies(command);
   if(legacyBump)legacyBump->Encode(command);
   // Source UpdateWater: underwater refraction scene precedes reflection.
   for(auto& view:waterViews)if(view.source){
    const auto& source=*view.source;
    auto waterPass=[MTL4RenderPassDescriptor new];
    waterPass.defaultRasterSampleCount=view.color.sampleCount;
    waterPass.colorAttachments[0].texture=view.color;waterPass.colorAttachments[0].loadAction=MTLLoadActionLoad;
    waterPass.colorAttachments[0].storeAction=MTLStoreActionStore;
    waterPass.depthAttachment.texture=view.depth;waterPass.depthAttachment.loadAction=MTLLoadActionLoad;
    waterPass.depthAttachment.storeAction=MTLStoreActionStore;
    waterPass.renderTargetWidth=view.color.width;waterPass.renderTargetHeight=view.color.height;
    const unsigned planarDiscard173=NativeInitialClearDiscardMask(view.initialClearSource.front(),
     uint32_t(view.color.width),uint32_t(view.color.height))&unsigned(ScreenAttachmentColor|ScreenAttachmentDepth);
    if(planarDiscard173&ScreenAttachmentColor)waterPass.colorAttachments[0].loadAction=MTLLoadActionDontCare;
    if(planarDiscard173&ScreenAttachmentDepth)waterPass.depthAttachment.loadAction=MTLLoadActionDontCare;
    view.initialClear->TryInitialAttachmentClear182(waterPass,frame.sourceDrawFrame,frame.simulationFrame);
    auto waterEncoder=[command renderCommandEncoderWithDescriptor:waterPass];Require(waterEncoder!=nil,"Cannot encode actual planar water scene");
    waterPass.colorAttachments[0].loadAction=MTLLoadActionLoad;
    waterPass.depthAttachment.loadAction=MTLLoadActionLoad;
    auto setup=[&]{
     [waterEncoder setViewport:MTLViewport{0,0,double(view.color.width),double(view.color.height),0,1}];
     [waterEncoder setScissorRect:MTLScissorRect{0,0,view.color.width,view.color.height}];
     [waterEncoder setDepthClipMode:MTLDepthClipModeClip];[waterEncoder setDepthBias:0 slopeScale:0 clamp:0];
     [waterEncoder setStencilFrontReferenceValue:0 backReferenceValue:0];
     [waterEncoder setTriangleFillMode:MTLTriangleFillModeFill];
     [arguments setAddress:view.scene.gpuAddress atIndex:3];
    };
    view.initialClear->EncodeDraw(0,waterEncoder);
#ifdef RECOIL_METAL4_LIVE_SCENE
    const unsigned planarSlot173=source.pass==NativeWaterScenePass::Reflection?1u:0u;
    if(!impl->reportedPlanar173[planarSlot173]){
     const unsigned omitted173=unsigned(bool(planarDiscard173&ScreenAttachmentColor))+unsigned(bool(planarDiscard173&ScreenAttachmentDepth));
     LOG("[NativePlanar173] source_frame=%llu view=%u width=%llu height=%llu samples=%llu discard_mask=%u omitted_initial_load_attachments=%u logical_source_bytes=%llu encoded=1 timing=0",
      static_cast<unsigned long long>(frame.sourceDrawFrame),planarSlot173,
      static_cast<unsigned long long>(view.color.width),static_cast<unsigned long long>(view.color.height),
      static_cast<unsigned long long>(view.color.sampleCount),planarDiscard173,omitted173,
      static_cast<unsigned long long>(uint64_t(view.color.width)*view.color.height*4u*omitted173));
     impl->reportedPlanar173[planarSlot173]=true;
    }
#endif
    setup();if(source.drawSky)environment->EncodeWaterSky(waterEncoder,view.sky);
    if(source.groundSource){
     // Sky has completed. Prepare source image producers outside the render
     // encoder, then resume these same water attachments with Load. This is
     // the sole ground consumer, before the original model phase starts.
     [waterEncoder barrierAfterStages:MTLStageVertex|MTLStageFragment beforeQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
     [waterEncoder endEncoding];waterEncoder=nil;
     waterPass.colorAttachments[0].loadAction=MTLLoadActionLoad;
     waterPass.depthAttachment.loadAction=MTLLoadActionLoad;
     view.groundTerrain->EncodeDependencies(command);
     if(view.groundCUS)view.groundCUS->EncodeDependencies(command);
     view.groundLua->Dependencies(command);
     waterEncoder=[command renderCommandEncoderWithDescriptor:waterPass];Require(waterEncoder,"Cannot resume actual water targets for original ground source");
     [waterEncoder barrierAfterQueueStages:MTLStageVertex|MTLStageFragment|MTLStageDispatch|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
     // The single queue merges PreForward, selected squares, PostForward,
     // genuine CUS/typed/generic commands and clears by their shared ordinal.
     view.groundLua->Encode(command,waterEncoder,waterPass);
     view.groundTerrain->FinishOrderedDraws();
     result.waterSceneTerrainDraws+=view.groundTerrain->EncodedDrawCount();
    }
    // The source frame owns the exact engine -> whole CUS -> legacy order
    // for units and features. Real/unknown dependencies run outside the render
    // encoder; proved no-command phases may keep these SAME water attachments.
    encodingProbe.Select(Family176::Model);
    if(view.opaqueModels&&view.opaqueModels->PhaseCount()){
     waterPass.colorAttachments[0].loadAction=MTLLoadActionLoad;
     waterPass.depthAttachment.loadAction=MTLLoadActionLoad;
     for(size_t n=0;n<view.opaqueModels->PhaseCount();++n){
      Require(!view.opaqueModels->IsDeferredPhase(n),"Planar water source tried to encode a main MRT phase");
      if(!keepForwardModelPass160(view.opaqueModels,n,waterEncoder,waterPass,MTLStageFragment,MTLStageFragment|MTLStageBlit)){
       if(waterEncoder){[waterEncoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[waterEncoder endEncoding];waterEncoder=nil;}
       view.opaqueModels->EncodeForwardDependencies(n,command);
       waterEncoder=[command renderCommandEncoderWithDescriptor:waterPass];Require(waterEncoder,"Cannot resume actual water targets for original opaque source");
       [waterEncoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
      }
      view.opaqueModels->EncodeForwardPhase(n,waterEncoder);
      // Keep only the inter-model boundary eligible for inspection. The final
      // exit to legacy pieces/particles retains its original fresh encoder.
      if(!pass160Enabled||n+1==view.opaqueModels->PhaseCount()){
       [waterEncoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment|MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
       [waterEncoder endEncoding];waterEncoder=nil;
      }
     }
     waterEncoder=[command renderCommandEncoderWithDescriptor:waterPass];Require(waterEncoder,"Cannot resume original water projectiles and alpha after opaque source");
     [waterEncoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
    }
    for(bool alpha:{false,true}){
					encodingProbe.Select(Family176::Model);
					if(alpha)encodeAlphaModels(view.alphaModels,waterEncoder,waterPass,view.source->frame.alphaModelFrames[0].get(),view.color,view.depth,nil);
     setup();[waterEncoder setFrontFacingWinding:MTLWindingCounterClockwise];[waterEncoder setCullMode:MTLCullModeBack];
     [waterEncoder setDepthStencilState:alpha?alphaDepthState:depthState];
     [arguments setTexture:impl->shadowDepth.gpuResourceID atIndex:7];[arguments setTexture:sampledShadowColor ? sampledShadowColor.gpuResourceID : MTLResourceID{0} atIndex:16];
     [arguments setTexture:environment->Reflection().gpuResourceID atIndex:14];[arguments setTexture:environment->Specular().gpuResourceID atIndex:15];
     for(const auto& piece:pieces)if(piece.view==source.pass && piece.visible && piece.alpha==alpha&&
						(!view.opaqueModels||alpha||piece.projectile)&&(!alpha||!view.alphaModels||piece.projectile)){
      markLegacyPiece(piece,false);
      [arguments setAddress:(piece.engineModel&&!piece.engineStandard?view.legacyModelScene:view.modelScene).gpuAddress atIndex:3];
      [waterEncoder setRenderPipelineState:piece.engineStandard?(alpha?impl->waterStandardAlphaPipelines[shadowColorPresence]:impl->waterStandardPipelines[shadowColorPresence]):piece.engineModel?impl->waterEnginePipeline:impl->waterUnitPipeline];
      [arguments setAddress:piece.geometry->vertices.gpuAddress atIndex:0];[arguments setAddress:uniformBuffer.gpuAddress+piece.uniformOffset atIndex:1];
      [arguments setTexture:(piece.feature?brdf:environment->UnitBRDF()).gpuResourceID atIndex:3];
      bindModelSource(piece);[arguments setTexture:piece.color.gpuResourceID atIndex:0];[arguments setTexture:piece.other.gpuResourceID atIndex:1];[arguments setTexture:piece.normal.gpuResourceID atIndex:2];
      [waterEncoder setArgumentTable:arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
      [waterEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:piece.geometry->count indexType:MTLIndexTypeUInt32 indexBuffer:piece.geometry->indices.gpuAddress indexBufferLength:piece.geometry->indices.length];++result.waterSceneModelDraws;
     }
     encodingProbe.Select(Family176::Effects);
     if(!alpha && view.particles)view.particles->Encode(NativeWorldParticleStage::OpaqueProjectiles,waterEncoder);
    }
    view.preParticlesLua.Encode(waterEncoder);
    if(view.particles){
     const bool reflected=source.pass==NativeWaterScenePass::Reflection;
     view.particles->Encode(reflected?NativeWorldParticleStage::AlphaAboveWater:NativeWorldParticleStage::AlphaBelowWater,waterEncoder);
     view.particles->Encode(reflected?NativeWorldParticleStage::NanoAboveWater:NativeWorldParticleStage::NanoBelowWater,waterEncoder);
    }
    view.worldLua.Encode(waterEncoder);
    encodingProbe.Select(Family176::Other);
    [waterEncoder barrierAfterStages:MTLStageFragment beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
    [waterEncoder endEncoding];
   }
   if(legacySurface&&frame.legacyWater->refraction){
    requireLogicalCopySource();mainAttachments->EncodeResolve(command,MainReadImages::Color);
    legacySurface->EncodeCopy(command,mainAttachments->ReadColor(),waterCopyOrigin);
   }
   if(waterSurface){
   requireLogicalCopySource();
   const unsigned reads=(frame.bumpWater->settings.refraction==1?1u:0u)|(frame.bumpWater->settings.depthCopy?2u:0u);
   if(reads)mainAttachments->EncodeResolve(command,static_cast<MainReadImages>(reads));
   auto copy=[command computeCommandEncoder];Require(copy!=nil,"Cannot encode original full pre-water copies");
   [copy barrierAfterQueueStages:MTLStageFragment beforeStages:MTLStageBlit|MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];
   if(frame.bumpWater->settings.refraction==1){
    [arguments setAddress:(logicalCopyMapping->unitScale?waterCopyOrigin:logicalCopyCoordinates).gpuAddress atIndex:0];[arguments setTexture:mainAttachments->ReadColor().gpuResourceID atIndex:0];
    [arguments setTexture:impl->preWaterColor.gpuResourceID atIndex:1];[copy setComputePipelineState:logicalCopyMapping->unitScale?impl->waterColorCopyPipeline:impl->logicalColorCopyPipeline];[copy setArgumentTable:arguments];
    [copy dispatchThreads:MTLSizeMake(impl->preWaterColor.width,impl->preWaterColor.height,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];
   }
   if(frame.bumpWater->settings.depthCopy&&logicalCopyMapping->unitScale)[copy copyFromTexture:mainAttachments->ReadDepth() sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(crop[0],crop[1],0)
    sourceSize:MTLSizeMake(crop[2],crop[3],1) toTexture:impl->preWaterDepth destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
   [copy barrierAfterStages:MTLStageBlit|MTLStageDispatch beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];
   if(frame.bumpWater->settings.depthCopy&&!logicalCopyMapping->unitScale)
    encodeLogicalDepth(mainAttachments->ReadDepth(),impl->preWaterDepth,logicalCopyCoordinates);
   }
   encoder=[command renderCommandEncoderWithDescriptor:pass];Require(encoder!=nil,"Cannot resume actual loaded main world for water");
   }
   setupViewport();
   if(waterSurface)waterSurface->Encode(encoder);
   else if(legacySurface)legacySurface->Encode(encoder);
   else if(dynamicSurface)dynamicSurface->EncodeSurface(encoder);
   else {basicSurface->Encode(encoder);++result.basicWaterDraws;}
   ++result.waterDraws;
   encodingProbe.Select(Family176::Effects);
   waterPostLua.Encode(encoder);
			encodeAlphaModels(alphaModels[1],encoder,pass,frame.alphaModelFrames[1].get(),mainAttachments->Color(),mainAttachments->Depth(),mainAttachments->Stencil());
   drawPieces(true,aboveWaterScene);
   // The observed218 route has a terminal main SSAO operation, then original
   // AlphaAboveWater/NanoAboveWater particles and two ordinary overlays before
   // the first Bloom command closes the pass. Inspect these exact entries;
   // never skip an unknown event or extend borrowing to other callback routes.
   const auto* ssaoSource218=frame.waterScene&&frame.waterScene->aboveWaterLua?
    frame.waterScene->aboveWaterLua->ssao.get():nullptr;
   const bool scope218=!offscreenSource&&!options.readback&&!continuation&&!borrowedScene217.borrowed&&mainSamples==1&&postprocess&&
    ssaoSource218&&ssaoSource218->sourceDrawFrame==frame.sourceDrawFrame&&aboveWaterLua.ssao&&
    !ssaoSource218->draws.empty()&&aboveWaterLua.ssao->DrawCount()==ssaoSource218->draws.size()&&
    aboveWaterLua.draws.empty()&&aboveWaterLua.commands.empty()&&
    frame.shieldDepthFrame&&frame.shieldDepthFrame->submissionIdentity==sourceSubmission;
   const size_t lastSSAO218=scope218?aboveWaterLua.ssao->DrawCount()-1:0;
   const bool particles218=scope218&&particles&&particles->CanFollowSSAO218(pass,frame.sourceDrawFrame,sourceSubmission);
   const bool suffix218=scope218&&overlays&&laterWorldDraws.size()>=3&&
    laterWorldDraws[0].kind==LuaDrawKind::Overlay&&laterWorldDraws[1].kind==LuaDrawKind::Overlay&&
    laterWorldDraws[2].kind==LuaDrawKind::Bloom&&bloom&&laterWorldDraws[2].index<bloom->DrawCount()&&
    bloom->SequenceAt(laterWorldDraws[2].index)==laterWorldDraws[2].sequence&&
    ssaoSource218->draws.back().sequence==aboveWaterLua.ssao->SequenceAt(lastSSAO218)&&
    aboveWaterLua.ssao->SequenceAt(lastSSAO218)<laterWorldDraws[0].sequence&&
    laterWorldDraws[0].sequence<laterWorldDraws[1].sequence&&laterWorldDraws[1].sequence<laterWorldDraws[2].sequence&&
    overlays->CanFollowSSAO218(laterWorldDraws[0].index,laterWorldDraws[0].sequence,pass)&&
    overlays->CanFollowSSAO218(laterWorldDraws[1].index,laterWorldDraws[1].sequence,pass);
   const bool allowSSAO218=scope218&&particles218&&suffix218;
#if defined(RECOIL_METAL4_LIVE_SCENE)
   static const bool probe218=[](){const char* value=std::getenv("BAR_NATIVE_SSAO218_PROBE");return value&&std::strcmp(value,"1")==0;}();
   static std::atomic<unsigned> probeFrames218{0};
   const bool report218=probe218&&frame.sourceDrawFrame>=2048&&frame.simulationFrame>=450&&
    probeFrames218.load(std::memory_order_relaxed)<4&&probeFrames218.fetch_add(1,std::memory_order_relaxed)<4;
   if(report218)LOG("[NativeSSAO218Inspect] source_frame=%llu sim=%llu scope=%u particles=%u suffix=%u effect_rejection=%u last_index=%llu sequence=%llu program=%u",
    (unsigned long long)frame.sourceDrawFrame,(unsigned long long)frame.simulationFrame,unsigned(scope218),unsigned(particles218),unsigned(suffix218),
    scope218?unsigned(aboveWaterLua.ssao->InspectSceneDraw218(lastSSAO218,pass)):~0u,(unsigned long long)lastSSAO218,
    (unsigned long long)(scope218?aboveWaterLua.ssao->SequenceAt(lastSSAO218):0),scope218?unsigned(ssaoSource218->draws.back().program):~0u);
#endif
   aboveWaterLua.Encode(command,encoder,pass,allowSSAO218?&borrowedScene217.borrowed:nullptr);
   const bool borrowedSSAO218=allowSSAO218&&borrowedScene217.borrowed;
   if(borrowedSSAO218){
    // The positive first engine particle binds pipeline/depth/table/cull/clip/
    // bias itself. Restore the other states changed by SSAO to fresh defaults.
    [encoder setFrontFacingWinding:MTLWindingClockwise];
    [encoder setStencilFrontReferenceValue:0 backReferenceValue:0];
    [encoder setTriangleFillMode:MTLTriangleFillModeFill];
   }
#if defined(RECOIL_METAL4_LIVE_SCENE)
   id<MTL4RenderCommandEncoder> __unsafe_unretained successorEncoder218=encoder;
   if(report218)LOG("[NativeSSAO218Borrow] source_frame=%llu admitted=%u encoder=%p removed_boundaries=%u",
    (unsigned long long)frame.sourceDrawFrame,unsigned(borrowedSSAO218),(__bridge void*)encoder,unsigned(borrowedSSAO218));
#endif
   // GL retains the world raster across SSAO FBO switches. A resumed
   // Metal encoder does not inherit it; restore before engine particles.
   restoreWorldRaster();
   if(particles){particles->Encode(NativeWorldParticleStage::AlphaAboveWater,encoder,ssaoAboveDepth);particles->Encode(NativeWorldParticleStage::NanoAboveWater,encoder,ssaoAboveDepth);}
#if defined(RECOIL_METAL4_LIVE_SCENE)
   if(report218)LOG("[NativeSSAO218Successor] source_frame=%llu stage=%u admitted=%u same_encoder=%u positive_first_draw=%u",
    (unsigned long long)frame.sourceDrawFrame,unsigned(NativeWorldParticleStage::AlphaAboveWater),unsigned(borrowedSSAO218),
    unsigned(encoder==successorEncoder218),unsigned(particles218));
#endif
  }else{
			encodeAlphaModels(alphaModels[2],encoder,pass,frame.alphaModelFrames[2].get(),mainAttachments->Color(),mainAttachments->Depth(),mainAttachments->Stencil());
   drawPieces(true);
   alphaAllLua.Encode(command,encoder,pass);
   // GL retains the world raster across SSAO FBO switches. A resumed
   // Metal encoder does not inherit it; restore before engine particles.
   restoreWorldRaster();
   if(particles){
    if(frame.particleFrame->visibleWater){
     particles->Encode(NativeWorldParticleStage::AlphaBelowWater,encoder);particles->Encode(NativeWorldParticleStage::NanoBelowWater,encoder);
     particles->Encode(NativeWorldParticleStage::AlphaAboveWater,encoder);particles->Encode(NativeWorldParticleStage::NanoAboveWater,encoder);
     pending.insert("Requested original water/reflection/refraction producer between alpha halves");
    }else{particles->Encode(NativeWorldParticleStage::AlphaAll,encoder,ssaoAllDepth);particles->Encode(NativeWorldParticleStage::NanoAll,encoder,ssaoAllDepth);}
   }
  }
  encodingProbe.Select(Family176::Other);
  if(particles)for(auto stage:{NativeWorldParticleStage::GroundFlashes,NativeWorldParticleStage::OpaqueProjectiles,NativeWorldParticleStage::AlphaAll,NativeWorldParticleStage::AlphaBelowWater,NativeWorldParticleStage::AlphaAboveWater,NativeWorldParticleStage::NanoAll,NativeWorldParticleStage::NanoBelowWater,NativeWorldParticleStage::NanoAboveWater})result.particleDraws+=particles->DrawCount(stage);

		encodeLuaDraws(laterWorldDraws,false);
		// Interpret each selected route once, after all world draw loops. Do not
		// allocate strings or insert set entries in per-piece encode paths.
		const std::array<const char*,4> legacyRoutes{{"Selected legacy shadow model: ",
			"Selected legacy main model: ","Selected legacy reflection model: ","Selected legacy refraction model: "}};
		for (size_t i=0;i<legacyDiagnosticMasks.size();++i) {
			const auto bits=legacyDiagnosticMasks[i];
			if (!bits) continue;
			const std::string route=legacyRoutes[i];
			if (bits & Impl::CurrentUnitPose) pending.insert(route + "local pieces use current poses; previous-pose interpolation remains unconnected");
			if (bits & Impl::DamageDisplacement) pending.insert(route + "CUS health displacement/noise remains unconnected");
			if (bits & Impl::DamageTexturing) pending.insert(route + "CUS health/wreck texture stages remain unconnected");
			if (bits & Impl::TreadUV) pending.insert(route + "CUS faction tread UV animation remains unconnected");
			if (bits & Impl::FeatureFadeAndNight) {
				pending.insert(route + "CUS squared-distance scale fade remains unconnected; engine distance-alpha retained");
				pending.insert(route + "CUS feature nightFactor brightness update remains unconnected");
			}
			if (bits & Impl::ViewerInfo) pending.insert(route + "CUS viewer info-map modulation remains unconnected");
			if (bits & Impl::ModelGamma) pending.insert(route + "non-default modelGamma uniform mapping remains unconnected");
		}
		result.materialDependenciesPending.assign(pending.begin(),pending.end());

		// Append the actual original UI to the same drawable and native encoder.
		// No second present or replacement window.
		if (screen != nullptr && !postprocess) {
			// The last world command may have ended the scene encoder (for
			// example Bloom). Resume the same loaded attachments for the UI.
			if (encoder == nil) {
				encoder = [command renderCommandEncoderWithDescriptor:pass];
				Require(encoder != nil, "Cannot resume original screen after ordered world commands");
				[encoder barrierAfterQueueStages:MTLStageFragment | MTLStageBlit beforeStages:MTLStageVertex | MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
			}
			screen->Encode(encoder);
		}
		[encoder endEncoding];
		borrowedScene217.borrowed=false;
		if(postprocess) postprocess->Encode(command,*screen);
		if(presentationMargins) {
			// Match the donor's final default-framebuffer scissor clear. All
			// original world, copy, postprocess and UI consumers have finished.
			auto margins=mainAttachments->LoadedPass();
			auto output=[command renderCommandEncoderWithDescriptor:margins];Require(output!=nil,"Cannot encode fullscreen safe margins");
			[output barrierAfterQueueStages:MTLStageFragment|MTLStageBlit|MTLStageDispatch beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
			presentationMargins->Encode(output);[output endEncoding];
		}
		if(readback){
			if(!options.readback->readColor)mainAttachments->EncodeResolve(command,MainReadImages::Color);
			*mainReadReady=true;readback->Encode(command);
		}else mainAttachments->EncodePresentationResolve(command);
		encodingProbe.Finish(frame.sourceDrawFrame,frame.simulationFrame);
		cpuPhase.Mark(10);preparationDetail.Mark(36);
		lease.FinishResourceCommit();
		result.residentBytes = lease.ResidentBytes(); result.residencyAllocationCount = lease.AllocationCount();
		cpuPhase.Counts(result.residentBytes,result.residencyAllocationCount);
  const bool defer=bool(options.deferredCompleted)&&!offscreenSource&&!options.readback&&!continuation&&drawable!=nil&&frame.simulationFrame>0&&screen&&screen->AllTargetsEncoded();
  // Own every completion input. No references to this Draw stack survive the
  // deferred return. Source packets remain pinned by the runtime graph too.
#ifdef RECOIL_METAL4_LIVE_SCENE
  std::array<uint64_t,3> mrt174Expected{};
  const bool mrt174Eligible=MRT174Enabled()&&MRT174Window(frame.sourceDrawFrame)&&frame.simulationFrame>0&&
   !offscreenSource&&!options.readback&&!continuation&&drawable!=nil;
  if(mrt174Eligible){
   for(const auto& ground:groundScenes)if(ground.mapMRT)++mrt174Expected[0];
   if(frame.opaqueModelFrame)for(const auto& phase:frame.opaqueModelFrame->passes)if(phase.mrt){
    const auto& source=*phase.mrt;
    if(source.role==NativeGeometryTextureRole::Map)++mrt174Expected[0];
    else if(source.role==NativeGeometryTextureRole::Models)++mrt174Expected[source.feature?2:1];
   }
  }
#endif
  auto complete=[owner=impl.get(), result, sourceSubmission, offscreenSource, passCounters160, passCounters165, passCounters166,
#ifdef RECOIL_METAL4_LIVE_SCENE
   mrt174Eligible,mrt174Expected,mrt174Simulation=frame.simulationFrame,
#endif
   completeTextureMips=impl->TakeTextureMipCompletion(sourceSubmission),
   completeAtlasMips=impl->particlesGPU.TakeAtlasCompletion(sourceSubmission),
   hasDrawable=drawable!=nil, sourceDrawFrame=frame.sourceDrawFrame,
   completedShield=frame.shieldDepthFrame,reflectionWorldInfo,
   postprocessSerial=options.screenUI&&options.screenUI->postprocess?options.screenUI->postprocess->serial:0,
   submissionCompleted=options.submissionCompleted,isReadback=bool(options.readback),readback,
   originalShadow=std::move(originalShadow),
   initialShadow=std::move(initialShadow),
   opaqueModels=std::move(opaqueModels),
   alphaModels=std::move(alphaModels),
   environment=std::move(environment),
   pbrBRDF=std::move(pbrBRDF),
   pbrEnvironment=std::move(pbrEnvironment),
   modelGBuffer=std::move(modelGBuffer),
   shadowColor=std::move(shadowColor),
   shadowLua=std::move(shadowLua),
   worldInfo=std::move(worldInfo),
   waterSurface=std::move(waterSurface),
   basicSurface=std::move(basicSurface),
   groundDecals=std::move(groundDecals),
   groundScenes=std::move(groundScenes),
   waterViews=std::move(waterViews),
   belowWaterLua=std::move(belowWaterLua),
   aboveWaterLua=std::move(aboveWaterLua),
   alphaAllLua=std::move(alphaAllLua),
   waterPostLua=std::move(waterPostLua),
   dynamicSurface=std::move(dynamicSurface),
   legacySurface=std::move(legacySurface),
   standardShade=std::move(standardShade),
   sensorPreview=std::move(sensorPreview),
   screen=std::move(screen),
   postprocess=std::move(postprocess),
   bloom=std::move(bloom),
   placementGrid=std::move(placementGrid),
   reclaimFields=std::move(reclaimFields),
   objectSpotlights=std::move(objectSpotlights),
   grass=std::move(grass),
   resourceSpots=std::move(resourceSpots),
   clouds=std::move(clouds),
   particles=std::move(particles),
   luaNano=std::move(luaNano),
   projectileEffects=std::move(projectileEffects),
   lightning=std::move(lightning),
   deferredLights=std::move(deferredLights),
   outline=std::move(outline),
   unitStencil=std::move(unitStencil),
   distortions=std::move(distortions),
   blueprint=std::move(blueprint),
   overlays=std::move(overlays),
   separateLineOwners=std::move(separateLineOwners),
   blockout=std::move(blockout),
   namedImageMipmaps=std::move(namedImageMipmaps),
   normalBacking=std::move(normalBacking)](const FrameMetrics& metrics) mutable {
   result.metrics=metrics;
  std::vector<std::exception_ptr> retirementFailures;
  const auto retire=[&](const auto& completion){try{completion();}catch(...){retirementFailures.push_back(std::current_exception());}};
  // Named producer completion is published immediately after the SAME wait,
  // before unrelated owner callbacks can fail during retirement.
  if(originalShadow)retire([&]{originalShadow->DidComplete();});
  for(const auto& backing:initialShadow)if(backing&&backing->encodedSubmission.load()==sourceSubmission&&!backing->completed.load())
   retire([&]{CompleteNativeShadowTexture(*backing);});
  retire([&]{completeTextureMips();});
  retire([&]{completeAtlasMips();});
  if(opaqueModels)retire([&]{opaqueModels->DidComplete();});
		for(auto& alpha:alphaModels)if(alpha)retire([&]{alpha->DidComplete();});
  retire([&]{environment->DidComplete();});
  if(reflectionWorldInfo)retire([&]{reflectionWorldInfo->DidComplete();});
  if(pbrBRDF)retire([&]{pbrBRDF->DidComplete(sourceSubmission);});
  if(pbrEnvironment)retire([&]{pbrEnvironment->DidComplete(sourceSubmission);});
  // PBR completion updates atomics only; the main capture owner compacts declarations.
  if(modelGBuffer)retire([&]{modelGBuffer->DidComplete();});
  if(shadowColor)retire([&]{shadowColor->DidComplete();});
  if(shadowColor)retire([&]{shadowLua.Complete();});
  if(worldInfo)retire([&]{worldInfo->DidComplete();result.completedWorldInfo={worldInfo->MapIdentity(),worldInfo->CompletionSerial()};});
  if(waterSurface)retire([&]{const auto completed=waterSurface->DidComplete();result.completedWaterUpdates={completed.generation,completed.normalSequence,completed.coastSequence};});
  if(basicSurface)retire([&]{basicSurface->DidComplete();});
  if(groundDecals)retire([&]{groundDecals->DidComplete();});
  for(auto& ground:groundScenes){
   if(ground.mapMRT)retire([&]{ground.mapMRT->DidComplete();});
   if(ground.terrain)retire([&]{ground.terrain->DidComplete();});
   if(ground.cus)retire([&]{ground.cus->DidComplete();});
   retire([&]{ground.ordered->Complete();});
  }
  for(auto& view:waterViews)if(view.source){
   if(view.groundTerrain)retire([&]{view.groundTerrain->DidComplete();});
   if(view.groundCUS)retire([&]{view.groundCUS->DidComplete();});
   if(view.source->groundSource)retire([&]{view.groundLua->Complete();});
   if(view.opaqueModels)retire([&]{view.opaqueModels->DidComplete();});
			if(view.alphaModels)retire([&]{view.alphaModels->DidComplete();});
   if(view.particles)retire([&]{view.particles->DidComplete();});
   retire([&]{view.preParticlesLua.Complete();});retire([&]{view.worldLua.Complete();});
  }
  retire([&]{belowWaterLua.Complete();});retire([&]{aboveWaterLua.Complete();});retire([&]{alphaAllLua.Complete();});retire([&]{waterPostLua.Complete();});
  if(dynamicSurface)retire([&]{const auto receipt=dynamicSurface->DidComplete();result.dynamicWaterGeneration=receipt.generation;result.dynamicWaterSequence=receipt.sequence;result.dynamicWaterSourceDrawFrame=receipt.sourceDrawFrame;});
  if(legacySurface)retire([&]{legacySurface->DidComplete();});
  if(standardShade)retire([&]{standardShade->DidComplete();});
  for(const auto& packet:sensorPreview)retire([&]{packet->DidComplete();});
  if(screen)retire([&]{screen->DidComplete();result.luaPiPDraws=screen->PiPDrawsCompleted();result.luaStartBoxMiniMapDraws=screen->StartBoxDrawsCompleted();});
  if(postprocess)retire([&]{postprocess->DidComplete();});
  if(bloom)retire([&]{bloom->DidComplete();});
  if(placementGrid)retire([&]{placementGrid->DidComplete();});
  if(reclaimFields)retire([&]{reclaimFields->DidComplete();});
  if(objectSpotlights)retire([&]{objectSpotlights->DidComplete();});
  if(grass)retire([&]{grass->DidComplete();});
  if(resourceSpots)retire([&]{resourceSpots->DidComplete();});
  if(clouds)retire([&]{clouds->DidComplete();});
  if(particles)retire([&]{particles->DidComplete();});
  if(luaNano)retire([&]{luaNano->DidComplete();});
  if(projectileEffects)retire([&]{projectileEffects->DidComplete();});
  if(lightning)retire([&]{lightning->DidComplete();});
  if(deferredLights)retire([&]{deferredLights->DidComplete();});
  if(outline)retire([&]{outline->DidComplete();});
  if(unitStencil)retire([&]{unitStencil->DidComplete();});
  if(distortions)retire([&]{distortions->DidComplete();});
  if(blueprint)retire([&]{blueprint->DidComplete();});
  if(overlays)retire([&]{overlays->DidComplete();});
  for(const auto& lines:separateLineOwners)if(lines)retire([&]{lines->CompleteLineDependencies();});
  if(blockout)retire([&]{blockout->DidComplete();});
  retire([&]{namedImageMipmaps->DidComplete();});
  retire([&]{owner->normalsReady=true;std::dynamic_pointer_cast<const Impl::MapImage>(normalBacking->Resource())->completed=true;});
  if(readback)retire([&]{readback->DidComplete();});
  if(submissionCompleted)retire([&]{submissionCompleted(sourceSubmission);});
  if(isReadback)retire([&]{ui::GetRecorder().RetireTargetSubmission(sourceSubmission);});
  if(!retirementFailures.empty()){
   if(retirementFailures.size()==1)std::rethrow_exception(retirementFailures.front());
   std::string message="Actual shared source retirement reported multiple failures:";
   for(const auto& failure:retirementFailures)try{std::rethrow_exception(failure);}catch(const std::exception& error){message+="\n";message+=error.what();}catch(...){message+="\nUnknown original retirement failure";}
   throw std::runtime_error(message);
  }
  if(owner->shieldPending==completedShield)owner->shieldPending.reset();
  if(owner->activeSourceSubmission==sourceSubmission){owner->submittedOpaqueSource.reset();owner->activeSourceSubmission=0;}
		result.capabilities.gpuTimestamps = result.metrics->gpuExecutionMs.has_value();
		result.presented=!offscreenSource && hasDrawable;
		result.sourceFrameCompleted=true;result.sourceDrawFrame=sourceDrawFrame;
#ifdef RECOIL_METAL4_LIVE_SCENE
  if(mrt174Eligible){
   const uint64_t bit=uint64_t(1)<<(sourceDrawFrame-2048);
   const bool duplicate=(owner->mrt174CompletedFrames.fetch_or(bit,std::memory_order_relaxed)&bit)!=0;
   if(duplicate&&MRT174IssueAllowed())LOG("[NativeMRT174Issue] kind=duplicate_completed_frame source_frame=%llu",static_cast<unsigned long long>(sourceDrawFrame));
   if(!duplicate){
    const bool commandTime=result.metrics&&result.metrics->gpuExecutionMs.has_value()&&std::isfinite(*result.metrics->gpuExecutionMs)&&*result.metrics->gpuExecutionMs>0;
    LOG("[NativeMRT174Frame] source_frame=%llu submission=%llu simulation_frame=%lld renderer=%p expected_map=%llu expected_unit=%llu expected_feature=%llu gpu_command_known=%u gpu_command_ms=%.6f retired=1 ordinary=1 positive=1",
     static_cast<unsigned long long>(sourceDrawFrame),static_cast<unsigned long long>(sourceSubmission),static_cast<long long>(mrt174Simulation),static_cast<void*>(owner),
     static_cast<unsigned long long>(mrt174Expected[0]),static_cast<unsigned long long>(mrt174Expected[1]),static_cast<unsigned long long>(mrt174Expected[2]),
     unsigned(commandTime),commandTime?*result.metrics->gpuExecutionMs:-1.0);
   }
  }
#endif
#ifdef RECOIL_METAL4_LIVE_SCENE
  static bool reportedPass160=false;
  static bool reportedPass165=false;
  static bool reportedPass166=false;
  if(!reportedPass166&&sourceDrawFrame>=300){
   LOG("[NativePass166] enabled=%u source_frame=%llu submission=%llu attempts=%llu effect_queries=%llu main_kept=%llu planar_kept=%llu kept_nonempty=%llu kept_empty=%llu disabled=%llu no_encoder=%llu attachment_fallback=%llu callback_unknown=%llu ordered_commands=%llu dependency_unknown=%llu dependency_commands=%llu cube_query=%llu retired=1",
    unsigned(ModelPass166Enabled()),static_cast<unsigned long long>(sourceDrawFrame),static_cast<unsigned long long>(sourceSubmission),
    static_cast<unsigned long long>(passCounters166.attempts),static_cast<unsigned long long>(passCounters166.queries),
    static_cast<unsigned long long>(passCounters166.mainKept),static_cast<unsigned long long>(passCounters166.planarKept),
    static_cast<unsigned long long>(passCounters166.keptNonempty),static_cast<unsigned long long>(passCounters166.keptEmpty),
    static_cast<unsigned long long>(passCounters166.disabled),static_cast<unsigned long long>(passCounters166.noEncoder),
    static_cast<unsigned long long>(passCounters166.attachment),static_cast<unsigned long long>(passCounters166.callbackUnknown),
    static_cast<unsigned long long>(passCounters166.orderedCommands),static_cast<unsigned long long>(passCounters166.dependencyUnknown),
    static_cast<unsigned long long>(passCounters166.dependencyCommands),static_cast<unsigned long long>(passCounters166.cubeQueries));
   reportedPass166=true;
  }
  if(!reportedPass165&&sourceDrawFrame>=300){
   LOG("[NativePass165] enabled=%u source_frame=%llu submission=%llu effect_queries=%llu main_mrt_boundary_kept=%llu disabled=%llu no_encoder=%llu post_deferred=%llu attachment_fallback=%llu unknown=%llu following_query160=%llu following_kept160=%llu following_boundary160=%llu retired=1",
    unsigned(ModelPass165Enabled()),static_cast<unsigned long long>(sourceDrawFrame),static_cast<unsigned long long>(sourceSubmission),
    static_cast<unsigned long long>(passCounters165.queries),static_cast<unsigned long long>(passCounters165.kept),
    static_cast<unsigned long long>(passCounters165.disabled),static_cast<unsigned long long>(passCounters165.noEncoder),
    static_cast<unsigned long long>(passCounters165.postDeferred),static_cast<unsigned long long>(passCounters165.attachment),
    static_cast<unsigned long long>(passCounters165.unknown),static_cast<unsigned long long>(passCounters165.followingQueries160),
    static_cast<unsigned long long>(passCounters165.followingKept160),static_cast<unsigned long long>(passCounters165.followingBoundary160));
   reportedPass165=true;
  }
  if(!reportedPass160&&sourceDrawFrame>=300){
   LOG("[NativePass160] enabled=%u kept_pass=%llu kept_nonempty=%llu kept_empty=%llu cube_query=%llu unknown_boundary=%llu model_mrt_phases=%llu model_mrt_load_phases=%llu",
    unsigned(ModelPass160Enabled()),static_cast<unsigned long long>(passCounters160.kept),
    static_cast<unsigned long long>(passCounters160.keptNonempty),static_cast<unsigned long long>(passCounters160.keptEmpty),
    static_cast<unsigned long long>(passCounters160.cubeQueries),static_cast<unsigned long long>(passCounters160.unknownBoundaries),
    static_cast<unsigned long long>(passCounters160.modelMRTPhases),static_cast<unsigned long long>(passCounters160.modelMRTLoadPhases));
   reportedPass160=true;
  }
#endif
		result.completedPostprocessSerial=postprocessSerial;
		result.screenTargetsCompleted=screen && screen->AllTargetsEncoded();
		if(offscreenSource)result.status=Metal4PresentationStatus::OffscreenSourceCompleted;
   return std::move(result);
  };
  if(defer){
   auto owned=std::make_shared<decltype(complete)>(std::move(complete));
   preparationDetail.Mark(37);
   lease.SubmitAndDefer(prepStart,impl->presentationFrame++,drawable,
    [owned,notify=options.deferredCompleted](const FrameMetrics& metrics){notify((*owned)(metrics));});
   preparationDetail.Mark(38);
   originalSubmission.retired=true; // Ownership transferred, NOT GPU-completed.
   // Capture transaction slots are reusable; GPU owners remain in the ticket.
   impl->shieldPending.reset();impl->submittedOpaqueSource.reset();impl->activeSourceSubmission=0;
   result.presented=true;result.sourceFrameCompleted=false;result.screenTargetsCompleted=false;
   result.sourceDrawFrame=frame.sourceDrawFrame;result.metrics.reset();
   return result;
  }
  const auto metrics=lease.SubmitAndWait(prepStart,options.readback?impl->screenTargetSubmission++:impl->presentationFrame++,drawable);
  cpuPhase.Mark(11);
  result=complete(metrics);originalSubmission.retired=true;
		if(continuation){
			try {
				continuation->cutoff=continuation->worldCutoff=options.readback->cutoff;
				continuation->rootCommands=std::uint32_t(options.screenUI->drawList->Commands().size());
				continuation->result=result;impl->screenContinuation=continuation;
				if(options.sourceCompleted)options.sourceCompleted(result);
			} catch(...) {lease.PoisonAfterBegin();throw;}
			// CPU allocation/bitmap failure may be caught by original Lua. The
			// successfully retired frame continuation is already installed first.
			result.readbackPixels=readback->PixelsRGBA8();
		}
		// Presented means scheduled via MTLDrawable after GPU completion signaling;
		// it is not a measurement of physical display scanout or game FPS.
		return result;
	}
}

} // namespace rendering
