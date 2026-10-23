/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeSceneSnapshotPlan.h"

#include <fstream>
#include <future>
#include <iostream>
#include <set>

int main(int argc, char** argv)
{
	if (argc != 2)
		throw std::runtime_error("Expected a fresh absolute test directory");
	const std::filesystem::path directory(argv[1]);
	if (!directory.is_absolute() || std::filesystem::exists(directory))
		throw std::runtime_error("Test directory must be fresh and absolute");
	std::filesystem::create_directories(directory);
	const auto output = (directory / "world.png").string();
	size_t checks = 0;
	const auto require = [&](bool condition) {
		if (!condition)
			throw std::runtime_error("Snapshot plan test failed");
		++checks;
	};
	const auto reject = [&](const char* path, const char* frame, const char* frames) {
		bool threw = false;
		try { (void) rendering::scene::MakeSnapshotPlan(path, frame, frames); }
		catch (const std::runtime_error&) { threw = true; }
		require(threw);
	};
	using rendering::scene::MakeSnapshotPlan;
	require(MakeSnapshotPlan(nullptr, "bad", "bad").empty());
	require(MakeSnapshotPlan("", nullptr, nullptr).empty());
	const auto original = MakeSnapshotPlan(output.c_str(), nullptr, nullptr);
	require(original.size() == 1 && original[0].path == output && original[0].minimumFrame == 150);
	const auto first = MakeSnapshotPlan(output.c_str(), "0", nullptr);
	require(first.size() == 1 && first[0].minimumFrame == 0);
	require(MakeSnapshotPlan(output.c_str(), "1000000", nullptr)[0].minimumFrame == 1'000'000);
	const auto sequence = MakeSnapshotPlan(output.c_str(), nullptr, "0,300,1800");
	require(sequence.size() == 3);
	require(sequence[0].path == directory / "world-frame-0.png" && sequence[0].minimumFrame == 0);
	require(sequence[1].path == directory / "world-frame-300.png" && sequence[1].minimumFrame == 300);
	require(sequence[2].path == directory / "world-frame-1800.png" && sequence[2].minimumFrame == 1800);
	for (const char* value: {"", "-1", "+1", " 1", "1 ", "1x", "1000001", "18446744073709551616"})
		reject(output.c_str(), value, nullptr);
	for (const char* value: {"", ",", "1,", ",1", "1,,2", "1,1", "2,1", "1, 2", "0,1000001"})
		reject(output.c_str(), nullptr, value);
	reject(output.c_str(), "0", "0,1");
	reject("relative.png", nullptr, nullptr);
	const auto wrongExtension = (directory / "world.jpg").string();
	reject(wrongExtension.c_str(), nullptr, nullptr);
	const auto missingParent = (directory / "missing/world.png").string();
	reject(missingParent.c_str(), nullptr, nullptr);
	std::string maximum;
	for (unsigned frame = 0; frame < 32; ++frame)
		maximum += (frame == 0 ? "" : ",") + std::to_string(frame);
	require(MakeSnapshotPlan(output.c_str(), nullptr, maximum.c_str()).size() == 32);
	maximum += ",32";
	reject(output.c_str(), nullptr, maximum.c_str());
	{
		std::ofstream existing(directory / "world-frame-300.png.json");
		existing << "existing metadata must survive";
	}
	reject(output.c_str(), nullptr, "0,300,1800");
	std::string existingText;
	std::ifstream existingMetadata(directory / "world-frame-300.png.json");
	std::getline(existingMetadata, existingText);
	require(existingText == "existing metadata must survive");
	{
		std::ofstream existing(output);
		existing << "existing image must survive";
	}
	reject(output.c_str(), nullptr, nullptr);
	std::ifstream existingImage(output);
	std::getline(existingImage, existingText);
	require(existingText == "existing image must survive");
	const auto sessions = directory / "sessions";
	std::filesystem::create_directory(sessions);
	const auto sessionRoot = sessions.string();
	const auto firstSession = MakeSnapshotPlan(nullptr, nullptr, "0,300", sessionRoot.c_str());
	require(firstSession.size() == 2 && firstSession[0].minimumFrame == 0 && firstSession[1].minimumFrame == 300);
	{
		std::ofstream image(firstSession[0].path);
		image << "first game image";
		std::ofstream metadata(firstSession[0].path.string() + ".json");
		metadata << "first game metadata";
	}
	const auto secondSession = MakeSnapshotPlan(nullptr, nullptr, "0,300", sessionRoot.c_str());
	require(secondSession[0].path.parent_path() != firstSession[0].path.parent_path());
	require(secondSession[0].path.filename() == firstSession[0].path.filename());
	std::ifstream firstSavedImage(firstSession[0].path);
	std::getline(firstSavedImage, existingText);
	require(existingText == "first game image");
	std::ifstream firstSavedMetadata(firstSession[0].path.string() + ".json");
	std::getline(firstSavedMetadata, existingText);
	require(existingText == "first game metadata");
	std::vector<std::future<std::vector<rendering::scene::SnapshotRequest>>> concurrent;
	for (unsigned index = 0; index < 32; ++index) {
		concurrent.push_back(std::async(std::launch::async, [&] {
			return MakeSnapshotPlan(nullptr, "0", nullptr, sessionRoot.c_str());
		}));
	}
	std::set<std::filesystem::path> reserved{firstSession[0].path.parent_path(), secondSession[0].path.parent_path()};
	for (auto& future: concurrent) {
		const auto plan = future.get();
		require(plan.size() == 1 && plan[0].minimumFrame == 0);
		require(reserved.insert(plan[0].path.parent_path()).second);
	}
	const auto directoryCount = [&] {
		return std::distance(std::filesystem::directory_iterator(sessions), std::filesystem::directory_iterator());
	};
	const auto beforeInvalid = directoryCount();
	const auto rejectSession = [&](const char* file, const char* frame, const char* list, const char* root) {
		bool threw = false;
		try { (void) MakeSnapshotPlan(file, frame, list, root); }
		catch (const std::runtime_error&) { threw = true; }
		require(threw);
	};
	rejectSession(output.c_str(), nullptr, nullptr, sessionRoot.c_str());
	rejectSession(nullptr, nullptr, nullptr, "relative");
	rejectSession(nullptr, "bad", nullptr, sessionRoot.c_str());
	rejectSession(nullptr, nullptr, "1,0", sessionRoot.c_str());
	rejectSession(nullptr, "1", "1,2", sessionRoot.c_str());
	require(directoryCount() == beforeInvalid);
	std::cout << "PASS " << checks << " independent snapshot-plan checks\n";
}
