/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "System/Platform/CpuTopology.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <sys/sysctl.h>

namespace cpu_topology {
namespace {

constexpr uint32_t MAX_CPUS = std::numeric_limits<uint32_t>::digits;

struct CoreType {
	uint32_t logicalCount = 0;
	uint32_t physicalCount = 0;
	bool efficiency = false;
	std::string sysctlPrefix;
};

struct Topology {
	ProcessorMasks masks;
	ProcessorCaches caches;
};

uint32_t ReadCpuCount(const char* name)
{
	int32_t value = 0;
	size_t size = sizeof(value);
	if (sysctlbyname(name, &value, &size, nullptr, 0) != 0 || size != sizeof(value) || value <= 0)
		return 0;
	return static_cast<uint32_t>(value);
}

uint32_t ReadCacheSize(const std::string& name)
{
	// Global byte counts are 64-bit; perflevel cache sizes can be 32-bit.
	// Absent caches are optional.
	unsigned char bytes[sizeof(uint64_t)] = {};
	size_t size = sizeof(bytes);
	if (sysctlbyname(name.c_str(), bytes, &size, nullptr, 0) != 0)
		return 0;
	uint64_t value = 0;
	if (size == sizeof(uint64_t)) {
		std::memcpy(&value, bytes, sizeof(value));
	} else if (size == sizeof(uint32_t)) {
		uint32_t value32 = 0;
		std::memcpy(&value32, bytes, sizeof(value32));
		value = value32;
	} else {
		return 0;
	}
	return static_cast<uint32_t>(std::min(value, uint64_t(std::numeric_limits<uint32_t>::max())));
}

bool IsEfficiencyLevel(const std::string& prefix)
{
	char name[64] = {};
	size_t size = sizeof(name);
	if (sysctlbyname((prefix + "name").c_str(), name, &size, nullptr, 0) != 0 || size == 0 || size > sizeof(name) || name[size - 1] != '\0')
		return false;
	return std::strcmp(name, "Efficiency") == 0;
}

std::vector<CoreType> ReadCoreTypes(uint32_t logicalCount, uint32_t physicalCount)
{
	std::vector<CoreType> coreTypes;
	const uint32_t levels = ReadCpuCount("hw.nperflevels");
	uint64_t levelLogicalCount = 0;
	uint64_t levelPhysicalCount = 0;

	// Bound optional metadata queries and reject inconsistent power-state snapshots.
	if (levels <= MAX_CPUS) {
		for (uint32_t level = 0; level < levels; ++level) {
			CoreType type;
			type.sysctlPrefix = "hw.perflevel" + std::to_string(level) + ".";
			type.logicalCount = ReadCpuCount((type.sysctlPrefix + "logicalcpu").c_str());
			type.physicalCount = ReadCpuCount((type.sysctlPrefix + "physicalcpu").c_str());
			type.efficiency = (level != 0) && IsEfficiencyLevel(type.sysctlPrefix);
			if (type.physicalCount == 0 || type.logicalCount < type.physicalCount) {
				coreTypes.clear();
				break;
			}
			levelLogicalCount += type.logicalCount;
			levelPhysicalCount += type.physicalCount;
			coreTypes.push_back(type);
		}
	}

	if (coreTypes.empty() || levelLogicalCount != logicalCount || levelPhysicalCount != physicalCount)
		return {{logicalCount, physicalCount, false, "hw."}};

	return coreTypes;
}

Topology ReadTopology()
{
	uint32_t logicalCount = ReadCpuCount("hw.logicalcpu");
	if (logicalCount == 0)
		logicalCount = ReadCpuCount("hw.activecpu");
	if (logicalCount == 0)
		logicalCount = ReadCpuCount("hw.ncpu");
	logicalCount = std::max(logicalCount, 1u);

	const uint32_t queriedPhysicalCount = ReadCpuCount("hw.physicalcpu");
	const uint32_t physicalCount = (queriedPhysicalCount == 0) ? logicalCount : std::min(queriedPhysicalCount, logicalCount);
	Topology topology;
	uint32_t nextCpu = 0;

	// Darwin does not provide CPU IDs suitable for hard pinning. These synthetic
	// masks retain counts/cache groups for the engine's scheduling heuristics only.
	// The existing interface can describe at most 32 logical processors.
	for (const CoreType& type : ReadCoreTypes(logicalCount, physicalCount)) {
		if (nextCpu == MAX_CPUS)
			break;
		const uint32_t count = std::min(type.logicalCount, MAX_CPUS - nextCpu);
		const uint32_t primaryCount = std::min(type.physicalCount, count);
		const uint32_t firstCpu = nextCpu;
		uint32_t typeMask = 0;
		for (uint32_t cpu = 0; cpu < count; ++cpu) {
			const uint32_t bit = uint32_t(1) << nextCpu++;
			typeMask |= bit;
			if (cpu >= primaryCount)
				topology.masks.hyperThreadHighMask |= bit;
			else if (type.logicalCount > type.physicalCount)
				topology.masks.hyperThreadLowMask |= bit;
		}
		if (type.efficiency)
			topology.masks.efficiencyCoreMask |= typeMask;
		else
			topology.masks.performanceCoreMask |= typeMask;

		ProcessorGroupCaches cache;
		cache.cacheSizes[0] = ReadCacheSize(type.sysctlPrefix + "l1dcachesize");
		cache.cacheSizes[1] = ReadCacheSize(type.sysctlPrefix + "l2cachesize");
		cache.cacheSizes[2] = ReadCacheSize(type.sysctlPrefix + "l3cachesize");
		const char* sharedCache = (cache.cacheSizes[2] != 0) ? "cpusperl3" : "cpusperl2";
		uint32_t groupCount = ReadCpuCount((type.sysctlPrefix + sharedCache).c_str());
		if (groupCount == 0 || groupCount > type.logicalCount || type.logicalCount % groupCount != 0 || type.logicalCount != type.physicalCount)
			groupCount = count;

		for (uint32_t cpu = 0; cpu < count; ++cpu) {
			cache.groupMask |= uint32_t(1) << (firstCpu + cpu);
			if ((cpu + 1) % groupCount == 0 || cpu + 1 == count) {
				topology.caches.groupCaches.push_back(cache);
				cache.groupMask = 0;
			}
		}
	}
	return topology;
}

const Topology& GetTopology()
{
	// Read masks and cache groups from a single snapshot for consistent counts.
	static const Topology topology = ReadTopology();
	return topology;
}

} // namespace

ThreadPinPolicy GetThreadPinPolicy()
{
	return THREAD_PIN_POLICY_NONE;
}

ProcessorMasks GetProcessorMasks()
{
	return GetTopology().masks;
}

ProcessorCaches GetProcessorCache()
{
	return GetTopology().caches;
}

} // namespace cpu_topology
