/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "WorldFrameCapture.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/Metal4/Metal4SceneBridge.h"
#include "Rendering/Backend/NativeWaterGroundSourceData.h"
#endif
#include "Rendering/Backend/NativeOriginalShadowData.h"
#include "Rendering/Backend/NativeShadowTextureSource.h"
#include "Rendering/Backend/NativeCUSSourceIdentity.h"
#include "Rendering/Backend/NativePBRSourceFiles.h"
#include "Rendering/EngineBridge/NativeS3OModelTextureCapture.h"
#include "Backend/NativeMetalInfoCapture.h"
#include "Backend/NativeEngineModelData.h"
#include "Backend/NativeCUSUniformCapture.h"
#include "Rendering/Common/ModelDrawerState.hpp"
#include "Backend/NativeCUSMaterialControl.h"
#include "Backend/NativeMiniMapData.h"
#include "Rendering/EngineBridge/NativeTerrainImageSource.h"
#include "Backend/NativeShadowColorCapture.h"
#include "Backend/NativeWorldInfoCapture.h"
#include "Map/Ground.h"

#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Game/GameSetup.h"
#include "Game/GlobalUnsynced.h"
#include "Map/MapInfo.h"
#include "Map/BaseGroundDrawer.h"
#include "Map/ReadMap.h"
#include "Map/SMF/SMFMapFile.h"
#include "Map/SMF/SMFReadMap.h"
#include "Map/SMF/SMFTileData.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/SkyBox.h"
#include "Rendering/Backend/NativeWorldEnvironmentData.h"
#include "Rendering/Backend/NativeCubeTextureSource.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Env/CubeMapHandler.h"
#include "Rendering/Textures/NativeSkyDDS.h"
#include "Rendering/Env/SunLighting.h"
#include "Rendering/Env/WaterRendering.h"
#include "Rendering/Env/MapRendering.h"
#include "Rendering/Features/FeatureDrawer.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Backend/NativeWorldParticleCapture.h"
#include "Rendering/Backend/NativeWorldFeaturePolicy.h"
#include "Lua/LuaParser.h"
#include "Rendering/Models/3DModel.hpp"
#include "Rendering/Models/3DModelPiece.hpp"
#include "Rendering/Models/ModelsLock.h"
#include "Rendering/Textures/Bitmap.h"
#include "Rendering/Textures/nv_dds.h"
#include "Rendering/Textures/NativeUIDDS.h"
#include "Rendering/Textures/NativeWorldDDS.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/Team.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Features/Feature.h"
#include "Sim/Features/FeatureDef.h"
#include "System/Config/ConfigHandler.h"
#include "System/Sync/SHA512.hpp"
#include "System/Exceptions.h"
#include "System/FileSystem/FileHandler.h"
#include "System/FileSystem/FileSystem.h"
#include "System/Matrix44f.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace rendering {
namespace {

WorldVector3 CopyVector(const float3& vector)
{
	return {vector.x, vector.y, vector.z};
}

WorldMatrix CopyMatrix(const CMatrix44f& matrix)
{
	WorldMatrix result;
	for (size_t i = 0; i < result.size(); ++i)
		result[i] = matrix[i];
	return result;
}

ClipDepthConvention GetClipDepth(const CCamera& inputCamera)
{
	WorldMatrix identity = {};
	identity[0] = identity[5] = identity[10] = identity[15] = 1.0f;
	const auto control = CopyMatrix(inputCamera.GetClipControlMatrix());
	if (control == identity)
		return ClipDepthConvention::MinusOneToOne;
	identity[10] = identity[14] = 0.5f;
	if (control == identity)
		return ClipDepthConvention::ZeroToOne;
	return ClipDepthConvention::Unsupported;
}

std::string ResolveTexturePath(const std::string& reference)
{
	for (const auto& path : {reference, std::string("unittextures/") + reference}) {
		if (!reference.empty() && CFileHandler::FileExists(path, SPRING_VFS_RAW_FIRST))
			return path;
	}
	throw content_error("Missing original model texture: " + reference);
}

WorldTextureMip CaptureRGBAImagePayload(const CBitmap& image, const std::string& path)
{
	if (image.Empty() || image.compressed || image.channels != 4 || image.dataType != 0x1401 || image.xsize <= 0 || image.ysize <= 0)
		throw content_error("Unsupported original RGBA image: " + path);
	const size_t width = static_cast<size_t>(image.xsize), height = static_cast<size_t>(image.ysize);
	if (width > std::numeric_limits<size_t>::max() / 4)
		throw content_error("Original RGBA image row extent overflows: " + path);
	const size_t rowBytes = width * 4;
	if (height > std::numeric_limits<size_t>::max() / rowBytes)
		throw content_error("Original RGBA image payload extent overflows: " + path);
	const size_t payloadBytes = rowBytes * height;
	// CBitmap::GetMemSize multiplies its signed32 dimensions/channels before
	// returning size_t. Validate that original arithmetic before evaluating it.
	if (payloadBytes > static_cast<size_t>(std::numeric_limits<int32_t>::max()))
		throw content_error("Original RGBA image exceeds CBitmap payload arithmetic: " + path);
	const auto* pixels = image.GetRawMem();
	if (pixels == nullptr || image.GetMemSize() != payloadBytes)
		throw content_error("Original RGBA image payload is unavailable or inconsistent: " + path);
	WorldTextureMip mip;
	mip.width = static_cast<uint32_t>(width); mip.height = static_cast<uint32_t>(height);
	mip.bytesPerRow = rowBytes;
	// GetSpan describes the texture-memory pool, not this image allocation.
	// Preserve the original decoder's channel order/origin and copy only its
	// validated logical payload, regardless of pooled or direct allocation.
	mip.bytes.assign(pixels, pixels + payloadBytes);
	return mip;
}

std::shared_ptr<const WorldTexture> LoadTextureBytes(const std::string& path, bool originalMapTexture = false, bool preserveLoadFailure = false)
{
	auto texture = std::make_shared<WorldTexture>();
	texture->sourcePath = path;
	if (FileSystem::GetExtensionLowerCase(path) == "dds") {
		CFileHandler original(path, SPRING_VFS_RAW_FIRST); std::string sourceBytes;
		if (original.FileSize()<128 || original.FileSize()>384*1024*1024 || !original.LoadStringData(sourceBytes)) throw content_error("Original world DDS file extent/load failure: "+path);
		ValidateNativeWorldDDS({reinterpret_cast<const uint8_t*>(sourceBytes.data()),sourceBytes.size()});
		if (originalMapTexture) {
			CFileHandler source(path, SPRING_VFS_RAW_FIRST);
			std::string bytes;
			if (source.FileSize() < 128 || source.FileSize() > 384 * 1024 * 1024 || !source.LoadStringData(bytes))
				throw content_error("Native map-edge DDS file extent/load failure: " + path);
			const auto layout=ui::ValidateNativeUIDDS({reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size()});
			// Original DDS base-only images generate mips; exactly two total
			// levels select GL_LINEAR. This bounded consumer currently uses the
			// full-chain trilinear contract and must not silently reinterpret it.
			if (layout.mips.size()<=2 && (layout.mips[0].width>1 || layout.mips[0].height>1))
				throw content_error("Native map-edge short DDS chain requires original mip generation/filter semantics: " + path);
		}
		// CBitmap's HEADLESS DDS branch returns a dummy. The same underlying
		// engine DDS decoder directly reads VFS bytes without making GL objects.
		nv_dds::CDDSImage image;
		if (!image.load(path, originalMapTexture || path.find("unitpics") == std::string::npos)) {
			if (preserveLoadFailure) return {};
			throw content_error("Unsupported original model DDS: " + path);
		}
		if (image.get_type() != nv_dds::TextureFlat)
			throw content_error("Unsupported original model DDS: " + path);
		// Preserve the original upload's RGB/RGBA/component contract even when
		// native transport expands ordinary RGB or luminance payload bytes.
		texture->originalInternalFormat = image.is_compressed() ? image.get_format() : image.get_components();
		size_t blockBytes = 16;
		bool expandBGR = false, expandLuminance = false;
		const bool compressed = image.is_compressed();
		switch (image.get_format()) {
			case 0x83f0: // GL_COMPRESSED_RGB_S3TC_DXT1_EXT
			case 0x83f1: // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
				texture->format = WorldTextureFormat::BC1RGBA;
				blockBytes = 8;
				break;
			case 0x83f2: texture->format = WorldTextureFormat::BC2RGBA; break;
			case 0x83f3: texture->format = WorldTextureFormat::BC3RGBA; break;
			case 0x8dbb: texture->format = WorldTextureFormat::BC4Unorm; blockBytes = 8; break;
			case 0x8dbd: texture->format = WorldTextureFormat::BC5Unorm; break;
			case 0x8e8c: texture->format = WorldTextureFormat::BC7RGBA; break;
			case 0x8e8d: texture->format = WorldTextureFormat::BC7RGBASRGB; break;
			case 0x80e1: texture->format = WorldTextureFormat::BGRA8Unorm; break;
			case 0x1908: texture->format = WorldTextureFormat::RGBA8Unorm; break;
			case 0x80e0: texture->format = WorldTextureFormat::RGBA8Unorm; expandBGR = true; break;
			case 0x1909: texture->format = WorldTextureFormat::RGBA8Unorm; expandLuminance = true; break;
			default: throw content_error("Unsupported original DDS compression: " + path);
		}
		auto append = [&](uint32_t width, uint32_t height, const uint8_t* bytes, size_t count) {
			if (width == 0 || height == 0 || width > 16384 || height > 16384)
				throw content_error("Unsupported original DDS dimensions: " + path);
			const size_t rowBytes = compressed ? ((static_cast<size_t>(width) + 3) / 4) * blockBytes : size_t(width) * image.get_components();
			if (count != rowBytes * (compressed ? ((static_cast<size_t>(height) + 3) / 4) : height))
				throw content_error("Original DDS mip extent differs from its dimensions: " + path);
			WorldTextureMip mip;
			mip.width = width;
			mip.height = height;
			mip.bytesPerRow = rowBytes;
			if (expandBGR) {
				mip.bytes = ui::ExpandNativeUIBGR8({bytes, count}, width, height);
				mip.bytesPerRow = size_t(width) * 4;
			} else if (expandLuminance) {
				mip.bytes.resize(size_t(width) * height * 4);
				for (size_t i = 0; i < count; ++i) {
					mip.bytes[4*i] = mip.bytes[4*i+1] = mip.bytes[4*i+2] = bytes[i]; mip.bytes[4*i+3] = 255;
				}
				mip.bytesPerRow = size_t(width) * 4;
			} else mip.bytes.assign(bytes, bytes + count);
			texture->mips.push_back(std::move(mip));
		};
		texture->mipmappedSampling = image.get_num_mipmaps() != 1;
		append(image.get_width(), image.get_height(), image, image.get_size());
		for (uint32_t i = 0; i < image.get_num_mipmaps(); ++i) {
			const auto& mip = image.get_mipmap(i);
			append(mip.get_width(), mip.get_height(), mip, mip.get_size());
		}
		texture->flipV = false; // Original nv_dds load-time block/row flip is retained, including its tail-mip/BC7 rules.
	} else {
		if (originalMapTexture)
			throw content_error("Native map-edge map image mip generation is not yet implemented for non-DDS overrides: " + path);
		CBitmap image;
		if (!image.Load(path)) {
			if (preserveLoadFailure) return {};
			throw content_error("Cannot load original RGBA image: " + path);
		}
		texture->mips.push_back(CaptureRGBAImagePayload(image, path));
		texture->originalInternalFormat = 0x8058; // CBitmap::Load defaults to four U8 channels.
	}
	texture->generateMipmaps = texture->mips.size() == 1;
	return texture;
}

std::vector<WorldPiecePose> CapturePieces(const LocalModel& local, const WorldModel& model)
{
	if (local.pieces.size() != model.pieces.size())
		throw content_error("Actual LocalModel piece count differs from its static model");
	std::vector<Transform> transforms;
	std::vector<WorldPiecePose> result;
	transforms.reserve(local.pieces.size()); result.reserve(local.pieces.size());
	for (size_t i = 0; i < local.pieces.size(); ++i) {
		const auto& piece = local.pieces[i];
		Transform transform = piece.CalcPieceSpaceTransform(piece.GetPosition(), piece.GetRotation(), piece.GetScaling());
		const uint32_t parent = model.pieces[i].parent;
		if (parent != std::numeric_limits<uint32_t>::max()) {
			if (parent >= transforms.size()) throw content_error("Actual model pieces are not in parent-first order");
			transform = transforms[parent] * transform;
		}
		transforms.push_back(transform);
		result.push_back({CopyMatrix(transform.ToMatrix()), piece.GetScriptVisible()});
	}
	return result;
}

std::array<float, 4> CopyFour(const float4& value) { return {value.x, value.y, value.z, value.w}; }

WorldLighting CaptureLighting()
{
	WorldLighting out;
	const auto& sky = ISky::GetSky();
	if (!sky) throw content_error("Native world lighting requires original sky/light state");
	out.sunDirection = CopyFour(sky->GetLight()->GetLightDir());
	out.groundAmbient = CopyFour(sunLighting->groundAmbientColor);
	out.groundDiffuse = CopyFour(sunLighting->groundDiffuseColor);
	out.groundSpecular = CopyFour(sunLighting->groundSpecularColor);
	out.modelAmbient = CopyFour(sunLighting->modelAmbientColor);
	out.modelDiffuse = CopyFour(sunLighting->modelDiffuseColor);
	out.modelSpecular = CopyFour(sunLighting->modelSpecularColor);
	out.specularExponent = sunLighting->specularExponent;
	out.groundShadowDensity = sunLighting->groundShadowDensity;
	out.modelShadowDensity = sunLighting->modelShadowDensity;
	out.fogColor = {sky->fogColor.x, sky->fogColor.y, sky->fogColor.z, 1};
	const auto* player = CCameraHandler::GetCamera(CCamera::CAMTYPE_PLAYER);
	const float start = sky->fogStart * player->GetFarPlaneDist(), end = sky->fogEnd * player->GetFarPlaneDist();
	if (!std::isfinite(start) || !std::isfinite(end) || !(end > start)) throw content_error("Original world fog range is invalid");
	out.fogParameters = {start, end, 0, 1 / (end - start)};
	out.waterBase = CopyVector(waterRendering->baseColor);
	out.waterMinimum = CopyVector(waterRendering->minColor);
	out.waterAbsorb = CopyVector(waterRendering->absorb);
	auto& requested = out.requested;
	requested.normalMapping = configHandler->GetBoolSafe("NormalMapping", true);
	requested.shadows = configHandler->GetInt("Shadows");
	requested.shadowQuality = configHandler->GetIntSafe("ShadowQuality", 2);
	requested.shadowMapSize = configHandler->GetInt("ShadowMapSize");
	requested.reflectionCubeSize = configHandler->GetInt("CubeTexSizeReflection");
	requested.specularCubeSize = configHandler->GetInt("CubeTexSizeSpecular");
	requested.reflectionMipmaps = configHandler->GetBool("CubeTexGenerateMipMaps");
	requested.deferredMap = configHandler->GetBoolSafe("AllowDeferredMapRendering", false);
	requested.deferredModel = configHandler->GetBoolSafe("AllowDeferredModelRendering", false);
	requested.waterRenderer = configHandler->GetInt("ReflectiveWater");
	requested.samples = configHandler->GetIntSafe("MSAA", 0) ? configHandler->GetIntSafe("MSAALevel", 1) : 1;
	requested.softParticles = configHandler->GetBoolSafe("SoftParticles", false);
	constexpr const char* keys[] = {"tonemapA", "tonemapB", "tonemapC", "tonemapD", "tonemapE", "envAmbient", "unitSunMult", "unitExposureMult"};
	constexpr float defaults[] = {4.75f, .75f, 3.5f, .85f, 1, .25f, 1, 1};
	for (size_t i = 0; i < 8; ++i) requested.pbrParameters[i] = configHandler->GetFloatSafe(keys[i], defaults[i]);
	requested.modelGamma = configHandler->GetFloatSafe("modelGamma", 1);
	return out;
}

std::shared_ptr<const NativeWorldSkyResource> LoadOriginalSkyResource()
{
	auto out=std::make_shared<NativeWorldSkyResource>();
	const auto& original=ISky::GetSky();
	if(!original || original->GetName()=="NullSky") { out->diagnostic="Original NullSky selection";return out; }
	if(original->GetName()=="ModernSky") { out->kind=NativeSkyKind::Modern;out->sourcePath="GLSL/ModernSkyVS.glsl + GLSL/ModernSkyFS.glsl";return out; }
	if(const auto* cube=dynamic_cast<const CSkyBox*>(original.get());cube&&cube->GetNativeResource())return cube->GetNativeResource();
	if(original->GetName()!="SkyBox" || mapInfo->atmosphere.skyBox.empty())
		throw content_error("Native original sky selection is unsupported: "+original->GetName());
	out->sourcePath="maps/"+mapInfo->atmosphere.skyBox;
	CFileHandler file(out->sourcePath,SPRING_VFS_RAW_FIRST);std::string source;
	if(file.FileSize()<1||file.FileSize()>384*1024*1024||!file.LoadStringData(source))throw content_error("Missing original sky image: "+out->sourcePath);
	sha512::raw_digest digest;sha512::calc_digest(reinterpret_cast<const uint8_t*>(source.data()),source.size(),digest.data());out->sourceIdentity=sha512::dump_digest(digest);
	out->generateMipmaps=configHandler->GetBool("CubeTexGenerateMipMaps");
	if(FileSystem::GetExtensionLowerCase(out->sourcePath)!="dds") {
		out->kind=NativeSkyKind::Equirectangular;out->equirectangular=LoadTextureBytes(out->sourcePath);return out;
	}
	const auto layout=ValidateNativeSkyDDS({reinterpret_cast<const uint8_t*>(source.data()),source.size()});
	nv_dds::CDDSImage image;
	if(!image.load(out->sourcePath,true)||image.get_type()!=nv_dds::TextureCubemap)throw content_error("Original sky DDS cube load failed: "+out->sourcePath);
	size_t block=16;unsigned channels=0;
	switch(image.get_format()) { case 0x83f0:case 0x83f1:out->format=WorldTextureFormat::BC1RGBA;block=8;break;
		case 0x83f2:out->format=WorldTextureFormat::BC2RGBA;break;case 0x83f3:out->format=WorldTextureFormat::BC3RGBA;break;
		case 0x8dbb:out->format=WorldTextureFormat::BC4Unorm;block=8;break;
		case 0x8dbd:out->format=WorldTextureFormat::BC5Unorm;break;
		case 0x8e8c:out->format=WorldTextureFormat::BC7RGBA;break;
		case 0x8e8d:out->format=WorldTextureFormat::BC7RGBASRGB;break;
		case 0x80e0:channels=3;out->format=WorldTextureFormat::RGBA8Unorm;break; // GL_BGR
		case 0x80e1:channels=4;out->format=WorldTextureFormat::RGBA8Unorm;break; // GL_BGRA storage for validated BGRX
		default:throw content_error("Unsupported original sky cube format: "+out->sourcePath); }
	if(channels!=layout.rawChannels||image.get_width()!=layout.size||image.get_num_mipmaps()!=layout.levels-1)
		throw content_error("Original sky DDS decoded layout differs from validated header");
	out->kind=NativeSkyKind::CubeMap;
	out->mipmappedSampling=image.get_num_mipmaps()!=1;
	out->generateMipmaps=image.get_num_mipmaps()==0;
	size_t retainedBytes=0;
	for(uint32_t face=0;face<6;++face) {
		const auto& input=image.get_cubemap_face(face);
		if(input.get_num_mipmaps()!=layout.levels-1)throw content_error("Original sky DDS faces have unequal mip counts");
		uint32_t expectedSize=layout.size;
		auto append=[&](const nv_dds::CSurface& surface) {
			const uint32_t w=surface.get_width(),h=surface.get_height();
			const size_t row=channels?size_t(w)*channels:size_t((w+3)/4)*block;
			const size_t bytes=row*(channels?h:(h+3)/4);
			if(w!=expectedSize||w!=h||surface.get_depth()!=1||surface.get_size()!=bytes)
				throw content_error("Original sky DDS face mip extent mismatch");
			const size_t retained=channels?size_t(w)*h*4:bytes;
			if(retained>384u*1024u*1024u-retainedBytes)throw content_error("Original sky DDS retained storage exceeds limit");
			retainedBytes+=retained;
			const uint8_t* data=surface;WorldTextureMip mip{w,h,row,{}};
			if(channels){
				mip.bytesPerRow=size_t(w)*4;mip.bytes.resize(retained);
				for(size_t i=0;i<size_t(w)*h;++i){
					const auto* q=data+i*channels;auto* p=mip.bytes.data()+i*4;
					p[0]=q[2];p[1]=q[1];p[2]=q[0];p[3]=255;
				}
				// Both admitted raw headers declare no alpha. The X byte in
				// BGRX32 is unused, even though nv_dds labels its storage BGRA.
			}else mip.bytes.assign(data,data+bytes);
			out->cubeFaces[face].push_back(std::move(mip));
			expectedSize=std::max(1u,expectedSize/2);
		};
		append(input);for(uint32_t mip=0;mip<input.get_num_mipmaps();++mip)append(input.get_mipmap(mip));
	}
	// Exactly the original nv_dds true-flip, including its +/-Y swap and tiny
	// tail-mip block rules. No second coordinate or row flip occurs at upload.
	return out;
}


std::shared_ptr<const NativeWorldSpecularResource> CaptureOriginalSpecularCube(const NativeWorldSpecularResource* previous, uint32_t size, uint32_t iteration, uint64_t revision)
{
	if(size<1||size>16384||size_t(size)*size*4>384u*1024u*1024u/6)throw content_error("Original specular cube exceeds bounded capture storage");
	auto out=previous?std::make_shared<NativeWorldSpecularResource>(*previous):std::make_shared<NativeWorldSpecularResource>();out->size=size;out->revision=revision;
	const float3 cdir[6]={{1,1,1},{-1,1,-1},{-1,1,-1},{-1,-1,1},{-1,1,1},{1,1,-1}};
	const float3 dx[6]={{0,0,-2},{0,0,2},{2,0,0},{2,0,0},{2,0,0},{-2,0,0}};
	const float3 dy[6]={{0,-2,0},{0,-2,0},{0,0,2},{0,0,-2},{0,-2,0},{0,-2,0}};
	const auto& sun=ISky::GetSky()->GetLight()->GetLightDir();const float3 color=sunLighting->modelSpecularColor;
	for(size_t face=0;face<6;++face) {
		if(!previous)out->faces[face].resize(size_t(size)*size*4);
		if(previous&&face/2!=iteration%3)continue;
		const uint32_t first=previous?iteration/3:0,end=previous?first+1:size;
		for(uint32_t y=first;y<end;++y)for(uint32_t x=0;x<size;++x) {
			const float3 dir=(cdir[face]+(dx[face]*(x+.5f))/size+(dy[face]*(y+.5f))/size).Normalize();
			const float dot=std::max(0.f,dir.dot(sun));const float spec=std::min(1.f,std::pow(dot,sunLighting->specularExponent)+std::pow(dot,3.f)*.25f);
			for(size_t channel=0;channel<3;++channel){const float value=color[channel]*spec*255;if(!std::isfinite(value)||value<0||value>255)throw content_error("Original specular cube RGBA8 conversion input is invalid");out->faces[face][(size_t(y)*size+x)*4+channel]=static_cast<uint8_t>(value);}
			out->faces[face][(size_t(y)*size+x)*4+3]=255;
		}
	}
	return out;
}

void VerifyOriginalBRDFProducer()
{
	// Use the same exact source contract as LuaNativePBRSource. A separate
	// version-specific list can reject the selected, paired BRDF producer.
	constexpr const char* sources[]={
		"luarules/gadgets/api_pbr_enabler.lua",
		"luarules/gadgets/include/genbrdflut.lua"};
	for(const char* path:sources) {
		const auto selected=std::find_if(pbrsource::Files.begin(),pbrsource::Files.end(),
			[path](const auto& original){return original.path==path;});
		if(selected==pbrsource::Files.end())throw content_error("Missing selected original BRDF source contract: "+std::string(path));
		CFileHandler file(path,SPRING_VFS_MOD);std::string source;
		if(file.FileSize()<1||file.FileSize()>256*1024||!file.LoadStringData(source))throw content_error("Missing original BRDF producer: "+std::string(path));
		sha512::raw_digest digest;sha512::calc_digest(reinterpret_cast<const uint8_t*>(source.data()),source.size(),digest.data());
		if(sha512::dump_digest(digest)!=selected->sha512)throw content_error("Native BRDF requires exact supported original source: "+std::string(path));
	}
}

} // namespace

