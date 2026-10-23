/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#include "Logger.h"

#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <cstddef>
#include <cstdio>

// Logging functions in standalone mode
// prdLogRaw is supposed to flush after printing (mostly to stdout/err
// for progress bars and such).
static void prdLogRaw(const char* /*fileName*/, int /*line*/, const char* /*funcName*/,
                      const char* format, va_list args)
{
	vprintf(format, args);
	fflush(stdout);
}

// Normal logging
static void prdLogError(const char* fileName, int line, const char* funcName, const char* format,
                        va_list args)
{
	fprintf(stderr, "[Error] %s:%d:%s():", fileName, line, funcName);
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
}

static void prdLogWarn(const char* fileName, int line, const char* funcName, const char* format,
                       va_list args)
{
	printf("[Warn] %s:%d:%s():", fileName, line, funcName);
	vprintf(format, args);
	printf("\n");
}

static void prdLogInfo(const char* fileName, int line, const char* funcName, const char* format,
                       va_list args)
{
	printf("[Info] %s:%d:%s():", fileName, line, funcName);
	vprintf(format, args);
	printf("\n");
}

static void prdLogDebug(const char* fileName, int line, const char* funcName, const char* format,
                        va_list args)
{
	printf("[Debug] %s:%d:%s():", fileName, line, funcName);
	vprintf(format, args);
	printf("\n");
}


static bool logEnabled = true;

void LOG_DISABLE(bool disableLogging)
{
	logEnabled = !disableLogging;
}

extern void L_LOG(const char* fileName, int line, const char* funName, L_LEVEL level,
                  const char* format...)
{
	if (!logEnabled) {
		return;
	}

	va_list args;
	va_start(args, format);
	switch (level) {
		case L_RAW:
			prdLogRaw(fileName, line, funName, format, args);
			break;
		default:
		case L_ERROR:
			prdLogError(fileName, line, funName, format, args);
			break;
		case L_WARN:
			prdLogWarn(fileName, line, funName, format, args);
			break;
		case L_INFO:
			prdLogInfo(fileName, line, funName, format, args);
			break;
		case L_DEBUG:
			prdLogDebug(fileName, line, funName, format, args);
			break;
	}
	va_end(args);
}

extern void LOG_PROGRESS(int64_t done, int64_t total, bool forceOutput)
{
	static std::chrono::steady_clock::time_point lastlogtime;
	static double lastPercentage = 0.0f;

	if (!logEnabled) {
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	if (now - lastlogtime > std::chrono::milliseconds(150)) {
		lastlogtime = now;
	} else {
		if (!forceOutput)
			return;
	}

	double percentage = 0;
	if (total > 0) {
		percentage = static_cast<double>(done) / static_cast<double>(total);
	}

	if (percentage == lastPercentage)
		return;
	lastPercentage = percentage;

	// In case the toal/done are incorrect, put 50%
	if (percentage < 0 || percentage > 1) {
		percentage = 0.5;
	}

	constexpr int barLen = 30;
	const char bar[barLen * 2 + 1] = "==============================                              ";
	LOG("[Progress] %3.0f%% [%.30s] %" PRIi64 "/%" PRIi64 " \r", percentage * 100.0f,
	    bar + static_cast<std::ptrdiff_t>(barLen * (1 - percentage)), done, total);
}
