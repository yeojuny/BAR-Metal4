#include <cstddef>
#include <cstdio>
#include <unordered_map>
#define BOOST_TEST_MODULE Float3
#include <algorithm>
#include <boost/test/unit_test.hpp>
#include <memory>
#include <optional>
#include <random>
#include <string>

#include "Downloader/Http/IOThreadPool.h"
#include "FileSystem/FileSystem.h"
#include "FileSystem/HashGzip.h"
#include "FileSystem/HashMD5.h"
#include "Util.h"

BOOST_AUTO_TEST_CASE(EscapeFilenameTest)
{
	BOOST_CHECK("_____" == CFileSystem::EscapeFilename("/<|>/"));
	BOOST_CHECK("_____" == CFileSystem::EscapeFilename("/<|>\\"));
	BOOST_CHECK("abC123" == CFileSystem::EscapeFilename("abC123"));
}

BOOST_AUTO_TEST_CASE(HashGzipTest)
{
	// Uncompressed output is:
	//   b'a'*100000 + b'cnoi1au4enfygw7oem9xbm3xoMBV9wmXOwuodawdca'
	constexpr int input_size = 178 + 10;
	unsigned char input[input_size] = {
		31, 139, 8, 0, 176, 70, 139, 98, 2, 255, 237, 193, 61, 14, 69, 64, 24, 0, 192, 51, 9, 137,
		104, 245, 162, 19, 237, 231, 231, 189, 40, 214, 86, 178, 220, 222, 53, 20, 51, 19, 1, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 124, 213, 122, 230, 163, 138, 171, 217, 207, 223, 243, 47, 109, 222, 83, 119, 47, 169,
		190, 243, 208, 79, 93, 73, 243, 88, 174, 188, 69, 217, 214, 120, 1, 52, 234, 240, 146, 202,
		134, 1, 0,
		// We add and allow some additional data past stream end as it's
	    // supposed to be ignored by zlib.
		23, 12, 43, 12, 41, 0, 0, 4, 2, 4};
	std::string input_hash = "9a3b049769e61efaced3b73adb6fc43e";

	HashMD5 md5_hash;
	BOOST_CHECK(md5_hash.IHash::Set(input_hash));

	HashGzip gzip_hash(std::make_unique<HashMD5>());
	gzip_hash.Init();
	for (int i = 0; i < input_size; ++i) {
		gzip_hash.Update(reinterpret_cast<char*>(&input[i]), 1);
	}
	gzip_hash.Final();
	BOOST_CHECK(gzip_hash.compare(&md5_hash));

	// When we corrupt input, we expect FFFF... hash.
	input[100] = 20;
	input[101] = 13;
	gzip_hash.Init();
	gzip_hash.Update(reinterpret_cast<char*>(input), input_size);
	gzip_hash.Final();
	BOOST_CHECK(gzip_hash.get(0) == 255);
}

BOOST_AUTO_TEST_CASE(IOThreadPoolTest)
{
	constexpr int handlersCount = 1000;
	constexpr int accCount = 1000;
	constexpr int modRetEval = 3;
	std::vector<int> counters(handlersCount);
	std::vector<int> queue(handlersCount * accCount);
	int total_ret_counter = 0;
	for (int h = 0; h < handlersCount; ++h) {
		for (int c = 0; c < accCount; ++c) {
			queue[h * accCount + c] = h;
		}
	}

	std::default_random_engine gen(std::random_device{}());
	std::shuffle(queue.begin(), queue.end(), gen);

	IOThreadPool pool(8, 10);
	std::vector<IOThreadPool::Handle> handlers;
	handlers.reserve(handlersCount);
	for (int i = 0; i < handlersCount; ++i) {
		handlers.emplace_back(pool.getHandle());
	}
	for (size_t i = 0; i < queue.size(); ++i) {
		int val = queue[i];
		handlers[val].submit(
			[&counters, &total_ret_counter, val, modRetEval]() -> IOThreadPool::OptRetF {
				counters[val] += 1;
				if (val % modRetEval == 0) {
					return [&total_ret_counter]() { total_ret_counter += 1; };
				}
				return std::nullopt;
			});
		if (i % handlersCount) {
			pool.pullResults();
		}
	}
	pool.finish();
	constexpr int expexted_total_ret_countet =
		accCount * ((handlersCount + modRetEval - 1) / modRetEval);
	BOOST_CHECK(total_ret_counter == expexted_total_ret_countet);
	for (int i = 0; i < handlersCount; ++i) {
		;
		BOOST_CHECK(counters[i] == accCount);
	}
}

BOOST_AUTO_TEST_CASE(ParseArgumentsTest)
{
	using ArgsT = std::unordered_map<std::string, std::vector<std::string>>;
	using PosT = std::vector<std::string>;
	const std::unordered_map<std::string, bool> opts = {
		{"opt1", true},
		{"opt2", true},
		{"flag1", false},
		{"flag2", false},
	};

	// Correct usage
	{
		constexpr int argc = 13;
		const char* argv[argc] = {"prd",     "--opt1", "value",   "pos1",   "--opt2=value=asd=asd",
		                          "--flag1", "pos2",   "--flag2", "--opt1", "--value2",
		                          "--opt2",  "value2", "--flag1"};
		const auto [args, positional] = parseArguments(argc, const_cast<char**>(argv), opts);
		const ArgsT expected_args = {
			{"opt1", {"value", "--value2"}},
			{"opt2", {"value=asd=asd", "value2"}},
			{"flag1", {}},
			{"flag2", {}},
		};
		const PosT expected_positional = {"pos1", "pos2"};
		BOOST_CHECK(args == expected_args);
		BOOST_CHECK(positional == expected_positional);
	}

	// Invalid option
	{
		constexpr int argc = 3;
		const char* argv[argc] = {"prd", "--opt_unknown", "asd"};
		BOOST_CHECK_THROW(parseArguments(argc, const_cast<char**>(argv), opts), ArgumentParseEx);
	}

	// Taking value when it shouldn't
	{
		constexpr int argc = 2;
		const char* argv[argc] = {"prd", "--flag1=asdasd"};
		BOOST_CHECK_THROW(parseArguments(argc, const_cast<char**>(argv), opts), ArgumentParseEx);
	}

	// No value for option
	{
		constexpr int argc = 2;
		const char* argv[argc] = {"prd", "--opt1"};
		BOOST_CHECK_THROW(parseArguments(argc, const_cast<char**>(argv), opts), ArgumentParseEx);
	}
}