struct WorldFrameCapture::State {
	const CReadMap* map = nullptr;
	std::string mapPath;
	uint64_t lastFrame = 0;
	std::shared_ptr<const WorldTerrainTiles> terrainTiles;
	std::shared_ptr<const WorldTerrainHeights> terrainHeights;
	std::shared_ptr<const WorldMapEdgeResources> mapEdgeResources;
	std::shared_ptr<const WorldTerrainMaterial> terrainMaterial;
	std::array<float,3> terrainFilterSettings{};
	bool terrainFilterSettingsCaptured = false;
	std::shared_ptr<const WorldTexture> featureBRDF;
	std::unordered_map<const S3DModel*, std::shared_ptr<const WorldModel>> models;
	std::unordered_map<std::string, std::shared_ptr<const WorldTexture>> textures;
	std::unordered_map<const SolidObjectDef*, std::shared_ptr<const WorldModelMaterial>> materials;
	std::unordered_set<std::string> knownTrees;
	bool materialSourcesVerified = false;
	uint64_t materialRevision = 0;
	bool cusEnabled = true;
	NativeWorldParticleCapture particleCapture;
	std::shared_ptr<const WorldModelMaterial> engineStandardMaterial;
	std::shared_ptr<const WorldModelMaterial> OriginalNonCUSMaterial()
	{
		if (!engineStandardMaterial) {
			auto out=std::make_shared<WorldModelMaterial>();out->definitionSource="shaders/GLSL/ModelFragProgGL4.glsl";
			out->definitionName="engine-standard-gl4";out->family=WorldMaterialFamily::EngineModel;
			out->engineStandardGL4=true;out->brightness=1;engineStandardMaterial=std::move(out);
		}
		return engineStandardMaterial;
	}
	std::shared_ptr<const NativeWorldSkyResource> skyResource;
	std::string skySelectionKey;
	std::shared_ptr<const NativeWorldEnvironmentFrame> environmentFrame;
	uint64_t environmentSerial=0,sunDirectionRevision=1,sunLightingRevision=1,skyRevision=1,waterRevision=1;
	bool brdfProducerVerified=false;
	std::shared_ptr<const NativeWorldSpecularResource> specular;
	uint32_t specularIteration=0;
	uint32_t reflectionSize=0,specularSize=0;
	bool reflectionMipmaps=false;

