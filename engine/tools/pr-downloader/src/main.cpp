/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */
#include <array>
#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "Downloader/DownloadEnum.h"
#include "Logger.h"
#include "Tracer.h"
#include "Util.h"
#include "Version.h"
#include "pr-downloader.h"

void show_version()
{
	LOG("pr-downloader %s (%s)\n", getVersion(), platformToString(PRD_CURRENT_PLATFORM));
}

const static std::array<std::tuple<std::string, bool, std::string>, 13> opts_array = {{
	{"help", false, "Print this help message"},
	{"version", false, "Show version of pr-downloader and quit"},
	{"filesystem-writepath", true, "Set the directory with data, defaults to current dir"},
	{"download-game", true,
     "Download games by name or rapid tag, eg. 'GG 1.2', 'gg:test', 'rapid://gg:test'"},
	{"rapid-download", true, "Alias to --download-game"},
	{"download-map", true, "Download maps by name"},
	{"download-engine", true, "Download engines by version"},
	{"rapid-validate", false, "Validates correctness of files in rapid pool"},
	{"delete", false, "Delete invalid files when executing --rapid-validate"},
	{"validate-sdp", true,
     "Validate correctness of files in Sdp archive, takes full path to the Sdp file"},
	{"dump-sdp", true, "Dump contents of Sdp file, takes full path to the Sdp file"},
	{"disable-logging", false, "Disables logging"},
	{"disable-fetch-depends", false, "Disables downloading of dependend archives"},
}};

void show_help(const char* cmd)
{
	std::cout << "Usage: " << cmd << "\nOptions:\n";
	for (auto const& [arg, has_val, help] : opts_array) {
		std::cout << " --" << arg;
		if (has_val) {
			std::cout << " <value>";
		}
		std::cout << "\n      " << help << "\n";
	}
	std::cout << R"env(
All --download-* flags can be specified multiple times which will download multiple
assets with a single invocation.

Environment variables:
  PRD_RAPID_USE_STREAMER=[true]|false
      Whatever to use streamer.cgi for downloading.
  PRD_RAPID_REPO_MASTER=[https://repos.springrts.com/repos.gz]
      URL of the rapid repo master.
  PRD_MAX_HTTP_REQS_PER_SEC=[0]
      Limit on number of requests per second for HTTP downloading, 0 = unlimited
  PRD_HTTP_SEARCH_URL=[https://springfiles.springrts.com/json.php]
      URL of springfiles used to download maps etc.
  PRD_DISABLE_CERT_CHECK=[false]|true
      Allows to disable TLS certificate validation, useful for testing.
)env";

#ifndef NDEBUG
	std::cout << R"env(  PRD_ENABLE_TRACING=[false]|true
      Enable execution tracing.
)env";
#endif
}

void show_results(int count)
{
	for (int i = 0; i < count; i++) {
		downloadInfo dl;
		DownloadGetInfo(i, dl);
		LOG_DEBUG("Download path: %s", dl.filename);
	}
}

int main(int argc, char** argv)
try {
	const char* enable_tracing_env = std::getenv("PRD_ENABLE_TRACING");
	TracerContext tracer_context(enable_tracing_env != nullptr &&
	                             std::string(enable_tracing_env) == "true");

	ensureUtf8Argv(&argc, &argv);
	show_version();
	if (argc < 2) {
		show_help(argv[0]);
		return 1;
	}

	std::unordered_map<std::string, bool> opts;
	for (const auto& [flag, has_val, help] : opts_array) {
		opts.emplace(flag, has_val);
	}
	const auto [args, positional] = parseArguments(argc, argv, opts);

	if (args.count("help")) {
		show_help(argv[0]);
		return 0;
	} else if (args.count("version")) {
		return 0;
	}
	if (args.count("disable-logging")) {
		DownloadDisableLogging(true);
	}
	if (args.count("filesystem-writepath")) {
		DownloadSetConfig(CONFIG_FILESYSTEM_WRITEPATH,
		                  args.at("filesystem-writepath").back().c_str());
	} else {
		DownloadSetConfig(CONFIG_FILESYSTEM_WRITEPATH, "");
	}
	if (args.count("disable-fetch-depends")) {
		bool fetch_depends = false;
		DownloadSetConfig(CONFIG_FETCH_DEPENDS, &fetch_depends);
	}

	if (auto it = args.find("dump-sdp"); it != args.end()) {
		if (!DownloadDumpSDP(it->second.back().c_str())) {
			LOG_ERROR("Error dumping sdp");
			return 1;
		}
		return 0;
	}

	if (auto it = args.find("validate-sdp"); it != args.end()) {
		if (!ValidateSDP(it->second.back().c_str())) {
			LOG_ERROR("Error validating SDP");
			return 1;
		}
		return 0;
	}

	if (args.count("rapid-validate")) {
		const bool removeinvalid = args.count("delete");
		if (!DownloadRapidValidate(removeinvalid)) {
			LOG_ERROR("Validation of the rapid pool failed");
			return 1;
		}
		return 0;
	}

	std::vector<DownloadSearchItem> items;
	for (auto [arg, cat] : std::array<std::pair<std::string, DownloadEnum::Category>, 4>{
			 {{"download-map", DownloadEnum::CAT_MAP},
	          {"download-game", DownloadEnum::CAT_GAME},
	          {"rapid-download", DownloadEnum::CAT_GAME},
	          {"download-engine", DownloadEnum::CAT_ENGINE}}}) {
		if (auto it = args.find(arg); it != args.end()) {
			for (auto val : it->second) {
				items.emplace_back(cat, val);
			}
		}
	}
	for (auto val : positional) {
		items.emplace_back(DownloadEnum::CAT_NONE, val);
	}

	DownloadInit();
	int count = DownloadSearch(items);
	if (count < 0) {
		DownloadShutdown();
		return 1;
	}
	bool found_all = true;
	for (auto& item : items) {
		if (!item.found) {
			found_all = false;
			LOG_ERROR("Failed to find '%s' for download", item.name.c_str());
		}
	}
	if (!found_all) {
		DownloadShutdown();
		return 1;
	}
	for (int i = 0; i < count; ++i) {
		DownloadAdd(i);
	}
	const int dlres = DownloadStart();
	if (dlres == 0) {
		show_results(count);
		LOG_INFO("Download complete!");
	} else {
		LOG_ERROR("Error occurred while downloading: %d", dlres);
	}
	DownloadShutdown();
	return dlres;
} catch (const ArgumentParseEx& ex) {
	LOG_ERROR("Failed to parse arguments: %s", ex.what());
	return 1;
}
