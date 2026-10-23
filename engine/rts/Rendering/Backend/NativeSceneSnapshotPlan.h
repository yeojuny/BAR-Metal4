/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rendering::scene {

struct SnapshotRequest {
	std::filesystem::path path;
	uint64_t minimumFrame = 150;
};

// Optional diagnostics only. Parse once on attachment; ordinary frames do not
// read files or allocate readback storage. Multiple requested frames retain the
// original renderer, camera and presentation rather than creating a test scene.
inline std::vector<SnapshotRequest> MakeSnapshotPlan(const char* output, const char* minimumFrame, const char* frameList, const char* sessionDirectory = nullptr)
{
	if (sessionDirectory != nullptr && sessionDirectory[0] != '\0') {
		if (output != nullptr && output[0] != '\0')
			throw std::runtime_error("Native scene snapshots require either a fixed file or a session directory");
		const std::filesystem::path root(sessionDirectory);
		if (!root.is_absolute() || !std::filesystem::is_directory(root))
			throw std::runtime_error("Native scene snapshot session directory must exist and be absolute");
		// A lobby passes its environment to every engine child. Reserve a new
		// namespace atomically for each attachment, including process restarts.
		// Existing PNGs/metadata remain immutable; fixed-file diagnostics below
		// deliberately retain their strict no-overwrite contract.
		for (uint64_t number = 1; number <= 1'000'000; ++number) {
			const auto directory = root / ("session-" + std::to_string(number));
			if (!std::filesystem::create_directory(directory))
				continue;
			try {
				const auto path = (directory / "world.png").string();
				return MakeSnapshotPlan(path.c_str(), minimumFrame, frameList);
			} catch (...) {
				std::error_code cleanupError;
				std::filesystem::remove(directory, cleanupError);
				throw;
			}
		}
		throw std::runtime_error("Native scene snapshot session directory is exhausted");
	}
	if (output == nullptr || output[0] == '\0')
		return {};
	const std::filesystem::path base(output);
	if (!base.is_absolute() || base.extension() != ".png" || !std::filesystem::is_directory(base.parent_path()))
		throw std::runtime_error("Native scene snapshot requires an absolute .png path in an existing directory");
	const auto parseFrame = [](std::string_view value) {
		uint64_t frame = 0;
		const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), frame);
		if (value.empty() || error != std::errc{} || end != value.data() + value.size() || frame > 1'000'000)
			throw std::runtime_error("Native scene snapshot frame must be an integer from 0 to 1000000");
		return frame;
	};
	std::vector<SnapshotRequest> requests;
	if (frameList == nullptr) {
		requests.push_back({base, minimumFrame == nullptr ? 150 : parseFrame(minimumFrame)});
	} else {
		if (minimumFrame != nullptr)
			throw std::runtime_error("Native scene snapshot accepts either one minimum frame or an explicit frame list");
		std::string_view remaining(frameList);
		for (;;) {
			const auto separator = remaining.find(',');
			const uint64_t frame = parseFrame(remaining.substr(0, separator));
			if ((!requests.empty() && frame <= requests.back().minimumFrame) || requests.size() == 32)
				throw std::runtime_error("Native scene snapshot list requires 1 to 32 strictly increasing frames");
			const auto name = base.stem().string() + "-frame-" + std::to_string(frame) + ".png";
			requests.push_back({base.parent_path() / name, frame});
			if (separator == std::string_view::npos)
				break;
			remaining.remove_prefix(separator + 1);
		}
	}
	for (const auto& request: requests) {
		if (std::filesystem::exists(request.path) || std::filesystem::exists(request.path.string() + ".json"))
			throw std::runtime_error("Native scene snapshot output already exists");
	}
	return requests;
}

} // namespace rendering::scene