	void VerifyMaterialSources()
	{
		if (materialSourcesVerified) return;
		// Share the selected source identities with the actual Lua CUS loader.
		constexpr const char* sources[] = {
			"LuaRules/Gadgets/cus_gl4.lua",
			"modelmaterials_gl4/known_feature_trees.lua",
			"modelmaterials_gl4/templates/cus_gl4.vert.glsl",
			"modelmaterials_gl4/templates/cus_gl4.frag.glsl",
		};
		for (const char* path : sources) {
			CFileHandler file(path, SPRING_VFS_MOD);
			std::string bytes;
			if (file.FileSize() < 1 || file.FileSize() > 2 * 1024 * 1024 || !file.LoadStringData(bytes))
				throw content_error("Missing native CUS material source: " + std::string(path));
			sha512::raw_digest digest;
			sha512::calc_digest(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), digest.data());
			if (!IsNativeCUSSelectedSource(path, sha512::dump_digest(digest)))
				throw content_error("Native CUS material selector requires its exact supported source: " + std::string(path));
		}
		LuaParser parser("modelmaterials_gl4/known_feature_trees.lua", SPRING_VFS_MOD, SPRING_VFS_MOD);
		if (!parser.Execute()) throw content_error("Cannot read original CUS tree metadata: " + parser.GetErrorLog());
		std::vector<std::string> names;
		parser.GetRoot().GetKeys(names);
		knownTrees.insert(names.begin(), names.end());
		materialSourcesVerified = true;
	}

	std::shared_ptr<const WorldModelMaterial> Material(const SolidObjectDef& def, const WorldModel& model, bool feature)
	{
		const auto found = materials.find(&def);
		if (found != materials.end()) return found->second;
		auto out = std::make_shared<WorldModelMaterial>();
		if (!cusEnabled) {
			out->definitionSource = "shaders/GLSL/ModelFragProgGL4.glsl";
			out->definitionName = "engine-standard-gl4";
			out->family = WorldMaterialFamily::EngineModel;
			out->engineStandardGL4 = true;
			out->brightness = 1;
			materials.emplace(&def, out);
			return out;
		}
		VerifyMaterialSources();
		out->definitionSource = "LuaRules/Gadgets/cus_gl4.lua";
		const bool tree = feature && def.name.find("raptor_egg") == std::string::npos && (knownTrees.contains(def.name) ||
			(def.customParams.find("treeshader") != def.customParams.end() && def.customParams.find("treeshader")->second == "yes"));
		const bool wreck = feature && (def.name.find("raptor_egg") != std::string::npos ||
			(!tree && (def.name.find("_dead") != std::string::npos || def.name.find("_heap") != std::string::npos)));
		const bool featurePBR = feature && !tree && !wreck && (def.name.find("pilha_crystal") != std::string::npos || def.customParams.contains("cuspbr"));
		out->definitionName = feature ? (tree ? "tree" : wreck ? "wreck" : featurePBR ? "featurepbr" : "feature") : "otherunit";
		if (!feature) {
			std::string lower = model.textureReferences[0]; std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
			if (lower.starts_with("arm")) out->definitionName="armunit";
			else if (lower.starts_with("cor")) out->definitionName="corunit";
			else if (lower.starts_with("leg")) out->definitionName="legunit";
			if (def.name.find("_scav")!=std::string::npos) {
				if(lower.starts_with("arm"))out->definitionName="armscavenger";
				else if(lower.starts_with("cor"))out->definitionName="corscavenger";
				else if(lower.starts_with("leg"))out->definitionName="legscavenger";
			}else if(def.name.find("raptor")!=std::string::npos)out->definitionName="raptor";
			if(def.customParams.contains("useskinning"))out->definitionName="otherunit";
		}
		out->family = feature ? (tree ? WorldMaterialFamily::FeatureTreeNormal : wreck ? WorldMaterialFamily::FeatureMetalWreck : WorldMaterialFamily::FeatureOther) : WorldMaterialFamily::UnitNormal;
		out->treeWind = tree;
		out->alphaShadows = tree;
		out->normalMapping = true;
		out->environmentDiffuse = out->environmentSpecular = true;
		out->healthDisplacement = out->healthTexturing = !feature && out->definitionName!="otherunit";
		out->vertexAO = out->flashlights = !feature && out->definitionName!="otherunit";
		out->pbrOverride = feature && !wreck && !featurePBR;
		out->metalness = .1f; out->roughness = .8f; out->emissiveness = .2f;
		out->brightness = feature ? 1.3f : 1.5f;
		std::string normalPath;
		// Match selected CUS GetNormal: a missing custom path is not selected.
		// Existence is not decode validity; present corrupt images still fail loading.
		if (auto it = def.customParams.find("normaltex"); it != def.customParams.end() &&
			CFileHandler::FileExists(it->second, SPRING_VFS_RAW_FIRST)) normalPath = it->second;
		if (normalPath.empty() && feature) {
			const auto& tex1 = model.textureReferences[0];
			if (tex1 == "Arm_wreck_color.dds") normalPath = "unittextures/Arm_wreck_color_normal.dds";
			else if (tex1 == "cor_color_wreck.dds") normalPath = "unittextures/cor_color_wreck_normal.dds";
			else if (tex1 == "leg_wreck_color.dds") normalPath = "unittextures/leg_wreck_normal.dds";
			else for (const char* suffix : {"_normals.", "_normal."}) {
				std::string candidate = "unittextures/" + tex1;
				for (size_t pos = 0; (pos = candidate.find('.', pos)) != std::string::npos; pos += std::strlen(suffix)) candidate.replace(pos, 1, suffix);
				if (CFileHandler::FileExists(candidate, SPRING_VFS_RAW_FIRST)) { normalPath = std::move(candidate); break; }
			}
		}
		if (normalPath.empty()) normalPath = "unittextures/blank_normal.dds"; // Exact original GetNormal fallback, not invented pixels.
		out->requestedNormalPath = normalPath;
		out->normal = Texture(normalPath);
		materials.emplace(&def, out);
		return out;
	}

	std::shared_ptr<const WorldTexture> Texture(const std::string& reference)
	{
		const std::string path = ResolveTexturePath(reference);
		const auto found = textures.find(path);
		if (found != textures.end())
			return found->second;
		auto result = LoadTextureBytes(path);
		textures.emplace(path, result);
		return result;
	}

	std::shared_ptr<const WorldModel> Model(const S3DModel* source)
	{
		const auto found = models.find(source);
		if (found != models.end())
			return found->second;
		auto model = std::make_shared<WorldModel>();
		bool rigidPerPiece = true;
		{
			auto lock = CModelsLock::GetScopedLock();
			if (source->type < MODELTYPE_3DO || source->type >= MODELTYPE_CNT || source->loadStatus != S3DModel::LOADED)
				throw content_error("Original model source requires a loaded 3DO/S3O/ASS model");
			// All original model formats publish this same SVertexData ABI and local
			// triangulated index list. Source engine/CUS capture owns the material;
			// this cache only retains the actual loaded geometry and bind pose.
			model->engineModelId = source->id;
			model->sourceName = source->name;
			model->textureReferences = source->texs;
			model->minimum = CopyVector(source->mins);
			model->maximum = CopyVector(source->maxs);
			std::unordered_map<const S3DModelPiece*, uint32_t> offsets;
			for (uint32_t i = 0; i < source->pieceObjects.size(); ++i)
				offsets.emplace(source->pieceObjects[i], i);
			model->pieces.reserve(source->pieceObjects.size());
			for (const S3DModelPiece* piece : source->pieceObjects) {
				WorldModelPiece output;
				output.name = piece->name;
				if (piece->parent != nullptr)
					output.parent = offsets.at(piece->parent);
				output.bindPose = CopyMatrix(piece->bposeTransform.ToMatrix());
				output.vertices.reserve(piece->GetVerticesVec().size());
				for (const SVertexData& vertex : piece->GetVerticesVec()) {
					WorldModelVertex value;
					value.position = CopyVector(vertex.pos);
					value.normal = CopyVector(vertex.normal);
					value.sTangent = CopyVector(vertex.sTangent);
					value.tTangent = CopyVector(vertex.tTangent);
					for (size_t i = 0; i < value.uv.size(); ++i)
						value.uv[i] = {vertex.texCoords[i].x, vertex.texCoords[i].y};
					for (size_t i = 0; i < value.boneIds.size(); ++i) {
						value.boneIds[i] = static_cast<uint16_t>(vertex.boneIDsLow[i]) |
							(static_cast<uint16_t>(vertex.boneIDsHigh[i]) << 8);
						value.boneWeights[i] = vertex.boneWeights[i];
					}
					output.vertices.push_back(value);
					// Emitter-only pieces retain unassigned bones and CPU/pose slots,
					// but the original HasGeometryData boundary omits their GPU vertices.
					if (piece->HasGeometryData() && (value.boneIds[0] != model->pieces.size() || value.boneWeights[0] != 255 ||
						value.boneWeights[1] || value.boneWeights[2] || value.boneWeights[3]))
						rigidPerPiece = false;
				}
				output.indices = piece->GetIndicesVec();
				for (uint32_t index : output.indices) {
					if (index >= output.vertices.size())
						throw content_error("Original model piece has invalid local indices: " + piece->name);
				}
				model->pieces.push_back(std::move(output));
			}
		}
		// This is a GL4 consumer capability, not a restriction on legacy/CUS models.
		model->gl4BoneLayout = rigidPerPiece ? WorldModelGL4BoneLayout::RigidPerPiece : WorldModelGL4BoneLayout::Unsupported;
		// Native handler owns the one decode/upload publication. Geometry is
  // cached once; source-time material pairs belong to each actual draw.
  if(!OriginalNativeS3OTextures().Attached())
   for(size_t i=0;i<model->textures.size();++i)model->textures[i]=Texture(model->textureReferences[i]);
		models.emplace(source, model);
		return model;
	}

	std::shared_ptr<const WorldTerrainMaterial> TerrainMaterial()
	{
		auto* smf = dynamic_cast<CSMFReadMap*>(readMap);
		const uint64_t revision = smf->GetNativeMapEdgeResourceRevision();
		if (!terrainMaterial || terrainMaterial->sourceRevision != revision) {
			auto out = std::make_shared<WorldTerrainMaterial>(); out->sourceRevision = revision;
			// Reuse immutable bytes from the real original texture creation sites,
			// including their exact failed-load fallback and reload generations.
			// DDS uses the existing common loader; the native source never publishes
			// HEADLESS CBitmap's dummy as the original texture.
			using Role=NativeTerrainImageRole;
			out->detail=smf->GetNativeTerrainImage(Role::Detail);
			if(!out->detail)throw content_error("Original detail texture producer has no retained native image");
			out->specular=smf->GetNativeTerrainImage(Role::Specular);
			out->blendNormals=smf->GetNativeTerrainImage(Role::BlendNormals);
			out->emission=smf->GetNativeTerrainImage(Role::Emission);
			out->splatDetail=smf->GetNativeTerrainImage(Role::SplatDetail);
			out->splatDistribution=smf->GetNativeTerrainImage(Role::SplatDistribution);
			for(size_t i=0;i<out->splatNormals.size();++i)
				out->splatNormals[i]=smf->GetNativeTerrainImage(Role(unsigned(Role::SplatNormal0)+i));
			out->skyReflectionModulation=mapInfo->smf.skyReflectModTexName;
			out->parallaxHeight=mapInfo->smf.parallaxHeightTexName;
			out->splatScales = CopyFour(mapRendering->splatTexScales);
			out->splatMultipliers = CopyFour(mapRendering->splatTexMults);
			out->splatNormalDiffuseAlpha = mapRendering->splatDetailNormalDiffuseAlpha;
			terrainMaterial = out;
		}
		// Runtime rendering flags and deformations are not immutable source data.
		auto out = std::make_shared<WorldTerrainMaterial>(*terrainMaterial);
		if (!terrainFilterSettingsCaptured) {
			// Original ConfigureTexAnisotropyLevels and ground-texture LOD bias
			// are map-initialization settings, not per-frame quality heuristics.
			terrainFilterSettings = {configHandler->GetFloat("SMFTexAniso"), configHandler->GetFloat("SSMFTexAniso"), configHandler->GetFloat("SMFTextureLodBias")};
			terrainFilterSettingsCaptured = true;
		}
		out->smfAnisotropy = terrainFilterSettings[0];
		out->ssmfAnisotropy = terrainFilterSettings[1];
		out->textureLodBias = terrainFilterSettings[2];
		out->voidWater = mapRendering->voidWater; out->voidGround = mapRendering->voidGround;
		out->waterAbsorption = readMap->HasVisibleWater(); out->voidAlphaMinimum = mapInfo->map.voidAlphaMin;
		out->minimumHeight = readMap->GetCurrMinHeight();
		out->advancedShading = readMap->GetGroundDrawer()->UseAdvShading();
		// This is the last actual original shading write, with its immutable
		// lighting/height inputs. Capture never repaints an inferred full map.
		out->standardShading=smf->GetNativeTerrainShadingSource();
		if(!out->standardShading)
			throw content_error("Original SMF shading has no actual retained producer write");
		return out;
	}

	void CacheTerrainHeights(const CReadMap* actualSourceMap=readMap)
	{
		const uint32_t width = static_cast<uint32_t>(mapDims.mapxp1);
		const uint32_t height = static_cast<uint32_t>(mapDims.mapyp1);
		const size_t count = static_cast<size_t>(width) * height;
		if(!actualSourceMap)throw content_error("Original height producer map owner is missing");
		const float* values = actualSourceMap->GetCornerHeightMapUnsynced();
		if (values == nullptr || count > std::numeric_limits<size_t>::max() / sizeof(float))
			throw content_error("Missing original unsynced terrain heights");
		if (terrainHeights == nullptr || terrainHeights->width != width || terrainHeights->height != height ||
			std::memcmp(values, terrainHeights->corners.data(), count * sizeof(float)) != 0) {
			auto heights = std::make_shared<WorldTerrainHeights>();
			heights->width = width;
			heights->height = height;
			heights->corners.assign(values, values + count);
			for (float value : heights->corners) {
				if (!std::isfinite(value))
					throw content_error("Non-finite original terrain height");
			}
			terrainHeights = std::move(heights);
		}
	}

	WorldTerrain Terrain()
	{
		if (terrainTiles == nullptr) {
			const auto* smf=dynamic_cast<const CSMFReadMap*>(readMap);
			if (smf == nullptr)
				throw content_error("World scene supports original SMF terrain only");
			// The actual original LoadTiles producer retains this payload. Water
			// square publications and the existing map cache share the same owner;
			// no second file load, decoder or reconstructed square image exists.
			terrainTiles=smf->GetNativeTerrainTiles();
			if (!terrainTiles)
				throw content_error("Original SMF LoadTiles has no retained native source payload");
		}
		CacheTerrainHeights();
		return {mapPath, static_cast<uint32_t>(mapDims.mapx), static_cast<uint32_t>(mapDims.mapy),
			8.0f, terrainTiles, terrainHeights, TerrainMaterial()};
	}

	WorldMapEdge MapEdge(std::shared_ptr<const mapedge::DrawPacket> draw)
	{
		WorldMapEdge result;result.draw=std::move(draw);
		// The verified original widget owns runtime gl.GetMapRendering eligibility
		// and its initialization/removal lifecycle. Do not substitute mapInfo's
		// initial flags or invent a second per-frame eligibility policy here.
		if (result.draw->mirrors.empty()) return result;
		const auto* smfMap=dynamic_cast<const CSMFReadMap*>(readMap);
		if (!smfMap) throw content_error("Native map-edge supports original SMF maps only");
		if (smfMap->HasNativeMapEdgeLuaOverride())
			throw content_error("Native map-edge Lua map texture override requires its owned native producer; GL ids cannot be substituted");
		const auto revision=smfMap->GetNativeMapEdgeResourceRevision();
		if (!mapEdgeResources || mapEdgeResources->sourceRevision!=revision) {
			auto next=std::make_shared<WorldMapEdgeResources>();next->sourceRevision=revision;
			// Retain the exact original raw publication, including the MiniMap
			// alias and reload generation. Never reopen the same image by path.
			next->color=smfMap->GetNativeTerrainImage(NativeTerrainImageRole::GrassShading);
			next->normal=smfMap->GetNativeTerrainImage(NativeTerrainImageRole::BlendNormals);
			if(!next->color || !next->normal)
				throw content_error("Native map-edge lacks its original published grass/minimap or blend-normal image");
			// Forward map-edge images use the shared original upload/mip producer
			// and retain each image's filter and orientation policy.
			for(const auto& image:{next->color,next->normal})
				if(image->mips.empty())throw content_error("Native map-edge original image has no uploaded base");
			mapEdgeResources=std::move(next);
		}
		result.resources=mapEdgeResources;
		// Exact UniformConstants source values; no independent fake sun/fog.
		const auto& sky=ISky::GetSky();
		const float4 color=sky?float4(sky->fogColor.x,sky->fogColor.y,sky->fogColor.z,1):float4(.7f,.7f,.8f,1);
		const auto* playerCamera=CCameraHandler::GetCamera(CCamera::CAMTYPE_PLAYER);
		if (!playerCamera) throw content_error("Native map-edge requires actual player far-plane distance");
		const float4 fog=sky?float4(sky->fogStart*playerCamera->GetFarPlaneDist(),sky->fogEnd*playerCamera->GetFarPlaneDist(),0,0):float4(.1f*CGlobalRendering::MAX_VIEW_RANGE,1.f*CGlobalRendering::MAX_VIEW_RANGE,0,0);
		const float4 sun=sky?sky->GetLight()->GetLightDir():float4(0,.447214f,.894427f,1);
		result.fogColor={color.x,color.y,color.z,color.w};result.sunDirection={sun.x,sun.y,sun.z,sun.w};
		result.fogParameters={fog.x,fog.y,0,1.f/(fog.y-fog.x)};
		return result;
	}
};

