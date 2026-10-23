// SPDX-FileCopyrightText: 2022 Marek Rusinowski
// SPDX-License-Identifier: MIT OR GPL-2.0-or-later OR Apache-2.0

/**
 * A tracer to plot the timeline of program execution with manually injected trace points using
 * TRACE and TRACEP macros. Currently it just outputs trace to stdout, but would be nice if instead
 * it supported exportin in the format compatible with chrome://trace to have a better UI.
 */

#pragma once

#ifdef NDEBUG

class TracerContext
{
public:
	TracerContext(bool)
	{
	}
};
#define TRACE(...)
#define TRACEP(tag)

#else

#include <chrono>

class ScopeTrace;
void tracePoint(const char* filename, int line, const char* funcname, const char* tag);

class TracerContext
{
public:
	TracerContext(bool enable = true);
	~TracerContext();

private:
	friend class ScopeTrace;
	friend void tracePoint(const char* filename, int line, const char* funcname, const char* tag);

	void printTime(std::chrono::high_resolution_clock::time_point t);
	void formatIndent(char* buf);
	void formatPrefix(char* buf, const char* filename, int line, const char* funcname,
	                  const char* tag);

	unsigned int indent = 0;
	const bool enable;
	const std::chrono::high_resolution_clock::time_point full_start;
	static thread_local TracerContext* global_context;
};

class ScopeTrace
{
public:
	ScopeTrace(const char* filename, int line, const char* funcname, const char* tag = nullptr);
	~ScopeTrace();

private:
	char indent[32], prefix[512];
	const std::chrono::high_resolution_clock::time_point start;
};

#define TRACE_NAME(x, y) x##y
#define TRACE_NAME2(x, y) TRACE_NAME(x, y)
#ifdef _MSC_VER
#define TRACE(...) \
	ScopeTrace TRACE_NAME2(_tracer, __COUNTER__)(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#else
#define TRACE(...)                                                   \
	ScopeTrace TRACE_NAME2(_tracer, __COUNTER__)(__FILE__, __LINE__, \
	                                             __FUNCTION__ __VA_OPT__(, ) __VA_ARGS__)
#endif


#define TRACEP(tag) tracePoint(__FILE__, __LINE__, __FUNCTION__, tag)


#endif
