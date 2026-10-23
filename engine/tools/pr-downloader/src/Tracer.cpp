// SPDX-FileCopyrightText: 2022 Marek Rusinowski
// SPDX-License-Identifier: MIT OR GPL-2.0-or-later OR Apache-2.0

#include "Tracer.h"

#ifndef NDEBUG

#include <cassert>
#include <chrono>
#include <cstdio>

thread_local TracerContext* TracerContext::global_context = nullptr;

TracerContext::TracerContext(bool enable_)
	: enable(enable_)
	, full_start(std::chrono::high_resolution_clock::now())
{
	assert(TracerContext::global_context == nullptr);
	if (!enable) {
		return;
	}
	TracerContext::global_context = this;
	printTime(full_start);
	printf("Started tracking\n");
}

TracerContext::~TracerContext()
{
	if (!enable) {
		return;
	}
	printTime(std::chrono::high_resolution_clock::now());
	printf("Stopped tracing\n");
	TracerContext::global_context = nullptr;
}

void TracerContext::printTime(std::chrono::high_resolution_clock::time_point t)
{
	printf("# %8.2lfms ", std::chrono::duration<double, std::milli>(t - full_start).count());
}

void TracerContext::formatIndent(char* buf)
{
	for (unsigned int i = 0; i < 2 * TracerContext::global_context->indent; ++i) {
		*(buf++) = ' ';
	}
	*buf = '\0';
}

void TracerContext::formatPrefix(char* buf, const char* filename, int line, const char* funcname,
                                 const char* tag)
{
	buf += sprintf(buf, "%s:%d:%s()", filename, line, funcname);
	if (tag != nullptr) {
		buf += sprintf(buf, ":%s", tag);
	}
}

ScopeTrace::ScopeTrace(const char* filename, int line, const char* funcname, const char* tag)
	: start(std::chrono::high_resolution_clock::now())
{
	if (TracerContext::global_context == nullptr) {
		return;
	}
	TracerContext::global_context->formatIndent(indent);
	TracerContext::global_context->formatPrefix(prefix, filename, line, funcname, tag);
	TracerContext::global_context->printTime(start);
	printf("%s->%s Start\n", indent, prefix);
	++TracerContext::global_context->indent;
}

ScopeTrace::~ScopeTrace()
{
	if (TracerContext::global_context == nullptr) {
		return;
	}
	--TracerContext::global_context->indent;
	auto end = std::chrono::high_resolution_clock::now();
	TracerContext::global_context->printTime(end);
	printf("%s<-%s End (%lfms)\n", indent, prefix,
	       std::chrono::duration<double, std::milli>(end - start).count());
}

void tracePoint(const char* filename, int line, const char* funcname, const char* tag)
{
	if (TracerContext::global_context == nullptr) {
		return;
	}
	char indent[32], prefix[512];
	TracerContext::global_context->formatIndent(indent);
	TracerContext::global_context->formatPrefix(prefix, filename, line, funcname, tag);
	TracerContext::global_context->printTime(std::chrono::high_resolution_clock::now());
	printf("%s%s\n", indent, prefix);
}

#endif