WorldFrameCapture::WorldFrameCapture(): state(std::make_unique<State>()) {}
WorldFrameCapture::~WorldFrameCapture() = default;

void WorldFrameCapture::Reset()
{
	state = std::make_unique<State>();
}

std::shared_ptr<const NativeMiniMapBackground> WorldFrameCapture::CaptureTerrainShadingSource(
	const CSMFReadMap& actualMap,std::uint64_t writeRevision)
{
	if(!writeRevision || !sunLighting || !waterRendering || !ISky::GetSky() || !ISky::GetSky()->GetLight())
		throw content_error("Original SMF shading write lacks its actual source inputs");
	// Use the explicit original owner: constructor-time writes may precede
	// assignment of the global readMap. The same existing cache serves frames.
	state->CacheTerrainHeights(&actualMap);
	auto source=std::make_shared<NativeMiniMapBackground>();
	source->sourceRevision=source->updateSerial=writeRevision;
	source->squaresX=mapDims.mapx;source->squaresZ=mapDims.mapy;
	source->paddedSquaresX=mapDims.pwr2mapx;source->paddedSquaresZ=mapDims.pwr2mapy;
	source->base=actualMap.GetNativeMiniMapImage();source->heights=state->terrainHeights;
	source->lighting.groundAmbient=CopyFour(sunLighting->groundAmbientColor);
	source->lighting.groundDiffuse=CopyFour(sunLighting->groundDiffuseColor);
	source->lighting.lightDirection=CopyFour(ISky::GetSky()->GetLight()->GetLightDir());
	source->lighting.waterBase=CopyVector(waterRendering->baseColor);
	source->lighting.waterAbsorb=CopyVector(waterRendering->absorb);
	source->lighting.waterMinimum=CopyVector(waterRendering->minColor);
	source->lighting.waterLevel=CGround::GetWaterPlaneLevel();
	return source;
}

