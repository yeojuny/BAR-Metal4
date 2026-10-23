/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "Metal4AssetController.h"
#include "Rendering/Backend/AssetDiagnosticTypes.h"
#include "Rendering/Metal4/Metal4AssetRenderer.h"
#include "Rendering/Models/S3OParser.h"
#include "Rendering/Models/3DModel.hpp"
#include "Rendering/Textures/Bitmap.h"
#include "Rendering/Textures/S3OTextureHandler.h"
#include "Rendering/Textures/nv_dds.h"
#include "System/FileSystem/FileHandler.h"
#include "System/FileSystem/FileSystemInitializer.h"
#include "System/Log/ILog.h"
#include "System/Platform/Watchdog.h"
#include "System/SpringMath.h"
#include "System/Threading/ThreadPool.h"

#include <json/json.h>
#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

std::array<float, 3> Components(const float3& value)
{
	return {value.x, value.y, value.z};
}

void WriteU32(std::ostream& output, uint32_t value)
{
	const char bytes[] = {char(value), char(value >> 8), char(value >> 16), char(value >> 24)};
	output.write(bytes, sizeof(bytes));
}

void WriteFloat(std::ostream& output, float value)
{
	WriteU32(output, std::bit_cast<uint32_t>(value));
}

void WriteFloat3(std::ostream& output, const float3& value)
{
	for (float component: Components(value))
		WriteFloat(output, component);
}

struct CpuState {
	CpuState() { CBitmap::InitPool(0); parser.Init(); }
	~CpuState() {
		parser.Kill();
		// No texture upload or textureHandler.Init occurs in this path: its
		// GPU texture vector is empty, and Kill only releases cached CPU data.
		textureHandlerS3O.Kill();
		CBitmap::KillPool();
	}
	CS3OParser parser;
};

} // namespace

