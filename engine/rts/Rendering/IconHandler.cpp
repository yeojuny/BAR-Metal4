/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "IconHandler.h"

#include <algorithm>
#include <cassert>
#include <locale>
#include <cctype>
#include <cmath>
#include <map>
#include <stdexcept>
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/Backend/ScreenDrawList.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Textures/LegacyAtlasAlloc.h"
#include "System/Config/ConfigHandler.h"
#endif

#include <fmt/format.h>

#include "Rendering/GL/myGL.h"
#include "Rendering/GL/RenderBuffers.h"
#include "System/Log/ILog.h"
#include "System/UnorderedSet.hpp"
#include "System/UnorderedMap.hpp"
#include "System/StringUtil.h"
#include "Lua/LuaParser.h"
#include "Textures/Bitmap.h"
#include "Textures/TextureRenderAtlas.h"
#include "System/Exceptions.h"

namespace icon {

CIconHandler iconHandler;

/******************************************************************************/
//
//  CIconHandler
//

void CIconHandler::Kill()
{
	defaultIconIdx = INVALID_ICON_INDEX;
	drawOrderIconCount = 0;

	glDeleteTextures(2, atlasTextureIDs.data());
	atlasTextureIDs = { 0 };
	atlasTextureSizes = { int2{0, 0}, int2{0, 0} };

	atlases = { nullptr };
	atlasNeedsUpdate = { false };

	iconsMap.clear();
	iconsData.clear();
#ifdef RECOIL_METAL4_LIVE_SCENE
	nativeAtlasTextures = {};
	nativeAtlasGenerations = {};
	nativeAtlasOwners.clear();
#endif
}


void CIconHandler::DumpAtlasTextures(const std::string& fileExt) const
{
	if (atlasTextureIDs[0]) {
		for (int level = 0; level < DEFAULT_NUM_OF_TEXTURE_LEVELS; ++level) {
			glSaveTexture(atlasTextureIDs[0], fmt::format("IconsAtlas1-{}.{}", level, fileExt).c_str(), level);
		}
	}
	if (atlasTextureIDs[1]) {
		for (int level = 0; level < DEFAULT_NUM_OF_TEXTURE_LEVELS; ++level) {
			glSaveTexture(atlasTextureIDs[1], fmt::format("IconsAtlas2-{}.{}", level, fileExt).c_str(), level);
		}
	}
}

bool CIconHandler::UpdateAtlasData(size_t atlasIdx)
{
	auto& atlas = atlases[atlasIdx];
	if (atlas)
		return true;

	atlas = std::make_unique<CTextureRenderAtlas>(CTextureAtlas::ATLAS_ALLOC_LEGACY, 0, 0, DEFAULT_NUM_OF_TEXTURE_LEVELS, GL_RGBA8, IntToString(atlasIdx, "IconsAtlas_%i"));

	spring::unordered_set<std::string> invalidIcons;

	// Sort icon names to ensure deterministic ordering across runs
	std::vector<std::string> sortedIconNames;
	sortedIconNames.reserve(iconsMap.size());
	for (const auto& [iconName, _] : iconsMap) {
		sortedIconNames.push_back(iconName);
	}
	std::sort(sortedIconNames.begin(), sortedIconNames.end());

	for (const auto& iconName : sortedIconNames) {
		const auto iconIndex = iconsMap[iconName];
		const auto& iconData = iconsData[iconIndex];

		if (iconData.GetAtlasIndex() != atlasIdx)
			continue;

		if (!atlas->AddTexFromFile(iconName, iconData.GetFileName(), iconData.GetSrcTexCoords())) {
			LOG_L(L_WARNING, "[CIconHandler::%s] Failed to load icon=%s, bitmap=%s", __func__, iconName.c_str(), iconData.GetFileName().c_str());
			invalidIcons.emplace(iconName);
		}
	}

	// everything above was loaded from a single image atlas, no need to run CTextureRenderAtlas machinery
	if (auto allFiles = atlas->GetAllFileNames(); allFiles.size() == 1) {
		const auto& uniqueSubTexMap = atlas->GetUniqueSubTexMap();
		for (const auto& [iconName, uniqSubTexName] : atlas->GetNameToUniqueSubTexMap()) {
			auto it = uniqueSubTexMap.find(uniqSubTexName);
			auto& item = iconsData[*iconsMap.try_get(iconName)];
			item.SetTexCoords(AtlasedTexture{ it->second.subTexCoords, 0 });
		}
	}

	if (auto it = invalidIcons.find("default"); it != invalidIcons.end()) {
		bool res = atlas->AddTexFromFile("default", "bitmaps/defaultradardot.png");
		assert(res);
		invalidIcons.erase(it);
	}

	if (!atlas->CalculateAtlas()) {
		atlas = nullptr;
		return false;
	}

	for (const auto& [iconName, _] : atlas->GetNameToUniqueSubTexMap()) {
		auto& item = iconsData[*iconsMap.try_get(iconName)];
		item.SetTexCoords(atlas->GetTexture(iconName));
	}

	const auto& defTC = iconsData[defaultIconIdx].GetTexCoords();
	for (const auto& iconName : invalidIcons) {
		auto& item = iconsData[*iconsMap.try_get(iconName)];
		item.SetTexCoords(defTC);
	}

	return true;
}

bool CIconHandler::CreateAtlasTexture(size_t atlasIdx)
{
	auto& atlas = atlases[atlasIdx];

	if (!atlas)
		return false;

	if (auto allFiles = atlas->GetAllFileNames(); allFiles.size() == 1) {
		CBitmap bm;

		if (!bm.Load(*allFiles.begin()))
			return false;

		glDeleteTextures(1, &atlasTextureIDs[atlasIdx]);
		atlasTextureIDs[atlasIdx] = 0; // just in case
		atlasTextureIDs[atlasIdx] = bm.CreateMipMapTexture();
		atlasTextureSizes[atlasIdx] = int2(bm.xsize, bm.ysize);

		// CBitmap::CreateTexture defaults to GL_REPEAT, which is not what we want for atlas
		glBindTexture(GL_TEXTURE_2D, atlasTextureIDs[atlasIdx]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		return true;
	}

	if (!atlas->CreateAtlasTexture())
		return false;

	atlasTextureSizes[atlasIdx] = atlas->GetAtlasSize();

	if (atlasTextureIDs[atlasIdx]) {
		glDeleteTextures(1, &atlasTextureIDs[atlasIdx]);
		atlasTextureIDs[atlasIdx] = 0; // just in case
	}

	atlasTextureIDs[atlasIdx] = atlas->DisownTexture();
	atlas = nullptr;

	return true;
}

#ifdef RECOIL_METAL4_LIVE_SCENE
std::shared_ptr<const rendering::ScreenTexture> CIconHandler::GetNativeAtlasTexture(size_t index) const
{
	if (index >= nativeAtlasTextures.size())
		throw std::out_of_range("Native minimap icon atlas index exceeds original two-atlas contract");
	return nativeAtlasTextures[index];
}

bool CIconHandler::CreateNativeAtlasTexture(size_t atlasIndex)
{
	// Preserve the original deterministic legacy allocator and the exact
	// TextureRenderAtlas pixel-snapped, source-LOD0 mip rendering equations.
	struct Source {
		std::uint32_t index;
		int width, height;
		std::vector<std::uint8_t> bytes;
	};
	struct Region { std::string file; float4 uv; };
	std::map<std::string, Source> sources;
	std::map<std::string, Region> regions;
	std::map<std::string, std::string> names;
	std::vector<std::string> invalid;
	CLegacyAtlasAlloc allocator;
	const int deviceLimit = globalRendering->maxTextureSize;
	const int configLimitX = configHandler->GetInt("MaxTextureAtlasSizeX");
	const int configLimitY = configHandler->GetInt("MaxTextureAtlasSizeY");
	if (deviceLimit <= 0 || configLimitX <= 0 || configLimitY <= 0)
		throw std::runtime_error(fmt::format(
			"Native minimap icon atlas {} has invalid limits: device={}, config={}x{}",
			atlasIndex, deviceLimit, configLimitX, configLimitY));
	const int limitX = std::min(deviceLimit, configLimitX);
	const int limitY = std::min(deviceLimit, configLimitY);
	allocator.SetMaxSize(limitX, limitY);
	allocator.SetMaxTexLevel(DEFAULT_NUM_OF_TEXTURE_LEVELS);
	auto add = [&](const std::string& name, const std::string& file, float4 uv) {
		auto source = sources.find(file);
		if (source == sources.end()) {
			if (StringToLower(file).ends_with(".dds"))
				throw std::runtime_error("Native icon atlas DDS source requires its original decoder path: " + file);
			CBitmap bitmap;
			if (!bitmap.Load(file)) return false;
			if (bitmap.compressed || bitmap.channels != 4 || bitmap.dataType != GL_UNSIGNED_BYTE)
				throw std::runtime_error("Native icon atlas requires a real RGBA8 source bitmap: " + file);
			Source image{static_cast<std::uint32_t>(sources.size()), bitmap.xsize, bitmap.ysize, {}};
			const auto count = size_t(image.width) * size_t(image.height) * 4;
			image.bytes.assign(bitmap.GetRawMem(), bitmap.GetRawMem() + count);
			source = sources.emplace(file, std::move(image)).first;
		}
		const auto& image = source->second;
		if (!std::isfinite(uv.x) || !std::isfinite(uv.y) || !std::isfinite(uv.z) || !std::isfinite(uv.w) || uv.x < 0 || uv.y < 0 || uv.z > 1 || uv.w > 1 || uv.x >= uv.z || uv.y >= uv.w)
			throw std::runtime_error("Native icon atlas source rectangle is invalid");
		const std::string key = fmt::format("{};{},{},{},{}", image.index, uv.x, uv.y, uv.z, uv.w);
		if (!regions.contains(key)) {
			const int2 dimensions{static_cast<int>(image.width * (uv.z - uv.x)), static_cast<int>(image.height * (uv.w - uv.y))};
			if (dimensions.x <= 0 || dimensions.y <= 0)
				throw std::runtime_error("Native icon atlas source rectangle has no pixels");
			allocator.AddEntry(key, dimensions);
			regions.emplace(key, Region{file, uv});
		}
		names[name] = key;
		return true;
	};
	std::vector<std::string> sorted;
	for (const auto& [name, index]: iconsMap)
		if (index < nativeAtlasOwners.size() && nativeAtlasOwners[index] == atlasIndex) sorted.push_back(name);
	std::sort(sorted.begin(), sorted.end());
	for (const auto& name: sorted) {
		const auto& data = iconsData[*iconsMap.try_get(name)];
		if (!add(name, data.GetFileName(), data.GetSrcTexCoords())) {
			if (name == "default") {
				if (!add(name, "bitmaps/defaultradardot.png", float4{0,0,1,1}))
					throw std::runtime_error("Native icon atlas cannot load original default radar image");
			} else {
				invalid.push_back(name);
			}
		}
	}
	if (names.empty()) {
		nativeAtlasTextures[atlasIndex].reset();
		return true;
	}
	if (!allocator.Allocate()) {
		std::uint64_t requestedPixels = 0;
		for (const auto& [key, entry]: allocator.GetEntries())
			requestedPixels += std::uint64_t(entry.size.x) * std::uint64_t(entry.size.y);
		// A failed initial allocation cannot publish UVs or a real image. Report
		// it at production instead of silently retrying until a unit first draws.
		throw std::runtime_error(fmt::format(
			"Native minimap icon atlas {} allocation failed: deviceLimit={}, configLimit={}x{}, effectiveLimit={}x{}, icons={}, uniqueImages={}, regions={}, requestedPixels={} before padding",
			atlasIndex, deviceLimit, configLimitX, configLimitY, limitX, limitY,
			names.size(), sources.size(), regions.size(), requestedPixels));
	}
	const auto size = allocator.GetAtlasSize();
	const auto levels = allocator.GetNumTexLevels();
	auto texture = std::make_shared<rendering::ScreenTexture>();
	texture->sourceName = "original minimap icon atlas " + std::to_string(atlasIndex);
	texture->generation = nativeAtlasGenerations[atlasIndex] + 1;
	texture->width = size.x; texture->height = size.y;
	texture->format = rendering::ScreenTextureFormat::RGBA8Unorm;
	texture->flipV = false; // bytes preserve original GL atlas row/UV convention
	for (int level = 0; level < levels; ++level) {
		rendering::ScreenTextureMip mip;
		mip.width = std::max(size.x >> level, 1u); mip.height = std::max(size.y >> level, 1u);
		mip.bytesPerRow = size_t(mip.width) * 4;
		mip.bytes.resize(mip.bytesPerRow * mip.height);
		const int scale = 1 << level;
		const int halfPad = (level < levels - 1) ? allocator.GetPadding() / 2 : 0;
		for (const auto& [key, entry]: allocator.GetEntries()) {
			const auto& region = regions.at(key); const auto& image = sources.at(region.file); const auto uv = region.uv;
			const int px1 = entry.texCoords.x1, py1 = entry.texCoords.y1;
			const int px2 = entry.texCoords.x2, py2 = entry.texCoords.y2;
			const float entryWidth = px2 - px1 + 1, entryHeight = py2 - py1 + 1;
			const int x1 = std::max(int(std::floor(float(px1 - halfPad) / scale)), 0);
			const int y1 = std::max(int(std::floor(float(py1 - halfPad) / scale)), 0);
			const int x2 = std::min(int(std::ceil(float(px2 + 1 + halfPad) / scale)), int(mip.width));
			const int y2 = std::min(int(std::ceil(float(py2 + 1 + halfPad) / scale)), int(mip.height));
			for (int y = y1; y < y2; ++y) for (int x = x1; x < x2; ++x) {
				const float u = std::clamp(uv.x + ((float(x) + .5f) * scale - px1) / entryWidth * (uv.z - uv.x), uv.x, uv.z);
				const float v = std::clamp(uv.y + ((float(y) + .5f) * scale - py1) / entryHeight * (uv.w - uv.y), uv.y, uv.w);
				const float sx = u * image.width - .5f, sy = v * image.height - .5f;
				const int ix = int(std::floor(sx)), iy = int(std::floor(sy));
				const float fx = sx - ix, fy = sy - iy;
				for (size_t channel = 0; channel < 4; ++channel) {
					const auto sample = [&](int xx, int yy) {return float(image.bytes[(size_t(std::clamp(yy,0,image.height-1))*image.width+std::clamp(xx,0,image.width-1))*4+channel]);};
					const float top = sample(ix,iy) * (1-fx) + sample(ix+1,iy) * fx;
					const float bottom = sample(ix,iy+1) * (1-fx) + sample(ix+1,iy+1) * fx;
					mip.bytes[size_t(y)*mip.bytesPerRow+size_t(x)*4+channel] = static_cast<std::uint8_t>(std::clamp(std::lround(top*(1-fy)+bottom*fy),0l,255l));
				}
			}
		}
		texture->mips.push_back(std::move(mip));
	}
	// Publish complete immutable pixels and matching UVs together. Prior frames
	// retain their old owned generation through GPU retirement.
	for (const auto& [name, key]: names) {
		auto coords = allocator.GetTexCoordsEdge(key); coords.pageNum = atlasIndex;
		iconsData[*iconsMap.try_get(name)].SetTexCoords(coords);
	}
	for (const auto& name: invalid)
		iconsData[*iconsMap.try_get(name)].SetTexCoords(iconsData[defaultIconIdx].GetTexCoords());
	atlasTextureSizes[atlasIndex] = {int(size.x), int(size.y)};
	nativeAtlasGenerations[atlasIndex] = texture->generation;
	nativeAtlasTextures[atlasIndex] = std::move(texture);
	return true;
}
#endif

void CIconHandler::LoadIcons(const std::string& filename)
{
	LuaParser luaParser(filename, SPRING_VFS_MOD_BASE, SPRING_VFS_MOD_BASE);

	if (!luaParser.Execute())
		throw content_error(fmt::format("{}: {}", filename, luaParser.GetErrorLog()));

	const LuaTable iconTypes = luaParser.GetRoot();

	std::vector<std::string> iconNames;
	iconTypes.GetKeys(iconNames);

	for (const auto& iconName : iconNames) {
		const auto iconTable = iconTypes.SubTable(iconName);
		AddIcon(0,
			iconName,
			iconTable.GetString("bitmap", ""),
			iconTable.GetFloat("size", 1.0f),
			iconTable.GetFloat("distance", 1.0f),
			iconTable.GetBool("radiusAdjust", false),
			iconTable.GetFloat("u0", 0.0f),
			iconTable.GetFloat("v0", 0.0f),
			iconTable.GetFloat("u1", 1.0f),
			iconTable.GetFloat("v1", 1.0f),
			iconTable.GetFloat("drawOrder", 0.0f)
		);
	}

	atlasNeedsUpdate.set(0);

	// force Update()
	Update();
}

bool CIconHandler::AddIcon(
	uint32_t atlasIndex,
	const std::string& iconName,
	const std::string& texFileName,
	float size,
	float distance,
	bool radAdj,
	float u0, float v0, float u1, float v1,
	float drawOrder
) {
	auto it = iconsMap.find(iconName);
	if (it != iconsMap.end()) {
		FreeIcon(iconName);
	} else {
		it = iconsMap.emplace(iconName, iconsData.size()).first;
		iconsData.emplace_back();
	}

	iconsData[it->second] = IconData{
		iconName,
		texFileName,
		size,
		distance,
		atlasIndex,
		radAdj,
		float4{u0, v0, u1, v1},
		drawOrder
	};

	drawOrderIconCount += (drawOrder != 0.0f);
#ifdef RECOIL_METAL4_LIVE_SCENE
	if (atlasIndex >= nativeAtlasTextures.size()) throw std::out_of_range("Native icon atlas index");
	if (nativeAtlasOwners.size() < iconsData.size()) nativeAtlasOwners.resize(iconsData.size());
	nativeAtlasOwners[it->second] = atlasIndex;
	atlasNeedsUpdate.set(atlasIndex);
#endif

	// do basic check instead of the full load procedure
	return CFileHandler::FileExists(texFileName, SPRING_VFS_ALL);
}


bool CIconHandler::FreeIcon(const std::string& iconName)
{
	if (iconName == "default")
		return false;

	auto it = iconsMap.find(iconName);

	if (it == iconsMap.end())
		return false;

	if (const auto& ai = iconsData[it->second].GetAtlasIndex(); ai > 0) {
		atlasNeedsUpdate.set(ai);
	}

	drawOrderIconCount -= (iconsData[it->second].GetDrawOrder() != 0.0f);

	iconsData[it->second] = IconData{};
	it->second = defaultIconIdx;

	return true;
}

void CIconHandler::Update()
{
	if (atlasNeedsUpdate.none())
		return;

	auto defIt = iconsMap.find("default");
	if (defIt == iconsMap.end()) {
		defIt = iconsMap.emplace("default", iconsData.size()).first;
		iconsData.emplace_back(
			"default",
			"bitmaps/defaultradardot.png",
			1.0f,
			1.0f,
			0,
			false,
			float4{
				0.0f,
				0.0f,
				1.0f,
				1.0f
			}
		);
	}

	defaultIconIdx = defIt->second;
#ifdef RECOIL_METAL4_LIVE_SCENE
	if (nativeAtlasOwners.size() < iconsData.size()) nativeAtlasOwners.resize(iconsData.size());
	for (size_t index = 0; index < atlasNeedsUpdate.size(); ++index)
		if (atlasNeedsUpdate.test(index) && CreateNativeAtlasTexture(index)) atlasNeedsUpdate.reset(index);
	return;
#endif

#ifdef HEADLESS
	// Headless builds don't render icons; skip the atlas pipeline entirely.
	// Clearing the bits makes the atlasNeedsUpdate.none() check above short-circuit
	// all subsequent calls to Update().
	atlasNeedsUpdate.reset();
	return;
#endif

	for (size_t i = 0; i < atlasNeedsUpdate.size(); ++i) {
		if (!atlasNeedsUpdate.test(i))
			continue;

		if (UpdateAtlasData(i) && CreateAtlasTexture(i))
			atlasNeedsUpdate.reset(i);
	}
}

std::pair<bool, spring::unordered_map<std::string, size_t>::const_iterator> CIconHandler::FindIconIdx(const std::string& iconName) const
{
	const auto it = iconsMap.find(iconName);
	return std::make_pair(it != iconsMap.end(), it);
}

const IconData& CIconHandler::GetIconData(const std::string& iconName) const
{
	const auto* ptrIconIdx = iconsMap.try_get(iconName);
	assert(ptrIconIdx != nullptr && (*ptrIconIdx) < iconsData.size());
	return iconsData[*ptrIconIdx];
}

const IconData& CIconHandler::GetIconData(size_t iconIdx) const
{
	assert(iconIdx < iconsData.size());
	return iconsData[iconIdx];
}

size_t CIconHandler::GetIconIdx(const std::string& iconName) const
{
	if (const auto& [found, it] = FindIconIdx(iconName); found)
		return it->second;
	else
		return INVALID_ICON_INDEX;
}

size_t CIconHandler::GetIconIdxOrDefault(const std::string& iconName) const
{
	if (const auto& [found, it] = FindIconIdx(iconName); found)
		return it->second;
	else
		return defaultIconIdx;
}

}