std::shared_ptr<const WorldTerrainHeights> WorldFrameCapture::PrepareMapHeights()
{
	if(!readMap||!gameSetup||!gs||!globalRendering)throw content_error("Original map publication requires its loaded owner");
	const auto path=gameSetup->MapFileName();const auto frame=uint64_t(std::max(0,gs->frameNum));
	if(state->map!=readMap||state->mapPath!=path||frame<state->lastFrame){Reset();state->map=readMap;state->mapPath=path;}
	state->lastFrame=frame;state->CacheTerrainHeights();return state->terrainHeights;
}
std::shared_ptr<const NativeWorldEnvironmentFrame> WorldFrameCapture::PrepareEnvironmentBeforePreUnit()
{
	if(!readMap||!mapInfo||!gameSetup||!gs||!globalRendering)throw content_error("Original native IBL stage requires a loaded map");
	const std::string path=gameSetup->MapFileName();const uint64_t frame=uint64_t(std::max(0,gs->frameNum));
	if(state->map!=readMap||state->mapPath!=path||frame<state->lastFrame){Reset();state->map=readMap;state->mapPath=path;}
	state->lastFrame=frame;
	if(!state->brdfProducerVerified){VerifyOriginalBRDFProducer();state->brdfProducerVerified=true;}
	const auto& sky=ISky::GetSky();if(!sky)throw content_error("Original IBL sky state is absent");
	const std::string skyKey=sky->GetName()+":"+mapInfo->atmosphere.skyBox+":"+(globalRendering->drawDebugCubeMap?"debug":"normal");
	if(!state->skyResource||state->skySelectionKey!=skyKey){state->skyResource=LoadOriginalSkyResource();state->skySelectionKey=skyKey;}
	auto bump=[](uint64_t& value){if(value==UINT64_MAX)throw content_error("Native environment revision overflow");++value;};
	const bool sourceCubes = OriginalNativeCubeTextures().Attached();
	const auto reflectionLighting = CaptureLighting();
	// Original IBL order: reflection face first, then sun delivery and the
	// CPU specular rows. CubeMapHandler owns the cursor and the actual bytes.
	if (sourceCubes) cubeMapHandler.UpdateReflectionTexture();
	// Exactly the original GenerateIBLTextures change delivery, consumed once.
	const bool directionChanged=sky->GetLight()->Update();
	if(directionChanged)bump(state->sunDirectionRevision);
	if (sourceCubes && directionChanged) cubeMapHandler.UpdateSpecularTexture();
	if(sunLighting->IsUpdated())bump(state->sunLightingRevision);
	if(sky->IsUpdated())bump(state->skyRevision);
	if(waterRendering->IsUpdated())bump(state->waterRevision);
	auto out=std::make_shared<NativeWorldEnvironmentFrame>();bump(state->environmentSerial);out->captureSerial=state->environmentSerial;
	out->sunDirectionRevision=state->sunDirectionRevision;out->sunLightingRevision=state->sunLightingRevision;out->skyRevision=state->skyRevision;out->waterRevision=state->waterRevision;
	out->sky=state->skyResource;out->terrain=state->Terrain();out->lighting=CaptureLighting();out->reflectionLighting=sourceCubes?reflectionLighting:out->lighting;
	const auto metalInfo = CaptureNativeMetalInfo();
	if (metalInfo && metalInfo->active) out->metalInfoFrame = metalInfo;
	out->worldInfoFrame = CaptureNativeWorldInfo();
	if (sourceCubes) {
		auto& registry = OriginalNativeTextureContext().Registry();
		out->sourceReflection = registry.Find(cubeMapHandler.GetEnvReflectionTextureID());
		out->sourceSkyReflection = registry.Find(cubeMapHandler.GetSkyReflectionTextureID());
		out->sourceSpecular = registry.Find(cubeMapHandler.GetSpecularTextureID());
		if (!out->sourceReflection || !out->sourceSpecular) throw content_error("Original named cube allocation was not initialized before IBL");
		out->sourceCubeUpdate = std::make_shared<NativeCubeReflectionUpdate>(OriginalNativeCubeTextures().CurrentReflectionUpdate());
		out->reflectionGround=scene::TakeOriginalTerrainReflectionFace();
		state->reflectionSize = cubeMapHandler.GetReflectionTextureSize();
		state->specularSize = cubeMapHandler.GetSpecularTextureSize();
		state->reflectionMipmaps = OriginalNativeCubeTextures().Image(cubeMapHandler.GetEnvReflectionTextureID())->levels > 1;
		out->reflectionSize=state->reflectionSize;out->specularSize=state->specularSize;
	} else {
	if(state->reflectionSize==0){state->reflectionSize=out->lighting.requested.reflectionCubeSize;state->specularSize=out->lighting.requested.specularCubeSize;state->reflectionMipmaps=out->lighting.requested.reflectionMipmaps;}
	out->reflectionSize=state->reflectionSize;out->specularSize=state->specularSize;
	if(!state->specular||state->specular->size!=out->specularSize){state->specular=CaptureOriginalSpecularCube(nullptr,out->specularSize,0,state->sunDirectionRevision);state->specularIteration=0;}
	else if(directionChanged){state->specular=CaptureOriginalSpecularCube(state->specular.get(),out->specularSize,state->specularIteration,state->sunDirectionRevision);state->specularIteration=(state->specularIteration+1)%(out->specularSize*3);}
	out->specular=state->specular;
	}
	out->reflectionMipmaps=state->reflectionMipmaps;out->skyOnlyReflections=!mapInfo->smf.skyReflectModTexName.empty();
	out->skyAxisAngle=CopyFour(sky->GetSkyAxisAngle());CMatrix44f rotation;rotation.Rotate(sky->GetSkyAxisAngle().w,float3(sky->GetSkyAxisAngle().x,sky->GetSkyAxisAngle().y,sky->GetSkyAxisAngle().z));out->skyRotation=CopyMatrix(rotation);out->skyColor=CopyFour(float4(sky->skyColor,1));out->sunColor=CopyFour(float4(sky->sunColor,sky->GetLight()->GetLightDir().w));
	out->cloudColor=CopyFour(float4(sky->cloudColor,mapInfo->atmosphere.cloudDensity));out->waterPlaneColor=CopyFour(float4(waterRendering->planeColor,float(waterRendering->hasWaterPlane&&!globalRendering->drawDebugCubeMap)));
	out->drawSky=globalRendering->drawSky;out->drawGround=globalRendering->drawGround;out->drawFog=globalRendering->drawFog;out->animationTime=(float(gs->frameNum)+globalRendering->timeOffset)*.005f;
	if(globalRendering->drawDebugCubeMap)out->unsupported.push_back("Original debug-cubemap visualization");
	if(sourceCubes) {
  if(!out->reflectionGround||out->reflectionGround->reflectionFace!=out->sourceCubeUpdate->face||out->reflectionGround->skyOnly!=out->sourceCubeUpdate->skyOnly)
   throw content_error("Original reflection source lost its actual selected face/camera");
  out->reflectionCameras[out->sourceCubeUpdate->face]=out->reflectionGround->viewCamera;
 } else {
 const CCamera* active=CCameraHandler::GetActiveCamera();if(!active)throw content_error("Original reflection requires actual active camera");
	const float3 dirs[6][3]={{RgtVector,FwdVector,UpVector},{-RgtVector,-FwdVector,UpVector},{UpVector,-RgtVector,-FwdVector},{-UpVector,-RgtVector,FwdVector},{FwdVector,-RgtVector,UpVector},{-FwdVector,RgtVector,UpVector}};
	for(size_t i=0;i<6;++i) {
		CCamera env=*CCameraHandler::GetCamera(CCamera::CAMTYPE_ENVMAP);env.forward=dirs[i][0];env.right=-dirs[i][1];env.up=-dirs[i][2];
		env.SetVFOV(90);env.SetAspectRatio(1);env.SetPos(active->GetPos());env.UpdateViewPort(0,0,out->reflectionSize,out->reflectionSize);env.UpdateViewRange();
		env.UpdateMatrices(globalRendering->viewSizeX,globalRendering->viewSizeY,env.GetAspectRatio());env.UpdateFrustum();
		auto& target=out->reflectionCameras[i];target.view=CopyMatrix(env.GetViewMatrix());target.projection=CopyMatrix(env.GetProjectionMatrix());target.position=CopyVector(env.GetPos());
		std::copy_n(env.viewport,4,target.viewport.begin());target.clipDepth=GetClipDepth(env);
	}
 }
	state->environmentFrame=std::move(out);
	return state->environmentFrame;
}