int RunMetal4AssetController(const std::string& assetDirectory,
	const std::string& outputDirectory, const std::string& materialSource,
	uint32_t width, uint32_t height)
{
	Json::Value report;
	report["schema"] = "recoil-engine-metal4-asset-v1";
	report["simulation_present"] = false;
	report["gl_context_created"] = false;
	report["scope"] = "Engine asset diagnostic; gameplay, BAR visual parity and application acceptance pending";
	const auto assets = std::filesystem::absolute(assetDirectory);
	const auto output = std::filesystem::absolute(outputDirectory);
	std::filesystem::create_directories(output);
	int status = 1;
	// FileSystemInitializer registers its loading thread with the watchdog;
	// mirror the normal engine's order before entering that shared subsystem.
	Watchdog::Install();
	Watchdog::RegisterThread(WDT_MAIN, true);
	try {
		SpringMath::Init();
		ThreadPool::SetMaximumThreadCount();
		if (!FileSystemInitializer::Initialize())
			throw std::runtime_error("Engine filesystem initialization failed");
		CpuState cpu;
		S3DModel model;
		cpu.parser.Load(model, (assets / "armcom.s3o").string());
		if (model.texs[0] != "Arm_color.dds" || model.texs[1] != "Arm_other.dds")
			throw std::runtime_error("Commander texture references differ from original staged assets");

		std::vector<rendering::AssetDiagnosticVertex> vertices;
		std::vector<uint32_t> indices;
		std::ofstream geometry(output / "engine-geometry.bin", std::ios::binary);
		geometry.exceptions(std::ios::badbit | std::ios::failbit);
		geometry.write("S3OE", 4);
		WriteU32(geometry, 1); // Explicit little-endian CPU output schema.
		WriteU32(geometry, model.numPieces);
		for (const S3DModelPiece* piece: model.pieceObjects) {
			const uint32_t firstVertex = static_cast<uint32_t>(vertices.size());
			WriteU32(geometry, static_cast<uint32_t>(piece->name.size()));
			geometry.write(piece->name.data(), piece->name.size());
			WriteFloat3(geometry, piece->offset);
			WriteFloat3(geometry, piece->goffset);
			WriteU32(geometry, static_cast<uint32_t>(piece->GetVerticesVec().size()));
			WriteU32(geometry, static_cast<uint32_t>(piece->GetIndicesVec().size()));
			for (const SVertexData& vertex: piece->GetVerticesVec()) {
				vertices.push_back({Components(vertex.pos + piece->goffset), Components(vertex.normal),
					{vertex.texCoords[0].x, vertex.texCoords[0].y}});
				WriteFloat3(geometry, vertex.pos);
				WriteFloat3(geometry, vertex.normal);
				WriteFloat3(geometry, vertex.sTangent);
				WriteFloat3(geometry, vertex.tTangent);
				for (const auto& uv: vertex.texCoords) {
					WriteFloat(geometry, uv.x);
					WriteFloat(geometry, uv.y);
				}
			}
			for (uint32_t index: piece->GetIndicesVec()) {
				indices.push_back(firstVertex + index);
				WriteU32(geometry, index);
			}
		}
		geometry.close();

		std::array<nv_dds::CDDSImage, 2> images;
		std::array<std::vector<rendering::AssetTextureMip>, 2> mips;
		rendering::AssetDiagnosticRequest request;
		for (size_t texture = 0; texture < images.size(); ++texture) {
			auto& image = images[texture];
			// Preserve original DDS rows; the native shader explicitly samples
			// (u, 1-v), matching the engine's unit-texture flip at upload.
			if (!image.load((assets / model.texs[texture]).string(), false) ||
				!image.is_compressed() || image.get_format() != 0x83F3 ||
				image.get_type() != nv_dds::TextureFlat)
				throw std::runtime_error("Original commander texture must be a flat BC3 DDS");
			auto addMip = [&](uint32_t w, uint32_t h, const uint8_t* bytes, size_t size) {
				mips[texture].push_back({w, h, {bytes, size}, size_t((w + 3) / 4) * 16});
			};
			addMip(image.get_width(), image.get_height(), image, image.get_size());
			for (uint32_t level = 0; level < image.get_num_mipmaps(); ++level) {
				const auto& mip = image.get_mipmap(level);
				addMip(mip.get_width(), mip.get_height(), mip, mip.get_size());
			}
			request.textures[texture] = {rendering::AssetTextureFormat::BC3RGBA, mips[texture]};
		}
		CFileHandler shader(materialSource);
		if (!shader.FileExists() || !shader.LoadStringData(request.materialSemanticsSource))
			throw std::runtime_error("Could not read pinned BAR material semantics");
		request.vertices = vertices;
		request.indices = indices;
		request.minimum = Components(model.mins);
		request.maximum = Components(model.maxs);
		request.width = width;
		request.height = height;
		request.measuredFrames = 4;
		const auto result = rendering::RenderMetal4Asset(request);
		// The headless CBitmap row-flip method is a no-op. Prepare bottom-up
		// pixels explicitly for its DevIL PNG writer; GPU output stays top-down.
		const size_t rowBytes = size_t(result.width) * 4;
		std::vector<uint8_t> pngPixels(result.rgba.size());
		for (size_t row = 0; row < result.height; ++row)
			std::copy_n(result.rgba.data() + row * rowBytes, rowBytes,
				pngPixels.data() + (result.height - row - 1) * rowBytes);
		CBitmap image(pngPixels.data(), result.width, result.height, 4, 0x1401);
		if (!image.Save((output / "engine-commander.png").string(), false, true))
			throw std::runtime_error("Could not write engine diagnostic image");
		report["status"] = "engine_asset_rendered";
		report["device"] = result.capabilities.deviceName;
		report["direct_metal4"] = result.capabilities.directMetal4;
		report["queue_class"] = result.queueClass;
		report["command_buffer_class"] = result.commandBufferClass;
		report["render_encoder_class"] = result.renderEncoderClass;
		report["piece_count"] = model.numPieces;
		report["vertex_count"] = Json::UInt64(vertices.size());
		report["index_count"] = Json::UInt64(indices.size());
		report["width"] = width;
		report["height"] = height;
		report["retina_multiplier"] = 1;
		report["cpu_geometry"] = "CS3OParser with engine normal/tangent/bounds preparation; S3OE v1 binary output";
		report["cpu_textures"] = "Engine nv_dds loader; original BC3 mip blocks, unflipped rows, shader V flip";
		report["submitted_frames"] = Json::UInt64(result.frames.size());
		report["warmup_frames"] = 1;
		report["measured_frames"] = request.measuredFrames;
		status = 0;
	} catch (const std::exception& error) {
		report["status"] = "failed";
		report["error"] = error.what();
		LOG_L(L_ERROR, "[Metal4AssetController] %s", error.what());
	}
	FileSystemInitializer::Cleanup(false);
	ThreadPool::SetThreadCount(0);
	Watchdog::DeregisterThread(WDT_MAIN);
	Watchdog::Uninstall();
	std::ofstream reportFile(output / "engine-asset-report.json");
	reportFile << report << '\n';
	return reportFile.good() ? status : 1;
}