WorldFrame WorldFrameCapture::Capture(NativeWaterScenePass pass,
 std::shared_ptr<const NativeOriginalShadowFrame> originalShadow)
{
	if (readMap == nullptr || gameSetup == nullptr || mapInfo == nullptr || gs == nullptr || gu == nullptr ||
		globalRendering == nullptr || mapDims.mapx <= 0 || mapDims.mapy <= 0)
		throw content_error("World capture requires a fully loaded engine simulation");
	const CCamera* activeCamera = CCameraHandler::GetActiveCamera();
	if (activeCamera == nullptr)
		throw content_error("World capture requires the actual engine camera");
	if (!gu->spectatingFullView && (gu->myAllyTeam < 0 || gu->myAllyTeam >= teamHandler.ActiveAllyTeams()))
		throw content_error("World capture requires a valid viewer ally team");
	const std::string mapPath = gameSetup->MapFileName();
	const uint64_t frame = static_cast<uint64_t>(std::max(gs->frameNum, 0));
	if (state->map != readMap || state->mapPath != mapPath || frame < state->lastFrame) {
		Reset();
		state->map = readMap;
		state->mapPath = mapPath;
	}
	state->lastFrame = frame;
	WorldFrame result;
	result.cusMaterial = CaptureNativeCUSMaterialState(configHandler->GetIntSafe("cus2", 1));
	if (result.cusMaterial.revision != state->materialRevision) {
		state->materials.clear();
		state->knownTrees.clear();
		state->materialSourcesVerified = false;
		state->materialRevision = result.cusMaterial.revision;
		state->cusEnabled = result.cusMaterial.enabled;
	}
	result.simulationFrame = frame;
	result.sourceDrawFrame = globalRendering->drawFrame;
	result.interpolationOffset = globalRendering->timeOffset;
	result.viewCamera.view = CopyMatrix(activeCamera->GetViewMatrix());
	result.viewCamera.projection = CopyMatrix(activeCamera->GetProjectionMatrix());
	result.viewCamera.position = CopyVector(activeCamera->GetPos());
	std::copy_n(activeCamera->viewport, result.viewCamera.viewport.size(), result.viewCamera.viewport.begin());
	result.viewCamera.clipDepth = GetClipDepth(*activeCamera);
	if (result.viewCamera.clipDepth == ClipDepthConvention::Unsupported)
		throw content_error("Unsupported actual engine camera clip-control matrix");
	result.terrain = state->Terrain();
	const auto metalInfo = CaptureNativeMetalInfo();
	if (metalInfo && metalInfo->active) result.metalInfoFrame = metalInfo;
	result.lighting = CaptureLighting();
	if(!state->environmentFrame)throw content_error("Original environment CPU stage must run before world capture");
	result.environmentFrame=state->environmentFrame;
	if (!state->featureBRDF) state->featureBRDF = state->Texture("modelmaterials_gl4/brdf_0.png");
	result.featureBRDF = state->featureBRDF;
	if (shadowHandler.shadowConfig > 0) {
		WorldShadow shadow;
		shadow.view = CopyMatrix(shadowHandler.GetShadowViewMatrix());
		shadow.projection = CopyMatrix(shadowHandler.GetShadowProjMatrix());
		shadow.clipDepth = GetClipDepth(*CCameraHandler::GetCamera(CCamera::CAMTYPE_SHADOW));
		shadow.parameters = CopyFour(shadowHandler.GetShadowParams());
		shadow.size = shadowHandler.shadowMapSize; shadow.generationBits = shadowHandler.shadowGenBits;
		shadow.terrainSlopeBias = readMap->GetGroundDrawer()->spPolygonOffsetScale;
		shadow.terrainDepthBias = readMap->GetGroundDrawer()->spPolygonOffsetUnits;
		result.shadow = shadow;
	}
	if(pass==NativeWaterScenePass::Main && result.shadow){
  if(OriginalNativeShadowTextures().Attached()&&shadowHandler.HasNativeTextureOwner()){
   if(!originalShadow||originalShadow->sourceDrawFrame!=uint64_t(globalRendering->drawFrame)||!originalShadow->textures)
    throw std::logic_error("Native world capture lost the actual original pre-IBL shadow source");
   result.originalShadow=std::move(originalShadow);
  }else result.shadowColorFrame=CaptureNativeShadowColor();
 }
	result.worldInfoFrame=CaptureNativeWorldInfo();
	if (auto draw=mapedge::Snapshot()) result.mapEdge=state->MapEdge(std::move(draw));
	// Screen/minimap icons, original model source passes and Lua effects have
	// separate consumers. A family-wide unsupported list cannot describe them.
	// Current-pose legacy model limitations are reported only if that consumer
	// actually encodes a piece; the original source route has its own transforms.
	for (const CUnit* unit : unitHandler.GetActiveUnits()) {
		if (unit->isDead || unit->model == nullptr || unit->noDraw)
			continue;
		if (!gu->spectatingFullView && !unit->IsInLosForAllyTeam(gu->myAllyTeam))
			continue; // Never expose a radar-only or hidden full model.
		if (unit->model->type != MODELTYPE_S3O) {
			result.unsupported.push_back("Model format: " + unit->model->name);
			continue;
		}
		WorldUnit instance;
		instance.unitId = unit->id;
		instance.teamId = unit->team;
		instance.selected = unit->isSelected;
		const auto passes = ClassifyNativeWaterModelPasses(unit->drawFlag,unit->engineDrawMask,pass);
		instance.opaque = passes.opaque;
		instance.alpha = passes.alpha;
		instance.drawAlpha = passes.alpha ? IModelDrawerState::alphaValues.x : 1;
		instance.castsShadow = unit->HasDrawFlag(DrawFlags::SO_SHOPAQ_FLAG);
		instance.world = CopyMatrix(unit->GetTransformMatrix(false));
		instance.model = state->Model(unit->model);
		instance.modelTextures=CaptureNativeS3OModelTextures(unit->model);
		const CTeam* team = teamHandler.Team(unit->team);
		std::copy_n(team->color, instance.teamColor.size(), instance.teamColor.begin());
		instance.pieces = CapturePieces(unit->localModel, *instance.model);
		// Exact30770 CUS bins are 1 (forward), 5 (reflection), 16 (shadow);
		// alpha and refraction retain the original engine-standard shader.
		instance.material = instance.alpha || pass == NativeWaterScenePass::Refraction ?
			state->OriginalNonCUSMaterial() : state->Material(*unit->unitDef, *instance.model, false);
		instance.healthFraction = std::clamp(unit->health / std::max(.01f, unit->maxHealth), 0.f, 1.f);
		result.units.push_back(std::move(instance));
	}
	for (const CFeature* feature : CFeatureDrawer::GetUnsortedFeatures()) {
		if (!feature || !feature->model || !feature->def || feature->def->drawType != DRAWTYPE_MODEL) continue;
		// Consume the original draw policy output, without invoking ShouldDraw*
		// (those helpers append to Lua material queues as a side effect).
		const bool shadow = feature->HasDrawFlag(DrawFlags::SO_SHOPAQ_FLAG) || feature->HasDrawFlag(DrawFlags::SO_SHTRAN_FLAG);
		const auto waterPasses = ClassifyNativeWaterModelPasses(feature->drawFlag,feature->engineDrawMask,pass);
		if ((!waterPasses.opaque && !waterPasses.alpha && !(pass == NativeWaterScenePass::Main && shadow)) || feature->noDraw || feature->IsInVoid() || feature->drawAlpha <= 0) continue;
		if (!gu->spectatingFullView && !feature->IsInLosForAllyTeam(gu->myAllyTeam)) continue;
		if (feature->luaDraw) { result.unsupported.push_back("Lua-owned feature drawing: " + feature->def->name); continue; }
		if (feature->model->type != MODELTYPE_S3O) { result.unsupported.push_back("Feature model format: " + feature->model->name); continue; }
		WorldFeature instance;
		instance.featureId = feature->id; instance.definitionId = feature->def->id; instance.teamId = feature->team;
		instance.model = state->Model(feature->model);instance.modelTextures=CaptureNativeS3OModelTextures(feature->model); instance.world = CopyMatrix(feature->GetTransformMatrixRef(false));
		instance.material = waterPasses.alpha || pass == NativeWaterScenePass::Refraction ?
			state->OriginalNonCUSMaterial() : state->Material(*feature->def, *instance.model, true);
		instance.pieces = CapturePieces(feature->localModel, *instance.model);
		const CTeam* team = teamHandler.IsValidTeam(feature->team) ? teamHandler.Team(feature->team) : nullptr;
		if (team) std::copy_n(team->color, instance.teamColor.size(), instance.teamColor.begin());
		else instance.teamColor = {255,255,255,255};
		instance.drawAlpha = feature->drawAlpha;
		instance.healthFraction = std::clamp(feature->health / std::max(.01f, feature->maxHealth), 0.f, 1.f);
		static_assert(DrawFlags::SO_OPAQUE_FLAG==1 && DrawFlags::SO_ALPHAF_FLAG==2 && DrawFlags::SO_SHOPAQ_FLAG==16);
		const auto passes = ClassifyNativeFeaturePasses(feature->drawFlag,feature->engineDrawMask);
		instance.opaque = waterPasses.opaque; instance.fading = waterPasses.alpha; instance.castsShadow = pass == NativeWaterScenePass::Main && passes.shadow;
		instance.inWater = feature->IsInWater();
		if (instance.opaque || instance.fading || instance.castsShadow) result.features.push_back(std::move(instance));
	}
	result.particleFrame = state->particleCapture.Capture(*activeCamera, [this](const S3DModel* model) { return CaptureModel(model); }, pass);
	return result;
}

std::shared_ptr<const WorldModel> WorldFrameCapture::CaptureModel(const S3DModel* model)
{
	if (!model) throw content_error("Cannot capture a null original model");
	return state->Model(model);
}

std::shared_ptr<const WorldTexture> WorldFrameCapture::CaptureTextureSource(const std::string& path, bool reload)
{
	if (!CFileHandler::FileExists(path, SPRING_VFS_RAW_FIRST)) return {};
	if (!reload) {
		const auto found = state->textures.find(path);
		if (found != state->textures.end()) return found->second;
	}
	auto image = LoadTextureBytes(path, false, true);
	if (image) state->textures[path] = image;
	return image;
}

std::shared_ptr<const WorldTexture> WorldFrameCapture::CaptureTexture(const std::string& path)
{
	return state->Texture(path);
}

std::shared_ptr<const NativeEngineModelObject> WorldFrameCapture::CaptureEngineUnit(const CUnit& unit,bool modern,const WorldMatrix& actualModelView,const std::array<float,4>& actualProgramTeam)
{
	auto lock=CModelsLock::GetScopedLock();auto out=std::make_shared<NativeEngineModelObject>();
	out->owner=NativeEngineModelOwner::Unit;out->id=unit.id;out->team=unit.team;
	out->modelType=unit.model->type;out->textureBin=unit.model->textureType;
	out->model=state->Model(unit.model);out->world=actualModelView;
	out->pieces=CapturePieces(unit.localModel,*out->model);out->teamColor=actualProgramTeam;return out;
}
std::shared_ptr<const NativeEngineModelObject> WorldFrameCapture::CaptureEngineFeature(const CFeature& feature,bool modern,const WorldMatrix& actualModelView,const std::array<float,4>& actualProgramTeam)
{
	auto lock=CModelsLock::GetScopedLock();auto out=std::make_shared<NativeEngineModelObject>();
	out->owner=NativeEngineModelOwner::Feature;out->id=feature.id;out->team=feature.team;
	out->modelType=feature.model->type;out->textureBin=feature.model->textureType;
	out->model=state->Model(feature.model);out->world=actualModelView;
	out->pieces=CapturePieces(feature.localModel,*out->model);out->teamColor=actualProgramTeam;return out;
}
std::shared_ptr<const NativeEngineModelObject> WorldFrameCapture::CaptureEngineStatic(
	const S3DModel* model,const WorldMatrix& matrix,int definition,int team,bool modern,const std::array<float,4>& actualProgramTeam)
{
	auto lock=CModelsLock::GetScopedLock();auto out=std::make_shared<NativeEngineModelObject>();
	if(!model)throw content_error("Original temporary unit has no actual static model");
	out->owner=NativeEngineModelOwner::TemporaryUnit;out->id=definition;out->team=team;
	out->modelType=model->type;out->textureBin=model->textureType;out->world=matrix;out->model=state->Model(model);
	out->teamColor=actualProgramTeam;
	for(const auto& piece:out->model->pieces)out->pieces.push_back({piece.bindPose,true});return out;
}
} // namespace rendering
